// SPDX-License-Identifier: MIT
/* The ROM patch's radio init (MediaTek MCU_Patch_init), which MediaTek's ROM patch runs at patch
 * download and ours does not carry: CBTOP and XO, pin mux, the CONN AFE calibration pulse of
 * every band, the A-die TOP init by chip ID, the CONN pads and switch, and the A-die eFuse trims.
 * The steps, their order and what is left out are in hw/mt7996/conn.h "The ROM patch's radio
 * init" and hw/adie/adie.h "The ROM patch's TOP init and trims".
 */
#include "hw/adie/mt7977.h"
#include "hw/mt7996/conn.h"
#include "hw/mt7996/mcu_periph.h"
#include "hw/mt7996/secure.h"

#include "patch_init.h"
#include "radio.h"

#define TOP ADIE_K_TOP

static void delay_ticks(uint32_t n)
{
	hal_udelay(MT7996_ROM_DELAY_TIME_US(n));
}

/* Step 10: one die's AFE calibration pulse on one band, the die's analog-interface nibbles
 * raised around it. */
static void afe_pulse(uint32_t die, uint32_t band)
{
	uint32_t ctl00 = MT7996_CONN_AFE_CTL00(band), ctl08 = MT7996_CONN_AFE_CTL08(band);
	static const uint32_t cal[] = { CONN_AFE_CTL00_CAL20, CONN_AFE_CTL00_CAL19, CONN_AFE_CTL00_CAL18,
					CONN_AFE_CTL00_CAL17 };
	uint32_t i;

	if (radio_is_mt7977(die))
		for (i = 0; i < ADIE_NUM_K_WF; i++)
			hal_awr(die, TOP, MT7977_TOP_DCOC_WF(i), MT7977_TOP_AFE_PULSE);
	else
		hal_awr(die, TOP, ADIE_TOP_CHAIN_IF, ADIE_TOP_CHAIN_IF_AFE_PULSE);
	hal_set(ctl00, CONN_AFE_CTL00_PULSE);
	delay_ticks(CONN_AFE_PULSE_TICKS);
	hal_clr(ctl00, CONN_AFE_CTL00_PULSE);
	hal_set(ctl08, CONN_AFE_CTL08_CAL);
	delay_ticks(CONN_AFE_PULSE_TICKS);
	for (i = 0; i < 4; i++)
		hal_set(ctl00, cal[i]);
	delay_ticks(CONN_AFE_CAL_TICKS);
	for (i = 0; i < 4; i++)
		hal_clr(ctl00, cal[i]);
	hal_clr(ctl08, CONN_AFE_CTL08_CAL);
	if (radio_is_mt7977(die))
		for (i = 0; i < ADIE_NUM_K_WF; i++)
			hal_awr(die, TOP, MT7977_TOP_DCOC_WF(i), 0);
	else
		hal_awr(die, TOP, ADIE_TOP_CHAIN_IF, ADIE_TOP_CHAIN_IF_IDLE);
}

static void top_common(uint32_t die)
{
	hal_awr(die, TOP, ADIE_TOP_A00, ADIE_TOP_A00_INIT);
	hal_udelay(ADIE_TOP_A00_SETTLE_US);
	hal_awf(die, TOP, ADIE_TOP_A20, ADIE_TOP_A20_B76, 1);
}

/* Step 12: one die's TOP init by chip ID; 0, or -1 for a chip this board does not carry. */
static int top_init(uint32_t die, int sel_band1)
{
	uint32_t id = radio_id.chip_id[die];

	switch (id) {
	case ADIE_TOP_CHIP_ID_MT7976:
	case ADIE_CHIP_ID_MT7976_B:
		if (sel_band1)
			hal_awf(die, TOP, ADIE_TOP_4AC, ADIE_TOP_4AC_LO5, ADIE_TOP_4AC_LO5_VAL);
		top_common(die);
		hal_awr(die, TOP, ADIE_TOP_038, ADIE_TOP_038_MT7976);
		hal_awr(die, TOP, ADIE_TOP_65C, ADIE_TOP_65C_MT7976);
		hal_awr(die, TOP, ADIE_TOP_664, ADIE_TOP_664_MT7976);
		return 0;
	case ADIE_CHIP_ID_MT7976C_A:
	case ADIE_CHIP_ID_MT7976C_B:
	case ADIE_CHIP_ID_MT7976C_C:
		hal_awf(die, TOP, ADIE_TOP_4AC, ADIE_TOP_4AC_LO5, ADIE_TOP_4AC_LO5_VAL);
		top_common(die);
		hal_awr(die, TOP, ADIE_TOP_038, ADIE_TOP_038_MT7976);
		hal_awr(die, TOP, ADIE_TOP_65C, ADIE_TOP_65C_MT7976C);
		hal_awr(die, TOP, ADIE_TOP_664, ADIE_TOP_664_MT7976C);
		return 0;
	case ADIE_CHIP_ID_MT7977_V1:
	case ADIE_CHIP_ID_MT7977_V2:
	case ADIE_TOP_CHIP_ID_MT7977:
	case ADIE_CHIP_ID_MT7977_V3B: {
		int v3 = id == ADIE_TOP_CHIP_ID_MT7977 || id == ADIE_CHIP_ID_MT7977_V3B;

		top_common(die);
		hal_awr(die, TOP, ADIE_TOP_66C, ADIE_TOP_66C_MT7977);
		hal_awr(die, TOP, ADIE_TOP_65C, ADIE_TOP_65C_MT7977);
		hal_awr(die, TOP, ADIE_TOP_660, ADIE_TOP_660_MT7977);
		hal_awr(die, TOP, ADIE_TOP_664, ADIE_TOP_664_MT7977);
		hal_awr(die, TOP, ADIE_TOP_668, v3 ? ADIE_TOP_668_MT7977 : ADIE_TOP_668_MT7977_V12);
		hal_awr(die, TOP, ADIE_TOP_7E2, ADIE_TOP_7E2_MT7977);
		hal_awr(die, TOP, ADIE_TOP_7E4, 0);
		hal_awr(die, TOP, ADIE_TOP_7E5, ADIE_TOP_7E5_MT7977);
		for (uint32_t i = ADIE_TOP_7E6; i <= ADIE_TOP_7E8; i++)
			hal_awr(die, TOP, i, 0);
		hal_awr(die, TOP, MT7977_TOP_7E9, MT7977_TOP_7E9_VAL);
		hal_awr(die, TOP, ADIE_TOP_220, v3 ? ADIE_TOP_220_MT7977 : ADIE_TOP_220_MT7977_V12);
		hal_awr(die, TOP, ADIE_TOP_034, v3 ? ADIE_TOP_034_MT7977 : ADIE_TOP_034_MT7977_V12);
		hal_armw(die, TOP, ADIE_TOP_030, ADIE_TOP_030_B18, 0);
		hal_armw(die, TOP, ADIE_TOP_030, ADIE_TOP_030_B18, ADIE_TOP_030_B18);
		return 0;
	}
	}
	hal_fail(HAL_FAIL_STATE, id);
	return -1;
}

/* Step 22: the die's trim from its eFuse byte, when the byte is marked valid. */
static void trim(uint32_t die)
{
	uint8_t b;

	if (hal_efuse8(MT7996_EFUSE_ADIE_TRIM(die), &b) || !(b & ADIE_TRIM_EFUSE_VALID))
		return;
	if (radio_is_mt7977(die))
		hal_awf(die, TOP, ADIE_TOP_220, ADIE_TOP_220_TRIM, HW_GET(ADIE_TRIM_EFUSE_CODE, b));
	else
		hal_awf(die, TOP, ADIE_TOP_034, ADIE_TOP_034_TRIM, HW_GET(ADIE_TRIM_EFUSE_CODE, b));
}

/* Step 7, E1 parts only. */
static void e1_path(void)
{
	uint32_t s18 = !!(hal_rd(MT7996_CBTOP_PAD_GPIO) & CBTOP_PAD_GPIO_B18);
	const uint32_t seq[] = CBTOP_HW_BND_E1_SEQ(s18);
	uint32_t i, b;

	hal_set(MT7996_CBTOP_00000, CBTOP_00000_E1_B18);
	hal_set(MT7996_CBTOP_00000, CBTOP_00000_E1_B19);
	for (i = 0; i < 4; i++)
		hal_wr(MT7996_CBTOP_HW_BND, seq[i]);
	delay_ticks(CBTOP_E1_STEP_TICKS);
	hal_wr(MT7996_CBTOP_HW_BND, CBTOP_HW_BND_E1_FINAL(s18));
	delay_ticks(CBTOP_E1_STEP_TICKS);
	hal_set(MT7996_CBTOP_00000, CBTOP_00000_E1_B17);
	delay_ticks(1);
	hal_set(MT7996_CBTOP_00000, CBTOP_00000_E1_B16);
	delay_ticks(1);
	delay_ticks(CBTOP_E1_AFE_TICKS);
	for (b = 0; b < 2; b++) {
		hal_clr(MT7996_CONN_AFE_CTL04(b), CONN_AFE_CTL04_LO);
		hal_clr(MT7996_CONN_AFE_CTL04(b), CONN_AFE_CTL04_HI);
		hal_clr(MT7996_CONN_AFE_CTL0C(b), CONN_AFE_CTL0C_MODE);
	}
	delay_ticks(1);
	for (b = 0; b < 2; b++) {
		hal_wf(MT7996_CONN_AFE_CTL04(b), CONN_AFE_CTL04_LO, CONN_AFE_CTL04_E1_VAL);
		hal_wf(MT7996_CONN_AFE_CTL04(b), CONN_AFE_CTL04_HI, CONN_AFE_CTL04_E1_VAL);
		hal_wf(MT7996_CONN_AFE_CTL0C(b), CONN_AFE_CTL0C_MODE, CONN_AFE_CTL0C_E1_VAL);
	}
}

uint32_t patch_init(void)
{
	uint32_t strap = hal_rd(MT7996_CBTOP_PAD_GPIO), die, k;
	uint8_t sel = 0;
	int band1;

	/* Only the W1700K's combination, MT7976 + two MT7977 with all three dies (the others change
	 * which dies and bands every step touches). */
	if (HW_GET(CBTOP_PAD_GPIO_ADIE_COMB, strap) != CBTOP_PAD_GPIO_ADIE_COMB_677 ||
	    (strap & CBTOP_PAD_GPIO_DIE2_ONLY)) {
		hal_fail(HAL_FAIL_STATE, strap);
		return RADIO_ERR_HW;
	}
	hal_wr(MT7996_CBTOP_27040, CBTOP_27040_INIT);
	hal_wr(MT7996_CBTOP_27014, CBTOP_27014_INIT);
	hal_wr(MT7996_CBTOP_01028, CBTOP_01028_INIT);
	hal_wr(MT7996_MCU_CFG_210, MCU_CFG_210_INIT);
	hal_set(MT7996_CBTOP_0015C, CBTOP_0015C_EN);
	hal_wf(MT7996_CBTOP_XO_CTL, CBTOP_XO_CTL_REQ, CBTOP_XO_CTL_REQ_ON);
	/* MediaTek only prints when the XO is not ready; so do we (soft). */
	hal_poll(MT7996_CBTOP_XO_STA, CBTOP_XO_STA_READY, HW_PREP(CBTOP_XO_STA_READY, CBTOP_XO_STA_READY_VAL),
		 CBTOP_XO_READY_TICKS * MT7996_ROM_DELAY_TIME_US(1), 1);
	/* MediaTek reads the chip IDs at its step 12; nothing before needs them. */
	radio_identify();
	if (radio_id.hw_rev == CBTOP_HW_REV_E1)
		e1_path();
	for (k = 0; k < CBTOP_PINMUX_N; k++)
		hal_wr(MT7996_CBTOP_PINMUX(k), CBTOP_PINMUX_INIT);
	/* The ROM's own eFuse section table (ours is installed, not MediaTek's) does not reach this
	 * D-die byte: a failed read counts as not selected, which is what MediaTek's load leaves on
	 * this unit too (TOP 0x4ac [4:0] = 0). */
	band1 = !hal_efuse8_soft(MT7996_EFUSE_7976_SEL, &sel) && sel == EFUSE_7976_SEL_BAND1;
	for (die = 0; die < RADIO_DIES; die++)
		afe_pulse(die, die);
	if (band1)
		afe_pulse(0, 1);
	for (die = 0; die < RADIO_DIES; die++)
		if (top_init(die, band1))
			return RADIO_ERR_HW;
	hal_set(MT7996_CBTOP_25000, CBTOP_25000_B24);
	hal_udelay(CBTOP_25000_SETTLE_US);
	hal_clr(MT7996_CBTOP_25000, CBTOP_25000_LOW);
	hal_set(MT7996_CBTOP_25000, CBTOP_25000_B8);
	hal_wf(MT7996_CBTOP_25000, CBTOP_25000_MODE, 1);
	/* Step 14 clears AFE CTL04 / CTL0C only on other A-die combinations (conn.h). */
	hal_wr(MT7996_CBTOP_PINMUX_320, CBTOP_PINMUX_320_INIT);
	hal_set(MT7996_CONN_PAD_E018, CONN_PAD_B3);
	hal_set(MT7996_CONN_PAD_E018, CONN_PAD_B1);
	hal_wf(MT7996_CONN_SWITCH_010, CONN_SWITCH_010_LO5, CONN_SWITCH_010_VAL);
	hal_set(MT7996_CONN_PAD_E00C, CONN_PAD_B3);
	hal_set(MT7996_CONN_PAD_E00C, CONN_PAD_B1);
	hal_wf(MT7996_CONN_PAD_E104, CONN_PAD_LO5, CONN_PAD_E104_VAL);
	hal_set(MT7996_CONN_PAD_E000, CONN_PAD_B3);
	hal_set(MT7996_CONN_PAD_E000, CONN_PAD_B1);
	hal_clr(MT7996_CONN_PAD_E008, CONN_PAD_B1);
	hal_clr(MT7996_CONN_PAD_E008, CONN_PAD_B0);
	hal_clr(MT7996_CONN_PAD_E014, CONN_PAD_B0);
	hal_wf(MT7996_CONN_PAD_E10C, CONN_PAD_BYTE0, CONN_PAD_E10C_B0_VAL);
	hal_set(MT7996_CONN_PAD_E108, CONN_PAD_B0);
	hal_wf(MT7996_CONN_PAD_E10C, CONN_PAD_BYTE1, CONN_PAD_E10C_B1_VAL);
	hal_set(MT7996_CONN_PAD_E108, CONN_PAD_B1);
	hal_wf(MT7996_CONN_PAD_E11C, CONN_PAD_BYTE1, CONN_PAD_E11C_VAL);
	hal_set(MT7996_CONN_PAD_E118, CONN_PAD_B1);
	hal_wf(MT7996_CONN_PAD_E11C, CONN_PAD_BYTE0, CONN_PAD_E11C_VAL);
	hal_set(MT7996_CONN_PAD_E118, CONN_PAD_B0);
	for (k = 0; k < CONN_SWITCH_KS; k++) {
		hal_rmw(MT7996_CONN_SWITCH_SEL, CONN_SWITCH_SEL_K(k), CONN_SWITCH_SEL_VAL << (8 * k));
		hal_set(MT7996_CONN_SWITCH_EN, CONN_SWITCH_EN_K(k));
	}
	for (die = 0; die < RADIO_DIES; die++)
		trim(die);
	return radio_failed() ? RADIO_ERR_HW : RADIO_OK;
}
