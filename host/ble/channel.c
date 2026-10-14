/* SPDX-License-Identifier: MIT */
#include "channel.h"
#include <math.h>
#include <string.h>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
void ble_channel_init(struct ble_channel *s, unsigned channel)
{
    const double pi = 3.14159265358979323846;
    double frequency = (channel == 37 ? -39e6 : channel == 38 ? -15e6 : 39e6) / 80e6;
    memset(s, 0, sizeof(*s));
    for (unsigned k = 0; k < 80; k++) {
        double angle = -2 * pi * frequency * k;
        int16_t r = (int16_t)lround(32767 * cos(angle)), i = (int16_t)lround(32767 * sin(angle));
        s->re[2 * k] = (int16_t)-i;
        s->re[2 * k + 1] = r;
        s->im[2 * k] = r;
        s->im[2 * k + 1] = i;
    }
}
unsigned ble_channel_run(struct ble_channel *s, const uint32_t *raw, unsigned n, int16_t *out)
{
    unsigned produced = 0, phase = s->phase, cic_phase = s->cic_phase;
#if defined(__ARM_NEON)
    uint32x2_t a0 = {s->integrator[0][0], s->integrator[1][0]};
    uint32x2_t a1 = {s->integrator[0][1], s->integrator[1][1]};
    uint32x2_t a2 = {s->integrator[0][2], s->integrator[1][2]};
    uint32x2_t c0 = {s->comb[0][0], s->comb[1][0]};
    uint32x2_t c1 = {s->comb[0][1], s->comb[1][1]};
    uint32x2_t c2 = {s->comb[0][2], s->comb[1][2]};
#endif
    /* DMA blocks are multiples of 8 samples. */
    for (unsigned k = 0; k + 7 < n; k += 8) {
#if defined(__ARM_NEON)
        int32x4_t ar = vdupq_n_s32(0), ai = vdupq_n_s32(0);
        for (unsigned j = 0; j < 8; j += 4) {
            int16x8_t x = vreinterpretq_s16_u32(vld1q_u32(raw + k + j));
            x = vshrq_n_s16(vshlq_n_s16(x, 2), 5);
            int16x8_t cr = vld1q_s16(s->re + 2 * (phase + j)),
                      ci = vld1q_s16(s->im + 2 * (phase + j));
            ar = vmlal_s16(ar, vget_low_s16(x), vget_low_s16(cr));
            ar = vmlal_high_s16(ar, x, cr);
            ai = vmlal_s16(ai, vget_low_s16(x), vget_low_s16(ci));
            ai = vmlal_high_s16(ai, x, ci);
        }
        int32x4_t t = vpaddq_s32(ar, ai);
        int32x2_t mixed = vshr_n_s32(vpadd_s32(vget_low_s32(t), vget_high_s32(t)), 15);
        a0 = vadd_u32(a0, vreinterpret_u32_s32(mixed));
        a1 = vadd_u32(a1, a0);
        a2 = vadd_u32(a2, a1);
#else
        int32_t r = 0, i = 0;
        for (unsigned j = 0; j < 8; j++) {
            int32_t q = ((int32_t)((raw[k + j] & 0x3fff) ^ 0x2000) - 0x2000) >> 3;
            int32_t a = ((int32_t)(((raw[k + j] >> 16) & 0x3fff) ^ 0x2000) - 0x2000) >> 3;
            r += q * s->re[2 * (phase + j)] + a * s->re[2 * (phase + j) + 1];
            i += q * s->im[2 * (phase + j)] + a * s->im[2 * (phase + j) + 1];
        }
        s->integrator[0][0] += (uint32_t)(r >> 15);
        s->integrator[1][0] += (uint32_t)(i >> 15);
        for (unsigned c = 0; c < 2; c++) {
            s->integrator[c][1] += s->integrator[c][0];
            s->integrator[c][2] += s->integrator[c][1];
        }
#endif
        phase += 8;
        if (phase == 80)
            phase = 0;
        if (++cic_phase != 5)
            continue;
        cic_phase = 0;
#if defined(__ARM_NEON)
        uint32x2_t d0 = vsub_u32(a2, c0);
        c0 = a2;
        uint32x2_t d1 = vsub_u32(d0, c1);
        c1 = d0;
        uint32x2_t d2 = vsub_u32(d1, c2);
        c2 = d1;
        /* Unsigned integrator wrap cancels in the comb. /128 normalizes
         * the CIC gain of 125; the extra /4 limits decoder input to 12 bits. */
        int32x2_t scaled = vshr_n_s32(vreinterpret_s32_u32(d2), 9);
        int32_t value[2] = {vget_lane_s32(scaled, 0), vget_lane_s32(scaled, 1)};
#else
        int32_t value[2];
        for (unsigned c = 0; c < 2; c++) {
            uint32_t v = s->integrator[c][2];
            for (unsigned j = 0; j < 3; j++) {
                uint32_t before = v;
                v -= s->comb[c][j];
                s->comb[c][j] = before;
            }
            value[c] = (int32_t)v >> 9;
        }
#endif
        for (unsigned c = 0; c < 2; c++) {
            int32_t v = value[c];
            out[2 * produced + c] = (int16_t)(v > 2047 ? 2047 : v < -2047 ? -2047 : v);
        }
        produced++;
    }
#if defined(__ARM_NEON)
#define SAVE(field, j, v)                                                                          \
    s->field[0][j] = vget_lane_u32(v, 0);                                                          \
    s->field[1][j] = vget_lane_u32(v, 1)
    SAVE(integrator, 0, a0);
    SAVE(integrator, 1, a1);
    SAVE(integrator, 2, a2);
    SAVE(comb, 0, c0);
    SAVE(comb, 1, c1);
    SAVE(comb, 2, c2);
#undef SAVE
#endif
    s->phase = phase;
    s->cic_phase = cic_phase;
    return produced;
}
