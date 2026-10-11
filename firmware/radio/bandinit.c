// SPDX-License-Identifier: MIT
/* The PHY init outside the calibrations (bandinit.h): MediaTek's common-phase PHY init, decoded
 * record for record against its cold stock load, as receive-only code.
 *
 * Left out, as not needed for reception (each by its decoded role):
 *  - before the calibration request: the thermal manager's temperature conversions, the DPD
 *    scratch words of V:e00e1316, the LMAC ARB_SCR TX / RX disable (already set at reset, written
 *    back unchanged), and the calibration init's bookkeeping (WF_COMM +0xd434, the ROM patch's
 *    console print, 0x8301fc10, +0xd7d8..+0xd7e8);
 *  - in the per-band init: WF_COMM +0xd430 (MT7996_PHY_DPD_GAIN_ERR, a word only DPD uses) and
 *    the MemDPD seed V:e00d98ac (DPD state; it reads ten words and writes none here);
 *  - in the tail: the thermal conversions, the PHY register backup (reads), HEMU_RULE4, the TX /
 *    RX state machine's ARB_SCR rewrite, the TMAC frame power limits, and the per-chain +0x2000
 *    clear of V:e012ee16 (written otherwise only by the TX power paths of PHY table A).
 * Everything else MediaTek writes there is here, in its order, rows of unknown role included.
 */
#include "hw/adie/adie.h"
#include "hw/adie/adie_init.h"
#include "hw/adie/mt7976.h"
#include "hw/adie/mt7977.h"
#include "hw/adie/rfmodes.h"
#include "hw/mt7996/conn.h"
#include "hw/mt7996/phy.h"
#include "hw/mt7996/phy_cal.h"
#include "hw/mt7996/phy_init.h"
#include "hw/mt7996/phy_map.h"

#include "bandinit.h"
#include "radio.h"

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))

/* ------------------------------------------------------------------------------------------------
 * Configuration: the chip SKU strap, AdieSel and the band's die (V:e00d4344, V:e00d41e4)
 * ------------------------------------------------------------------------------------------------ */

struct phy_cfg {
	uint32_t sku;			/* MediaTek's a_die_sku */
	uint8_t sel[RADIO_BANDS];	/* AdieSel: the band's A-die type */
	uint8_t die[RADIO_BANDS];	/* the band's die */
};

static const uint8_t adiesel_tbl[][RADIO_BANDS] = MT7996_PHY_FCB_CFG_INIT;

/* One 3-bit field of EEPROM WIFI_CONF (band select, TX / RX path count). */
static uint32_t ee_conf(uint32_t f)
{
	return HW_GET(MT7996_PHY_EE_CONF_FIELD(f), ee8(MT7996_PHY_EE_CONF_OFF(f)));
}

static int is_part(uint32_t die, uint32_t part)
{
	return HW_GET(ADIE_TOP_CHIP_ID_PART, radio_id.chip_id[die]) == part;
}

/* Reads the strap and derives every band's AdieSel and die as MediaTek's calibration init does;
 * refuses (nonzero) unless each present band's AdieSel names the chip its die reports. */
static int phy_config(struct phy_cfg *c)
{
	uint32_t eco = radio_eco(), row, band;

	if (!radio_id.identified) {
		hal_fail(HAL_FAIL_STATE, BANDINIT_FAIL_ID);
		return -1;
	}
	c->sku = HW_GET(CBTOP_PAD_GPIO_SKU, hal_rd(MT7996_CBTOP_PAD_GPIO));
	if ((c->sku & CBTOP_PAD_GPIO_SKU_ROW6_MASK) == CBTOP_PAD_GPIO_SKU_ROW6)
		row = MT7996_PHY_ADIESEL_ROW_SKU;
	else if (eco == MT7996_PHY_ADIESEL_ECO_ROW5)
		row = MT7996_PHY_ADIESEL_ROW_ECO5;
	else if (eco == MT7996_PHY_ADIESEL_ECO_ROW4)
		row = MT7996_PHY_ADIESEL_ROW_ECO4;
	else
		row = HW_GET(CBTOP_PAD_GPIO_SKU_COMB, c->sku);
	for (band = 0; band < RADIO_BANDS; band++) {
		uint32_t sel = adiesel_tbl[row][band], die = band, ok;

		if ((eco & MT7996_PHY_ADIESEL_ECO_REMAP) == MT7996_PHY_ADIESEL_ECO_REMAP)
			die = band == MT7996_PHY_BAND_6G ? ADIE_DIE_MT7977_6G : ADIE_DIE_MT7976;
		c->sel[band] = sel;
		c->die[band] = die;
		if (!ee_conf(MT7996_PHY_EE_CONF_BAND_SEL(band)))
			continue;	/* band absent: MediaTek skips it */
		ok = (sel == MT7996_PHY_ADIESEL_MT7976 && is_part(die, ADIE_TOP_CHIP_ID_PART_MT7976)) ||
		     (sel == MT7996_PHY_ADIESEL_MT7977 && is_part(die, ADIE_TOP_CHIP_ID_PART_MT7977));
		if (!ok) {
			hal_fail(HAL_FAIL_STATE, BANDINIT_FAIL_ADIE(band));
			return -1;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------------------------------------
 * Common init: the FEM / antenna-switch pin mux (V:e00cea48 per band; adie_init.h)
 * ------------------------------------------------------------------------------------------------ */

struct fem_top_row {
	uint16_t reg;
	uint32_t mask, val;
};

#define FEM_TOP_ROW(reg, mask, val)	{ reg, mask, val },
static const struct fem_top_row fem_top_tbl[] = { ADIE_FEM_TOP_TBL(FEM_TOP_ROW) };
static const struct fem_top_row fem_top_tbl_744[] = { ADIE_FEM_TOP_TBL_744(FEM_TOP_ROW) };

struct fem_pin_field {
	uint16_t reg;
	uint32_t mask;
};

#define FEM_PIN_FIELD(reg, mask)	{ reg, mask },
static const struct fem_pin_field fem_pin_fields[ADIE_FEM_PINS] = { ADIE_FEM_PIN_FIELDS(FEM_PIN_FIELD) };

#define FEM_REG(reg)			reg,
static const uint16_t fem_readback[] = { ADIE_FEM_READBACK(FEM_REG) };
static const uint16_t fem_readback_744[] = { ADIE_FEM_READBACK_744(FEM_REG) };

static const uint8_t fem_set_tbl[][RADIO_BANDS] = ADIE_FEM_SET_INIT;
static const uint8_t fem_tmpl_type1[ADIE_FEM_PINS] = ADIE_FEM_TMPL_TYPE1;
static const uint8_t fem_tmpl_type2[ADIE_FEM_PINS] = ADIE_FEM_TMPL_TYPE2;
static const uint8_t fem_tmpl_type4[ADIE_FEM_PINS] = ADIE_FEM_TMPL_TYPE4;

/* V:e00cea26: the WF a pin code refers to. */
static uint32_t fem_pin_wf(uint32_t code)
{
	uint32_t sig = HW_GET(ADIE_FEM_CODE_SIG, code);

	if (sig <= ADIE_FEM_SIG_WF_LAST)
		return sig % ADIE_NUM_K_WF;
	if (sig <= ADIE_FEM_SIG_KEEP_LAST)
		return 0;
	return sig - ADIE_FEM_SIG_IDX_FIRST;
}

static void fem_pin_mux(uint32_t band, uint32_t sku)
{
	uint32_t die = band, ee19c = ee8(MT7996_PHY_EE_19C(band));
	uint32_t tx_path = ee_conf(MT7996_PHY_EE_CONF_TX_PATH(band));
	uint32_t rx_path = ee_conf(MT7996_PHY_EE_CONF_RX_PATH(band));
	uint32_t band_sel = ee_conf(MT7996_PHY_EE_CONF_BAND_SEL(band));
	uint32_t set = fem_set_tbl[HW_GET(CBTOP_PAD_GPIO_SKU_COMB, sku)][band];
	uint32_t tbtc = !!(sku & CBTOP_PAD_GPIO_SKU_TBTC), shared = !!(ee19c & MT7996_PHY_EE_19C_SHARED);
	uint32_t top0, w744 = 0, field[ADIE_FEM_PINS], mode[ADIE_FEM_PINS], i, j, n;
	const struct fem_top_row *tbl;
	const uint8_t *tmpl;
	const uint16_t *rb;

	/* 0. the band's FEM, its type and pin set (V:e00ce6a0, V:e00cea48) */
	if (!(tx_path | rx_path) || !(ee8(MT7996_PHY_EE_196(band)) & (MT7996_PHY_EE_196_EPA | MT7996_PHY_EE_196_ELNA)))
		return;
	if (tbtc && band == MT7996_PHY_BAND_2G4) {
		tmpl = fem_tmpl_type4;
		top0 = ADIE_FEM_4A0_TYPE4;
	} else if (band_sel == MT7996_PHY_EE_BAND_SEL_2G4) {
		tmpl = fem_tmpl_type1;
		top0 = ADIE_FEM_4A0_TYPE1;
	} else if (band_sel > MT7996_PHY_EE_BAND_SEL_2G4 && band_sel <= MT7996_PHY_EE_BAND_SEL_LAST) {
		tmpl = fem_tmpl_type2;
		top0 = ADIE_FEM_4A0_TYPE2;
	} else {
		return;
	}

	/* 1. V:e00ce91a: the set's TOP words, row 0 by type */
	if (set == ADIE_FEM_SET_744) {
		tbl = fem_top_tbl_744;
		n = ARRAY_SIZE(fem_top_tbl_744);
	} else {
		tbl = fem_top_tbl;
		n = ARRAY_SIZE(fem_top_tbl);
	}
	for (i = 0; i < n; i++)
		hal_armw(die, ADIE_K_TOP, tbl[i].reg, tbl[i].mask, (i ? tbl[i].val : top0) & tbl[i].mask);

	/* 2. the pins' field codes and mode bits (V:e00cec30..V:e00ced5e) */
	for (j = 0; j < ADIE_FEM_PINS; j++) {
		uint32_t code = tmpl[j], wf = fem_pin_wf(code), grp = HW_GET(ADIE_FEM_CODE_GRP, code);
		uint32_t pol = 0, unused = 0, off = 0;

		if (grp == ADIE_FEM_GRP_TX) {
			pol = !!(ee19c & MT7996_PHY_EE_19C_POL_TX);
			unused = !tbtc && wf >= tx_path;
		} else if (grp == ADIE_FEM_GRP_RX) {
			pol = !!(ee19c & (wf == ADIE_K_WF(4) ? MT7996_PHY_EE_19C_POL_RX_WF4 : MT7996_PHY_EE_19C_POL_RX));
			unused = !tbtc && wf >= rx_path;
		} else if (grp == ADIE_FEM_GRP_OTHER) {
			pol = !!(ee19c & MT7996_PHY_EE_19C_POL_OTHER);
		}
		field[j] = code;
		mode[j] = 0;
		if (!code) {
			off = 1;
		} else if (HW_GET(ADIE_FEM_CODE_SIG, code) <= ADIE_FEM_SIG_KEEP_LAST) {
			mode[j] = pol;
			off = unused;
		} else if (shared) {
			off = unused;
		} else if (code == ADIE_FEM_CODE_9B) {
			field[j] = ADIE_FEM_FIELD_9B;
			off = unused;
		} else {
			off = 1;
		}
		if (off) {
			if (set == ADIE_FEM_SET_744)
				w744 |= HW_PREP(MT7976_TOP_FEM_PIN(j), MT7976_TOP_FEM_PIN_OFF);
			else
				field[j] = ADIE_FEM_FIELD_OFF;
		}
	}

	/* 3. V:e00ce9c6: the set's switch word */
	if (set == ADIE_FEM_SET_744)
		hal_awr(die, ADIE_K_TOP, MT7976_TOP_FEM, w744);
	else if (set == ADIE_FEM_SET_49D)
		hal_armw(die, ADIE_K_TOP, ADIE_TOP_FEM_49D, shared ? 0 : ADIE_TOP_FEM_49D_CLR,
			 shared ? ADIE_TOP_FEM_49D_SET : 0);

	/* 4. V:e00ce81e: per pin the mode bit, then the field */
	for (j = 0; j < ADIE_FEM_PINS; j++) {
		hal_armw(die, ADIE_K_TOP, ADIE_TOP_FEM_4A4, ADIE_TOP_FEM_4A4_PINMODE(j),
			 mode[j] ? ADIE_TOP_FEM_4A4_PINMODE(j) : 0);
		hal_awf(die, ADIE_K_TOP, fem_pin_fields[j].reg, fem_pin_fields[j].mask,
			HW_GET(ADIE_FEM_CODE_SIG, field[j]));
	}

	/* 5. V:e00ce7b6: MediaTek reads the set's words back and discards them */
	if (set == ADIE_FEM_SET_744) {
		rb = fem_readback_744;
		n = ARRAY_SIZE(fem_readback_744);
	} else {
		rb = fem_readback;
		n = ARRAY_SIZE(fem_readback);
	}
	for (i = 0; i < n; i++)
		hal_ard(die, ADIE_K_TOP, rb[i]);
}

void phy_common_init(void)
{
	struct phy_cfg c;
	uint32_t band;

	if (phy_config(&c))
		return;
	for (band = 0; band < RADIO_BANDS; band++)
		fem_pin_mux(band, c.sku);
}

/* ------------------------------------------------------------------------------------------------
 * Per-band init (V:e00de622, V:e00dbfe6, V:e00de5f6, V:e00d8ffe, V:e00d9028)
 * ------------------------------------------------------------------------------------------------ */

static const uint32_t gsel_map[] = MT7996_PHY_GSEL_MAP_INIT;
static const uint32_t gsel0_grp_7976[] = MT7996_PHY_GSEL0_GRP_MT7976;
static const uint32_t gsel0_step_7976[] = MT7996_PHY_GSEL0_STEP_MT7976;
static const uint32_t gsel1_grp_7976[] = MT7996_PHY_GSEL1_GRP_MT7976;
static const uint32_t gsel1_step_7976[] = MT7996_PHY_GSEL1_STEP_MT7976;
static const uint32_t gsel0_grp_7977[] = MT7996_PHY_GSEL0_GRP_MT7977;
static const uint32_t gsel0_step_7977[] = MT7996_PHY_GSEL0_STEP_MT7977;
static const uint32_t gsel1_grp_7977[] = MT7996_PHY_GSEL1_GRP_MT7977;
static const uint32_t gsel1_step_7977[] = MT7996_PHY_GSEL1_STEP_MT7977;
static const uint32_t rxtd_coef_hi[MT7996_PHY_BN_RXTD_COEF_N] = MT7996_PHY_BN_RXTD_COEF_HI_INIT;
static const uint32_t rxtd_coef_lo[MT7996_PHY_BN_RXTD_COEF_N] = MT7996_PHY_BN_RXTD_COEF_LO_INIT;
static const uint32_t phyintf_2aa0[] = MT7996_PHY_BN_PHYINTF_2AA0_INIT;
static const uint32_t phyintf_2b00[] = MT7996_PHY_BN_PHYINTF_2B00_INIT;

struct reg_val {
	uint32_t reg, val;
};

#define REG_VAL(reg, val)	{ reg, val },
static const struct reg_val regtbl_perchain[] = { MT7996_PHY_INIT_REGTBL_PERCHAIN(REG_VAL) };
static const struct reg_val regtbl_abs[] = { MT7996_PHY_INIT_REGTBL_ABS(REG_VAL) };

static void wr_words(uint32_t (*reg)(uint32_t band, uint32_t n), uint32_t band, const uint32_t *v, uint32_t n)
{
	for (uint32_t i = 0; i < n; i++)
		hal_wr(reg(band, i), v[i]);
}

static uint32_t gsel_map0(uint32_t band, uint32_t n) { return MT7996_PHY_GSEL_MAP(band, 0, n); }
static uint32_t gsel_map1(uint32_t band, uint32_t n) { return MT7996_PHY_GSEL_MAP(band, 1, n); }
static uint32_t gsel_grp0(uint32_t band, uint32_t n) { return MT7996_PHY_GSEL_GRP(band, 0, n); }
static uint32_t gsel_grp1(uint32_t band, uint32_t n) { return MT7996_PHY_GSEL_GRP(band, 1, n); }
static uint32_t gsel_step0(uint32_t band, uint32_t n) { return MT7996_PHY_GSEL_STEP(band, 0, n); }
static uint32_t gsel_step1(uint32_t band, uint32_t n) { return MT7996_PHY_GSEL_STEP(band, 1, n); }
static uint32_t phyintf_a(uint32_t band, uint32_t n) { return MT7996_PHY_BN_PHYINTF_2AA0(band, n); }
static uint32_t phyintf_b(uint32_t band, uint32_t n) { return MT7996_PHY_BN_PHYINTF_2B00(band, n); }

/* V:e00de622 up to its TOP init call: the band's PHY words. */
static void band_words(uint32_t band, uint32_t sel)
{
	uint32_t k, n;

	hal_set(MT7996_PHY_WFCOMM_D208(band), MT7996_PHY_WFCOMM_D208_B1);
	for (k = 0; k < MT7996_PHY_INIT_CHAINS; k++)
		hal_wr(MT7996_PHY_RFINTF_728C(band, k), MT7996_PHY_RFINTF_728C_INIT);
	if (band != MT7996_PHY_BAND_2G4) {
		/* V:e00df274: the 'mode 16' values of the WM PHY preparation */
		hal_set(MT7996_PHY_WFCOMM_D234(band), MT7996_PHY_WFCOMM_D234_B8);
		hal_wf(MT7996_PHY_WFCOMM_D230(band), MT7996_PHY_WFCOMM_D230_F5_4, MT7996_PHY_WFCOMM_D230_F5_4_INIT);
		hal_wf(MT7996_PHY_WFCOMM_D240(band), MT7996_PHY_WFCOMM_D240_LO, MT7996_PHY_WFCOMM_D240_INIT);
	} else {
		/* V:e00de6f8: band 0's DFE_BN_TX words */
		hal_wf(MT7996_PHY_DFE_E334, MT7996_PHY_DFE_BYTE0, MT7996_PHY_DFE_E334_BYTE0_INIT);
		for (n = 0; n < MT7996_PHY_DFE_E2X_N; n++) {
			hal_wf(MT7996_PHY_DFE_E2A4(band, n), MT7996_PHY_DFE_BYTE2, MT7996_PHY_DFE_E2A4_BYTE2_INIT);
			hal_wf(MT7996_PHY_DFE_E2A4(band, n), MT7996_PHY_DFE_BYTE1, MT7996_PHY_DFE_E2A4_BYTE1_INIT);
			hal_wf(MT7996_PHY_DFE_E2A4(band, n), MT7996_PHY_DFE_BYTE0, MT7996_PHY_DFE_E2A4_BYTE0_INIT);
			hal_wf(MT7996_PHY_DFE_E2C0(band, n), MT7996_PHY_DFE_BYTE3, MT7996_PHY_DFE_E2C0_BYTE3_INIT);
			hal_wf(MT7996_PHY_DFE_E2C0(band, n), MT7996_PHY_DFE_BYTE2, MT7996_PHY_DFE_E2C0_BYTE2_INIT);
			hal_wf(MT7996_PHY_DFE_E2C0(band, n), MT7996_PHY_DFE_BYTE1, MT7996_PHY_DFE_E2C0_BYTE1_INIT);
		}
	}
	hal_wf(MT7996_PHY_DFE_E330(band), MT7996_PHY_DFE_BYTE2, MT7996_PHY_DFE_E330_BYTE2_INIT);
	hal_wf(MT7996_PHY_DFE_E330(band), MT7996_PHY_DFE_BYTE1, MT7996_PHY_DFE_E330_BYTE1_INIT);
	hal_wf(MT7996_PHY_DFE_E330(band), MT7996_PHY_DFE_BYTE0, MT7996_PHY_DFE_E330_BYTE0_INIT);
	for (k = 0; k < MT7996_PHY_INIT_TBL_CHAINS; k++)
		hal_wf(MT7996_PHY_CHAIN_87AC(band, k), MT7996_PHY_CHAIN_87AC_ROUTE, MT7996_PHY_CHAIN_87AC_ROUTE_INIT);

	/* the gain-index selectors: identity maps, then the A-die type's groups and steps */
	wr_words(gsel_map0, band, gsel_map, ARRAY_SIZE(gsel_map));
	wr_words(gsel_map1, band, gsel_map, ARRAY_SIZE(gsel_map));
	if (sel == MT7996_PHY_ADIESEL_MT7975 || sel == MT7996_PHY_ADIESEL_MT7976) {
		wr_words(gsel_grp0, band, gsel0_grp_7976, ARRAY_SIZE(gsel0_grp_7976));
		wr_words(gsel_step0, band, gsel0_step_7976, ARRAY_SIZE(gsel0_step_7976));
		wr_words(gsel_grp1, band, gsel1_grp_7976, ARRAY_SIZE(gsel1_grp_7976));
		wr_words(gsel_step1, band, gsel1_step_7976, ARRAY_SIZE(gsel1_step_7976));
	} else {
		wr_words(gsel_grp0, band, gsel0_grp_7977, ARRAY_SIZE(gsel0_grp_7977));
		wr_words(gsel_step0, band, gsel0_step_7977, ARRAY_SIZE(gsel0_step_7977));
		wr_words(gsel_grp1, band, gsel1_grp_7977, ARRAY_SIZE(gsel1_grp_7977));
		wr_words(gsel_step1, band, gsel1_step_7977, ARRAY_SIZE(gsel1_step_7977));
	}

	hal_wr(MT7996_PHY_WFCOMM_C0FC(band), MT7996_PHY_WFCOMM_C0FC_INIT);
	hal_wr(MT7996_PHY_WFCOMM_C0A4(band), MT7996_PHY_WFCOMM_C0A4_INIT);
	hal_set(MT7996_PHY_WFCOMM_D4F0(band), MT7996_PHY_WFCOMM_D4F0_B31);
	hal_wr(MT7996_PHY_WFCOMM_38E4(band), MT7996_PHY_WFCOMM_38E4_INIT);
	hal_wf(MT7996_PHY_WFCOMM_38F0(band), MT7996_PHY_WFCOMM_38F0_LO, MT7996_PHY_WFCOMM_38F0_LO_INIT);
	hal_wf(MT7996_PHY_WFCOMM_3824(band), MT7996_PHY_WFCOMM_3824_LO, MT7996_PHY_WFCOMM_3824_LO_INIT);
	hal_wf(MT7996_PHY_WFCOMM_3824(band), MT7996_PHY_WFCOMM_3824_HI, MT7996_PHY_WFCOMM_3824_HI_INIT);
	hal_wf(MT7996_PHY_WFCOMM_38B4(band), MT7996_PHY_WFCOMM_38B4_LO, MT7996_PHY_WFCOMM_38B4_LO_INIT);
	hal_wf(MT7996_PHY_WFCOMM_38C4(band), MT7996_PHY_WFCOMM_38C4_HI, MT7996_PHY_WFCOMM_38C4_HI_INIT);
	hal_clr(MT7996_PHY_WFCOMM_3844(band), MT7996_PHY_WFCOMM_3844_B19);
	hal_clr(MT7996_PHY_WFCOMM_D480(band), MT7996_PHY_WFCOMM_D480_B20);
	hal_wf(MT7996_PHY_WFCOMM_D4CC(band), MT7996_PHY_WFCOMM_D4CC_F15_8, MT7996_PHY_WFCOMM_D4CC_F15_8_INIT);
	hal_clr(MT7996_PHY_BN_8240(band), MT7996_PHY_BN_8240_F23_19);

	/* RFINTF +0x268 / +0x26c: chains 0..3 cleared; chain 4 parked with +0x268 [31:28] = 0xf */
	for (k = 0; k < MT7996_PHY_INIT_CHAINS; k++) {
		hal_clr(MT7996_PHY_RFINTF_7268(band, k), MT7996_PHY_RFINTF_7268_LO);
		hal_wf(MT7996_PHY_RFINTF_7268(band, k), MT7996_PHY_RFINTF_7268_HI,
		       k < MT7996_PHY_INIT_TBL_CHAINS ? 0 : MT7996_PHY_RFINTF_7268_HI_CH4);
		hal_clr(MT7996_PHY_RFINTF_726C(band, k), MT7996_PHY_RFINTF_726C_LO);
	}

	hal_clr(MT7996_PHY_BN_8590(band), MT7996_PHY_BN_85X0_B15);
	hal_clr(MT7996_PHY_BN_8590(band), MT7996_PHY_BN_85X0_B14);
	hal_clr(MT7996_PHY_BN_85A0(band), MT7996_PHY_BN_85X0_B15);
	hal_clr(MT7996_PHY_BN_85A0(band), MT7996_PHY_BN_85X0_B14);
	hal_clr(MT7996_PHY_WFCOMM_3034(band), MT7996_PHY_WFCOMM_3034_F11_9);
	hal_set(MT7996_PHY_BN_8E30(band), MT7996_PHY_BN_8E30_B30);
	hal_clr(MT7996_PHY_BN_8E30(band), MT7996_PHY_BN_8E30_B29);
	for (n = 0; n < MT7996_PHY_BN_RXTD_COEF_N; n++) {
		hal_wf(MT7996_PHY_BN_RXTD_COEF(band, n), MT7996_PHY_BN_RXTD_COEF_HI, rxtd_coef_hi[n]);
		if (!(n & 1))
			hal_wf(MT7996_PHY_BN_RXTD_COEF(band, n), MT7996_PHY_BN_RXTD_COEF_LO, rxtd_coef_lo[n]);
	}
	wr_words(phyintf_a, band, phyintf_2aa0, ARRAY_SIZE(phyintf_2aa0));
	wr_words(phyintf_b, band, phyintf_2b00, ARRAY_SIZE(phyintf_2b00));
	hal_clr(MT7996_PHY_WFCOMM_303C(band), MT7996_PHY_WFCOMM_303C_B15);
	hal_set(MT7996_PHY_BN_0044(band), MT7996_PHY_BN_0044_B31);
	hal_clr(MT7996_PHY_BN_0044(band), MT7996_PHY_BN_0044_F10_8);
	hal_clr(MT7996_PHY_BN_0044(band), MT7996_PHY_BN_0044_B26);
	hal_clr(MT7996_PHY_BN_8234(band), MT7996_PHY_BN_8234_B0);
	hal_wf(MT7996_PHY_BN_10A4(band), MT7996_PHY_BN_10A4_LO, MT7996_PHY_BN_10A4_LO_INIT);
	hal_wf(MT7996_PHY_BN2_10A4(band), MT7996_PHY_BN_10A4_LO, MT7996_PHY_BN_10A4_LO_INIT);
	hal_wf(MT7996_PHY_BN_6094(band), MT7996_PHY_BN_6094_F21_20, MT7996_PHY_BN_6094_F21_20_INIT);
	hal_wf(MT7996_PHY_BN2_6094(band), MT7996_PHY_BN_6094_F21_20, MT7996_PHY_BN_6094_F21_20_INIT);

	/* the D-die revision (V:e00defa8): E1 parts, or every other revision */
	if (radio_id.hw_rev == CBTOP_HW_REV_E1) {
		hal_wf(MT7996_PHY_WFCOMM_B580(band), MT7996_PHY_WFCOMM_B580_F24_23, MT7996_PHY_WFCOMM_B580_F24_23_VAL);
		hal_clr(MT7996_PHY_BN_8590(band), MT7996_PHY_BN_85X0_B15);
		hal_clr(MT7996_PHY_BN_8590(band), MT7996_PHY_BN_85X0_B14);
		hal_clr(MT7996_PHY_BN_85A0(band), MT7996_PHY_BN_85X0_B15);
		hal_clr(MT7996_PHY_BN_85A0(band), MT7996_PHY_BN_85X0_B14);
		hal_clr(MT7996_PHY_BN_8E30(band), MT7996_PHY_BN_8E30_B31);
	} else {
		for (k = 0; k < MT7996_PHY_INIT_TBL_CHAINS; k++)
			hal_set(MT7996_PHY_CHAIN_D0D0(band, k), MT7996_PHY_CHAIN_D0D0_B0);
	}

	hal_wf(MT7996_PHY_WFCOMM_C104(band), MT7996_PHY_WFCOMM_C104_F31_30, MT7996_PHY_WFCOMM_C104_F31_30_INIT);
	hal_set(MT7996_PHY_WFCOMM_C0FC(band), MT7996_PHY_WFCOMM_C0FC_B7);
	hal_clr(MT7996_PHY_WFCOMM_303C(band), MT7996_PHY_WFCOMM_303C_F12_11);
	hal_clr(MT7996_PHY_WFCOMM_3040(band), MT7996_PHY_WFCOMM_3040_F31_23);
	hal_clr(MT7996_PHY_WFCOMM_3044(band), MT7996_PHY_WFCOMM_3044_F2_0);
}

/* V:e00dbfe6: the TOP init (adie.h "TOP init"). Every IFMODE access keeps [26:24] (HAZARD). */
static void top_init(uint32_t band, uint32_t die, uint32_t sel)
{
	uint32_t k;

	hal_set(MT7996_PHY_WFCOMM_D480(band), MT7996_PHY_WFCOMM_D480_B19);
	hal_clr(MT7996_PHY_WFCOMM_D480(band), MT7996_PHY_WFCOMM_D480_B19);
	if (sel == MT7996_PHY_ADIESEL_MT7976 || sel == MT7996_PHY_ADIESEL_MT7977) {
		hal_armw(die, ADIE_K_TOP, ADIE_TOP_DC4, ADIE_TOP_DC4_F7_3, 0);
		hal_armw(die, ADIE_K_TOP, ADIE_TOP_DC4, 0, ADIE_TOP_DC4_F7_3);
	}
	if (sel == MT7996_PHY_ADIESEL_MT7976) {
		hal_armw(die, ADIE_K_TOP, ADIE_TOP_4AC, 0, ADIE_TOP_4AC_B17);
		for (k = 0; k < ADIE_NUM_K_WF; k++)
			hal_armw(die, ADIE_K_WF(k), ADIE_TSSI_479, ADIE_TSSI_479_B29, 0);
		hal_awf(die, ADIE_K_TOP, ADIE_TOP_IFMODE, ADIE_TOP_IFMODE_LO, ADIE_TOP_IFMODE_LO_INIT);
		hal_awf(die, ADIE_K_TOP, ADIE_TOP_068, ADIE_TOP_068_F23_20, ADIE_TOP_068_F23_20_INIT);
		hal_armw(die, ADIE_K_TOP, ADIE_TOP_068, 0, ADIE_TOP_068_B30);
		hal_armw(die, ADIE_K_TOP, ADIE_TOP_4AC, 0, ADIE_TOP_4AC_B16);
		hal_set(MT7996_PHY_WFCOMM_D49C(band), MT7996_PHY_WFCOMM_D49C_B30);
		hal_clr(MT7996_PHY_WFCOMM_D49C(band), MT7996_PHY_WFCOMM_D49C_B31);
		hal_set(MT7996_PHY_WFCOMM_D4A0(band), MT7996_PHY_WFCOMM_D4A0_B22);
		return;
	}
	/* MT7977 (phy_config() admits no other type) */
	hal_set(MT7996_PHY_WFCOMM_D4A0(band), MT7996_PHY_WFCOMM_D4A0_B20);
	hal_clr(MT7996_PHY_WFCOMM_D4A0(band), MT7996_PHY_WFCOMM_D4A0_B21);
	hal_set(MT7996_PHY_WFCOMM_D4A0(band), MT7996_PHY_WFCOMM_D4A0_B22);
	hal_set(MT7996_PHY_WFCOMM_D49C(band), MT7996_PHY_WFCOMM_D49C_B30);
	hal_clr(MT7996_PHY_WFCOMM_D49C(band), MT7996_PHY_WFCOMM_D49C_B31);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_4AC, 0, ADIE_TOP_4AC_B17);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_4AC, ADIE_TOP_4AC_B18, 0);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_4AC, 0, ADIE_TOP_4AC_B16);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_068, 0, ADIE_TOP_068_B30);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_068, ADIE_TOP_068_B3, 0);
	hal_awf(die, ADIE_K_TOP, ADIE_TOP_IFMODE, ADIE_TOP_IFMODE_LO, ADIE_TOP_IFMODE_LO_INIT);
	hal_awf(die, ADIE_K_TOP, ADIE_TOP_068, ADIE_TOP_068_F23_20, ADIE_TOP_068_F23_20_INIT);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_068, 0, ADIE_TOP_068_B30);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_068, 0, ADIE_TOP_068_B3);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_4AC, 0, ADIE_TOP_4AC_B16);
	hal_armw(die, ADIE_K_TOP, ADIE_TOP_4AC, 0, ADIE_TOP_4AC_B17);
	hal_set(MT7996_PHY_WFCOMM_D49C(band), MT7996_PHY_WFCOMM_D49C_B30);
	hal_set(MT7996_PHY_WFCOMM_D49C(band), MT7996_PHY_WFCOMM_D49C_B31);
	hal_set(MT7996_PHY_WFCOMM_D4A0(band), MT7996_PHY_WFCOMM_D4A0_B22);
	hal_set(MT7996_PHY_WFCOMM_D4A0(band), MT7996_PHY_WFCOMM_D4A0_B20);
	hal_awr(die, ADIE_K_TOP, ADIE_TOP_57C, ADIE_TOP_57C_INIT);
	hal_awr(die, ADIE_K_TOP, ADIE_TOP_57C, 0);
	hal_awr(die, ADIE_K_TOP, ADIE_TOP_57C, ADIE_TOP_57C_INIT);
}

/* V:e00de5f6: the band-0 / band-1 WF PLL clock gated off and on (fixed addresses, every band). */
static void pll_gate_pulse(void)
{
	hal_wr(MT7996_WFSYS_ON_LPCR_DIS(0), WFSYS_ON_LPCR_DIS_PLL_CKEN);
	hal_wr(MT7996_WFSYS_ON_LPCR_DIS(1), WFSYS_ON_LPCR_DIS_PLL_CKEN);
	hal_wr(MT7996_WFSYS_ON_LPCR_EN(0), WFSYS_ON_LPCR_EN_PLL_CKEN);
	hal_wr(MT7996_WFSYS_ON_LPCR_EN(1), WFSYS_ON_LPCR_EN_PLL_CKEN);
}

/* V:e00df096..V:e00df264: the per-chain table pages, then +0xd37c / +0xd3b8 / +0xd3bc. */
static void chain_pages(uint32_t band)
{
	uint32_t k, i;

	for (k = 0; k < MT7996_PHY_INIT_TBL_CHAINS; k++) {
		for (i = 0; i < MT7996_PHY_CHAIN_9000_N; i++)
			hal_wr(MT7996_PHY_CHAIN_9000(band, k, i), MT7996_PHY_CHAIN_9000_INIT);
		for (i = 0; i < MT7996_PHY_CHAIN_9000_N; i++)
			hal_wr(MT7996_PHY_CHAIN_9500(band, k, i), 0);
		for (i = 0; i < MT7996_PHY_CHAIN_9000_N; i++)
			hal_wr(MT7996_PHY_CHAIN_9A00(band, k, i), 0);
		for (i = 0; i < MT7996_PHY_CHAIN_9000_N; i++)
			hal_wr(MT7996_PHY_CHAIN_B000(band, k, i), 0);
		for (i = 0; i < MT7996_PHY_CHAIN_B500_N; i++)
			hal_wr(MT7996_PHY_CHAIN_B500(band, k, i), 0);
	}
	for (k = 0; k < MT7996_PHY_INIT_TBL_CHAINS; k++)
		hal_clr(MT7996_PHY_CHAIN_D37C(band, k), MT7996_PHY_CHAIN_D37C_LO);
	for (k = 0; k < MT7996_PHY_INIT_TBL_CHAINS; k++)
		hal_wf(MT7996_PHY_CHAIN_D3B8(band, k), MT7996_PHY_CHAIN_D3BX_LO, MT7996_PHY_CHAIN_D3B8_INIT);
	for (k = 0; k < MT7996_PHY_INIT_TBL_CHAINS; k++)
		hal_wf(MT7996_PHY_CHAIN_D3BC(band, k), MT7996_PHY_CHAIN_D3BX_LO, MT7996_PHY_CHAIN_D3BC_INIT);
}

/* V:e00d8ffe, V:e00d9028: the register tables (phy_init.h), + band << 20. */
static void reg_tables(uint32_t band)
{
	uint32_t b = band * MT7996_PHY_BAND_STRIDE, k, i;

	for (k = 0; k < MT7996_PHY_INIT_TBL_CHAINS; k++)
		for (i = 0; i < ARRAY_SIZE(regtbl_perchain); i++)
			hal_wr(regtbl_perchain[i].reg + k * MT7996_PHY_CHAIN_STRIDE + b, regtbl_perchain[i].val);
	for (i = 0; i < ARRAY_SIZE(regtbl_abs); i++)
		hal_wr(regtbl_abs[i].reg + b, regtbl_abs[i].val);
}

void phy_band_init(uint32_t band)
{
	struct phy_cfg c;

	if (band >= RADIO_BANDS) {
		hal_fail(HAL_FAIL_ARG, band);
		return;
	}
	if (!ee_conf(MT7996_PHY_EE_CONF_BAND_SEL(band)))
		return;		/* band absent (V:e00d6ec2) */
	if (phy_config(&c))
		return;
	band_words(band, c.sel[band]);
	top_init(band, c.die[band], c.sel[band]);
	pll_gate_pulse();
	chain_pages(band);
	reg_tables(band);
}

/* ------------------------------------------------------------------------------------------------
 * The PHY-init routine's tail (V:e00b06a2's per-band handlers, V:e00b06bc)
 * ------------------------------------------------------------------------------------------------ */

/* V:e00e2314 -> V:e0130752 mode 2: chain 0xe90 [30] = !(EEPROM 0x199 + band [3]) on chains 0..3. */
static void chain_e90(uint32_t band)
{
	for (uint32_t k = 0; k < MT7996_PHY_INIT_TBL_CHAINS; k++)
		hal_armw(band, ADIE_K_WF(k), ADIE_CHAIN_E90, 0, ADIE_CHAIN_E90_B30);
}

/* V:e00cd44e: die 0's TOP 0xb08 / 0x664 and dies 1 and 2's TOP 0x668 by EEPROM 0x199 + die [2]. */
static void fem_words(void)
{
	uint32_t fem0 = ee8(MT7996_PHY_EE_199(ADIE_DIE_MT7976)) & MT7996_PHY_EE_199_FEM, die;

	hal_armw(ADIE_DIE_MT7976, ADIE_K_TOP, MT7976_TOP_B08, MT7976_TOP_B08_FEM_A | MT7976_TOP_B08_FEM_B,
		 fem0 ? HW_PREP(MT7976_TOP_B08_FEM_A, MT7976_TOP_B08_FEM_ON) |
			HW_PREP(MT7976_TOP_B08_FEM_B, MT7976_TOP_B08_FEM_ON) : 0);
	hal_armw(ADIE_DIE_MT7976, ADIE_K_TOP, MT7976_TOP_664, MT7976_TOP_664_FEM, fem0 ? 0 : MT7976_TOP_664_FEM);
	for (die = ADIE_DIE_MT7977_5G; die <= ADIE_DIE_MT7977_6G; die++) {
		uint32_t fem = ee8(MT7996_PHY_EE_199(die)) & MT7996_PHY_EE_199_FEM;

		hal_armw(die, ADIE_K_TOP, MT7977_TOP_TTG_668, MT7977_TOP_TTG_668_FEM, fem ? 0 : MT7977_TOP_TTG_668_FEM);
	}
}

/* V:e00b096e -> V:e012f064: die 2's crystal trim, EEPROM 0x1269 plus the signed offset 0x126f
 * (V:e00e1228, sign-magnitude), into TOP 0x654 [30:24]; the same word to 0x658. */
static void xtal_trim(void)
{
	uint32_t ofs = ee8(ADIE_EE_XTAL_TRIM_OFS), mag = HW_GET(ADIE_EE_XTAL_TRIM_OFS_MAG, ofs);
	uint32_t trim = ee8(ADIE_EE_XTAL_TRIM) + (ofs & ADIE_EE_XTAL_TRIM_OFS_POS ? mag : -mag);
	uint32_t v = hal_armw(ADIE_TOP_XTAL_TRIM_DIE, ADIE_K_TOP, ADIE_TOP_XTAL_TRIM0, ADIE_TOP_XTAL_TRIM_VAL,
			      HW_PREP(ADIE_TOP_XTAL_TRIM_VAL, trim));

	hal_awr(ADIE_TOP_XTAL_TRIM_DIE, ADIE_K_TOP, ADIE_TOP_XTAL_TRIM1, v);
}

void phy_post_init(void)
{
	struct phy_cfg c;
	uint32_t band;

	if (phy_config(&c))
		return;
	if (c.sku & CBTOP_PAD_GPIO_SKU_FEM_ALT) {
		hal_fail(HAL_FAIL_STATE, BANDINIT_FAIL_SKU_FEM);
		return;
	}
	if (c.sku & CBTOP_PAD_GPIO_SKU_XTAL_DIE0) {
		hal_fail(HAL_FAIL_STATE, BANDINIT_FAIL_SKU_XTAL);
		return;
	}
	for (band = 0; band < RADIO_BANDS; band++) {
		if (ee8(MT7996_PHY_EE_199(band)) & MT7996_PHY_EE_199_E90) {
			hal_fail(HAL_FAIL_STATE, BANDINIT_FAIL_E90(band));
			return;
		}
	}
	/* per band, as MediaTek's handler table runs them: the FEM words and the trim each time */
	for (band = 0; band < RADIO_BANDS; band++) {
		chain_e90(band);
		fem_words();
		xtal_trim();
	}
}
