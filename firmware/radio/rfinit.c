// SPDX-License-Identifier: MIT
/* Calibration item 0 (the A-die RF init) and the two power-on calibrations MediaTek runs from
 * inside it, T0[0] (R-cal) and T0[1] (RC calibration), for one die.
 *
 * Order and registers: hw/adie/adie.h "RF_INIT and the WF tables", "RC calibration and RC trim"
 * and ADIE_RCAL; the D-die words: hw/mt7996/phy.h MT7996_PHY_RFIF_D414, MT7996_PHY_RF_TBL_KEY;
 * MediaTek's tables and the procedure's constants: hw/adie/rfinit.h (generated from MediaTek's
 * WM image). Left out of MediaTek's sequence: the RF table helpers' debug gate on +0xd414 (two
 * reads per MT7976 RF_COMMON call; the word was written by this procedure just before and never
 * holds the debug key), the re-read of die 0's chip ID for the 7976C test (radio_id holds it),
 * and phyCalDo's bookkeeping around T0[0] / T0[1] (state bytes, timing, the CalTime and DPD
 * status reads). MediaTek's restore after a calibration maps table 0 to no restore flags, so
 * nothing is restored after R-cal and RC; the MT7977 RC body ends with its own restore of TOP
 * 0x3a4 and the measurement bank's 0xe50 mode, as MediaTek's does.
 */
#include "hw/adie/rfinit.h"
#include "hw/mt7996/phy.h"
#include "hw/mt7996/phy_cal.h"

#include "radio.h"
#include "rfinit.h"

#define TOP ADIE_K_TOP

/* MediaTek's two table forms (hw/adie/rfinit.h "TABLES"): pre-rows, one value per register
 * for every WF; walker tables, one value column per key, ADIE_RFTBL_SKIP = no write. */
struct rf_pre {
	uint16_t idx;
	uint32_t val;
};

struct rf_walk {
	const uint16_t *idx;
	const uint32_t *val;	/* n rows of ncols */
	uint32_t n, ncols;
};

#define RF_PRE(reg, val)	{ (reg), (val) },
#define RF_IDX(reg, ...)	(reg),
#define RF_VALS(reg, ...)	__VA_ARGS__,
#define ARRAY_SIZE(a)		(sizeof(a) / sizeof((a)[0]))

#define PRE_TABLE(name, list) static const struct rf_pre name[] = { list(RF_PRE) }

#define WALK_TABLE(name, list, cols)                                                             \
	static const uint16_t name##_idx[] = { list(RF_IDX) };                                    \
	static const uint32_t name##_val[] = { list(RF_VALS) };                                   \
	_Static_assert(ARRAY_SIZE(name##_val) == (cols) * ARRAY_SIZE(name##_idx), #name " columns"); \
	static const struct rf_walk name = { name##_idx, name##_val, ARRAY_SIZE(name##_idx), (cols) }

PRE_TABLE(mt7977_pre, MT7977_RFINIT_PRE);
WALK_TABLE(mt7977_walk, MT7977_RFINIT_WALK, ADIE_NUM_K_WF);
WALK_TABLE(mt7977_walk_eco9, MT7977_RFINIT_WALK_ECO9, ADIE_NUM_K_WF);

PRE_TABLE(mt7976_rfinit_pre, MT7976_RFINIT_PRE);
WALK_TABLE(mt7976_rfinit_walk, MT7976_RFINIT_WALK, ADIE_NUM_K_WF);
PRE_TABLE(mt7976_common_pre, MT7976_RF_COMMON_PRE);
WALK_TABLE(mt7976_common_walk, MT7976_RF_COMMON_WALK, MT7976_NUM_CHAINS);
WALK_TABLE(mt7976_epa_walk, MT7976_RF_EPA_COMMON_WALK, MT7976_NUM_CHAINS);
PRE_TABLE(mt7976_xpa_pre, MT7976_XPA_PRE);
PRE_TABLE(mt7976_xlna_pre, MT7976_XLNA_PRE);
WALK_TABLE(mt7976c_epa_walk, MT7976C_RF_EPA_COMMON_WALK, MT7976_NUM_CHAINS);
PRE_TABLE(mt7976c_xpa_pre, MT7976C_XPA_PRE);

static const uint8_t mt7976_common_key[MT7976_NUM_CHAINS] = MT7976_RF_COMMON_KEYS;

/* Pre-rows to bank k (k = WF): V:e0051536 / V:e0050d3e / V:e0050a30. */
static void put_pre(uint32_t die, uint32_t k, const struct rf_pre *t, uint32_t n)
{
	for (uint32_t i = 0; i < n; i++)
		hal_awr(die, k, t[i].idx, t[i].val);
}

/* A walker table's column to bank k: V:e0051af2 / V:e0050f44 / V:e0050b2a. */
static void put_walk(uint32_t die, uint32_t k, const struct rf_walk *t, uint32_t col)
{
	for (uint32_t i = 0; i < t->n; i++) {
		uint32_t v = t->val[i * t->ncols + col];

		if (v != ADIE_RFTBL_SKIP)
			hal_awr(die, k, t->idx[i], v);
	}
}

/* Item 0's D-die words, before any table (MT7976 V:e005105a..V:e00510a2, MT7977
 * V:e0051c3a..V:e0051c84). */
static void d_words(uint32_t die, uint32_t d414)
{
	hal_wr(MT7996_PHY_RFIF_D414(die), d414);
	for (uint32_t wf = 0; wf < ADIE_NUM_K_WF; wf++)
		hal_wr(MT7996_PHY_RF_TBL_KEY(die, wf), 0);
}

/* MT7976, V:e00512e8 and the 7976C test V:e0040edc. Every table helper gets P = {WF, 0, 0}
 * (memset structs): P[1] = 0, P[2] = 0 (SX0), whatever the EEPROM's EPA / ELNA bits. */
static void rf_tables_mt7976(uint32_t die)
{
	uint32_t wf;

	d_words(die, MT7996_PHY_RFIF_D414_MT7976);
	for (wf = 0; wf < ADIE_NUM_K_WF; wf++) {			/* "7976 RF_INIT" */
		put_pre(die, wf, mt7976_rfinit_pre, ARRAY_SIZE(mt7976_rfinit_pre));
		put_walk(die, wf, &mt7976_rfinit_walk, wf);
	}
	for (wf = 0; wf < MT7976_NUM_CHAINS; wf++) {
		/* "7976 RF_COMMON": the key record, pre-rows, walk */
		hal_rmw(MT7996_PHY_RF_TBL_KEY(die, wf), MT7996_PHY_RF_TBL_KEY_SX0,
			HW_PREP(MT7996_PHY_RF_TBL_KEY_SX0, MT7996_PHY_RF_TBL_KEY_REC(mt7976_common_key[wf])));
		put_pre(die, wf, mt7976_common_pre, ARRAY_SIZE(mt7976_common_pre));
		put_walk(die, wf, &mt7976_common_walk, wf);
		put_walk(die, wf, &mt7976_epa_walk, wf);			/* "7976 RF_EPA_COMMON" */
		put_pre(die, wf, mt7976_xpa_pre, ARRAY_SIZE(mt7976_xpa_pre));	/* "7976 XPA" */
		hal_awr(die, TOP, ADIE_TOP_71C, ADIE_TOP_71C_XPA);
		put_pre(die, wf, mt7976_xlna_pre, ARRAY_SIZE(mt7976_xlna_pre));	/* "7976 XLNA" */
	}
	if (HW_GET(ADIE_TOP_CHIP_ID_11_8, radio_id.chip_id[die]) != ADIE_TOP_CHIP_ID_11_8_MT7976C)
		return;
	for (wf = 0; wf < MT7976_NUM_CHAINS; wf++) {			/* 7976C, V:e0050cbe */
		put_walk(die, wf, &mt7976c_epa_walk, wf);		/* "7976dap RF_EPA_COMMON" */
		put_pre(die, wf, mt7976c_xpa_pre, ARRAY_SIZE(mt7976c_xpa_pre));	/* "7976dap XPA" */
		hal_awr(die, TOP, ADIE_TOP_71C, ADIE_TOP_71C_XPA);
	}
}

/* MT7977, V:e0051c04: the ECO byte of die 2 picks the variant for both MT7977 dies. */
static void rf_tables_mt7977(uint32_t die)
{
	int eco9 = radio_eco() <= MT7977_ECO_P1_MAX;
	const struct rf_walk *walk = eco9 ? &mt7977_walk_eco9 : &mt7977_walk;

	d_words(die, eco9 ? MT7996_PHY_RFIF_D414_MT7977_ECO9 : MT7996_PHY_RFIF_D414_MT7977);
	for (uint32_t wf = 0; wf < ADIE_NUM_K_WF; wf++) {
		put_pre(die, wf, mt7977_pre, ARRAY_SIZE(mt7977_pre));
		put_walk(die, wf, walk, wf);
	}
}

/* T0[0], phyCalRcal V:e00419ac: the die's R-cal byte from the EEPROM image into 0xe40, two
 * read-modify-writes per bank (the code, then [9]). */
static void rcal(uint32_t die, int mt7977)
{
	uint32_t r = ee8(MT7996_PHY_EE_RCAL(die)), k;
	uint32_t code77 = HW_GET(MT7996_PHY_EE_RCAL_CODE_MT7977, r);
	uint32_t code76 = HW_GET(MT7996_PHY_EE_RCAL_CODE_MT7976, r);
	int valid = (r & MT7996_PHY_EE_RCAL_VALID) != 0;

	if (mt7977) {						/* V:e0041bec..V:e0041c74 */
		if (!valid)
			return;
		for (k = 0; k < ADIE_RCAL_BANKS_MT7977; k++) {
			hal_awf(die, k, ADIE_RCAL, ADIE_RCAL_CODE_MT7977, code77);
			hal_armw(die, k, ADIE_RCAL, ADIE_RCAL_EN, ADIE_RCAL_EN);
		}
		return;
	}
	for (k = 0; k < ADIE_RCAL_BANKS_MT7976; k++) {		/* V:e0041a1e..V:e0041af2 */
		if (valid) {
			hal_awf(die, k, ADIE_RCAL, ADIE_RCAL_CODE_MT7976, code76);
			hal_armw(die, k, ADIE_RCAL, ADIE_RCAL_EN, ADIE_RCAL_EN);
		} else {
			hal_awf(die, k, ADIE_RCAL, ADIE_RCAL_DEFAULT_FIELD_MT7976, ADIE_RCAL_DEFAULT_MT7976);
		}
	}
}

/* Strobe the measurement bank's RC-cal prep bit low, then high. */
static void rccal_prep(uint32_t die, uint32_t m)
{
	hal_armw(die, m, ADIE_STROBE, ADIE_STROBE_RCCAL_PREP, 0);
	hal_armw(die, m, ADIE_STROBE, ADIE_STROBE_RCCAL_PREP, ADIE_STROBE_RCCAL_PREP);
}

/* One measurement: the start bit high for ADIE_RCTRIM_START_US, then low (no done flag). */
static void rccal_start(uint32_t die, uint32_t m)
{
	hal_armw(die, m, ADIE_RCTRIM, ADIE_RCTRIM_START, ADIE_RCTRIM_START);
	hal_udelay(ADIE_RCTRIM_START_US);
	hal_armw(die, m, ADIE_RCTRIM, ADIE_RCTRIM_START, 0);
}

/* T0[1], RC dispatcher V:e004fff8: MT7977 V:e004d34a, two measurements on bank k 2. */
static void rccal_mt7977(uint32_t die)
{
	uint32_t m = ADIE_RCCAL_BANK_MT7977, code, k;

	rccal_prep(die, m);
	hal_armw(die, TOP, ADIE_TOP_RCCAL, ADIE_TOP_RCCAL_EN, ADIE_TOP_RCCAL_EN);
	hal_awf(die, TOP, ADIE_TOP_RCCAL, ADIE_TOP_RCCAL_PHASE, ADIE_TOP_RCCAL_PHASE_MEAS1);
	hal_awf(die, m, ADIE_RCCAL_CFG, ADIE_RCCAL_CFG_MODE, ADIE_RCCAL_MODE_MEAS1);
	hal_awf(die, m, ADIE_RCCAL_CFG, ADIE_RCCAL_CFG_TARGET, ADIE_RCCAL_TARGET_MT7977);
	rccal_start(die, m);
	hal_awf(die, TOP, ADIE_TOP_RCCAL, ADIE_TOP_RCCAL_PHASE, ADIE_TOP_RCCAL_PHASE_MEAS2);
	hal_awf(die, m, ADIE_RCCAL_CFG, ADIE_RCCAL_CFG_MODE, ADIE_RCCAL_MODE_IDLE);
	hal_awf(die, m, ADIE_RCCAL_CFG, ADIE_RCCAL_CFG_MODE, ADIE_RCCAL_MODE_MEAS2);
	hal_armw(die, m, ADIE_RCTRIM2, ADIE_RCTRIM2_PHASE2, ADIE_RCTRIM2_PHASE2);
	rccal_start(die, m);
	hal_armw(die, m, ADIE_RCTRIM2, ADIE_RCTRIM2_PHASE2, 0);
	code = HW_GET(ADIE_RCCAL_RESULT_MT7977_C1, hal_ard(die, m, ADIE_RCCAL_RESULT));
	for (k = 0; k < ADIE_NUM_K_WF; k++) {
		hal_awf(die, k, ADIE_RCTRIM, ADIE_RCTRIM_CODE, code);
		hal_armw(die, k, ADIE_RCTRIM, ADIE_RCTRIM_SW, ADIE_RCTRIM_SW);
	}
	radio_cal.rc_code[die] = code;
	radio_cal.rc_valid |= 1u << die;
	code = HW_GET(ADIE_RCCAL_RESULT_MT7977_C2, hal_ard(die, m, ADIE_RCCAL_RESULT));
	for (k = 0; k < ADIE_NUM_K_WF; k++) {
		hal_awf(die, k, ADIE_RCTRIM2, ADIE_RCTRIM2_CODE, code);
		hal_armw(die, k, ADIE_RCTRIM2, ADIE_RCTRIM2_VALID, ADIE_RCTRIM2_VALID);
	}
	hal_awf(die, m, ADIE_RCCAL_CFG, ADIE_RCCAL_CFG_MODE, ADIE_RCCAL_MODE_IDLE);
	hal_awf(die, TOP, ADIE_TOP_RCCAL, ADIE_TOP_RCCAL_PHASE, ADIE_TOP_RCCAL_PHASE_OFF);
	hal_armw(die, TOP, ADIE_TOP_RCCAL, ADIE_TOP_RCCAL_EN, 0);
}

/* MT7976 V:e00602bc: one measurement on bank k 0 against the reference clock's target. */
static void rccal_mt7976(uint32_t die)
{
	uint32_t m = ADIE_RCCAL_BANK_MT7976, code, k;

	rccal_prep(die, m);
	hal_awf(die, m, ADIE_RCCAL_CFG, ADIE_RCCAL_CFG_TARGET, ADIE_RCCAL_TARGET_MT7976(ADIE_RCCAL_REF_HZ));
	rccal_start(die, m);
	code = HW_GET(ADIE_RCCAL_RESULT_MT7976_C1, hal_ard(die, m, ADIE_RCCAL_RESULT));
	for (k = 0; k < ADIE_NUM_K_WF; k++) {
		hal_awf(die, k, ADIE_RCTRIM, ADIE_RCTRIM_CODE, code);
		hal_armw(die, k, ADIE_RCTRIM, ADIE_RCTRIM_SW, ADIE_RCTRIM_SW);
	}
	radio_cal.rc_code[die] = code;
	radio_cal.rc_valid |= 1u << die;
}

void rf_init(uint32_t die)
{
	uint32_t want = die == ADIE_DIE_MT7976 ? ADIE_TOP_CHIP_ID_PART_MT7976 : ADIE_TOP_CHIP_ID_PART_MT7977;
	int mt7977 = die != ADIE_DIE_MT7976;

	if (die >= RADIO_DIES) {
		hal_fail(HAL_FAIL_ARG, die);
		return;
	}
	if (HW_GET(ADIE_TOP_CHIP_ID_PART, radio_id.chip_id[die]) != want) {
		hal_fail(HAL_FAIL_STATE, radio_id.chip_id[die]);
		return;
	}
	if (mt7977 && !radio_is_mt7977(ADIE_DIE_MT7977_6G)) {	/* the ECO byte's source */
		hal_fail(HAL_FAIL_STATE, radio_id.chip_id[ADIE_DIE_MT7977_6G]);
		return;
	}
	if (mt7977)
		rf_tables_mt7977(die);
	else
		rf_tables_mt7976(die);
	rcal(die, mt7977);
	if (mt7977)
		rccal_mt7977(die);
	else
		rccal_mt7976(die);
}
