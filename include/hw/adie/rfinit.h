/* SPDX-License-Identifier: MIT */
/*
 * Calibration item 0, the A-die RF init, and the two power-on calibrations that run from inside
 * it, T0[0] (R-cal) and T0[1] (RC calibration): MediaTek's tables and the constants of the
 * procedure that the other headers do not hold. Used by firmware/radio/rfinit.c; the procedure
 * itself is described in adie.h ("RF_INIT and the WF tables", "RC calibration and RC trim",
 * ADIE_RCAL) and phy.h (MT7996_PHY_RFIF_D414, MT7996_PHY_RF_TBL_KEY).
 *
 * GENERATED from MediaTek's WM image (vendor__mt7996_wm_tm.elf, sha256 01e320509194333a...; V:
 * addresses) by the research tree's radio-port/rfinit/gen_rfinit.py: the tables are read from
 * the image as data and every expansion is checked against the decodes' walker models, which
 * reproduce MediaTek's cold load record for record (spitrace-20261009T2225Z mt7976-decode /
 * mt7977-decode). Do not edit by hand.
 *
 * TABLES. MediaTek's RFPG tables come in two forms, both written to the chain bank k = WF (the
 * die is the band): pre-rows (V:e0051536 MT7977, V:e0050d3e MT7976, V:e0050a30 7976C), one
 * value per register for every WF, X(reg, value); and walker tables (V:e0051af2 MT7977,
 * V:e0050f44 MT7976, V:e0050b2a 7976C), whose register sequence is fixed and whose value comes
 * from a column chosen by a key, X(reg, column 0, column 1, ...). ADIE_RFTBL_SKIP in a column
 * is MediaTek's -2: that column writes nothing for the row (the walker skips it). Rows are in
 * MediaTek's order; a register can appear twice. [static][measured: the decodes regenerate
 * every write of the cold trace with mask 0x00020001 and of the stock load]
 *
 * NAMES. Every register is named by the header of its block: adie.h (with the offset names
 * ADIE_CHAIN_<index> of the chain-bank words of unknown role), sx.h (ADIE_SX_<index> in the
 * synthesizer family 0xa00..0xbff), rfmodes.h, mt7976.h, mt7977.h; a die-specific name also
 * names the same index in the other die type's rows when that is its only name (the comment
 * at the name says so). This header holds only the tables and their selection constants.
 */
#ifndef HW_ADIE_RFINIT_H
#define HW_ADIE_RFINIT_H

#include "../hw_bits.h"
#include "adie.h"
#include "mt7976.h"
#include "mt7977.h"
#include "rfmodes.h"
#include "sx.h"

/* ------------------------------------------------------------------------
 * Selection
 * ------------------------------------------------------------------------ */
/* MT7976C: ADIE_TOP_CHIP_ID_11_8 == ADIE_TOP_CHIP_ID_11_8_MT7976C (adie.h) adds MediaTek's
 * "dap" tables (MT7976C_*, below) after the MT7976 tables. [static][measured] */

/* The MT7977 variant: MediaTek's ECO byte (die 2's TOP chip ID [7:0], radio.h radio_eco()) <= 9
 * sets P[1] = 1 (V:e0051c2a: bgeu 9, ECO), which selects MT7977_RFINIT_WALK_ECO9 below and
 * +0xd414 = MT7996_PHY_RFIF_D414_MT7977_ECO9; above 9, P[1] = 0, MT7977_RFINIT_WALK and
 * MT7996_PHY_RFIF_D414_MT7977. The ECO byte of die 2 selects die 1's variant too. 0x10 here.
 * [static][measured] */
#define MT7977_ECO_P1_MAX                9u	/* ECO <= 9: P[1] = 1 */

/* ------------------------------------------------------------------------
 * Tables
 * ------------------------------------------------------------------------ */
#define ADIE_RFTBL_SKIP                  0xfffffffeu	/* -2: no write */

/* MT7977 RF_INIT pre-rows (V:e0051c04 -> V:e0051536): descriptor 0xe007fdb4, values 0xe007fc18,
 * 0x35 rows, index column 0; every WF 0..4 (k = WF), before that WF's walk. X(reg, value).
 * Two of them are defaults later code overwrites: ADIE_RCTRIM (RC calibration) and
 * MT7977_TTG_636 (the channel set's gain steps). [static][measured] */
#define MT7977_RFINIT_PRE(X) \
	X(ADIE_DCOC_11A,      0x80013814u) \
	X(MT7976_MON_EN,      0x00600000u) \
	X(ADIE_CHAIN_124,     0x1400ea14u) \
	X(ADIE_CHAIN_126,     0x0000000du) \
	X(ADIE_CHAIN_13C,     0x0000053cu) \
	X(MT7977_TTG_ROUTE,   0x00007f00u) \
	X(ADIE_CHAIN_145,     0x64400b64u) \
	X(MT7977_CHAIN_146,   0x20050ca0u) \
	X(MT7977_TTG_636,     0x00a86420u) \
	X(ADIE_CHAIN_637,     0x00007fb5u) \
	X(ADIE_CHAIN_638,     0x0001b491u) \
	X(ADIE_CHAIN_950,     0x80000000u) \
	X(ADIE_CHAIN_148,     0x81000000u) \
	X(ADIE_CALTAIL_031,   0x00285008u) \
	X(ADIE_CHAIN_033,     0x14001408u) \
	X(ADIE_CHAIN_035,     0x00000004u) \
	X(ADIE_CHAIN_036,     0x00280000u) \
	X(ADIE_CHSET,         0xaa000003u) \
	X(ADIE_TSSI_477,      0xd0f01007u) \
	X(ADIE_CHAIN_485,     0x50f01007u) \
	X(ADIE_BW_AUX0,       0x00000001u) \
	X(ADIE_BW_AUX1,       0x00000001u) \
	X(ADIE_CHAIN_13A,     0x00000000u) \
	X(ADIE_CHAIN_309,     0x08102040u) \
	X(ADIE_CHAIN_30A,     0x08102040u) \
	X(ADIE_CHAIN_30B,     0x08102040u) \
	X(ADIE_CHAIN_30C,     0x08102040u) \
	X(ADIE_CHAIN_30D,     0x08102040u) \
	X(ADIE_CHAIN_30E,     0x08102040u) \
	X(ADIE_CHAIN_30F,     0x08102040u) \
	X(ADIE_CHAIN_310,     0x08102040u) \
	X(ADIE_SX_BA3,        0x00000047u) \
	X(ADIE_CHAIN_2A4,     0x398a4188u) \
	X(ADIE_CHAIN_2A5,     0x080e6290u) \
	X(ADIE_CHAIN_2A6,     0x18820000u) \
	X(ADIE_CHAIN_637,     0x00007ff6u) \
	X(ADIE_CHAIN_638,     0x0002d6d2u) \
	X(ADIE_CHAIN_748,     0xddddddddu) \
	X(ADIE_CHAIN_749,     0xcba87643u) \
	X(ADIE_CHAIN_74C,     0x00077311u) \
	X(MT7977_SX_DCOC_745, 0x00003000u) \
	X(MT7977_SX_DCOC_14A, 0x800aaaaau) \
	X(ADIE_FB_SEL,        0x000003ffu) \
	X(ADIE_RCTRIM,        0x00c00c00u) \
	X(ADIE_CHAIN_E52,     0x0c000c00u) \
	X(ADIE_CHAIN_E53,     0x0c000c00u) \
	X(ADIE_CHAIN_E54,     0x0c000c00u) \
	X(ADIE_CHAIN_E55,     0x0c000c00u) \
	X(ADIE_CHAIN_E56,     0x0c000c00u) \
	X(ADIE_CHAIN_E57,     0x0c000c00u) \
	X(ADIE_CHAIN_115,     0x000d0f80u) \
	X(ADIE_LOOP_129,      0x00004000u) \
	X(ADIE_CHAIN_025,     0x07270000u)

/* MT7977 RF_INIT walker (V:e0051af2), ECO byte > 9 (this unit): descriptor 0xe007f964 (6 groups),
 * offsets 0xe007faf4, values 0xe007fc18, column map 0xe007faac; key = the position of P[1] * 5 +
 * WF in 0xe007fae8 (0..9 in order), P[1] = 0: column = WF, k = WF; k 4 is the synthesizer bank.
 * Writes per WF 0..4: 58/58/58/58/33. X(reg, wf0, ..., wf4). [static][measured] */
#define MT7977_RFINIT_WALK(X) \
	X(ADIE_RX_13B,    0x04da6d36u, 0x04da6d36u, 0x04da6d36u, 0x04da6d36u, 0x04da6d36u) \
	X(ADIE_SX_B94,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x0000000fu) \
	X(ADIE_SX_B95,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x0f8629c6u) \
	X(ADIE_SX_B84,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x7831a084u) \
	X(ADIE_SX_132,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0xd5110100u) \
	X(ADIE_SX_A08,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x02031100u) \
	X(ADIE_SX_134,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x00fd2bf0u) \
	X(ADIE_RX_12F,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0xffc04622u) \
	X(ADIE_SX_133,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x8ca320a0u) \
	X(ADIE_CHAIN_364, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_365, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_366, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_367, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_36E, 0x20820820u, 0x20820820u, 0x20820820u, 0x20820820u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_36F, 0x20820820u, 0x20820820u, 0x20820820u, 0x20820820u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_370, 0x20820820u, 0x20820820u, 0x20820820u, 0x20820820u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_371, 0x20820820u, 0x20820820u, 0x20820820u, 0x20820820u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_372, 0x55555000u, 0x55555000u, 0x55555000u, 0x55555000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_373, 0x55555000u, 0x55555000u, 0x55555000u, 0x55555000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_374, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_375, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_376, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_377, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_378, 0x6db6db6cu, 0x6db6db6cu, 0x6db6db6cu, 0x6db6db6cu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_379, 0x6db6db6cu, 0x6db6db6cu, 0x6db6db6cu, 0x6db6db6cu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_37A, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_37B, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_37C, 0x91200077u, 0x91200077u, 0x91200077u, 0x91200077u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_11D, 0x00010000u, 0x00010000u, 0x00010000u, 0x00010000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_72F, 0x50504830u, 0x50504830u, 0x50504830u, 0x50504830u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_722, 0x50504830u, 0x50504830u, 0x50504830u, 0x50504830u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_667, 0x77765435u, 0x77765435u, 0x77765435u, 0x77765435u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_668, 0x43210210u, 0x43210210u, 0x43210210u, 0x43210210u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_734, 0x20203050u, 0x20203050u, 0x20203050u, 0x20203050u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_724, 0x20203050u, 0x20203050u, 0x20203050u, 0x20203050u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_735, 0x20203050u, 0x20203050u, 0x20203050u, 0x20203050u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_725, 0x20203050u, 0x20203050u, 0x20203050u, 0x20203050u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_73D, 0xdca7cca3u, 0xdca7cca3u, 0xdca7cca3u, 0xdca7cca3u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_72D, 0xdca7cca3u, 0xdca7cca3u, 0xdca7cca3u, 0xdca7cca3u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_727, 0x01008888u, 0x01008888u, 0x01008888u, 0x01008888u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_728, 0x01008888u, 0x01008888u, 0x01008888u, 0x01008888u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_737, 0x01008888u, 0x01008888u, 0x01008888u, 0x01008888u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_738, 0x01008888u, 0x01008888u, 0x01008888u, 0x01008888u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_72A, 0x00001111u, 0x00001111u, 0x00001111u, 0x00001111u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_72B, 0x00001111u, 0x00001111u, 0x00001111u, 0x00001111u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_73A, 0x00001111u, 0x00001111u, 0x00001111u, 0x00001111u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_73B, 0x00001111u, 0x00001111u, 0x00001111u, 0x00001111u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_03A, 0x00001003u, 0x00001003u, 0x00001003u, 0x00001003u, 0x00000000u) \
	X(ADIE_SX_B8A,    0x00000fe2u, 0x00000fe2u, 0x00000fe2u, 0x00000fe2u, 0x54296473u) \
	X(ADIE_SX_B8B,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x0000f876u) \
	X(ADIE_SX_B83,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x50001c50u) \
	X(ADIE_SX_B8D,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x000a0000u) \
	X(ADIE_CHAIN_14B, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x00000020u) \
	X(ADIE_SX_A05,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x005140d5u) \
	X(ADIE_CHAIN_E90, 0x01630210u, 0x01630210u, 0x01630210u, 0x01630210u, 0x01630210u) \
	X(ADIE_CHAIN_E91, 0x00780230u, 0x00780230u, 0x00780230u, 0x00780230u, 0x00780230u) \
	X(ADIE_CHAIN_E92, 0x00000300u, 0x00000300u, 0x00000300u, 0x00000300u, 0x00000300u) \
	X(ADIE_RX_139,    0x20000000u, 0x20000000u, 0x20000000u, 0x20000000u, 0x20000000u) \
	X(ADIE_SX_A07,    0x08080009u, 0x08080009u, 0x08080009u, 0x08080009u, 0x08080009u) \
	X(ADIE_CHAIN_116, 0x44051200u, 0x44051200u, 0x44051200u, 0x44051200u, 0x44051200u) \
	X(ADIE_CHAIN_E93, 0x00080082u, 0x00080082u, 0x00080082u, 0x00080082u, 0x00080082u) \
	X(ADIE_CHAIN_750, 0x000caaaau, 0x000caaaau, 0x000caaaau, 0x000caaaau, 0x000caaaau) \
	X(ADIE_CHAIN_751, 0x000cbabau, 0x000cbabau, 0x000cbabau, 0x000cbabau, 0x000cbabau) \
	X(ADIE_CHAIN_6BB, 0x00044666u, 0x00044666u, 0x00044666u, 0x00044666u, 0x00044666u) \
	X(ADIE_RX_113,    0x00f80010u, 0x00f80010u, 0x00f80010u, 0x00f80010u, 0x00f80010u) \
	X(ADIE_CHAIN_6C1, 0x00077100u, 0x00077100u, 0x00077100u, 0x00077100u, 0x00077100u) \
	X(ADIE_LOOP_12D,  0x00000490u, 0x00000490u, 0x00000490u, 0x00000490u, 0x00000490u) \
	X(ADIE_CHAIN_12E, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u) \
	X(ADIE_SX_B83,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x50001850u) \
	X(ADIE_CHAIN_143, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x03120900u) \
	X(MT7976_LOBUF_A13, 0x088a0400u, 0x088a0400u, 0x08890400u, 0x08880400u, 0x08890400u) \
	X(MT7976_MON_EN,  0x00141402u, 0x00141402u, 0x00141402u, 0x00141402u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_123, 0x00b75020u, 0x00b75020u, 0x00b75020u, 0x00b75020u, ADIE_RFTBL_SKIP)

/* The same walker for ECO byte <= 9 (MT7977_ECO_P1_MAX): P[1] = 1, keys 5..9; the register
 * sequence is the same (rows that write nothing in any column left out). Writes per WF 0..4:
 * 50/50/50/50/27. Not run on this unit. X(reg, wf0, ..., wf4). [static] */
#define MT7977_RFINIT_WALK_ECO9(X) \
	X(ADIE_RX_13B,    0x04da6d36u, 0x04da6d36u, 0x04da6d36u, 0x04da6d36u, 0x04da6d36u) \
	X(ADIE_SX_B94,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x0000000fu) \
	X(ADIE_SX_B95,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x0f8629c6u) \
	X(ADIE_SX_B84,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x7831a084u) \
	X(ADIE_SX_132,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0xd5110100u) \
	X(ADIE_SX_A08,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x02031100u) \
	X(ADIE_SX_134,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x00fd2bf0u) \
	X(ADIE_RX_12F,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0xffc04622u) \
	X(ADIE_SX_133,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x8ca320a0u) \
	X(ADIE_CHAIN_364, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_365, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_366, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_367, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_36E, 0x20820820u, 0x20820820u, 0x20820820u, 0x20820820u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_36F, 0x20820820u, 0x20820820u, 0x20820820u, 0x20820820u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_370, 0x20820820u, 0x20820820u, 0x20820820u, 0x20820820u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_371, 0x20820820u, 0x20820820u, 0x20820820u, 0x20820820u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_372, 0x55555000u, 0x55555000u, 0x55555000u, 0x55555000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_373, 0x55555000u, 0x55555000u, 0x55555000u, 0x55555000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_374, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_375, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_376, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_377, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_378, 0x6db6db6cu, 0x6db6db6cu, 0x6db6db6cu, 0x6db6db6cu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_379, 0x6db6db6cu, 0x6db6db6cu, 0x6db6db6cu, 0x6db6db6cu, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_37A, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_37B, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, 0xaaaaa000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_37C, 0x91200077u, 0x91200077u, 0x91200077u, 0x91200077u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_11D, 0x00010000u, 0x00010000u, 0x00010000u, 0x00010000u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_72F, 0x50504830u, 0x50504830u, 0x50504830u, 0x50504830u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_722, 0x50504830u, 0x50504830u, 0x50504830u, 0x50504830u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_667, 0x77765435u, 0x77765435u, 0x77765435u, 0x77765435u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_668, 0x43210210u, 0x43210210u, 0x43210210u, 0x43210210u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_734, 0x20203050u, 0x20203050u, 0x20203050u, 0x20203050u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_724, 0x20203050u, 0x20203050u, 0x20203050u, 0x20203050u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_735, 0x20203050u, 0x20203050u, 0x20203050u, 0x20203050u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_725, 0x20203050u, 0x20203050u, 0x20203050u, 0x20203050u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_73D, 0xdca7cca3u, 0xdca7cca3u, 0xdca7cca3u, 0xdca7cca3u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_72D, 0xdca7cca3u, 0xdca7cca3u, 0xdca7cca3u, 0xdca7cca3u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_727, 0x01008888u, 0x01008888u, 0x01008888u, 0x01008888u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_728, 0x01008888u, 0x01008888u, 0x01008888u, 0x01008888u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_737, 0x01008888u, 0x01008888u, 0x01008888u, 0x01008888u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_738, 0x01008888u, 0x01008888u, 0x01008888u, 0x01008888u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_72A, 0x00001111u, 0x00001111u, 0x00001111u, 0x00001111u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_72B, 0x00001111u, 0x00001111u, 0x00001111u, 0x00001111u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_73A, 0x00001111u, 0x00001111u, 0x00001111u, 0x00001111u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_73B, 0x00001111u, 0x00001111u, 0x00001111u, 0x00001111u, ADIE_RFTBL_SKIP) \
	X(ADIE_CHAIN_03A, 0x00001003u, 0x00001003u, 0x00001003u, 0x00001003u, 0x00000000u) \
	X(ADIE_SX_B8A,    0x00000fe2u, 0x00000fe2u, 0x00000fe2u, 0x00000fe2u, 0x54296473u) \
	X(ADIE_SX_B8B,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x0000f876u) \
	X(ADIE_SX_B83,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x50001c50u) \
	X(ADIE_SX_B8D,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x000a0000u) \
	X(ADIE_CHAIN_14B, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x00000020u) \
	X(ADIE_SX_A05,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x005140d5u) \
	X(ADIE_CHAIN_E90, 0x03630210u, 0x03630210u, 0x03630210u, 0x03630210u, 0x03630210u) \
	X(ADIE_CHAIN_E91, 0x004b015eu, 0x004b015eu, 0x004b015eu, 0x004b015eu, 0x004b015eu) \
	X(ADIE_CHAIN_E92, 0x000001e0u, 0x000001e0u, 0x000001e0u, 0x000001e0u, 0x000001e0u) \
	X(ADIE_RX_139,    0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u) \
	X(ADIE_SX_A07,    0x08080007u, 0x08080007u, 0x08080007u, 0x08080007u, 0x08080007u) \
	X(ADIE_CHAIN_116, 0xfc050000u, 0xfc050000u, 0xfc050000u, 0xfc050000u, 0xfc050000u) \
	X(ADIE_CHAIN_750, 0x000eeeeau, 0x000eeeeau, 0x000eeeeau, 0x000eeeeau, 0x000eeeeau) \
	X(ADIE_RX_113,    0x00a80010u, 0x00a80010u, 0x00a80010u, 0x00a80010u, 0x00a80010u) \
	X(ADIE_SX_B83,    ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x50001e50u) \
	X(ADIE_CHAIN_143, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, ADIE_RFTBL_SKIP, 0x12040610u) \
	X(MT7976_LOBUF_A13, 0x08890400u, 0x08880400u, 0x08870400u, 0x08870400u, 0x08890400u)

/* MT7976 "7976 RF_INIT" pre-rows (V:e00512e8 -> V:e0051038 -> V:e0050d3e): descriptor 0xe0075640,
 * values 0xe007556c, 0x33 rows, index column 0; every WF 0..4 (k = WF), before that WF's walk.
 * ADIE_RCAL = ADIE_RCAL_RFINIT_MT7976 here is the R-cal default. X(reg, value). [static][measured] */
#define MT7976_RFINIT_PRE(X) \
	X(ADIE_CHAIN_1D0,   0x20210304u) \
	X(MT7976_LO_DIST5G, 0x00000000u) \
	X(ADIE_DCOC_602,    0x00000001u) \
	X(ADIE_RX_110,      0x73800000u) \
	X(ADIE_CHAIN_117,   0x00003003u) \
	X(ADIE_SX_B90,      0x00000090u) \
	X(ADIE_RCAL,        0x00000368u) \
	X(ADIE_CHAIN_E90,   0x01630210u) \
	X(ADIE_CHAIN_E91,   0x004f01e0u) \
	X(ADIE_CHAIN_E92,   0x00000260u) \
	X(ADIE_CHAIN_118,   0xe0000000u) \
	X(ADIE_CHAIN_119,   0x70000000u) \
	X(ADIE_DCOC_11A,    0x941007cdu) \
	X(ADIE_CHAIN_11D,   0x81023008u) \
	X(MT7977_TX_244,    0x00040004u) \
	X(ADIE_CHAIN_245,   0x0003000cu) \
	X(ADIE_CHAIN_246,   0x0014001cu) \
	X(ADIE_CHAIN_247,   0x001b0019u) \
	X(ADIE_CHAIN_130,   0x4d0f584fu) \
	X(ADIE_CHAIN_131,   0x3e94c4f6u) \
	X(ADIE_SX_132,      0x6f70c000u) \
	X(ADIE_SX_134,      0xff4f47d8u) \
	X(ADIE_CHAIN_135,   0x0001e8b0u) \
	X(ADIE_SX_B90,      0x00000090u) \
	X(ADIE_SX_B82,      0x002493e0u) \
	X(ADIE_SX_B99,      0x5c322320u) \
	X(ADIE_SX_BB0,      0x00a40f02u) \
	X(ADIE_SX_138,      0x00000008u) \
	X(ADIE_CHAIN_127,   0x162000ffu) \
	X(ADIE_LOOP_129,    0x65f01040u) \
	X(ADIE_CHAIN_12A,   0x00d42000u) \
	X(MT7977_CHAIN_12B, 0x80000000u) \
	X(ADIE_CHAIN_290,   0x22222222u) \
	X(ADIE_CHAIN_291,   0x22222222u) \
	X(ADIE_CHAIN_292,   0x33333333u) \
	X(ADIE_CHAIN_293,   0x33333333u) \
	X(ADIE_CHAIN_294,   0x33333333u) \
	X(ADIE_CHAIN_295,   0x33333333u) \
	X(ADIE_TSSI_121,    0x3f80009du) \
	X(ADIE_CHAIN_694,   0x05053310u) \
	X(ADIE_CHAIN_11E,   0xb86b8000u) \
	X(ADIE_CHAIN_11F,   0x000002c0u) \
	X(MT7976_MON_BAND,  0x00010000u) \
	X(ADIE_TTG_ROUTE,   0x10000000u) \
	X(ADIE_CHAIN_641,   0xe182051cu) \
	X(ADIE_CHAIN_E54,   0x10141014u) \
	X(ADIE_CHAIN_E55,   0x10141014u) \
	X(ADIE_CHAIN_013,   0x004023c5u) \
	X(ADIE_CHAIN_033,   0x14081408u) \
	X(ADIE_CHAIN_104,   0x00000001u) \
	X(ADIE_CHAIN_025,   0x07270000u)

/* MT7976 RF_INIT walker (V:e0050f44, u8 value index): descriptor 0xe0075540 (2 groups), offsets
 * 0xe007555c, values 0xe007556c, column map 0xe0075550; column = WF (key list 0xe0081428 is
 * 0..4). X(reg, wf0, ..., wf4). [static][measured] */
#define MT7976_RFINIT_WALK(X) \
	X(MT7976_LO_SRC2, 0x72000000u, 0x72000000u, 0x72000000u, 0x72000000u, 0x73000000u) \
	X(MT7976_MON_EN,  0xe80483dfu, 0xe80483dfu, 0xe80483dfu, 0xe80483dfu, 0xea0483dfu) \
	X(ADIE_SX_B8B,    0x000d9876u, 0x000d9876u, 0x000d9876u, 0x000d9876u, 0x0d987fe6u) \
	X(MT7977_RX_12C,  0x40000880u, 0x40000880u, 0x60000880u, 0x6a000880u, 0x40000880u)

/* MT7976 "7976 RF_COMMON" pre-rows (V:e00510f2 -> V:e0050d3e): descriptor 0xe0075508, values
 * 0xe0075120, 9 rows, index column P[2] = 0; WF 0..3 (k = WF), after the key record and before
 * the walk. X(reg, value). [static][measured] */
#define MT7976_RF_COMMON_PRE(X) \
	X(ADIE_CHAIN_64B, 0x318cc600u) \
	X(ADIE_CHAIN_69D, 0x00000000u) \
	X(ADIE_CHAIN_69E, 0x00000000u) \
	X(ADIE_CHAIN_69F, 0x00000000u) \
	X(ADIE_CHAIN_6A3, 0x00000000u) \
	X(ADIE_CHAIN_6A4, 0x00000000u) \
	X(ADIE_CHAIN_E54, 0x10001000u) \
	X(ADIE_CHAIN_E55, 0x10001000u) \
	X(ADIE_SX_A53,    0x08860000u)

/* MT7976 RF_COMMON walker (V:e0050f44, pairs, offset column P[2] = 0): descriptor 0xe0074998 (0x1f
 * groups), offsets 0xe0074fc8, values 0xe0075120, column map 0xe0074c48; column = WF 0..3, the
 * key being the position of P[1] * 5 + WF (P[1] = 0) in 0xe0074fac (MT7976_RF_COMMON_KEYS).
 * Writes per WF: 86/86/86/86. X(reg, wf0, ..., wf3). [static][measured] */
#define MT7976_RF_COMMON_WALK(X) \
	X(ADIE_BW_AUX0,     0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u) \
	X(MT7976_TXTRIM_SRC(0u), 0x00302000u, 0x00302000u, 0x00302000u, 0x00302000u) \
	X(MT7976_TXTRIM_SRC(1u), 0x24302000u, 0x24302000u, 0x24302000u, 0x24302000u) \
	X(MT7976_TXTRIM_SRC(2u), 0x68302000u, 0x68302000u, 0x68302000u, 0x68302000u) \
	X(MT7976_TXTRIM_SRC(3u), 0xec302000u, 0xec302000u, 0xec302000u, 0xec302000u) \
	X(MT7976_TXTRIM_SRC(4u), 0xf0312000u, 0xf0312000u, 0xf0312000u, 0xf0312000u) \
	X(MT7976_TXTRIM_SRC(5u), 0xf0332000u, 0xf0332000u, 0xf0332000u, 0xf0332000u) \
	X(MT7976_TXTRIM_SRC(6u), 0xf1332000u, 0xf1332000u, 0xf1332000u, 0xf1332000u) \
	X(ADIE_CHAIN_287,   0xf1332000u, 0xf1332000u, 0xf1332000u, 0xf1332000u) \
	X(ADIE_CHAIN_640,   0xe182051cu, 0xe182051cu, 0xe182051cu, 0xe182051cu) \
	X(ADIE_CHAIN_643,   0x0ac00000u, 0x0ac00000u, 0x0ac00000u, 0x0ac00000u) \
	X(ADIE_CHAIN_644,   0x77777000u, 0x77777000u, 0x77777000u, 0x77777000u) \
	X(ADIE_CHAIN_648,   0x55400000u, 0x55400000u, 0x55400000u, 0x55400000u) \
	X(ADIE_CHAIN_649,   0x55400000u, 0x55400000u, 0x55400000u, 0x55400000u) \
	X(ADIE_CHAIN_684,   0x0070cc30u, 0x0070cc30u, 0x0070cc30u, 0x0070cc30u) \
	X(ADIE_CHAIN_688,   0x0070cc30u, 0x0070cc30u, 0x0070cc30u, 0x0070cc30u) \
	X(ADIE_CHAIN_E70,   0xf0efeeedu, 0xf0efeeedu, 0xf0efeeedu, 0xf0efeeedu) \
	X(ADIE_CHAIN_E71,   0xf5f4f3f1u, 0xf5f4f3f1u, 0xf5f4f3f1u, 0xf5f4f3f1u) \
	X(ADIE_CHAIN_E72,   0xfaf9f7f6u, 0xfaf9f7f6u, 0xfaf9f7f6u, 0xfaf9f7f6u) \
	X(ADIE_CHAIN_E73,   0xfffefcfbu, 0xfffefcfbu, 0xfffefcfbu, 0xfffefcfbu) \
	X(ADIE_CHAIN_E74,   0x04020100u, 0x04020100u, 0x04020100u, 0x04020100u) \
	X(ADIE_CHAIN_E75,   0x09070605u, 0x09070605u, 0x09070605u, 0x09070605u) \
	X(ADIE_CHAIN_E76,   0x0d0c0b0au, 0x0d0c0b0au, 0x0d0c0b0au, 0x0d0c0b0au) \
	X(ADIE_CHAIN_E77,   0x1211100fu, 0x1211100fu, 0x1211100fu, 0x1211100fu) \
	X(ADIE_CHAIN_E78,   0x17161513u, 0x17161513u, 0x17161513u, 0x17161513u) \
	X(ADIE_CHAIN_E79,   0x1c1b1a18u, 0x1c1b1a18u, 0x1c1b1a18u, 0x1c1b1a18u) \
	X(ADIE_CHAIN_E7A,   0x21201e1du, 0x21201e1du, 0x21201e1du, 0x21201e1du) \
	X(ADIE_CHAIN_E7B,   0x26242322u, 0x26242322u, 0x26242322u, 0x26242322u) \
	X(ADIE_CHAIN_E7C,   0x2b292827u, 0x2b292827u, 0x2b292827u, 0x2b292827u) \
	X(ADIE_CHAIN_E7D,   0x2f2e2d2cu, 0x2f2e2d2cu, 0x2f2e2d2cu, 0x2f2e2d2cu) \
	X(ADIE_CHAIN_E7E,   0x34333231u, 0x34333231u, 0x34333231u, 0x34333231u) \
	X(ADIE_CHAIN_E7F,   0x39383735u, 0x39383735u, 0x39383735u, 0x39383735u) \
	X(ADIE_CHAIN_24C,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfca6e2u, 0xfbfca6e2u) \
	X(ADIE_CHAIN_24D,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfe96e0u, 0xfbfe96e0u) \
	X(ADIE_CHAIN_254,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfe86dbu, 0xfbfe86dbu) \
	X(ADIE_CHAIN_24E,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_24F,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_250,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_251,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_252,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_253,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_255,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_256,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_257,   0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u, 0xfbfec6e1u) \
	X(ADIE_CHAIN_642,   0xbac00000u, 0xbac00000u, 0xbac00000u, 0xbac00000u) \
	X(ADIE_CHAIN_645,   0x77777000u, 0x77777000u, 0x77777000u, 0x77777000u) \
	X(ADIE_CHAIN_646,   0xc8cee000u, 0xc8cee000u, 0xc8cbc000u, 0x8389b000u) \
	X(ADIE_CHAIN_647,   0x000ff000u, 0x000ff000u, 0x000ff000u, 0x000ff000u) \
	X(ADIE_CHAIN_64A,   0x318cc600u, 0x318cc600u, 0x318cc600u, 0x318cc600u) \
	X(ADIE_CHAIN_680,   0x53645342u, 0x53645342u, 0x53645342u, 0x53645342u) \
	X(ADIE_CHAIN_699,   0xb2bb8000u, 0xb2bb8000u, 0xb2bb8000u, 0xb2bb8000u) \
	X(ADIE_CHAIN_681,   0x30000000u, 0x30000000u, 0x30000000u, 0x30000000u) \
	X(ADIE_CHAIN_682,   0x64534242u, 0x64534242u, 0x75534242u, 0x75534242u) \
	X(ADIE_CHAIN_683,   0x30000000u, 0x30000000u, 0x30000000u, 0x30000000u) \
	X(ADIE_CHAIN_685,   0x68939000u, 0x68939000u, 0x68939000u, 0x68939000u) \
	X(ADIE_CHAIN_689,   0x68939000u, 0x68939000u, 0x68939000u, 0x68939000u) \
	X(ADIE_CHAIN_686,   0x0070cc30u, 0x0070cc30u, 0x0070cc30u, 0x0070cc30u) \
	X(ADIE_CHAIN_68A,   0x0070cc30u, 0x0070cc30u, 0x0070cc30u, 0x0070cc30u) \
	X(ADIE_CHAIN_68E,   0x14b2152cu, 0x14b2152cu, 0x14b2152cu, 0x14b2152cu) \
	X(ADIE_CHAIN_692,   0x3e173484u, 0x3e173484u, 0x3e173484u, 0x3e173484u) \
	X(ADIE_CHAIN_687,   0x68939000u, 0x68939000u, 0x68939000u, 0x68939000u) \
	X(ADIE_CHAIN_68B,   0x68939000u, 0x68939000u, 0x68939000u, 0x68939000u) \
	X(ADIE_CHAIN_68F,   0x5cb5d500u, 0x5cb5d500u, 0x5cb5d500u, 0x5cb5d500u) \
	X(ADIE_CHAIN_693,   0xfe1ff400u, 0xfe1ff400u, 0xfe1ff400u, 0xfe1ff400u) \
	X(ADIE_CHAIN_68C,   0x21514b20u, 0x21514b20u, 0x21514b20u, 0x21514b20u) \
	X(ADIE_CHAIN_690,   0x7343e170u, 0x7343e170u, 0x7343e170u, 0x7343e170u) \
	X(ADIE_CHAIN_68D,   0x54b5d500u, 0x54b5d500u, 0x54b5d500u, 0x54b5d500u) \
	X(ADIE_CHAIN_691,   0xd21ff400u, 0xd21ff400u, 0xd21ff400u, 0xd21ff400u) \
	X(ADIE_CHAIN_695,   0x15658000u, 0x15658000u, 0x15658000u, 0x15658000u) \
	X(ADIE_CHAIN_697,   0x15658000u, 0x15658000u, 0x15658000u, 0x15658000u) \
	X(ADIE_CHAIN_696,   0x15658000u, 0x15658000u, 0x15658000u, 0x15658000u) \
	X(ADIE_CHAIN_698,   0x15658000u, 0x15658000u, 0x15658000u, 0x15658000u) \
	X(ADIE_CHAIN_69A,   0x26bbc000u, 0x26bbc000u, 0x26bbc000u, 0x26bbc000u) \
	X(ADIE_CHAIN_69C,   0xbbffc000u, 0xbbffc000u, 0xbbffc000u, 0xbbffc000u) \
	X(ADIE_CHAIN_69B,   0xbbbfc000u, 0xbbbfc000u, 0xbbbfc000u, 0xbbbfc000u) \
	X(ADIE_CHAIN_6A0,   0x00804000u, 0x00804000u, 0x00000000u, 0x00000000u) \
	X(ADIE_CHAIN_6A1,   0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u) \
	X(ADIE_CHAIN_6A2,   0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u) \
	X(ADIE_SX_B87,      0x50006208u, 0x50006208u, 0x50006208u, 0x50006208u) \
	X(ADIE_SX_A47,      0x00000508u, 0x00000508u, 0x00000508u, 0x00000508u) \
	X(ADIE_SX_B86,      0x0002061fu, 0x0002061fu, 0x0002061fu, 0x0002061fu) \
	X(ADIE_SX_A54,      0x40040002u, 0x40040002u, 0x40040002u, 0x40040002u) \
	X(MT7976_LOBUF_A06, 0x08088408u, 0x0808868fu, 0x08088408u, 0x08088408u) \
	X(ADIE_SX_A07,      0x00000508u, 0x00000408u, 0x00000408u, 0x00000408u) \
	X(MT7976_LOBUF_A13, 0x84860400u, 0x86860400u, 0x08860400u, 0x84860400u) \
	X(ADIE_SX_BB4,      0x801e3106u, 0x801e3106u, 0x801e3106u, 0x801e3106u)
#define MT7976_RF_COMMON_KEYS            { 0u, 1u, 2u, 3u }

/* MT7976 "7976 RF_EPA_COMMON" walker (V:e00512a6 -> V:e0050f44, pairs, P[2] = 0): descriptor
 * 0xe0074514 (6 groups), offsets 0xe0074720, values 0xe0074750, column map 0xe007465c; column =
 * WF 0..3 (position of WF in 0xe0074704). It overwrites RF_COMMON's 0x24c..0x257. Item 0 applies
 * it on every MT7976 whatever the EEPROM's EPA bit (V:e00512e8 tests nothing).
 * X(reg, wf0, ..., wf3). [static][measured] */
#define MT7976_RF_EPA_COMMON_WALK(X) \
	X(ADIE_CHAIN_24C, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_254, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_255, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_256, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_24D, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_24F, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_250, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_257, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_24E, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_251, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_252, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_253, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u)

/* MT7976 "7976 XPA" pre-rows (V:e0050d8e): descriptor 0xe007488c, values 0xe0074878, 5 rows,
 * column 0; WF 0..3 (k = WF), then TOP ADIE_TOP_71C = ADIE_TOP_71C_XPA. Applied whatever the EEPROM's EPA bit.
 * X(reg, value). [static][measured] */
#define MT7976_XPA_PRE(X) \
	X(ADIE_TX_GAIN_MODE, 0xcfefc7efu) \
	X(ADIE_TX_GAIN,      0xc3efc1efu) \
	X(ADIE_CHAIN_242,    0xc1e7c0e7u) \
	X(ADIE_CHAIN_243,    0xc0e3c0e3u) \
	X(MT7976_CHAIN_218,  0x00000060u)

/* MT7976 "7976 XLNA" pre-row (V:e0050dd2): descriptor 0xe008137c, values 0xe0081378, 1 row,
 * column 0; WF 0..3 (k = WF). Applied whatever the EEPROM's ELNA bit. X(reg, value).
 * [static][measured] */
#define MT7976_XLNA_PRE(X) \
	X(ADIE_RX_INPUT, 0xa0000000u)

/* 7976C "7976dap RF_EPA_COMMON" walker (V:e0050cbe -> V:e0050c7c -> V:e0050b2a, u16 value index,
 * pairs, P[2] = 0): descriptor 0xe0073904 (7 groups), offsets 0xe0073c44, values 0xe0073c78,
 * column map 0xe0073b64; column = WF 0..3 (position of WF in 0xe0073c28). Only when
 * ADIE_TOP_CHIP_ID_11_8 == ADIE_TOP_CHIP_ID_11_8_MT7976C; not run on this unit.
 * X(reg, wf0, ..., wf3). [static] */
#define MT7976C_RF_EPA_COMMON_WALK(X) \
	X(ADIE_CHAIN_24C, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_254, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_255, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_256, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_24D, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_24F, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_250, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_257, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_24E, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_251, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_252, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_253, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u, 0xfbfe86e8u) \
	X(ADIE_CHAIN_013, 0x004023c5u, 0x004023c5u, 0x004023c5u, 0x004023c5u)

/* 7976C "7976dap XPA" pre-rows (V:e0050a80 -> V:e0050a30): descriptor 0xe0073e34, values
 * 0xe0073e1c, 6 rows, column 0; per WF 0..3 after that WF's 7976C RF_EPA_COMMON, then TOP
 * ADIE_TOP_71C = ADIE_TOP_71C_XPA. Not run on this unit. X(reg, value). [static] */
#define MT7976C_XPA_PRE(X) \
	X(ADIE_TX_GAIN_MODE, 0xcfefc7efu) \
	X(ADIE_TX_GAIN,      0xc3efc1efu) \
	X(ADIE_CHAIN_242,    0xc1e7c0e7u) \
	X(ADIE_CHAIN_243,    0xc0e3c0e3u) \
	X(MT7976_CHAIN_218,  0x00000060u) \
	X(ADIE_LOOP_129,     0x66f01040u)

#endif /* HW_ADIE_RFINIT_H */
