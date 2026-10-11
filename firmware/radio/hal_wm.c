// SPDX-License-Identifier: MIT
/* hal.h on the WM: D-die accesses are plain loads and stores, A-die accesses one SPI transaction
 * each (stream/adie_spi.h), time from the core's cycle counter (720 MHz). Interrupts are off for
 * the whole of our WM's run, so nothing interleaves with a transaction.
 */
#include "hw/mt7996/mcu_periph.h"
#include "hw/mt7996/secure.h"

#include "../stream/adie_spi.h"
#include "hal.h"

struct hal_fail hal_fail_rec;

void hal_fail(uint32_t code, uint32_t arg)
{
	if (!hal_fail_rec.count++) {
		hal_fail_rec.code = code;
		hal_fail_rec.arg = arg;
	}
}

uint32_t hal_rd(uint32_t addr)
{
	return R32(addr);
}

void hal_wr(uint32_t addr, uint32_t val)
{
	R32(addr) = val;
}

/* Microseconds accumulated from cycle deltas (no 64-bit division in a freestanding build); valid
 * while calls come less than 2^32 cycles (5.9 s) apart, which every wait here does. */
uint32_t hal_now_us(void)
{
	static uint32_t last, us, rem;
	uint32_t now = adie_rdcycle(), d = now - last + rem;

	last = now;
	us += d / MT7996_MCU_CYCLES_PER_US;
	rem = d % MT7996_MCU_CYCLES_PER_US;
	return us;
}

void hal_udelay(uint32_t us)
{
	uint32_t t0 = adie_rdcycle();

	while (adie_rdcycle() - t0 < us * MT7996_MCU_CYCLES_PER_US)
		;
}

uint32_t hal_poll(uint32_t addr, uint32_t mask, uint32_t val, uint32_t timeout_us, int soft)
{
	uint32_t t0 = hal_now_us(), v;

	for (;;) {
		v = R32(addr);
		if ((v & mask) == val)
			return v;
		if (hal_now_us() - t0 > timeout_us) {
			if (!soft)
				hal_fail(HAL_FAIL_POLL, addr);
			return v;
		}
	}
}

static uint32_t xfer(uint32_t die, uint32_t k, uint32_t idx, int read, uint32_t val)
{
	uint32_t reg = MT7996_PROBE_CTL_AREG(die, k, idx), v = val;

	if (adie_xfer(reg, read, &v)) {
		hal_fail(HAL_FAIL_SPI, HAL_AREG(die, k, idx));
		return 0;
	}
	return v;
}

uint32_t hal_ard(uint32_t die, uint32_t k, uint32_t idx)
{
	return xfer(die, k, idx, 1, 0);
}

void hal_awr(uint32_t die, uint32_t k, uint32_t idx, uint32_t val)
{
	xfer(die, k, idx, 0, val);
}

uint32_t hal_apoll(uint32_t die, uint32_t k, uint32_t idx, uint32_t mask, uint32_t val, uint32_t timeout_us,
		   int soft)
{
	uint32_t t0 = hal_now_us(), v;

	for (;;) {
		v = hal_ard(die, k, idx);
		if ((v & mask) == val)
			return v;
		if (hal_now_us() - t0 > timeout_us) {
			if (!soft)
				hal_fail(HAL_FAIL_APOLL, HAL_AREG(die, k, idx));
			return v;
		}
	}
}

/* The ROM routine runs with the ROM's gp, which our WM keeps in gp; like every ROM routine it may
 * leave interrupts on, so they go off again (no source is unmasked in our WM anyway). */
int hal_efuse8_soft(uint32_t addr, uint8_t *val)
{
	uint32_t (*read8)(uint32_t, void *, uint32_t) = (void *)(uintptr_t)MT7996_ROM_EFUSE_READ8;
	uint32_t buf = 0, r;

	r = read8(addr, &buf, 1);
	asm volatile("csrci mstatus, %0" : : "n"(MT7996_MCU_MSTATUS_MIE) : "memory");
	*val = (uint8_t)buf;
	return r ? -1 : 0;
}

uint32_t hal_rom_call4(uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
	uint32_t (*f)(uint32_t, uint32_t, uint32_t, uint32_t) = (void *)(uintptr_t)fn;
	uint32_t r = f(a0, a1, a2, a3);

	asm volatile("csrci mstatus, %0" : : "n"(MT7996_MCU_MSTATUS_MIE) : "memory");
	return r;
}
