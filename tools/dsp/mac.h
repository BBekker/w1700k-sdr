/* SPDX-License-Identifier: MIT */
/* MRV5 two-accumulator complex multiply. Not standard RISC-V P or V.
 * See include/hw/mt7996/dsp.h. The round/read encoding (funct7=2,
 * funct3=6) is identified in the public DSP image. No vendor code used.
 * Interrupts must stay disabled; both implicit accumulators are clobbered.
 * Operands must keep the sum of two products within signed 36 bits. */
#ifndef DSP_MRV_MAC_H
#define DSP_MRV_MAC_H
#include <stdint.h>
static inline void mrv_cmul(int32_t ar, int32_t ai, int32_t br, int32_t bi,
                            unsigned shift, int32_t *re, int32_t *im)
{
#if defined(DSP_MRV5)
    int32_t r, i;
    __asm__ volatile (
        ".insn r 0x7b, 0, 0x60, x0, %2, %4\n"
        ".insn r 0x7b, 0, 0x60, x1, %3, %4\n"
        ".insn r 0x7b, 3, 0x40, x0, %3, %5\n"
        ".insn r 0x7b, 1, 0x40, x1, %2, %5\n"
        ".insn r 0x7b, 6, 2, %0, x0, %6\n"
        ".insn r 0x7b, 6, 2, %1, x1, %6\n"
        : "=&r"(r), "=&r"(i) : "r"(ar), "r"(ai), "r"(br), "r"(bi), "r"(shift));
    *re=r; *im=i;
#else
    int64_t bias=shift ? (int64_t)1 << (shift-1) : 0;
    *re=(int32_t)(((int64_t)ar*br-(int64_t)ai*bi+bias)>>shift);
    *im=(int32_t)(((int64_t)ar*bi+(int64_t)ai*br+bias)>>shift);
#endif
}
#endif
