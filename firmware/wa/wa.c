// SPDX-License-Identifier: MIT
/* Our WA's main loop: MediaTek's WA dispatch, as far as the stream producer uses it. The
 * producer raises the WA by putting a function in the action-bit-1 slot of the WA's
 * soft-interrupt table, setting bit 1 of the action word and writing the WA's software
 * interrupt; it checks first that the dispatch is at rest (slot holding the vendor WA's stock
 * handler, action and interrupt words clear). We poll instead of taking the interrupt: clear the
 * bit and the interrupt word, call the slot's function (the packer, until the stream's stop),
 * then go on polling. Nothing else of the WiFi subsystem is touched.
 *
 * The WA comes out of reset with its caches and memory attributes in their reset state; MediaTek's
 * WA sets them up itself. Ours sets the ROM's profile (the UMAC pool write-back, the rest as on
 * the WM) and turns the caches on, since the packer reads the pool through the D-cache.
 */
#include <stdint.h>

#include "hw/mt7996/mcu_irq.h"
#include "hw/mt7996/mcu_periph.h"

#include "wa.h"

#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define ST(off) R32(WA_STATUS + (off))

#define CSR_READ(csr) ({ uint32_t v_; asm volatile("csrr %0, %1" : "=r"(v_) : "i"(csr)); v_; })
#define CSR_WRITE(csr, v) asm volatile("csrw %0, %1" : : "i"(csr), "r"(v) : "memory")

void wa_main(void);

static const uint32_t pma_addr[MT7996_MCU_PMA_PROFILE_ENTRIES] = {
	MT7996_MCU_PMAADDR0_VAL, MT7996_MCU_PMAADDR1_VAL, MT7996_MCU_PMAADDR2_VAL, MT7996_MCU_PMAADDR3_VAL,
	MT7996_MCU_PMAADDR4_VAL, MT7996_MCU_PMAADDR5_VAL, MT7996_MCU_PMAADDR6_VAL, MT7996_MCU_PMAADDR7_VAL,
	MT7996_MCU_PMAADDR8_VAL, MT7996_MCU_PMAADDR9_VAL, MT7996_MCU_PMAADDR10_VAL,
};

/* pmaaddr(n) takes its CSR number as an immediate. */
static void pma_addr_write(uint32_t n, uint32_t v)
{
	switch (n) {
#define PMA_CASE(i) case i: CSR_WRITE(MT7996_MCU_CSR_PMAADDR(i), v); break;
	PMA_CASE(0) PMA_CASE(1) PMA_CASE(2) PMA_CASE(3) PMA_CASE(4) PMA_CASE(5)
	PMA_CASE(6) PMA_CASE(7) PMA_CASE(8) PMA_CASE(9) PMA_CASE(10)
#undef PMA_CASE
	}
}

/* Our code, stack and status block are in local memory or SYSRAM, which entry 0 covers as
 * non-cacheable both before and after, so no step leaves them without attributes. */
static void core_setup(void)
{
	uint32_t i;

	ST(WA_ENTRY_MCACHE_AT) = CSR_READ(MT7996_MCU_CSR_MCACHE_CTL);
	ST(WA_ENTRY_MMISC_AT) = CSR_READ(MT7996_MCU_CSR_MMISC_CTL);
	ST(WA_ENTRY_PMACFG0_AT) = CSR_READ(MT7996_MCU_CSR_PMACFG(0));
	ST(WA_ENTRY_PMACFG2_AT) = CSR_READ(MT7996_MCU_CSR_PMACFG(2));
	for (i = 0; i < MT7996_MCU_PMA_PROFILE_ENTRIES; i++)
		pma_addr_write(i, pma_addr[i]);
	CSR_WRITE(MT7996_MCU_CSR_PMACFG(0), MT7996_MCU_PMACFG0_VAL);
	CSR_WRITE(MT7996_MCU_CSR_PMACFG(1), MT7996_MCU_PMACFG1_VAL);
	CSR_WRITE(MT7996_MCU_CSR_PMACFG(2), MT7996_MCU_PMACFG2_VAL);
	asm volatile("fence" ::: "memory");
	/* Caches: drop whatever the reset left, then on. */
	CSR_WRITE(MT7996_MCU_CSR_MCCTLCOMMAND, MT7996_MCU_CCTL_L1D_INVAL_ALL);
	for (i = 0; i < MT7996_MCU_WA_ICACHE_BYTES; i += MT7996_MCU_WA_ICACHE_LINE) {
		CSR_WRITE(MT7996_MCU_CSR_MCCTLBEGINADDR, i);
		CSR_WRITE(MT7996_MCU_CSR_MCCTLCOMMAND, MT7996_MCU_CCTL_L1I_IX_INVAL);
	}
	CSR_WRITE(MT7996_MCU_CSR_MCACHE_CTL, MT7996_MCU_MCACHE_CTL_STOCK);
	CSR_WRITE(MT7996_MCU_CSR_MMISC_CTL, MT7996_MCU_MMISC_CTL_VAL);
}

void wa_main(void)
{
	uint32_t n = 0, a;

	core_setup();
	R32(MT7996_MCU_WA_SWI_SLOT1) = MT7996_MCU_WA_SWI_SLOT1_STOCK;
	R32(MT7996_MCU_SWDEF_WA_ACTION) = 0;
	R32(MT7996_MCU_WA_SWI) = 0;
	ST(WA_BEATS_AT) = 0;
	ST(WA_CALLS_AT) = 0;
	ST(WA_LAST_AT) = 0;
	asm volatile("fence" ::: "memory");
	ST(WA_MAGIC_AT) = WA_MAGIC;
	for (;;) {
		a = R32(MT7996_MCU_SWDEF_WA_ACTION);
		if (a & MT7996_MCU_SWDEF_WA_ACTION_SLOT1) {
			void (*fn)(void) = (void (*)(void))(uintptr_t)R32(MT7996_MCU_WA_SWI_SLOT1);

			R32(MT7996_MCU_SWDEF_WA_ACTION) = a & ~MT7996_MCU_SWDEF_WA_ACTION_SLOT1;
			R32(MT7996_MCU_WA_SWI) = 0;
			ST(WA_LAST_AT) = (uint32_t)(uintptr_t)fn;
			if ((uintptr_t)fn != MT7996_MCU_WA_SWI_SLOT1_STOCK)
				fn();
			asm volatile("csrci mstatus, %0" : : "n"(MT7996_MCU_MSTATUS_MIE) : "memory");
			ST(WA_CALLS_AT) = ST(WA_CALLS_AT) + 1;
		}
		if (!(++n & 1023))
			ST(WA_BEATS_AT) = n >> 10;
	}
}
