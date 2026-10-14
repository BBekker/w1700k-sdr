/* SPDX-License-Identifier: MIT */
/* LE 1M, 2 complex samples/symbol. Two timing hypotheses persist across blocks.
 * All bytes are unwhitened and CRC checked on the DSP; no invalid packets escape.
 * See Bluetooth Core Vol 6 Part B 2.1, 2.3, 3.1 and 3.2. */
#include "decode.h"
static void clear(void *ptr, unsigned n)
{
    unsigned char *p = ptr;
    while (n--)
        *p++ = 0;
}
void ble_reset(struct ble_decoder *s, unsigned channel)
{
    clear(s, sizeof(*s));
    s->channel = channel;
    s->last_end = 0xffffffffu;
}
static void bit(struct ble_decoder *s, struct ble_phase *p, unsigned b, struct ble_output *out)
{
    if (!p->active) {
        p->shift = (p->shift >> 1) | (b << 31);
        if (p->shift == 0x8e89bed6u) {
            p->active = 1;
            p->bits = 0;
            p->target = 16;
            p->whitening = (uint8_t)(s->channel | 64);
            p->crc = 0xaaaaaa;
            p->start = s->samples - 63;
        }
        return;
    }
    b ^= p->whitening & 1;
    p->whitening = (uint8_t)((p->whitening >> 1) ^ ((p->whitening & 1) ? 0x44 : 0));
    unsigned pos = p->bits++, byte = pos >> 3;
    if (!(pos & 7))
        p->bytes[byte] = 0;
    p->bytes[byte] |= (uint8_t)(b << (pos & 7));
    /* Header is needed before the payload length is known. */
    if (pos < 16 || pos < (unsigned)(p->bytes[1] + 2) * 8) {
        unsigned feedback = (p->crc ^ b) & 1;
        p->crc = (p->crc >> 1) ^ (feedback ? 0xda6000u : 0);
    }
    if (p->bits == 16) {
        unsigned type = p->bytes[0] & 15, len = p->bytes[1];
        int valid = type == 7
                        ? len >= 1
                        : ((type == 0 || type == 2 || type == 4 || type == 6)
                               ? len >= 6 && len <= 37
                               : ((type == 1 || type == 3) ? len == 12 : type == 5 && len == 34));
        if (!valid) {
            p->active = 0;
            p->shift = 0;
            return;
        }
        p->target = (uint16_t)((len + 5) * 8);
    }
    if (p->bits == p->target && p->bits > 16) {
        unsigned n = p->bytes[1] + 2;
        uint32_t crc =
            p->bytes[n] | ((uint32_t)p->bytes[n + 1] << 8) | ((uint32_t)p->bytes[n + 2] << 16);
        if (crc == p->crc) {
            if (s->samples - s->last_end > 2 || s->last_crc != crc) {
                s->valid++;
                s->last_end = s->samples;
                s->last_crc = crc;
                if (out->count < BLE_MAX_EVENTS) {
                    struct ble_packet *r = &out->packets[out->count++];
                    r->sample = p->start;
                    r->length = (uint16_t)n;
                    r->channel = (uint8_t)s->channel;
                    r->reserved = 0;
                    for (unsigned i = 0; i < n + 3; i++)
                        r->pdu[i] = p->bytes[i];
                } else
                    out->dropped++;
            }
        } else
            s->crc_fail++;
        p->active = 0;
        p->shift = 0;
    }
}
void ble_decode(struct ble_decoder *s, const int16_t *iq, unsigned n, struct ble_output *out)
{
    out->count = out->dropped = 0;
    for (unsigned k = 0; k < n; k++) {
        int32_t i = iq[2 * k], q = iq[2 * k + 1];
        /* Input is scaled to 12 bits: products and their difference fit int32. */
        int32_t cross = s->prev_i * q - s->prev_q * i;
        s->prev_i = i;
        s->prev_q = q;
        bit(s, &s->phase[s->samples & 1], cross > 0, out);
        s->samples++;
    }
    out->samples = s->samples;
    out->crc_fail = s->crc_fail;
    out->valid = s->valid;
}

void ble_decode8(struct ble_decoder *s, const int8_t *iq, unsigned n, struct ble_output *out)
{
    out->count = out->dropped = 0;
    for (unsigned k = 0; k < n; k++) {
        int32_t i = iq[2 * k], q = iq[2 * k + 1];
        /* Input is scaled to 12 bits: products and their difference fit int32. */
        int32_t cross = s->prev_i * q - s->prev_q * i;
        s->prev_i = i;
        s->prev_q = q;
        bit(s, &s->phase[s->samples & 1], cross > 0, out);
        s->samples++;
    }
    out->samples = s->samples;
    out->crc_fail = s->crc_fail;
    out->valid = s->valid;
}
