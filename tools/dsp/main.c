/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include "../../host/ble/decode.h"

/* DTCM offsets: mailbox [0,0x1000), general FFT [0x1000,0x9000),
 * twiddles [0x9000,0xd000), raw IQ [0xd000,0xf000), stack [0xf000,0x10000).
 * Spectrum ops 4/6 use only [0x1000,0x5000) for FFT data; power occupies
 * [0x5000,0x7000), and experimental op 6's Q15 twiddles [0x7000,0x9000). */
struct mailbox {
    uint32_t magic, request, ack, op, n, cycles;
    uint32_t cause, epc, tval, result, state, address, map_window;
    uint32_t windows, base5, limit5, misa, window_after;
};
#define BOX ((volatile struct mailbox *)0x200000)
#define CSR_READ(n) ({ uint32_t v; __asm__ volatile ("csrr %0, " #n : "=r"(v)); v; })
#define CSR_WRITE(n,v) __asm__ volatile ("csrw " #n ", %0" :: "r"(v) : "memory")
extern void dsp_fft(float *, const float *, unsigned);
extern void dsp_spectrum(const volatile uint32_t *, float *, const float *, unsigned, float *);
extern uint32_t guarded_probe(uint32_t);
extern uint32_t dsp_mac_check(void);
extern void dsp_fixed_spectrum(const volatile uint32_t *, int32_t *, const float *, unsigned, float *, int32_t *);

static int allowed(uint32_t a)
{
    /* Read-only probes; no peripheral addresses or arbitrary CSR sweeps. */
    return !(a & 3) && ((a >= 0x00400000 && a < 0x00480000) ||
           (a >= 0x00900000 && a < 0x00938000) ||
           (a >= 0x02200000 && a < 0x02240000));
}

void dsp_main(void)
{
    BOX->request = BOX->ack = 0;
    BOX->misa = CSR_READ(misa);
    BOX->state = 1;
    BOX->magic = 0x44535031; /* DSP1 */
    /* Same per-core ready location used by the vendor startup. */
    *(volatile uint32_t *)0x86c00014 = 0x44535031;
    *(volatile uint32_t *)0x86c00010 = 1;
    for (;;) {
        uint32_t req = BOX->request;
        if (req == BOX->ack) continue;
        BOX->cause = BOX->epc = BOX->tval = BOX->cycles = 0;
        BOX->state = 2;
        uint32_t start = CSR_READ(cycle);
        if (BOX->op == 8 && BOX->n <= 2048 &&
            (BOX->address & 255) >= 37 && (BOX->address & 255) <= 39) {
            struct ble_decoder *decoder = (struct ble_decoder *)0x201000;
            if (BOX->address & 256) ble_reset(decoder, BOX->address & 255);
            if (BOX->address & 512)
                ble_decode8(decoder, (const int8_t *)0x204000, BOX->n,
                            (struct ble_output *)0x208000);
            else
                ble_decode(decoder, (const int16_t *)0x204000, BOX->n,
                           (struct ble_output *)0x208000);
            BOX->result = 0;
        } else if (BOX->op == 5) {
            BOX->result = dsp_mac_check();
        } else if (BOX->op == 6 && BOX->n == 2048) {
            dsp_fixed_spectrum((const volatile uint32_t *)0x20d000, (int32_t *)0x201000,
                              (const float *)0x209000, 2048, (float *)0x205000,
                              (int32_t *)0x207000);
            BOX->result = 0;
        } else if ((BOX->op == 4 || (BOX->op == 7 && BOX->address<4)) && BOX->n == 2048) {
            /* Op 7: two independent input/output banks. The caller owns
             * the selected raw bank until ack; the selected power bank
             * until output DMA completes. Op 6 shares these reservations
             * and must never run alongside the pipeline. */
            uint32_t bank=BOX->op==7 ? BOX->address : 0;
            dsp_spectrum((const volatile uint32_t *)((bank&1)?0x207000:0x20d000), (float *)0x201000,
                         (const float *)0x209000, 2048, (float *)((bank&2)?0x20b000:0x205000));
            BOX->result = 0;
        } else if ((BOX->op == 1 || (BOX->op == 3 && BOX->n <= 2048)) &&
            BOX->n >= 2 && BOX->n <= 4096 && !(BOX->n & (BOX->n-1))) {
            if (BOX->op == 3) {
                const volatile uint32_t *raw = (const volatile uint32_t *)0x20d000;
                float *x = (float *)0x201000;
                for (uint32_t i = 0; i < BOX->n; i++) {
                    uint32_t v = raw[i];
                    x[2*i] = (float)((int32_t)(((v >> 16) & 0x3fff) ^ 0x2000) - 0x2000);
                    x[2*i+1] = (float)((int32_t)((v & 0x3fff) ^ 0x2000) - 0x2000);
                }
            }
            dsp_fft((float *)0x201000, (const float *)0x209000, BOX->n);
            BOX->result = 0;
        } else if (BOX->op == 2 && allowed(BOX->address)) {
            uint32_t mask = CSR_READ(0x9c0), base = CSR_READ(0x9e5), limit = CSR_READ(0x9f5);
            BOX->windows = mask; BOX->base5 = base; BOX->limit5 = limit;
            if (BOX->map_window) {
                uint32_t page = BOX->address & ~4095u;
                CSR_WRITE(0x9e5, page | 0xc0u);
                CSR_WRITE(0x9f5, page + 4096);
                CSR_WRITE(0x9c0, mask | 32u);
                __asm__ volatile ("fence iorw, iorw" ::: "memory");
            }
            BOX->window_after = CSR_READ(0x9c0);
            BOX->result = guarded_probe(BOX->address);
            CSR_WRITE(0x9c0, mask);
            CSR_WRITE(0x9e5, base);
            CSR_WRITE(0x9f5, limit);
        } else BOX->result = 0xffffffffu;
        BOX->cycles = CSR_READ(cycle) - start;
        BOX->state = 1;
        __asm__ volatile ("fence iorw, iorw" ::: "memory");
        BOX->ack = req;
    }
}
