/* SPDX-License-Identifier: MIT */
/*
 * MT7977 A-die: die 1 (band 1, 5 GHz) and die 2 (band 2, 6 GHz).
 * Chip ID 0x79778a10 at TOP 0x02c on both.
 *
 * ADDRESS SPACE: 12-bit A-die register indices on SPI master 1 or 2
 * (adie.h); banks k 0..4 = chains WF0..WF4, k 4 also the synthesizer (SX),
 * k 15 = TOP. Band 1 receives on k 0..3, band 2 on k 0..4.
 * Banks k 2 and k 3 (CS 6, CS 7) answer through our WM's A-die path (the RC
 * calibration reads bank 2); an earlier census through the stream
 * producer's raw chip-select reads saw only the garbage pattern there
 * (adie.h HAZARDS). [measured]
 *
 * SYNTHESIZER (register family in sx.h): one SX in k 4 drives every chain
 * of the die. f_PFD = 2 x 80 MHz = 160 MHz (hard-coded in the firmware).
 * Two LO ratios, selected by 0xa40[0] with the matching preset column:
 *     5/8 (0xa40[0] = 0): LO 4.65..6.18 GHz
 *     3/4 (0xa40[0] = 1): LO 5.58..7.41 GHz
 * over a VCO of 7.44..9.88 GHz (20..25 MHz steps against the lock flag).
 * [measured] Nothing else: no VCO/8, /2, /4 or 3/8-image response (> 79 dB
 * down), and the dead preset's other mode values do not lock. [measured]
 * Low edge: the software band search reaches its last code (0xbff) at LO
 * 4665 MHz (5/8); the flag gives up below 4650 MHz but the loop holds LO
 * exactly to 4643..4644 MHz (VCO 7429 MHz), where it parks. [measured]
 * The VCO band (capacitor) search is done in software (SAR below).
 *
 * Reception follows each die's front end (5 GHz diplexer/FEM filtering):
 * band 1 strong 4.8..5.8 GHz, gone by 6.0 GHz; band 2 strong from 5.9 GHz,
 * 4.8..5.9 GHz still 25..45 dB down. Above 6.0 GHz not verified with a
 * reference. [measured]
 *
 * HAZARD: a radio reload or a firmware channel switch transmits a full-scale
 * burst at the antennas; register programs written at run time never do.
 * [measured]
 *
 * Confidence tags: [measured], [static], [inferred].
 */
#ifndef HW_MT7977_ADIE_H
#define HW_MT7977_ADIE_H

#include "../hw_bits.h"
#include "adie.h"
#include "sx.h"

#define MT7977_DIE_BAND1                1u
#define MT7977_DIE_BAND2                2u
#define MT7977_K_SX                     4u
#define MT7977_NUM_CHAINS_BAND1         4u
#define MT7977_NUM_CHAINS_BAND2         5u

/* ------------------------------------------------------------------------
 * Frequency plan
 * ------------------------------------------------------------------------ */
#define MT7977_PFD_KHZ                  160000u
/* f_LO = f_VCO * NUM / DEN */
#define MT7977_RATIO_5_8_NUM            5u
#define MT7977_RATIO_5_8_DEN            8u
#define MT7977_RATIO_3_4_NUM            3u
#define MT7977_RATIO_3_4_DEN            4u

#define MT7977_VCO_MIN_MHZ              7440u	/* [measured] */
#define MT7977_VCO_MAX_MHZ              9880u
#define MT7977_VCO_FLOOR_MHZ            7429u	/* closed-loop floor, band 0xbff */
#define MT7977_LO_5_8_MIN_MHZ           4650u
#define MT7977_LO_5_8_MAX_MHZ           6175u
#define MT7977_LO_3_4_MIN_MHZ           5580u
#define MT7977_LO_3_4_MAX_MHZ           7410u
/* TUNE's reach (firmware/radio/rfctl.c): beyond each die's lock edges, which the host finds by
 * the lock flag at load (host/w1700k/controller.py Controller.lo_lock). Unit A, 2026-10-11, by
 * TUNE's program and the flag, to 0.1 MHz: die 1 locks 4646.56..7509.05 MHz, die 2
 * 4707.15..7576.22 MHz (VCO 7.43..10.01 / 7.53..10.10 GHz); the edges wander by about 0.1 MHz
 * between probes. A B205 tone confirmed the LO at the lower edges (line at +20 MHz, moving with the
 * B205); above 6 GHz only its harmonics reach, too weak over the air to see, so the LO above the
 * earlier measured 7.41 GHz rests on the lock flag alone. [measured] */
#define MT7977_LO_TUNE_MIN_MHZ          4640u
#define MT7977_LO_TUNE_MAX_MHZ          8000u

/* Firmware: tunes 4850..7125 MHz only (it refuses <= 4849.999 MHz); 5/8
 * below 5926 MHz, 3/4 from 5926 MHz; input quantised to 1 MHz (500 kHz when
 * the remainder is >= 500 kHz). All 24 FRAC bits work (about 6 Hz at
 * the LO), at any frequency the VCO reaches. The DPD calibration (phy_cal.h
 * T2{0..3}) retunes to F - 10 MHz for its call and back to F at its exit.
 * [static][measured] */
#define MT7977_FW_LO_MIN_MHZ            4850u
#define MT7977_FW_LO_MAX_MHZ            7125u
#define MT7977_FW_SPLIT_MHZ             5926u

/* Worked words (firmware arithmetic; the 5250 MHz pair read back live). */
#define MT7977_SX_INT_5250MHZ           0x03406271u	/* 5/8, VCO 8400 */
#define MT7977_SX_FRAC_5250MHZ          0x80800000u
#define MT7977_SX_INT_6105MHZ           0x03205f65u	/* 3/4, VCO 8140 */
#define MT7977_SX_FRAC_6105MHZ          0x80e00000u
#define MT7977_SX_INT_4850MHZ           0x03005af1u	/* 5/8, VCO 7760 */
#define MT7977_SX_FRAC_4850MHZ          0x80800000u
#define MT7977_SX_INT_7125MHZ           0x03b06f55u	/* 3/4, VCO 9500 */
#define MT7977_SX_FRAC_7125MHZ          0x80600000u

/* ------------------------------------------------------------------------
 * SX mode registers (bank k 4)
 * ------------------------------------------------------------------------ */

/* 0xa40  Ratio select. RW. Preset 0x00100014 (5/8) / 0x00100015 (3/4).
 * [0] RATIO: 0 = 5/8, 1 = 3/4 (0xa00[1] has the same effect).
 * [1], [3] not implemented (read back 0). [2], [5]: no effect.
 * [4]: keep 1. Cleared before a relock the 5/8 LO was normal; flipped under
 *      a running lock it removed the LO from 70 MHz..6 GHz while 0xca4 still
 *      read locked (above 6 GHz not checked).
 * [20]: keep 1; clear, the flag reads "too high" everywhere.
 * [measured][static] */
#define MT7977_SX_CFG                   ADIE_SX_CFG
#define MT7977_SX_CFG_RATIO             HW_BIT(0)
#define MT7977_SX_CFG_RATIO_5_8         0u
#define MT7977_SX_CFG_RATIO_3_4         1u
#define MT7977_SX_CFG_B4                HW_BIT(4)
#define MT7977_SX_CFG_B20               HW_BIT(20)
#define MT7977_SX_CFG_5_8               0x00100014u
#define MT7977_SX_CFG_3_4               0x00100015u

/* 0xa55  Observed 0x4000. [9:8] is the SDM split d of the firmware's word
 * arithmetic: keep 0; values 1..3 make the flag read "too low". [14] always
 * 1. [static][measured] */
#define MT7977_SX_SPLIT                 ADIE_SX_A55
#define MT7977_SX_SPLIT_D               HW_GENMASK(9, 8)

/* 0xa63  [31:28] one-hot mode field, written only by an unused firmware
 * preset (4 for its 5/8 column, 1 for 3/4). Observed 0x44000000 in both
 * ratios. Values 8, 2, 1, 0 do not lock: flipping [30] reads "too low",
 * flipping [28] "too high". [static][measured] */
#define MT7977_SX_A63_MODE              HW_GENMASK(31, 28)
#define MT7977_SX_A63_MODE_LIVE         4u

/* 0x138  [28] differs between the unused preset's columns; observed 0;
 * changing it does nothing. [static][measured] */
#define MT7977_SX_138_B28               HW_BIT(28)

/* Single bits of the SX bank that force a flag at every target (band 0xbff
 * forced; none lowers the closed-loop floor) [measured]:
 *   "too high": 0xa00 b21/b28/b31, 0xa0a b30, 0xa40 b20, 0xa63 b28,
 *               0xb92 b27/b30
 *   "too low":  0xa23 b31, 0xa55 b8/b9, 0xa63 b30
 *   "locked" (detector fault, LO parked 4652..4657 MHz): 0xa05 b6,
 *               0xbcb b5, 0xb86 b28 (alternating)
 *   all bits of 0xb80, 0xb82..0xb8a, 0xb9c..0xb9f change the flags alike;
 *   all bits of 0xbb1..0xbbf stop the lock; 0xbb0 b31 opens the loop (sx.h).
 */

/* ------------------------------------------------------------------------
 * Retune sequence (SX bank k 4 unless noted). [static][measured]
 * ------------------------------------------------------------------------
 *  0. If the target leaves the current preset Group or RCD range, or the
 *     ratio changes: write MT7977_SX_E3_ROWS for the Group and the RCD rows
 *     (below). Crossing a Group without them measured the same line level
 *     and floor as the firmware's own channel switch.
 *  1. 0xb81 = 0x100; 0x171[9] = 0 then 1;
 *  2. 0xa04 = MT7977_SX_A04_PRE;
 *  3. 0xa4b = INT word, 0xa4a = FRAC word (N = f_VCO / 160 MHz,
 *     f_VCO = f_LO * DEN / NUM);
 *  4. 0xa04 = _A04_SAR, 0xb8a = _B8A_SAR, 0xb83 = _B83_SAR,
 *     0xa03 = _A03_SAR, 0xa05 = _A05_SAR;
 *  5. low = 0xa01[19:0]; for c in MT7977_SAR_RAMP0..3:
 *     0xa01 = c << 20 | low, 0xa02 = c << 20;
 *  6. SAR, i = 0..9: 0xb81 = 0x101; wait 10 us (poll 0xca7[16]);
 *     0xb81 = 0x100; acc += 0xca5[7] << (9 - i);
 *     code = MT7977_SAR_CODE(acc, i); 0xa01 = code << 20 | low;
 *     0xa02 = code << 20. The last code is the band (0x800..0xbff).
 *  7. 0xa05 = _A05_LOCK, 0xa03 = _A03_LOCK, 0xb83 = _B83_LOCK,
 *     0xb8a = _B8A_LOCK, 0xb8b = _B8B_LOCK;
 *  8. 0xb81 = 0x101; wait 10 us; poll 0xca7[16] <= 1 ms; 0xb81 = 0x10;
 *     0xa04 = _A04_RUN;
 *  9. for k = 3, 2, 1, 0: 0xb81 = 0x100, 0xb81 = 0x2101, wait 10 us,
 *     poll 0xca7[16] in k <= 1 ms, 0xb81 = 0x10;
 * 10. 0xca4[1:0] == 0. (Firmware: 0xbc8 on k 0 = LO in kHz, on k 1 = P[1],
 *     written after the presets and RCD rows and before step 1; adie.h
 *     ADIE_FREQ_NOTE_MT7977.)
 * Cost: 0.8 ms of WM time, 1.5 ms with presets. The firmware skips step 3
 * when the words are unchanged, 0xca4[1:0] = 0 and 0xcd1[16:12] is 3..28.
 * Words-only (step 3 alone) moves a held lock within its band code.
 */
#define MT7977_SX_A04_PRE               0xc0000000u
#define MT7977_SX_A04_SAR               0xc0040000u
#define MT7977_SX_A04_RUN               0x00040000u
#define MT7977_SX_B8A_SAR               0x00000005u
#define MT7977_SX_B8A_LOCK              0x5429c673u
#define MT7977_SX_B83_SAR               0x50001e10u
#define MT7977_SX_B83_LOCK              0x50001e50u
#define MT7977_SX_A03_SAR               0x8e040000u
#define MT7977_SX_A03_LOCK              0x08040000u
#define MT7977_SX_A05_SAR               0x005250d5u
#define MT7977_SX_A05_LOCK              0x005150d5u
#define MT7977_SX_B8B_LOCK              0x0000f876u

#define MT7977_SAR_RAMP0                0x800u
#define MT7977_SAR_RAMP1                0x880u
#define MT7977_SAR_RAMP2                0x900u
#define MT7977_SAR_RAMP3                0xa00u
#define MT7977_SAR_STEPS                10u
#define MT7977_SAR_CODE(acc, i)         (0x800u + (acc) + (0x100u >> (i)))

/* ------------------------------------------------------------------------
 * SX presets, "SX80_E3" variant (the firmware picks it when die 2's TOP
 * 0x02c[7:0] > 9; both dies here read 0x10). That byte (the ECO byte, read
 * at calibration init, phy_cal.h) also selects item 0's RF_INIT variant and
 * +0xd414 value (phy.h MT7996_PHY_RFIF_D414_MT7977) for both MT7977 dies,
 * and item 1's RF COMMON column (phy.h MT7996_PHY_RF_TBL_KEY; adie.h
 * "RF_INIT"). Firmware row order: 0xa01
 * (only [19:0] matters; the SAR owns [31:20]), 0xa51, 0xa5b, 0xa64, 0xa48,
 * 0xa52, 0xa5a, 0xa40, 0xa44, 0xa57. Groups 1..8 are the 5/8 columns, 9..23
 * the 3/4 columns; never pair a column with the other ratio. Outside the
 * firmware's range keep the nearest column of the chosen ratio (the
 * firmware writes nothing for Group 0). Live die-1 Group-1 values match.
 * [static][measured]
 * ------------------------------------------------------------------------ */
#define MT7977_SX_E3_A51                0x04060303u
#define MT7977_SX_E3_A5B                0x0a00020au
#define MT7977_SX_E3_A64                0x00448424u	/* reads 0 */
#define MT7977_SX_E3_A48                0x06070800u
/* X(group, lo_mhz, hi_mhz, a01_19_0, a52, a5a, a40, a44, a57)
 * (Groups 9 and 10 have identical columns; the running firmware moves their
 * boundary by 1 MHz.) */
#define MT7977_SX_E3_ROWS(X)                                                              \
	X( 1, 4850u, 5250u, 0x10cffu, 0x01070403u, 0x401a0404u, 0x00100014u, 0x8000u, 0x04000000u) \
	X( 2, 5251u, 5330u, 0x10ceeu, 0x01060403u, 0x401a0404u, 0x00100014u, 0x8000u, 0x04000000u) \
	X( 3, 5331u, 5490u, 0x10cccu, 0x01060403u, 0x401a0404u, 0x00100014u, 0x9000u, 0x04000000u) \
	X( 4, 5491u, 5570u, 0x10caau, 0x01060403u, 0x401a0404u, 0x00100014u, 0xa000u, 0x04000000u) \
	X( 5, 5571u, 5650u, 0x10caau, 0x01060403u, 0x401a0404u, 0x00100014u, 0xa000u, 0x04000000u) \
	X( 6, 5651u, 5734u, 0x10c99u, 0x01050403u, 0x401a0404u, 0x00100014u, 0xb000u, 0x04000000u) \
	X( 7, 5735u, 5815u, 0x10c88u, 0x01050403u, 0x401a0404u, 0x00100014u, 0xb000u, 0x04000000u) \
	X( 8, 5816u, 5925u, 0x10c66u, 0x01050403u, 0x401a0404u, 0x00100014u, 0xc000u, 0x10000000u) \
	X( 9, 5926u, 6024u, 0x10cffu, 0x01060403u, 0x40210505u, 0x00100015u, 0x8000u, 0x00000000u) \
	X(10, 6025u, 6108u, 0x10cffu, 0x01060403u, 0x40210505u, 0x00100015u, 0x8000u, 0x00000000u) \
	X(11, 6109u, 6185u, 0x10cffu, 0x01060403u, 0x40210505u, 0x00100015u, 0x8000u, 0x00000000u) \
	X(12, 6186u, 6265u, 0x10cffu, 0x01060403u, 0x40200505u, 0x00100015u, 0x8000u, 0x00000000u) \
	X(13, 6266u, 6345u, 0x10ceeu, 0x01060403u, 0x40200505u, 0x00100015u, 0x8000u, 0x00000000u) \
	X(14, 6346u, 6425u, 0x10ceeu, 0x01050403u, 0x40200505u, 0x00100015u, 0x9000u, 0x00000000u) \
	X(15, 6426u, 6505u, 0x10cddu, 0x01050403u, 0x40200505u, 0x00100015u, 0x8000u, 0x00000000u) \
	X(16, 6506u, 6585u, 0x10cccu, 0x01050403u, 0x40200505u, 0x00100015u, 0x9000u, 0x00000000u) \
	X(17, 6586u, 6665u, 0x10cbbu, 0x01040403u, 0x40200505u, 0x00100015u, 0xa000u, 0x00000000u) \
	X(18, 6666u, 6745u, 0x10caau, 0x01040403u, 0x40210505u, 0x00100015u, 0xa000u, 0x00000000u) \
	X(19, 6746u, 6825u, 0x10c99u, 0x01040403u, 0x40200505u, 0x00100015u, 0xc000u, 0x00000000u) \
	X(20, 6826u, 6905u, 0x10c88u, 0x01040403u, 0x401e0505u, 0x00100015u, 0xc000u, 0x00000000u) \
	X(21, 6906u, 6985u, 0x10c77u, 0x01040403u, 0x401e0505u, 0x00100015u, 0xc000u, 0x00000000u) \
	X(22, 6986u, 7065u, 0x10c66u, 0x01040403u, 0x401c0505u, 0x00100015u, 0xd000u, 0x10000000u) \
	X(23, 7066u, 7125u, 0x10c55u, 0x01040403u, 0x401c0505u, 0x00100015u, 0xd000u, 0x10000000u)
#define MT7977_SX_E3_GROUPS             23u
#define MT7977_SX_E3_LAST_5_8           8u

/* ------------------------------------------------------------------------
 * RCD rows, by range of integer LO MHz (anything outside is range 0).
 * Written to k 4, 3, 2, 1, 0 in that order: first 0x403 = 0x15, then 0x768,
 * 0x766, 0x765, 0x767, 0x744, 0x746 (chain-bank 0x744/0x746; calibrations
 * later rewrite them, observed 0x00200000 / 0x1414). [static][measured]
 * ------------------------------------------------------------------------ */
#define MT7977_RCD_PRE                  0x403u
#define MT7977_RCD_PRE_VAL              0x00000015u
#define MT7977_RCD_768                  0x768u
#define MT7977_RCD_766                  0x766u
#define MT7977_RCD_765                  0x765u
#define MT7977_RCD_767                  0x767u
#define MT7977_RCD_744                  0x744u
#define MT7977_RCD_746                  0x746u
/* X(range, lo_mhz, hi_mhz, r768, r766, r765, r767, r744, r746) */
#define MT7977_RCD_ROWS(X)                                                                      \
	X(0, 4915u, 5535u, 0x18183131u, 0x18181818u, 0x18u, 0x0cu, 0x00200004u, 0x1818u) \
	X(1, 5536u, 5950u, 0x1b1b3737u, 0x1b1b1b1bu, 0x1bu, 0x0du, 0x00200004u, 0x1818u) \
	X(2, 5951u, 6370u, 0x18183131u, 0x18181818u, 0x18u, 0x0cu, 0x00200004u, 0x0c0cu) \
	X(3, 6371u, 6715u, 0x1a1a3535u, 0x1a1a1a1au, 0x1au, 0x0du, 0x00200061u, 0x0d0du) \
	X(4, 6716u, 7125u, 0x1c1c3838u, 0x1c1c1c1cu, 0x1cu, 0x0eu, 0x00200061u, 0x0e0eu)
#define MT7977_RCD_RANGES               5u

/* SX init values the firmware loads at power-on (not part of a retune):
 * 0xb94 0xf, 0xb95 0x0f8629c6, 0xb84 0x7831a084, 0x132 0xd5110100, 0xa08
 * 0x02031100, 0x134 0x00fd2bf0, 0x133 0x8ca320a0, 0xb8a 0x54296473, 0xb8b
 * 0xf876, 0xb83 0x50001c50, 0xb8d 0x000a0000, 0xa05 0x005140d5; chain banks
 * 0xb8a 0xfe2. Live die 2 matches for 0x132/0x133/0x134/0xb84/0xa08.
 * [static][measured] */
#define MT7977_SX_INIT_133              0x8ca320a0u

/* ------------------------------------------------------------------------
 * TTG PLL on the MT7977 (sx.h registers in the SX bank). TTG RX path of the
 * calibrations (phy_cal.h T1{18..23}), firmware order (V:e00680de), every
 * value regenerated against MediaTek's cold stock load [static][measured]:
 *   TOP 0x600 |= 0x3000; TOP 0x664 |= 0x80000000, |= 2, |= 1; TOP 0x668 |=
 *     0x80000000, |= 0x40000000;
 *   SX bank (k 4): 0x944 [31] = 0 (TTG mode 4: the DC phase, both phases
 *     of T1[23]) / 1 (mode 5: the IQ phase of T1{18..22}); 0x943 |= 1;
 *     0x13d |= 0x80000000; 0x942 [27:24] = 6; 0x941 = INT << 23 | FRAC
 *     (N = F / 80 MHz, MT7977_TTG_REF_KHZ); mode 4 only: 0x140 [18] = 0;
 *     0x943 &= ~1;
 *   chains in the order k3, k4, k2, k1, k0: 0x141 = 0x00007f85 (whole
 *     word, not only [7]); then in the same order 0xa0d = 0x0bf00000
 *     (whole word);
 *   10 us; mode 5: k 4 0x010 [30] = 0, 0x944 |= 0x80000000, 0x944 [22:0] =
 *     offset, 0x010 [30] = 1; mode 4: TOP 0x000 [4] cleared, then set
 *     again (not left cleared).
 * Chain state before it (V:e0066aa4 modes 4 and 5, V:e0064c04): every
 *   chain 0x001 [9:8] = 3, [17:16] = 2, 0x4a0 [10:9] = 2, 0x477 [31] = 0,
 *   0x12c = 0x0e00c110; TOP 0x043 (MT7977_TOP_DCOC_WF(0)) [31:28] = 0xc;
 *   0x003 [31] then [30] = 1 (RX_INPUT 6 on every chain, k 4 included),
 *   0x101 = 1, 0x100 = 0x102 = bw code, 0x010 = ADIE_CHAIN_EN_RX_AUX
 *   (rfmodes.h), 0x011 = 0x80000000; per chain and gain step (route helper V:e0067a8e flag 1):
 *   0x940 [8] = 0 and 0x636 = 0x00642053 (whole word). k0 0x633 is read
 *   once per step (nibble [LNA step] > 7 -> 3) but the route helper ignores
 *   the result.
 * Observed 0x141 0x7f00, 0x13d 0x09205d40, 0x140 0xfe84ac02, TOP 0x664
 * 0xffa428af, 0x668 0xffd42d00. [measured] A bench attempt with the
 * routing as this header gave it before the decode (0x141 [7] only, TOP
 * 0x000 [4] left cleared, no 0xa0d or 0x636 write) produced no tone in the
 * capture (PLL activity not confirmed); the stock sequence above has not
 * been tried on the bench. [measured]
 * HAZARD: TOP 0x664 = 0 is one of the two candidate write-lock triggers
 * (adie.h HAZARDS); the routing above only ORs bits in.
 * ------------------------------------------------------------------------ */
#define MT7977_TTG_EN                   0x13du
#define MT7977_TTG_EN_ON                HW_BIT(31)
#define MT7977_TTG_ROUTE                0x141u
#define MT7977_TTG_ROUTE_TO_CHAIN       HW_BIT(7)
#define MT7977_TTG_ROUTE_RX             0x00007f85u	/* whole word, TTG RX */
#define MT7977_TTG_140                  0x140u
#define MT7977_TTG_140_B18              HW_BIT(18)	/* cleared in TTG mode 4 */
#define MT7977_TTG_A0D                  0xa0du
#define MT7977_TTG_A0D_RX               0x0bf00000u
#define MT7977_TTG_636                  0x636u
#define MT7977_TTG_636_RX               0x00642053u
#define MT7977_TTG_REF_KHZ              80000u	/* WM: P[0x10] = 80 MHz, mult 1 */
#define MT7977_RX_12C                   0x12cu	/* MT7976 RF_INIT walker writes it on die 0 too, role there not decoded */
#define MT7977_RX_12C_TTG_RX            0x0e00c110u
#define MT7977_TOP_TTG_664              0x664u
#define MT7977_TOP_TTG_668              0x668u
/* TOP 0x668 [23] = !(EEPROM 0x199 + die [2]), by the PHY-init routine's
 * per-band FEM call (V:e00cd44e -> V:e00cd3fa, dies 1 and 2, every call;
 * 0xffd42d00 unchanged). [static][measured] */
#define MT7977_TOP_TTG_668_FEM          HW_BIT(23)

/* Chain-bank synthesizer copy (no PLL, sx.h): idle INT word observed in
 * k 0/1. [measured] */
#define MT7977_CHAIN_SX_COPY_INT        0x06f06811u

/* ------------------------------------------------------------------------
 * RX DC-offset calibration (adie.h DCOC) on the SX bank (k 4) and TOP
 * (dies 1, 2). Rest values restored. [static][measured]
 * ------------------------------------------------------------------------
 * SX bank 0x000 = 0x82000000 for the fine cal (every WF) and for the coarse
 *   per step on WF 0..3 (on die 2 WF4 the bank is the chain itself and gets
 *   chain mode 0x88000000 instead). Rest 0.
 * SX bank 0x745: fine 0x3300 then 0x3000; coarse per step [25:24] = 3 and
 *   [9:8] = 3, cleared after the steps. Rest 0x00003000.
 * SX bank 0x746 (MT7977_RCD_746) [16] and 0x747 [29:28] = 2 for the fine
 *   step. Rest 0x1414 (after the SX retune 0x1818) and 0.
 * SX bank 0x14a = 0x800aaaaa, 0x149 (ADIE_FB_SEL) = 0x3ff (fine; equal to
 *   rest).
 * TOP 0x043 / 0x044 / 0x048 / 0x049 / 0x04a: per-WF word (WF 0..4) = 0xc0000000
 *   for the calibrated chain during the fine cal; all five zeroed after. Rest 0.
 * TOP 0x7e9 = 0xea000000 (fine; equal to rest).
 * TOP 0x4a8 [4:0] = 0, then [5] = 1 (EPA boards, V:e00526c2 mode 0); rest 0.
 */
#define MT7977_SX_DCOC_MODE             0x82000000u	/* SX bank 0x000 */
#define MT7977_SX_DCOC_745              0x745u
#define MT7977_SX_DCOC_745_FINE         0x00003300u
#define MT7977_SX_DCOC_745_REST         0x00003000u
#define MT7977_SX_DCOC_745_STEP         0x03000300u	/* [25:24] [9:8] */
#define MT7977_SX_DCOC_746              MT7977_RCD_746
#define MT7977_SX_DCOC_746_FINE         HW_BIT(16)
#define MT7977_SX_DCOC_747              0x747u
#define MT7977_SX_DCOC_747_FINE         HW_GENMASK(29, 28)	/* 2 */
#define MT7977_SX_DCOC_14A              0x14au
#define MT7977_SX_DCOC_14A_VAL          0x800aaaaau
/* The coarse body sets SX bank 0x745 [25:24] and then [9:8] by two read-modify-writes
 * (MT7977_SX_DCOC_745_STEP is both) and clears them after the steps in the same order;
 * the fine step writes 0x149 (rfmodes.h ADIE_FB_SEL) = 0x3ff and 0x747 [29:28] = 2.
 * [static][measured] */
#define MT7977_SX_DCOC_745_STEP_HI      HW_GENMASK(25, 24)
#define MT7977_SX_DCOC_745_STEP_LO      HW_GENMASK(9, 8)
#define MT7977_SX_DCOC_149_VAL          0x3ffu
#define MT7977_SX_DCOC_747_FINE_VAL     2u
#define MT7977_TOP_DCOC_WF(wf)          ((wf) == 0 ? 0x043u : (wf) == 1 ? 0x044u : 0x048u + (wf) - 2u)
#define MT7977_TOP_DCOC_WF_ON           0xc0000000u
#define MT7977_TOP_7E9                  0x7e9u
#define MT7977_TOP_7E9_VAL              0xea000000u
#define MT7977_TOP_EPA                  0x4a8u
#define MT7977_TOP_EPA_WF               HW_GENMASK(4, 0)
#define MT7977_TOP_EPA_EN               HW_BIT(5)

/* TX loop DC (adie.h, phy_cal.h mask bit 10) on chain bank k = WF (not the SX
 * bank): 0x744 (MT7977_RCD_744) = 0x100, 0x745 (MT7977_SX_DCOC_745) =
 * 0x03003000, 0x746 (MT7977_RCD_746) = 0x00011414, 0x747
 * (MT7977_SX_DCOC_747) = 0x20000000 before the 0x922 setup; after the 8 steps
 * 0, 0, 0x1414, 0; the exit restores 0x00200000, 0x3000, 0x1414, 0. TOP
 * 0x4a8 as for the DC cal above ([4:0] = 0, [5] = 1). [static][measured] */
#define MT7977_LOOPDC_744               0x00000100u
#define MT7977_LOOPDC_745               0x03003000u
#define MT7977_LOOPDC_746               0x00011414u
#define MT7977_LOOPDC_747               0x20000000u

/* The same TOP words in the channel-set TX calibrations (rfmodes.h; all
 * restored) and the TTG RX calibrations (TOP 0x043 [31:28] = 0xc, above):
 * [static][measured]
 * MT7977_TOP_DCOC_WF(wf) is one word per bank k = wf. T2{4} writes
 *   0x20000000 to the word of the transmitting WF's feedback partner (0x04a
 *   for k 4, 0x043 for k 0, the loop network); T2{5} sets [31:28] = 0xb on
 *   the WF's own word, then (P[6] = 1) writes it whole, 0xb0000000; the TX
 *   gain-step, TX IQ and TX LO / IQ calibrations (T1{7,11,15}, T1{8,12,16},
 *   T1{9,13,17}, rfmodes.h) set [31:28] = 0xb on the WF's own word too
 *   (RMW, [27:0] kept).
 * MT7977_TOP_EPA: the FEM helper V:e00526c2 in mode 1 (T2{5}; T2{6} with
 *   P[6]) writes [4:0] = 1 << WF, then [5] = 1; T2{8} (P[6] set, OTFIQ CFG0
 *   [26] clear, phy_cal.h) writes [4:0] = 0, then [5] = 1, as mode 0 does
 *   (the DC cal above, T1{6,10,14}, T1{7,11,15}, T1{8,12,16},
 *   T1{9,13,17}). */
#define MT7977_TOP_DCOC_WF_MODE         HW_GENMASK(31, 28)
#define MT7977_TOP_DCOC_WF_LOOP         0x20000000u	/* T2{4}, feedback partner */
#define MT7977_TOP_DCOC_WF_TSSI         0xb0000000u	/* T2{5}, the WF */

/* TX gain-step calibration (T1{7,11,15}, rfmodes.h), bank k = WF: chain
 * 0x12b [0] = 1 (read 2 on die 1, 0 on die 2; restored) and 0x13b
 * (rfmodes.h ADIE_RX_13B) [31] = 1 (0x04da6d36 -> 0x84da6d36; restored).
 * [static][measured] */
#define MT7977_CHAIN_12B                0x12bu	/* MT7976 RF_INIT writes it on die 0 too, role there not decoded */
#define MT7977_CHAIN_12B_LOOP           HW_BIT(0)
#define MT7977_RX_13B_LOOP              HW_BIT(31)	/* ADIE_RX_13B */

/* TX LO / IQ calibration (phy_cal.h T1{9,13,17}, rfmodes.h), bank k = WF:
 * 0x146 [30:25] = 0x3a in the mode set (V:e005253a; RMW, mask 0x81ffffff |
 * 0x74000000: 0x20050ca0 -> 0x74050ca0); 0x244 per step from the TX gain HAL
 * (V:e00686ec): [9] = bit 3 and [21:19] = [2:0] of one table byte, [18:16] =
 * a second table byte (7 in stock) shifted unmasked (a byte > 7 would also
 * set bits above 18) (phy_cal.h MT7996_PHY_TXLOIQ_T77_244*; observed
 * 0x00000200..0x003f0200). Names unknown. Both restored.
 * [static, measured] */
#define MT7977_CHAIN_146                0x146u
#define MT7977_CHAIN_146_3025           HW_GENMASK(30, 25)	/* 0x3a in the TX LO / IQ cal */
#define MT7977_TX_244                   0x244u	/* MT7976 RF_INIT writes it on die 0 too, role there not decoded */
#define MT7977_TX_244_B9                HW_BIT(9)
#define MT7977_TX_244_2119              HW_GENMASK(21, 19)
#define MT7977_TX_244_1816              HW_GENMASK(18, 16)

/* DPD (T2{0..3}, rfmodes.h): chain states, loop network, TOP words and FEM
 * as T2{4}. The SW AGC steps the feedback partner's gain through this WM
 * table, 11 words indexed by the gain code g 0..10 (start 5), written to
 * the partner's 0x121 [13:10] (rfmodes.h ADIE_TSSI_121_GAIN): 7, 5, 4, 3,
 * 2, 1, 0xd, 0, 0xa, 9, 8. [static, measured] */
#define MT7977_DPD_FB_GAIN_TBL          0xe008034cu	/* WM table, 11 words */

#endif /* HW_MT7977_ADIE_H */
