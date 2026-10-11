// SPDX-License-Identifier: MIT
/* RX DC-offset calibration of one band at the channel it is set to (dcoc.h).
 *
 * Per chain WF, what MediaTek's two calibration bodies do for one group, in its order
 * (hw/adie/adie.h "RX DC-offset calibration (DCOC)", hw/adie/dcoc.h; MediaTek WM V:e004bde0 /
 * V:e004c632 on the MT7977, V:e005f2c2 / V:e005f7b4 on the MT7976):
 *   fine (mask bit 8): the chain in the DC-calibration state, one coarse conversion (the MT7976
 *     keeps its two codes, the MT7977 discards them), then the fine engine, which sweeps the gain
 *     steps itself and leaves 7-bit codes in the fine result rows;
 *   coarse per step (mask bit 9): the chain in the receive state with the monitor mode word,
 *     one coarse conversion per gain step (5..0 MT7977, 4..0 MT7976), each leaving a signed byte
 *     in the coarse result rows;
 * each followed by MediaTek's calibration tail: the result rows read and checked, WF_COMM +0xd4d0
 * and A-die 0x031 / 0x032 as MediaTek writes them, our restore of the D-die word, the band's
 * WFSYS gate sequence, the calibration-end PHY pulse, our restore of the A-die registers.
 *
 * Left out of MediaTek's calls: the synthesizer retune before each body (item 10; the channel
 * set has tuned it), phyCalDo's bookkeeping (its log gates WF_COMM +0xd414 / +0xd7e0 and DPD
 * state reads), the result record store and its write-back (the codes stay in the rows), and the
 * bit-3 backup and restore (431 / 219 A-die registers, 430 D-die words, ARB_SCR x3), replaced by
 * our save of what the bodies change (hw/adie/dcoc.h ADIE_DCOC_SAVE_*). Two dead reads (MT7977:
 * TOP 0x744 in the FEM helper; MT7976: TOP 0x070 before its write) are not made.
 */
#include "hw/adie/adie.h"
#include "hw/adie/dcoc.h"
#include "hw/adie/mt7976.h"
#include "hw/adie/mt7977.h"
#include "hw/adie/rfmodes.h"
#include "hw/adie/sx.h"
#include "hw/mt7996/conn.h"
#include "hw/mt7996/phy.h"

#include "dcoc.h"
#include "radio.h"

#define TOP	ADIE_K_TOP
#define NELEM(a)	(sizeof(a) / sizeof((a)[0]))

uint32_t dcoc_fail[RADIO_BANDS];

struct save_ent {
	uint8_t role;		/* ADIE_DCOC_W / _S / _T */
	uint16_t idx;
};

#define SAVE_ENT(role, idx)	{ (role), (idx) },
static const struct save_ent save_77[] = { ADIE_DCOC_SAVE_MT7977(SAVE_ENT) };
static const struct save_ent save_77_sx[] = { ADIE_DCOC_SAVE_MT7977_SX(SAVE_ENT) };
static const struct save_ent save_76[] = { ADIE_DCOC_SAVE_MT7976(SAVE_ENT) };
static const struct save_ent save_76_sx[] = { ADIE_DCOC_SAVE_MT7976_SX(SAVE_ENT) };
#define SAVE_MAX	NELEM(save_77)
_Static_assert(NELEM(save_77_sx) <= SAVE_MAX && NELEM(save_76) <= SAVE_MAX && NELEM(save_76_sx) <= SAVE_MAX,
	       "save list sizes");

struct row {
	uint16_t idx;
	uint32_t fields;	/* byte-aligned code fields */
};

#define ROW(idx, fields)	{ (idx), (fields) },
static const struct row fine_rows[] = { ADIE_DCOC_FINE_ROWS(ROW) };
static const struct row step_rows[] = { ADIE_DCOC_STEP_ROWS(ROW) };

static const uint8_t steps_77[] = ADIE_DCOC_STEPS_MT7977;
static const uint8_t steps_76[] = ADIE_DCOC_STEPS_MT7976;

struct dcoc {
	uint32_t band, die, wf;
	uint32_t sx;		/* the die's SX bank */
	uint32_t mt7977;
	uint32_t gn;		/* MediaTek's calibration group != 0: not the 2.4 GHz group */
	uint32_t epa;		/* P[6]: EEPROM 0x196 + band [1] */
	uint32_t cls;		/* RX bandwidth class the bodies calibrate at */
	const struct save_ent *list;
	uint32_t n;
	uint32_t saved[SAVE_MAX];
	uint32_t d4d0;
};

/* ------------------------------------------------------------------ A-die helpers */

static uint32_t rd(const struct dcoc *c, uint32_t k, uint32_t idx)
{
	return hal_ard(c->die, k, idx);
}

static void wr(const struct dcoc *c, uint32_t k, uint32_t idx, uint32_t v)
{
	hal_awr(c->die, k, idx, v);
}

static void set(const struct dcoc *c, uint32_t k, uint32_t idx, uint32_t bits)
{
	hal_armw(c->die, k, idx, 0, bits);
}

static void clr(const struct dcoc *c, uint32_t k, uint32_t idx, uint32_t bits)
{
	hal_armw(c->die, k, idx, bits, 0);
}

static void fld(const struct dcoc *c, uint32_t k, uint32_t idx, uint32_t mask, uint32_t v)
{
	hal_awf(c->die, k, idx, mask, v);
}

static uint32_t bank(const struct dcoc *c, uint32_t role)
{
	return role == ADIE_DCOC_W ? c->wf : role == ADIE_DCOC_S ? c->sx : TOP;
}

/* MediaTek's poll (hw/adie/dcoc.h ADIE_DCOC_POLL_*): three satisfying reads of the chain's
 * register within one deadline. A timeout is recorded (MediaTek's is silent) and the run goes
 * on, as MediaTek's does. */
static void poll3(const struct dcoc *c, uint32_t idx, uint32_t mask, uint32_t val)
{
	uint32_t end = hal_now_us() + ADIE_DCOC_POLL_DEADLINE_US, left, i;

	hal_udelay(ADIE_DCOC_POLL_PRE_US);
	for (i = 0; i < ADIE_DCOC_POLL_MATCHES; i++) {
		left = end - hal_now_us();
		hal_apoll(c->die, c->wf, idx, mask, val, (int32_t)left > 0 ? left : 0, 0);
		hal_udelay(ADIE_DCOC_POLL_MATCH_US);
	}
}

/* ------------------------------------------------------------------ pieces of the bodies */

/* First accesses of every body: DCOC setup [15] = 1, then [13:12] = 0. */
static void setup(const struct dcoc *c)
{
	set(c, c->wf, ADIE_DCOC_SETUP, ADIE_DCOC_SETUP_EN);
	clr(c, c->wf, ADIE_DCOC_SETUP, ADIE_DCOC_SETUP_1312);
}

/* MediaTek's FEM helper V:e00526c2 in mode 0 (EPA boards): no WF's external PA selected. */
static void fem(const struct dcoc *c)
{
	if (c->mt7977) {
		clr(c, TOP, MT7977_TOP_EPA, MT7977_TOP_EPA_WF);
		set(c, TOP, MT7977_TOP_EPA, MT7977_TOP_EPA_EN);
	} else {
		hal_armw(c->die, TOP, MT7976_TOP_FEM, MT7976_TOP_FEM_WF_FIELDS, MT7976_TOP_FEM_WF_CAL);
	}
}

/* 0x101 / 0x103 whole: [0] = (group != 0) (MT7977 coarse, both MT7976 bodies). */
static void aux_words(const struct dcoc *c)
{
	wr(c, c->wf, ADIE_BW_AUX0, c->gn ? ADIE_BW_AUX0_EN : 0);
	wr(c, c->wf, ADIE_BW_AUX1, c->gn ? ADIE_BW_AUX1_EN : 0);
}

/* The MT7977 bodies' RX bandwidth class. */
static void bw_class(const struct dcoc *c)
{
	if (c->cls <= ADIE_RX_BW_CODE_320MHZ) {
		wr(c, c->wf, ADIE_RX_BW, c->cls);
		wr(c, c->wf, ADIE_TX_BW, c->cls);
	}
}

/* One coarse conversion, its two codes copied into 0x923 with [21] set (fine item). */
static void coarse_codes(const struct dcoc *c, uint32_t start_us)
{
	uint32_t w = c->wf, a, b;

	set(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_START);
	hal_udelay(start_us);
	poll3(c, ADIE_DCOC_DC_RB, ADIE_DCOC_DC_RB_BUSY, 0);
	clr(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_START);
	a = rd(c, w, ADIE_DCOC_DC_RB);
	b = rd(c, w, ADIE_DCOC_DC_RB);
	set(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_APPLY);
	fld(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_CODE_B, HW_GET(ADIE_DCOC_DC_RB_CODE_B, b));
	fld(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_CODE_A, HW_GET(ADIE_DCOC_DC_RB_CODE_A, a));
}

/* Coarse per step: 0x923 [21], [19:18], [17:16] cleared. */
static void coarse_clear(const struct dcoc *c)
{
	clr(c, c->wf, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_APPLY);
	clr(c, c->wf, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_1918);
	clr(c, c->wf, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_1716);
}

static void start_codes(const struct dcoc *c)
{
	wr(c, c->wf, ADIE_DCOC_927, ADIE_DCOC_START_CODES);
	wr(c, c->wf, ADIE_DCOC_928, ADIE_DCOC_START_CODES);
	wr(c, c->wf, ADIE_DCOC_929, ADIE_DCOC_START_CODE);
}

/* End of the fine bodies: the effective enables still show the DC state -> 0x171 [11] low. */
static void strobe(const struct dcoc *c)
{
	if (!(rd(c, c->wf, ADIE_CHAIN_EN_RB) & ADIE_CHAIN_EN_RB_DCOC))
		return;
	clr(c, c->wf, ADIE_STROBE, ADIE_STROBE_CAL);
	hal_udelay(ADIE_DCOC_STROBE_US);
	set(c, c->wf, ADIE_STROBE, ADIE_STROBE_CAL);
}

/* ------------------------------------------------------------------ the four bodies */

static void fine_mt7977(const struct dcoc *c)
{
	uint32_t w = c->wf, s = c->sx, i;

	setup(c);
	bw_class(c);
	wr(c, s, ADIE_CHAIN_MODE, MT7977_SX_DCOC_MODE);
	wr(c, w, ADIE_CHAIN_EN, ADIE_CHAIN_EN_RX_DCOC);
	set(c, w, ADIE_CHAIN_OVR, ADIE_CHAIN_OVR_MANUAL);
	set(c, w, ADIE_CHAIN_CTL, ADIE_CHAIN_CTL_ON);
	/* MediaTek's branch for group != 0; its group-0 branch (0x925 [7:4] = 5, 0x101 / 0x103 [0]
	 * cleared) is for the 2.4 GHz group, which an MT7977 never serves */
	fld(c, w, ADIE_DCOC_SETUP2, ADIE_DCOC_SETUP2_GN, ADIE_DCOC_SETUP2_VAL);
	set(c, w, ADIE_BW_AUX0, ADIE_BW_AUX0_EN);
	set(c, w, ADIE_BW_AUX1, ADIE_BW_AUX1_EN);
	if (c->epa)
		fem(c);
	clr(c, w, ADIE_DCOC_601, ADIE_DCOC_60X_B0);
	clr(c, w, ADIE_DCOC_602, ADIE_DCOC_60X_B0);
	wr(c, TOP, MT7977_TOP_DCOC_WF(w), MT7977_TOP_DCOC_WF_ON);
	wr(c, TOP, MT7977_TOP_7E9, MT7977_TOP_7E9_VAL);
	set(c, w, ADIE_DCOC_11A, ADIE_DCOC_11A_FINE);
	set(c, w, MT7977_RCD_PRE, ADIE_DCOC_403_FINE);
	wr(c, s, MT7977_SX_DCOC_745, MT7977_SX_DCOC_745_FINE);
	wr(c, s, MT7977_SX_DCOC_14A, MT7977_SX_DCOC_14A_VAL);
	wr(c, s, ADIE_FB_SEL, MT7977_SX_DCOC_149_VAL);
	fld(c, w, ADIE_DCOC_024, ADIE_DCOC_024_MODE, ADIE_DCOC_024_MODE_FINE);
	fld(c, w, ADIE_CHAIN_CTL, ADIE_CHAIN_CTL_CALPATH, ADIE_CHAIN_CTL_CALPATH_DC);
	set(c, w, MT7977_RCD_744, ADIE_DCOC_744_FINE);
	set(c, s, MT7977_SX_DCOC_746, MT7977_SX_DCOC_746_FINE);
	fld(c, s, MT7977_SX_DCOC_747, MT7977_SX_DCOC_747_FINE, MT7977_SX_DCOC_747_FINE_VAL);
	set(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_B0);
	set(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_B3);
	clr(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_COARSE_CLR);
	set(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_B4);
	set(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_B8);
	coarse_codes(c, 0);
	/* the MT7977 discards the coarse codes before the fine step */
	wr(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_MT7977_FINE);
	clr(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_B0);
	clr(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_B3);
	clr(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_COARSE_CLR);
	clr(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_B4);
	clr(c, w, ADIE_DCOC_11B, ADIE_DCOC_11B_B8);
	start_codes(c);
	fld(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_MODE, ADIE_DCOC_FINE_MODE_DC);
	set(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_1918);
	clr(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_B17);
	set(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_START);
	poll3(c, ADIE_DCOC_DONE, ADIE_DCOC_DONE_FLAG, ADIE_DCOC_DONE_FLAG);
	clr(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_START);
	clr(c, w, MT7977_RCD_744, ADIE_DCOC_744_FINE);
	clr(c, s, MT7977_SX_DCOC_746, MT7977_SX_DCOC_746_FINE);
	clr(c, s, MT7977_SX_DCOC_747, MT7977_SX_DCOC_747_FINE);
	clr(c, w, MT7977_RCD_PRE, ADIE_DCOC_403_FINE);
	clr(c, w, ADIE_DCOC_11A, ADIE_DCOC_11A_FINE);
	wr(c, s, MT7977_SX_DCOC_745, MT7977_SX_DCOC_745_REST);
	for (i = 0; i < ADIE_NUM_K_WF; i++)
		wr(c, TOP, MT7977_TOP_DCOC_WF(i), 0);
	strobe(c);
}

static void step_mt7977(const struct dcoc *c)
{
	uint32_t w = c->wf, s = c->sx, i;

	setup(c);
	bw_class(c);
	if (w != s)	/* on the SX bank's own chain the chain mode word below is the one */
		wr(c, s, ADIE_CHAIN_MODE, MT7977_SX_DCOC_MODE);
	wr(c, w, ADIE_CHAIN_MODE, HW_PREP(ADIE_CHAIN_MODE_SEL, ADIE_CHAIN_MODE_SEL_MONITOR));
	wr(c, w, ADIE_CHAIN_EN, ADIE_CHAIN_EN_RX);
	set(c, w, ADIE_CHAIN_OVR, ADIE_CHAIN_OVR_MANUAL);
	set(c, w, ADIE_CHAIN_CTL, ADIE_CHAIN_CTL_ON);
	aux_words(c);
	if (c->epa)
		fem(c);
	clr(c, w, ADIE_DCOC_601, ADIE_DCOC_60X_B0);
	clr(c, w, ADIE_DCOC_602, ADIE_DCOC_60X_B0);
	coarse_clear(c);
	set(c, w, ADIE_DCOC_STEP, ADIE_DCOC_STEP_EN);
	set(c, w, ADIE_DCOC_024, HW_PREP(ADIE_DCOC_024_MODE, ADIE_DCOC_024_MODE_STEP));
	fld(c, w, ADIE_CHAIN_CTL, ADIE_CHAIN_CTL_CALPATH, ADIE_CHAIN_CTL_CALPATH_DC);
	set(c, s, MT7977_SX_DCOC_745, MT7977_SX_DCOC_745_STEP_HI);
	set(c, s, MT7977_SX_DCOC_745, MT7977_SX_DCOC_745_STEP_LO);
	for (i = 0; i < NELEM(steps_77); i++) {
		fld(c, w, ADIE_DCOC_STEP, ADIE_DCOC_STEP_IDX, steps_77[i]);
		set(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_START);
		poll3(c, ADIE_DCOC_DC_RB, ADIE_DCOC_DC_RB_BUSY, 0);
		clr(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_START);
	}
	clr(c, s, MT7977_SX_DCOC_745, MT7977_SX_DCOC_745_STEP_HI);
	clr(c, s, MT7977_SX_DCOC_745, MT7977_SX_DCOC_745_STEP_LO);
}

/* MT7976 head of both bodies: chain state, 0xe90 mode (its entry value returned), 0x001 [9:8],
 * 0x101 / 0x103, FEM, the SX output force. */
static uint32_t head_mt7976(const struct dcoc *c, uint32_t chain_en)
{
	uint32_t w = c->wf, e90;

	setup(c);
	wr(c, w, ADIE_CHAIN_EN, chain_en);
	wr(c, w, ADIE_CHAIN_OVR, ADIE_CHAIN_OVR_MANUAL);
	e90 = rd(c, w, MT7976_DCOC_E90);
	fld(c, w, MT7976_DCOC_E90, MT7976_DCOC_E90_MODE, MT7976_DCOC_E90_MODE_DCOC);
	set(c, w, ADIE_CHAIN_CTL, ADIE_CHAIN_CTL_ON);
	aux_words(c);
	if (c->epa)
		fem(c);
	set(c, c->sx, ADIE_SX_OUT_FORCE, ADIE_SX_OUT_FORCE_ON);
	return e90;
}

static void tail_mt7976(const struct dcoc *c, uint32_t e90)
{
	wr(c, c->wf, MT7976_DCOC_E90, e90);
	clr(c, c->sx, ADIE_SX_OUT_FORCE, ADIE_SX_OUT_FORCE_ON);
}

static void fine_mt7976(const struct dcoc *c)
{
	uint32_t w = c->wf, e90 = head_mt7976(c, ADIE_CHAIN_EN_RX_DCOC);

	wr(c, w, MT7976_DCOC_607, MT7976_DCOC_607_COARSE);
	wr(c, TOP, ADIE_TOP_CHAIN_IF, ADIE_TOP_CHAIN_IF_CAL);
	fld(c, w, ADIE_DCOC_024, ADIE_DCOC_024_MODE, ADIE_DCOC_024_MODE_FINE);
	fld(c, w, ADIE_CHAIN_CTL, ADIE_CHAIN_CTL_CALPATH, ADIE_CHAIN_CTL_CALPATH_DC);
	coarse_codes(c, MT7976_DCOC_START_US);	/* the MT7976 keeps them for the fine step */
	wr(c, w, MT7976_DCOC_607, MT7976_DCOC_607_FINE);
	start_codes(c);
	fld(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_MODE, ADIE_DCOC_FINE_MODE_DC);
	clr(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_B17);
	hal_udelay(MT7976_DCOC_PRE_US);
	set(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_START);
	hal_udelay(MT7976_DCOC_START_US);
	poll3(c, ADIE_DCOC_DONE, ADIE_DCOC_DONE_FLAG, ADIE_DCOC_DONE_FLAG);
	clr(c, w, ADIE_DCOC_FINE, ADIE_DCOC_FINE_START);
	tail_mt7976(c, e90);
	strobe(c);
}

static void step_mt7976(const struct dcoc *c)
{
	uint32_t w = c->wf, e90 = head_mt7976(c, ADIE_CHAIN_EN_RX), i;

	coarse_clear(c);
	set(c, w, ADIE_DCOC_STEP, ADIE_DCOC_STEP_EN);
	set(c, w, ADIE_DCOC_024, HW_PREP(ADIE_DCOC_024_MODE, ADIE_DCOC_024_MODE_STEP));
	fld(c, w, ADIE_CHAIN_CTL, ADIE_CHAIN_CTL_CALPATH, ADIE_CHAIN_CTL_CALPATH_DC);
	for (i = 0; i < NELEM(steps_76); i++) {
		fld(c, w, ADIE_DCOC_STEP, ADIE_DCOC_STEP_IDX, steps_76[i]);
		hal_udelay(MT7976_DCOC_PRE_US);
		set(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_START);
		hal_udelay(MT7976_DCOC_START_US);
		poll3(c, ADIE_DCOC_DC_RB, ADIE_DCOC_DC_RB_BUSY, 0);
		clr(c, w, ADIE_DCOC_COARSE, ADIE_DCOC_COARSE_START);
	}
	tail_mt7976(c, e90);
}

/* ------------------------------------------------------------------ save, tail, restore */

static void save(struct dcoc *c)
{
	uint32_t i;

	c->d4d0 = hal_rd(MT7996_PHY_WFCOMM_D4D0(c->band));
	for (i = 0; i < c->n; i++)
		c->saved[i] = rd(c, bank(c, c->list[i].role), c->list[i].idx);
}

static void restore(const struct dcoc *c)
{
	uint32_t i;

	for (i = 0; i < c->n; i++)
		wr(c, bank(c, c->list[i].role), c->list[i].idx, c->saved[i]);
}

/* The chain's value of a saved register (MT7977: its RX bandwidth class). */
static uint32_t saved_chain(const struct dcoc *c, uint32_t idx)
{
	uint32_t i;

	for (i = 0; i < c->n; i++)
		if (c->list[i].role == ADIE_DCOC_W && c->list[i].idx == idx)
			return c->saved[i];
	return 0;
}

/* The six result rows read once; 1 if a code is out of range. */
static uint32_t check(const struct dcoc *c, const struct row *rows, uint32_t n, uint32_t fine)
{
	uint32_t i, lane, v, m, f, bad = 0;

	for (i = 0; i < n; i++) {
		v = rd(c, c->wf, rows[i].idx);
		for (lane = 0; lane < ADIE_DCOC_LANES; lane++) {
			m = rows[i].fields & ADIE_DCOC_LANE(lane);
			if (!m)
				continue;
			f = (v & m) >> HW_SHIFT(ADIE_DCOC_LANE(lane));
			if (fine ? f < ADIE_DCOC_F_MIN || f > ADIE_DCOC_F_MAX :
				   (int8_t)f < -ADIE_DCOC_S_LIMIT || (int8_t)f > ADIE_DCOC_S_LIMIT)
				bad = 1;
		}
	}
	return bad;
}

/* Band WFSYS gate sequence (hw/mt7996/conn.h at MT7996_CONN_PWR_800). */
struct gate_seq {
	const uint32_t *clr, *set;
	uint32_t nclr, nset;
};

static const uint32_t clr0[] = CONN_PWR_800_GATE_CLR_B0, set0[] = CONN_PWR_800_GATE_SET_B0;
static const uint32_t clr1[] = CONN_PWR_800_GATE_CLR_B1, set1[] = CONN_PWR_800_GATE_SET_B1;
static const uint32_t clr2[] = CONN_PWR_800_GATE_CLR_B2, set2[] = CONN_PWR_800_GATE_SET_B2;
static const struct gate_seq gates[RADIO_BANDS] = {
	{ clr0, set0, NELEM(clr0), NELEM(set0) },
	{ clr1, set1, NELEM(clr1), NELEM(set1) },
	{ clr2, set2, NELEM(clr2), NELEM(set2) },
};

static void gate(uint32_t band)
{
	const struct gate_seq *g = &gates[band];
	uint32_t i;

	if (band == CONN_PWR_GATE_BAND_LPCR)
		hal_wr(MT7996_WFSYS_ON_LPCR_DIS(band), WFSYS_ON_LPCR_DIS_PLL_CKEN);
	else
		hal_clr(CONN_PWR_GATE_REG(band), CONN_PWR_1XX_GATE);
	for (i = 0; i < g->nclr; i++)
		hal_clr(MT7996_CONN_PWR_800, g->clr[i]);
	hal_clr(MT7996_CONN_PWR_800, CONN_PWR_800_SHARED);
	hal_udelay(CONN_PWR_GATE_HOLD_US);
	hal_udelay(CONN_PWR_GATE_RST_US);
	hal_clr(MT7996_WF_TOP_ON_BAND_RST, WF_TOP_ON_BAND_RST_BAND(band));
	hal_set(MT7996_WF_TOP_ON_BAND_RST, WF_TOP_ON_BAND_RST_BAND(band));
	for (i = 0; i < g->nset; i++)
		hal_set(MT7996_CONN_PWR_800, g->set[i]);
	if (band == CONN_PWR_GATE_BAND_LPCR)
		hal_wr(MT7996_WFSYS_ON_LPCR_EN(band), WFSYS_ON_LPCR_EN_PLL_CKEN);
	else
		hal_set(CONN_PWR_GATE_REG(band), CONN_PWR_1XX_GATE);
}

/* MediaTek's tail after each body (hw/mt7996/phy_cal.h "Tail of every DCOC body"), with our
 * restore in place of its backup restore; then, if the codes failed the check, the item's
 * defaults, where MediaTek's write-back writes them. */
static void tail(const struct dcoc *c, uint32_t fine)
{
	const struct row *rows = fine ? fine_rows : step_rows;
	uint32_t nrows = fine ? NELEM(fine_rows) : NELEM(step_rows);
	uint32_t d4d0 = MT7996_PHY_WFCOMM_D4D0(c->band), w = c->wf, bad, i, n;

	bad = check(c, rows, nrows, fine);
	/* V:e0048b26: every lane [8n+4] = 1, [8n+3:8n] = 0, from lane 3 down */
	for (n = MT7996_PHY_WFCOMM_D4D0_LANES; n-- > 0;) {
		hal_set(d4d0, MT7996_PHY_WFCOMM_D4D0_EN(n));
		hal_clr(d4d0, MT7996_PHY_WFCOMM_D4D0_SEL(n));
	}
	/* V:e00486fc */
	fld(c, w, ADIE_CALTAIL_031, ADIE_CALTAIL_031_LO, ADIE_CALTAIL_031_LO_VAL);
	fld(c, w, ADIE_CALTAIL_031, ADIE_CALTAIL_031_HI, ADIE_CALTAIL_031_HI_VAL);
	fld(c, w, ADIE_CALTAIL_032, ADIE_CALTAIL_032_HI, ADIE_CALTAIL_032_HI_VAL);
	/* MediaTek restores its 430 backed-up PHY words here: this is the only one the DCOC changes */
	hal_wr(d4d0, c->d4d0);
	gate(c->band);
	/* the calibration-end PHY pulse (V:e0047e36 / V:e0049416) */
	hal_wr(MT7996_PHY_WFCOMM_3008(c->band), MT7996_PHY_WFCOMM_3008_CALEND);
	hal_wr(MT7996_PHY_WFCOMM_3008(c->band), 0);
	hal_wr(MT7996_PHY_RST_CTRL(c->band), MT7996_PHY_RST_CALEND);
	hal_wr(MT7996_PHY_RST_CTRL(c->band), 0);
	hal_set(MT7996_PHY_DSP_EF00(c->band), MT7996_PHY_DSP_EF00_PULSE);
	hal_clr(MT7996_PHY_DSP_EF00(c->band), MT7996_PHY_DSP_EF00_PULSE);
	hal_udelay(MT7996_PHY_CALEND_US);
	restore(c);
	if (!bad)
		return;
	for (i = 0; i < nrows; i++)
		wr(c, w, rows[i].idx, fine ? ADIE_DCOC_F_DEFAULT : ADIE_DCOC_S_DEFAULT);
	dcoc_fail[c->band] |= fine ? DCOC_FAIL_FINE(w) : DCOC_FAIL_STEP(w);
}

/* ------------------------------------------------------------------ entry */

uint32_t dcoc_cal(uint32_t band)
{
	struct dcoc c;
	uint32_t n0 = hal_fail_rec.count, n1, off, code;

	if (band >= RADIO_BANDS)
		return RADIO_ERR_ARG;
	if (!radio_id.identified)
		return RADIO_ERR_ORDER;
	c.band = band;
	c.die = ADIE_DIE_OF_BAND(band);
	c.mt7977 = radio_is_mt7977(c.die);
	/* The MT7976 serves band 0 (die 0) here; MediaTek's body also forces its 5 GHz SX
	 * (MT7976_K_SX5G) when it sits on die 1, which this board does not have. */
	if (!c.mt7977 && (c.die != ADIE_DIE_MT7976 ||
			  HW_GET(ADIE_TOP_CHIP_ID_PART, radio_id.chip_id[c.die]) != ADIE_TOP_CHIP_ID_PART_MT7976)) {
		hal_fail(HAL_FAIL_STATE, radio_id.chip_id[c.die]);
		return RADIO_ERR_HW;
	}
	c.sx = c.mt7977 ? MT7977_K_SX : MT7976_K_SX;
	c.gn = c.mt7977;	/* group 0 is the 2.4 GHz group: the MT7976's band */
	c.epa = !!(ee8(MT7996_PHY_EE_196(band)) & MT7996_PHY_EE_196_EPA);
	off = ee8(ADIE_DCOC_EE_OFF);
	dcoc_fail[band] = 0;
	if ((off & ADIE_DCOC_EE_OFF_FINE) && (off & ADIE_DCOC_EE_OFF_STEP))
		return RADIO_OK;
	for (c.wf = 0; c.wf < MT7996_PHY_BAND_RX_CHAINS(band); c.wf++) {
		if (c.mt7977) {
			c.list = c.wf == c.sx ? save_77_sx : save_77;
			c.n = c.wf == c.sx ? NELEM(save_77_sx) : NELEM(save_77);
		} else {
			c.list = c.wf == c.sx ? save_76_sx : save_76;
			c.n = c.wf == c.sx ? NELEM(save_76_sx) : NELEM(save_76);
		}
		n1 = hal_fail_rec.count;
		save(&c);
		if (hal_fail_rec.count != n1)
			return RADIO_ERR_HW;	/* never write back a value that was not read */
		/* The class the channel set wrote; MediaTek calibrates 20 / 40 / 80 MHz channels at the
		 * 20 MHz class (their records share it) and 160 / 320 MHz at their own. */
		code = saved_chain(&c, ADIE_RX_BW);
		c.cls = code <= ADIE_RX_BW_CODE_80MHZ ? ADIE_RX_BW_CODE_20MHZ : code;
		if (!(off & ADIE_DCOC_EE_OFF_FINE)) {
			if (c.mt7977)
				fine_mt7977(&c);
			else
				fine_mt7976(&c);
			tail(&c, 1);
		}
		if (!(off & ADIE_DCOC_EE_OFF_STEP)) {
			if (c.mt7977)
				step_mt7977(&c);
			else
				step_mt7976(&c);
			tail(&c, 0);
		}
	}
	return hal_fail_rec.count != n0 ? RADIO_ERR_HW : RADIO_OK;
}
