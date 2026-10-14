/* SPDX-License-Identifier: MIT */
#include <stdint.h>

/* DTCM: mailbox [0,0x1000), FFT [0x1000,0x9000), twiddles
 * [0x9000,0xd000), raw IQ [0xd000,0xf000), stack [0xf000,0x10000). */
struct mailbox {
    uint32_t magic, request, ack, op, n, cycles;
    uint32_t cause, epc, tval, result, state, address, map_window;
    uint32_t windows, base5, limit5, misa, window_after;
};
#define BOX ((volatile struct mailbox *)0x200000)
#define CSR_READ(n) ({ uint32_t v; __asm__ volatile ("csrr %0, " #n : "=r"(v)); v; })
#define CSR_WRITE(n,v) __asm__ volatile ("csrw " #n ", %0" :: "r"(v) : "memory")
extern void dsp_fft(float *, const float *, unsigned);
extern uint32_t guarded_probe(uint32_t);

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
        if ((BOX->op == 1 || (BOX->op == 3 && BOX->n <= 2048)) &&
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
