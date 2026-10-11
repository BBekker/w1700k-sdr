// SPDX-License-Identifier: MIT
/* The capture path set-up of MediaTek's SPECTRUM prime, by our WM (capture.h).
 *
 * What MediaTek's arm (vendor TESTMODE WM V:e00aba5e, hw/mt7996/capture_setup.h) writes on the
 * band and nothing undoes: in its order, the capture clear (CAPTRIG_CFG [7:0], WF_COMM +0x3008
 * [21] then [22], DFE MUX and MUX2 = 0), the band's capture clocks (BN_CLK0 / BN_CLK1 CAPTURE), the
 * capture configure (DFE CTRL EN, the domain-6 select cleared, +0x3008 [22:21] cleared, CAPTRIG_CFG
 * [7:0], DFE MODE = the node record's activation code, the lane word), the node's DFE mux fields
 * (SEL0, RATE0 by u4BW, ACT0) and the capture trigger (CAPTRIG, CAPTRIG_CFG [9:8], the trigger
 * event word). The request is the prime's: node MT7996_DFE_NODE_STOCK, trigger event 0,
 * u4BW from the bandwidth as MediaTek's (mt76 icap_bw) numbering, band index = PHY index = band.
 *
 * Left out of MediaTek's arm:
 *  - the ICAP engines (icap.h): ICAPCR0 reset pulse, EN, SRC_SEL, RING_BUF and CAPTURE, ADDR0,
 *    the ICAP2 packer, the stop cycle. They parameterise MediaTek's own one-shot window capture;
 *    the stream programs ICAP0 itself (host stream.py arm_writes: CR0, CR2, CR3, CR10; the
 *    producer: CR0) and uses neither ICAP1 nor ICAP2. ICAPCR8 is capture_select().
 *  - the route block (cap_route.h): the stream's arm writes every route word itself, and the
 *    prime's retire releases the grant anyway.
 *  - the vendor TM's manual-gain recipe around the start (V:e012e998: BN +0x8200 / +0x8d60 /
 *    +0x8550 saved and restored, RXFE_MAN, DFE +0xf008, RXGAIN of the request's path, LMAC
 *    ARB_SCR [9] pulsed): a gain hold for MediaTek's 50 us snapshot, restored by the same command;
 *    the stock TM's arm has none.
 *  - the capture clear runs inside MediaTek's per-instance loop, three identical times; once here.
 *  - the read of DFE LANE before the default lane word is written (its value is not used).
 * The retire (enable 0) clears ICAP CAPTURE and the route grant only: nothing here to undo.
 */
#include "hw/mt7996/capture_setup.h"
#include "hw/mt7996/dfe.h"
#include "hw/mt7996/icap.h"
#include "hw/mt7996/phy.h"
#include "hw/mt7996/spectrum_cmd.h"

#include "capture.h"
#include "radio.h"

#define NELEM(a)	(sizeof(a) / sizeof((a)[0]))

/* Channel bandwidth (chanset.h 0..3) -> u4BW (spectrum_cmd.h). */
static const uint8_t u4bw_of[] = {
	MT7996_SPECTRUM_BW_20, MT7996_SPECTRUM_BW_40, MT7996_SPECTRUM_BW_80, MT7996_SPECTRUM_BW_160,
};

/* MediaTek's record of the node: column 1 is used on PHY index 1, column 0 on the others. */
#define COL(i, v)	[i] = (v),
static const uint8_t node_sel[] = { MT7996_CAPNODE_STOCK_SEL(COL) };
static const uint8_t node_rate[] = { MT7996_CAPNODE_STOCK_RATE(COL) };	/* by u4BW */
static const uint8_t node_act[] = { MT7996_CAPNODE_STOCK_ACT(COL) };

_Static_assert(MT7996_CAPNODE_STOCK_KEY == MT7996_CAPSETUP_NODE, "the record is the prime's node");
_Static_assert(NELEM(u4bw_of) == RADIO_NBW, "a u4BW for every bandwidth class");
_Static_assert(NELEM(node_rate) > MT7996_SPECTRUM_BW_160, "a rate byte for every u4BW of u4bw_of");
/* A domain-0 node: SEL0 / RATE0 / ACT0 of DFE MUX, the domain-6 select cleared. */
_Static_assert(HW_GET(MT7996_DFE_NODE_DOMAIN, MT7996_CAPSETUP_NODE) == 0u, "domain-0 node");
#define ACT_IS_D0(i, v)	_Static_assert((v) == MT7996_DFE_MODE_DOMAIN0, "activation 0x11 -> MUX ACT0");
MT7996_CAPNODE_STOCK_ACT(ACT_IS_D0)
/* Category 2, config 4: the default lane word at every u4BW but 5 (capture_setup.h "Lane word"), so
 * at every one of u4bw_of. */
_Static_assert(HW_GET(MT7996_DFE_NODE_CATEGORY, MT7996_CAPSETUP_NODE) == 2u &&
	       HW_GET(MT7996_DFE_NODE_CONFIG, MT7996_CAPSETUP_NODE) == 4u, "the stock node's lane rule");
/* An event is given: CAPTRIG [19] set, [15:8] = 1, the event word written. */
_Static_assert(MT7996_CAPSETUP_TRIG_EVENT != MT7996_PHY_CAPTRIG_EVENT_NONE, "trigger event given");

uint32_t capture_setup(uint32_t band, uint32_t bw)
{
	uint32_t u4bw, col;

	if (band >= RADIO_BANDS || bw >= NELEM(u4bw_of))
		return RADIO_ERR_ARG;
	u4bw = u4bw_of[bw];
	col = band == MT7996_PHY_BAND_5G;	/* PHY index 1 */

	/* The capture clear, V:e012e30e(1). */
	hal_clr(MT7996_PHY_CAPTRIG_CFG(band), MT7996_PHY_CAPTRIG_CFG_SEL);
	hal_set(MT7996_PHY_WFCOMM_3008(band), MT7996_PHY_WFCOMM_3008_CAP_CLR_B21);
	hal_set(MT7996_PHY_WFCOMM_3008(band), MT7996_PHY_WFCOMM_3008_CAP_CLR_B22);
	hal_wr(MT7996_DFE_CAP_MUX(band), 0);
	hal_wr(MT7996_DFE_CAP_MUX2(band), 0);

	/* The band's capture clocks, V:e012e504 / V:e012e522. */
	hal_set(MT7996_PHY_BN_CLK0(band), MT7996_PHY_BN_CLK_CAPTURE);
	hal_set(MT7996_PHY_BN_CLK1(band), MT7996_PHY_BN_CLK_CAPTURE);

	/* The capture configure, V:e012e30e(0). */
	hal_set(MT7996_DFE_CAP_CTRL(band), MT7996_DFE_CAP_CTRL_EN);
	hal_clr(MT7996_DFE_CAP_D6(band), MT7996_DFE_CAP_D6_SEL);
	hal_clr(MT7996_PHY_WFCOMM_3008(band), MT7996_PHY_WFCOMM_3008_CAP_CLR);
	hal_wf(MT7996_PHY_CAPTRIG_CFG(band), MT7996_PHY_CAPTRIG_CFG_SEL, MT7996_PHY_CAPTRIG_CFG_SEL_CAPTURE);
	hal_wf(MT7996_DFE_CAP_MODE(band), MT7996_DFE_CAP_MODE_CODE, node_act[col]);
	hal_wr(MT7996_DFE_CAP_LANE(band), MT7996_DFE_CAP_LANE_FW_DEFAULT);

	/* The node's DFE mux fields, V:e012e5d2. */
	hal_wf(MT7996_DFE_CAP_MUX(band), MT7996_DFE_CAP_MUX_SEL0, node_sel[col]);
	hal_wf(MT7996_DFE_CAP_MUX(band), MT7996_DFE_CAP_MUX_RATE0, node_rate[u4bw]);
	hal_set(MT7996_DFE_CAP_MUX(band), MT7996_DFE_CAP_MUX_ACT0);

	/* The capture trigger, V:e012e540, with the prime's event. */
	hal_set(MT7996_PHY_CAPTRIG(band), MT7996_PHY_CAPTRIG_B19);
	hal_clr(MT7996_PHY_CAPTRIG(band), MT7996_PHY_CAPTRIG_B17);
	hal_set(MT7996_PHY_CAPTRIG(band), MT7996_PHY_CAPTRIG_B16);
	hal_set(MT7996_PHY_CAPTRIG(band), MT7996_PHY_CAPTRIG_B26);
	hal_wf(MT7996_PHY_CAPTRIG(band), MT7996_PHY_CAPTRIG_F15_8, MT7996_PHY_CAPTRIG_F15_8_START);
	hal_clr(MT7996_PHY_CAPTRIG_CFG(band), MT7996_PHY_CAPTRIG_CFG_F9_8);
	hal_wr(MT7996_PHY_CAPTRIG_EVENT(band), MT7996_CAPSETUP_TRIG_EVENT);

	return radio_failed() ? RADIO_ERR_HW : RADIO_OK;
}

/* MediaTek's V:e012e7da then SelWfBank per instance: the whole word (AFE_SEL for a band >= 2, else
 * 0), then WF_BANK_SEL = band[0] through the ROM's field write (one read-modify-write). */
void capture_select(uint32_t band)
{
	if (band >= RADIO_BANDS) {
		hal_fail(HAL_FAIL_ARG, band);
		return;
	}
	for (uint32_t n = 0; n < MT7996_ICAP_NUM; n++) {
		hal_wr(MT7996_ICAP_CR8(n), band >= MT7996_PHY_BAND_6G ? MT7996_ICAP_CR8_AFE_SEL : 0);
		hal_wf(MT7996_ICAP_CR8(n), MT7996_ICAP_CR8_WF_BANK_SEL, band);
	}
}
