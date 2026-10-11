/* SPDX-License-Identifier: MIT */
/*
 * Calibration item 10 at a channel set: the synthesizer tune of MediaTek's channel flow (WM
 * V:e00d708a step 14: V:e00d5820 sets P[1] / P[2], then V:e0041c84(1, P, 0xa, 0) ->
 * phyCalBasicConfig item 10 V:e00410f8 -> V:e004d884), for both A-die types: MediaTek's tables
 * and the procedure constants the other headers do not hold. Used by firmware/radio/sx.c. The
 * registers and their fields are in sx.h, mt7976.h, mt7977.h, adie.h and phy.h; the retune
 * sequences there (mt7976.h "SX0 retune", mt7977.h "Retune sequence") are what this procedure
 * does, every access of it in firmware/radio/sx.c with its V: address.
 *
 * GENERATED from MediaTek's WM image (vendor__mt7996_wm_tm.elf, sha256 01e320509194333a...; V:
 * addresses) by the research tree's radio-port/chanset/sx/gen_sxchan.py: the tables are read from
 * the image as data and each expansion (79 table / key combinations) is checked against the
 * decodes' walker models (spitrace-20261009T2225Z mt7977-decode sx.py / walkers.py, mt7976-decode
 * flows76.py / walkers76.py), which reproduce MediaTek's channel sets and calibration preludes
 * record for record. Do not edit by hand.
 *
 * TABLE FORM. Pre-rows (V:e0050d3e MT7976, V:e0051536 MT7977): X(reg, value), one value for every
 * key. Walker tables (V:e0050f44 MT7976, V:e0051af2 MT7977): a fixed register sequence and, per
 * key column, a value: X(reg, column 0, column 1, ...); the column is the position of the key
 * value in the table's key list (MediaTek's linear search, V:e0050e42 etc.). SXCH_SKIP in a
 * column is MediaTek's -2: that column writes nothing for the row. The register column is
 * MediaTek's variant 0 (SX0, P[2] = 0: V:e00d5820 sets P[2] = 0 on every channel set and
 * phyCalDo keeps it 0 on this board; the variant-1 SX1 offsets are not used). Rows are in
 * MediaTek's order; every write is a whole word to the die's bank k (MT7976: the SX0 bank k 0,
 * ADIE_ANT without the die index, die 0 only; MT7977: the SX bank k 4, the RCD rows k 4..0).
 * [static][measured: the decodes regenerate every write of the stock channel sets and of the
 * 20 working T1[18] calibration preludes]
 *
 * Table sizes as the C arrays hold them (bytes, registers u16, values u32, keys u8): MT7976
 * 224, MT7977 SX80_E3 1045, SX80 1045, RCD 643, group map 18.
 */
#ifndef HW_ADIE_SXCHAN_H
#define HW_ADIE_SXCHAN_H

#include "../hw_bits.h"
#include "adie.h"
#include "mt7976.h"
#include "mt7977.h"
#include "sx.h"

/* MediaTek's "skip" value in a table column (V:e0051bae, V:e0050fee). */
#define SXCH_SKIP                       0xfffffffeu

/* ------------------------------------------------------------------------
 * The channel flow's P[1] for item 10 (V:e00d5820(P, record, 0)): with P[3] != 0, as in every
 * stock channel set, and V:e00d41e4() = 0 (WM image byte 0xe0093075 = 0), P[2] = 0 and P[1] = 0
 * when the band's RF record A-die type (phy_cal.h MT7996_WM_BAND_RF_REC_ADIE_TYPE) is 7, the
 * MT7977 (V:e00d5870), else 3 (V:e00d583a). phyCalDo passes the same values to its prelude
 * (V:e0040b6e..V:e0040bea). On the MT7976 P[1] is the last chain bank k 1..P[1] given SX0's LO;
 * on the MT7977 the first chain of the per-chain lock steps k 3 down to P[1]. Both store it in
 * ADIE_FREQ_NOTE_* of k 1 and MT7996_PHY_SX0_PARAM. [static]
 * ------------------------------------------------------------------------ */
#define SXCH_P1_MT7976                  3u
#define SXCH_P1_MT7977                  0u

/* ------------------------------------------------------------------------
 * The frequency words, both dies (V:e005e93a..V:e005eb24 MT7976, V:e004b800..V:e004b972 MT7977),
 * 32-bit integer arithmetic, no 64-bit division. [static][measured: every stock word regenerated]
 *   MHz = kHz / 1000. If the kHz remainder is >= 500 ((kHz + 500) / 1000 == MHz + 1) the unit is
 *   500 kHz: F = kHz / 500, else F = MHz. pf = f_PFD / unit (MT7976 80 / 160, MT7977 160 / 320).
 *   With LO = VCO * num / den: INT = den F / (num pf); a = (num pf) >> d (d: the SDM split, 0 here);
 *   FRAC = ((((den F) mod a) << pre) + a / 2) / a << (24 - pre), masked to 24 bits, pre = 23
 *   (MT7976; MT7977 when f_PFD <= 159,999,999 Hz) or 22 (MT7977 at 160 MHz), 2 less in 500 kHz
 *   units (the split keeps the shifted remainder below 2^32); SUB = (((den F) mod (num pf)) / a
 *   << 3) / 2^d; VT = SXCH_VT(MHz, num, den) (MHz, not F, in both units).
 *   INT word = INT << 20 | SUB << 16 | VT (ADIE_SX_INT_*), FRAC word = FRAC | ADIE_SX_FRAC_LOAD.
 * ------------------------------------------------------------------------ */
#define SXCH_KHZ_PER_MHZ                1000u
#define SXCH_KHZ_HALF_MHZ               500u
#define SXCH_FRAC_BITS                  24u	/* ADIE_SX_FRAC_F */
#define SXCH_FRAC_PRE                   23u
#define SXCH_FRAC_PRE_FAST_PFD          22u	/* MT7977, f_PFD > SXCH_PFD_SPLIT_HZ */
#define SXCH_PFD_SPLIT_HZ               159999999u	/* V:e004b87a */
#define SXCH_FRAC_PRE_HALF              2u	/* 500 kHz units: pre - 2 (V:e004b892, V:e005e95c) */
#define SXCH_SUB_BITS                   3u	/* ADIE_SX_INT_SUB */
/* VT, the VCO target in 1/3 MHz: floor(3 MHz den / num + 1.5) in MediaTek's integer form. */
#define SXCH_VT(mhz, num, den) \
	(((den) * (mhz) * 30u + (num) * 10u + (((num) * 10u) >> 1)) / ((num) * 10u))

/* ------------------------------------------------------------------------
 * The done poll V:e00444e0(ADIE_SX_DONE, ADIE_SX_DONE_FLAG, timeout ms, k, 1): deadline = now +
 * timeout, 10 us (ADIE_SX_SETTLE_US), then reads until SXCH_POLL_MATCHES reads have matched
 * (mismatches between them do not reset the count), 1 us after each match; at the deadline one
 * more read unless the last one matched, and a mismatch there is MediaTek's poll failure
 * (recorded in WM RAM 0xe0081650). Timeout 0 (the MT7977 SAR steps) is therefore exactly one
 * read. The lock steps use 1 ms (ADIE_SX_LOCK_TIMEOUT_US). [static][measured]
 * ------------------------------------------------------------------------ */
#define SXCH_POLL_MATCHES               3u
#define SXCH_POLL_MATCH_US              1u
#define SXCH_SAR_POLL_TIMEOUT_US        0u	/* V:e004bae0 */

/* ========================================================================
 * MT7976 (die 0): V:e005ee1e(P, 1, 0) -> tables V:e0051206 (A) / V:e0051264 (B), then
 * V:e005e902(P, ratio, 0) with the per-chain step V:e005e896(k). [static]
 * ======================================================================== */

/* Item 10 passes ratio 1 (5/16, MT7976_RATIO_5_16_*; 0 = 3/8) for group 0; the table selector
 * sel is 0 (SX0 in k 0; 4 = the 5 GHz SX in k 4 for an A-die index 1, or a group != 0 with
 * P[2] = 1, V:e005ee84 / V:e005ef80: not this board). Key value (V:e0050e42 / V:e0050e70) =
 * (sel == 4 ? 1 : sel) + 2 ratio. */
#define MT7976_SXCH_RATIO_ITEM10        1u
#define MT7976_SXCH_SEL_SX0             0u
#define MT7976_SXCH_KEY(sel1, ratio)    ((sel1) + 2u * (ratio))
/* V:e005e902, P[2] = 0, group 0: P[1] >= 2 first writes k 4's MT7976_LO_EN [31:28] =
 * MT7976_LO_EN_SX_INACTIVE (V:e005ec3a); the chains k 1..min(P[1], 4) get MT7976_LO_SRC_SX0 and
 * the per-chain lock step. */
#define MT7976_SXCH_P1_K4_INACTIVE      2u
#define MT7976_SXCH_P1_MAX              4u

/* Table A pre-rows (MediaTek WM 0xe0074960, values 0xe00748fc, 9 rows), every key. */
#define MT7976_SXCH_PRE(X) \
	X(ADIE_SX_B88,      0x14051a67u) \
	X(ADIE_SX_A51,      0x00060300u) \
	X(ADIE_SX_A68,      0xffffffffu) \
	X(ADIE_SX_A69,      0xffffffffu) \
	X(ADIE_SX_A6A,      0xffffffffu) \
	X(ADIE_SX_A6B,      0x0000ffffu) \
	X(ADIE_SX_A48,      0x02030000u) \
	X(ADIE_SX_A57,      0x00000000u) \
	X(MT7976_SX_SPLIT,  0xf00113a4u)

/* Table A (V:e0051206: descriptors 0xe00748ac, offsets 0xe00748dc, values 0xe00748fc, column map
 * 0xe00748d0, 3 groups) and table B (V:e0051264: 0xe0081384, 0xe0081398, 0xe008139c, 0xe0081390).
 * Key lists 0xe00813ac (A) and 0xe0081394 (B), key value MT7976_SXCH_KEY: columns 0: SX0 3/8,
 * 1: SX0 5/16, 2: 5 GHz SX 3/4, 3: 5 GHz SX 5/8 (mt7976.h MT7976_SX_TABLE_A). Item 10 on this
 * board selects column 1. */
#define MT7976_SXCH_A_KEYS(X) X(0u) X(2u) X(1u) X(3u)
#define MT7976_SXCH_A(X) \
	X(MT7976_SX_CFG,    0x00000011u, 0x00000015u, 0x00000011u, 0x00000015u) \
	X(ADIE_SX_A52,      0x00190103u, 0x001b0103u, 0x00190103u, 0x001b0103u) \
	X(ADIE_SX_A5B,      0x2e00030cu, 0x2e00020au, 0x2e00030cu, 0x2e00020au) \
	X(ADIE_SX_B84,      0x784a8143u, 0x784a8174u, 0x784a8143u, 0x784a8174u) \
	X(ADIE_SX_A1B,      0x3000020au, 0x2800020au, 0x3000020au, 0x2800020au) \
	X(MT7976_SX_SSB,    0x00004200u, 0x00004200u, 0x00004100u, 0x00004100u) \
	X(ADIE_SX_A08,      0x12200300u, 0x12200300u, 0x12200000u, 0x12200000u) \
	X(ADIE_SX_A5A,      0x08070103u, 0x08060103u, 0x08070202u, 0x08060202u)
#define MT7976_SXCH_B_KEYS(X) X(0u) X(2u) X(1u) X(3u)
#define MT7976_SXCH_B(X) \
	X(ADIE_SX_A4E,      0x0000a0a0u, 0x0000a0a0u, 0x0000a0a0u, 0x0000a0a0u)

/* ========================================================================
 * MT7977 (dies 1, 2): V:e004bc28(P, 1, 0) -> tables V:e00522a6 (SX80_E3) or V:e0052264 (SX80), RCD
 * V:e0051f1a per bank, then V:e004b800(P, ratio, 0, 0) with the per-chain step V:e004b7a4(k).
 * [static]
 * ======================================================================== */

/* f_PFD = SXCH_PFD_MULT x P[0x10]: V:e004bc4a stores P[0x10] = 80 MHz, the multiplier is 2 for
 * both ratios with or without a group (stack bytes V:e004b844..V:e004b852) = MT7977_PFD_KHZ. */
#define MT7977_SXCH_REF_HZ              80000000u
#define MT7977_SXCH_PFD_MULT            2u
/* Ratio 5/8 (MT7977_RATIO_5_8_*) when the group is 0 or MT7977_FW_LO_MIN_MHZ <= MHz <
 * MT7977_FW_SPLIT_MHZ, else 3/4 (V:e004bcce..V:e004bcdc); below MT7977_FW_LO_MIN_MHZ the tune
 * returns after the ADIE_STROBE_SX_PREP pulse (V:e004bcc2). */
/* Table selection: SX80_E3 when the ECO byte (radio.h radio_eco()) > 9, else SX80 (V:e004bd26). */
#define MT7977_SXCH_ECO_E3_MIN          10u
/* Group remap before the table, in MediaTek's test order (V:e004bcea..V:e004bdd2):
 * X(group, lo_mhz, hi_mhz); MHz in [lo, hi] replaces the group. */
#define MT7977_SXCH_REMAP(X) \
	X(9u, 5926u, 6024u) \
	X(10u, 6025u, 6105u) \
	X(8u, 5816u, 5925u)
/* Key value (V:e00519bc / V:e00518f6) = group' + 0x18 ratio_5_8, group' = the group if <= 0x17,
 * else MT7977_SXCH_GROUP_MAP[group - 0x18] for 0x18..0x29 (the BW160 / BW320 groups of the
 * calibrations; above that MediaTek logs and uses group 1); a key outside the list asserts. */
#define MT7977_SXCH_GROUP_DIRECT_MAX    0x17u
#define MT7977_SXCH_GROUP_MAP_FIRST     0x18u
#define MT7977_SXCH_GROUP_MAP_LAST      0x29u
#define MT7977_SXCH_KEY(group, ratio58) ((group) + 0x18u * (ratio58))
/* Group map (MediaTek WM 0xe0069a10, 18 bytes): base group of g' 0x18..0x29. */
#define MT7977_SXCH_GROUP_MAP(X) \
	X(1u) X(3u) X(4u) X(7u) X(10u) X(11u) X(13u) X(15u) X(17u) X(19u) \
	X(21u) X(10u) X(12u) X(14u) X(16u) X(18u) X(20u) X(5u)

/* SX80_E3 (V:e00522a6: descriptors 0xe0076198, offsets 0xe00762ec, values 0xe0076314, column map
 * 0xe0076220, 7 groups; key list 0xe00762d0, 25 entries) and SX80 (V:e0052264: 0xe00763a4,
 * 0xe0076500, 0xe0076528, 0xe0076434; key list 0xe00764e4, the same values), written to the SX
 * bank k 4.
 * Columns: 0: group 0 at 3/4; 1..9: groups 0..8 at 5/8; 10..24: groups 9..23 at 3/4.
 * Group 0 writes nothing; ADIE_SX_BAND's [31:20] is the SAR's (sx.h). (mt7977.h
 * MT7977_SX_E3_ROWS is the SX80_E3 table by group.) */
#define MT7977_SXCH_E3_KEYS(X) \
	X(0u) X(24u) X(25u) X(26u) X(27u) X(28u) X(29u) X(30u) X(31u) X(32u) \
	X(9u) X(10u) X(11u) X(12u) X(13u) X(14u) X(15u) X(16u) X(17u) X(18u) \
	X(19u) X(20u) X(21u) X(22u) X(23u)
#define MT7977_SXCH_E3(X) \
	X(ADIE_SX_BAND, \
	  SXCH_SKIP, SXCH_SKIP, 0x20010cffu, 0x20010ceeu, 0x20010cccu, \
	  0x20010caau, 0x20010caau, 0x20010c99u, 0x20010c88u, 0x20010c66u, \
	  0x20010cffu, 0x20010cffu, 0x20010cffu, 0x20010cffu, 0x20010ceeu, \
	  0x20010ceeu, 0x20010cddu, 0x20010cccu, 0x20010cbbu, 0x20010caau, \
	  0x20010c99u, 0x20010c88u, 0x20010c77u, 0x20010c66u, 0x20010c55u) \
	X(ADIE_SX_A51, \
	  SXCH_SKIP, SXCH_SKIP, 0x04060303u, 0x04060303u, 0x04060303u, \
	  0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, \
	  0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, \
	  0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, \
	  0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u) \
	X(ADIE_SX_A5B, \
	  SXCH_SKIP, SXCH_SKIP, 0x0a00020au, 0x0a00020au, 0x0a00020au, \
	  0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, \
	  0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, \
	  0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, \
	  0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au) \
	X(ADIE_SX_A64, \
	  SXCH_SKIP, SXCH_SKIP, 0x00448424u, 0x00448424u, 0x00448424u, \
	  0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, \
	  0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, \
	  0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, \
	  0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u) \
	X(ADIE_SX_A48, \
	  SXCH_SKIP, SXCH_SKIP, 0x06070800u, 0x06070800u, 0x06070800u, \
	  0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, \
	  0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, \
	  0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, \
	  0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u) \
	X(ADIE_SX_A52, \
	  SXCH_SKIP, SXCH_SKIP, 0x01070403u, 0x01060403u, 0x01060403u, \
	  0x01060403u, 0x01060403u, 0x01050403u, 0x01050403u, 0x01050403u, \
	  0x01060403u, 0x01060403u, 0x01060403u, 0x01060403u, 0x01060403u, \
	  0x01050403u, 0x01050403u, 0x01050403u, 0x01040403u, 0x01040403u, \
	  0x01040403u, 0x01040403u, 0x01040403u, 0x01040403u, 0x01040403u) \
	X(ADIE_SX_A5A, \
	  SXCH_SKIP, SXCH_SKIP, 0x401a0404u, 0x401a0404u, 0x401a0404u, \
	  0x401a0404u, 0x401a0404u, 0x401a0404u, 0x401a0404u, 0x401a0404u, \
	  0x40210505u, 0x40210505u, 0x40210505u, 0x40200505u, 0x40200505u, \
	  0x40200505u, 0x40200505u, 0x40200505u, 0x40200505u, 0x40210505u, \
	  0x40200505u, 0x401e0505u, 0x401e0505u, 0x401c0505u, 0x401c0505u) \
	X(MT7977_SX_CFG, \
	  SXCH_SKIP, SXCH_SKIP, 0x00100014u, 0x00100014u, 0x00100014u, \
	  0x00100014u, 0x00100014u, 0x00100014u, 0x00100014u, 0x00100014u, \
	  0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, \
	  0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, \
	  0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u) \
	X(ADIE_SX_A44, \
	  SXCH_SKIP, SXCH_SKIP, 0x00008000u, 0x00008000u, 0x00009000u, \
	  0x0000a000u, 0x0000a000u, 0x0000b000u, 0x0000b000u, 0x0000c000u, \
	  0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u, \
	  0x00009000u, 0x00008000u, 0x00009000u, 0x0000a000u, 0x0000a000u, \
	  0x0000c000u, 0x0000c000u, 0x0000c000u, 0x0000d000u, 0x0000d000u) \
	X(ADIE_SX_A57, \
	  SXCH_SKIP, SXCH_SKIP, 0x04000000u, 0x04000000u, 0x04000000u, \
	  0x04000000u, 0x04000000u, 0x04000000u, 0x04000000u, 0x10000000u, \
	  0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, \
	  0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, \
	  0x00000000u, 0x00000000u, 0x00000000u, 0x10000000u, 0x10000000u)
#define MT7977_SXCH_SX80_KEYS(X) \
	X(0u) X(24u) X(25u) X(26u) X(27u) X(28u) X(29u) X(30u) X(31u) X(32u) \
	X(9u) X(10u) X(11u) X(12u) X(13u) X(14u) X(15u) X(16u) X(17u) X(18u) \
	X(19u) X(20u) X(21u) X(22u) X(23u)
#define MT7977_SXCH_SX80(X) \
	X(ADIE_SX_A5B, \
	  SXCH_SKIP, SXCH_SKIP, 0x0a00020au, 0x0a00020au, 0x0a00020au, \
	  0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, \
	  0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, \
	  0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, \
	  0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au, 0x0a00020au) \
	X(ADIE_SX_A51, \
	  SXCH_SKIP, SXCH_SKIP, 0x04060303u, 0x04060303u, 0x04060303u, \
	  0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, \
	  0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, \
	  0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, \
	  0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u, 0x04060303u) \
	X(ADIE_SX_A64, \
	  SXCH_SKIP, SXCH_SKIP, 0x00448424u, 0x00448424u, 0x00448424u, \
	  0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, \
	  0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, \
	  0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, \
	  0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u, 0x00448424u) \
	X(ADIE_SX_A48, \
	  SXCH_SKIP, SXCH_SKIP, 0x06070800u, 0x06070800u, 0x06070800u, \
	  0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, \
	  0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, \
	  0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, \
	  0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u, 0x06070800u) \
	X(ADIE_SX_A5A, \
	  SXCH_SKIP, SXCH_SKIP, 0x401a0404u, 0x401a0404u, 0x401a0404u, \
	  0x401a0404u, 0x401a0404u, 0x401a0404u, 0x401a0404u, 0x401a0404u, \
	  0x401c0505u, 0x401c0505u, 0x401c0505u, 0x401f0505u, 0x40200505u, \
	  0x40200505u, 0x40200505u, 0x40200505u, 0x40200505u, 0x40210505u, \
	  0x401c0505u, 0x401e0505u, 0x401e0505u, 0x401a0505u, 0x401a0505u) \
	X(ADIE_SX_A52, \
	  SXCH_SKIP, SXCH_SKIP, 0x01070403u, 0x01060403u, 0x01060403u, \
	  0x01060403u, 0x01060403u, 0x01050403u, 0x01050403u, 0x01050403u, \
	  0x01060403u, 0x01060403u, 0x01060403u, 0x01060403u, 0x01060403u, \
	  0x01050403u, 0x01050403u, 0x01050403u, 0x01040403u, 0x01040403u, \
	  0x01040403u, 0x01040403u, 0x01040403u, 0x01040403u, 0x01040403u) \
	X(ADIE_SX_BAND, \
	  SXCH_SKIP, SXCH_SKIP, 0x20010cffu, 0x20010ceeu, 0x20010cccu, \
	  0x20010cbbu, 0x20010caau, 0x20010c88u, 0x20010c77u, 0x20010c55u, \
	  0x20010cffu, 0x20010cffu, 0x20010cffu, 0x20010cffu, 0x20010ceeu, \
	  0x20010cddu, 0x20010cbbu, 0x20010cbbu, 0x20010cbbu, 0x20010c99u, \
	  0x20010c99u, 0x20010c88u, 0x20010c77u, 0x20010c66u, 0x20010c55u) \
	X(MT7977_SX_CFG, \
	  SXCH_SKIP, SXCH_SKIP, 0x00100014u, 0x00100014u, 0x00100014u, \
	  0x00100014u, 0x00100014u, 0x00100014u, 0x00100014u, 0x00100014u, \
	  0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, \
	  0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, \
	  0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u, 0x00100015u) \
	X(ADIE_SX_A44, \
	  SXCH_SKIP, SXCH_SKIP, 0x0000a000u, 0x0000a000u, 0x0000b000u, \
	  0x0000c000u, 0x0000c000u, 0x0000d000u, 0x0000d000u, 0x0000e000u, \
	  0x00008000u, 0x00009000u, 0x00009000u, 0x0000a000u, 0x0000a000u, \
	  0x0000b000u, 0x0000a000u, 0x0000b000u, 0x0000c000u, 0x0000c000u, \
	  0x0000e000u, 0x0000e000u, 0x0000e000u, 0x0000f000u, 0x0000f000u) \
	X(ADIE_SX_A57, \
	  SXCH_SKIP, SXCH_SKIP, 0x04000000u, 0x04000000u, 0x04000000u, \
	  0x04000000u, 0x04000000u, 0x04000000u, 0x04000000u, 0x10000000u, \
	  0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, \
	  0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, \
	  0x00000000u, 0x00000000u, 0x00000000u, 0x10000000u, 0x10000000u)

/* RCD (V:e0051f1a), for k = 4, 3, 2, 1, 0: the pre-row (MediaTek WM 0xe00814dc, values 0xe007aec0),
 * then the walk (descriptors 0xe007adf0, offsets 0xe007aea8, values 0xe007aec0, column map
 * 0xe007ae28, 4 groups); key value MT7977_SXCH_RCD_KEY(range, k) in list 0xe007ae8c (25
 * entries; V:e00517f6), the range from the LO MHz by V:e004b748 (mt7977.h MT7977_RCD_ROWS lo /
 * hi, anything outside: range 0).
 * Column = 5 k + range (the list holds range * 5 + k at position 5 k + range). */
#define MT7977_SXCH_RCD_KEY(range, k)   ((range) * 5u + (k))
#define MT7977_SXCH_RCD_PRE(X) \
	X(MT7977_RCD_PRE,   0x00000015u)
#define MT7977_SXCH_RCD_KEYS(X) \
	X(0u) X(5u) X(10u) X(15u) X(20u) X(1u) X(6u) X(11u) X(16u) X(21u) \
	X(2u) X(7u) X(12u) X(17u) X(22u) X(3u) X(8u) X(13u) X(18u) X(23u) \
	X(4u) X(9u) X(14u) X(19u) X(24u)
#define MT7977_SXCH_RCD(X) \
	X(MT7977_RCD_768, \
	  0x18183131u, 0x1b1b3737u, 0x18183131u, 0x1a1a3535u, 0x1c1c3838u, \
	  0x18183131u, 0x1b1b3737u, 0x18183131u, 0x1a1a3535u, 0x1c1c3838u, \
	  0x18183131u, 0x1b1b3737u, 0x18183131u, 0x1a1a3535u, 0x1c1c3838u, \
	  0x18183131u, 0x1b1b3737u, 0x18183131u, 0x1a1a3535u, 0x1c1c3838u, \
	  0x18183131u, 0x1b1b3737u, 0x18183131u, 0x1a1a3535u, 0x1c1c3838u) \
	X(MT7977_RCD_766, \
	  0x18181818u, 0x1b1b1b1bu, 0x18181818u, 0x1a1a1a1au, 0x1c1c1c1cu, \
	  0x18181818u, 0x1b1b1b1bu, 0x18181818u, 0x1a1a1a1au, 0x1c1c1c1cu, \
	  0x18181818u, 0x1b1b1b1bu, 0x18181818u, 0x1a1a1a1au, 0x1c1c1c1cu, \
	  0x18181818u, 0x1b1b1b1bu, 0x18181818u, 0x1a1a1a1au, 0x1c1c1c1cu, \
	  0x18181818u, 0x1b1b1b1bu, 0x18181818u, 0x1a1a1a1au, 0x1c1c1c1cu) \
	X(MT7977_RCD_765, \
	  0x00000018u, 0x0000001bu, 0x00000018u, 0x0000001au, 0x0000001cu, \
	  0x00000018u, 0x0000001bu, 0x00000018u, 0x0000001au, 0x0000001cu, \
	  0x00000018u, 0x0000001bu, 0x00000018u, 0x0000001au, 0x0000001cu, \
	  0x00000018u, 0x0000001bu, 0x00000018u, 0x0000001au, 0x0000001cu, \
	  0x00000018u, 0x0000001bu, 0x00000018u, 0x0000001au, 0x0000001cu) \
	X(MT7977_RCD_767, \
	  0x0000000cu, 0x0000000du, 0x0000000cu, 0x0000000du, 0x0000000eu, \
	  0x0000000cu, 0x0000000du, 0x0000000cu, 0x0000000du, 0x0000000eu, \
	  0x0000000cu, 0x0000000du, 0x0000000cu, 0x0000000du, 0x0000000eu, \
	  0x0000000cu, 0x0000000du, 0x0000000cu, 0x0000000du, 0x0000000eu, \
	  0x0000000cu, 0x0000000du, 0x0000000cu, 0x0000000du, 0x0000000eu) \
	X(MT7977_RCD_744, \
	  0x00200004u, 0x00200004u, 0x00200004u, 0x00200061u, 0x00200061u, \
	  0x00200004u, 0x00200004u, 0x00200004u, 0x00200061u, 0x00200061u, \
	  0x00200004u, 0x00200004u, 0x00200004u, 0x00200061u, 0x00200061u, \
	  0x00200004u, 0x00200004u, 0x00200004u, 0x00200061u, 0x00200061u, \
	  0x00200004u, 0x00200004u, 0x00200004u, 0x00200061u, 0x00200061u) \
	X(MT7977_RCD_746, \
	  0x00001818u, 0x00001818u, 0x00000c0cu, 0x00000d0du, 0x00000e0eu, \
	  0x00001818u, 0x00001818u, 0x00000c0cu, 0x00000d0du, 0x00000e0eu, \
	  0x00001818u, 0x00001818u, 0x00000c0cu, 0x00000d0du, 0x00000e0eu, \
	  0x00001818u, 0x00001818u, 0x00000c0cu, 0x00000d0du, 0x00000e0eu, \
	  0x00001818u, 0x00001818u, 0x00000c0cu, 0x00000d0du, 0x00000e0eu)

/* The per-chain lock steps run on k MT7977_SXCH_WF_LOCK_FIRST down to P[1] (V:e004bbe8; k 4 is
 * the SX bank itself, locked before). */
#define MT7977_SXCH_WF_LOCK_FIRST       3u

#endif /* HW_ADIE_SXCHAN_H */
