/* SPDX-License-Identifier: MIT */
/*
 * A-die chain states, input/route selects, calibration and test modes.
 *
 * ADDRESS SPACE: 12-bit A-die register indices in a chain bank k 0..4, or in
 * the TOP bank (k 15) where marked (see adie.h).
 *
 * Every calibration and test state of a chain is set through one override
 * pair: ADIE_CHAIN_OVR[31] hands the chain to software, ADIE_CHAIN_EN then
 * enables its blocks bit by bit. Normal operation: OVR = 0 (hardware/PHY
 * control). The firmware uses about a dozen distinct states (below) and
 * restores every register it changed, ending with 0x011 then 0x010. [static]
 *
 * Search result: no calibration, test or engineering state, and no single
 * bit among about 36,900 flips (MT7977 chain 0 0x000..0x31d and 0xa00..0xbff,
 * MT7977 SX bank 0xa00..0xbff, MT7976 bank 0 0x000..0x218, 0x600..0x6ff,
 * 0x980..0xbc6), routes the antenna into the TIA/ADC without the mixer or
 * puts new spectrum into the capture. Every firmware mode reaches baseband
 * through the mixer, or through loop/feedback/TTG paths that need an internal
 * transmitter or tone. [measured][static]
 *
 * Confidence tags: [measured], [static], [inferred]; "HAZARD:" as in adie.h.
 */
#ifndef HW_ADIE_RFMODES_H
#define HW_ADIE_RFMODES_H

#include "../hw_bits.h"

/* ------------------------------------------------------------------------
 * Chain override and block enables
 * ------------------------------------------------------------------------ */

/* 0x011  Chain override. RW. Observed 0 (normal). [31] = software control
 * of the chain's blocks through 0x010. [static][measured] */
#define ADIE_CHAIN_OVR                  0x011u
#define ADIE_CHAIN_OVR_MANUAL           HW_BIT(31)

/* 0x010  Chain block enables, honoured while 0x011[31] = 1. RW. Observed 0
 * on chains at rest, 0x0003fbcb on die-0 k 4 (monitor receiver). Bit groups
 * from the firmware's states [static]; roles [inferred] from co-occurrence:
 *   base {0,6,7,8,9,13}                     standby (0x23c1)
 *   RX   {1,3,11,12,14,15,16,17}            receive, LNA to ADC driver
 *        14..16 are the RF front end (LNA/mixer): the only RX bits dropped
 *        in every feedback (bit 27) state
 *   TX   {2,19..24}
 *   27   feedback receiver: RX baseband without the RF front end
 *   28, 29  TX detector paths (seen only with TX on)
 *   30   RX plus an auxiliary input (TTG tone, same-chain TX->RX loop)
 *   31   DC-offset calibration
 * Effect on a receiving MT7977 chain with a wired/OTA tone [measured]:
 * standby -14 dB floor; feedback-only (27) no input; 28 or 29 mute;
 * 30 normal; dropping any of 14/15/16 costs 5..8 dB; 31 adds 9 dB.
 * On the MT7976 the bit-30 state mutes the antenna. [measured]
 * Bit 17 also appears with TX and no other RX bit (0x01fa23c5 = TX | bit
 * 17: T2{6}'s transient and the MT7977 TX loop-DC state, phy_cal.h mask bit
 * 10, P[6] = 1, group != 0; group 0 uses ADIE_CHAIN_EN_TX and the P[6] = 0
 * path ADIE_CHAIN_EN_TX_DC), so it may belong to TX [inferred]. */
#define ADIE_CHAIN_EN                   0x010u
#define ADIE_CHAIN_EN_BASE              0x000023c1u
#define ADIE_CHAIN_EN_RX_MASK           0x0003d80au	/* bits 1,3,11,12,14..17 */
#define ADIE_CHAIN_EN_RXFE_MASK         0x0001c000u	/* bits 14..16 */
#define ADIE_CHAIN_EN_TX_MASK           0x01f80004u	/* bits 2,19..24 */
#define ADIE_CHAIN_EN_FB                HW_BIT(27)
#define ADIE_CHAIN_EN_TXDET_B           HW_BIT(28)
#define ADIE_CHAIN_EN_TXDET_A           HW_BIT(29)
#define ADIE_CHAIN_EN_AUX               HW_BIT(30)
#define ADIE_CHAIN_EN_DCOC              HW_BIT(31)
/* Whole-word states the firmware writes. [static] */
#define ADIE_CHAIN_EN_STANDBY           0x000023c1u
#define ADIE_CHAIN_EN_RX                0x0003fbcbu
#define ADIE_CHAIN_EN_RX_DCOC           0x8003fbcbu
#define ADIE_CHAIN_EN_RX_AUX            0x4003fbcbu	/* TTG-tone RX cals, every chain (0x011 manual) */
#define ADIE_CHAIN_EN_TX                0x01f823c5u
#define ADIE_CHAIN_EN_TX_DC             0x01f803c5u
#define ADIE_CHAIN_EN_TX_DETA           0x20f803c7u	/* TX LO / IQ cal T1{9,13,17}; no [24] */
#define ADIE_CHAIN_EN_TX_DETB           0x107803c7u	/* TX IQ cal T1{8,12,16}; no [24:23] */
#define ADIE_CHAIN_EN_LOOP              0x40fbfbcfu	/* same-chain TX -> RX (T1{7,11,15}) */
#define ADIE_CHAIN_EN_TX_FB             0x09fa3bcfu	/* TX + feedback RX (MT7976 T2{4}; DPD with P[6] = 0) */
#define ADIE_CHAIN_EN_FB_ONLY           0x08003bcbu	/* MT7977 feedback chain */
/* 0x087a3bcf: feedback receiver ([27]) with RX baseband (no RF front end
 * 14..16) and TX-side bits {2,17,19..22}: the MT7977 TSSI-DNL (T2{4})
 * feedback partner of a transmitting chain (k4 for WF 0/1, k0 for WF 2/3).
 * 0x01f823c7: TX plus bit 1, the MT7977 TSSI PA-cell calibration (T2{5})
 * on the WF. [static, measured] */
#define ADIE_CHAIN_EN_TSSI_FB           0x087a3bcfu
#define ADIE_CHAIN_EN_TX_TSSI           0x01f823c7u

/* 0x000  Chain mode word. RW. [31:24]: 0 normal; 0x82 DC-offset cal (the
 * MT7977 writes it to the SX bank k 4; its TX loop DC writes 0x82000000 to
 * the WF pair's first bank, k0 for WF 0/1 and k2 for WF 2/3, not to the
 * chain itself, and its P[6] = 0 path ends with 0x84000000 on the chain);
 * 0x84 loop / TSSI; 0x88 monitor receiver (die-0 k 4 runs 0x88000000) and
 * the chain in the coarse per-step DC cal. Observed 0 on chains, 0x25000000
 * on die-0 k 1. On a receiving MT7977 chain 0x82 and 0x84 mute, 0x88 is
 * normal. [static][measured] */
#define ADIE_CHAIN_MODE                 0x000u
#define ADIE_CHAIN_MODE_SEL             HW_GENMASK(31, 24)
#define ADIE_CHAIN_MODE_SEL_NORMAL      0x00u
#define ADIE_CHAIN_MODE_SEL_DCOC        0x82u
#define ADIE_CHAIN_MODE_SEL_LOOP        0x84u
#define ADIE_CHAIN_MODE_SEL_MONITOR     0x88u

/* 0x001  Chain control. RW. Observed 0 (0x300 on die-0 k 1). [9:8] = 3 in
 * every manual calibration (the hardware SPI injector toggles die-0 k 4's
 * [9:8] for TX blanking); [17:16] = 2 for DC cal and on every bank in the
 * TTG RX, the MT7976 T2{4}, the TX gain-step (T1{7,11,15}, the same-chain
 * loopback), the TX IQ (T1{8,12,16}) and the TX LO / IQ (T1{9,13,17})
 * calibrations, 3 on the calibrated WF for the TX loop DC (T1{6,10,14}) and
 * the TSSI path (T2{5}).
 * [static][measured] */
#define ADIE_CHAIN_CTL                  0x001u
#define ADIE_CHAIN_CTL_ON               HW_GENMASK(9, 8)
#define ADIE_CHAIN_CTL_CALPATH          HW_GENMASK(17, 16)
#define   ADIE_CHAIN_CTL_CALPATH_DC     2u	/* DC calibration (adie.h DCOC) */

/* 0x003  RX input select. RW. [31:29]: observed 6 (0xc0000000) on MT7977
 * chains, 5 (0xa0000000) on MT7976 chains; 7 (0xe0000000) is the 2.4 GHz
 * monitor input on die-0 k 4, 6 its 5 GHz input; 5 is also the MT7976
 * loop-pair RX input, and DPD writes 0xa0000000 to its MT7976 feedback
 * chain (below). On a receiving MT7977 chain any value other than 6 or 7
 * costs 4.5 dB. [static][measured] */
#define ADIE_RX_INPUT                   0x003u
#define ADIE_RX_INPUT_SEL               HW_GENMASK(31, 29)
#define ADIE_RX_INPUT_SEL_MT7977        6u
#define ADIE_RX_INPUT_SEL_MT7976        5u
#define ADIE_RX_INPUT_SEL_MON_2G4       7u
#define ADIE_RX_INPUT_SEL_MON_5G        6u

/* ------------------------------------------------------------------------
 * Loop and feedback networks
 * ------------------------------------------------------------------------ */

/* MT7977 on-die loop network (calibration path writes; live values 0):
 * 0x630 ([7:0], [15:12], [23:20], [31:24]), 0x631 = 0x17000f00, 0x741,
 * 0x007..0x009, 0x128 / 0x129, 0x12d. Applied alone it loads RX by 4.5 dB;
 * with the feedback state and 0x149 = 6 it gives no input. [static][measured]
 * T2{4} (V:e0066aa4 mode 0x14) sets it on the transmitting WF's feedback
 * partner bank (k4 for WF 0/1, k0 for WF 2/3), in this order: 0x630 [7:0] =
 * 0x8f, [15:12] = 0xf, [23:20] = 9, [31:24] = 0x87; 0x631 = 0x17000f00;
 * 0x12d [19:0] = 0x14444; 0x009 [31] = 1; 0x008 [31] = 1; 0x007 = 0xc001;
 * 0x741 [20:0] = 0x181814; 0x128 = 0x00a00000; 0x129 = 0x00e00000; then the
 * partner bank's TOP word (mt7977.h MT7977_TOP_DCOC_WF: 0x04a for k4, 0x043
 * for k0) = 0x20000000; every chain 0x477 [31] = 0, 0x041 [27] = 1. Chain
 * states: the WF ADIE_CHAIN_EN_TX, the partner ADIE_CHAIN_EN_TSSI_FB, the
 * others standby; k0 and k4 0x149 [31:28] = 0. All restored.
 * [static, measured] */
#define ADIE_LOOP_630                   0x630u
#define ADIE_LOOP_631                   0x631u
/* 0x741 [4:0] = 0x14 in three MT7977 writers: the loop network (T2{4},
 * [20:0] = 0x181814), T2{5} ([4] = 1, [2:0] = 4, [20] = 1, [19:16] = 3) and
 * the TX IQ calibration on the WF (T1{8,12,16}, below); the TX LO / IQ
 * calibration (T1{9,13,17}, below) writes [4] = 1, [2:0] = 0, [20] = 1 and
 * [19:16] = the step's LPF index (its RX gain HAL step, mode 0) on the WF.
 * [static, measured] */
#define ADIE_LOOP_741                   0x741u
#define ADIE_LOOP_741_LO                HW_GENMASK(4, 0)
#define ADIE_LOOP_741_B4                HW_BIT(4)
#define ADIE_LOOP_741_B20               HW_BIT(20)
#define ADIE_LOOP_741_LPF               HW_GENMASK(19, 16)
#define ADIE_LOOP_007                   0x007u
#define ADIE_LOOP_008                   0x008u
#define ADIE_LOOP_009                   0x009u
#define ADIE_LOOP_128                   0x128u
#define ADIE_LOOP_129                   0x129u
#define ADIE_LOOP_12D                   0x12du
#define ADIE_TX_041                     0x041u	/* [27] set with TX (T2{4}; the MT7976 TX detector) */
/* 0x042  MT7976: [24] set with 0x041 [27] by the TX detector path (T2{6})
 * and the TX loop DC (phy_cal.h mask bit 10); the TX LO / IQ calibration
 * (T1{9,13,17}, below) writes 0x041 = 0x08000000 and 0x042 = 0x01000000
 * (whole words) on the WF; restored. [static][measured] */
#define ADIE_TX_042                     0x042u
#define ADIE_TX_042_B24                 HW_BIT(24)
/* 0x201  [9:0]: 0x300 at rest on MT7977 k 0..3 (0x18 on k 4); the TSSI
 * detector path writes 8, T2{5} 0x100, the MT7977 TX loop DC 0x300 with P[6]
 * = 1 ([9:1] cleared, then [9:8] = 3), 8 with P[6] = 0. Restored. [guess: TX
 * bias / enable word] [static][measured] */
#define ADIE_TX_201                     0x201u
#define ADIE_TX_201_LO                  HW_GENMASK(9, 0)
/* 0x149  MT7977 feedback path select. RW. Observed 0x3ff. Feedback chain:
 * [10:0] = 6, [31:28] = 0; [31:29] = 6 on k 4 with a chain in TX+FB.
 * Bit 31 cleared on a receiving chain kills reception. [static][measured] */
#define ADIE_FB_SEL                     0x149u
#define ADIE_FB_SEL_PATH                HW_GENMASK(10, 0)

/* DPD (T2{0..3}, HAL mode 2 with a4 = P[6] = 1; phy_cal.h), MT7976 per TX
 * WF (k = WF). [static, measured]
 * Chain states (V:e0062e84, e00630b6..): TX k 0x010 = ADIE_CHAIN_EN_TX; the
 * feedback chain (table 0xe0080468: WF0->WF1, WF1->WF2, WF2->WF3, WF3->WF4
 * = the monitor receiver with the TTG as LO, WF4->WF2) 0x010 =
 * ADIE_CHAIN_EN_RX (receive); for WF 1..3 one more neighbour standby
 * (0x23c1); 0x011 = 0x80000000 on these; the feedback chain's 0x003 =
 * 0xa0000000 (ADIE_RX_INPUT_SEL_MT7976). With a4 = 0 (not in the stock
 * load) the same HAL step writes ADIE_CHAIN_EN_TX_FB on the WF, as T2{4}.
 * Path (V:e006366a mode 2, a4 = 1): TX 0x605 [1] = 1; TOP 0x070 = 5 (WF 0,
 * 2), 0x00808005 with TOP 0x4c4 = 2 and k2 0x022 [23] = 1 (WF 1),
 * 0x80800005 with TOP 0x4c4 = 0x20 and k4 0x022 [23] = 1 (WF 3; WF 1/3 with
 * WM flag 0xe01ec1c4 [7]: 0x070 = 5 only); P[5]: TX 0x003 [31], [29] (WF
 * 0/1); feedback 0x600 (adie.h ADIE_DCOC_STEP) [16] = 1, [11:8] = 0, [4:1] =
 * AGC gain code 0..9 (WM bounds 0xe01ec1fc / 0xe01ec200).
 * Also TX k 0x046 [31] = 1 for the call; TOP 0x744 (mt7976.h
 * MT7976_TOP_FEM) [21:20] = 3 for the call and, per step, [24] (calibration
 * group 0, else [26]) on the TOP of the die in calibration variable 0x19
 * (this die here); the coupling 0x631 = 0x633 = 0x00086420 on the feedback
 * chain; WF 3: the TTG at F - 20 MHz (sx.h: mult 2, INT/FRAC of
 * V:e0048534(2, 5, 40 MHz, F - 20 MHz), HAL7 mode 4 on every chain) and the
 * WF4 monitor powered as in mt7976.h (0x003 [31:30] |= 3 instead of =
 * 0xe0000000). A 17-register save (0x631, 0x633 on k0..k4; k4 0x101, 0x122,
 * 0x000, 0x105, 0x605, 0x003, 0x941) is written back after every step; if
 * k4 0x105 then reads 0x80000000 the TTG is stopped (0x943 [0] pulse, 0x606
 * [31:30] = 3, 0x942 [27:24] = 6, 0x941 = saved). TX gain: chain 0x200 [16]
 * = 1, [6:0] = code (HAL V:e0052612); 0x605 [11:8] / 0x004 [29], [18:16] =
 * HAL5 (tables 0xe0087164 / 0xe007168c). */
#define ADIE_DPD_COUPLE_631             0x631u
#define ADIE_DPD_COUPLE_633             0x633u
#define ADIE_DPD_COUPLE_VAL             0x00086420u
/* Feedback-chain RX DC calibration in every DPD step (V:e004d8a8 ->
 * V:e005ef86, MT7976; adie.h DCOC): fb 0x010 [31] = 1, 0x011 = 0x80000000,
 * 0xe90 [31:30] = 1 (saved), k0 0xa0f [1:0] = 3, 0x923 [21] = 1, [15:0] =
 * 0, TOP 0x070 = 0x88888005 (saved), 0x607 = 0x80008000
 * (MT7976_DCOC_607_FINE), 0x922 [27:26] = 2, [17] = 1, 10 us, [31] = 1, 20
 * us, wait 0x98e [15], 0x922 [31] = 0, 0x607 = 0x8000, TOP 0x070 / 0xe90
 * back, 0x010 [31] = 0, k0 0xa0f [1:0] = 0; 0x080 [31] set: 0x171 [11]
 * pulsed low 10 us. The result words 0x8e0.. are not restored (phy_cal.h
 * HAZARD). [static, measured]
 * DPD on the MT7977 (V:e0067b88 mode 2): TX chain 0x121 [9] = 1, [13:10] =
 * word 0xe0080360 (1), 0x004 [29] = 1, [18:16] = 3, then as T2{4}
 * (ADIE_TSSI_121); the feedback partner's 0x121 [13:10] = word
 * 0xe008034c[g] (mt7977.h MT7977_DPD_FB_GAIN_TBL; g = AGC gain code 0..10,
 * start 5: 7, 5, 4, 3, 2, 1, 0xd, 0, 0xa, 9, 8); TX gain 0x240 [30] = 1,
 * 0x241 [27:21] = code. Chain states (ADIE_CHAIN_EN_TX, partner
 * ADIE_CHAIN_EN_TSSI_FB), loop network and FEM as T2{4}. [static, measured]
 */

/* TSSI detector path (MT7977 cal values; live in brackets): 0x745 =
 * 0x03000000 [0x3000], 0x201 = 8 [0x300], 0x121 = 0x08000000 [0], 0x4a0 =
 * 0x00080600 [0x80000], 0x001 = 0x00030000 [0], 0x402 = 0x30000040 [0x80],
 * 0x478 = 0x10000004 [0x30000004], 0x479 = 0x18000000, 0x477 = 0xd0ff1007 /
 * 0xb0ff1007 / 0xf0ff1007 [0xd0f01007]. A TX-detector state showed no
 * response to any external tone 150 MHz..5.5 GHz. [static][measured]
 * These are the MT7977 detector select V:e0058be4 with its first flag
 * (WM 0xe01ec1e6); with the second (0xe01ec1e7, the stock T2{6} path) it
 * writes 0x4a0 = 0x84480600, 0x745 = 0x03000000, 0x001 = 0x00030000 and
 * 0x402 = 0x00300040 instead, the T2{5} values below. [static]
 * 0x477: [31] cleared on every chain by T2{4}, T2{5} and the MT7977 TX LO /
 * IQ calibration (T1{9,13,17}, below); [15] strobes a TX
 * detector reading into 0x5c4, [14] + [15] one into 0x460 (T2{6}).
 * [static, measured] */
#define ADIE_TSSI_477                   0x477u
#define ADIE_TSSI_477_STROBE            HW_BIT(15)
#define ADIE_TSSI_477_STROBE2           HW_BIT(14)
#define ADIE_TSSI_478                   0x478u
#define ADIE_TSSI_479                   0x479u
/* 0x479 [29]: the MT7976 TOP init clears it on k 0..4 (0x38000000 ->
 * 0x18000000, adie.h "TOP init"); 0x18000000 is also the MT7977 detector
 * path value above. [static][measured] */
#define ADIE_TSSI_479_B29               HW_BIT(29)

/* 0x121  TSSI detector path word. T2{4}: chain WF = 0x00080000, then the
 * partner = 0x4000e27f (WF 0, 2) / 0x2000de7f (WF 1, 3); every chain [9] = 1,
 * [13:10] = 8. MT7977 DPD: the WF [9] = 1, [13:10] = 1, then the feedback
 * partner's [13:10] = the AGC's gain step (above). [static] */
#define ADIE_TSSI_121                   0x121u
#define ADIE_TSSI_121_EN                HW_BIT(9)
#define ADIE_TSSI_121_GAIN              HW_GENMASK(13, 10)

/* TX power detector (T2{6}). 0x5c4 [15:0]: reading after a 0x477 [15]
 * strobe (set, 20 us, clear); 0x460 [15:0]: reading after a [14] + [15]
 * strobe; the MT7976 first copies k0 0x011 [30:0] into the chain's 0x011.
 * MT7976 detector path (V:e00589aa): 0xe90 [31:30] = 1 (mt7976.h
 * MT7976_DCOC_E90; 0 = off), k0 0xa0f [1:0] |= 3, 0x041 [27], 0x042 [24],
 * 0x000 [31:24] = 0x84 (ADIE_CHAIN_MODE_SEL_LOOP), 0x478 [31:29] = 0, [3] =
 * 0, 0x479 [31:29] = 0, 0x477 [26:16] = 0xff, [30:28] = 3, [3:0] = 7.
 * MT7977: 0x4a0 [31] latch, [27:24] detector attenuation step (5, then 4/3
 * when the reading <= 0x3ff) [inferred]. MT7976 chain 0x216 [7] = 1, [10:8]
 * = 7. 0x010 transient 0x01fa23c5 (TX | bit 17). All restored.
 * [static, measured]
 * DPD's TX power sweep (phy_cal.h) reads the MT7977 detector 0x5c4 [12:0]
 * and the MT7976 chain's 0x083 [31:20] (mt7976.h MT7976_TXDET_083), five
 * readings 5 us apart 100 us after the tone starts; MT7977 detector step
 * 0x4a0 [31] = 1, [27:24] = 0..7 (ADIE_PA_4A0). The TX chain's 0x200 /
 * 0x240 / 0x241 / 0x4a0 are saved and restored. [static] */
#define ADIE_TXDET_PWR                  0x5c4u
#define ADIE_TXDET_PWR2                 0x460u

/* 0x240 [30] (V:e00686ec clears only [30], mask 0xbfffffff), 0x241 [27:21]:
 * MT7977 TX gain mode and index (T2{4} index from 0xe0071a7c, T2{5} 1..0x39
 * from 0xe007ff88, DPD per step and per sweep point; same role as the
 * MT7976's 0x200 [16] / [6:0], V:e006492e). T2{8} also sets 0x240 [28]
 * with 0x241 [11:1] and [29] with 0x241 [20:12] (OTFIQ CFG2 A241_LO /
 * A241_HI, phy_cal.h). The TX LO / IQ calibration (T1{9,13,17}, below) sets
 * 0x240 [17:16] = 3 and [28] = 1 with 0x241 [11:1] from its per-step
 * tables. [static] */
#define ADIE_TX_GAIN_MODE               0x240u
#define ADIE_TX_GAIN_MODE_EN            HW_BIT(30)
#define ADIE_TX_GAIN_MODE_B28           HW_BIT(28)	/* with 0x241 [11:1] */
#define ADIE_TX_GAIN_MODE_1716          HW_GENMASK(17, 16)	/* 3 in the TX LO / IQ cal */
#define ADIE_TX_GAIN                    0x241u
#define ADIE_TX_GAIN_IDX                HW_GENMASK(27, 21)
#define ADIE_TX_GAIN_LO                 HW_GENMASK(11, 1)

/* T2{5} (mode 0x17) A-die words, MT7977: WF 0x010 = ADIE_CHAIN_EN_TX_TSSI;
 * 0x4a0 [31] = 1, [27:24] = PA step 0..7, [10:9] = 2 (every chain, 3 on WF;
 * whole 0x84480600 with P[6] = 1); 0x920 [28] = 1, [26:20] = 0x8ef / 0x8ee
 * [6:0]; 0x600 [16] = 1, [11:8] = LNA 1, [4:1] = LPF 3; 0x741 [4] = 1, [2:0]
 * = 4, [20] = 1, [19:16] = 3; 0x201 [9:0] = 0x100; 0x402 = 0x00300040; 0x745
 * = 0x03000000; 0x001 [9:8] = 3, [17:16] = 2 (every chain; the WF then
 * [17:16] = 3, whole 0x00030000 with P[6] = 1); the WF's TOP word
 * (mt7977.h MT7977_TOP_DCOC_WF) [31:28] = 0xb; TOP 0x4a8 (MT7977_TOP_EPA)
 * [4:0] = 1 << WF, [5] = 1. All restored. T2{8} writes 0x920 = 0x10014241 on
 * the WF. [static, measured]
 * 0x4a0 in the MT7977 TX loop DC (phy_cal.h mask bit 10): [31] = 1, [10:9] =
 * 3, [27:24] = PA step 0..7, one fine-engine run each; WF 1 also writes k0
 * 0x4a0 = 0x85080600. Rest 0x00080000 ([19] = 1); the TTG RX path writes
 * [10:9] = 2 on every bank (mt7977.h), as does the MT7977 TX LO / IQ
 * calibration (T1{9,13,17}, below). [static, measured] */
#define ADIE_PA_4A0                     0x4a0u
#define ADIE_PA_4A0_EN                  HW_BIT(31)
#define ADIE_PA_4A0_STEP                HW_GENMASK(27, 24)
#define ADIE_PA_4A0_LATCH               HW_BIT(23)	/* pulsed with [22] = 1 after a TX gain write (T1{8,12,16}) */
#define ADIE_PA_4A0_B22                 HW_BIT(22)
#define ADIE_PA_4A0_1009                HW_GENMASK(10, 9)
#define ADIE_TX_LVL_920                 0x920u
#define ADIE_TX_LVL_920_EN              HW_BIT(28)
#define ADIE_TX_LVL_920_VAL             HW_GENMASK(26, 20)

/* MT7976 TX test mode (T2{4}, V:e006366a mode 2): every chain 0x001 [9:8] =
 * 3, [17:16] = 2, 0x477 [31] = 0; TOP 0x070 (ADIE_TOP_CHAIN_IF) = 0x8888d005
 * with the transmitting WF's nibble [15 + 4 WF : 12 + 4 WF] = 0xd, the
 * others 8; chain 0x605 (mt7976.h MT7976_MON_BAND) [1] = 0; 0x003 [31], [29]
 * set on WF 0/1 when P[5] = 1; 0x600 [16] = 1, [11:8] = 0, [4:1] = 5 (mode
 * set), then [4:1] = table 0xe00812b8 byte (9), then [4:1] = 8 (mode 0x14
 * gain set); 0x605 [11:8] = table 0xe00812c0 byte, 0x004 [29] = 1, [18:16] =
 * 3; 0x200 [16] = 1, [6:0] = PA gain code; chain WF 0x010 =
 * ADIE_CHAIN_EN_TX_FB (0x09fa3bcf), the others standby, 0x011 manual on all.
 * All restored. [static, measured] */

/* TX gain-step calibration (T1{7,11,15}, phy_cal.h mask bit 11; A-die mode
 * 1, V:e005253a / V:e005236c), the WF's bank unless noted; all restored by
 * the bit-3 lists. [static][measured]
 * MT7976 (V:e006366a, V:e0062e84):
 *   every bank 0x001 [9:8] = 3, [17:16] = 2; every bank 0x605 [0] = 1;
 *   0x060 [6:3] = 2 (mt7976.h MT7976_CHAIN_060; only when g5 = 0 and the
 *     mode-1 a5 = 0);
 *   TOP 0x070 = 0x88888005 with the WF's nibble [15 + 4 WF : 12 + 4 WF] =
 *     0xd (as T2{4}'s TX test mode);
 *   g5 = 0: 0x600 (adie.h ADIE_DCOC_STEP) [16] = 1, [11:8] = 4, [4:1] = 0xc
 *     (RX gain held: LNA 4, LPF 0xc);
 *   P[5] = 1 and WF 0 / 1: 0x003 |= 0x80000000, then |= 0x20000000;
 *   0x010 = ADIE_CHAIN_EN_LOOP, the other banks 0x23c1, every bank 0x011 =
 *     0x80000000.
 * MT7977 (V:e0066aa4, V:e0064c04):
 *   every bank 0x001 [9:8] = 3, [17:16] = 2, 0x4a0 [10:9] = 2, 0x477 [31] =
 *     0;
 *   0x13b [31] = 1; 0x12b [0] = 1 (mt7977.h);
 *   TOP MT7977_TOP_DCOC_WF(WF) [31:28] = 0xb ([27:0] kept);
 *   0x600 = 0x00010110 (whole word: [16] = 1, LNA 1, LPF 8);
 *   0x010 = 0x40fbfbc5 (ADIE_CHAIN_EN_LOOP without RX bits 1, 3), the other
 *     banks 0x23c1, every bank 0x011 = 0x80000000, then 0x010 =
 *     ADIE_CHAIN_EN_LOOP.
 * TX gain per step: MT7976 0x200 [16] = 1, [6:0] = index (V:e006492e);
 *   MT7977 0x240 [30] = 1, 0x241 [27:21] = index (V:e00686ec).
 * ADIE_CHAIN_CTL [17:16] is 2 here (not 3, which T2{5} writes on its WF). */
#define ADIE_CHAIN_EN_LOOP_PRE_MT7977   0x40fbfbc5u
#define ADIE_LOOP_RXGAIN_MT7976         0x00010418u	/* 0x600 after the RMWs, from rest 0x806 */
#define ADIE_LOOP_RXGAIN_MT7977         0x00010110u

/* TX IQ calibration (T1{8,12,16}, phy_cal.h mask bit 12; HAL mode 0xc), on
 * the calibrated WF w. [static][measured]
 * MT7976 (V:e006366a / e0062e84 / e006492e / e0064a14): every bank 0x001
 *   [9:8] = 3, [17:16] = 2; TOP 0x070 (adie.h ADIE_TOP_CHAIN_IF) =
 *   0x88888005 with the WF's nibble [15 + 4 w : 12 + 4 w] = 0xd (0x8888d005
 *   for w 0; as T2{4}); w 0/1 with P[5] = 1 and 2.4 GHz: 0x003 [31], then
 *   [29] = 1; chain w 0x010 = ADIE_CHAIN_EN_TX_DETB, the others standby,
 *   0x011 manual on every bank; per step 0x200 [16] = 1, [6:0] = TX code
 *   (2.4 GHz only), 0x4a0 [23:22] = 1, 3, 1, 0x600 [16] = 1, [11:8] = 0,
 *   [4:1] = LPF index, 0x027 [27] = 1, [11:8] = LPF index.
 * MT7977 (V:e0066aa4 / e0064c04 / e00686ec / e0068982): every bank 0x001
 *   [9:8] = 3, [17:16] = 2, 0x4a0 [10:9] = 2, 0x477 [31] = 0; the WF's TOP
 *   word (mt7977.h MT7977_TOP_DCOC_WF) [31:28] = 0xb; chain w 0x741 [4:0] =
 *   0x14 (ADIE_LOOP_741), 0x742 [21:20] |= 3; chain w 0x010 = 0x107803c5,
 *   the others standby, 0x011 manual, then w 0x010 = ADIE_CHAIN_EN_TX_DETB;
 *   per step 0x240 [30] = 1, 0x241 [27:21] = TX code (5/6 GHz), 0x4a0
 *   [23:22] = 1, 3, 1, 0x600 and 0x027 as the MT7976.
 * Both: TOP 0x060 [26:24] = 0, BW code into 0x100 / 0x102 of every bank
 *   (MT7976 3, MT7977 the class), 0x101 (MT7976 0x101 = 0x103 = 0 at 2.4
 *   GHz, MT7977 0x101 = 1). Every register is in the bit-3 restore lists and
 *   is written back. The WF's TX output reaches the paired chain's ADC
 *   (phy_cal.h MT7996_PHY_TXIQ_RX_CHAIN) through TX detector B; detector B's
 *   analog role, and why the WF's LPF index is stepped while the paired
 *   chain digitises, are not established. */
#define ADIE_TXIQ_027                   0x027u
#define ADIE_TXIQ_027_EN                HW_BIT(27)
#define ADIE_TXIQ_027_LPF               HW_GENMASK(11, 8)	/* = 0x600 [4:1] */
#define ADIE_TXIQ_742                   0x742u	/* MT7977: [21:20] |= 3 */
#define ADIE_TXIQ_742_2120              HW_GENMASK(21, 20)
#define ADIE_CHAIN_EN_TX_DETB_77PRE     0x107803c5u	/* MT7977 intermediate (bit 1 clear) */

/* TX LO-leakage and TX IQ calibration (T1{9,13,17}, phy_cal.h mask bit 13;
 * HAL mode 0), on the calibrated WF w. [static][measured]
 * Mode set (V:e005253a): every bank 0x001 [9:8] = 3, [17:16] = 2; MT7976
 *   TOP 0x070 (adie.h ADIE_TOP_CHAIN_IF) = 0x88888005 with the WF's nibble
 *   [15 + 4 w : 12 + 4 w] = 0xd, w 0/1 with g5 = 0 and P[5] = 1: 0x003
 *   [31], then [29] = 1; MT7977 0x4a0 [10:9] = 2 and 0x477 [31] = 0 on
 *   every bank, the WF's TOP word (mt7977.h MT7977_TOP_DCOC_WF) [31:28] =
 *   0xb, chain w 0x146 [30:25] = 0x3a (mt7977.h MT7977_CHAIN_146).
 * Chain state (V:e005236c): MT7976 w 0x010 = ADIE_CHAIN_EN_TX_DETA, the
 *   others standby, 0x011 manual on every bank; later in the setup, after
 *   the RX DC engine run on w in this state (0x010 [31] = 1, phy_cal.h
 *   HAZARD (RX)), w 0x010 = ADIE_CHAIN_EN_TX_DETA_76 with 0x041 = 0x08000000
 *   and 0x042 = 0x01000000 (ADIE_TX_042_B24); MT7977 w 0x010 =
 *   ADIE_CHAIN_EN_TX_DETA_77PRE, the others standby, 0x011 manual, then w
 *   0x010 = ADIE_CHAIN_EN_TX_DETA.
 * TX gain per step (V:e0052612): MT7976 0x200 [16] = 1, [6:0] = index;
 *   MT7977 0x240 [30] = 1, 0x241 [27:21] = index, 0x240 [17:16] = 3 (OR),
 *   0x244 (mt7977.h MT7977_TX_244) [9] = t bit 3, [21:19] = t [2:0], [18:16]
 *   = u (u << 16 not masked), 0x240 [30] and 0x241 [27:21] again, 0x240
 *   [28] = 1, 0x241 [11:1] = v (t, u, v: the HAL's stack arguments from the
 *   per-step tables, phy_cal.h MT7996_PHY_TXLOIQ_T77_*).
 * RX gain (V:e005265c): MT7976 0x600 [16] = 1, [11:8] = 0, [4:1] = LPF;
 *   MT7977 (mode 0 is not in the HAL's mode set 0x911036) no 0x600 write:
 *   0x741 [4] = 1, [2:0] = 0, [20] = 1, [19:16] = LPF.
 * TX mode (V:e0052582, MT7976 only, g5 = 0): 0x202 [22:20] = x, [18:16] = x
 *   (x: a WM RAM byte, 0; mt7976.h MT7976_TXMODE_202).
 * Every register above is in the bit-3 restore lists and is written back
 * (not the MT7976 RX DC results). TX detector A as the calibration's
 * receiver is [inferred] from the chain state's [29]
 * (ADIE_CHAIN_EN_TXDET_A); its analog role is not established. */
#define ADIE_CHAIN_EN_TX_DETA_76        0x21f803c7u	/* MT7976: _DETA | [24], the remaining TX bit */
#define ADIE_CHAIN_EN_TX_DETA_77PRE     0x20f803c5u	/* MT7977 intermediate (bit 1 clear) */

/* ------------------------------------------------------------------------
 * Bits with a visible effect on a receiving chain (single-bit flips against
 * an in-band tone; MT7977 chain 0 unless noted). [measured]
 * ------------------------------------------------------------------------
 * Kill reception: 0x101 b0, 0x110 b1/b2/b7, 0x113 b12/b27, 0x139 b23..b25,
 *   0x13b b27/b29/b30, 0x149 b31, 0x171 b0/b8.
 * 0x113 b13: DC/IQ change. 0x113 b14: oscillation-like lines at +-104 MHz.
 * MT7976 bank 0: 0x12f b27 mirrors the spectrum about an LO 0.523 MHz higher
 *   (full strength); 0x021 b30 clear adds 16 dB of noise that follows the LPF
 *   gain, not the LNA gain (generated at the mixer/LO buffer). Other kill
 *   states attenuate about 30 dB or mute.
 */
#define ADIE_RX_110                     0x110u
#define ADIE_RX_113                     0x113u
#define ADIE_RX_113_DCIQ                HW_BIT(13)
#define ADIE_RX_113_OSC                 HW_BIT(14)
#define ADIE_RX_139                     0x139u
#define ADIE_RX_13B                     0x13bu
#define ADIE_RX_12F                     0x12fu
#define ADIE_RX_12F_MIRROR              HW_BIT(27)	/* MT7976 */

/* ------------------------------------------------------------------------
 * Calibration catalogue (the firmware's per-die bodies). [static]
 * ------------------------------------------------------------------------
 *  R-cal apply     0xe40 code from the EEPROM image (adie.h)
 *  RC cal          0xe50/0xe51/0xe59/0xece, TOP 0x3a4 (adie.h)
 *  RX DC (DCOC)    fine: CHAIN_EN_RX_DCOC, 0x923 then 0x922 engines,
 *                  results 0x8e0..; coarse per step: CHAIN_EN_RX, CHAIN_MODE
 *                  0x88, step index 0x600, results 0x8f0.. (adie.h)
 *  TX loop DC      T1{6,10,14}, MT7977: chain 0x010 = TX | bit 17 (P[6] =
 *                  1), pair bank 0x000 = 0x82, CALPATH 3, 0x744..0x747, the
 *                  0x922 engine in loop mode x 8 PA steps (0x4a0), results
 *                  0x8ef / 0x8ee (adie.h); P[6] = 0: CHAIN_EN_TX_DC; MT7976
 *                  with EPA: setup and save only
 *  TX gain steps   T1{7,11,15}: CHAIN_EN_LOOP on the WF (MT7977: first
 *                  0x40fbfbc5, then 0x40fbfbcf), standby elsewhere; TX gain
 *                  index stepped, D-die step codes (phy.h TXSTEP_CODES)
 *  TX IQ           T1{8,12,16}: CHAIN_EN_TX_DETB on the WF (TX detector B
 *                  as the loopback; MT7977 first 0x107803c5), standby
 *                  elsewhere, TOP 0x060 [26:24] = 0; the paired chain's ADC
 *                  and the D-die IQ engine measure the image (phy_cal.h)
 *  TX LO / IQ      T1{9,13,17}: CHAIN_EN_TX_DETA on the WF (MT7977 first
 *                  0x20f803c5; MT7976 then 0x21f803c7, 0x041 = 0x08000000,
 *                  0x042 = 0x01000000), standby elsewhere, 0x011 manual on
 *                  every bank; MT7976 first the A-die RX DC engine on the
 *                  WF's chain in this state (0x010 [31]); TX gain index
 *                  stepped; the D-die TX LO / IQ engine (phy_cal.h
 *                  MT7996_PHY_TXCAL_*) measures through detector A
 *                  [inferred]; results phy.h MT7996_PHY_TXLO_*,
 *                  MT7996_PHY_TXLOIQ_A / _B
 *  TTG-tone RX     T1{18..23}: every chain CHAIN_EN_RX_AUX, the gain forced
 *                  per step (0x600), the TTG at LO + 5 MHz (DC phase), then
 *                  LO - SUB (IQ phase) routed into every chain (sx.h;
 *                  MT7976 path mt7976.h, MT7977 path mt7977.h), TOP 0x060
 *                  = 3 then 1; D-die DC and IQ engines (phy_cal.h, phy.h)
 *  DPD             T2{0..3} (P[6] = 1, the stock case): CHAIN_EN_TX on the
 *                  WF; MT7976 the next chain in CHAIN_EN_RX, coupling
 *                  0x631/0x633, TOP 0x744, its DC cal; MT7977 partner
 *                  CHAIN_EN_TSSI_FB + loop network; TX power sweep
 *  TSSI            CHAIN_MODE 0x84, TSSI path above
 *  TSSI DNL        T2{4}: MT7976 CHAIN_EN_TX_FB + TX test mode; MT7977
 *                  CHAIN_EN_TX, partner CHAIN_EN_TSSI_FB + loop network
 *  TSSI PA cells   T2{5}: CHAIN_EN_TX_TSSI, 0x4a0 / 0x920 steps (MT7977)
 *  TX flatness     T2{6}: TX + band tone generator, TX detector 0x5c4
 *  OTFIQ           T2{8}: other chains standby, 0x920, 0x14b, 0x03a; the
 *                  band DSP computes (phy_cal.h)
 * Host RF-test commands for these states (SET_ADC, SET_RX_GAIN, SET_TTG,
 * TTG_ON_OFF) are stubs in this firmware.
 */

#endif /* HW_ADIE_RFMODES_H */
