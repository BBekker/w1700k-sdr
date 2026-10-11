// SPDX-License-Identifier: MIT
/* A built RF program run synchronously in our WM (rfctl.h rfctl_run): the ops as the control
 * interpreter (stream/ctl.h) executes them, through the HAL, so a radio command (SETUP) tunes with
 * the very program TUNE runs. A poll's timeout is a failure unless the op is soft (counted, not
 * failed, as the interpreter does). */
#include "hw/adie/mt7977.h"
#include "hw/adie/sx.h"
#include "hw/mt7996/probe_page.h"

#include "hal.h"
#include "radio.h"
#include "radio_cmd.h"
#include "rfctl.h"

#define AREG_DIE(a)	HW_GET(MT7996_PROBE_CTL_AREG_DIE, a)
#define AREG_K(a)	HW_GET(MT7996_PROBE_CTL_AREG_K, a)
#define AREG_IDX(a)	HW_GET(MT7996_PROBE_CTL_AREG_INDEX, a)

/* The MT7977's software VCO band search, as stream/ctl.h sx_sar: 10 steps, each a measurement
 * (SX_CMD = MEASURE, ADIE_SX_SETTLE_US, SX_CMD = PREP), the comparator kept as the next code bit,
 * the next trial code written to SX_BAND (presets kept) and SX_BAND2. Returns the final code. */
static uint32_t sar(uint32_t die)
{
	uint32_t k = MT7977_K_SX, low = hal_ard(die, k, ADIE_SX_BAND) & ADIE_SX_BAND_PRESET, acc = 0, code = 0;

	for (uint32_t i = 0; i < MT7977_SAR_STEPS; i++) {
		hal_awr(die, k, ADIE_SX_CMD, ADIE_SX_CMD_MEASURE);
		hal_udelay(ADIE_SX_SETTLE_US);
		hal_awr(die, k, ADIE_SX_CMD, ADIE_SX_CMD_PREP);
		acc = (acc + (HW_GET(ADIE_SX_CMP_KEEP, hal_ard(die, k, ADIE_SX_CMP)) << (MT7977_SAR_STEPS - 1 - i))) &
		      0xffffu;
		code = MT7977_SAR_CODE(acc, i);
		hal_awr(die, k, ADIE_SX_BAND, HW_PREP(ADIE_SX_BAND_WORD, code) | low);
		hal_awr(die, k, ADIE_SX_BAND2, HW_PREP(ADIE_SX_BAND2_WORD, code));
	}
	return code;
}

uint32_t rfctl_run(const struct rfctl_op *ops, uint32_t n, uint32_t *res)
{
	for (uint32_t i = 0; i < n; i++) {
		const struct rfctl_op *o = &ops[i];
		uint32_t code = HW_GET(MT7996_PROBE_CTL_OP_CODE, o->op), us = HW_GET(MT7996_PROBE_CTL_OP_ARG, o->op);
		int soft = (o->op & MT7996_PROBE_CTL_OP_SOFT) != 0;
		uint32_t v = 0;

		switch (code) {
		case MT7996_PROBE_CTL_OP_END:
			return radio_failed() ? RADIO_ERR_HW : RADIO_OK;
		case MT7996_PROBE_CTL_OP_W32:
			hal_wr(o->a, o->b);
			break;
		case MT7996_PROBE_CTL_OP_RMW32:
			v = hal_rmw(o->a, o->b, o->c);
			break;
		case MT7996_PROBE_CTL_OP_R32:
			v = hal_rd(o->a);
			break;
		case MT7996_PROBE_CTL_OP_DELAY:
			hal_udelay(us);
			break;
		case MT7996_PROBE_CTL_OP_POLL:
			v = hal_poll(o->a, o->b, o->c, us, soft);
			break;
		case MT7996_PROBE_CTL_OP_AW:
			hal_awr(AREG_DIE(o->a), AREG_K(o->a), AREG_IDX(o->a), o->b);
			break;
		case MT7996_PROBE_CTL_OP_AR:
			v = hal_ard(AREG_DIE(o->a), AREG_K(o->a), AREG_IDX(o->a));
			break;
		case MT7996_PROBE_CTL_OP_ARMW:
			v = hal_armw(AREG_DIE(o->a), AREG_K(o->a), AREG_IDX(o->a), o->b, o->c);
			break;
		case MT7996_PROBE_CTL_OP_APOLL:
			v = hal_apoll(AREG_DIE(o->a), AREG_K(o->a), AREG_IDX(o->a), o->b, o->c, us, soft);
			break;
		case MT7996_PROBE_CTL_OP_SAR:
			v = sar(o->a);
			break;
		default:
			hal_fail(HAL_FAIL_ARG, o->op);
			return RADIO_ERR_HW;
		}
		if (res)
			res[i] = v;
	}
	return radio_failed() ? RADIO_ERR_HW : RADIO_OK;
}
