/* SPDX-License-Identifier: MIT */
/* One A-die SPI transaction from an MCU core, as the ROM's sys_spi driver does it: CONN semaphore
 * 12 taken and released around it, the master's busy bit polled before and after. Shared by the
 * control interpreter (ctl.h) and the radio code (radio/hal_wm.c).
 *
 * An A-die reg is die << 16 | k << 12 | index (MT7996_PROBE_CTL_AREG): k 0..4 a chain or the
 * synthesizer bank, 8..14 a raw chip-select 0..6, 15 the die's TOP bank.
 */
#ifndef STREAM_ADIE_SPI_H
#define STREAM_ADIE_SPI_H

#include <stdint.h>

#include "hw/adie/adie.h"
#include "hw/mt7996/mcu_periph.h"
#include "hw/mt7996/probe_page.h"

#ifndef R32
#define R32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif

static inline uint32_t adie_rdcycle(void)
{
	uint32_t v;

	asm volatile("csrr %0, mcycle" : "=r"(v));
	return v;
}

/* One SPI transaction; 0 or a CTL code. */
static inline uint32_t adie_xfer(uint32_t reg, int read, uint32_t *val)
{
	uint32_t die = HW_GET(MT7996_PROBE_CTL_AREG_DIE, reg), k = HW_GET(MT7996_PROBE_CTL_AREG_K, reg);
	uint32_t m, cs, cmd, wd, rd, busy, t0;

	if (die >= ADIE_NUM_DIES || (k >= ADIE_NUM_K_WF && k < MT7996_PROBE_CTL_AREG_K_RAW_CS))
		return MT7996_PROBE_CTL_BAD_OP;
	/* k 8..14 names a raw chip-select 0..6 (the ROM's sys_spi ids: 2 "BT", 4 "GPS" are otherwise unreachable) */
	cs = k >= MT7996_PROBE_CTL_AREG_K_RAW_CS && k < ADIE_K_TOP ? k - MT7996_PROBE_CTL_AREG_K_RAW_CS :
	     ADIE_K_TO_CS(k);
	m = ADIE_SPI_MCU_BASE(die);
	if (cs == ADIE_CS_TOP) {
		cmd = ADIE_SPI_TOP_CMD; wd = ADIE_SPI_TOP_WDATA; rd = ADIE_SPI_TOP_RDATA;
		busy = ADIE_SPI_STATUS_BUSY_TOP;
	} else if (cs == ADIE_CS_BT) {
		cmd = ADIE_SPI_CH1_CMD; wd = ADIE_SPI_CH1_WDATA; rd = ADIE_SPI_CH1_RDATA;
		busy = ADIE_SPI_STATUS_BUSY_CH1;
	} else {
		cmd = ADIE_SPI_CH0_CMD; wd = ADIE_SPI_CH0_WDATA; rd = ADIE_SPI_CH0_RDATA;
		busy = ADIE_SPI_STATUS_BUSY_CH0;
	}
	t0 = adie_rdcycle();
	while (!ADIE_SEMA_GRANTED(R32(ADIE_SEMA_ACQ_MCU)))	/* a READ is the acquire */
		if (adie_rdcycle() - t0 > 1000u * MT7996_MCU_CYCLES_PER_US)
			return MT7996_PROBE_CTL_SEMA;
	if (R32(m + ADIE_SPI_STATUS) & busy) {
		R32(ADIE_SEMA_REL_MCU) = ADIE_SEMA_REL_VAL;
		return MT7996_PROBE_CTL_BUSY;
	}
	R32(m + cmd) = (R32(m + cmd) & ADIE_SPI_CMD_KEEP) | cs << HW_SHIFT(ADIE_SPI_CMD_CS) |
		       (uint32_t)read << HW_SHIFT(ADIE_SPI_CMD_READ) |
		       HW_PREP(ADIE_SPI_CMD_INDEX, HW_GET(MT7996_PROBE_CTL_AREG_INDEX, reg));
	(void)R32(m + wd);
	R32(m + wd) = read ? 0 : *val;		/* starts the transfer */
	t0 = adie_rdcycle();
	while (R32(m + ADIE_SPI_STATUS) & busy)
		if (adie_rdcycle() - t0 > 1000u * MT7996_MCU_CYCLES_PER_US) {
			R32(ADIE_SEMA_REL_MCU) = ADIE_SEMA_REL_VAL;
			return MT7996_PROBE_CTL_SPI;
		}
	if (read)
		*val = R32(m + rd);
	R32(ADIE_SEMA_REL_MCU) = ADIE_SEMA_REL_VAL;
	return MT7996_PROBE_CTL_OK;
}

static inline uint32_t adie_w(uint32_t reg, uint32_t v)
{
	return adie_xfer(reg, 0, &v);
}

#endif
