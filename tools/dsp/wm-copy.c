/* SPDX-License-Identifier: MIT */
/* Baseline only: copy ICAP's stopped window, path 0, into DSP0 DTCM.
 * Called by the own WM from documented free ULM1 scratch. No host IQ round
 * trip and no DMA claim: the radio MCU executes every load and store.
 * Two pages hold 2048 consecutive raw complex samples (iq_format.h).
 * Caller must retire ICAP and release the window bank grants first. */
#include <stdint.h>

uint32_t copy_window(void)
{
    uint32_t start, end;
    __asm__ volatile ("rdcycle %0" : "=r"(start));
    volatile uint32_t *dst = (volatile uint32_t *)0x8620d000;
    for (uint32_t i = 0; i < 2048; i++) {
        uint32_t address = 0x00464000 + ((i >> 10) << 13) + ((i & 1023) << 2);
        dst[i] = *(volatile uint32_t *)address;
    }
    __asm__ volatile ("fence iorw, iorw\n rdcycle %0" : "=r"(end) :: "memory");
    return end - start;
}
