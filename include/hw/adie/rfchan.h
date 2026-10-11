/* SPDX-License-Identifier: MIT */
/*
 * MT7977 per-channel A-die RF tables of the channel set: calibration item 1
 * (phyCalBasicConfig V:e0040dde, item path V:e0040fe2), run per WF by
 * MediaTek's channel flow V:e00d708a on the MT7977 bands (1, 2), with
 * their constants. The registers are named in adie.h (the chain-bank offset
 * names ADIE_CHAIN_<index> of unknown role), sx.h, rfmodes.h and mt7977.h.
 * Generated from MediaTek's WM image by the chanset port's gen.py (research
 * tree, radio-port/chanset/); do not edit by hand.
 *
 * ADDRESS SPACE: 12-bit A-die register indices in the chain banks k 0..4 of
 * the band's die (adie.h); the D-die key records are WF_COMM words (phy.h).
 *
 * Item 1, per WF (P[1]), in MediaTek's order (V:e0040fe2; mask bit 0, AdieSel
 * 7 only):
 *  1. RF COMMON: with EPA (P[6]) RFCHAN_EPA_COMMON (V:e0051ff8), else
 *     RFCHAN_NOEPA_COMMON (V:e0051ce0). Each reads MT7996_PHY_RFIF_D414
 *     twice and, unless [27:16] == 0xbb1 with [0], writes the WF's key record
 *     (phy.h MT7996_PHY_RF_TBL_KEY: the column key + nibble-swapped (key + 1)
 *     << 8 into [15:0], or [31:16] when P[2] != 0, of the A-die index's
 *     WF_COMM; read-modify-write) and walks the table. The EPA path then
 *     writes ADIE_CHAIN_637 / _638 = RFCHAN_637_EPA / RFCHAN_638_EPA on k
 *     0..4 (even when the walk was skipped), and, when the ECO byte > 9 and
 *     EEPROM 0x1a4 [7] is set, the extra words of RFCHAN_EXTRA by group.
 *  2. At BW160 / BW320 (P[4] 3 / 4): COMMON_BW, D414 read twice and the same
 *     skip test, then with EPA the 8 pre-rows RFCHAN_EPA_BW_PRE (column P[2])
 *     and RFCHAN_EPA_BW (V:e0051de2), else RFCHAN_NOEPA_BW (V:e0051e6a);
 *     key: the BW group of the centre frequency (RFCHAN_BWGRP_*).
 *  3. RFCHAN_XLNA (key P[7]), RFCHAN_XPA (key P[6]), no D414 read.
 *  4. For groups != 0: the group table by temperature class (EPA
 *     RFCHAN_EPA_GROUP, else RFCHAN_NOEPA_GROUP).
 * Walker (V:e0051af2): rows in order, row i's register is the second pair
 * entry when P[2] = 1 (pair tables; the first otherwise) or the only one;
 * the value is the row's column of its group; RFCHAN_SKIP writes nothing. Bank: the WF (k = WF), or k 0..4
 * each when WF = 5. MediaTek ORs WM byte 0xe01ec1d9 into the bank number: no
 * writer of it was found and it is 0 in every stock load [static][measured];
 * it is taken as 0. A key value missing from its list asserts in the WM: the
 * channel set refuses such a channel before writing anything.
 *
 * Confidence tags: [measured], [static], [inferred].
 */
#ifndef HW_ADIE_RFCHAN_H
#define HW_ADIE_RFCHAN_H

#include "../hw_bits.h"
#include "adie.h"
#include "mt7977.h"
#include "rfmodes.h"

/* Walker value -2: the row writes nothing (V:e0051af2, V:e0051536). [static] */
#define RFCHAN_SKIP                     0xfffffffeu

/* Item 1 runs only on an MT7977 band (AdieSel 7) and with calibration mask
 * bit 0 (stock); WF 5 in a walk means every bank k 0..4. [static] */
#define RFCHAN_WF_ALL                   5u
#define RFCHAN_NBANKS                   5u	/* k 0..4 */

/* Key record field per P[2] (V:e0052064..V:e00520be): 16 bits at 0 or 16;
 * value = key + (nibble-swapped low byte of key + 1) << 8. [static] */
#define RFCHAN_KEYREC_SHIFT_SX1         16u
#define RFCHAN_KEYREC_BYTE              HW_GENMASK(7, 0)
#define RFCHAN_KEYREC_NIB_LO            HW_GENMASK(3, 0)
#define RFCHAN_KEYREC_NIB_HI            HW_GENMASK(7, 4)
#define RFCHAN_KEYREC_HI_SHIFT          8u
#define RFCHAN_KEY_ECO_LT10             0x78u	/* EPA COMMON key: + (ECO < 10) * 0x78 */
#define RFCHAN_ECO_NEW                  10u	/* ECO byte >= 10: the current tables */
#define RFCHAN_KEY_GRP_STRIDE           5u	/* key = group * 5 + WF + ... */
#define RFCHAN_KEY_EPA_BW320            0x5au	/* EPA COMMON_BW key: + (BW320) * 0x5a */
#define RFCHAN_KEY_NOEPA_BW320          0x55u	/* no-EPA COMMON_BW key: + (BW320) * 0x55 */
#define RFCHAN_KEY_TEMP                 0x78u	/* group table key: + T * 0x78 */

/* After the EPA RF COMMON walk (V:e00520f6..V:e0052136), on banks k 0..4,
 * even when the D414 test skipped the walk: 0x637 = 0x7ff6, 0x638 =
 * 0x2d6d2. Role unknown. [static][measured] */
#define RFCHAN_637_EPA                  0x00007ff6u
#define RFCHAN_638_EPA                  0x0002d6d2u

/* Then, only when the ECO byte > 9 and WM byte 0xe01ec17e (= EEPROM 0x1a4
 * [7], V:e00e15a0) is 1, extra words by group (V:e005213a..V:e0052262).
 * Not taken on this unit (EEPROM 0x1a4 = 0x04; phy.h
 * MT7996_PHY_EE_1A4_B7). X(first group, last group,
 * bank k, register, value), in MediaTek's order. [static] */
#define RFCHAN_EXTRA(X) \
	X( 1u,  1u, 0u, ADIE_CHAIN_36A, 0x0026d6b8u) \
	X( 1u,  1u, 1u, ADIE_CHAIN_360, 0x0d249b6cu) \
	X( 1u,  1u, 1u, ADIE_CHAIN_36A, 0x001ee740u) \
	X( 1u,  1u, 2u, ADIE_CHAIN_36A, 0x0026d6bcu) \
	X( 1u,  1u, 3u, ADIE_CHAIN_36A, 0x001cd9b8u) \
	X( 9u,  9u, 0u, ADIE_CHAIN_36A, 0x001cd9bcu) \
	X( 9u,  9u, 1u, ADIE_CHAIN_36A, 0x001ee73cu) \
	X(20u, 22u, 0u, ADIE_CHAIN_36A, 0x001cd9c0u) \
	X(20u, 22u, 1u, ADIE_CHAIN_36A, 0x001ee740u) \
	X(20u, 22u, 3u, ADIE_CHAIN_36A, 0x001cd6b8u)

/* COMMON_BW's BW group of the centre frequency (V:e0043328 EPA, V:e00435f0
 * no EPA; code immediates): from the integer MHz, group g when lo <= MHz <=
 * lo + span; none -> group 0 with BW160's selector. X(group, lo MHz, span).
 * [static] */
#define RFCHAN_BWGRP_160(X) \
	X( 0u, 4850u, 0x1e0u) \
	X( 1u, 5331u, 0x09fu) \
	X( 2u, 5491u, 0x0f4u) \
	X( 3u, 5736u, 0x0bdu) \
	X( 4u, 5926u, 0x0b3u) \
	X( 5u, 6106u, 0x09fu) \
	X( 6u, 6266u, 0x09fu) \
	X( 7u, 6426u, 0x09fu) \
	X( 8u, 6586u, 0x09fu) \
	X( 9u, 6746u, 0x09fu) \
	X(10u, 6906u, 0x0dbu)
/* EPA boards (V:e0043420..): 5650 MHz exactly is group 0xb; then: */
#define RFCHAN_BWGRP_320_EPA_5650_MHZ   5650u
#define RFCHAN_BWGRP_320_EPA_5650       0x0bu
#define RFCHAN_BWGRP_320_EPA(X) \
	X(0x0cu, 5926u, 0x103u) \
	X(0x0du, 6186u, 0x09fu) \
	X(0x0eu, 6346u, 0x09fu) \
	X(0x0fu, 6506u, 0x09fu) \
	X(0x10u, 6666u, 0x09fu) \
	X(0x11u, 6826u, 0x12bu)
/* Boards without EPA (V:e00436e8..): */
#define RFCHAN_BWGRP_320_NOEPA(X) \
	X(0x0bu, 5926u, 0x103u) \
	X(0x0cu, 6186u, 0x09fu) \
	X(0x0du, 6346u, 0x09fu) \
	X(0x0eu, 6506u, 0x09fu) \
	X(0x0fu, 6666u, 0x09fu) \
	X(0x10u, 6826u, 0x12bu)

/* Group table temperature selector T from P[8] (V:e0042780 / V:e0042752):
 * P[8] = 1 -> 2, P[8] = 2 -> 1, else 0. [static] */
#define RFCHAN_TEMP_HOT                 1u	/* P[8] */
#define RFCHAN_TEMP_COLD                2u
#define RFCHAN_TSEL_HOT                 2u
#define RFCHAN_TSEL_COLD                1u

/* The D-die RX setup's A-die words (V:e00ddd0e -> V:e00dcd88). */

/* TOP 0x4b0 / 0x4b4 (adie.h ADIE_TOP_CHSET0 / 1) as V:e00dd2ee / V:e00dd306 write them on
 * the MT7976 (AdieSel 6). The other values (3 / 0x1c, V:e00dd21c / V:e00dd234) belong to the
 * path taken when the RX setup's argument [0xe] is 1 (V:e00d41e4: WM configuration byte
 * 0xe0093075 [2:0] == 7, not this board); not ported. [static][measured] */
#define RFCHAN_RXSET_TOP_CHSET_MT7976     0x1fu

/* 0x36a [name guess], chains k 0..3 of the band's die: V:e00de40a..V:e00de5f4 writes it whole
 * at BW320 on centre channel 95 (6 GHz, 6,425 MHz) when EEPROM 0x1a4 [7] (WM byte 0xe01ec154)
 * is set, after everything else, one column per primary half: _LO when the record's [0x10]
 * (primary 20 MHz index, MT7996_PHY_BN_MODE_PRI) is <= 7, else _HI; entry = bank k. Not in
 * the stock channel sets (this EEPROM has [7] = 0). Role unknown. [static] */
#define RFCHAN_RXSET_36A_CENTRE           95u
#define RFCHAN_RXSET_36A_PRI_LO_MAX       7u
#define RFCHAN_RXSET_36A_LO               { 0x001cd9bcu, 0x001ee73cu, 0x0026d6bcu, 0x0026d9b8u }
#define RFCHAN_RXSET_36A_HI               { 0x001cd9c0u, 0x001ee740u, 0x001ed9c0u, 0x001cd6b8u }

/* Item 1 RF COMMON table, EPA boards (V:e0051ff8).
 * MediaTek WM tables (V:e0051ff8): descriptors 0xe00765c8, register indices 0xe007a2b8, values 0xe007a4a4, column
 * map 0xe0076f28 (u8 [key][group]), keys 0xe007a1c8 (u8, 240). Key value = group * 5 + WF + (ECO byte < 10) * 0x78.
 * Rows in walk order: X(register for P[2] != 1, register for P[2] = 1, then one value per column of the row's group); the column of group g
 * is COLMAP[key][g], key = the position of the key value in KEYS. RFCHAN_SKIP = no write. [static] */
#define RFCHAN_EPA_COMMON_NGROUPS 54u
#define RFCHAN_EPA_COMMON_NKEYS 240u
#define RFCHAN_EPA_COMMON_GROUPS(X) \
	X(4u, 3u) \
	X(1u, 5u) \
	X(1u, 6u) \
	X(1u, 5u) \
	X(12u, 2u) \
	X(1u, 6u) \
	X(1u, 9u) \
	X(1u, 3u) \
	X(4u, 6u) \
	X(1u, 7u) \
	X(1u, 10u) \
	X(1u, 8u) \
	X(2u, 3u) \
	X(2u, 4u) \
	X(1u, 6u) \
	X(4u, 6u) \
	X(1u, 18u) \
	X(1u, 19u) \
	X(1u, 19u) \
	X(1u, 12u) \
	X(1u, 6u) \
	X(1u, 4u) \
	X(1u, 12u) \
	X(1u, 33u) \
	X(1u, 4u) \
	X(2u, 5u) \
	X(1u, 15u) \
	X(1u, 14u) \
	X(1u, 5u) \
	X(1u, 4u) \
	X(1u, 5u) \
	X(1u, 3u) \
	X(1u, 3u) \
	X(1u, 11u) \
	X(5u, 7u) \
	X(5u, 17u) \
	X(5u, 15u) \
	X(5u, 7u) \
	X(5u, 5u) \
	X(5u, 10u) \
	X(9u, 4u) \
	X(4u, 4u) \
	X(4u, 20u) \
	X(1u, 8u) \
	X(1u, 23u) \
	X(1u, 8u) \
	X(4u, 3u) \
	X(1u, 2u) \
	X(6u, 2u) \
	X(1u, 27u) \
	X(1u, 39u) \
	X(1u, 5u) \
	X(1u, 21u) \
	X(1u, 31u)
#define RFCHAN_EPA_COMMON_ROWS(X) \
	X(ADIE_CHAIN_2A7, ADIE_CHAIN_2AE, RFCHAN_SKIP, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2A8, ADIE_CHAIN_2AF, RFCHAN_SKIP, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2AC, ADIE_CHAIN_2B3, RFCHAN_SKIP, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2AD, ADIE_CHAIN_2B4, RFCHAN_SKIP, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2A9, ADIE_CHAIN_2B0, RFCHAN_SKIP, 0x5294a18cu, 0x5294a39cu, 0x5294a294u, 0x6318c9ccu) \
	X(ADIE_CHAIN_2AA, ADIE_CHAIN_2B1, RFCHAN_SKIP, 0x18e318c4u, 0x18d4a528u, 0x39e318c4u, 0x2954a528u, 0x9cd8c630u) \
	X(ADIE_CHAIN_2AB, ADIE_CHAIN_2B2, RFCHAN_SKIP, 0x5294a528u, 0x8c631528u, 0x6b5ad528u, 0x6318c630u) \
	X(ADIE_CHAIN_2BF, ADIE_CHAIN_2C6, RFCHAN_SKIP, 0x94a52948u) \
	X(ADIE_CHAIN_2C0, ADIE_CHAIN_2C7, RFCHAN_SKIP, 0x94a52948u) \
	X(ADIE_CHAIN_2C4, ADIE_CHAIN_2CB, RFCHAN_SKIP, 0x94a52948u) \
	X(ADIE_CHAIN_2C5, ADIE_CHAIN_2CC, RFCHAN_SKIP, 0x94a52948u) \
	X(ADIE_CHAIN_24A, ADIE_CHAIN_25C, RFCHAN_SKIP, 0xffffcf00u) \
	X(ADIE_CHAIN_24B, ADIE_CHAIN_25D, RFCHAN_SKIP, 0x18080400u) \
	X(ADIE_CHAIN_24C, ADIE_CHAIN_25E, RFCHAN_SKIP, 0x00807fe0u) \
	X(ADIE_CHAIN_24F, ADIE_CHAIN_261, RFCHAN_SKIP, 0x00800000u) \
	X(ADIE_CHAIN_644, ADIE_CHAIN_654, RFCHAN_SKIP, 0x00000000u) \
	X(ADIE_CHAIN_730, ADIE_CHAIN_73E, RFCHAN_SKIP, 0x0000f540u) \
	X(MT7977_RX_12C, MT7977_RX_12C, RFCHAN_SKIP, 0x42008000u) \
	X(ADIE_CHAIN_731, ADIE_CHAIN_73F, RFCHAN_SKIP, 0xa0502200u) \
	X(ADIE_CHAIN_2C1, ADIE_CHAIN_2C8, RFCHAN_SKIP, 0x94a52840u, 0x94a52948u, 0x94a52630u, 0x94a52738u, 0x94a52528u) \
	X(ADIE_CHAIN_2C2, ADIE_CHAIN_2C9, RFCHAN_SKIP, 0x842318c4u, 0x841ce738u, 0x84252948u, 0x84210840u, 0x94a52948u, 0x63252948u, 0x73a52948u, 0x52a52948u) \
	X(ADIE_CHAIN_2C3, ADIE_CHAIN_2CA, RFCHAN_SKIP, 0x84210948u, 0x94a52948u) \
	X(ADIE_CHAIN_2E9, ADIE_CHAIN_2EB, RFCHAN_SKIP, 0xffffffffu, 0xeeeeeeeeu, 0xccccccccu, 0x66666666u, 0x00000000u) \
	X(ADIE_CHAIN_2ED, ADIE_CHAIN_2EF, RFCHAN_SKIP, 0xffffffffu, 0xeeeeeeeeu, 0xccccccccu, 0x66666666u, 0x00000000u) \
	X(ADIE_CHAIN_2EE, ADIE_CHAIN_2F0, RFCHAN_SKIP, 0xffffffffu, 0xeeeeeeeeu, 0xccccccccu, 0x66666666u, 0x00000000u) \
	X(ADIE_CHAIN_2F9, ADIE_CHAIN_2FC, RFCHAN_SKIP, 0x842108c8u, 0x84210840u, 0x5294a528u, 0x294a5294u, 0x00000000u) \
	X(ADIE_CHAIN_2EA, ADIE_CHAIN_2EC, RFCHAN_SKIP, 0xffffffffu, 0xeeeeeeeeu, 0xccccccccu, 0x66666666u, 0x20000000u, 0x00000000u) \
	X(ADIE_CHAIN_2FA, ADIE_CHAIN_2FD, RFCHAN_SKIP, 0x842108a8u, 0x842108c8u, 0x5294a5b8u, 0x294a5520u, 0x00000190u, 0x00000084u, 0x00004210u, 0x0000518cu, 0x00005294u) \
	X(ADIE_CHAIN_2FB, ADIE_CHAIN_2FE, RFCHAN_SKIP, 0x9ce94000u, 0x739ef528u, 0x4a94a294u, 0x294c6000u, 0x10842000u, 0x18c63000u, 0x294a5000u) \
	X(ADIE_CHAIN_29E, ADIE_CHAIN_29E, RFCHAN_SKIP, 0x68888888u, 0x78888888u) \
	X(ADIE_CHAIN_29F, ADIE_CHAIN_29F, RFCHAN_SKIP, 0x68888888u, 0x78888888u) \
	X(ADIE_CHAIN_2FF, ADIE_CHAIN_302, RFCHAN_SKIP, 0x7a084210u, 0x00000000u, 0x7bd0c420u) \
	X(ADIE_CHAIN_301, ADIE_CHAIN_304, RFCHAN_SKIP, 0x21084000u, 0x00000000u, 0x42108000u) \
	X(ADIE_CHAIN_300, ADIE_CHAIN_303, RFCHAN_SKIP, 0x21108210u, 0x211e8210u, 0x00000000u, 0x421ef420u, 0x421e8420u) \
	X(ADIE_CHAIN_26E, MT7976_TXTRIM_SRC(6u), RFCHAN_SKIP, 0x03807000u, 0x0700e000u, 0x0780f000u, 0x0f01e000u, 0x0f81f000u) \
	X(ADIE_CHAIN_26F, ADIE_CHAIN_287, RFCHAN_SKIP, 0x03807000u, 0x0700e000u, 0x0780f000u, 0x0f01e000u, 0x0f81f000u) \
	X(ADIE_CHAIN_270, ADIE_CHAIN_288, RFCHAN_SKIP, 0x03803000u, 0x07003800u, 0x07806000u, 0x0f006800u, 0x0f807000u) \
	X(ADIE_CHAIN_271, ADIE_CHAIN_289, RFCHAN_SKIP, 0x01801000u, 0x01c01400u, 0x03001800u, 0x03401c00u, 0x03803000u) \
	X(ADIE_CHAIN_272, ADIE_CHAIN_28A, RFCHAN_SKIP, 0x03206400u, 0x03406800u, 0x03806c00u, 0x07007400u, 0x07007000u, 0x07807400u, 0x03406400u, 0x0780e800u, 0x07a0ec00u, 0x07e0e400u, 0x0700c800u, 0x0700cc00u, 0x0740e400u, 0x0f41e400u, 0x1f81f400u, 0x1fc1f800u, 0x1fc3ec00u) \
	X(ADIE_CHAIN_273, ADIE_CHAIN_28B, RFCHAN_SKIP, 0x02204000u, 0x02404000u, 0x02604800u, 0x02804800u, 0x02805400u, 0x03404c00u, 0x0720c400u, 0x0740c800u, 0x06006c00u, 0x06207000u, 0x0640c400u, 0x0700c800u, 0x0f61c000u, 0x1f21e000u, 0x1f41e800u, 0x1e63cc00u, 0x1f03cc00u, 0x1f03d000u) \
	X(ADIE_CHAIN_274, ADIE_CHAIN_28C, RFCHAN_SKIP, 0x01402800u, 0x01602800u, 0x01602c00u, 0x02a02c00u, 0x01802c00u, 0x01803000u, 0x01a03000u, 0x01e03400u, 0x06203400u, 0x03402c00u, 0x03602c00u, 0x03603000u, 0x03a03400u, 0x07604c00u, 0x0e406400u, 0x0ea06800u, 0x0fa07000u, 0x0fc07800u) \
	X(ADIE_CHAIN_275, ADIE_CHAIN_28D, RFCHAN_SKIP, 0x01400c00u, 0x01600c00u, 0x01601000u, 0x01801000u, 0x01a01000u, 0x01e01400u, 0x01e01800u, 0x03002800u, 0x03402c00u, 0x03602c00u, 0x03803000u) \
	X(ADIE_CHAIN_24D, ADIE_CHAIN_25F, RFCHAN_SKIP, 0xff9fc700u, 0xff9fc620u, 0xff9fc600u, 0xff9e0600u, 0xff9f0600u) \
	X(ADIE_CHAIN_24E, ADIE_CHAIN_260, RFCHAN_SKIP, 0x18080020u, 0x10080020u, 0x10880020u) \
	X(ADIE_CHAIN_66D, ADIE_CHAIN_63C, RFCHAN_SKIP, 0x00a390e4u, 0x00930ca3u, 0x00a390c3u, 0x00b414e4u, 0x00930ca2u, 0x00b41505u, 0x00a390c4u, 0x00b414c3u, 0x009390c3u, 0x00a390a2u, 0x009390a2u) \
	X(ADIE_CHAIN_66E, ADIE_CHAIN_6C4, RFCHAN_SKIP, 0x06000000u, 0x04000000u, 0x04100000u, 0x06000020u, 0x06008020u, 0x08108020u, 0x04000420u, 0x04100420u, 0x04200420u, 0x06000400u, 0x06100400u, 0x06100000u, 0x08100020u, 0x0a208020u, 0x0c308020u, 0x04200400u, 0x08100000u, 0x06208820u, 0x04100820u, 0x08108420u, 0x06208401u, 0x08108441u, 0x04000020u, 0x04100001u, 0x06000422u, 0x08300820u, 0x04100020u, 0x06108420u, 0x04108420u, 0x06000401u, 0x06000021u, 0x08100021u) \
	X(ADIE_CHAIN_645, ADIE_CHAIN_655, RFCHAN_SKIP, 0x00000557u, 0x00000044u, 0x00002266u) \
	X(ADIE_CHAIN_63F, ADIE_CHAIN_64F, RFCHAN_SKIP, 0x00066666u, 0x00044444u, 0x00022222u, 0x00000000u) \
	X(ADIE_CHAIN_64D, ADIE_CHAIN_64D, RFCHAN_SKIP, 0x003fffffu, 0x002aaaaau, 0x00155555u, 0x00000000u) \
	X(ADIE_CHAIN_640, ADIE_CHAIN_650, RFCHAN_SKIP, 0x00666767u, 0x00444757u, 0x00222727u, 0x00000000u, 0x00002000u, 0x00000200u, 0x00446757u, 0x00002002u, 0x00022722u, 0x00444777u, 0x00000004u, 0x00000002u, 0x00000707u, 0x00022707u) \
	X(ADIE_CHAIN_63D, ADIE_CHAIN_63D, RFCHAN_SKIP, 0xfffff000u, 0xaaaeb800u, 0xaaaaec00u, 0x55559800u, 0x00048400u, 0x00008400u, 0x55699800u, 0x00004400u, 0xfffffc00u, 0xaaaea800u, 0x00048800u, 0x00008800u, 0x5555c800u) \
	X(ADIE_CHAIN_642, ADIE_CHAIN_652, RFCHAN_SKIP, 0x002aaaaau, 0x002aaaabu, 0x002aaabau, 0x002aaabbu) \
	X(ADIE_CHAIN_647, ADIE_CHAIN_657, RFCHAN_SKIP, 0x00044444u, 0x00044445u, 0x00000000u) \
	X(ADIE_CHAIN_648, ADIE_CHAIN_658, RFCHAN_SKIP, 0x0055df9bu, 0x00dd9b97u, 0x00001313u, 0x00001111u) \
	X(MT7977_CHAIN_12B, MT7977_CHAIN_12B, RFCHAN_SKIP, 0x00000002u, 0x00000000u) \
	X(ADIE_CHAIN_71E, ADIE_CHAIN_71F, RFCHAN_SKIP, 0xa0a0a3a7u, 0x30303337u) \
	X(ADIE_CHAIN_66F, ADIE_CHAIN_6C5, RFCHAN_SKIP, 0x00b390e4u, 0x00b30ca3u, 0x00730ca3u, 0x00a390c3u, 0x00730ca2u, 0x00b30ca2u, 0x00b390c3u, 0x007390c3u, 0x007390e4u, 0x000390e4u) \
	X(ADIE_CHAIN_670, ADIE_CHAIN_6C6, RFCHAN_SKIP, 0x00050f20u, 0x00051428u, 0x000c1428u, 0x00080f20u, 0x0005101au, 0x000c0f20u) \
	X(ADIE_CHAIN_675, ADIE_CHAIN_6CB, RFCHAN_SKIP, 0x00050f20u, 0x00051428u, 0x000c1428u, 0x00080f20u, 0x0005101au, 0x000c0f20u) \
	X(ADIE_CHAIN_67A, ADIE_CHAIN_6D0, RFCHAN_SKIP, 0x00071124u, 0x0007172cu, 0x000e172cu, 0x000a1124u, 0x0007121du, 0x000e1124u) \
	X(ADIE_CHAIN_67F, ADIE_CHAIN_6D5, RFCHAN_SKIP, 0x00071124u, 0x0007172cu, 0x000e172cu, 0x000a1124u, 0x0007121du, 0x000e1124u) \
	X(ADIE_CHAIN_684, ADIE_CHAIN_6DA, RFCHAN_SKIP, 0x00071124u, 0x0007172cu, 0x000e172cu, 0x000a1124u, 0x0007121du, 0x000e1124u) \
	X(ADIE_CHAIN_671, ADIE_CHAIN_6C7, RFCHAN_SKIP, 0x0f202837u, 0x1a283037u, 0x1a28302du, 0x14282837u, 0x0f20202du, 0x1a303037u, 0x1a30302du, 0x1a303030u, 0x1428282du, 0x1a30202du, 0x14281a30u, 0x14281428u, 0x14282830u, 0x101a202du, 0x14202830u, 0x0f202028u) \
	X(ADIE_CHAIN_676, ADIE_CHAIN_6CC, RFCHAN_SKIP, 0x0f202837u, 0x1a283037u, 0x1a28302du, 0x14282837u, 0x0f20202du, 0x1a303037u, 0x1a30302du, 0x1a303030u, 0x1428282du, 0x1a30202du, 0x14281a30u, 0x14281428u, 0x14282830u, 0x101a202du, 0x14202830u, 0x0f202028u) \
	X(ADIE_CHAIN_67B, ADIE_CHAIN_6D1, RFCHAN_SKIP, 0x11242c3du, 0x1d2c343du, 0x1d2c3432u, 0x172c2c3du, 0x11242432u, 0x1d34343du, 0x1d343432u, 0x1d343434u, 0x172c2c32u, 0x1d342432u, 0x172c1d34u, 0x172c172cu, 0x172c2c34u, 0x121d2432u, 0x17242c34u, 0x1124242cu) \
	X(ADIE_CHAIN_680, ADIE_CHAIN_6D6, RFCHAN_SKIP, 0x11242c3du, 0x1d2c343du, 0x1d2c3432u, 0x172c2c3du, 0x11242432u, 0x1d34343du, 0x1d343432u, 0x1d343434u, 0x172c2c32u, 0x1d342432u, 0x172c1d34u, 0x172c172cu, 0x172c2c34u, 0x121d2432u, 0x17242c34u, 0x1124242cu) \
	X(ADIE_CHAIN_685, ADIE_CHAIN_6DB, RFCHAN_SKIP, 0x11242c3du, 0x1d2c343du, 0x1d2c3432u, 0x172c2c3du, 0x11242432u, 0x1d34343du, 0x1d343432u, 0x1d343434u, 0x172c2c32u, 0x1d342432u, 0x172c1d34u, 0x172c172cu, 0x172c2c34u, 0x121d2432u, 0x17242c34u, 0x1124242cu) \
	X(ADIE_CHAIN_672, ADIE_CHAIN_6C8, RFCHAN_SKIP, 0x37373737u, 0x37372d37u, 0x2d372d37u, 0x372d2d37u, 0x372d3737u, 0x2d302d37u, 0x37302d37u, 0x2d2d2d37u, 0x2d2d372du, 0x2d2d302du, 0x3737372du, 0x372d2d30u, 0x372d372du, 0x37372d2du) \
	X(ADIE_CHAIN_677, ADIE_CHAIN_6CD, RFCHAN_SKIP, 0x37373737u, 0x37372d37u, 0x2d372d37u, 0x372d2d37u, 0x372d3737u, 0x2d302d37u, 0x37302d37u, 0x2d2d2d37u, 0x2d2d372du, 0x2d2d302du, 0x3737372du, 0x372d2d30u, 0x372d372du, 0x37372d2du) \
	X(ADIE_CHAIN_67C, ADIE_CHAIN_6D2, RFCHAN_SKIP, 0x3d3d3d3du, 0x3d3d323du, 0x323d323du, 0x3d32323du, 0x3d323d3du, 0x3234323du, 0x3d34323du, 0x3232323du, 0x32323d32u, 0x32323432u, 0x3d3d3d32u, 0x3d323234u, 0x3d323d32u, 0x3d3d3232u) \
	X(ADIE_CHAIN_681, ADIE_CHAIN_6D7, RFCHAN_SKIP, 0x3d3d3d3du, 0x3d3d323du, 0x323d323du, 0x3d32323du, 0x3d323d3du, 0x3234323du, 0x3d34323du, 0x3232323du, 0x32323d32u, 0x32323432u, 0x3d3d3d32u, 0x3d323234u, 0x3d323d32u, 0x3d3d3232u) \
	X(ADIE_CHAIN_686, ADIE_CHAIN_6DC, RFCHAN_SKIP, 0x3d3d3d3du, 0x3d3d323du, 0x323d323du, 0x3d32323du, 0x3d323d3du, 0x3234323du, 0x3d34323du, 0x3232323du, 0x32323d32u, 0x32323432u, 0x3d3d3d32u, 0x3d323234u, 0x3d323d32u, 0x3d3d3232u) \
	X(ADIE_CHAIN_673, ADIE_CHAIN_6C9, RFCHAN_SKIP, 0x00050f20u, 0x00051428u, 0x000f1428u, 0x00080f20u, 0x000f0f20u, 0x00370f20u) \
	X(ADIE_CHAIN_678, ADIE_CHAIN_6CE, RFCHAN_SKIP, 0x00050f20u, 0x00051428u, 0x000f1428u, 0x00080f20u, 0x000f0f20u, 0x00370f20u) \
	X(ADIE_CHAIN_67D, ADIE_CHAIN_6D3, RFCHAN_SKIP, 0x00071124u, 0x0007172cu, 0x0011172cu, 0x000a1124u, 0x00111124u, 0x003d1124u) \
	X(ADIE_CHAIN_682, ADIE_CHAIN_6D8, RFCHAN_SKIP, 0x00071124u, 0x0007172cu, 0x0011172cu, 0x000a1124u, 0x00111124u, 0x003d1124u) \
	X(ADIE_CHAIN_687, ADIE_CHAIN_6DD, RFCHAN_SKIP, 0x00071124u, 0x0007172cu, 0x0011172cu, 0x000a1124u, 0x00111124u, 0x003d1124u) \
	X(ADIE_CHAIN_674, ADIE_CHAIN_6CA, RFCHAN_SKIP, 0x0f200000u, 0x1a280000u, 0x14280000u, 0x1a300000u) \
	X(ADIE_CHAIN_679, ADIE_CHAIN_6CF, RFCHAN_SKIP, 0x0f200000u, 0x1a280000u, 0x14280000u, 0x1a300000u) \
	X(ADIE_CHAIN_67E, ADIE_CHAIN_6D4, RFCHAN_SKIP, 0x11240000u, 0x1d2c0000u, 0x172c0000u, 0x1d340000u) \
	X(ADIE_CHAIN_683, ADIE_CHAIN_6D9, RFCHAN_SKIP, 0x11240000u, 0x1d2c0000u, 0x172c0000u, 0x1d340000u) \
	X(ADIE_CHAIN_688, ADIE_CHAIN_6DE, RFCHAN_SKIP, 0x11240000u, 0x1d2c0000u, 0x172c0000u, 0x1d340000u) \
	X(ADIE_CHAIN_689, ADIE_CHAIN_6DF, RFCHAN_SKIP, 0x000556aau, 0x000156aau, 0x000555aau, 0x0005559au, 0x0005569au, 0x000116aau, 0x000156a6u, 0x000156a9u, 0x0001559au) \
	X(ADIE_CHAIN_68B, ADIE_CHAIN_6E1, RFCHAN_SKIP, 0x000556aau, 0x000156aau, 0x000555aau, 0x0005559au, 0x0005569au, 0x000116aau, 0x000156a6u, 0x000156a9u, 0x0001559au) \
	X(ADIE_CHAIN_68D, ADIE_CHAIN_6E3, RFCHAN_SKIP, 0x000556aau, 0x000156aau, 0x000555aau, 0x0005559au, 0x0005569au, 0x000116aau, 0x000156a6u, 0x000156a9u, 0x0001559au) \
	X(ADIE_CHAIN_68F, ADIE_CHAIN_6E5, RFCHAN_SKIP, 0x000556aau, 0x000156aau, 0x000555aau, 0x0005559au, 0x0005569au, 0x000116aau, 0x000156a6u, 0x000156a9u, 0x0001559au) \
	X(ADIE_CHAIN_691, ADIE_CHAIN_6E7, RFCHAN_SKIP, 0x000556aau, 0x000156aau, 0x000555aau, 0x0005559au, 0x0005569au, 0x000116aau, 0x000156a6u, 0x000156a9u, 0x0001559au) \
	X(ADIE_CHAIN_68A, ADIE_CHAIN_6E0, RFCHAN_SKIP, 0x00055000u, 0x00155000u, 0x00255000u) \
	X(ADIE_CHAIN_68C, ADIE_CHAIN_6E2, RFCHAN_SKIP, 0x00055000u, 0x00155000u, 0x00255000u) \
	X(ADIE_CHAIN_68E, ADIE_CHAIN_6E4, RFCHAN_SKIP, 0x00055000u, 0x00155000u, 0x00255000u) \
	X(ADIE_CHAIN_690, ADIE_CHAIN_6E6, RFCHAN_SKIP, 0x00055000u, 0x00155000u, 0x00255000u) \
	X(ADIE_CHAIN_692, ADIE_CHAIN_6E8, RFCHAN_SKIP, 0x00055000u, 0x00155000u, 0x00255000u) \
	X(ADIE_CHAIN_695, ADIE_CHAIN_6EB, RFCHAN_SKIP, 0x00023434u, 0x00033434u, 0x00053434u) \
	X(ADIE_CHAIN_698, ADIE_CHAIN_6EE, RFCHAN_SKIP, 0x00023434u, 0x00033434u, 0x00053434u) \
	X(ADIE_CHAIN_69B, ADIE_CHAIN_6F1, RFCHAN_SKIP, 0x00023535u, 0x00033535u, 0x00073535u) \
	X(ADIE_CHAIN_69E, ADIE_CHAIN_6F4, RFCHAN_SKIP, 0x00002424u, 0x00022424u, 0x00052424u) \
	X(ADIE_CHAIN_693, ADIE_CHAIN_6E9, RFCHAN_SKIP, 0x00023434u, 0x00023334u, 0x00023333u) \
	X(ADIE_CHAIN_696, ADIE_CHAIN_6EC, RFCHAN_SKIP, 0x00023434u, 0x00023334u, 0x00023333u) \
	X(ADIE_CHAIN_699, ADIE_CHAIN_6EF, RFCHAN_SKIP, 0x00023535u, 0x00023335u, 0x00023333u) \
	X(ADIE_CHAIN_69C, ADIE_CHAIN_6F2, RFCHAN_SKIP, 0x00002424u, 0x00000224u, 0x00000202u) \
	X(ADIE_CHAIN_694, ADIE_CHAIN_6EA, RFCHAN_SKIP, 0x00455555u, 0x00445555u, 0x00455545u, 0x00454545u, 0x00444545u, 0x00455445u, 0x00445445u, 0x00455455u, 0x00445455u, 0x00445545u, 0x00344545u, 0x00444445u, 0x00444454u, 0x00444444u, 0x00445554u, 0x00455444u, 0x00455454u, 0x00455544u, 0x00445544u) \
	X(ADIE_CHAIN_697, ADIE_CHAIN_6ED, RFCHAN_SKIP, 0x00455555u, 0x00445555u, 0x00455545u, 0x00454545u, 0x00444545u, 0x00455445u, 0x00445445u, 0x00455455u, 0x00445455u, 0x00445545u, 0x00344545u, 0x00444445u, 0x00444454u, 0x00444444u, 0x00445554u, 0x00455444u, 0x00455454u, 0x00455544u, 0x00445544u) \
	X(ADIE_CHAIN_69A, ADIE_CHAIN_6F0, RFCHAN_SKIP, 0x00577777u, 0x00557777u, 0x00577757u, 0x00575757u, 0x00555757u, 0x00577557u, 0x00557557u, 0x00577577u, 0x00557577u, 0x00557757u, 0x00355757u, 0x00555557u, 0x00555575u, 0x00555555u, 0x00557775u, 0x00577555u, 0x00577575u, 0x00577755u, 0x00557755u) \
	X(ADIE_CHAIN_69D, ADIE_CHAIN_6F3, RFCHAN_SKIP, 0x00455555u, 0x00445555u, 0x00455545u, 0x00454545u, 0x00444545u, 0x00455445u, 0x00445445u, 0x00455455u, 0x00445455u, 0x00445545u, 0x00244545u, 0x00444445u, 0x00444454u, 0x00444444u, 0x00445554u, 0x00455444u, 0x00455454u, 0x00455544u, 0x00445544u) \
	X(ADIE_CHAIN_69F, ADIE_CHAIN_6F5, RFCHAN_SKIP, 0x00002424u, 0x00002646u, 0x00022646u, 0x00002426u, 0x00022426u, 0x00002446u, 0x00022446u) \
	X(ADIE_CHAIN_6A0, ADIE_CHAIN_6F6, RFCHAN_SKIP, 0x00677777u, 0x00667777u, 0x00677767u, 0x00676767u, 0x00466767u, 0x00677667u, 0x00667667u, 0x00677677u, 0x00667677u, 0x00467767u, 0x00266767u, 0x00467777u, 0x00666667u, 0x00466667u, 0x00666676u, 0x00466666u, 0x00667776u, 0x00677666u, 0x00467667u, 0x00677676u, 0x00677766u, 0x00467766u) \
	X(ADIE_CHAIN_6A1, ADIE_CHAIN_6F7, RFCHAN_SKIP, 0x00002424u, 0x00002646u, 0x00022646u, 0x00002426u, 0x00022426u, 0x00022424u, 0x00072424u) \
	X(ADIE_CHAIN_124, ADIE_CHAIN_124, RFCHAN_SKIP, 0x1600ea14u, 0x16002a14u) \
	X(MT7977_TTG_EN, MT7977_TTG_EN, RFCHAN_SKIP, 0x09205d40u, 0x09208d40u) \
	X(ADIE_CHAIN_13E, ADIE_CHAIN_13E, RFCHAN_SKIP, 0x58fe0000u, 0x59ce0000u) \
	X(MT7977_TTG_140, MT7977_TTG_140, RFCHAN_SKIP, 0xfe80ac02u, 0xfe80ac02u) \
	X(ADIE_CHAIN_13F, ADIE_CHAIN_13F, RFCHAN_SKIP, 0xc4000044u) \
	X(ADIE_CHAIN_353, ADIE_CHAIN_353, RFCHAN_SKIP, 0x0000d0b0u) \
	X(ADIE_CHAIN_354, ADIE_CHAIN_354, RFCHAN_SKIP, 0x30880804u) \
	X(ADIE_CHAIN_355, ADIE_CHAIN_355, RFCHAN_SKIP, 0x04020000u) \
	X(ADIE_CHAIN_358, ADIE_CHAIN_35A, RFCHAN_SKIP, 0x00000000u) \
	X(ADIE_CHAIN_35C, ADIE_CHAIN_35E, RFCHAN_SKIP, 0x002318a4u) \
	X(ADIE_CHAIN_35D, ADIE_CHAIN_35F, RFCHAN_SKIP, 0x4b5af000u) \
	X(ADIE_CHAIN_356, ADIE_CHAIN_357, RFCHAN_SKIP, 0x01304000u, 0x01300000u, 0x01200000u, 0x01b00000u, 0x01d0c000u, 0x01508000u, 0x01500000u, 0x0150c000u, 0x0151c000u, 0x01510000u, 0x01b10000u, 0x0131c000u, 0x01d00000u, 0x01b18000u, 0x00b04000u, 0x00810000u, 0x00818000u, 0x00300000u, 0x0010c000u, 0x0030c000u, 0x00308000u, 0x00208000u, 0x01108000u, 0x01a1c000u, 0x01b0c000u, 0x00d10000u) \
	X(ADIE_CHAIN_359, ADIE_CHAIN_35B, RFCHAN_SKIP, 0x00000400u, 0x00000800u, 0x00000810u, 0x00000c20u, 0x00011c30u, 0x00000c00u, 0x01011c20u, 0x00012850u, 0x01012c30u, 0x01022c54u, 0x02033c64u, 0x00001400u, 0x00000c10u, 0x00010c10u, 0x00012814u, 0x00011c14u, 0x00001410u, 0x00010c30u, 0x00000820u, 0x00001408u, 0x01010c14u, 0x01021c18u, 0x02020c10u, 0x02021c20u, 0x02032850u, 0x03032c30u, 0x03072c54u, 0x03073ca0u, 0x01011414u, 0x01011408u, 0x01020c70u, 0x02072870u, 0x01031c54u, 0x00010c00u, 0x00000814u, 0x00011420u, 0x00050c20u, 0x00052830u) \
	X(ADIE_CHAIN_360, ADIE_CHAIN_361, RFCHAN_SKIP, 0x0c925b6cu, 0x0db6e490u, 0x0d249b6cu, 0x0c925248u) \
	X(ADIE_CHAIN_36A, ADIE_CHAIN_36C, RFCHAN_SKIP, 0x0026d9b8u, 0x0026d6b8u, 0x001cd9bcu, 0x001cd9c0u, 0x001cd6b8u, 0x001ee740u, 0x001ee73cu, 0x001ee7c0u, 0x0026d6ccu, 0x0026d6bcu, 0x001ed9c0u, 0x001cd73cu, 0x0026d9bcu, 0x001cd9b8u, 0x001cd738u, 0x001cd7bcu, 0x001ee7bcu, 0x001ed7bcu, 0x001ed73cu, 0x001ad738u) \
	X(ADIE_CHAIN_36B, ADIE_CHAIN_36D, RFCHAN_SKIP, 0x6cded000u, 0x6b9d3000u, 0x6b9ed000u, 0x9bded000u, 0x73dcd000u, 0x6cdad000u, 0x9ba6c000u, 0x73a6c000u, 0x9be6d000u, 0x9bdcd000u, 0x6cdac000u, 0x6ce6d000u, 0x6ba6c000u, 0x9cdcd000u, 0x73a6d000u, 0x7bdf3000u, 0x6b5cc000u, 0x6cdcd000u, 0x6b9ec000u, 0x6bdcc000u, 0x9b9ec000u, 0x73ded000u, 0x73dee000u, 0x73dce000u, 0x6b9ad000u, 0x73dae000u, 0x739ae000u, 0x739ee000u, 0x739ed000u, 0x7bdef000u)
#define RFCHAN_EPA_COMMON_KEYS { \
	0u, 5u, 10u, 15u, 20u, 25u, 30u, 35u, 40u, 45u, 50u, 55u, 60u, 65u, 70u, 75u, \
	80u, 85u, 90u, 95u, 100u, 105u, 110u, 115u, 1u, 6u, 11u, 16u, 21u, 26u, 31u, 36u, \
	41u, 46u, 51u, 56u, 61u, 66u, 71u, 76u, 81u, 86u, 91u, 96u, 101u, 106u, 111u, 116u, \
	2u, 7u, 12u, 17u, 22u, 27u, 32u, 37u, 42u, 47u, 52u, 57u, 62u, 67u, 72u, 77u, \
	82u, 87u, 92u, 97u, 102u, 107u, 112u, 117u, 3u, 8u, 13u, 18u, 23u, 28u, 33u, 38u, \
	43u, 48u, 53u, 58u, 63u, 68u, 73u, 78u, 83u, 88u, 93u, 98u, 103u, 108u, 113u, 118u, \
	4u, 9u, 14u, 19u, 24u, 29u, 34u, 39u, 44u, 49u, 54u, 59u, 64u, 69u, 74u, 79u, \
	84u, 89u, 94u, 99u, 104u, 109u, 114u, 119u, 120u, 125u, 130u, 135u, 140u, 145u, 150u, 155u, \
	160u, 165u, 170u, 175u, 180u, 185u, 190u, 195u, 200u, 205u, 210u, 215u, 220u, 225u, 230u, 235u, \
	121u, 126u, 131u, 136u, 141u, 146u, 151u, 156u, 161u, 166u, 171u, 176u, 181u, 186u, 191u, 196u, \
	201u, 206u, 211u, 216u, 221u, 226u, 231u, 236u, 122u, 127u, 132u, 137u, 142u, 147u, 152u, 157u, \
	162u, 167u, 172u, 177u, 182u, 187u, 192u, 197u, 202u, 207u, 212u, 217u, 222u, 227u, 232u, 237u, \
	123u, 128u, 133u, 138u, 143u, 148u, 153u, 158u, 163u, 168u, 173u, 178u, 183u, 188u, 193u, 198u, \
	203u, 208u, 213u, 218u, 223u, 228u, 233u, 238u, 124u, 129u, 134u, 139u, 144u, 149u, 154u, 159u, \
	164u, 169u, 174u, 179u, 184u, 189u, 194u, 199u, 204u, 209u, 214u, 219u, 224u, 229u, 234u, 239u, \
}
#define RFCHAN_EPA_COMMON_COLMAP { \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 2u, 2u, 1u, 2u, 2u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 2u, 2u, 1u, 2u, 2u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 2u, 2u, 1u, 2u, 2u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 2u, 2u, 1u, 2u, 2u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 3u, 3u, 3u, 1u, 3u, 2u, 1u, 2u, 1u, 2u, 3u, 2u, 3u, 0u, 0u, 1u, 2u, 3u, 1u, 2u, 3u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 3u, 3u, 3u, 1u, 3u, 2u, 1u, 2u, 1u, 2u, 3u, 2u, 3u, 0u, 0u, 1u, 2u, 3u, 1u, 2u, 3u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	3u, 3u, 3u, 3u, 2u, 1u, 3u, 4u, 4u, 2u, 3u, 2u, 1u, 2u, 1u, 3u, 4u, 3u, 3u, 0u, 0u, 1u, 2u, 3u, 1u, 2u, 3u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 3u, 1u, 2u, 3u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 4u, 1u, 3u, 4u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 4u, 1u, 3u, 4u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 4u, 1u, 3u, 4u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 4u, 6u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 5u, 5u, 5u, 3u, 3u, 4u, 2u, 2u, 2u, 5u, 1u, 5u, 3u, 0u, 0u, 1u, 2u, 4u, 1u, 3u, 4u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 2u, 4u, 1u, 3u, 4u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 3u, 5u, 1u, 4u, 5u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 3u, 5u, 1u, 4u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 3u, 5u, 1u, 4u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 3u, 5u, 1u, 4u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 5u, 9u, 3u, 4u, 5u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 8u, 4u, 3u, 4u, 3u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 3u, 5u, 1u, 4u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 3u, 10u, 3u, 4u, 6u, \
	6u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 4u, 5u, 1u, 3u, 1u, 1u, 1u, 8u, 4u, 8u, 4u, 0u, 0u, 1u, 4u, 5u, 1u, 3u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 3u, 10u, 3u, 4u, 6u, \
	6u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 4u, 5u, 1u, 3u, 1u, 1u, 1u, 8u, 4u, 8u, 4u, 0u, 0u, 1u, 4u, 5u, 1u, 3u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 3u, 11u, 3u, 4u, 6u, \
	6u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 9u, 5u, 1u, 3u, 1u, 1u, 1u, 9u, 4u, 9u, 4u, 0u, 0u, 1u, 4u, 5u, 1u, 3u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 3u, 12u, 3u, 4u, 4u, \
	6u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 9u, 1u, 1u, 3u, 1u, 1u, 1u, 2u, 4u, 2u, 4u, 0u, 0u, 1u, 4u, 5u, 1u, 3u, 5u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 5u, 6u, 2u, 5u, 6u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 6u, 7u, 3u, 6u, 7u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 6u, 7u, 3u, 6u, 7u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 6u, 7u, 3u, 6u, 7u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 6u, 7u, 3u, 6u, 7u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 3u, 8u, 3u, 3u, 8u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 2u, 3u, 1u, 2u, 7u, \
	2u, 4u, 2u, 2u, 1u, 1u, 3u, 3u, 3u, 1u, 3u, 2u, 1u, 2u, 1u, 2u, 3u, 2u, 3u, 0u, 0u, 1u, 3u, 8u, 3u, 3u, 8u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 5u, 6u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 3u, 3u, 10u, 3u, 3u, 2u, 1u, 2u, 1u, 5u, 3u, 5u, 3u, 0u, 0u, 1u, 3u, 8u, 3u, 3u, 8u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 5u, 6u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 3u, 3u, 10u, 3u, 3u, 2u, 1u, 2u, 1u, 5u, 3u, 5u, 3u, 0u, 0u, 1u, 3u, 8u, 3u, 3u, 8u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 5u, 13u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 3u, 3u, 10u, 2u, 3u, 2u, 1u, 2u, 1u, 10u, 3u, 10u, 3u, 0u, 0u, 1u, 7u, 9u, 3u, 7u, 9u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 14u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 11u, 3u, 3u, 4u, 3u, 2u, 1u, 11u, 4u, 5u, 3u, 0u, 0u, 1u, 7u, 9u, 3u, 7u, 9u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 14u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 11u, 3u, 3u, 4u, 3u, 2u, 1u, 11u, 4u, 5u, 3u, 0u, 0u, 1u, 7u, 9u, 3u, 7u, 9u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 3u, 15u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 12u, 3u, 3u, 4u, 3u, 2u, 1u, 11u, 4u, 11u, 3u, 0u, 0u, 1u, 7u, 9u, 3u, 7u, 9u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 7u, 9u, 3u, 7u, 9u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 7u, 10u, 3u, 6u, 5u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 7u, 10u, 3u, 6u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 7u, 10u, 3u, 6u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 7u, 10u, 3u, 6u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 5u, 16u, 3u, 4u, 8u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 8u, 5u, 3u, 4u, 3u, 2u, 1u, 9u, 3u, 9u, 3u, 0u, 0u, 1u, 7u, 10u, 3u, 6u, 5u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 3u, 4u, 3u, 4u, 9u, \
	8u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 4u, 2u, 1u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 0u, 0u, 1u, 2u, 11u, 3u, 8u, 10u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 4u, 6u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 3u, 5u, 3u, 2u, 2u, 2u, 5u, 1u, 5u, 5u, 0u, 0u, 1u, 2u, 11u, 3u, 8u, 10u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 4u, 17u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 1u, 5u, 3u, 2u, 2u, 2u, 2u, 1u, 12u, 5u, 0u, 0u, 1u, 2u, 11u, 3u, 8u, 10u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 4u, 17u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 1u, 5u, 3u, 2u, 2u, 2u, 2u, 1u, 12u, 5u, 0u, 0u, 1u, 2u, 11u, 3u, 8u, 10u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 8u, 12u, 3u, 9u, 11u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 9u, 12u, 3u, 10u, 12u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 9u, 12u, 3u, 10u, 12u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 9u, 12u, 3u, 10u, 12u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 0u, 0u, 1u, 9u, 12u, 3u, 10u, 12u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 0u, 0u, 1u, 9u, 13u, 3u, 10u, 13u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 0u, 0u, 1u, 9u, 13u, 3u, 10u, 13u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	3u, 1u, 3u, 3u, 2u, 1u, 1u, 4u, 4u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 4u, 3u, 1u, 0u, 0u, 1u, 9u, 13u, 3u, 10u, 13u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 9u, 13u, 3u, 10u, 13u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 10u, 14u, 3u, 10u, 14u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 10u, 14u, 3u, 10u, 14u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 10u, 14u, 3u, 10u, 14u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 10u, 14u, 3u, 10u, 14u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 10u, 14u, 3u, 10u, 14u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 11u, 15u, 3u, 11u, 15u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 3u, 7u, 1u, 0u, 0u, 1u, 11u, 15u, 3u, 11u, 15u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 3u, 7u, 1u, 0u, 0u, 1u, 11u, 15u, 3u, 11u, 15u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 3u, 18u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 13u, 6u, 1u, 1u, 4u, 1u, 1u, 12u, 4u, 13u, 1u, 0u, 0u, 1u, 11u, 15u, 3u, 11u, 15u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 5u, 19u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 7u, 1u, 1u, 5u, 1u, 1u, 7u, 3u, 7u, 1u, 0u, 0u, 1u, 11u, 15u, 3u, 11u, 15u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 3u, 4u, 3u, 4u, 6u, \
	8u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 4u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 4u, 3u, 1u, 0u, 0u, 1u, 2u, 16u, 4u, 12u, 16u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 3u, 1u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 4u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 1u, 1u, 0u, 0u, 1u, 2u, 16u, 4u, 12u, 16u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 4u, 17u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 1u, 5u, 3u, 2u, 2u, 2u, 2u, 1u, 12u, 5u, 0u, 0u, 1u, 2u, 16u, 4u, 12u, 16u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 4u, 17u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 1u, 5u, 3u, 2u, 2u, 2u, 2u, 1u, 12u, 5u, 0u, 0u, 1u, 2u, 16u, 4u, 12u, 16u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 6u, 20u, 1u, 1u, 1u, \
	9u, 1u, 1u, 1u, 1u, 1u, 9u, 5u, 14u, 8u, 5u, 1u, 6u, 2u, 3u, 12u, 1u, 14u, 6u, 0u, 0u, 1u, 12u, 1u, 1u, 13u, 17u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 6u, 20u, 1u, 1u, 1u, \
	9u, 1u, 1u, 1u, 1u, 1u, 9u, 5u, 14u, 8u, 5u, 1u, 6u, 2u, 3u, 12u, 1u, 14u, 6u, 0u, 0u, 1u, 2u, 2u, 1u, 14u, 18u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 6u, 20u, 1u, 1u, 1u, \
	9u, 1u, 1u, 1u, 1u, 1u, 9u, 5u, 14u, 8u, 5u, 1u, 6u, 2u, 3u, 12u, 1u, 14u, 6u, 0u, 0u, 1u, 2u, 2u, 1u, 14u, 18u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 10u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 2u, 2u, 1u, 14u, 18u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 2u, 1u, 2u, 10u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 2u, 2u, 1u, 14u, 18u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 0u, 0u, 1u, 2u, 3u, 1u, 1u, 19u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 7u, 21u, 1u, 2u, 2u, \
	10u, 2u, 2u, 2u, 1u, 1u, 1u, 4u, 15u, 9u, 1u, 1u, 3u, 1u, 1u, 13u, 1u, 15u, 1u, 0u, 0u, 1u, 2u, 3u, 1u, 1u, 19u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 4u, 22u, 2u, 3u, 3u, \
	3u, 1u, 3u, 3u, 2u, 1u, 1u, 5u, 5u, 10u, 1u, 1u, 7u, 1u, 2u, 14u, 1u, 16u, 1u, 0u, 0u, 1u, 2u, 3u, 1u, 1u, 19u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 3u, 1u, 1u, 19u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 17u, 1u, 1u, 4u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 17u, 1u, 1u, 4u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 17u, 1u, 1u, 4u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 2u, 17u, 1u, 1u, 4u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 5u, 23u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 6u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 1u, 0u, 0u, 1u, 2u, 17u, 1u, 1u, 4u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 24u, 3u, 4u, 11u, \
	11u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 11u, 1u, 1u, 1u, 1u, 1u, 15u, 3u, 17u, 1u, 0u, 0u, 1u, 13u, 4u, 1u, 5u, 20u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 5u, 24u, 3u, 4u, 12u, \
	11u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 11u, 1u, 1u, 1u, 1u, 1u, 15u, 3u, 17u, 1u, 0u, 0u, 1u, 13u, 4u, 1u, 5u, 20u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 3u, 7u, 1u, 0u, 0u, 1u, 13u, 4u, 1u, 5u, 20u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 5u, 16u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 8u, 5u, 1u, 1u, 3u, 1u, 1u, 9u, 3u, 9u, 1u, 0u, 0u, 1u, 13u, 4u, 1u, 5u, 20u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 5u, 16u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 8u, 5u, 1u, 1u, 3u, 1u, 1u, 9u, 3u, 9u, 1u, 0u, 0u, 1u, 13u, 4u, 1u, 5u, 20u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 8u, 25u, 3u, 4u, 13u, \
	12u, 1u, 3u, 4u, 2u, 2u, 1u, 5u, 4u, 12u, 1u, 1u, 8u, 1u, 2u, 16u, 4u, 18u, 1u, 0u, 0u, 1u, 13u, 18u, 1u, 14u, 21u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 4u, 26u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 16u, 7u, 5u, 3u, 9u, 2u, 2u, 7u, 1u, 19u, 5u, 0u, 0u, 1u, 13u, 18u, 1u, 14u, 21u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 4u, 26u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 16u, 7u, 5u, 3u, 9u, 2u, 2u, 7u, 1u, 19u, 5u, 0u, 0u, 1u, 13u, 18u, 1u, 14u, 21u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 4u, 26u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 16u, 7u, 5u, 3u, 9u, 2u, 2u, 7u, 1u, 19u, 5u, 0u, 0u, 1u, 13u, 18u, 1u, 14u, 21u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 2u, 27u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	3u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	13u, 3u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 4u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 4u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 3u, 4u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 9u, 28u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 6u, 9u, 8u, 1u, 1u, 1u, 1u, 1u, 12u, 5u, 13u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 29u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 8u, 1u, 1u, 1u, 1u, 1u, 12u, 3u, 13u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 10u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 6u, 7u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 11u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 6u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 7u, 7u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 10u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 6u, 7u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 10u, 9u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 8u, 4u, 1u, 1u, 3u, 1u, 1u, 7u, 6u, 7u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 3u, 30u, 3u, 4u, 14u, \
	12u, 1u, 3u, 4u, 2u, 2u, 1u, 4u, 4u, 13u, 1u, 1u, 1u, 1u, 1u, 17u, 4u, 20u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 3u, 31u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 10u, 4u, 4u, 14u, 6u, 1u, 1u, 3u, 1u, 18u, 4u, 21u, 7u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 4u, 32u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 14u, 5u, 3u, 2u, 2u, 2u, 19u, 1u, 22u, 5u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 4u, 32u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 14u, 5u, 3u, 2u, 2u, 2u, 19u, 1u, 22u, 5u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 14u, 19u, 1u, 15u, 22u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 11u, 3u, 1u, 15u, 23u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 11u, 3u, 1u, 15u, 23u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 11u, 3u, 1u, 15u, 23u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 11u, 3u, 1u, 15u, 23u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 3u, 3u, 3u, 1u, 3u, 2u, 1u, 2u, 1u, 2u, 3u, 2u, 3u, 0u, 0u, 1u, 11u, 20u, 1u, 15u, 23u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 3u, 3u, 3u, 1u, 3u, 2u, 1u, 2u, 1u, 2u, 3u, 2u, 3u, 0u, 0u, 1u, 11u, 20u, 1u, 15u, 23u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	3u, 3u, 3u, 3u, 2u, 1u, 3u, 4u, 4u, 2u, 3u, 2u, 1u, 2u, 1u, 3u, 4u, 3u, 3u, 0u, 0u, 1u, 11u, 20u, 1u, 15u, 23u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 11u, 20u, 1u, 15u, 23u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 15u, 21u, 1u, 15u, 23u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 15u, 21u, 1u, 15u, 23u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 15u, 21u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 4u, 6u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 5u, 5u, 5u, 3u, 3u, 4u, 2u, 2u, 2u, 5u, 1u, 5u, 3u, 0u, 0u, 1u, 15u, 21u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 15u, 21u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 16u, 22u, 1u, 12u, 24u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 16u, 22u, 1u, 12u, 24u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 16u, 22u, 1u, 12u, 24u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 16u, 22u, 1u, 12u, 24u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 5u, 9u, 3u, 4u, 5u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 8u, 4u, 3u, 4u, 3u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 16u, 22u, 1u, 12u, 24u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 3u, 10u, 3u, 4u, 6u, \
	6u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 4u, 5u, 1u, 3u, 1u, 1u, 1u, 8u, 4u, 8u, 4u, 0u, 0u, 1u, 2u, 22u, 1u, 15u, 24u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 3u, 10u, 3u, 4u, 6u, \
	6u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 4u, 5u, 1u, 3u, 1u, 1u, 1u, 8u, 4u, 8u, 4u, 0u, 0u, 1u, 2u, 22u, 1u, 15u, 24u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 3u, 11u, 3u, 4u, 6u, \
	6u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 9u, 5u, 1u, 3u, 1u, 1u, 1u, 9u, 4u, 9u, 4u, 0u, 0u, 1u, 2u, 22u, 1u, 15u, 24u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 3u, 12u, 3u, 4u, 4u, \
	6u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 9u, 1u, 1u, 3u, 1u, 1u, 1u, 2u, 4u, 2u, 4u, 0u, 0u, 1u, 2u, 22u, 1u, 15u, 24u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 17u, 23u, 2u, 16u, 25u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 18u, 24u, 3u, 17u, 26u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 18u, 24u, 3u, 17u, 26u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 18u, 24u, 3u, 17u, 26u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 18u, 24u, 3u, 17u, 26u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 19u, 25u, 3u, 18u, 27u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 2u, 3u, 1u, 2u, 7u, \
	2u, 4u, 2u, 2u, 1u, 1u, 3u, 3u, 3u, 1u, 3u, 2u, 1u, 2u, 1u, 2u, 3u, 2u, 3u, 0u, 0u, 1u, 19u, 25u, 3u, 18u, 27u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 5u, 6u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 3u, 3u, 10u, 3u, 3u, 2u, 1u, 2u, 1u, 5u, 3u, 5u, 3u, 0u, 0u, 1u, 19u, 25u, 3u, 18u, 27u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 5u, 6u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 3u, 3u, 10u, 3u, 3u, 2u, 1u, 2u, 1u, 5u, 3u, 5u, 3u, 0u, 0u, 1u, 19u, 25u, 3u, 18u, 27u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 5u, 13u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 3u, 3u, 10u, 2u, 3u, 2u, 1u, 2u, 1u, 10u, 3u, 10u, 3u, 0u, 0u, 1u, 20u, 26u, 3u, 17u, 26u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 14u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 11u, 3u, 3u, 4u, 3u, 2u, 1u, 11u, 4u, 5u, 3u, 0u, 0u, 1u, 20u, 26u, 3u, 17u, 26u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 14u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 11u, 3u, 3u, 4u, 3u, 2u, 1u, 11u, 4u, 5u, 3u, 0u, 0u, 1u, 20u, 26u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 3u, 15u, 2u, 3u, 3u, \
	7u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 12u, 3u, 3u, 4u, 3u, 2u, 1u, 11u, 4u, 11u, 3u, 0u, 0u, 1u, 20u, 26u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 20u, 26u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 21u, 27u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 21u, 27u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 21u, 27u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 7u, 4u, 3u, 4u, 1u, 2u, 1u, 7u, 3u, 7u, 3u, 0u, 0u, 1u, 21u, 27u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 5u, 16u, 3u, 4u, 8u, \
	5u, 1u, 3u, 3u, 2u, 2u, 5u, 3u, 8u, 5u, 3u, 4u, 3u, 2u, 1u, 9u, 3u, 9u, 3u, 0u, 0u, 1u, 21u, 27u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 3u, 4u, 3u, 4u, 9u, \
	8u, 1u, 3u, 3u, 2u, 2u, 7u, 4u, 4u, 2u, 1u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 0u, 0u, 1u, 18u, 28u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 4u, 6u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 3u, 5u, 3u, 2u, 2u, 2u, 5u, 1u, 5u, 5u, 0u, 0u, 1u, 18u, 28u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 4u, 17u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 1u, 5u, 3u, 2u, 2u, 2u, 2u, 1u, 12u, 5u, 0u, 0u, 1u, 18u, 28u, 3u, 17u, 26u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 4u, 17u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 1u, 5u, 3u, 2u, 2u, 2u, 2u, 1u, 12u, 5u, 0u, 0u, 1u, 18u, 28u, 3u, 17u, 26u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 22u, 29u, 3u, 15u, 22u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 21u, 30u, 3u, 15u, 28u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 21u, 30u, 3u, 15u, 28u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 21u, 30u, 3u, 15u, 28u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 0u, 0u, 1u, 21u, 30u, 3u, 15u, 28u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 0u, 0u, 1u, 21u, 21u, 3u, 15u, 28u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 0u, 0u, 1u, 21u, 21u, 3u, 15u, 28u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	3u, 1u, 3u, 3u, 2u, 1u, 1u, 4u, 4u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 4u, 3u, 1u, 0u, 0u, 1u, 21u, 21u, 3u, 15u, 28u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 21u, 21u, 3u, 15u, 28u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 20u, 31u, 3u, 12u, 29u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 20u, 31u, 3u, 12u, 29u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 20u, 31u, 3u, 12u, 29u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 20u, 31u, 3u, 12u, 29u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 20u, 31u, 3u, 12u, 29u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 7u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 6u, 3u, 6u, 4u, 2u, 4u, 1u, 1u, 1u, 6u, 3u, 6u, 2u, 0u, 0u, 1u, 23u, 32u, 3u, 19u, 23u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 3u, 7u, 1u, 0u, 0u, 1u, 23u, 32u, 3u, 19u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 3u, 7u, 1u, 0u, 0u, 1u, 23u, 32u, 3u, 19u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 3u, 18u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 13u, 6u, 1u, 1u, 4u, 1u, 1u, 12u, 4u, 13u, 1u, 0u, 0u, 1u, 23u, 32u, 3u, 19u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 5u, 19u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 7u, 1u, 1u, 5u, 1u, 1u, 7u, 3u, 7u, 1u, 0u, 0u, 1u, 23u, 32u, 3u, 19u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 3u, 4u, 3u, 4u, 6u, \
	8u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 4u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 4u, 3u, 1u, 0u, 0u, 1u, 24u, 33u, 4u, 18u, 30u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 3u, 1u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 4u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 1u, 1u, 0u, 0u, 1u, 24u, 33u, 4u, 18u, 30u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 4u, 17u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 1u, 5u, 3u, 2u, 2u, 2u, 2u, 1u, 12u, 5u, 0u, 0u, 1u, 24u, 33u, 4u, 18u, 30u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 4u, 17u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 1u, 5u, 3u, 2u, 2u, 2u, 2u, 1u, 12u, 5u, 0u, 0u, 1u, 24u, 33u, 4u, 18u, 30u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 6u, 20u, 1u, 1u, 1u, \
	9u, 1u, 1u, 1u, 1u, 1u, 9u, 5u, 14u, 8u, 5u, 1u, 6u, 2u, 3u, 12u, 1u, 14u, 6u, 0u, 0u, 1u, 1u, 34u, 1u, 20u, 23u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 6u, 20u, 1u, 1u, 1u, \
	9u, 1u, 1u, 1u, 1u, 1u, 9u, 5u, 14u, 8u, 5u, 1u, 6u, 2u, 3u, 12u, 1u, 14u, 6u, 0u, 0u, 1u, 11u, 3u, 1u, 15u, 23u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 6u, 20u, 1u, 1u, 1u, \
	9u, 1u, 1u, 1u, 1u, 1u, 9u, 5u, 14u, 8u, 5u, 1u, 6u, 2u, 3u, 12u, 1u, 14u, 6u, 0u, 0u, 1u, 11u, 3u, 1u, 15u, 23u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 10u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 11u, 3u, 1u, 15u, 23u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 2u, 1u, 2u, 10u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 0u, 0u, 1u, 11u, 3u, 1u, 15u, 23u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 0u, 0u, 1u, 25u, 35u, 1u, 15u, 22u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 7u, 21u, 1u, 2u, 2u, \
	10u, 2u, 2u, 2u, 1u, 1u, 1u, 4u, 15u, 9u, 1u, 1u, 3u, 1u, 1u, 13u, 1u, 15u, 1u, 0u, 0u, 1u, 25u, 35u, 1u, 15u, 22u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 4u, 22u, 2u, 3u, 3u, \
	3u, 1u, 3u, 3u, 2u, 1u, 1u, 5u, 5u, 10u, 1u, 1u, 7u, 1u, 2u, 14u, 1u, 16u, 1u, 0u, 0u, 1u, 25u, 35u, 1u, 15u, 22u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 25u, 35u, 1u, 15u, 22u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 25u, 36u, 1u, 15u, 23u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 25u, 36u, 1u, 15u, 23u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 25u, 36u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 3u, 5u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 1u, 4u, 4u, 4u, 4u, 0u, 0u, 1u, 25u, 36u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 5u, 23u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 6u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 1u, 0u, 0u, 1u, 25u, 36u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 24u, 3u, 4u, 11u, \
	11u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 11u, 1u, 1u, 1u, 1u, 1u, 15u, 3u, 17u, 1u, 0u, 0u, 1u, 10u, 37u, 1u, 20u, 23u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 5u, 24u, 3u, 4u, 12u, \
	11u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 11u, 1u, 1u, 1u, 1u, 1u, 15u, 3u, 17u, 1u, 0u, 0u, 1u, 10u, 37u, 1u, 20u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 5u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 3u, 7u, 1u, 0u, 0u, 1u, 10u, 37u, 1u, 20u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 5u, 16u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 8u, 5u, 1u, 1u, 3u, 1u, 1u, 9u, 3u, 9u, 1u, 0u, 0u, 1u, 10u, 37u, 1u, 20u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 5u, 16u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 8u, 5u, 1u, 1u, 3u, 1u, 1u, 9u, 3u, 9u, 1u, 0u, 0u, 1u, 10u, 37u, 1u, 20u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 8u, 25u, 3u, 4u, 13u, \
	12u, 1u, 3u, 4u, 2u, 2u, 1u, 5u, 4u, 12u, 1u, 1u, 8u, 1u, 2u, 16u, 4u, 18u, 1u, 0u, 0u, 1u, 26u, 38u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 4u, 26u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 16u, 7u, 5u, 3u, 9u, 2u, 2u, 7u, 1u, 19u, 5u, 0u, 0u, 1u, 26u, 38u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 4u, 26u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 16u, 7u, 5u, 3u, 9u, 2u, 2u, 7u, 1u, 19u, 5u, 0u, 0u, 1u, 26u, 38u, 1u, 15u, 23u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 4u, 26u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 16u, 7u, 5u, 3u, 9u, 2u, 2u, 7u, 1u, 19u, 5u, 0u, 0u, 1u, 26u, 38u, 1u, 15u, 23u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 2u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 2u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 5u, 3u, 3u, 1u, 2u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 3u, 3u, 2u, 1u, 1u, 2u, 1u, 6u, 5u, 5u, 3u, 3u, 1u, 2u, 3u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 7u, 6u, 2u, 1u, 4u, 1u, 2u, 27u, 1u, 2u, 2u, \
	2u, 2u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 3u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 2u, 3u, 1u, 1u, 1u, 3u, 1u, 4u, 4u, 4u, 3u, 1u, 2u, 3u, 1u, 2u, 6u, 2u, 1u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	3u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 8u, 7u, 6u, 3u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	13u, 3u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 3u, 4u, 1u, 1u, 1u, 4u, 1u, 5u, 5u, 5u, 4u, 1u, 3u, 4u, 2u, 9u, 7u, 7u, 4u, 4u, 1u, 3u, 4u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 3u, 4u, 3u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 8u, 8u, 5u, 4u, 3u, 3u, 4u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 3u, 4u, 1u, 1u, 2u, 5u, 2u, 5u, 5u, 5u, 4u, 1u, 3u, 5u, 2u, 10u, 7u, 9u, 5u, 4u, 2u, 3u, 4u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 11u, 9u, 10u, 1u, 4u, 2u, 3u, 4u, 2u, 3u, 3u, \
	4u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 4u, 2u, 4u, 3u, 1u, 1u, 1u, 3u, 4u, 3u, 4u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 3u, 6u, 2u, 5u, 5u, 6u, 5u, 1u, 2u, 3u, 2u, 12u, 10u, 11u, 2u, 4u, 2u, 9u, 28u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 6u, 9u, 8u, 1u, 1u, 1u, 1u, 1u, 12u, 5u, 13u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 13u, 11u, 12u, 3u, 4u, 2u, 5u, 29u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 3u, 7u, 8u, 1u, 1u, 1u, 1u, 1u, 12u, 3u, 13u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 4u, 7u, 2u, 5u, 5u, 7u, 6u, 1u, 2u, 3u, 3u, 8u, 12u, 13u, 4u, 4u, 2u, 10u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 6u, 7u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 6u, 4u, 2u, 11u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 6u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 7u, 7u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 8u, 6u, 2u, 3u, 5u, 4u, 14u, 13u, 14u, 7u, 4u, 2u, 10u, 8u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 7u, 4u, 1u, 1u, 1u, 1u, 1u, 7u, 6u, 7u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 15u, 14u, 15u, 8u, 4u, 2u, 10u, 9u, 3u, 4u, 4u, \
	5u, 1u, 3u, 3u, 2u, 2u, 1u, 4u, 8u, 4u, 1u, 1u, 3u, 1u, 1u, 7u, 6u, 7u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 5u, 9u, 7u, 2u, 3u, 5u, 5u, 16u, 15u, 16u, 9u, 4u, 2u, 3u, 30u, 3u, 4u, 14u, \
	12u, 1u, 3u, 4u, 2u, 2u, 1u, 4u, 4u, 13u, 1u, 1u, 1u, 1u, 1u, 17u, 4u, 20u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 16u, 17u, 10u, 5u, 2u, 3u, 31u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 10u, 4u, 4u, 14u, 6u, 1u, 1u, 3u, 1u, 18u, 4u, 21u, 7u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 17u, 17u, 10u, 5u, 2u, 4u, 32u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 14u, 5u, 3u, 2u, 2u, 2u, 19u, 1u, 22u, 5u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 5u, 4u, 1u, 5u, 8u, 2u, 5u, 6u, 9u, 7u, 2u, 3u, 5u, 5u, 17u, 18u, 18u, 11u, 5u, 2u, 4u, 32u, 3u, 4u, 4u, \
	8u, 1u, 3u, 3u, 2u, 2u, 8u, 5u, 5u, 14u, 5u, 3u, 2u, 2u, 2u, 19u, 1u, 22u, 5u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
}

/* Item 1 RF COMMON table, boards without EPA (V:e0051ce0).
 * MediaTek WM tables (V:e0051ce0): descriptors 0xe007c4b0, register indices 0xe007ec3c, values 0xe007ee70, column
 * map 0xe007d364 (u8 [key][group]), keys 0xe007ebc4 (u8, 120). Key value = group * 5 + WF.
 * Rows in walk order: X(register for P[2] != 1, register for P[2] = 1, then one value per column of the row's group); the column of group g
 * is COLMAP[key][g], key = the position of the key value in KEYS. RFCHAN_SKIP = no write. [static] */
#define RFCHAN_NOEPA_COMMON_NGROUPS 52u
#define RFCHAN_NOEPA_COMMON_NKEYS 120u
#define RFCHAN_NOEPA_COMMON_GROUPS(X) \
	X(1u, 3u) \
	X(1u, 6u) \
	X(1u, 5u) \
	X(2u, 5u) \
	X(6u, 3u) \
	X(2u, 6u) \
	X(1u, 4u) \
	X(1u, 5u) \
	X(4u, 16u) \
	X(2u, 14u) \
	X(1u, 11u) \
	X(19u, 2u) \
	X(2u, 4u) \
	X(2u, 3u) \
	X(2u, 44u) \
	X(1u, 37u) \
	X(1u, 8u) \
	X(1u, 21u) \
	X(1u, 18u) \
	X(1u, 13u) \
	X(1u, 6u) \
	X(1u, 5u) \
	X(1u, 16u) \
	X(1u, 9u) \
	X(1u, 3u) \
	X(1u, 3u) \
	X(10u, 24u) \
	X(5u, 48u) \
	X(2u, 14u) \
	X(5u, 29u) \
	X(3u, 19u) \
	X(5u, 17u) \
	X(5u, 7u) \
	X(6u, 12u) \
	X(4u, 9u) \
	X(2u, 14u) \
	X(2u, 17u) \
	X(1u, 12u) \
	X(1u, 4u) \
	X(1u, 3u) \
	X(4u, 3u) \
	X(1u, 2u) \
	X(2u, 5u) \
	X(1u, 11u) \
	X(1u, 11u) \
	X(15u, 2u) \
	X(1u, 3u) \
	X(2u, 29u) \
	X(1u, 15u) \
	X(1u, 24u) \
	X(1u, 21u) \
	X(1u, 21u)
#define RFCHAN_NOEPA_COMMON_ROWS(X) \
	X(ADIE_CHAIN_145, ADIE_CHAIN_145, RFCHAN_SKIP, 0x64400b64u, 0x64400fe4u) \
	X(ADIE_CHAIN_2A7, ADIE_CHAIN_2AE, RFCHAN_SKIP, 0xf4a52948u, 0xf49d2948u, 0xf48f2948u, 0xfca52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2A8, ADIE_CHAIN_2AF, RFCHAN_SKIP, 0x94bc739cu, 0x94be739cu, 0x94bf18c4u, 0xfffffffcu) \
	X(ADIE_CHAIN_2A9, ADIE_CHAIN_2B0, RFCHAN_SKIP, 0x94a52948u, 0x94a52738u, 0x94a5239cu, 0xfffffffcu) \
	X(ADIE_CHAIN_2AA, ADIE_CHAIN_2B1, RFCHAN_SKIP, 0x94a52948u, 0x73a52948u, 0x39e52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2AB, ADIE_CHAIN_2B2, RFCHAN_SKIP, 0x94a52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2AC, ADIE_CHAIN_2B3, RFCHAN_SKIP, 0x94a52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2AD, ADIE_CHAIN_2B4, RFCHAN_SKIP, 0x94a52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2C3, ADIE_CHAIN_2CA, RFCHAN_SKIP, 0x84210840u, 0xbdef7bdcu) \
	X(ADIE_CHAIN_2C4, ADIE_CHAIN_2CB, RFCHAN_SKIP, 0x84210840u, 0xbdef7bdcu) \
	X(ADIE_CHAIN_2C5, ADIE_CHAIN_2CC, RFCHAN_SKIP, 0x84210840u, 0xbdef7ffcu) \
	X(ADIE_CHAIN_2BF, ADIE_CHAIN_2C6, RFCHAN_SKIP, 0x84350840u, 0x837f0840u, 0x84310840u, 0xc6310840u, 0xffff7bdcu) \
	X(ADIE_CHAIN_2C1, ADIE_CHAIN_2C8, RFCHAN_SKIP, 0x84210d68u, 0x6b5adffcu, 0x84210c60u, 0xc6318c60u, 0xfffffffcu) \
	X(ADIE_CHAIN_2C0, ADIE_CHAIN_2C7, RFCHAN_SKIP, 0x8421bdecu, 0x8431fffcu, 0xbdfffffcu) \
	X(ADIE_CHAIN_2C2, ADIE_CHAIN_2C9, RFCHAN_SKIP, 0xd6a10840u, 0xffe10840u, 0xc6210840u, 0xffef7bdcu) \
	X(ADIE_CHAIN_2E9, ADIE_CHAIN_2EB, RFCHAN_SKIP, 0xffffffffu, 0xddddddddu, 0x8cccccccu, 0xccccccccu, 0x0aaaaaaau, 0x28888888u, 0x66666666u, 0x00555555u, 0x00444444u, 0x00333333u, 0x00222222u, 0x11111111u, 0x00000000u, 0xaaaaaaaau, 0x88888888u) \
	X(ADIE_CHAIN_2EA, ADIE_CHAIN_2EC, RFCHAN_SKIP, 0xffffffffu, 0xddddddddu, 0x8cccccccu, 0xccccccccu, 0x0aaaaaaau, 0x28888888u, 0x66666666u, 0x00555555u, 0x00444444u, 0x00333333u, 0x00222222u, 0x11111111u, 0x00000000u, 0xaaaaaaaau, 0x88888888u) \
	X(ADIE_CHAIN_2ED, ADIE_CHAIN_2EF, RFCHAN_SKIP, 0xffffffffu, 0xddddddddu, 0x8cccccccu, 0xccccccccu, 0x0aaaaaaau, 0x28888888u, 0x66666666u, 0x00555555u, 0x00444444u, 0x00333333u, 0x00222222u, 0x11111111u, 0x00000000u, 0xaaaaaaaau, 0x88888888u) \
	X(ADIE_CHAIN_2EE, ADIE_CHAIN_2F0, RFCHAN_SKIP, 0xffffffffu, 0xddddddddu, 0x8cccccccu, 0xccccccccu, 0x0aaaaaaau, 0x28888888u, 0x66666666u, 0x00555555u, 0x00444444u, 0x00333333u, 0x00222222u, 0x11111111u, 0x00000000u, 0xaaaaaaaau, 0x88888888u) \
	X(ADIE_CHAIN_2F9, ADIE_CHAIN_2FC, RFCHAN_SKIP, 0xa5294a50u, 0x631f18c4u, 0x431f18c4u, 0x329af7bcu, 0x3a58e738u, 0x39d4c630u, 0x3192b5acu, 0x210c6318u, 0x1086318cu, 0x00000014u, 0x00000000u, 0x529af7bcu, 0x4a58e738u) \
	X(ADIE_CHAIN_2FA, ADIE_CHAIN_2FD, RFCHAN_SKIP, 0xa5294a50u, 0x8c58c7c4u, 0x8c50c7c4u, 0x7bcca6bcu, 0x738e9638u, 0x630e7530u, 0x5acc64acu, 0x31884318u, 0x18c4218cu, 0x29400000u, 0x00000000u, 0x7bd4a6bcu, 0x73929638u) \
	X(ADIE_CHAIN_2FB, ADIE_CHAIN_2FE, RFCHAN_SKIP, 0xa5294a50u, 0x8c6318c4u, 0x7bdef7bcu, 0x739ce738u, 0x6318c630u, 0x5ad6b5acu, 0x318c6318u, 0x18c6318cu, 0x014a5294u, 0x00000000u) \
	X(ADIE_CHAIN_29E, ADIE_CHAIN_29E, RFCHAN_SKIP, 0x78888888u) \
	X(ADIE_CHAIN_29F, ADIE_CHAIN_29F, RFCHAN_SKIP, 0x78888888u) \
	X(ADIE_CHAIN_301, ADIE_CHAIN_304, RFCHAN_SKIP, 0x00000000u) \
	X(ADIE_CHAIN_26F, ADIE_CHAIN_287, RFCHAN_SKIP, 0x0f807000u) \
	X(ADIE_CHAIN_270, ADIE_CHAIN_288, RFCHAN_SKIP, 0x03807000u) \
	X(ADIE_CHAIN_271, ADIE_CHAIN_289, RFCHAN_SKIP, 0x03803000u) \
	X(ADIE_CHAIN_273, ADIE_CHAIN_28B, RFCHAN_SKIP, 0x0f807000u) \
	X(ADIE_CHAIN_274, ADIE_CHAIN_28C, RFCHAN_SKIP, 0x03807000u) \
	X(ADIE_CHAIN_275, ADIE_CHAIN_28D, RFCHAN_SKIP, 0x03803000u) \
	X(ADIE_CHAIN_24A, ADIE_CHAIN_25C, RFCHAN_SKIP, 0xffffffe0u) \
	X(ADIE_CHAIN_24B, ADIE_CHAIN_25D, RFCHAN_SKIP, 0xff9e0600u) \
	X(ADIE_CHAIN_24C, ADIE_CHAIN_25E, RFCHAN_SKIP, 0x10083fe0u) \
	X(ADIE_CHAIN_24D, ADIE_CHAIN_25F, RFCHAN_SKIP, 0xffffffe0u) \
	X(ADIE_CHAIN_24E, ADIE_CHAIN_260, RFCHAN_SKIP, 0x3c0c0400u) \
	X(ADIE_CHAIN_24F, ADIE_CHAIN_261, RFCHAN_SKIP, 0x10000000u) \
	X(ADIE_CHAIN_644, ADIE_CHAIN_654, RFCHAN_SKIP, 0x00000000u) \
	X(ADIE_CHAIN_730, ADIE_CHAIN_73E, RFCHAN_SKIP, 0x0000f540u) \
	X(MT7977_RX_12C, MT7977_RX_12C, RFCHAN_SKIP, 0x42008000u) \
	X(ADIE_CHAIN_731, ADIE_CHAIN_73F, RFCHAN_SKIP, 0xa0502200u) \
	X(ADIE_CHAIN_2FF, ADIE_CHAIN_302, RFCHAN_SKIP, 0x00000000u, 0x40000000u, 0x78000000u) \
	X(ADIE_CHAIN_300, ADIE_CHAIN_303, RFCHAN_SKIP, 0x00000000u, 0x00100000u, 0x001e0000u) \
	X(ADIE_CHAIN_26E, MT7976_TXTRIM_SRC(6u), RFCHAN_SKIP, 0xff9ff000u, 0xfffff000u) \
	X(ADIE_CHAIN_272, ADIE_CHAIN_28A, RFCHAN_SKIP, 0xff9ff000u, 0xfffff000u) \
	X(ADIE_CHAIN_66D, ADIE_CHAIN_63C, RFCHAN_SKIP, 0x00211086u, 0x002118c5u, 0x00319ce6u, 0x003198c3u, 0x004218c3u, 0x00420843u, 0x00528c64u, 0x00630c64u, 0x00738c65u, 0x00739086u, 0x00739087u, 0x00840840u, 0x00840c60u, 0x00948c61u, 0x00211087u, 0x003194a7u, 0x00211ce6u, 0x00319ce7u, 0x003194a2u, 0x00528424u, 0x00630845u, 0x00738846u, 0x00738c67u, 0x00840420u, 0x00948840u, 0x002118c6u, 0x003194a3u, 0x00528844u, 0x00738c66u, 0x00422107u, 0x00630424u, 0x00738845u, 0x00840c67u, 0x00841084u, 0x008414a5u, 0x00841083u, 0x00840c65u, 0x00840c64u, 0x00841085u, 0x007390a5u, 0x007390c5u, 0x007394a4u, 0x009494a5u) \
	X(ADIE_CHAIN_66F, ADIE_CHAIN_6C5, RFCHAN_SKIP, 0x00211086u, 0x002118c5u, 0x00319ce6u, 0x003198c3u, 0x004218c3u, 0x00420843u, 0x00528c64u, 0x00630c64u, 0x00738c65u, 0x00739086u, 0x00739087u, 0x00840840u, 0x00840c60u, 0x00948c61u, 0x00211087u, 0x003194a7u, 0x00211ce6u, 0x00319ce7u, 0x003194a2u, 0x00528424u, 0x00630845u, 0x00738846u, 0x00738c67u, 0x00840420u, 0x00948840u, 0x002118c6u, 0x003194a3u, 0x00528844u, 0x00738c66u, 0x00422107u, 0x00630424u, 0x00738845u, 0x00840c67u, 0x00841084u, 0x008414a5u, 0x00841083u, 0x00840c65u, 0x00840c64u, 0x00841085u, 0x007390a5u, 0x007390c5u, 0x007394a4u, 0x009494a5u) \
	X(ADIE_CHAIN_66E, ADIE_CHAIN_6C4, RFCHAN_SKIP, 0x0c210841u, 0x0a108840u, 0x0c108840u, 0x06210c61u, 0x06211082u, 0x08108841u, 0x0a108841u, 0x0e210c62u, 0x10000000u, 0x12108000u, 0x12108440u, 0x0e210841u, 0x0e108840u, 0x04210c61u, 0x08319082u, 0x0a210841u, 0x08211082u, 0x0a211082u, 0x0c108841u, 0x12210440u, 0x0e210c60u, 0x08210c62u, 0x0c108842u, 0x0c210842u, 0x0e318c62u, 0x06318843u, 0x0a318843u, 0x08210422u, 0x06210422u, 0x0a210421u, 0x08210001u, 0x0a210001u, 0x0a108020u, 0x08108001u, 0x08108421u, 0x08108000u) \
	X(ADIE_CHAIN_645, ADIE_CHAIN_655, RFCHAN_SKIP, 0x00000301u, 0x00000503u, 0x00000304u, 0x00000777u, 0x00000002u, 0x00000042u, 0x00002262u) \
	X(ADIE_CHAIN_63F, ADIE_CHAIN_64F, RFCHAN_SKIP, 0x00000007u, 0x00000004u, 0x00000002u, 0x00000772u, 0x00000440u, 0x00000330u, 0x00066440u, 0x00066660u, 0x00066442u, 0x00044550u, 0x00044444u, 0x00044441u, 0x00044221u, 0x00022770u, 0x00022331u, 0x00022550u, 0x00022771u, 0x00022441u, 0x00000221u, 0x00000220u) \
	X(ADIE_CHAIN_640, ADIE_CHAIN_650, RFCHAN_SKIP, 0x00766667u, 0x00444446u, 0x00222222u, 0x00000000u, 0x00700777u, 0x00733777u, 0x00055557u, 0x00255337u, 0x00077777u, 0x00444757u, 0x00144777u, 0x00166777u, 0x00066777u, 0x00133667u, 0x00033777u, 0x00133557u, 0x00177777u) \
	X(ADIE_CHAIN_63D, ADIE_CHAIN_63D, RFCHAN_SKIP, 0xaaaffc00u, 0x555aa800u, 0x00055400u, 0x00000000u, 0x00005400u, 0xaffffc00u, 0xaffff800u, 0x5aaaa800u, 0x0aaaa800u, 0x00555400u, 0x05555400u, 0x05000000u) \
	X(ADIE_CHAIN_64D, ADIE_CHAIN_64D, RFCHAN_SKIP, 0x003fffffu, 0x002aaaaau, 0x00155555u, 0x00000000u, 0x05000000u) \
	X(ADIE_CHAIN_642, ADIE_CHAIN_652, RFCHAN_SKIP, 0x002aaaabu, 0x002aaabfu, 0x002aaaaau, 0x002aaabau) \
	X(ADIE_CHAIN_647, ADIE_CHAIN_657, RFCHAN_SKIP, 0x00044447u, 0x00044777u, 0x00000000u, 0x00000330u, 0x00011330u, 0x00011220u, 0x00011002u, 0x00011001u, 0x00011000u, 0x00011003u, 0x00044330u, 0x00044220u, 0x00044445u, 0x00000110u, 0x00000220u) \
	X(ADIE_CHAIN_648, ADIE_CHAIN_658, RFCHAN_SKIP, 0x007cc997u, 0x007ccbb7u, 0x00022111u, 0x00000333u, 0x00011111u, 0x00000000u, 0x00dd9b97u, 0x00000110u) \
	X(MT7977_CHAIN_12B, MT7977_CHAIN_12B, RFCHAN_SKIP, 0x00000002u, 0x00000000u) \
	X(ADIE_CHAIN_71E, ADIE_CHAIN_71F, RFCHAN_SKIP, 0xa0a0a3a7u, 0x30303337u) \
	X(ADIE_CHAIN_670, ADIE_CHAIN_6C6, RFCHAN_SKIP, 0x00373724u, 0x00373714u, 0x002d2d0fu, 0x002d2d14u, 0x00242414u, 0x00242437u, 0x001a1a2du, 0x0014142du, 0x000f0f2du, 0x000f0f24u, 0x00101030u, 0x00101028u, 0x000c0c28u, 0x002d2d1au, 0x0037370fu, 0x00141437u, 0x000f0f37u, 0x0010102du, 0x000c0c30u, 0x001a1a37u, 0x00141424u, 0x0014141au, 0x000f0f1au) \
	X(ADIE_CHAIN_673, ADIE_CHAIN_6C9, RFCHAN_SKIP, 0x00373724u, 0x00373714u, 0x002d2d0fu, 0x002d2d14u, 0x00242414u, 0x00242437u, 0x001a1a2du, 0x0014142du, 0x000f0f2du, 0x000f0f24u, 0x00101030u, 0x00101028u, 0x000c0c28u, 0x002d2d1au, 0x0037370fu, 0x00141437u, 0x000f0f37u, 0x0010102du, 0x000c0c30u, 0x001a1a37u, 0x00141424u, 0x0014141au, 0x000f0f1au) \
	X(ADIE_CHAIN_675, ADIE_CHAIN_6CB, RFCHAN_SKIP, 0x00373724u, 0x00373714u, 0x002d2d0fu, 0x002d2d14u, 0x00242414u, 0x00242437u, 0x001a1a2du, 0x0014142du, 0x000f0f2du, 0x000f0f24u, 0x00101030u, 0x00101028u, 0x000c0c28u, 0x002d2d1au, 0x0037370fu, 0x00141437u, 0x000f0f37u, 0x0010102du, 0x000c0c30u, 0x001a1a37u, 0x00141424u, 0x0014141au, 0x000f0f1au) \
	X(ADIE_CHAIN_678, ADIE_CHAIN_6CE, RFCHAN_SKIP, 0x00373724u, 0x00373714u, 0x002d2d0fu, 0x002d2d14u, 0x00242414u, 0x00242437u, 0x001a1a2du, 0x0014142du, 0x000f0f2du, 0x000f0f24u, 0x00101030u, 0x00101028u, 0x000c0c28u, 0x002d2d1au, 0x0037370fu, 0x00141437u, 0x000f0f37u, 0x0010102du, 0x000c0c30u, 0x001a1a37u, 0x00141424u, 0x0014141au, 0x000f0f1au) \
	X(ADIE_CHAIN_67A, ADIE_CHAIN_6D0, RFCHAN_SKIP, 0x003d3d28u, 0x003d3d17u, 0x00323211u, 0x00323217u, 0x00282817u, 0x0028283du, 0x001e1e32u, 0x00171732u, 0x00111132u, 0x00111128u, 0x00121234u, 0x0012122cu, 0x000e0e2cu, 0x0032321eu, 0x003d3d11u, 0x0017173du, 0x0011113du, 0x00121232u, 0x000e0e34u, 0x001e1e3du, 0x00171728u, 0x0017171eu, 0x0011111eu) \
	X(ADIE_CHAIN_67D, ADIE_CHAIN_6D3, RFCHAN_SKIP, 0x003d3d28u, 0x003d3d17u, 0x00323211u, 0x00323217u, 0x00282817u, 0x0028283du, 0x001e1e32u, 0x00171732u, 0x00111132u, 0x00111128u, 0x00121234u, 0x0012122cu, 0x000e0e2cu, 0x0032321eu, 0x003d3d11u, 0x0017173du, 0x0011113du, 0x00121232u, 0x000e0e34u, 0x001e1e3du, 0x00171728u, 0x0017171eu, 0x0011111eu) \
	X(ADIE_CHAIN_67F, ADIE_CHAIN_6D5, RFCHAN_SKIP, 0x003d3d28u, 0x003d3d17u, 0x00323211u, 0x00323217u, 0x00282817u, 0x0028283du, 0x001e1e32u, 0x00171732u, 0x00111132u, 0x00111128u, 0x00121234u, 0x0012122cu, 0x000e0e2cu, 0x0032321eu, 0x003d3d11u, 0x0017173du, 0x0011113du, 0x00121232u, 0x000e0e34u, 0x001e1e3du, 0x00171728u, 0x0017171eu, 0x0011111eu) \
	X(ADIE_CHAIN_682, ADIE_CHAIN_6D8, RFCHAN_SKIP, 0x003d3d28u, 0x003d3d17u, 0x00323211u, 0x00323217u, 0x00282817u, 0x0028283du, 0x001e1e32u, 0x00171732u, 0x00111132u, 0x00111128u, 0x00121234u, 0x0012122cu, 0x000e0e2cu, 0x0032321eu, 0x003d3d11u, 0x0017173du, 0x0011113du, 0x00121232u, 0x000e0e34u, 0x001e1e3du, 0x00171728u, 0x0017171eu, 0x0011111eu) \
	X(ADIE_CHAIN_684, ADIE_CHAIN_6DA, RFCHAN_SKIP, 0x003d3d28u, 0x003d3d17u, 0x00323211u, 0x00323217u, 0x00282817u, 0x0028283du, 0x001e1e32u, 0x00171732u, 0x00111132u, 0x00111128u, 0x00121234u, 0x0012122cu, 0x000e0e2cu, 0x0032321eu, 0x003d3d11u, 0x0017173du, 0x0011113du, 0x00121232u, 0x000e0e34u, 0x001e1e3du, 0x00171728u, 0x0017171eu, 0x0011111eu) \
	X(ADIE_CHAIN_687, ADIE_CHAIN_6DD, RFCHAN_SKIP, 0x003d3d28u, 0x003d3d17u, 0x00323211u, 0x00323217u, 0x00282817u, 0x0028283du, 0x001e1e32u, 0x00171732u, 0x00111132u, 0x00111128u, 0x00121234u, 0x0012122cu, 0x000e0e2cu, 0x0032321eu, 0x003d3d11u, 0x0017173du, 0x0011113du, 0x00121232u, 0x000e0e34u, 0x001e1e3du, 0x00171728u, 0x0017171eu, 0x0011111eu) \
	X(ADIE_CHAIN_671, ADIE_CHAIN_6C7, RFCHAN_SKIP, 0x24141437u, 0x141a1a2du, 0x0f14142du, 0x142d2d37u, 0x372d2d37u, 0x2d242d37u, 0x2d24242du, 0x2d1a1a2du, 0x240f0f37u, 0x30371037u, 0x28371037u, 0x282d0c2du, 0x1a0f0f37u, 0x0f0f0f2du, 0x1a373737u, 0x371a1a37u, 0x37141437u, 0x2d0f0f37u, 0x2d371037u, 0x30370c2du, 0x14141a2du, 0x1a2d2d37u, 0x372d2437u, 0x2d241a37u, 0x37241a2du, 0x2d241a2du, 0x371a142du, 0x2d140f37u, 0x282d0c30u, 0x1414142du, 0x140f0f37u, 0x1a2d2437u, 0x142d2437u, 0x2d242437u, 0x2d24142du, 0x371a1437u, 0x2d0f0f2du, 0x24242d2du, 0x1a1a1a2du, 0x24242437u, 0x242d2d37u, 0x2d1a1a37u, 0x241a1a37u, 0x1a1a1a37u, 0x2424242du, 0x1a24242du, 0x1a1a242du) \
	X(ADIE_CHAIN_676, ADIE_CHAIN_6CC, RFCHAN_SKIP, 0x24141437u, 0x141a1a2du, 0x0f14142du, 0x142d2d37u, 0x372d2d37u, 0x2d242d37u, 0x2d24242du, 0x2d1a1a2du, 0x240f0f37u, 0x30371037u, 0x28371037u, 0x282d0c2du, 0x1a0f0f37u, 0x0f0f0f2du, 0x1a373737u, 0x371a1a37u, 0x37141437u, 0x2d0f0f37u, 0x2d371037u, 0x30370c2du, 0x14141a2du, 0x1a2d2d37u, 0x372d2437u, 0x2d241a37u, 0x37241a2du, 0x2d241a2du, 0x371a142du, 0x2d140f37u, 0x282d0c30u, 0x1414142du, 0x140f0f37u, 0x1a2d2437u, 0x142d2437u, 0x2d242437u, 0x2d24142du, 0x371a1437u, 0x2d0f0f2du, 0x24242d2du, 0x1a1a1a2du, 0x24242437u, 0x242d2d37u, 0x2d1a1a37u, 0x241a1a37u, 0x1a1a1a37u, 0x2424242du, 0x1a24242du, 0x1a1a242du) \
	X(ADIE_CHAIN_67B, ADIE_CHAIN_6D1, RFCHAN_SKIP, 0x2817173du, 0x171e1e32u, 0x11171732u, 0x1732323du, 0x3d32323du, 0x3228323du, 0x32282832u, 0x321e1e32u, 0x2811113du, 0x343d123du, 0x2c3d123du, 0x2c320e32u, 0x1e11113du, 0x11111132u, 0x1e3d3d3du, 0x3d1e1e3du, 0x3d17173du, 0x3211113du, 0x323d123du, 0x343d0e32u, 0x17171e32u, 0x1e32323du, 0x3d32283du, 0x32281e3du, 0x3d281e32u, 0x32281e32u, 0x3d1e1732u, 0x3217113du, 0x2c320e34u, 0x17171732u, 0x1711113du, 0x1e32283du, 0x1732283du, 0x3228283du, 0x32281732u, 0x3d1e173du, 0x32111132u, 0x28283232u, 0x1e1e1e32u, 0x2828283du, 0x2832323du, 0x321e1e3du, 0x281e1e3du, 0x1e1e1e3du, 0x28282832u, 0x1e282832u, 0x1e1e2832u) \
	X(ADIE_CHAIN_680, ADIE_CHAIN_6D6, RFCHAN_SKIP, 0x2817173du, 0x171e1e32u, 0x11171732u, 0x1732323du, 0x3d32323du, 0x3228323du, 0x32282832u, 0x321e1e32u, 0x2811113du, 0x343d123du, 0x2c3d123du, 0x2c320e32u, 0x1e11113du, 0x11111132u, 0x1e3d3d3du, 0x3d1e1e3du, 0x3d17173du, 0x3211113du, 0x323d123du, 0x343d0e32u, 0x17171e32u, 0x1e32323du, 0x3d32283du, 0x32281e3du, 0x3d281e32u, 0x32281e32u, 0x3d1e1732u, 0x3217113du, 0x2c320e34u, 0x17171732u, 0x1711113du, 0x1e32283du, 0x1732283du, 0x3228283du, 0x32281732u, 0x3d1e173du, 0x32111132u, 0x28283232u, 0x1e1e1e32u, 0x2828283du, 0x2832323du, 0x321e1e3du, 0x281e1e3du, 0x1e1e1e3du, 0x28282832u, 0x1e282832u, 0x1e1e2832u) \
	X(ADIE_CHAIN_685, ADIE_CHAIN_6DB, RFCHAN_SKIP, 0x2817173du, 0x171e1e32u, 0x11171732u, 0x1732323du, 0x3d32323du, 0x3228323du, 0x32282832u, 0x321e1e32u, 0x2811113du, 0x343d123du, 0x2c3d123du, 0x2c320e32u, 0x1e11113du, 0x11111132u, 0x1e3d3d3du, 0x3d1e1e3du, 0x3d17173du, 0x3211113du, 0x323d123du, 0x343d0e32u, 0x17171e32u, 0x1e32323du, 0x3d32283du, 0x32281e3du, 0x3d281e32u, 0x32281e32u, 0x3d1e1732u, 0x3217113du, 0x2c320e34u, 0x17171732u, 0x1711113du, 0x1e32283du, 0x1732283du, 0x3228283du, 0x32281732u, 0x3d1e173du, 0x32111132u, 0x28283232u, 0x1e1e1e32u, 0x2828283du, 0x2832323du, 0x321e1e3du, 0x281e1e3du, 0x1e1e1e3du, 0x28282832u, 0x1e282832u, 0x1e1e2832u) \
	X(ADIE_CHAIN_672, ADIE_CHAIN_6C8, RFCHAN_SKIP, 0x3737372du, 0x2d373737u, 0x372d2d2du, 0x37242437u, 0x2d37372du, 0x372d2d37u, 0x37373737u, 0x2d2d3037u, 0x2d242437u, 0x302d3037u, 0x2d2d2d37u, 0x2d372d37u, 0x2d2d2d2du) \
	X(ADIE_CHAIN_677, ADIE_CHAIN_6CD, RFCHAN_SKIP, 0x3737372du, 0x2d373737u, 0x372d2d2du, 0x37242437u, 0x2d37372du, 0x372d2d37u, 0x37373737u, 0x2d2d3037u, 0x2d242437u, 0x302d3037u, 0x2d2d2d37u, 0x2d372d37u, 0x2d2d2d2du) \
	X(ADIE_CHAIN_674, ADIE_CHAIN_6CA, RFCHAN_SKIP, 0x24140000u, 0x141a0000u, 0x0f140000u, 0x142d0000u, 0x372d0000u, 0x2d240000u, 0x2d1a0000u, 0x240f0000u, 0x30370000u, 0x28370000u, 0x282d0000u, 0x1a0f0000u, 0x0f0f0000u, 0x1a370000u, 0x371a0000u, 0x37140000u, 0x2d0f0000u, 0x2d370000u, 0x14140000u, 0x1a2d0000u, 0x37240000u, 0x2d140000u, 0x140f0000u, 0x24240000u, 0x1a1a0000u, 0x242d0000u, 0x241a0000u, 0x1a240000u) \
	X(ADIE_CHAIN_679, ADIE_CHAIN_6CF, RFCHAN_SKIP, 0x24140000u, 0x141a0000u, 0x0f140000u, 0x142d0000u, 0x372d0000u, 0x2d240000u, 0x2d1a0000u, 0x240f0000u, 0x30370000u, 0x28370000u, 0x282d0000u, 0x1a0f0000u, 0x0f0f0000u, 0x1a370000u, 0x371a0000u, 0x37140000u, 0x2d0f0000u, 0x2d370000u, 0x14140000u, 0x1a2d0000u, 0x37240000u, 0x2d140000u, 0x140f0000u, 0x24240000u, 0x1a1a0000u, 0x242d0000u, 0x241a0000u, 0x1a240000u) \
	X(ADIE_CHAIN_67E, ADIE_CHAIN_6D4, RFCHAN_SKIP, 0x28170000u, 0x171e0000u, 0x11170000u, 0x17320000u, 0x3d320000u, 0x32280000u, 0x321e0000u, 0x28110000u, 0x343d0000u, 0x2c3d0000u, 0x2c320000u, 0x1e110000u, 0x11110000u, 0x1e3d0000u, 0x3d1e0000u, 0x3d170000u, 0x32110000u, 0x323d0000u, 0x17170000u, 0x1e320000u, 0x3d280000u, 0x32170000u, 0x17110000u, 0x28280000u, 0x1e1e0000u, 0x28320000u, 0x281e0000u, 0x1e280000u) \
	X(ADIE_CHAIN_683, ADIE_CHAIN_6D9, RFCHAN_SKIP, 0x28170000u, 0x171e0000u, 0x11170000u, 0x17320000u, 0x3d320000u, 0x32280000u, 0x321e0000u, 0x28110000u, 0x343d0000u, 0x2c3d0000u, 0x2c320000u, 0x1e110000u, 0x11110000u, 0x1e3d0000u, 0x3d1e0000u, 0x3d170000u, 0x32110000u, 0x323d0000u, 0x17170000u, 0x1e320000u, 0x3d280000u, 0x32170000u, 0x17110000u, 0x28280000u, 0x1e1e0000u, 0x28320000u, 0x281e0000u, 0x1e280000u) \
	X(ADIE_CHAIN_688, ADIE_CHAIN_6DE, RFCHAN_SKIP, 0x28170000u, 0x171e0000u, 0x11170000u, 0x17320000u, 0x3d320000u, 0x32280000u, 0x321e0000u, 0x28110000u, 0x343d0000u, 0x2c3d0000u, 0x2c320000u, 0x1e110000u, 0x11110000u, 0x1e3d0000u, 0x3d1e0000u, 0x3d170000u, 0x32110000u, 0x323d0000u, 0x17170000u, 0x1e320000u, 0x3d280000u, 0x32170000u, 0x17110000u, 0x28280000u, 0x1e1e0000u, 0x28320000u, 0x281e0000u, 0x1e280000u) \
	X(ADIE_CHAIN_67C, ADIE_CHAIN_6D2, RFCHAN_SKIP, 0x3d3d3d32u, 0x323d3d3fu, 0x3d323232u, 0x3d28283du, 0x323d3d32u, 0x3d32323du, 0x3d3d3d3du, 0x323d3d3du, 0x3232343du, 0x3228283du, 0x3432343du, 0x3d32323fu, 0x3232323du, 0x3d3f3f32u, 0x323f323fu, 0x323f3f32u, 0x32323232u, 0x323f3f3fu) \
	X(ADIE_CHAIN_681, ADIE_CHAIN_6D7, RFCHAN_SKIP, 0x3d3d3d32u, 0x323d3d3fu, 0x3d323232u, 0x3d28283du, 0x323d3d32u, 0x3d32323du, 0x3d3d3d3du, 0x323d3d3du, 0x3232343du, 0x3228283du, 0x3432343du, 0x3d32323fu, 0x3232323du, 0x3d3f3f32u, 0x323f323fu, 0x323f3f32u, 0x32323232u, 0x323f3f3fu) \
	X(ADIE_CHAIN_686, ADIE_CHAIN_6DC, RFCHAN_SKIP, 0x3d3d3d32u, 0x323d3d3fu, 0x3d323232u, 0x3d28283du, 0x323d3d32u, 0x3d32323du, 0x3d3d3d3du, 0x323d3d3du, 0x3232343du, 0x3228283du, 0x3432343du, 0x3d32323fu, 0x3232323du, 0x3d3f3f32u, 0x323f323fu, 0x323f3f32u, 0x32323232u, 0x323f3f3fu) \
	X(ADIE_CHAIN_689, ADIE_CHAIN_6DF, RFCHAN_SKIP, 0x002aaaabu, 0x002aabebu, 0x002aaaaau, 0x000162aau, 0x000162a6u, 0x002bebebu, 0x0002a2aau, 0x00016166u, 0x00296aabu, 0x002bebeau, 0x0016aaaau, 0x0016aabeu, 0x0016aabfu, 0x002aaabfu, 0x002aabffu, 0x0016abffu) \
	X(ADIE_CHAIN_68B, ADIE_CHAIN_6E1, RFCHAN_SKIP, 0x002aaaabu, 0x002aabebu, 0x002aaaaau, 0x000162aau, 0x000162a6u, 0x002bebebu, 0x0002a2aau, 0x00016166u, 0x00296aabu, 0x002bebeau, 0x0016aaaau, 0x0016aabeu, 0x0016aabfu, 0x002aaabfu, 0x002aabffu, 0x0016abffu) \
	X(ADIE_CHAIN_68D, ADIE_CHAIN_6E3, RFCHAN_SKIP, 0x002aaaabu, 0x002aabebu, 0x002aaaaau, 0x000162aau, 0x000162a6u, 0x002bebebu, 0x0002a2aau, 0x00016166u, 0x00296aabu, 0x002bebeau, 0x0016aaaau, 0x0016aabeu, 0x0016aabfu, 0x002aaabfu, 0x002aabffu, 0x0016abffu) \
	X(ADIE_CHAIN_68F, ADIE_CHAIN_6E5, RFCHAN_SKIP, 0x002aaaabu, 0x002aabebu, 0x002aaaaau, 0x000162aau, 0x000162a6u, 0x002bebebu, 0x0002a2aau, 0x00016166u, 0x00296aabu, 0x002bebeau, 0x0016aaaau, 0x0016aabeu, 0x0016aabfu, 0x002aaabfu, 0x002aabffu, 0x0016abffu) \
	X(ADIE_CHAIN_691, ADIE_CHAIN_6E7, RFCHAN_SKIP, 0x002aaaabu, 0x002aabebu, 0x002aaaaau, 0x000162aau, 0x000162a6u, 0x002bebebu, 0x0002a2aau, 0x00016166u, 0x00296aabu, 0x002bebeau, 0x0016aaaau, 0x0016aabeu, 0x0016aabfu, 0x002aaabfu, 0x002aabffu, 0x0016abffu) \
	X(ADIE_CHAIN_68A, ADIE_CHAIN_6E0, RFCHAN_SKIP, 0x002aa000u, 0x00016000u, 0x002be000u, 0x0002a000u, 0x00296000u, 0x0016a000u) \
	X(ADIE_CHAIN_68C, ADIE_CHAIN_6E2, RFCHAN_SKIP, 0x002aa000u, 0x00016000u, 0x002be000u, 0x0002a000u, 0x00296000u, 0x0016a000u) \
	X(ADIE_CHAIN_68E, ADIE_CHAIN_6E4, RFCHAN_SKIP, 0x002aa000u, 0x00016000u, 0x002be000u, 0x0002a000u, 0x00296000u, 0x0016a000u) \
	X(ADIE_CHAIN_690, ADIE_CHAIN_6E6, RFCHAN_SKIP, 0x002aa000u, 0x00016000u, 0x002be000u, 0x0002a000u, 0x00296000u, 0x0016a000u) \
	X(ADIE_CHAIN_692, ADIE_CHAIN_6E8, RFCHAN_SKIP, 0x002aa000u, 0x00016000u, 0x002be000u, 0x0002a000u, 0x00296000u, 0x0016a000u) \
	X(ADIE_CHAIN_693, ADIE_CHAIN_6E9, RFCHAN_SKIP, 0x00044443u, 0x00044333u, 0x00044334u, 0x00044444u, 0x00033444u, 0x00033443u, 0x00088888u, 0x00033333u, 0x00033433u, 0x00033334u, 0x00022333u) \
	X(ADIE_CHAIN_695, ADIE_CHAIN_6EB, RFCHAN_SKIP, 0x00044443u, 0x00044333u, 0x00044334u, 0x00044444u, 0x00033444u, 0x00033443u, 0x00088888u, 0x00033333u, 0x00033433u, 0x00033334u, 0x00022333u) \
	X(ADIE_CHAIN_696, ADIE_CHAIN_6EC, RFCHAN_SKIP, 0x00044443u, 0x00044333u, 0x00044334u, 0x00044444u, 0x00033444u, 0x00033443u, 0x00088888u, 0x00033333u, 0x00033433u, 0x00033334u, 0x00022333u) \
	X(ADIE_CHAIN_698, ADIE_CHAIN_6EE, RFCHAN_SKIP, 0x00044443u, 0x00044333u, 0x00044334u, 0x00044444u, 0x00033444u, 0x00033443u, 0x00088888u, 0x00033333u, 0x00033433u, 0x00033334u, 0x00022333u) \
	X(ADIE_CHAIN_699, ADIE_CHAIN_6EF, RFCHAN_SKIP, 0x00055553u, 0x00055333u, 0x00055335u, 0x00055555u, 0x00033555u, 0x00033553u, 0x00088888u, 0x00033333u, 0x00033533u, 0x00033335u, 0x00022333u) \
	X(ADIE_CHAIN_69B, ADIE_CHAIN_6F1, RFCHAN_SKIP, 0x00055553u, 0x00055333u, 0x00055335u, 0x00055555u, 0x00033555u, 0x00033553u, 0x00088888u, 0x00033333u, 0x00033533u, 0x00033335u, 0x00022333u) \
	X(ADIE_CHAIN_694, ADIE_CHAIN_6EA, RFCHAN_SKIP, 0x00344444u, 0x00344445u, 0x00444444u, 0x00888888u, 0x00444554u, 0x00344554u, 0x00344545u, 0x00444555u) \
	X(ADIE_CHAIN_697, ADIE_CHAIN_6ED, RFCHAN_SKIP, 0x00344444u, 0x00344445u, 0x00444444u, 0x00888888u, 0x00444554u, 0x00344554u, 0x00344545u, 0x00444555u) \
	X(ADIE_CHAIN_69A, ADIE_CHAIN_6F0, RFCHAN_SKIP, 0x00355555u, 0x00355557u, 0x00555555u, 0x00888888u, 0x00555775u, 0x00355775u, 0x00355757u, 0x00555777u) \
	X(ADIE_CHAIN_69D, ADIE_CHAIN_6F3, RFCHAN_SKIP, 0x00244444u, 0x00244445u, 0x00444444u, 0x00888888u, 0x00444554u, 0x00244554u, 0x00244545u, 0x00444555u) \
	X(ADIE_CHAIN_69C, ADIE_CHAIN_6F2, RFCHAN_SKIP, 0x00044442u, 0x00044222u, 0x00044224u, 0x00044444u, 0x00022444u, 0x00022442u, 0x00088888u, 0x00044002u, 0x00000442u, 0x00000444u, 0x00000222u, 0x00022422u, 0x00022224u) \
	X(ADIE_CHAIN_69E, ADIE_CHAIN_6F4, RFCHAN_SKIP, 0x00044442u, 0x00044222u, 0x00044224u, 0x00044444u, 0x00022444u, 0x00022442u, 0x00088888u, 0x00044002u, 0x00000442u, 0x00000444u, 0x00000222u, 0x00022422u, 0x00022224u) \
	X(ADIE_CHAIN_69F, ADIE_CHAIN_6F5, RFCHAN_SKIP, 0x00066442u, 0x00066224u, 0x00066222u, 0x00066226u, 0x00044226u, 0x00044666u, 0x00044664u, 0x00022664u, 0x00022442u, 0x00088888u, 0x00066446u, 0x00022662u, 0x00044222u, 0x00022444u, 0x00022446u, 0x00022424u) \
	X(ADIE_CHAIN_6A1, ADIE_CHAIN_6F7, RFCHAN_SKIP, 0x00066442u, 0x00066224u, 0x00066222u, 0x00066226u, 0x00044226u, 0x00044666u, 0x00044664u, 0x00022664u, 0x00022442u, 0x00088888u, 0x00066446u, 0x00022662u, 0x00044222u, 0x00022444u, 0x00022446u, 0x00022424u) \
	X(ADIE_CHAIN_6A0, ADIE_CHAIN_6F6, RFCHAN_SKIP, 0x00266666u, 0x00466667u, 0x00266667u, 0x00666666u, 0x00666446u, 0x00466666u, 0x00888888u, 0x00466446u, 0x00466776u, 0x00466767u, 0x00466777u) \
	X(ADIE_CHAIN_637, ADIE_CHAIN_637, RFCHAN_SKIP, 0x00007fb5u, 0x00007ffdu, 0x00007fffu) \
	X(ADIE_CHAIN_638, ADIE_CHAIN_638, RFCHAN_SKIP, 0x0001b491u, 0x0003ecd2u) \
	X(ADIE_CHAIN_124, ADIE_CHAIN_124, RFCHAN_SKIP, 0x1600ea14u, 0x16002a14u) \
	X(MT7977_TTG_EN, MT7977_TTG_EN, RFCHAN_SKIP, 0x09205d40u, 0x09208d40u) \
	X(ADIE_CHAIN_13E, ADIE_CHAIN_13E, RFCHAN_SKIP, 0x58fe0000u, 0x59ce0000u) \
	X(MT7977_TTG_140, MT7977_TTG_140, RFCHAN_SKIP, 0xfe80ac02u, 0xfe80ac02u) \
	X(ADIE_CHAIN_13F, ADIE_CHAIN_13F, RFCHAN_SKIP, 0xc4000044u) \
	X(ADIE_CHAIN_31D, ADIE_CHAIN_31F, RFCHAN_SKIP, 0x00000000u, 0xaa000000u, 0x33000000u, 0xcc000000u) \
	X(ADIE_CHAIN_31E, ADIE_CHAIN_320, RFCHAN_SKIP, 0x00000000u, 0xaa000000u, 0x33000000u, 0xcc000000u) \
	X(ADIE_CHAIN_321, ADIE_CHAIN_323, RFCHAN_SKIP, 0x00ffffffu, 0x000ffcccu, 0x00ddd000u, 0x00ddd004u, 0x00000222u, 0x00fffcccu, 0x00000000u, 0x00fffaaau, 0x00ddd006u, 0x00000004u) \
	X(ADIE_CHAIN_322, ADIE_CHAIN_324, RFCHAN_SKIP, 0x00ffffffu, 0x000ffcccu, 0x00ddd004u, 0x00000222u, 0x00fffcccu, 0x00ddd000u, 0x00000000u, 0x00fffaaau, 0x00ddd006u, 0x00000004u) \
	X(ADIE_CHAIN_325, ADIE_CHAIN_327, RFCHAN_SKIP, 0x6db6db6cu) \
	X(ADIE_CHAIN_326, ADIE_CHAIN_328, RFCHAN_SKIP, 0x6db6db6cu) \
	X(ADIE_CHAIN_329, ADIE_CHAIN_32A, RFCHAN_SKIP, 0x00000000u) \
	X(ADIE_CHAIN_4A5, ADIE_CHAIN_4A5, RFCHAN_SKIP, 0x08101820u) \
	X(ADIE_CHAIN_4A6, ADIE_CHAIN_4A6, RFCHAN_SKIP, 0x282c3038u) \
	X(ADIE_CHAIN_4A7, ADIE_CHAIN_4A7, RFCHAN_SKIP, 0x08101820u) \
	X(ADIE_CHAIN_4A8, ADIE_CHAIN_4A8, RFCHAN_SKIP, 0x282c3038u) \
	X(ADIE_CHAIN_337, ADIE_CHAIN_33F, RFCHAN_SKIP, 0x806a846eu) \
	X(ADIE_CHAIN_338, ADIE_CHAIN_340, RFCHAN_SKIP, 0xb041b041u) \
	X(ADIE_CHAIN_339, ADIE_CHAIN_341, RFCHAN_SKIP, 0xb041a241u) \
	X(ADIE_CHAIN_33A, ADIE_CHAIN_342, RFCHAN_SKIP, 0xa241a241u) \
	X(ADIE_CHAIN_33B, ADIE_CHAIN_343, RFCHAN_SKIP, 0x806a846eu) \
	X(ADIE_CHAIN_33C, ADIE_CHAIN_344, RFCHAN_SKIP, 0xb041b041u) \
	X(ADIE_CHAIN_33D, ADIE_CHAIN_345, RFCHAN_SKIP, 0xb041a241u) \
	X(ADIE_CHAIN_33E, ADIE_CHAIN_346, RFCHAN_SKIP, 0xa241a241u) \
	X(ADIE_CHAIN_32B, ADIE_CHAIN_32B, RFCHAN_SKIP, 0x00000000u, 0xc0c0c0c0u) \
	X(ADIE_CHAIN_32C, ADIE_CHAIN_32E, RFCHAN_SKIP, 0x006ef28du, 0x0029e18bu, 0x0012919eu, 0x0022809au, 0x002280aau, 0x0005f08au, 0x0004f0aeu, 0x004c839fu, 0x003cf19eu, 0x0004d0abu, 0x0023908bu, 0x0023908eu, 0x0005f09cu, 0x0004f08fu, 0x0029f38du, 0x003be18bu, 0x0005e099u, 0x0022906cu, 0x00229078u, 0x0005f09au, 0x0004f09du, 0x005ec0adu, 0x002ae1adu, 0x0002908cu, 0x0022927cu, 0x0022918cu, 0x0005f0aau, 0x0004f09bu) \
	X(ADIE_CHAIN_32D, ADIE_CHAIN_32F, RFCHAN_SKIP, 0x006ef28du, 0x0029e18bu, 0x0012919eu, 0x0022809au, 0x002280aau, 0x0005f08au, 0x0004f0aeu, 0x004c839fu, 0x003cf1aeu, 0x0004d0abu, 0x0023908bu, 0x0023908eu, 0x0005f09cu, 0x0004f088u, 0x0029f38du, 0x003be18bu, 0x0005e099u, 0x0022906cu, 0x00229078u, 0x0005f09au, 0x0004f09du, 0x005ec0adu, 0x002ae1adu, 0x0002908cu, 0x0022927cu, 0x0022918cu, 0x0005f0aau, 0x0004f09bu) \
	X(ADIE_CHAIN_330, ADIE_CHAIN_330, RFCHAN_SKIP, 0x01010101u, 0x03030303u, 0xfefefefeu, 0xf8f8f8f8u, 0xfdfdfdfdu, 0x00000000u, 0xc6c6c6c6u, 0x7d7d7d7du, 0x02020202u, 0x3f3f3f3fu, 0x3e3e3e3eu, 0xffffffffu, 0x04040404u, 0xfcfcfcfcu) \
	X(ADIE_CHAIN_331, ADIE_CHAIN_334, RFCHAN_SKIP, 0x4a588108u, 0x5ad8a18cu, 0x7bdad210u, 0xa532d214u, 0xa532d210u, 0x97bec228u, 0xcffec2a8u, 0x5b18a10cu, 0x5296a18cu, 0x9cdac210u, 0x8c70d214u, 0xb73ec2a8u, 0x6b58a210u, 0x5ad8a190u, 0x949ac210u, 0x8c72d214u, 0x97bec22cu, 0xcfbed2a8u, 0x4a58a108u, 0x5ad8a194u, 0x8c60c210u, 0xa7fec228u, 0xcffed2acu) \
	X(ADIE_CHAIN_332, ADIE_CHAIN_335, RFCHAN_SKIP, 0x08129620u, 0x0816b628u, 0x105ef6b4u, 0x18694cb4u, 0x30a5efb0u, 0x30f3ffb0u, 0x1056c628u, 0x1054a5a8u, 0x186736b0u, 0x18631c34u, 0x30adcfb0u, 0x105ad628u, 0x1056b628u, 0x186526b0u, 0x18631cb4u, 0x30b3efb4u, 0x10529628u, 0x18631830u, 0x30a9ffb0u, 0x30f3ffb4u) \
	X(ADIE_CHAIN_333, ADIE_CHAIN_336, RFCHAN_SKIP, 0x10820620u, 0x18c20628u, 0x210416b4u, 0x21461cb4u, 0x21061cb4u, 0x228c2fb0u, 0x2a8c3fb0u, 0x10c41628u, 0x18c415a8u, 0x210616b0u, 0x21461c34u, 0x2a8c2fb0u, 0x21041628u, 0x19041628u, 0x22cc2fb0u, 0x2a8c2fb4u, 0x10841628u, 0x19441628u, 0x21061830u, 0x2acc3fb4u)
#define RFCHAN_NOEPA_COMMON_KEYS { \
	0u, 5u, 10u, 15u, 20u, 25u, 30u, 35u, 40u, 45u, 50u, 55u, 60u, 65u, 70u, 75u, \
	80u, 85u, 90u, 95u, 100u, 105u, 110u, 115u, 1u, 6u, 11u, 16u, 21u, 26u, 31u, 36u, \
	41u, 46u, 51u, 56u, 61u, 66u, 71u, 76u, 81u, 86u, 91u, 96u, 101u, 106u, 111u, 116u, \
	2u, 7u, 12u, 17u, 22u, 27u, 32u, 37u, 42u, 47u, 52u, 57u, 62u, 67u, 72u, 77u, \
	82u, 87u, 92u, 97u, 102u, 107u, 112u, 117u, 3u, 8u, 13u, 18u, 23u, 28u, 33u, 38u, \
	43u, 48u, 53u, 58u, 63u, 68u, 73u, 78u, 83u, 88u, 93u, 98u, 103u, 108u, 113u, 118u, \
	4u, 9u, 14u, 19u, 24u, 29u, 34u, 39u, 44u, 49u, 54u, 59u, 64u, 69u, 74u, 79u, \
	84u, 89u, 94u, 99u, 104u, 109u, 114u, 119u, \
}
#define RFCHAN_NOEPA_COMMON_COLMAP { \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 3u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, \
	2u, 2u, 2u, 2u, 2u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	4u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 3u, 3u, 2u, 3u, 2u, 2u, 1u, 2u, 2u, \
	2u, 3u, 3u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 3u, \
	2u, 1u, 2u, 1u, 3u, 3u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 3u, 3u, 2u, 3u, 2u, 2u, 1u, 2u, 2u, 2u, 3u, \
	3u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 5u, 4u, 3u, 1u, \
	1u, 1u, 4u, 4u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 4u, 4u, 3u, 4u, 3u, 1u, 1u, 3u, 3u, 3u, 4u, 4u, 1u, \
	1u, 0u, 0u, 2u, 2u, 2u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 6u, 5u, 4u, 1u, 1u, 1u, \
	5u, 4u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 3u, 4u, 3u, 1u, 1u, 3u, 3u, 3u, 5u, 4u, 1u, 1u, 0u, \
	0u, 2u, 2u, 2u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 1u, 7u, 6u, 5u, 1u, 1u, 1u, 5u, 4u, \
	2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 3u, 4u, 3u, 1u, 1u, 3u, 3u, 3u, 5u, 4u, 1u, 1u, 0u, 0u, 2u, \
	2u, 2u, 1u, 2u, 3u, 3u, 3u, 3u, 3u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 8u, 7u, 6u, 1u, 2u, 2u, 6u, 4u, 2u, 4u, \
	3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 5u, 3u, 5u, 3u, 1u, 1u, 4u, 3u, 4u, 6u, 4u, 2u, 1u, 0u, 0u, 3u, 3u, 3u, \
	1u, 2u, 4u, 4u, 4u, 4u, 4u, 1u, 3u, 1u, 3u, 1u, 2u, 1u, 2u, 9u, 7u, 6u, 1u, 2u, 2u, 6u, 4u, 2u, 4u, 3u, 3u, \
	3u, 1u, 4u, 3u, 2u, 1u, 6u, 5u, 3u, 5u, 3u, 1u, 1u, 4u, 3u, 4u, 6u, 4u, 2u, 1u, 0u, 0u, 3u, 3u, 3u, 1u, 2u, \
	4u, 4u, 4u, 4u, 4u, 1u, 3u, 1u, 3u, 1u, 2u, 1u, 2u, 10u, 8u, 7u, 1u, 2u, 2u, 6u, 4u, 2u, 4u, 3u, 3u, 3u, 1u, \
	4u, 3u, 2u, 1u, 6u, 5u, 3u, 5u, 3u, 1u, 1u, 4u, 3u, 4u, 6u, 4u, 2u, 1u, 0u, 0u, 3u, 3u, 3u, 1u, 2u, 4u, 4u, \
	4u, 4u, 4u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 11u, 9u, 8u, 1u, 2u, 1u, 6u, 4u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, \
	2u, 1u, 6u, 5u, 3u, 5u, 3u, 1u, 1u, 4u, 3u, 4u, 6u, 4u, 2u, 1u, 0u, 0u, 3u, 3u, 3u, 1u, 2u, 4u, 4u, 4u, 4u, \
	4u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 12u, 10u, 9u, 1u, 2u, 1u, 7u, 5u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, \
	7u, 6u, 4u, 6u, 4u, 3u, 1u, 5u, 3u, 5u, 7u, 5u, 2u, 1u, 0u, 0u, 3u, 4u, 3u, 1u, 2u, 5u, 5u, 5u, 4u, 5u, 2u, \
	4u, 2u, 1u, 1u, 3u, 1u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 8u, 6u, 3u, 5u, 4u, 4u, 4u, 1u, 5u, 3u, 2u, 2u, 8u, 7u, \
	5u, 6u, 5u, 2u, 1u, 5u, 3u, 5u, 8u, 6u, 2u, 1u, 0u, 0u, 3u, 4u, 3u, 1u, 2u, 5u, 5u, 5u, 4u, 5u, 2u, 4u, 3u, \
	1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 8u, 6u, 3u, 5u, 4u, 4u, 4u, 1u, 5u, 3u, 2u, 2u, 8u, 7u, 5u, 6u, \
	5u, 2u, 1u, 5u, 3u, 5u, 8u, 6u, 2u, 1u, 0u, 0u, 3u, 4u, 3u, 1u, 2u, 5u, 5u, 5u, 4u, 5u, 2u, 4u, 3u, 1u, 1u, \
	4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 9u, 7u, 3u, 6u, 4u, 4u, 4u, 1u, 6u, 3u, 2u, 2u, 9u, 8u, 5u, 7u, 5u, 2u, \
	1u, 6u, 1u, 6u, 8u, 6u, 2u, 1u, 0u, 0u, 3u, 4u, 3u, 1u, 2u, 5u, 5u, 5u, 4u, 5u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, \
	3u, 13u, 10u, 9u, 1u, 2u, 1u, 9u, 7u, 3u, 6u, 4u, 4u, 4u, 1u, 6u, 3u, 2u, 2u, 9u, 8u, 5u, 7u, 5u, 2u, 1u, 6u, \
	1u, 6u, 8u, 6u, 2u, 1u, 0u, 0u, 4u, 5u, 4u, 1u, 2u, 6u, 3u, 6u, 5u, 6u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, \
	10u, 9u, 1u, 2u, 1u, 10u, 1u, 3u, 6u, 4u, 4u, 4u, 1u, 6u, 3u, 2u, 2u, 10u, 1u, 1u, 1u, 1u, 1u, 1u, 6u, 1u, 6u, \
	9u, 1u, 2u, 1u, 0u, 0u, 4u, 5u, 4u, 1u, 2u, 6u, 3u, 6u, 5u, 6u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, \
	1u, 3u, 1u, 11u, 8u, 3u, 6u, 4u, 4u, 4u, 1u, 6u, 3u, 2u, 2u, 10u, 9u, 6u, 8u, 6u, 3u, 1u, 6u, 1u, 6u, 9u, 1u, \
	2u, 1u, 0u, 0u, 4u, 5u, 4u, 1u, 2u, 6u, 3u, 6u, 5u, 6u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, \
	1u, 12u, 9u, 4u, 1u, 5u, 5u, 4u, 2u, 7u, 3u, 2u, 2u, 11u, 10u, 7u, 9u, 7u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, \
	0u, 0u, 4u, 5u, 4u, 1u, 2u, 7u, 5u, 7u, 6u, 7u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 13u, \
	9u, 4u, 1u, 5u, 4u, 4u, 2u, 8u, 4u, 2u, 2u, 12u, 11u, 7u, 10u, 7u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, \
	4u, 5u, 4u, 1u, 2u, 7u, 5u, 7u, 6u, 7u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 14u, 10u, 4u, \
	1u, 5u, 4u, 4u, 2u, 8u, 4u, 2u, 2u, 13u, 12u, 2u, 11u, 8u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 5u, \
	4u, 1u, 2u, 7u, 5u, 7u, 6u, 7u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 11u, 10u, 1u, 3u, 1u, 14u, 11u, 4u, 1u, 5u, \
	4u, 4u, 2u, 8u, 4u, 2u, 2u, 13u, 12u, 8u, 11u, 9u, 5u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 5u, 4u, 1u, \
	2u, 7u, 5u, 7u, 6u, 7u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 15u, 12u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 9u, 1u, 8u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 8u, 6u, 8u, \
	7u, 8u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 16u, 12u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 14u, 13u, 1u, 12u, 1u, 1u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 9u, 7u, 9u, 8u, 9u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 1u, 1u, 1u, 1u, 1u, 17u, 3u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 15u, \
	3u, 2u, 3u, 2u, 2u, 1u, 2u, 2u, 2u, 3u, 3u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 9u, 7u, 9u, 8u, 9u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 3u, 3u, 2u, \
	3u, 2u, 2u, 1u, 2u, 2u, 2u, 3u, 3u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 9u, 7u, 9u, 8u, 9u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 4u, 3u, 2u, 1u, 2u, 1u, 18u, 13u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 3u, 14u, 2u, 13u, 2u, \
	2u, 1u, 2u, 2u, 2u, 3u, 3u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 9u, 7u, 9u, 8u, 9u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 5u, 4u, 3u, 1u, 1u, 1u, 19u, 14u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 14u, 15u, 3u, 14u, 3u, 1u, 1u, \
	3u, 3u, 3u, 11u, 4u, 1u, 1u, 0u, 0u, 2u, 6u, 5u, 1u, 1u, 10u, 4u, 10u, 9u, 10u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	6u, 5u, 4u, 1u, 1u, 1u, 5u, 4u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 3u, 4u, 3u, 1u, 1u, 3u, 3u, \
	3u, 5u, 4u, 1u, 1u, 0u, 0u, 2u, 6u, 5u, 1u, 1u, 10u, 4u, 10u, 9u, 10u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 1u, 7u, 6u, \
	5u, 1u, 1u, 1u, 5u, 4u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 3u, 4u, 3u, 1u, 1u, 3u, 3u, 3u, 5u, \
	4u, 1u, 1u, 0u, 0u, 2u, 6u, 5u, 1u, 1u, 10u, 4u, 10u, 9u, 10u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 8u, 7u, 6u, 1u, \
	2u, 2u, 6u, 4u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 5u, 3u, 5u, 3u, 1u, 1u, 4u, 3u, 4u, 6u, 4u, 2u, \
	1u, 0u, 0u, 3u, 3u, 6u, 1u, 2u, 11u, 5u, 11u, 10u, 11u, 1u, 3u, 1u, 3u, 1u, 2u, 1u, 2u, 9u, 7u, 6u, 1u, 2u, 2u, \
	6u, 4u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 5u, 3u, 5u, 3u, 1u, 1u, 4u, 3u, 4u, 6u, 4u, 2u, 1u, 0u, \
	0u, 3u, 3u, 6u, 1u, 2u, 11u, 5u, 11u, 10u, 11u, 1u, 3u, 1u, 3u, 1u, 2u, 1u, 2u, 10u, 8u, 7u, 1u, 2u, 2u, 6u, 4u, \
	2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 5u, 3u, 5u, 3u, 1u, 1u, 4u, 3u, 4u, 6u, 4u, 2u, 1u, 0u, 0u, 3u, \
	3u, 6u, 1u, 2u, 11u, 5u, 11u, 10u, 11u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 11u, 9u, 8u, 1u, 2u, 1u, 6u, 4u, 2u, 4u, \
	3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 5u, 3u, 5u, 3u, 1u, 1u, 4u, 3u, 4u, 6u, 4u, 2u, 1u, 0u, 0u, 3u, 3u, 6u, \
	1u, 2u, 11u, 5u, 11u, 10u, 11u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 12u, 10u, 9u, 1u, 2u, 1u, 7u, 15u, 2u, 4u, 3u, 3u, \
	3u, 1u, 4u, 3u, 2u, 1u, 7u, 7u, 9u, 6u, 10u, 3u, 1u, 5u, 3u, 5u, 7u, 8u, 2u, 1u, 0u, 0u, 3u, 3u, 6u, 1u, 2u, \
	12u, 5u, 11u, 10u, 11u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 20u, 6u, 3u, 5u, 4u, 4u, 4u, 1u, \
	9u, 3u, 2u, 2u, 7u, 7u, 5u, 6u, 5u, 6u, 3u, 5u, 3u, 5u, 7u, 6u, 2u, 1u, 0u, 0u, 3u, 3u, 6u, 1u, 2u, 12u, 5u, \
	11u, 10u, 11u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 21u, 16u, 3u, 5u, 4u, 4u, 4u, 1u, 9u, 3u, \
	2u, 2u, 16u, 16u, 1u, 15u, 1u, 1u, 1u, 6u, 1u, 6u, 8u, 6u, 2u, 1u, 0u, 0u, 3u, 3u, 6u, 1u, 2u, 12u, 5u, 11u, 10u, \
	11u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 21u, 16u, 3u, 6u, 4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, \
	16u, 16u, 1u, 15u, 1u, 1u, 1u, 6u, 1u, 6u, 8u, 6u, 2u, 1u, 0u, 0u, 3u, 3u, 6u, 1u, 2u, 12u, 5u, 11u, 10u, 11u, 2u, \
	4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 22u, 1u, 3u, 6u, 4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, 17u, 17u, \
	1u, 16u, 1u, 1u, 1u, 6u, 1u, 6u, 12u, 1u, 2u, 1u, 0u, 0u, 4u, 7u, 7u, 1u, 1u, 13u, 8u, 6u, 5u, 6u, 2u, 4u, 3u, \
	1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 22u, 1u, 3u, 6u, 4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, 17u, 17u, 1u, 16u, \
	1u, 1u, 1u, 6u, 1u, 6u, 12u, 1u, 2u, 1u, 0u, 0u, 4u, 7u, 7u, 1u, 1u, 13u, 8u, 6u, 5u, 6u, 2u, 5u, 4u, 4u, 2u, \
	5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 23u, 12u, 3u, 6u, 4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, 9u, 18u, 1u, 17u, 1u, 1u, \
	1u, 6u, 1u, 6u, 12u, 1u, 2u, 1u, 0u, 0u, 4u, 7u, 7u, 1u, 1u, 13u, 8u, 6u, 5u, 6u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, \
	4u, 13u, 10u, 9u, 1u, 3u, 1u, 24u, 9u, 4u, 1u, 6u, 5u, 4u, 2u, 10u, 5u, 2u, 2u, 18u, 19u, 7u, 18u, 7u, 7u, 4u, 7u, \
	4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 5u, 4u, 1u, 2u, 14u, 5u, 12u, 11u, 12u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, \
	10u, 9u, 1u, 3u, 1u, 12u, 9u, 4u, 1u, 6u, 4u, 4u, 2u, 10u, 4u, 2u, 2u, 11u, 10u, 7u, 9u, 7u, 4u, 2u, 7u, 4u, 7u, \
	10u, 7u, 3u, 2u, 0u, 0u, 4u, 5u, 4u, 1u, 2u, 14u, 5u, 12u, 11u, 12u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, \
	1u, 3u, 1u, 25u, 10u, 4u, 1u, 6u, 4u, 4u, 2u, 10u, 4u, 2u, 2u, 19u, 20u, 2u, 9u, 8u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, \
	3u, 2u, 0u, 0u, 4u, 5u, 4u, 1u, 2u, 14u, 5u, 12u, 11u, 12u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 11u, 10u, 1u, 3u, \
	1u, 14u, 11u, 4u, 1u, 5u, 4u, 4u, 2u, 8u, 4u, 2u, 2u, 13u, 12u, 8u, 11u, 9u, 5u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, \
	0u, 0u, 4u, 5u, 4u, 1u, 2u, 14u, 5u, 12u, 11u, 12u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 6u, \
	5u, 1u, 1u, 15u, 6u, 13u, 12u, 13u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 16u, 12u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 14u, 13u, 1u, 12u, 1u, 1u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 1u, 0u, 0u, 1u, 6u, 5u, 1u, \
	1u, 16u, 9u, 14u, 13u, 14u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 1u, 1u, 1u, 1u, 1u, 26u, 2u, 1u, 2u, 2u, 2u, 2u, \
	1u, 2u, 2u, 1u, 1u, 2u, 21u, 2u, 19u, 2u, 2u, 1u, 2u, 2u, 2u, 3u, 2u, 1u, 1u, 0u, 0u, 1u, 6u, 5u, 1u, 1u, 16u, \
	9u, 14u, 13u, 14u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, \
	2u, 1u, 1u, 3u, 3u, 2u, 3u, 2u, 2u, 1u, 2u, 2u, 2u, 3u, 3u, 1u, 1u, 0u, 0u, 1u, 6u, 5u, 1u, 1u, 16u, 9u, 14u, \
	13u, 14u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 3u, 2u, 1u, 2u, 1u, 18u, 13u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, \
	1u, 3u, 14u, 2u, 13u, 2u, 2u, 1u, 2u, 2u, 2u, 3u, 3u, 1u, 1u, 0u, 0u, 1u, 6u, 5u, 1u, 1u, 16u, 9u, 14u, 13u, 14u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 5u, 4u, 3u, 1u, 1u, 1u, 27u, 4u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 14u, \
	22u, 3u, 20u, 3u, 1u, 1u, 3u, 3u, 3u, 11u, 4u, 1u, 1u, 0u, 0u, 2u, 8u, 8u, 1u, 1u, 17u, 4u, 15u, 14u, 10u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 6u, 5u, 4u, 1u, 1u, 1u, 27u, 4u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 14u, 22u, 3u, \
	20u, 3u, 1u, 1u, 3u, 3u, 3u, 11u, 4u, 1u, 1u, 0u, 0u, 2u, 8u, 8u, 1u, 1u, 17u, 4u, 15u, 14u, 10u, 1u, 2u, 1u, 2u, \
	1u, 1u, 1u, 1u, 7u, 6u, 5u, 1u, 1u, 1u, 5u, 4u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, 1u, 5u, 4u, 3u, 4u, 3u, \
	1u, 1u, 3u, 3u, 3u, 5u, 4u, 1u, 1u, 0u, 0u, 2u, 8u, 8u, 1u, 1u, 17u, 4u, 15u, 14u, 10u, 1u, 2u, 1u, 2u, 1u, 2u, \
	1u, 2u, 8u, 7u, 6u, 1u, 2u, 2u, 6u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 23u, 4u, 5u, 4u, 3u, 1u, \
	4u, 3u, 4u, 6u, 8u, 2u, 1u, 0u, 0u, 3u, 3u, 3u, 1u, 2u, 18u, 10u, 16u, 15u, 4u, 1u, 3u, 1u, 3u, 1u, 2u, 1u, 2u, \
	9u, 7u, 6u, 1u, 2u, 2u, 6u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 23u, 4u, 5u, 4u, 3u, 1u, 4u, 3u, \
	4u, 6u, 8u, 2u, 1u, 0u, 0u, 3u, 3u, 3u, 1u, 2u, 18u, 10u, 16u, 15u, 4u, 1u, 3u, 1u, 3u, 1u, 2u, 1u, 2u, 10u, 8u, \
	7u, 1u, 2u, 2u, 6u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 23u, 4u, 5u, 4u, 3u, 1u, 4u, 3u, 4u, 6u, \
	8u, 2u, 1u, 0u, 0u, 3u, 3u, 3u, 1u, 2u, 18u, 10u, 16u, 15u, 4u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 11u, 9u, 8u, 1u, \
	2u, 1u, 6u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 6u, 23u, 4u, 5u, 4u, 3u, 1u, 4u, 3u, 4u, 6u, 8u, 2u, \
	1u, 0u, 0u, 3u, 3u, 3u, 1u, 2u, 18u, 10u, 16u, 15u, 4u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 12u, 10u, 9u, 1u, 2u, 1u, \
	7u, 18u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 7u, 24u, 4u, 6u, 4u, 3u, 1u, 5u, 1u, 5u, 7u, 8u, 2u, 1u, 0u, \
	0u, 3u, 4u, 3u, 1u, 2u, 19u, 11u, 16u, 15u, 4u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 28u, 7u, \
	3u, 5u, 4u, 4u, 4u, 1u, 5u, 3u, 2u, 2u, 20u, 25u, 5u, 21u, 5u, 2u, 1u, 5u, 1u, 5u, 7u, 6u, 2u, 1u, 0u, 0u, 3u, \
	4u, 3u, 1u, 2u, 19u, 11u, 16u, 15u, 4u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 8u, 7u, 3u, 5u, \
	4u, 4u, 4u, 1u, 5u, 3u, 2u, 2u, 8u, 26u, 5u, 6u, 5u, 2u, 1u, 5u, 1u, 5u, 8u, 6u, 2u, 1u, 0u, 0u, 3u, 4u, 3u, \
	1u, 2u, 19u, 11u, 16u, 15u, 4u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 21u, 19u, 3u, 6u, 4u, 4u, \
	4u, 1u, 6u, 3u, 2u, 2u, 16u, 27u, 5u, 15u, 5u, 2u, 1u, 6u, 1u, 6u, 8u, 1u, 2u, 1u, 0u, 0u, 3u, 4u, 3u, 1u, 2u, \
	19u, 11u, 16u, 15u, 4u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 21u, 19u, 3u, 6u, 4u, 4u, 4u, 1u, \
	6u, 3u, 2u, 2u, 16u, 27u, 5u, 15u, 5u, 2u, 1u, 6u, 1u, 6u, 8u, 1u, 2u, 1u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 20u, 8u, \
	17u, 5u, 15u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 29u, 8u, 3u, 6u, 4u, 4u, 4u, 1u, 6u, 3u, \
	2u, 2u, 9u, 28u, 6u, 22u, 6u, 3u, 1u, 6u, 1u, 6u, 12u, 1u, 2u, 1u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 20u, 8u, 17u, 5u, \
	15u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 23u, 12u, 3u, 6u, 4u, 4u, 4u, 1u, 6u, 3u, 2u, 2u, \
	9u, 18u, 1u, 17u, 1u, 1u, 1u, 6u, 1u, 6u, 12u, 1u, 2u, 1u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 20u, 8u, 17u, 5u, 15u, 2u, \
	5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 12u, 9u, 4u, 1u, 5u, 5u, 4u, 2u, 7u, 5u, 2u, 2u, 11u, 10u, \
	7u, 9u, 7u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 21u, 12u, 18u, 16u, 16u, 2u, 5u, 4u, \
	4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 12u, 9u, 4u, 1u, 5u, 4u, 4u, 2u, 8u, 4u, 2u, 2u, 11u, 10u, 7u, 9u, \
	7u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 21u, 12u, 18u, 16u, 16u, 2u, 5u, 4u, 4u, 2u, \
	5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 14u, 10u, 4u, 1u, 5u, 4u, 4u, 2u, 8u, 4u, 2u, 2u, 13u, 12u, 2u, 11u, 8u, 4u, \
	2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 21u, 12u, 18u, 16u, 16u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, \
	4u, 13u, 11u, 10u, 1u, 3u, 1u, 14u, 20u, 4u, 1u, 5u, 4u, 4u, 2u, 8u, 4u, 2u, 2u, 13u, 29u, 10u, 11u, 11u, 8u, 2u, 7u, \
	4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 21u, 12u, 18u, 16u, 16u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 0u, 0u, 1u, 6u, 5u, 1u, 1u, 22u, 13u, 19u, 17u, 17u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, \
	1u, 16u, 12u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 14u, 13u, 1u, 12u, 1u, 1u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 1u, \
	0u, 0u, 1u, 6u, 5u, 1u, 1u, 23u, 9u, 20u, 13u, 18u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 1u, 1u, 1u, 1u, 1u, 26u, \
	3u, 1u, 2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 2u, 30u, 2u, 19u, 2u, 2u, 1u, 2u, 2u, 2u, 3u, 3u, 1u, 1u, 0u, 0u, \
	1u, 6u, 5u, 1u, 1u, 23u, 9u, 20u, 13u, 18u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 2u, 1u, 1u, 1u, 3u, 3u, 1u, \
	2u, 2u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 3u, 3u, 2u, 3u, 2u, 2u, 1u, 2u, 2u, 2u, 3u, 3u, 1u, 1u, 0u, 0u, 1u, 6u, \
	5u, 1u, 1u, 23u, 9u, 20u, 13u, 18u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 3u, 2u, 1u, 2u, 1u, 30u, 21u, 1u, 2u, 2u, \
	2u, 2u, 1u, 2u, 2u, 1u, 1u, 5u, 31u, 6u, 23u, 12u, 9u, 5u, 2u, 2u, 8u, 13u, 3u, 1u, 1u, 0u, 0u, 1u, 6u, 5u, 1u, \
	1u, 23u, 9u, 20u, 13u, 18u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 5u, 4u, 3u, 1u, 1u, 1u, 27u, 22u, 2u, 3u, 3u, 3u, 3u, \
	1u, 3u, 3u, 2u, 1u, 14u, 32u, 6u, 20u, 6u, 3u, 1u, 3u, 3u, 3u, 11u, 6u, 1u, 1u, 0u, 0u, 2u, 8u, 8u, 1u, 1u, 24u, \
	5u, 21u, 18u, 19u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 6u, 5u, 4u, 1u, 1u, 1u, 5u, 22u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, \
	3u, 2u, 1u, 5u, 33u, 6u, 4u, 6u, 3u, 1u, 3u, 3u, 3u, 5u, 6u, 1u, 1u, 0u, 0u, 2u, 8u, 8u, 1u, 1u, 24u, 5u, 21u, \
	18u, 19u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 1u, 7u, 6u, 5u, 1u, 1u, 1u, 5u, 22u, 2u, 3u, 3u, 3u, 3u, 1u, 3u, 3u, 2u, \
	1u, 5u, 33u, 6u, 4u, 6u, 3u, 1u, 3u, 3u, 3u, 5u, 6u, 1u, 1u, 0u, 0u, 2u, 8u, 8u, 1u, 1u, 24u, 5u, 21u, 18u, 19u, \
	1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 8u, 7u, 6u, 1u, 2u, 2u, 7u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 7u, \
	34u, 4u, 6u, 4u, 3u, 1u, 5u, 3u, 5u, 7u, 8u, 2u, 1u, 0u, 0u, 3u, 9u, 9u, 1u, 2u, 25u, 12u, 16u, 15u, 4u, 1u, 3u, \
	1u, 3u, 1u, 2u, 1u, 2u, 9u, 7u, 6u, 1u, 2u, 2u, 7u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 7u, 34u, 4u, \
	6u, 4u, 3u, 1u, 5u, 3u, 5u, 7u, 8u, 2u, 1u, 0u, 0u, 3u, 9u, 9u, 1u, 2u, 25u, 12u, 16u, 15u, 4u, 1u, 3u, 1u, 3u, \
	1u, 2u, 1u, 2u, 10u, 8u, 7u, 1u, 2u, 2u, 7u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 7u, 34u, 4u, 6u, 4u, \
	3u, 1u, 5u, 3u, 5u, 7u, 8u, 2u, 1u, 0u, 0u, 3u, 9u, 9u, 1u, 2u, 25u, 12u, 16u, 15u, 4u, 2u, 4u, 2u, 1u, 1u, 3u, \
	1u, 3u, 11u, 9u, 8u, 1u, 2u, 1u, 7u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 7u, 34u, 4u, 6u, 4u, 3u, 1u, \
	5u, 3u, 5u, 7u, 8u, 2u, 1u, 0u, 0u, 3u, 9u, 9u, 1u, 2u, 25u, 12u, 16u, 15u, 4u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, \
	12u, 10u, 9u, 1u, 2u, 1u, 7u, 17u, 2u, 4u, 3u, 3u, 3u, 1u, 4u, 3u, 2u, 1u, 7u, 34u, 4u, 6u, 4u, 3u, 1u, 5u, 3u, \
	5u, 7u, 8u, 2u, 1u, 0u, 0u, 3u, 4u, 3u, 1u, 2u, 26u, 5u, 16u, 15u, 4u, 2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 13u, 10u, \
	9u, 1u, 2u, 1u, 31u, 23u, 3u, 5u, 4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, 8u, 35u, 2u, 6u, 8u, 10u, 3u, 5u, 1u, 5u, 8u, \
	1u, 2u, 1u, 0u, 0u, 3u, 4u, 3u, 1u, 2u, 26u, 5u, 16u, 15u, 4u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, \
	2u, 1u, 21u, 24u, 3u, 5u, 4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, 16u, 36u, 7u, 15u, 7u, 3u, 1u, 6u, 1u, 6u, 8u, 1u, 2u, \
	1u, 0u, 0u, 3u, 4u, 3u, 1u, 2u, 26u, 5u, 16u, 15u, 4u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, \
	32u, 24u, 3u, 6u, 4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, 17u, 36u, 7u, 15u, 7u, 3u, 1u, 6u, 1u, 6u, 8u, 1u, 2u, 1u, 0u, \
	0u, 3u, 4u, 3u, 1u, 2u, 26u, 5u, 16u, 15u, 4u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 22u, 24u, \
	3u, 6u, 4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, 17u, 17u, 7u, 16u, 7u, 3u, 1u, 6u, 1u, 6u, 12u, 1u, 2u, 1u, 0u, 0u, 4u, \
	10u, 10u, 1u, 2u, 27u, 14u, 22u, 19u, 6u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, 2u, 1u, 33u, 25u, 3u, 6u, \
	4u, 4u, 4u, 1u, 9u, 3u, 2u, 2u, 8u, 37u, 11u, 17u, 13u, 11u, 6u, 6u, 1u, 9u, 12u, 1u, 2u, 1u, 0u, 0u, 4u, 10u, 10u, \
	1u, 2u, 27u, 14u, 22u, 19u, 6u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 33u, 25u, 3u, 6u, 4u, 4u, \
	4u, 1u, 9u, 3u, 2u, 2u, 8u, 37u, 11u, 17u, 13u, 11u, 6u, 6u, 1u, 9u, 12u, 1u, 2u, 1u, 0u, 0u, 4u, 10u, 10u, 1u, 2u, \
	27u, 14u, 22u, 19u, 6u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 12u, 9u, 4u, 1u, 5u, 5u, 4u, 2u, \
	10u, 3u, 2u, 2u, 11u, 10u, 7u, 9u, 7u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 28u, 8u, \
	23u, 20u, 20u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 12u, 9u, 4u, 1u, 5u, 4u, 4u, 2u, 8u, 4u, \
	2u, 2u, 11u, 10u, 7u, 9u, 7u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 28u, 8u, 23u, 20u, \
	20u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 3u, 1u, 14u, 10u, 4u, 1u, 5u, 4u, 4u, 2u, 8u, 4u, 2u, 2u, \
	13u, 12u, 2u, 11u, 8u, 4u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 28u, 8u, 23u, 20u, 20u, 2u, \
	5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 11u, 10u, 1u, 3u, 1u, 14u, 11u, 4u, 1u, 5u, 4u, 4u, 2u, 8u, 4u, 2u, 2u, 13u, 12u, \
	8u, 11u, 9u, 5u, 2u, 7u, 4u, 7u, 10u, 7u, 3u, 2u, 0u, 0u, 4u, 7u, 7u, 1u, 2u, 28u, 8u, 23u, 20u, 20u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 34u, 26u, 5u, 7u, 4u, 6u, 1u, 3u, 11u, 6u, 1u, 1u, 21u, 38u, 5u, 24u, 5u, 11u, \
	6u, 5u, 3u, 10u, 14u, 4u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 2u, 1u, 1u, 1u, 1u, 1u, 35u, 27u, 5u, 8u, 7u, 7u, 1u, 3u, 12u, 6u, 1u, 1u, 22u, 39u, 5u, 25u, 5u, 11u, 6u, 8u, \
	1u, 11u, 14u, 6u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, \
	1u, 1u, 1u, 1u, 1u, 34u, 28u, 5u, 9u, 8u, 8u, 1u, 3u, 12u, 6u, 1u, 1u, 21u, 40u, 6u, 24u, 6u, 12u, 6u, 5u, 3u, 10u, \
	14u, 6u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 2u, \
	1u, 1u, 1u, 36u, 29u, 5u, 10u, 9u, 8u, 2u, 1u, 11u, 6u, 1u, 1u, 21u, 41u, 6u, 26u, 6u, 12u, 6u, 5u, 3u, 10u, 15u, 4u, \
	2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 2u, 1u, 1u, \
	1u, 37u, 30u, 5u, 11u, 10u, 8u, 2u, 1u, 13u, 7u, 1u, 1u, 8u, 42u, 3u, 7u, 3u, 13u, 6u, 6u, 1u, 9u, 8u, 6u, 2u, 1u, \
	1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 14u, 12u, 3u, 1u, 1u, 1u, 34u, \
	28u, 5u, 12u, 11u, 9u, 2u, 1u, 11u, 6u, 1u, 1u, 21u, 40u, 6u, 24u, 6u, 12u, 6u, 5u, 3u, 10u, 14u, 6u, 2u, 1u, 1u, 1u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 15u, 13u, 4u, 1u, 1u, 1u, 34u, 28u, 5u, \
	13u, 12u, 9u, 2u, 1u, 11u, 6u, 1u, 1u, 21u, 40u, 6u, 24u, 6u, 12u, 6u, 5u, 3u, 10u, 14u, 6u, 2u, 1u, 1u, 1u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 1u, 7u, 6u, 5u, 1u, 1u, 1u, 38u, 31u, 6u, 14u, 13u, \
	10u, 3u, 3u, 14u, 6u, 2u, 1u, 8u, 34u, 1u, 6u, 14u, 13u, 6u, 5u, 5u, 10u, 8u, 9u, 2u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 8u, 7u, 6u, 1u, 1u, 2u, 39u, 32u, 6u, 15u, 14u, 11u, 3u, \
	4u, 4u, 6u, 2u, 1u, 21u, 43u, 1u, 27u, 14u, 13u, 6u, 6u, 6u, 9u, 14u, 9u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 1u, 3u, 1u, 3u, 1u, 2u, 1u, 2u, 9u, 7u, 6u, 1u, 1u, 2u, 35u, 32u, 6u, 16u, 15u, 11u, 3u, 3u, 4u, \
	6u, 2u, 1u, 22u, 44u, 1u, 25u, 14u, 13u, 6u, 8u, 6u, 11u, 14u, 9u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 1u, 3u, 1u, 3u, 1u, 2u, 1u, 2u, 10u, 8u, 7u, 1u, 1u, 2u, 40u, 32u, 6u, 17u, 16u, 11u, 3u, 3u, 15u, 6u, 2u, \
	1u, 10u, 44u, 1u, 25u, 14u, 14u, 1u, 9u, 6u, 12u, 14u, 9u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 4u, 2u, 1u, 1u, 3u, 1u, 3u, 11u, 9u, 8u, 1u, 1u, 1u, 41u, 33u, 6u, 18u, 17u, 11u, 3u, 3u, 4u, 8u, 2u, 1u, 10u, \
	2u, 12u, 2u, 15u, 15u, 1u, 9u, 7u, 12u, 16u, 10u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 4u, \
	2u, 1u, 1u, 3u, 1u, 3u, 12u, 10u, 9u, 1u, 1u, 1u, 34u, 34u, 6u, 15u, 12u, 12u, 3u, 3u, 15u, 6u, 2u, 1u, 21u, 45u, 5u, \
	24u, 16u, 16u, 6u, 5u, 5u, 10u, 14u, 9u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 4u, 2u, 1u, \
	1u, 3u, 1u, 3u, 13u, 10u, 9u, 1u, 1u, 1u, 42u, 35u, 7u, 19u, 12u, 12u, 5u, 3u, 4u, 6u, 2u, 2u, 23u, 46u, 13u, 28u, 17u, \
	15u, 1u, 10u, 3u, 13u, 14u, 6u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 4u, 3u, 1u, 1u, 4u, \
	2u, 3u, 13u, 10u, 9u, 1u, 1u, 1u, 34u, 35u, 7u, 20u, 4u, 4u, 4u, 3u, 4u, 6u, 2u, 2u, 21u, 45u, 13u, 24u, 17u, 16u, 6u, \
	5u, 3u, 10u, 14u, 6u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, \
	13u, 10u, 9u, 1u, 1u, 1u, 34u, 35u, 7u, 20u, 4u, 4u, 4u, 3u, 4u, 6u, 2u, 2u, 21u, 45u, 13u, 24u, 17u, 16u, 6u, 5u, 3u, \
	10u, 14u, 6u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, \
	9u, 1u, 1u, 1u, 34u, 35u, 7u, 20u, 4u, 4u, 4u, 3u, 4u, 6u, 2u, 2u, 21u, 45u, 13u, 24u, 17u, 16u, 6u, 5u, 3u, 10u, 14u, \
	6u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 4u, 3u, 1u, 1u, 4u, 2u, 3u, 13u, 10u, 9u, 1u, \
	1u, 1u, 34u, 35u, 7u, 20u, 4u, 4u, 4u, 3u, 4u, 6u, 2u, 2u, 21u, 45u, 13u, 24u, 17u, 16u, 6u, 5u, 3u, 10u, 14u, 6u, 2u, \
	1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 1u, 1u, \
	34u, 35u, 7u, 20u, 4u, 4u, 4u, 3u, 4u, 6u, 2u, 2u, 21u, 45u, 13u, 24u, 17u, 16u, 6u, 5u, 3u, 10u, 14u, 6u, 2u, 1u, 2u, \
	1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 1u, 1u, 34u, 36u, \
	7u, 20u, 4u, 4u, 4u, 3u, 4u, 6u, 2u, 2u, 21u, 45u, 2u, 24u, 18u, 16u, 6u, 5u, 8u, 10u, 14u, 11u, 2u, 1u, 2u, 1u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 1u, 1u, 34u, 35u, 7u, 20u, \
	4u, 4u, 4u, 3u, 4u, 6u, 2u, 2u, 21u, 45u, 13u, 24u, 17u, 16u, 6u, 5u, 3u, 10u, 14u, 6u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 10u, 9u, 1u, 1u, 1u, 43u, 35u, 7u, 20u, 4u, 4u, \
	4u, 3u, 4u, 6u, 2u, 2u, 23u, 47u, 13u, 25u, 17u, 16u, 6u, 11u, 3u, 11u, 14u, 6u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 2u, 5u, 4u, 4u, 2u, 5u, 3u, 4u, 13u, 11u, 10u, 1u, 1u, 1u, 43u, 35u, 7u, 20u, 4u, 4u, 4u, 3u, \
	4u, 6u, 2u, 2u, 23u, 47u, 13u, 25u, 17u, 16u, 6u, 11u, 3u, 11u, 14u, 6u, 2u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, \
}

/* Item 1 COMMON_BW pre-rows, EPA boards, BW160 / BW320 (V:e0051de2 -> V:e0051536, 8 rows).
 * MediaTek WM table (V:e0051536 format): rows 0xe007c480 (u16 index for P[2] = 0, u16 index for
 * P[2] != 0, u16 value index), values 0xe007c288. X(register for P[2] = 0, register for P[2] != 0,
 * value). [static] */
#define RFCHAN_EPA_BW_PRE_ROWS(X) \
	X(ADIE_CHAIN_2BF, ADIE_CHAIN_2C6, 0x94a52948u) \
	X(ADIE_CHAIN_2C0, ADIE_CHAIN_2C7, 0x94a52948u) \
	X(ADIE_CHAIN_2C4, ADIE_CHAIN_2CB, 0x94a52948u) \
	X(ADIE_CHAIN_2C5, ADIE_CHAIN_2CC, 0x94a52948u) \
	X(ADIE_CHAIN_24A, ADIE_CHAIN_25C, 0xffffcf00u) \
	X(ADIE_CHAIN_24B, ADIE_CHAIN_25D, 0x18080400u) \
	X(ADIE_CHAIN_24C, ADIE_CHAIN_25E, 0x00807fe0u) \
	X(ADIE_CHAIN_24F, ADIE_CHAIN_261, 0x00800000u)

/* Item 1 COMMON_BW table, EPA boards, BW160 / BW320 (V:e0051de2).
 * MediaTek WM tables (V:e0051de2): descriptors 0xe007b8c4, register indices 0xe007c208, values 0xe007c288, column
 * map 0xe007baa4 (u8 [key][group]), keys 0xe007c1ac (u8, 90). Key value = BW group * 5 + WF + (BW320) * 0x5a.
 * Rows in walk order: X(register for P[2] != 1, register for P[2] = 1, then one value per column of the row's group); the column of group g
 * is COLMAP[key][g], key = the position of the key value in KEYS. RFCHAN_SKIP = no write. [static] */
#define RFCHAN_EPA_BW_NGROUPS 20u
#define RFCHAN_EPA_BW_NKEYS 90u
#define RFCHAN_EPA_BW_GROUPS(X) \
	X(5u, 2u) \
	X(2u, 4u) \
	X(1u, 3u) \
	X(1u, 4u) \
	X(2u, 2u) \
	X(3u, 6u) \
	X(1u, 7u) \
	X(1u, 5u) \
	X(1u, 10u) \
	X(1u, 8u) \
	X(2u, 2u) \
	X(2u, 3u) \
	X(1u, 7u) \
	X(1u, 6u) \
	X(3u, 5u) \
	X(1u, 17u) \
	X(1u, 12u) \
	X(1u, 14u) \
	X(1u, 11u) \
	X(1u, 4u)
#define RFCHAN_EPA_BW_ROWS(X) \
	X(ADIE_CHAIN_2A7, ADIE_CHAIN_2AE, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2A8, ADIE_CHAIN_2AF, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2AB, ADIE_CHAIN_2B2, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2AC, ADIE_CHAIN_2B3, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2AD, ADIE_CHAIN_2B4, 0x5294a528u, 0x6318c630u) \
	X(ADIE_CHAIN_2A9, ADIE_CHAIN_2B0, 0x5294a18cu, 0x5294a294u, 0x6318c9ccu, 0x6318c39cu) \
	X(ADIE_CHAIN_2AA, ADIE_CHAIN_2B1, 0x18d4a528u, 0x2954a528u, 0x9cd8c630u, 0x39d8c630u) \
	X(ADIE_CHAIN_2C1, ADIE_CHAIN_2C8, 0x94a52840u, 0x94a52948u, 0x94a52528u) \
	X(ADIE_CHAIN_2C2, ADIE_CHAIN_2C9, 0x841ce738u, 0x84210840u, 0x94a52948u, 0x52a52948u) \
	X(ADIE_CHAIN_2C3, ADIE_CHAIN_2CA, 0x84210948u, 0x94a52948u) \
	X(ADIE_CHAIN_24E, ADIE_CHAIN_260, 0x18080020u, 0x10080020u) \
	X(ADIE_CHAIN_2E9, ADIE_CHAIN_2EB, 0xffffffffu, 0xeeeeeeeeu, 0xccccccccu, 0x66666666u, 0x00000000u, 0x99999999u) \
	X(ADIE_CHAIN_2ED, ADIE_CHAIN_2EF, 0xffffffffu, 0xeeeeeeeeu, 0xccccccccu, 0x66666666u, 0x00000000u, 0x99999999u) \
	X(ADIE_CHAIN_2EE, ADIE_CHAIN_2F0, 0xffffffffu, 0xeeeeeeeeu, 0xccccccccu, 0x66666666u, 0x00000000u, 0x99999999u) \
	X(ADIE_CHAIN_2EA, ADIE_CHAIN_2EC, 0xffffffffu, 0xeeeeeeeeu, 0xccccccccu, 0x66666666u, 0x20000000u, 0x00000000u, 0x99999999u) \
	X(ADIE_CHAIN_2F9, ADIE_CHAIN_2FC, 0x842108c8u, 0x84210840u, 0x5294a528u, 0x294a5294u, 0x00000000u) \
	X(ADIE_CHAIN_2FA, ADIE_CHAIN_2FD, 0x842108c8u, 0x5294a5b8u, 0x294a53a0u, 0x00000190u, 0x00000004u, 0x00004210u, 0x0000518cu, 0x00005294u, 0x294a23a0u, 0x00005210u) \
	X(ADIE_CHAIN_2FB, ADIE_CHAIN_2FE, 0x9ce94000u, 0x739ef528u, 0x4a94a294u, 0x294c6000u, 0x10842000u, 0x18c63000u, 0x294a5000u, 0x21084000u) \
	X(ADIE_CHAIN_29E, ADIE_CHAIN_29E, 0x68888888u, 0x78888888u) \
	X(ADIE_CHAIN_29F, ADIE_CHAIN_29F, 0x68888888u, 0x78888888u) \
	X(ADIE_CHAIN_2FF, ADIE_CHAIN_302, 0x7a084210u, 0x00000000u, 0x7bd0c420u) \
	X(ADIE_CHAIN_301, ADIE_CHAIN_304, 0x21084000u, 0x00000000u, 0x42108000u) \
	X(ADIE_CHAIN_300, ADIE_CHAIN_303, 0x21108210u, 0x211e8210u, 0x00000000u, 0x421ef420u, 0x421e8420u, 0x42108420u, 0x421e4420u) \
	X(ADIE_CHAIN_26E, MT7976_TXTRIM_SRC(6u), 0x03807000u, 0x0700e000u, 0x0780f000u, 0x0f01e000u, 0x0f81f000u, 0x0780e000u) \
	X(ADIE_CHAIN_26F, ADIE_CHAIN_287, 0x03807000u, 0x0700e000u, 0x0780f000u, 0x0f01e000u, 0x0f81f000u) \
	X(ADIE_CHAIN_270, ADIE_CHAIN_288, 0x03803000u, 0x07003800u, 0x07806000u, 0x0f006800u, 0x0f807000u) \
	X(ADIE_CHAIN_271, ADIE_CHAIN_289, 0x01801000u, 0x01c01400u, 0x03001800u, 0x03401c00u, 0x03803000u) \
	X(ADIE_CHAIN_272, ADIE_CHAIN_28A, 0x03406800u, 0x03806c00u, 0x07007000u, 0x03406400u, 0x0780e800u, 0x07c0e000u, 0x0700cc00u, 0x0740e400u, 0x0f41e400u, 0x1f81f400u, 0x1fc3ec00u, 0x03406c00u, 0x0780f000u, 0x07a0f000u, 0x0700e000u, 0x0f81e000u, 0x1f83e000u) \
	X(ADIE_CHAIN_273, ADIE_CHAIN_28B, 0x02404800u, 0x02606c00u, 0x03806400u, 0x0720c400u, 0x06607000u, 0x0f61c000u, 0x1f21e000u, 0x1f03cc00u, 0x0700c000u, 0x0780e000u, 0x0f81e000u, 0x1f81f000u) \
	X(ADIE_CHAIN_274, ADIE_CHAIN_28C, 0x01602800u, 0x01602c00u, 0x01402800u, 0x01a03000u, 0x06003000u, 0x03602c00u, 0x03603000u, 0x07604c00u, 0x0e406400u, 0x0fa07000u, 0x06603800u, 0x05603000u, 0x0e206000u, 0x0f806c00u) \
	X(ADIE_CHAIN_275, ADIE_CHAIN_28D, 0x01400c00u, 0x01600c00u, 0x01601000u, 0x01801000u, 0x01e01800u, 0x03002800u, 0x03602c00u, 0x01a01400u, 0x00e01000u, 0x03001800u, 0x03401c00u) \
	X(ADIE_CHAIN_24D, ADIE_CHAIN_25F, 0xff9fc700u, 0xff9fc600u, 0xff9e0600u, 0xff9f0600u)
#define RFCHAN_EPA_BW_KEYS { \
	0u, 5u, 10u, 15u, 20u, 25u, 30u, 35u, 40u, 45u, 50u, 145u, 150u, 155u, 160u, 165u, \
	170u, 175u, 1u, 6u, 11u, 16u, 21u, 26u, 31u, 36u, 41u, 46u, 51u, 146u, 151u, 156u, \
	161u, 166u, 171u, 176u, 2u, 7u, 12u, 17u, 22u, 27u, 32u, 37u, 42u, 47u, 52u, 147u, \
	152u, 157u, 162u, 167u, 172u, 177u, 3u, 8u, 13u, 18u, 23u, 28u, 33u, 38u, 43u, 48u, \
	53u, 148u, 153u, 158u, 163u, 168u, 173u, 178u, 4u, 9u, 14u, 19u, 24u, 29u, 34u, 39u, \
	44u, 49u, 54u, 149u, 154u, 159u, 164u, 169u, 174u, 179u, \
}
#define RFCHAN_EPA_BW_COLMAP { \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, \
	0u, 0u, 0u, 0u, 0u, 2u, 2u, 2u, 1u, 1u, 0u, 0u, 1u, 0u, 0u, 2u, 1u, 1u, 1u, 1u, \
	0u, 0u, 0u, 0u, 0u, 3u, 3u, 3u, 2u, 2u, 0u, 1u, 2u, 0u, 0u, 3u, 2u, 2u, 0u, 2u, \
	0u, 1u, 0u, 1u, 0u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 4u, 3u, 3u, 2u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 4u, 1u, 1u, 5u, 3u, 4u, 3u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 6u, 4u, 5u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 7u, 3u, 6u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 6u, 5u, 1u, 2u, 4u, 3u, 3u, 8u, 5u, 7u, 4u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 9u, 6u, 8u, 5u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 10u, 7u, 9u, 6u, 3u, \
	0u, 0u, 0u, 0u, 0u, 5u, 6u, 3u, 8u, 2u, 0u, 1u, 2u, 0u, 0u, 11u, 2u, 2u, 0u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 12u, 8u, 4u, 3u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 5u, 5u, 1u, 13u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 14u, 8u, 11u, 8u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 12u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 9u, 7u, 1u, 2u, 6u, 3u, 3u, 15u, 10u, 12u, 9u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 6u, 4u, 4u, 16u, 11u, 13u, 10u, 2u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, \
	0u, 0u, 0u, 0u, 0u, 2u, 2u, 2u, 1u, 1u, 0u, 0u, 1u, 0u, 0u, 2u, 1u, 1u, 1u, 1u, \
	0u, 0u, 0u, 0u, 0u, 3u, 3u, 3u, 2u, 2u, 0u, 1u, 2u, 0u, 0u, 3u, 2u, 2u, 0u, 2u, \
	0u, 1u, 0u, 1u, 0u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 4u, 3u, 3u, 2u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 4u, 1u, 1u, 5u, 3u, 4u, 3u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 6u, 4u, 5u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 7u, 3u, 6u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 6u, 5u, 1u, 2u, 4u, 3u, 3u, 8u, 5u, 7u, 4u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 9u, 6u, 8u, 5u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 10u, 7u, 9u, 6u, 3u, \
	0u, 0u, 0u, 0u, 0u, 5u, 6u, 3u, 8u, 2u, 0u, 1u, 2u, 0u, 0u, 11u, 2u, 2u, 0u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 12u, 8u, 4u, 3u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 5u, 5u, 1u, 13u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 14u, 8u, 11u, 8u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 12u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 9u, 7u, 1u, 2u, 6u, 3u, 3u, 15u, 10u, 12u, 9u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 6u, 4u, 4u, 16u, 11u, 13u, 10u, 2u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, \
	0u, 0u, 0u, 0u, 0u, 2u, 2u, 2u, 1u, 1u, 0u, 0u, 1u, 0u, 0u, 2u, 1u, 1u, 1u, 1u, \
	0u, 0u, 0u, 0u, 0u, 3u, 3u, 3u, 2u, 2u, 0u, 1u, 2u, 0u, 0u, 3u, 2u, 2u, 0u, 2u, \
	0u, 1u, 0u, 1u, 0u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 4u, 3u, 3u, 2u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 4u, 1u, 1u, 5u, 3u, 4u, 3u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 6u, 4u, 5u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 7u, 3u, 6u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 6u, 5u, 1u, 2u, 4u, 3u, 3u, 8u, 5u, 7u, 4u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 9u, 6u, 8u, 5u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 10u, 7u, 9u, 6u, 3u, \
	0u, 0u, 0u, 0u, 0u, 5u, 6u, 3u, 8u, 2u, 0u, 1u, 2u, 0u, 0u, 11u, 2u, 2u, 0u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 12u, 8u, 4u, 3u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 5u, 5u, 1u, 13u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 14u, 8u, 11u, 8u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 12u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 9u, 7u, 1u, 2u, 6u, 3u, 3u, 15u, 10u, 12u, 9u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 6u, 4u, 4u, 16u, 11u, 13u, 10u, 2u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, \
	0u, 0u, 0u, 0u, 0u, 2u, 2u, 2u, 1u, 1u, 0u, 0u, 1u, 0u, 0u, 2u, 1u, 1u, 1u, 1u, \
	0u, 0u, 0u, 0u, 0u, 3u, 3u, 3u, 2u, 2u, 0u, 1u, 2u, 0u, 0u, 3u, 2u, 2u, 0u, 2u, \
	0u, 1u, 0u, 1u, 0u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 4u, 3u, 3u, 2u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 4u, 1u, 1u, 5u, 3u, 4u, 3u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 6u, 4u, 5u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 7u, 3u, 6u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 6u, 5u, 1u, 2u, 4u, 3u, 3u, 8u, 5u, 7u, 4u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 9u, 6u, 8u, 5u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 10u, 7u, 9u, 6u, 3u, \
	0u, 0u, 0u, 0u, 0u, 5u, 6u, 3u, 8u, 2u, 0u, 1u, 2u, 0u, 0u, 11u, 2u, 2u, 0u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 12u, 8u, 4u, 3u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 5u, 5u, 1u, 13u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 14u, 8u, 11u, 8u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 12u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 9u, 7u, 1u, 2u, 6u, 3u, 3u, 15u, 10u, 12u, 9u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 6u, 4u, 4u, 16u, 11u, 13u, 10u, 2u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, \
	0u, 0u, 0u, 0u, 0u, 2u, 2u, 2u, 1u, 1u, 0u, 0u, 1u, 0u, 0u, 2u, 1u, 1u, 1u, 1u, \
	0u, 0u, 0u, 0u, 0u, 3u, 3u, 3u, 2u, 2u, 0u, 1u, 2u, 0u, 0u, 3u, 2u, 2u, 0u, 2u, \
	0u, 1u, 0u, 1u, 0u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 4u, 3u, 3u, 2u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 4u, 1u, 1u, 5u, 3u, 4u, 3u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 6u, 4u, 5u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 7u, 3u, 6u, 2u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 6u, 5u, 1u, 2u, 4u, 3u, 3u, 8u, 5u, 7u, 4u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 4u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 9u, 6u, 8u, 5u, 2u, \
	1u, 2u, 2u, 3u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 4u, 4u, 4u, 10u, 7u, 9u, 6u, 3u, \
	0u, 0u, 0u, 0u, 0u, 5u, 6u, 3u, 8u, 2u, 0u, 1u, 2u, 0u, 0u, 11u, 2u, 2u, 0u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 3u, 1u, 1u, 12u, 8u, 4u, 3u, 2u, \
	0u, 1u, 1u, 2u, 1u, 4u, 4u, 4u, 3u, 3u, 0u, 2u, 5u, 5u, 1u, 13u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 4u, 4u, 0u, 1u, 2u, 1u, 1u, 14u, 8u, 11u, 8u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 5u, 5u, 0u, 1u, 2u, 2u, 2u, 12u, 9u, 10u, 7u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 9u, 7u, 1u, 2u, 6u, 3u, 3u, 15u, 10u, 12u, 9u, 2u, \
	1u, 3u, 1u, 2u, 1u, 4u, 5u, 4u, 7u, 6u, 1u, 2u, 6u, 4u, 4u, 16u, 11u, 13u, 10u, 2u, \
}

/* Item 1 COMMON_BW table, boards without EPA, BW160 / BW320 (V:e0051e6a).
 * MediaTek WM tables (V:e0051e6a): descriptors 0xe007b150, register indices 0xe007b6cc, values 0xe007b734, column
 * map 0xe007b278 (u8 [key][group]), keys 0xe007b674 (u8, 85). Key value = BW group * 5 + WF + (BW320) * 0x55.
 * Rows in walk order: X(register for P[2] != 1, register for P[2] = 1, then one value per column of the row's group); the column of group g
 * is COLMAP[key][g], key = the position of the key value in KEYS. RFCHAN_SKIP = no write. [static] */
#define RFCHAN_NOEPA_BW_NGROUPS 12u
#define RFCHAN_NOEPA_BW_NKEYS 85u
#define RFCHAN_NOEPA_BW_GROUPS(X) \
	X(1u, 2u) \
	X(1u, 5u) \
	X(1u, 4u) \
	X(2u, 4u) \
	X(6u, 2u) \
	X(2u, 5u) \
	X(1u, 3u) \
	X(1u, 4u) \
	X(4u, 9u) \
	X(3u, 7u) \
	X(2u, 3u) \
	X(2u, 2u)
#define RFCHAN_NOEPA_BW_ROWS(X) \
	X(ADIE_CHAIN_145, ADIE_CHAIN_145, 0x64400b64u, 0x64400fe4u) \
	X(ADIE_CHAIN_2A7, ADIE_CHAIN_2AE, 0xf4a52948u, 0xf49d2948u, 0xf48f2948u, 0xfca52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2A8, ADIE_CHAIN_2AF, 0x94bc739cu, 0x94be739cu, 0x94bf18c4u, 0xfffffffcu) \
	X(ADIE_CHAIN_2A9, ADIE_CHAIN_2B0, 0x94a52948u, 0x94a52738u, 0x94a5239cu, 0xfffffffcu) \
	X(ADIE_CHAIN_2AA, ADIE_CHAIN_2B1, 0x94a52948u, 0x73a52948u, 0x39e52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2AB, ADIE_CHAIN_2B2, 0x94a52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2AC, ADIE_CHAIN_2B3, 0x94a52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2AD, ADIE_CHAIN_2B4, 0x94a52948u, 0xfffffffcu) \
	X(ADIE_CHAIN_2C3, ADIE_CHAIN_2CA, 0x84210840u, 0xbdef7bdcu) \
	X(ADIE_CHAIN_2C4, ADIE_CHAIN_2CB, 0x84210840u, 0xbdef7bdcu) \
	X(ADIE_CHAIN_2C5, ADIE_CHAIN_2CC, 0x84210840u, 0xbdef7ffcu) \
	X(ADIE_CHAIN_2BF, ADIE_CHAIN_2C6, 0x84350840u, 0x837f0840u, 0x84310840u, 0xc6310840u, 0xffff7bdcu) \
	X(ADIE_CHAIN_2C1, ADIE_CHAIN_2C8, 0x84210d68u, 0x6b5adffcu, 0x84210c60u, 0xc6318c60u, 0xfffffffcu) \
	X(ADIE_CHAIN_2C0, ADIE_CHAIN_2C7, 0x8421bdecu, 0x8431fffcu, 0xbdfffffcu) \
	X(ADIE_CHAIN_2C2, ADIE_CHAIN_2C9, 0xd6a10840u, 0xffe10840u, 0xc6210840u, 0xffef7bdcu) \
	X(ADIE_CHAIN_2E9, ADIE_CHAIN_2EB, 0xddddddddu, 0x8cccccccu, 0xccccccccu, 0x66666666u, 0x00444444u, 0x00222222u, 0x00000000u, 0x00333333u, 0x11111111u) \
	X(ADIE_CHAIN_2EA, ADIE_CHAIN_2EC, 0xddddddddu, 0x8cccccccu, 0xccccccccu, 0x66666666u, 0x00444444u, 0x00222222u, 0x00000000u, 0x00333333u, 0x11111111u) \
	X(ADIE_CHAIN_2ED, ADIE_CHAIN_2EF, 0xddddddddu, 0x8cccccccu, 0xccccccccu, 0x66666666u, 0x00444444u, 0x00222222u, 0x00000000u, 0x00333333u, 0x11111111u) \
	X(ADIE_CHAIN_2EE, ADIE_CHAIN_2F0, 0xddddddddu, 0x8cccccccu, 0xccccccccu, 0x66666666u, 0x00444444u, 0x00222222u, 0x00000000u, 0x00333333u, 0x11111111u) \
	X(ADIE_CHAIN_2F9, ADIE_CHAIN_2FC, 0xa5294a50u, 0x631f18c4u, 0x39d4c630u, 0x3192b5acu, 0x1086318cu, 0x00000014u, 0x210c6318u) \
	X(ADIE_CHAIN_2FA, ADIE_CHAIN_2FD, 0xa5294a50u, 0x8c58c7c4u, 0x630e7530u, 0x5acc64acu, 0x18c4218cu, 0x29400000u, 0x31884318u) \
	X(ADIE_CHAIN_2FB, ADIE_CHAIN_2FE, 0xa5294a50u, 0x8c6318c4u, 0x6318c630u, 0x5ad6b5acu, 0x18c6318cu, 0x014a5294u, 0x318c6318u) \
	X(ADIE_CHAIN_2FF, ADIE_CHAIN_302, 0x00000000u, 0x40000000u, 0x78000000u) \
	X(ADIE_CHAIN_300, ADIE_CHAIN_303, 0x00000000u, 0x00100000u, 0x001e0000u) \
	X(ADIE_CHAIN_26E, MT7976_TXTRIM_SRC(6u), 0xff9ff000u, 0xfffff000u) \
	X(ADIE_CHAIN_272, ADIE_CHAIN_28A, 0xff9ff000u, 0xfffff000u)
#define RFCHAN_NOEPA_BW_KEYS { \
	0u, 5u, 10u, 15u, 20u, 25u, 30u, 35u, 40u, 45u, 50u, 140u, 145u, 150u, 155u, 160u, \
	165u, 1u, 6u, 11u, 16u, 21u, 26u, 31u, 36u, 41u, 46u, 51u, 141u, 146u, 151u, 156u, \
	161u, 166u, 2u, 7u, 12u, 17u, 22u, 27u, 32u, 37u, 42u, 47u, 52u, 142u, 147u, 152u, \
	157u, 162u, 167u, 3u, 8u, 13u, 18u, 23u, 28u, 33u, 38u, 43u, 48u, 53u, 143u, 148u, \
	153u, 158u, 163u, 168u, 4u, 9u, 14u, 19u, 24u, 29u, 34u, 39u, 44u, 49u, 54u, 144u, \
	149u, 154u, 159u, 164u, 169u, \
}
#define RFCHAN_NOEPA_BW_COLMAP { \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 1u, 0u, 0u, \
	0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 3u, 2u, 0u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 4u, 3u, 1u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 5u, 4u, 1u, 0u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 7u, 6u, 1u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 8u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 1u, 0u, 0u, \
	0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 3u, 2u, 0u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 4u, 3u, 1u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 5u, 4u, 1u, 0u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 7u, 6u, 1u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 8u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 1u, 0u, 0u, \
	0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 3u, 2u, 0u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 4u, 3u, 1u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 5u, 4u, 1u, 0u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 7u, 6u, 1u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 8u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 1u, 0u, 0u, \
	0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 3u, 2u, 0u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 4u, 3u, 1u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 5u, 4u, 1u, 0u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 7u, 6u, 1u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 8u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 1u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 2u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 1u, 0u, 0u, \
	0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 3u, 2u, 0u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 4u, 3u, 0u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 5u, 4u, 0u, 0u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 6u, 5u, 0u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 0u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 0u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 0u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 0u, 0u, \
	0u, 2u, 0u, 2u, 0u, 1u, 0u, 1u, 7u, 6u, 0u, 1u, \
	1u, 3u, 1u, 0u, 0u, 2u, 0u, 2u, 8u, 5u, 0u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 0u, 0u, \
	1u, 3u, 2u, 0u, 0u, 3u, 1u, 2u, 6u, 5u, 0u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 0u, 0u, \
	1u, 4u, 3u, 3u, 1u, 4u, 2u, 3u, 6u, 5u, 0u, 0u, \
}

/* Item 1 external-LNA table (P[7] = ELNA; V:e0051fb8).
 * MediaTek WM tables (V:e0051fb8): descriptors 0xe007ad5c, register indices 0xe007ada0, values 0xe007adac, column
 * map 0xe007ad74 (u8 [key][group]), keys 0xe007ad94 (u8, 10). Key value = P[7] * 5 + WF.
 * Rows in walk order: X(register, then one value per column of the row's group); the column of group g
 * is COLMAP[key][g], key = the position of the key value in KEYS. RFCHAN_SKIP = no write. [static] */
#define RFCHAN_XLNA_NGROUPS 3u
#define RFCHAN_XLNA_NKEYS 10u
#define RFCHAN_XLNA_GROUPS(X) \
	X(1u, 2u) \
	X(1u, 2u) \
	X(1u, 3u)
#define RFCHAN_XLNA_ROWS(X) \
	X(MT7977_TTG_636, 0x00a86420u, 0x00642053u) \
	X(ADIE_RX_INPUT, 0x00000000u, 0xc0000000u) \
	X(ADIE_CHAIN_63A, 0x00033222u, 0x00033332u, 0x00022222u)
#define RFCHAN_XLNA_KEYS { \
	0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, \
}
#define RFCHAN_XLNA_COLMAP { \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 1u, 1u, 1u, 1u, 2u, 1u, 1u, 2u, 1u, 1u, 2u, \
	1u, 1u, 2u, 1u, 1u, 2u, \
}

/* Item 1 external-PA table (P[6] = EPA; V:e0051f78).
 * MediaTek WM tables (V:e0051f78): descriptors 0xe00814b0, register indices 0xe00814c0, values 0xe007ade0, column
 * map 0xe007adc8 (u8 [key][group]), keys 0xe007add4 (u8, 10). Key value = P[6] * 5 + WF.
 * Rows in walk order: X(register, then one value per column of the row's group); the column of group g
 * is COLMAP[key][g], key = the position of the key value in KEYS. RFCHAN_SKIP = no write. [static] */
#define RFCHAN_XPA_NGROUPS 1u
#define RFCHAN_XPA_NKEYS 10u
#define RFCHAN_XPA_GROUPS(X) \
	X(2u, 2u)
#define RFCHAN_XPA_ROWS(X) \
	X(ADIE_TX_201, 0x00000018u, 0x00000300u) \
	X(ADIE_TSSI_121, 0x08000000u, 0x00000000u)
#define RFCHAN_XPA_KEYS { \
	0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, \
}
#define RFCHAN_XPA_COLMAP { \
	0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, \
}

/* Item 1 group table by temperature class, EPA boards (V:e005232a).
 * MediaTek WM tables (V:e005232a): descriptors 0xe0081430, register indices 0xe0081440, values 0xe0075bac, column
 * map 0xe0075774 (u8 [key][group]), keys 0xe00758dc (u16, 360). Key value = group * 5 + WF + T * 0x78, T = 2 for P[8] = 1, 1 for P[8] = 2, else 0.
 * Rows in walk order: X(register for P[2] != 1, register for P[2] = 1, then one value per column of the row's group); the column of group g
 * is COLMAP[key][g], key = the position of the key value in KEYS. RFCHAN_SKIP = no write. [static] */
#define RFCHAN_EPA_GROUP_NGROUPS 1u
#define RFCHAN_EPA_GROUP_NKEYS 360u
#define RFCHAN_EPA_GROUP_GROUPS(X) \
	X(1u, 3u)
#define RFCHAN_EPA_GROUP_ROWS(X) \
	X(MT7977_CHAIN_146, MT7977_CHAIN_146, RFCHAN_SKIP, 0x20050ca0u, 0x200080a0u)
#define RFCHAN_EPA_GROUP_KEYS { \
	0u, 5u, 10u, 15u, 20u, 25u, 30u, 35u, 40u, 45u, 50u, 55u, 60u, 65u, 70u, 75u, \
	80u, 85u, 90u, 95u, 100u, 105u, 110u, 115u, 1u, 6u, 11u, 16u, 21u, 26u, 31u, 36u, \
	41u, 46u, 51u, 56u, 61u, 66u, 71u, 76u, 81u, 86u, 91u, 96u, 101u, 106u, 111u, 116u, \
	2u, 7u, 12u, 17u, 22u, 27u, 32u, 37u, 42u, 47u, 52u, 57u, 62u, 67u, 72u, 77u, \
	82u, 87u, 92u, 97u, 102u, 107u, 112u, 117u, 3u, 8u, 13u, 18u, 23u, 28u, 33u, 38u, \
	43u, 48u, 53u, 58u, 63u, 68u, 73u, 78u, 83u, 88u, 93u, 98u, 103u, 108u, 113u, 118u, \
	4u, 9u, 14u, 19u, 24u, 29u, 34u, 39u, 44u, 49u, 54u, 59u, 64u, 69u, 74u, 79u, \
	84u, 89u, 94u, 99u, 104u, 109u, 114u, 119u, 120u, 125u, 130u, 135u, 140u, 145u, 150u, 155u, \
	160u, 165u, 170u, 175u, 180u, 185u, 190u, 195u, 200u, 205u, 210u, 215u, 220u, 225u, 230u, 235u, \
	121u, 126u, 131u, 136u, 141u, 146u, 151u, 156u, 161u, 166u, 171u, 176u, 181u, 186u, 191u, 196u, \
	201u, 206u, 211u, 216u, 221u, 226u, 231u, 236u, 122u, 127u, 132u, 137u, 142u, 147u, 152u, 157u, \
	162u, 167u, 172u, 177u, 182u, 187u, 192u, 197u, 202u, 207u, 212u, 217u, 222u, 227u, 232u, 237u, \
	123u, 128u, 133u, 138u, 143u, 148u, 153u, 158u, 163u, 168u, 173u, 178u, 183u, 188u, 193u, 198u, \
	203u, 208u, 213u, 218u, 223u, 228u, 233u, 238u, 124u, 129u, 134u, 139u, 144u, 149u, 154u, 159u, \
	164u, 169u, 174u, 179u, 184u, 189u, 194u, 199u, 204u, 209u, 214u, 219u, 224u, 229u, 234u, 239u, \
	240u, 245u, 250u, 255u, 260u, 265u, 270u, 275u, 280u, 285u, 290u, 295u, 300u, 305u, 310u, 315u, \
	320u, 325u, 330u, 335u, 340u, 345u, 350u, 355u, 241u, 246u, 251u, 256u, 261u, 266u, 271u, 276u, \
	281u, 286u, 291u, 296u, 301u, 306u, 311u, 316u, 321u, 326u, 331u, 336u, 341u, 346u, 351u, 356u, \
	242u, 247u, 252u, 257u, 262u, 267u, 272u, 277u, 282u, 287u, 292u, 297u, 302u, 307u, 312u, 317u, \
	322u, 327u, 332u, 337u, 342u, 347u, 352u, 357u, 243u, 248u, 253u, 258u, 263u, 268u, 273u, 278u, \
	283u, 288u, 293u, 298u, 303u, 308u, 313u, 318u, 323u, 328u, 333u, 338u, 343u, 348u, 353u, 358u, \
	244u, 249u, 254u, 259u, 264u, 269u, 274u, 279u, 284u, 289u, 294u, 299u, 304u, 309u, 314u, 319u, \
	324u, 329u, 334u, 339u, 344u, 349u, 354u, 359u, \
}
#define RFCHAN_EPA_GROUP_COLMAP { \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, \
}

/* Item 1 group table by temperature class, boards without EPA (V:e00522e8).
 * MediaTek WM tables (V:e00522e8): descriptors 0xe0075bb8, register indices 0xe0076174, values 0xe0076180, column
 * map 0xe0075bd4 (u8 [key][group]), keys 0xe0075ea4 (u16, 360). Key value = group * 5 + WF + T * 0x78 (T as for the EPA table).
 * Rows in walk order: X(register for P[2] != 1, register for P[2] = 1, then one value per column of the row's group); the column of group g
 * is COLMAP[key][g], key = the position of the key value in KEYS. RFCHAN_SKIP = no write. [static] */
#define RFCHAN_NOEPA_GROUP_NGROUPS 2u
#define RFCHAN_NOEPA_GROUP_NKEYS 360u
#define RFCHAN_NOEPA_GROUP_GROUPS(X) \
	X(1u, 3u) \
	X(2u, 3u)
#define RFCHAN_NOEPA_GROUP_ROWS(X) \
	X(MT7977_CHAIN_146, MT7977_CHAIN_146, RFCHAN_SKIP, 0x20050ca0u, 0x200070a0u) \
	X(ADIE_CHAIN_2FF, ADIE_CHAIN_302, RFCHAN_SKIP, 0x78000000u, 0x00000000u) \
	X(ADIE_CHAIN_300, ADIE_CHAIN_303, RFCHAN_SKIP, 0x001e0000u, 0x00000000u)
#define RFCHAN_NOEPA_GROUP_KEYS { \
	0u, 5u, 10u, 15u, 20u, 25u, 30u, 35u, 40u, 45u, 50u, 55u, 60u, 65u, 70u, 75u, \
	80u, 85u, 90u, 95u, 100u, 105u, 110u, 115u, 1u, 6u, 11u, 16u, 21u, 26u, 31u, 36u, \
	41u, 46u, 51u, 56u, 61u, 66u, 71u, 76u, 81u, 86u, 91u, 96u, 101u, 106u, 111u, 116u, \
	2u, 7u, 12u, 17u, 22u, 27u, 32u, 37u, 42u, 47u, 52u, 57u, 62u, 67u, 72u, 77u, \
	82u, 87u, 92u, 97u, 102u, 107u, 112u, 117u, 3u, 8u, 13u, 18u, 23u, 28u, 33u, 38u, \
	43u, 48u, 53u, 58u, 63u, 68u, 73u, 78u, 83u, 88u, 93u, 98u, 103u, 108u, 113u, 118u, \
	4u, 9u, 14u, 19u, 24u, 29u, 34u, 39u, 44u, 49u, 54u, 59u, 64u, 69u, 74u, 79u, \
	84u, 89u, 94u, 99u, 104u, 109u, 114u, 119u, 120u, 125u, 130u, 135u, 140u, 145u, 150u, 155u, \
	160u, 165u, 170u, 175u, 180u, 185u, 190u, 195u, 200u, 205u, 210u, 215u, 220u, 225u, 230u, 235u, \
	121u, 126u, 131u, 136u, 141u, 146u, 151u, 156u, 161u, 166u, 171u, 176u, 181u, 186u, 191u, 196u, \
	201u, 206u, 211u, 216u, 221u, 226u, 231u, 236u, 122u, 127u, 132u, 137u, 142u, 147u, 152u, 157u, \
	162u, 167u, 172u, 177u, 182u, 187u, 192u, 197u, 202u, 207u, 212u, 217u, 222u, 227u, 232u, 237u, \
	123u, 128u, 133u, 138u, 143u, 148u, 153u, 158u, 163u, 168u, 173u, 178u, 183u, 188u, 193u, 198u, \
	203u, 208u, 213u, 218u, 223u, 228u, 233u, 238u, 124u, 129u, 134u, 139u, 144u, 149u, 154u, 159u, \
	164u, 169u, 174u, 179u, 184u, 189u, 194u, 199u, 204u, 209u, 214u, 219u, 224u, 229u, 234u, 239u, \
	240u, 245u, 250u, 255u, 260u, 265u, 270u, 275u, 280u, 285u, 290u, 295u, 300u, 305u, 310u, 315u, \
	320u, 325u, 330u, 335u, 340u, 345u, 350u, 355u, 241u, 246u, 251u, 256u, 261u, 266u, 271u, 276u, \
	281u, 286u, 291u, 296u, 301u, 306u, 311u, 316u, 321u, 326u, 331u, 336u, 341u, 346u, 351u, 356u, \
	242u, 247u, 252u, 257u, 262u, 267u, 272u, 277u, 282u, 287u, 292u, 297u, 302u, 307u, 312u, 317u, \
	322u, 327u, 332u, 337u, 342u, 347u, 352u, 357u, 243u, 248u, 253u, 258u, 263u, 268u, 273u, 278u, \
	283u, 288u, 293u, 298u, 303u, 308u, 313u, 318u, 323u, 328u, 333u, 338u, 343u, 348u, 353u, 358u, \
	244u, 249u, 254u, 259u, 264u, 269u, 274u, 279u, 284u, 289u, 294u, 299u, 304u, 309u, 314u, 319u, \
	324u, 329u, 334u, 339u, 344u, 349u, 354u, 359u, \
}
#define RFCHAN_NOEPA_GROUP_COLMAP { \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	0u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, \
	2u, 0u, 2u, 0u, 2u, 0u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	0u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, \
	2u, 0u, 2u, 0u, 2u, 0u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	0u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, \
	2u, 0u, 2u, 0u, 2u, 0u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	0u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, \
	2u, 0u, 2u, 0u, 2u, 0u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	0u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, 2u, 0u, \
	2u, 0u, 2u, 0u, 2u, 0u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, \
	0u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, 1u, 2u, \
}

#endif /* HW_ADIE_RFCHAN_H */
