/* SPDX-License-Identifier: MIT */
/*
 * The channel set's PHY tables and constants (MediaTek's channel flow
 * V:e00d708a; phy_cal.h "Channel set", phy_map.h "Channel-set PHY tables A
 * and B"): PHY tables A and B with their keys and column maps, the AGC table
 * entries, the gain-step tables, MediaTek's frequency-group and BW-group
 * ranges, and the procedure's constants. The registers and their fields are
 * named in phy.h (with the offset names of the table rows of unknown role),
 * phy_cal.h, mac.h, conn.h, mcu_map.h and adie.h.
 * Generated from MediaTek's WM image by the chanset port's gen.py (research
 * tree, radio-port/chanset/); do not edit by hand.
 *
 * ADDRESS SPACE: WF bus as the WM sees it (phy.h): per-chain registers at
 * 0x83000000 + band << 20 + chain << 16 + offset, band registers at
 * 0x83000000 + band << 20 + offset. Table rows give the band-0 address
 * (per-chain rows at their own chain); the walker adds band << 20 and, for
 * table A, moves a per-chain row to the walked chain.
 *
 * Confidence tags: [measured], [static], [inferred].
 */
#ifndef HW_MT7996_PHY_CHAN_H
#define HW_MT7996_PHY_CHAN_H

#include "../hw_bits.h"
#include "../adie/adie.h"
#include "phy.h"
#include "phy_cal.h"
#include "phy_map.h"

/* ========================================================================
 * Channel and frequency (the WM's conversion of the host's channel numbers,
 * V:e00e5f00, and its group function V:e00d41f6). [static]
 * ======================================================================== */

/* Channel -> centre frequency in kHz: base + 5000 kHz * channel, with
 * 2.4 GHz channel 14 = 2484 MHz and 6 GHz channel 2 = 5935 MHz. The band's
 * base is also the channel record's u32[0]. */
#define MT7996_CHSET_BASE_KHZ_2G4       2407000u
#define MT7996_CHSET_BASE_KHZ_5G        5000000u
#define MT7996_CHSET_BASE_KHZ_6G        5950000u
#define MT7996_CHSET_CH_KHZ             5000u
#define MT7996_CHSET_2G4_CH14           14u
#define MT7996_CHSET_2G4_CH14_KHZ       2484000u
#define MT7996_CHSET_6G_CH2             2u
#define MT7996_CHSET_6G_CH2_KHZ         5935000u
/* Channel numbers the host interface uses (mt76): */
#define MT7996_CHSET_2G4_CH_MIN         1u
#define MT7996_CHSET_2G4_CH_MAX         14u
#define MT7996_CHSET_5G_CH_MIN          36u
#define MT7996_CHSET_5G_CH_MAX          177u
#define MT7996_CHSET_6G_CH_MIN          1u
#define MT7996_CHSET_6G_CH_MAX          233u
#define MT7996_CHSET_KHZ_PER_MHZ        1000u

/* BW classes (MediaTek's P[4]): 0..4 = 20 / 40 / 80 / 160 / 320 MHz. */
#define MT7996_CHSET_BW20               0u
#define MT7996_CHSET_BW40               1u
#define MT7996_CHSET_BW80               2u
#define MT7996_CHSET_BW160              3u
#define MT7996_CHSET_BW320              4u
#define MT7996_CHSET_NBW                5u

/* Index of the primary 20 MHz channel within the bandwidth (V:e00e55b0, the
 * channel record's [0x10]): BW40 (primary >= centre); BW80 / 160 / 320
 * (primary - centre + OFS) >> 2, OFS 6 / 14 / 30; BW20 0. */
#define MT7996_CHSET_PRI_OFS(bw)        ((bw) == 2u ? 6u : (bw) == 3u ? 14u : 30u)
#define MT7996_CHSET_PRI_SHIFT          2u
#define MT7996_CHSET_PRI_MASK           HW_GENMASK(7, 0)
/* The primary of a centre channel at BW class bw lies 2 + 4 k channel
 * numbers from it, at most 2 << bw - 2 (20 MHz = 4 channel numbers). */
#define MT7996_CHSET_CH_PER_20MHZ       4u
#define MT7996_CHSET_PRI_HALF           2u	/* centre to the nearest primary */
#define MT7996_CHSET_PRI_MAX_DIST(bw)   ((2u << (bw)) - 2u)

/* Frequency groups (V:e00d41f6): from the integer MHz of the centre, group g
 * when lo <= MHz <= lo + span; 0 otherwise (2.4 GHz). Groups 1..8 are 5 GHz,
 * 9..23 6 GHz (the per-band RF record's type check, V:e00d406c). X(group,
 * lo MHz, span). */
#define MT7996_CHSET_GROUPS(X) \
	X( 1u, 4850u, 400u) \
	X( 2u, 5251u,  79u) \
	X( 3u, 5331u, 159u) \
	X( 4u, 5491u,  79u) \
	X( 5u, 5571u,  79u) \
	X( 6u, 5651u,  83u) \
	X( 7u, 5735u,  80u) \
	X( 8u, 5816u, 109u) \
	X( 9u, 5926u,  99u) \
	X(10u, 6026u,  79u) \
	X(11u, 6106u,  79u) \
	X(12u, 6186u,  79u) \
	X(13u, 6266u,  79u) \
	X(14u, 6346u,  79u) \
	X(15u, 6426u,  79u) \
	X(16u, 6506u,  79u) \
	X(17u, 6586u,  79u) \
	X(18u, 6666u,  79u) \
	X(19u, 6746u,  79u) \
	X(20u, 6826u,  79u) \
	X(21u, 6906u,  79u) \
	X(22u, 6986u,  79u) \
	X(23u, 7066u,  59u)
#define MT7996_CHSET_GROUP_5G_FIRST     1u
#define MT7996_CHSET_GROUP_5G_LAST      8u
#define MT7996_CHSET_GROUP_6G_FIRST     9u
#define MT7996_CHSET_GROUP_6G_LAST      23u

/* The host's stream counts in UniCmdChannelSwitch (channel record [0x12],
 * [0x13]); the stock host sends 4 / 4 on every band. */
#define MT7996_CHSET_NSS_STOCK          4u

/* The WF range (V:e00d6232, on this SKU: the configuration byte 0xe0093075
 * [2:0] != 7): WF 0..3, band 2 WF 0..4. [static] */
#define MT7996_CHSET_WF_LAST(band)      ((band) == 2u ? 4u : 3u)

/* ========================================================================
 * PHY tables A and B (V:e00d9238, V:e00d9702; phy_map.h). [static]
 * ======================================================================== */

/* The walker V:e00d917e moves a row address in 0x83000000..0x8303ffff to the
 * walked chain (table A); chain 0xf (table B) leaves it. */
#define MT7996_CHSET_TBL_RELOC_LO       0x83000000u
#define MT7996_CHSET_TBL_RELOC_HI       0x8303ffffu
#define MT7996_CHSET_TBL_RELOC_OFS      HW_GENMASK(15, 0)
#define MT7996_CHSET_TBL_NOCHAIN        0xfu
/* Table A's per-chain walks run on chains first WF..3; its key's class k0 is
 * 2 above 6585 MHz without EPA, k2 1 at 2484 MHz (V:e00d17da). */
#define MT7996_CHSET_TBL_A_LAST_CHAIN   3u
#define MT7996_CHSET_TBL_A_K0_2G4       0u
#define MT7996_CHSET_TBL_A_K0_5G6G      1u
#define MT7996_CHSET_TBL_A_K0_HI        2u	/* above 6585 MHz without EPA */
#define MT7996_CHSET_TBL_A_K0_HI_KHZ    6585000u
#define MT7996_CHSET_TBL_A_K2_CH14      1u	/* 2484 MHz */
#define MT7996_CHSET_TBL_A_K2_6G5G      2u
/* Table B's class k0 from the group (V:e00d1836): 0 for group 0, 1 for
 * 1..8 (and g' 0x18..0x1b), else 2; with k1 = 4 (BW320) k0 1 becomes 2; BW
 * class 5 counts as 2. */
#define MT7996_CHSET_TBL_B_K0_2G4       0u
#define MT7996_CHSET_TBL_B_K0_5G        1u
#define MT7996_CHSET_TBL_B_K0_6G        2u

/* Table A's explicit fields (V:e00d92c2..V:e00d9700; phy_map.h). With
 * MT7996_CBTOP_HW_REV != E1, MT7996_PHY_DPD_FORCE_TBL != 1 and EPA: */
#define MT7996_CHSET_DPD_FORCE_ON       1u
/* DFE gain compensation entries (phy_cal.h MT7996_PHY_DFE_GAIN_COMP; its
 * writer is this table A field step): entry 5 = 0xb0, entry 4 = 0x50
 * (+0xfdf8), entry 3 = 0x80, entry 2 = 0xa0 (+0xfdf4), in that order. */
#define MT7996_CHSET_GCOMP(X) \
	X(5u, 0x0b0u) \
	X(4u, 0x050u) \
	X(3u, 0x080u) \
	X(2u, 0x0a0u)
#define MT7996_CHSET_FD70_VAL           0x47655544u	/* whole word */
#define MT7996_CHSET_FF14_VAL           0x575757u
#define MT7996_CHSET_FF18_VAL           0x575754u
#define MT7996_CHSET_FF80_B1_VAL        0x51u
#define MT7996_CHSET_FF20_VAL           0x565656u
#define MT7996_CHSET_FF24_VAL           0x565653u
#define MT7996_CHSET_FF80_B0_VAL        0x50u
#define MT7996_CHSET_FF2C_VAL           0x525252u
#define MT7996_CHSET_FF30_VAL           0x52u
/* With E2 parts and EPA: TPC_MCS +0xe738 [18:16] = 3, then 3 in the four
 * 3-bit fields [2:0], [6:4], [10:8], [14:12] of +0xe738, +0xe73c, +0xe74c
 * (MediaTek's loop index arithmetic: offsets 0, 4, 0x14), then +0xe750,
 * +0xe758 (phy.h MT7996_PHY_DFE_E738_*); per chain c..3 TSSI_PWR_CTRL [24]
 * (phy_cal.h MT7996_PHY_TSSI_DPD_SHIFT_RM) = 1 and, with k0 = 1,
 * MT7996_PHY_CHAIN_2000 +0x0c / +0x18 / +0x28 [23:0] = 0x20d0d0. */
#define MT7996_CHSET_TPCMCS_VAL         3u
#define MT7996_CHSET_CHAIN_2000_OFS0    0x0cu
#define MT7996_CHSET_CHAIN_2000_OFS1    0x18u
#define MT7996_CHSET_CHAIN_2000_OFS2    0x28u
#define MT7996_CHSET_CHAIN_2000_VAL     0x20d0d0u
/* With E2 parts: WF_COMM +0xc058 [16], then [20] = 1; WF_COMM +0xb580
 * [13:4] = 0. Always: MT7996_PHY_TPC_CTL(band, 9) [14], [13], [6:4] = 0
 * (phy_cal.h MT7996_PHY_TPC_CTL9_*); BN +0xff68 [22:20], [17:16], [28],
 * [27:24], [29], [30], [31] = 0 (phy.h MT7996_PHY_DFE_FF68_*), one
 * read-modify-write each. */
#define MT7996_CHSET_TPC_CTL_N          9u

/* Table B's fields (V:e00d979e..V:e00d985c): with E2 parts, BN +0x85a0
 * [15:14] and BN +0x8e30 [31:30] = 3 on 6 GHz (k0 = 2) at k1 >= 2, else 0;
 * then WF_COMM +0xc104 [3:0] = 4 on band 2 at BW320 with EEPROM 0x1a4 [6]
 * (MT7996_PHY_EE_1A4_B6)
 * (written at band 2's address), else 0. */
#define MT7996_CHSET_C104_VAL           4u

/* ========================================================================
 * RX front end (V:e00d188e, V:e00d196a, V:e00d1a5e, V:e00d1b72). [static]
 * ======================================================================== */

/* MT7996_PHY_BN_6188: [11:0] then [27:16] (two read-modify-writes) =
 * round(N / f_MHz), N = 0x190000 (BW classes 0..5; 0x64000 at class 6,
 * 0xc8000 at class 7); MediaTek rounds by the next decimal digit
 * ((10 N / f) % 10 > 4). MT7996_PHY_DFE_F168 [17:0] = round(0xa000000 /
 * f_MHz). MT7996_PHY_CHAIN_D0AC [20:0] on chains 0..4 = round(0xa000000 /
 * f_MHz) << s, s = 0 / 1 / 2 / 3 / 4 / 2 at classes 0..5. f_MHz is the
 * centre (record u32[4]) in whole MHz. */
#define MT7996_CHSET_6188_NUM           0x190000u
#define MT7996_CHSET_F168_NUM           0x0a000000u
#define MT7996_CHSET_ROUND_DIGITS       10u
#define MT7996_CHSET_ROUND_HALF         4u	/* round up when the digit > 4 */
#define MT7996_CHSET_D0AC_SHIFT(bw)     (bw)	/* classes 0..4 */
#define MT7996_CHSET_D0AC_CHAINS        5u	/* chains 0..4, every band */

/* AGC table port MT7996_PHY_CSD_AGC_TBL: 1 us after each write
 * (cos_delay_usec, slot 0x0081ac14). */
#define MT7996_CHSET_AGC_DELAY_US       1u
#define MT7996_CHSET_AGC_CHAINS_2G4     4u
#define MT7996_CHSET_AGC_CHAINS_5G6G    5u

/* MT7996_PHY_WFCOMM_0080 [7:4] (V:e00d1b72): on 2.4 GHz (2407..2472 MHz
 * and 2484 MHz) the primary channel: at BW20 the channel, at BW40 the
 * centre - 2 / + 2 (primary index 0 / 1) for centres 3..11, else 6; not
 * written on 5 / 6 GHz (stock band 0: channel 6 -> 7). */
#define MT7996_CHSET_0080_LO_KHZ        2407001u	/* channels 1..13: lo <= kHz <= lo + span */
#define MT7996_CHSET_0080_SPAN_KHZ      64999u
#define MT7996_CHSET_0080_CH_OTHER      6u
#define MT7996_CHSET_0080_BW40_FIRST    3u
#define MT7996_CHSET_0080_BW40_LAST     11u
#define MT7996_CHSET_0080_BW40_OFS      2u

/* ========================================================================
 * Gain steps and RX gain offsets (V:e00d2540, V:e00d1dc4; phy_cal.h
 * MT7996_PHY_FW_GAINSTEP_*, phy.h MT7996_PHY_CSD_RXGOFS_*). [static]
 * ======================================================================== */

/* EEPROM RX-gain bytes E(x, y) (V:e00e0774): 2.4 GHz base 0x65e (n 1,
 * subband 0); 5 GHz 0x66e (n 3, subbands 0..2); 6 GHz 0x6aa (n 4, subbands
 * 0..3); byte base + 4 (WF n + subband) + 2 x + y / 2. The subband of a
 * group (V:e00d2540): 5 GHz groups 1..2, 3..6, 7..8 -> 0, 1, 2; 6 GHz
 * groups 9..12, 13..16, 17..20, 21.. -> 0..3. */
#define MT7996_CHSET_EE_RXG_2G4         0x65eu
#define MT7996_CHSET_EE_RXG_5G          0x66eu
#define MT7996_CHSET_EE_RXG_6G          0x6aau
#define MT7996_CHSET_EE_RXG_N_2G4       1u
#define MT7996_CHSET_EE_RXG_N_5G        3u
#define MT7996_CHSET_EE_RXG_N_6G        4u
#define MT7996_CHSET_EE_RXG(base, n, sub, wf, x, y) \
	((base) + 4u * ((wf) * (n) + (sub)) + 2u * (x) + (y) / 2u)
#define MT7996_CHSET_RXG_SUB(g) \
	((g) <= 2u ? 0u : (g) <= 6u ? 1u : (g) <= 8u ? 2u : (g) <= 12u ? 0u : (g) <= 16u ? 1u : (g) <= 20u ? 2u : 3u)

/* Gain-step class of a group (V:e00d1dc4): 0 for group 0, then groups 1..2,
 * 3..5, 6..8, 9..12, 13..16, 17..20, 21.. -> 1..7; with it the number of
 * steps of the class (9 for class 0, else 11). */
#define MT7996_CHSET_GCLASS(g) \
	((g) == 0u ? 0u : (g) <= 2u ? 1u : (g) <= 5u ? 2u : (g) <= 8u ? 3u : (g) <= 12u ? 4u : \
	 (g) <= 16u ? 5u : (g) <= 20u ? 6u : 7u)
#define MT7996_CHSET_GSTEPS(cls)        ((cls) == 0u ? 9u : 11u)
#define MT7996_CHSET_GPAIRS             11u	/* MT7996_CHSET_GSTEP pairs per class */
#define MT7996_CHSET_GSLOTS             6u	/* steps written, each a (step, set) pair */
#define MT7996_CHSET_GSET_NONE          0xffu	/* default set: the step's own values */
#define MT7996_CHSET_GSTEP_DEFAULT(slot) (2u * (slot))	/* E(0, 0) = 0: steps 0, 2, .., 10 */
#define MT7996_CHSET_GLIN_MASK          HW_GENMASK(15, 0)	/* the sum V:e00d1c22 gets */
#define MT7996_CHSET_GTHR_OFS           3	/* threshold + 3 */
#define MT7996_CHSET_GLIN_LO            (-9)	/* V:e00d1d14: x < -9 -> 0 */
#define MT7996_CHSET_GLIN_M4            (-4)	/* x < -4 -> 1 */
#define MT7996_CHSET_GLIN_M1            (-1)	/* x < -1 -> 2; x <= 0 -> 4 */
#define MT7996_CHSET_GLIN_HI            30	/* x > 30 -> 0x1000; else MT7996_CHSET_GLIN[x - 1] */
#define MT7996_CHSET_GLIN_V0            0u
#define MT7996_CHSET_GLIN_V1            1u
#define MT7996_CHSET_GLIN_V2            2u
#define MT7996_CHSET_GLIN_V4            4u
#define MT7996_CHSET_GLIN_VHI           0x1000u
/* V:e00d1c22: a linear gain (16 bits) back to a signed step: -6 up to 3;
 * then per segment (a + PRE) * MUL + ADD >> SHIFT for a <= MAX; 0x3d above
 * 4096. X(MAX, PRE, MUL, ADD, SHIFT). */
#define MT7996_CHSET_GLOG_MIN           3u
#define MT7996_CHSET_GLOG_LOW           (-6)
#define MT7996_CHSET_GLOG_HIGH          0x3d
#define MT7996_CHSET_GLOG(X) \
	X(   8u,  -4, 6,     2,  2) \
	X(  16u,   0, 3,     2,  2) \
	X(  32u,  16, 24,   32,  6) \
	X(  64u,  76, 11,   32,  6) \
	X( 128u, 256, 5,    32,  6) \
	X( 256u,   0, 3, 0x648,  6) \
	X( 512u,   0, 3, 0xf8c,  7) \
	X(1024u,   0, 3, 0x251c, 8) \
	X(2048u,   0, 3, 0x5644, 9) \
	X(4096u,   0, 3, 0xc49c, 10)
#define MT7996_CHSET_GOFS_MIN           (-64)	/* RX gain offset clamp (V:e00d1ffe) */
#define MT7996_CHSET_GOFS_MAX           63
#define MT7996_CHSET_GOFS7              HW_GENMASK(6, 0)
#define MT7996_CHSET_GOFS9              HW_GENMASK(8, 0)

/* A-die gain-step flags (V:e00d1f06.., V:e00d227c..): group 0: TOP 0x4a4
 * [4:0] = the five step flags; else TOP word / bit of the WF (0x4a4 bit 0
 * for WF 0; u16 table 0x022182d8 / u8 0x022182e0 for WF 1..4: 0x4a5 bit 0,
 * 0x4a5 bit 16, 0x4a6 bit 0, 0x4a6 bit 16), six flags from that bit. Then
 * chain 0x631 (group 0) / 0x636 (else): [3:0], [7:4], [11:8], [15:12],
 * [19:16] the step of slots 0..4, [23:20] slot 5 (group != 0), [31:24]
 * kept. Read-modify-writes; TOP bank of the band's die. */
#define MT7996_CHSET_GSTEP_TOP(wf) \
	((wf) == 0u || (wf) > 4u ? ADIE_TOP_FEM_4A4 : (wf) <= 2u ? ADIE_TOP_FEM_4A5 : ADIE_TOP_FEM_4A6)
#define MT7996_CHSET_GSTEP_BIT(wf)      ((wf) == 2u || (wf) == 4u ? 16u : 0u)
#define MT7996_CHSET_GSTEP_NFLAGS       6u
/* The second pass's per-chain RX gain offsets (V:e00d2048..V:e00d216c),
 * read-modify-writes of words PHY table B wrote: +0x188 the four 7-bit
 * fields (slots 0..3: [30:24], [22:16], [14:8], [6:0]), +0x0f8 [30:24] slot
 * 4, +0x390 [30:24] slot 5 (group != 0); +0x164 [8:0] / [24:16] the 9-bit
 * values of slots 1 / 2, +0x160 of slots 3 / 4, +0x408 [24:16] slot 5
 * (group != 0). MediaTek then reads CSD +0x104 five times and discards the
 * values (group != 0). */

/* ========================================================================
 * The D-die RX setup (V:e00ddd0e). [static][measured]
 * ======================================================================== */

/* The channel record's channel-0 frequency (record u32[0], ctx->base_khz) values V:e00ddd0e
 * and V:e00dd40a compare with: 2.4 GHz, 5 GHz and the 4.9 GHz (802.11j) base; 6 GHz
 * (5,950,000 kHz) is "any other". [static] */
#define MT7996_CHSET_BASE_KHZ_4G9         4000000u

/* The 6 GHz 320 MHz channels of the second channelization (centres 63, 127, 191): V:e00ddd6a
 * tests (centre & 0xbf) == 0x3f or centre == 0xbf on the record's centre channel (e00d708a's
 * a4). At BW320 they set MT7996_PHY_BN_MODE_B8. [static] */
#define MT7996_CHSET_320_2(centre)        ((((centre) & 0xbfu) == 0x3fu) || (centre) == 0xbfu)

/* P[0x1c] (ctx->fem) value for which V:e00ddd0e clears MT7996_PHY_TSSI_CORE1_B11
 * (V:e00de204). The channel-set P block (V:e00d4a1e mode 2) stores only 0 or 1 there (or
 * leaves it without EPA); no writer of 2 was found. [static] */
#define MT7996_CHSET_FEM_2                2u

/* Chains V:e00ddd0e walks: CHAIN_E01C / E034 / E024 on chain positions 0..4 of every band
 * (band 0's position 4 is the monitor chain); CHAIN_D010 on 0..3 and on 4 except on band 0
 * (V:e00dddf6 / V:e00de116 test the band). [static][measured] */
#define MT7996_CHSET_RX_CHAINS            5u
#define MT7996_CHSET_D010_CHAINS(band)    ((band) != MT7996_PHY_BAND_2G4 ? 5u : 4u)

/* cos_delay_usec(1) at the start of each gate-opening helper (V:e00ddbfe.., through the ROM
 * slot 0x0081ac14), before the band reset pulse. [static] */
#define MT7996_CHSET_GATE_DELAY_US        1u

/* BN +0xfc30.. (phy_cal.h MT7996_PHY_TXCAL_TBL) as V:e00dd40a writes it after DFE_FCB0 /
 * FCB4, code immediates, whole words. MT7976 (AdieSel 6) only with EPA (P[6]) on base
 * 2,407,000 kHz: pattern MT7996_PHY_DFE_FCB_PAT_CFG6 and 16 words; MT7977 (any other
 * AdieSel but 5) only off base 2,407,000 kHz: pattern MT7996_PHY_DFE_FCB_PAT_CFG7 and, with
 * EPA, 16 words, without EPA 32 words (+0xfc30..+0xfcac: MT7996_PHY_TXCAL_TBL(band, i) with
 * i 0..31, two 16-word halves). AdieSel 5 (another die type, not on this board) writes
 * MT7996_PHY_DFE_FCB_PAT_CFG5 and 32 words (0x04050607 / 0x00010203 x 8, 0x0c0d0e0f /
 * 0x08090a0b x 8) without EPA on base 2,407,000 kHz; not ported. [static][measured] */
#define MT7996_PHY_TXCAL_TBL_MT7976 { \
	0x0a0b0c0du, 0x07070809u, 0x0a0b0c0du, 0x07070809u, 0x0a0b0c0du, 0x07070809u, \
	0x0a0b0c0du, 0x07070809u, 0x0a0b0c0du, 0x07070809u, 0x03040e0fu, 0x00000102u, \
	0x03040506u, 0x00000102u, 0x03040506u, 0x00000102u }
#define MT7996_PHY_TXCAL_TBL_MT7977_EPA { \
	0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, \
	0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, \
	0x03040505u, 0x00000102u, 0x090a0b0bu, 0x06060708u }
#define MT7996_PHY_TXCAL_TBL_MT7977_NOEPA { \
	0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, \
	0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, \
	0x0f101111u, 0x0c0c0d0eu, 0x090a0b0bu, 0x06060708u, \
	0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, \
	0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, 0x03040505u, 0x00000102u, \
	0x0f101111u, 0x0c0c0d0eu, 0x090a0b0bu, 0x06060708u }

/* ========================================================================
 * The channel-switch handler's MAC words and the BW320 background receiver
 * (V:e00b103c / V:e00b0a74; CHANSET_F_MAC, CHANSET_F_BGRX). [static][measured]
 * ======================================================================== */

/* The SMPS_CTRL values of the channel switch's first step (mac.h RMAC_SMPS_CTRL_*). */
#define MT7996_CHSET_SMPS_MODE            3u	/* V:e01240cc immediate */
#define MT7996_CHSET_SMPS_BY_RTS_ONLY     1u	/* V:e01240ca immediate */
/* RX_STREAM's width: V:e00b1006 builds the mask in a byte (V:e00b1028), so 8 or more streams
 * give 0xff. */
#define MT7996_CHSET_RX_STREAM_BITS       8u
/* V:e0123c70 counts RX_STREAM [3:0] only (its loop ends at 4, V:e0123ccc), on every band. */
#define MT7996_CHSET_SMPS_COUNT_BITS      4u

/* RMAC CHFREQ [7:0] (mac.h RMAC_CHFREQ_CH),
 * written by V:e00e5c9a with the control channel as V:e00b193a numbers it: 2.4 / 5 GHz the
 * channel itself (a 2.4 GHz channel above MT7996_CHSET_2G4_CH_MAX is written as 1); 6 GHz
 * ((ch - 1) [9:2] - 0x4b) mod 256, i.e. 181 + (ch - 1) / 4 (ch 1 = 0xb5, ch 233 = 0xef).
 * Stock 7 / 0x24 / 0xb5. [static][measured] */
#define MT7996_CHSET_CHFREQ_6G_IDX        HW_GENMASK(9, 2)	/* of ch - 1, V:e00b1948 */
#define MT7996_CHSET_CHFREQ_6G_SUB        0x4bu	/* V:e00b194c */
#define MT7996_CHSET_CHFREQ_2G4_BAD       1u	/* V:e00e5d26 */

/* RATE BWCR [4:2] (mac.h RATE_BWCR_BW; CR map name RF_BW0, which confirms the field): the code
 * V:e00e5c9a writes, from MediaTek's channel-width code: 20 / 40 MHz 0 / 1 (1 when the control
 * channel is not the centre), 80 MHz 2, 160 and 80+80 MHz 3, 320 MHz 4; 5 / 10 MHz (0 / 1 as
 * 20 / 40). For the BW classes of chanset.h this is the class itself. The handler tail
 * (V:e00b1322..V:e00b1346) writes the value read before the switch (V:e00e54d2) back when the
 * new code is smaller. Stock 0x80 -> 0x80 / 0x8c / 0x90. [static][measured] */
#define MT7996_CHSET_BWCR_BW80            2u
#define MT7996_CHSET_BWCR_BW160           3u
#define MT7996_CHSET_BWCR_BW320           4u

/* BN0 / BN1 TICKCR0 (CR map ENUM_WFSYS_ON_CONFIG_BN0_TICKCR0 0x2600e0 / e1, BN1 0x260420 /
 * 421): V:e00e5c9a -> V:e00e549a writes, BN0 then BN1, WLAN_CLK_MODE then LMAC_1X_MODE (one
 * read-modify-write each: the ROM's non-transaction setter FIT +0x20, ROM V:0081c682), both
 * = 2 for MediaTek's 5 MHz width, 1 for 10 MHz, else 0. Stock 0 unchanged. WM writes (conn.h's
 * host-write HAZARD for 0x8103xxxx stands). [static][measured] */
#define MT7996_CHSET_TICKCR0_MODE         0u	/* every BW class (V:e00e5d60) */
#define MT7996_CHSET_TICKCR0_MODE_5M      2u	/* V:e00e5d02, MediaTek's 5 MHz width only */
#define MT7996_CHSET_TICKCR0_MODE_10M     1u	/* V:e00e5d5a, 10 MHz only */
#define MT7996_CHSET_TICKCR0_BANDS        2u	/* BN0, BN1 (both on every band's channel set) */

/* BN +0x8554 [23:16], [15:8], [7:0] and BN +0x8d08 [7:0]: four 8-bit EDCCA thresholds
 * [inferred: MediaTek's log in V:e00afefc, "No EDCCA TH value from wifi.cfg SKIP setting";
 * stock band 0 0xd8c5c2c1 / 0x00d4d4c9, i.e. -59 / -62 / -63 and -55 as signed bytes, dBm].
 * V:e00afca2 reads and writes them back through V:e00e6362 / V:e00cba3e (each pass: both words
 * read, then both written, +0x8554 first; +0x8554 [31:24] and +0x8d08 [31:8] kept), with a rule
 * chosen by the WM's EDCCA region (WM 0xe01ec030, set by a host command; 0 in the stock load and
 * in our firmware): region 0 subtracts the WM .data byte 0x02218879 (0) from all four, i.e.
 * writes them back unchanged. The other regions (1..7: fixed 0xb4 on 6 GHz, -10, the TX-power
 * path V:e00b58f4) and V:e00afefc (wifi.cfg values at WM .data 0x0221887a.., all 0) are not
 * ported. A value of 0x7f in the write buffer means "keep the current one" (V:e00e6394).
 * [static][measured] */
#define MT7996_CHSET_EDCCA_N              4u	/* +0x8554 F0, F1, F2, +0x8d08 F0 */
#define MT7996_CHSET_EDCCA_OFS            0u	/* WM .data 0x02218879, MediaTek's initial value */
#define MT7996_CHSET_EDCCA_KEEP           0x7fu	/* V:e00e6394 */

/* At a channel set on 6 GHz (record u32[0] = MT7996_CHSET_BASE_KHZ_6G) at BW320 (record [0x11]
 * = 4) V:e00b0a74 stops (V:e00ad640(0, 2), before the channel flow) and restarts
 * (V:e00ad4de(0, 2, 0), after the flow and the thermal TX power) MediaTek's radar detector 2,
 * the background (zero-wait DFS) detector on band 0's monitor chain (phy.h
 * MT7996_PHY_MONITOR_CHAIN), whatever the band of the channel set. Its pulse records go
 * through band 0's AHB bridge client 1 (the WM's RDD-2 slot: CFG0 / START / END / ISR enable 1);
 * the ROM's halRdm stall setter (ROM V:008299ca through slot 0x0081bd24) addresses client 2.
 * [static][measured] */
#define MT7996_CHSET_BGRX_RDD             2u	/* rdd_idx 2 (mt76 MT_RDD_IDX_BACKGROUND) */
#define MT7996_CHSET_BGRX_AHB_CLIENT      MT7996_PHY_AHB_CLIENT_RDD1	/* V:e00ca9a2, V:e00ca8c4 */
#define MT7996_CHSET_BGRX_AHB_STALL       MT7996_PHY_AHB_CLIENT_RDD2	/* ROM V:008299ec */

/* MT7996_PHY_BN_CTRL0 [28:24] / [20:16] at a detector start (phy.h). */
#define MT7996_CHSET_BGRX_CTRL0_ON        1u

/* RST_CTRL RDD reset pulse at a detector start: V:e00caa6e sets then clears [13] at the fixed
 * band-1 address 0x83180014, whatever the band. [static][measured] */
#define MT7996_CHSET_BGRX_RST_BAND        MT7996_PHY_BAND_5G

/* Monitor chain gain at the detector start (V:e00cab7c, V:e00cab98, V:e00cabc8, V:e00cabe2,
 * V:e00cabfc on band 0 chain 4): RFDGC A gain 0; RXGAIN set A forced (A_EN) at LNA 4
 * (MT7996_PHY_RXGAIN_LNA_UH) and LPF 3, each written twice; coarse-AGC switch forced (SW_EN)
 * to state 4. [static][measured] */
#define MT7996_CHSET_BGRX_RFDGC           0u	/* V:e00ed0be */
#define MT7996_CHSET_BGRX_LPF             3u	/* V:e00cabbc, V:e00ed0dc */
#define MT7996_CHSET_BGRX_CAGC_STATE      4u	/* V:e00cac16 */

/* WF_COMM +0x3078 / +0x307c [12:0] (phy.h MT7996_PHY_BGRX_307X_VAL) and +0x3050 / +0x3054
 * (MT7996_PHY_CSI_CLK0 / 1, written whole after a read whose value is discarded) at the
 * detector start (V:e00cac62). [static][measured] */
#define MT7996_CHSET_BGRX_307X_ON         0x1800u	/* V:e00cac6a */
#define MT7996_CHSET_BGRX_CSI_CLK         0x10u	/* V:e00cac88 */

/* BGRX_5DF4 [29:21] at the detector start's first use (phy.h MT7996_PHY_BGRX_5DF4_*). */
#define MT7996_CHSET_BGRX_5DF4_INIT       0x1f8u	/* V:e00cab4a */

/* The BGRX_CFG values of the detector start (phy.h MT7996_PHY_BGRX_CFG_*). */
#define MT7996_CHSET_BGRX_CFG_19_10_FIRST 0xb8u	/* V:e00cab24 (0x2e000 >> 10) */
#define MT7996_CHSET_BGRX_CFG_19_10       0xb0u	/* V:e00ed13a */
#define MT7996_CHSET_BGRX_IPI_EN          5u	/* V:e00caaf0 (phy.h: "IPI_EN = 5 on first use") */

/* Channel-set PHY table A (V:e00d9238; phy_map.h "Channel-set PHY tables A and B").
 * MediaTek WM .data (VMA): descriptors 0x0221d7f0, addresses 0x0221df44, values 0x0221e1f4, column map
 * 0x0221dc34 (u8 [key][group]), keys 0x0221df28 (u8, 28). Walked per chain (relocated); key value = MT7996_PHY_CHTBL_A_KEY.
 * Rows in walk order: X(address at band 0, then one value per column of the row's group); the
 * column of group g is COLMAP[key][g], key = the position of the key value in KEYS.
 * MT7996_PHY_REGTBL_SKIP = no write. [static] */
#define MT7996_CHSET_TBL_A_NGROUPS 27u
#define MT7996_CHSET_TBL_A_NKEYS 28u
#define MT7996_CHSET_TBL_A_GROUPS(X) \
	X(36u, 2u) \
	X(27u, 2u) \
	X(12u, 2u) \
	X(3u, 3u) \
	X(19u, 3u) \
	X(2u, 2u) \
	X(1u, 4u) \
	X(14u, 4u) \
	X(4u, 3u) \
	X(3u, 2u) \
	X(2u, 6u) \
	X(3u, 5u) \
	X(15u, 2u) \
	X(1u, 5u) \
	X(1u, 5u) \
	X(2u, 3u) \
	X(2u, 4u) \
	X(2u, 4u) \
	X(2u, 3u) \
	X(4u, 2u) \
	X(2u, 3u) \
	X(1u, 2u) \
	X(1u, 3u) \
	X(7u, 3u) \
	X(1u, 2u) \
	X(4u, 3u) \
	X(1u, 3u)
#define MT7996_CHSET_TBL_A_ROWS(X) \
	X(MT7996_PHY_WFCOMM_B000(0), 0x80017111u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FD08(0), 0x00000002u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FD14(0), 0x00000001u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_WFCOMM_D4EC(0), 0x00000b1fu, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_GAIN_COMP(0, 6u), 0x00800080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DPD_LUT_SEL(0), 0x01100000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF74(0), 0x00000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF38(0), 0x00404040u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DPD_CFG4(0), 0x00050000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DPD_FF44(0), 0x00000007u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF48(0), 0x80808080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF4C(0), 0x80808080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF50(0), 0x80801c00u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF58(0), 0x7f7f7f7fu, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF5C(0), 0x7f7f7f7fu, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF60(0), 0x7f7f0000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF54(0), 0xf8000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF64(0), 0x00008080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FFC0(0), 0x00000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FFB8(0), 0x80000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FFBC(0), 0x80808080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF90(0), 0x80808080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF94(0), 0x80000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FFB0(0), 0x80808080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FFB4(0), 0x80001c00u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF04(0), 0x800000e4u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_E428(0), 0x0000e200u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_TPC_CTL(0, 6u), 0xc9600000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_E430(0), 0x0000c960u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_BN_D0A0(0), 0xb1101110u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_WFCOMM_B538(0), 0x66660000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FD78(0), 0x00000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FE64(0), 0x00000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_WFCOMM_B59C(0), 0x00000001u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_WFCOMM_B580(0), 0x01000007u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_WFCOMM_C058(0), 0x00110000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x008u), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x00cu), 0x0030c0c0u, 0x0020d0d0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x014u), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x018u), 0x0030c0c0u, 0x0020d0d0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x020u), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x028u), 0x0030c0c0u, 0x0020d0d0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x030u), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x03cu), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x050u), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x054u), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x05cu), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x060u), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x064u), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x068u), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x06cu), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x078u), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x07cu), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x080u), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_DFE_FD30(0), 0x00000000u, 0x00000300u) \
	X(MT7996_PHY_TSSI_DFE_BACKOFF0(0, 0), 0x27242423u, 0x26232322u) \
	X(MT7996_PHY_CHAIN_C04C(0, 0), 0x807f04fcu, 0x807f00f8u) \
	X(MT7996_PHY_CHAIN_D100(0, 0), 0x01430143u, 0x00fd0143u) \
	X(MT7996_PHY_CHAIN_D114(0, 0), 0x00e400e4u, 0x00b300e4u) \
	X(MT7996_PHY_CHAIN_D2C0(0, 0), MT7996_PHY_REGTBL_SKIP, 0x10000000u) \
	X(MT7996_PHY_CHAIN_D2CC(0, 0), MT7996_PHY_REGTBL_SKIP, 0x00036ef2u) \
	X(MT7996_PHY_CHAIN_D2D0(0, 0), MT7996_PHY_REGTBL_SKIP, 0x03ff2aaeu) \
	X(MT7996_PHY_CHAIN_D2D4(0, 0), MT7996_PHY_REGTBL_SKIP, 0x03fae66au) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x01cu), 0x0030c0c0u, 0x0040a0a0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x02cu), 0x0030c0c0u, 0x0040a0a0u) \
	X(MT7996_PHY_CHAIN_D254(0, 0), 0x00801180u, 0x00801380u) \
	X(MT7996_PHY_CHAIN_D260(0, 0), 0x11800000u, 0x13800000u) \
	X(MT7996_PHY_CHAIN_D264(0, 0), 0x80118000u, 0x80138000u) \
	X(MT7996_PHY_CHAIN_D26C(0, 0), 0x00008011u, 0x00008013u) \
	X(MT7996_PHY_CHAIN_D274(0, 0), 0x11800000u, 0x13800000u) \
	X(MT7996_PHY_CHAIN_D27C(0, 0), 0x00801180u, 0x00801380u) \
	X(MT7996_PHY_CHAIN_D280(0, 0), 0x00008011u, 0x00008013u) \
	X(MT7996_PHY_CHAIN_D28C(0, 0), 0x80112060u, 0x80132060u) \
	X(MT7996_PHY_CHAIN_D2A0(0, 0), 0x80118000u, 0x80138000u) \
	X(MT7996_PHY_CHAIN_D2A8(0, 0), 0x00008011u, 0x00008013u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x044u), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u, 0x0030c0c0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x048u), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u, 0x0030c0c0u) \
	X(MT7996_PHY_CHAIN_2000(0, 0, 0x04cu), MT7996_PHY_REGTBL_SKIP, 0x0040a0a0u, 0x0030c0c0u) \
	X(MT7996_PHY_BN_D01C(0), 0x3fffffffu, MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_BN_D024(0), 0x00100401u, MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_BN_D028(0), 0x00200c04u, MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_BN_D02C(0), 0x00401807u, MT7996_PHY_REGTBL_SKIP, 0x3ffffbfeu) \
	X(MT7996_PHY_BN_D030(0), 0x0080280cu, MT7996_PHY_REGTBL_SKIP, 0x3fdff3fau) \
	X(MT7996_PHY_BN_D034(0), 0x00e04012u, MT7996_PHY_REGTBL_SKIP, 0x3f9fdff5u) \
	X(MT7996_PHY_BN_D038(0), 0x0150601bu, MT7996_PHY_REGTBL_SKIP, 0x3f4fcbf0u) \
	X(MT7996_PHY_BN_D03C(0), 0x01e08826u, MT7996_PHY_REGTBL_SKIP, 0x3eefb3ebu) \
	X(MT7996_PHY_BN_D040(0), 0x02a0b832u, MT7996_PHY_REGTBL_SKIP, 0x3eafa7e9u) \
	X(MT7996_PHY_BN_D044(0), 0x0370ec40u, MT7996_PHY_REGTBL_SKIP, 0x3eafb3efu) \
	X(MT7996_PHY_BN_D048(0), 0x04512850u, MT7996_PHY_REGTBL_SKIP, 0x3f3fe7ffu) \
	X(MT7996_PHY_BN_D04C(0), 0x0551685fu, MT7996_PHY_REGTBL_SKIP, 0x0080441cu) \
	X(MT7996_PHY_BN_D050(0), 0x0651a86fu, MT7996_PHY_REGTBL_SKIP, 0x0290dc46u) \
	X(MT7996_PHY_BN_D054(0), 0x0751e87fu, MT7996_PHY_REGTBL_SKIP, 0x05619c7au) \
	X(MT7996_PHY_BN_D058(0), 0x0832208cu, MT7996_PHY_REGTBL_SKIP, 0x08d280b3u) \
	X(MT7996_PHY_BN_D05C(0), 0x09125498u, MT7996_PHY_REGTBL_SKIP, 0x0c7368ecu) \
	X(MT7996_PHY_BN_D060(0), 0x09c27ca1u, MT7996_PHY_REGTBL_SKIP, 0x0fe4391du) \
	X(MT7996_PHY_BN_D064(0), 0x0a4294a7u, MT7996_PHY_REGTBL_SKIP, 0x12a4d93fu) \
	X(MT7996_PHY_BN_D068(0), 0x0a82a4a9u, MT7996_PHY_REGTBL_SKIP, 0x14652d4eu) \
	X(MT7996_PHY_BN_D020(0), 0x00000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_E200(0), 0x00060006u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_E764(0), 0x00000204u, MT7996_PHY_REGTBL_SKIP, 0x00000003u, 0x00000004u) \
	X(MT7996_PHY_DFE_FD7C(0), 0x0100010cu, MT7996_PHY_REGTBL_SKIP, 0x0000ff00u, 0x00000000u) \
	X(MT7996_PHY_DFE_FD70(0), 0x76540000u, MT7996_PHY_REGTBL_SKIP, 0x76544444u, 0x47655544u) \
	X(MT7996_PHY_DFE_FF14(0), 0x00525252u, MT7996_PHY_REGTBL_SKIP, 0x00535353u, 0x00575757u) \
	X(MT7996_PHY_DFE_FF18(0), 0x52505050u, MT7996_PHY_REGTBL_SKIP, 0x53535000u, 0x57575400u) \
	X(MT7996_PHY_DFE_FF20(0), 0x00404040u, MT7996_PHY_REGTBL_SKIP, 0x00545454u, 0x00565656u) \
	X(MT7996_PHY_DFE_FF24(0), 0x40515151u, MT7996_PHY_REGTBL_SKIP, 0x54545100u, 0x56565300u) \
	X(MT7996_PHY_DFE_E738(0), 0x44411111u, MT7996_PHY_REGTBL_SKIP, 0x33111111u, 0x33333333u) \
	X(MT7996_PHY_DFE_E73C(0), 0x44441111u, MT7996_PHY_REGTBL_SKIP, 0x33311111u, 0x33333333u) \
	X(MT7996_PHY_DFE_E740(0), 0x44441111u, MT7996_PHY_REGTBL_SKIP, 0x33311111u, 0x33333333u) \
	X(MT7996_PHY_DFE_E744(0), 0x44441111u, MT7996_PHY_REGTBL_SKIP, 0x33311111u, 0x33333333u) \
	X(MT7996_PHY_DFE_E748(0), 0x44441111u, MT7996_PHY_REGTBL_SKIP, 0x33311111u, 0x33333333u) \
	X(MT7996_PHY_DFE_E750(0), 0x44441111u, MT7996_PHY_REGTBL_SKIP, 0x33311111u, 0x33333333u) \
	X(MT7996_PHY_DFE_E758(0), 0x44441111u, MT7996_PHY_REGTBL_SKIP, 0x33311111u, 0x33333333u) \
	X(MT7996_PHY_DFE_E760(0), 0x44441111u, MT7996_PHY_REGTBL_SKIP, 0x33311111u, 0x33333333u) \
	X(MT7996_PHY_DFE_FD84(0), 0x10161c21u, MT7996_PHY_REGTBL_SKIP, 0x02080e14u) \
	X(MT7996_PHY_DFE_FDA4(0), 0x02080e13u, MT7996_PHY_REGTBL_SKIP, 0x767c0208u) \
	X(MT7996_PHY_DFE_FDC4(0), 0x7f7c7977u, MT7996_PHY_REGTBL_SKIP, 0x05027f7cu) \
	X(MT7996_PHY_DFE_FDE0(0), 0x00002221u, MT7996_PHY_REGTBL_SKIP, 0x02220000u) \
	X(MT7996_PHY_DFE_FD80(0), MT7996_PHY_REGTBL_SKIP, 0x1472787eu) \
	X(MT7996_PHY_DFE_FDA0(0), MT7996_PHY_REGTBL_SKIP, 0x14666c72u) \
	X(MT7996_PHY_DFE_FDC0(0), MT7996_PHY_REGTBL_SKIP, 0x140d0a07u) \
	X(MT7996_PHY_DFE_FD94(0), 0x080e1419u, MT7996_PHY_REGTBL_SKIP, 0x050d0f15u, 0x01090b11u, 0x7a00060cu, 0x787e040au) \
	X(MT7996_PHY_DFE_FDD4(0), 0x03007d7bu, MT7996_PHY_REGTBL_SKIP, 0x04007f7cu, 0x0602017eu, 0x09060300u, 0x0a070401u) \
	X(MT7996_PHY_DFE_FD90(0), MT7996_PHY_REGTBL_SKIP, 0x6d73797fu, 0x696f757bu, 0x146a7076u, 0x62686e72u) \
	X(MT7996_PHY_DFE_FDB0(0), MT7996_PHY_REGTBL_SKIP, 0x63696f75u, 0x5f656b71u, 0x145e646au, 0x585e6468u) \
	X(MT7996_PHY_DFE_FDD0(0), MT7996_PHY_REGTBL_SKIP, 0x100d0a07u, 0x120f0c09u, 0x14110e0bu, 0x15120f0du) \
	X(MT7996_PHY_DFE_FD9C(0), 0x050b1016u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FDBC(0), 0x777d0208u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FDDC(0), 0x05027f7cu, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FDEC(0), 0x00002211u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FE4C(0), 0x77777777u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FE48(0), 0x66666666u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FE44(0), 0x55555555u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FE40(0), 0x44444444u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_GAIN_COMP(0, 14u), 0x00800080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_GAIN_COMP(0, 12u), 0x00800080u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FD74(0), 0x76540000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF1C(0), 0x50000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF28(0), 0x51000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF34(0), 0x40000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FF40(0), 0x40000000u, MT7996_PHY_REGTBL_SKIP) \
	X(MT7996_PHY_DFE_FDB4(0), 0x7a00060bu, MT7996_PHY_REGTBL_SKIP, 0x7b03050bu, 0x777f0107u, 0x6e747a00u) \
	X(MT7996_PHY_DFE_FDE8(0), 0x00002221u, MT7996_PHY_REGTBL_SKIP, 0x22222200u, 0x02220000u, 0x22200000u) \
	X(MT7996_PHY_DFE_FE2C(0), 0x77777777u, MT7996_PHY_REGTBL_SKIP, 0x66666677u) \
	X(MT7996_PHY_DFE_FE28(0), 0x66666666u, MT7996_PHY_REGTBL_SKIP, 0x55555555u) \
	X(MT7996_PHY_DFE_FE24(0), 0x55555555u, MT7996_PHY_REGTBL_SKIP, 0x44445555u, 0x33334455u) \
	X(MT7996_PHY_DFE_FE20(0), 0x44444444u, MT7996_PHY_REGTBL_SKIP, 0x33333333u, 0x22222333u) \
	X(MT7996_PHY_DFE_FE1C(0), MT7996_PHY_REGTBL_SKIP, 0x22222222u, 0x11111111u, 0x33333333u) \
	X(MT7996_PHY_DFE_FE18(0), MT7996_PHY_REGTBL_SKIP, 0x11111111u, 0x00000000u, 0x22222222u) \
	X(MT7996_PHY_DFE_FE14(0), MT7996_PHY_REGTBL_SKIP, 0x00000000u, 0x11111111u) \
	X(MT7996_PHY_DFE_GAIN_COMP(0, 2u), MT7996_PHY_REGTBL_SKIP, 0x00800080u, 0x00b00080u) \
	X(MT7996_PHY_DFE_FE10(0), MT7996_PHY_REGTBL_SKIP, 0x00000000u) \
	X(MT7996_PHY_BN_D0B4(0), MT7996_PHY_REGTBL_SKIP, 0x00000071u) \
	X(MT7996_PHY_BN_D0B8(0), MT7996_PHY_REGTBL_SKIP, 0x001f7f80u) \
	X(MT7996_PHY_BN_D0BC(0), MT7996_PHY_REGTBL_SKIP, 0x00000010u) \
	X(MT7996_PHY_DFE_GAIN_COMP(0, 4u), 0x00800080u, MT7996_PHY_REGTBL_SKIP, 0x005000b0u) \
	X(MT7996_PHY_DFE_FF80(0), 0x00005051u, MT7996_PHY_REGTBL_SKIP, 0x00005150u) \
	X(MT7996_PHY_DFE_GAIN_COMP(0, 0u), MT7996_PHY_REGTBL_SKIP, 0x00800080u) \
	X(MT7996_PHY_TSSI_PWR_CTRL(0, 0), 0x00000000u, 0x01000000u, 0x01000020u) \
	X(MT7996_PHY_DFE_FF2C(0), 0x00404040u, MT7996_PHY_REGTBL_SKIP, 0x00525252u) \
	X(MT7996_PHY_DFE_FF30(0), 0x40404040u, MT7996_PHY_REGTBL_SKIP, 0x52525200u) \
	X(MT7996_PHY_DFE_FF84(0), 0x40400000u, MT7996_PHY_REGTBL_SKIP, 0x40520000u) \
	X(MT7996_PHY_DFE_FF3C(0), 0x40404040u, MT7996_PHY_REGTBL_SKIP, 0x40404000u) \
	X(MT7996_PHY_DFE_E74C(0), 0x00004444u, MT7996_PHY_REGTBL_SKIP, 0x00003333u) \
	X(MT7996_PHY_DFE_E754(0), 0x00444444u, MT7996_PHY_REGTBL_SKIP, 0x00333333u) \
	X(MT7996_PHY_DFE_E75C(0), 0x11444444u, MT7996_PHY_REGTBL_SKIP, 0x11333333u) \
	X(MT7996_PHY_DFE_FF6C(0), MT7996_PHY_REGTBL_SKIP, 0x40000000u) \
	X(MT7996_PHY_DFE_FF8C(0), 0x80808080u, MT7996_PHY_REGTBL_SKIP, 0x00808080u) \
	X(MT7996_PHY_WFCOMM_D4F4(0), 0xfbfe01ffu, MT7996_PHY_REGTBL_SKIP, 0xfbff01ffu) \
	X(MT7996_PHY_WFCOMM_D4F8(0), 0xfbfe0002u, MT7996_PHY_REGTBL_SKIP, 0xfbff0002u) \
	X(MT7996_PHY_DFE_FFAC(0), 0x80808080u, MT7996_PHY_REGTBL_SKIP, 0x00808080u) \
	X(MT7996_PHY_CHAIN_D250(0, 0), 0x00000000u, 0x80000000u, MT7996_PHY_REGTBL_SKIP)
#define MT7996_CHSET_TBL_A_KEYS { \
	0u, 18u, 36u, 54u, 6u, 24u, 42u, 60u, 13u, 31u, 49u, 67u, 14u, 32u, 50u, 68u, \
	3u, 21u, 39u, 57u, 9u, 27u, 45u, 63u, 16u, 34u, 52u, 70u, \
}
#define MT7996_CHSET_TBL_A_COLMAP { \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 0u, \
	1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, \
	1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, \
	0u, 0u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, \
	1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, \
	1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, \
	1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, \
	0u, 1u, 1u, 1u, 1u, 1u, 0u, 2u, 1u, 0u, 2u, 1u, 1u, 2u, 2u, 0u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, 2u, 0u, 2u, 2u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 2u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 2u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 2u, \
	0u, 1u, 1u, 1u, 1u, 1u, 0u, 2u, 1u, 0u, 3u, 2u, 1u, 3u, 2u, 2u, 3u, 2u, 1u, 1u, 0u, 1u, 0u, 2u, 0u, 2u, 2u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 2u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 2u, \
	1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 2u, \
	0u, 0u, 1u, 0u, 0u, 0u, 2u, 3u, 2u, 1u, 4u, 3u, 1u, 4u, 3u, 0u, 0u, 3u, 2u, 0u, 2u, 1u, 1u, 2u, 0u, 0u, 0u, \
	1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, \
	0u, 0u, 1u, 0u, 2u, 0u, 2u, 3u, 2u, 1u, 4u, 3u, 1u, 4u, 3u, 0u, 0u, 3u, 2u, 0u, 2u, 1u, 1u, 2u, 0u, 0u, 0u, \
	1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, \
	1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, \
	0u, 1u, 1u, 2u, 1u, 1u, 3u, 3u, 1u, 0u, 5u, 4u, 1u, 4u, 4u, 0u, 0u, 3u, 2u, 1u, 2u, 1u, 2u, 2u, 1u, 2u, 2u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 2u, 1u, 0u, 1u, 2u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 2u, 1u, 0u, 1u, 2u, \
	1u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 2u, 1u, 0u, 1u, 2u, \
}

/* Channel-set PHY table B (V:e00d9702; phy_map.h "Channel-set PHY tables A and B").
 * MediaTek WM .data (VMA): descriptors 0x0221a02c, addresses 0x0221c038, values 0x0221c564, column map
 * 0x0221a900 (u8 [key][group]), keys 0x0221bfc8 (u8, 110). Not relocated (chain 0xf); key value = MT7996_PHY_CHTBL_B_KEY.
 * Rows in walk order: X(address at band 0, then one value per column of the row's group); the
 * column of group g is COLMAP[key][g], key = the position of the key value in KEYS.
 * MT7996_PHY_REGTBL_SKIP = no write. [static] */
#define MT7996_CHSET_TBL_B_NGROUPS 53u
#define MT7996_CHSET_TBL_B_NKEYS 110u
#define MT7996_CHSET_TBL_B_GROUPS(X) \
	X(5u, 2u) \
	X(1u, 3u) \
	X(5u, 4u) \
	X(83u, 2u) \
	X(30u, 5u) \
	X(10u, 4u) \
	X(5u, 3u) \
	X(5u, 4u) \
	X(5u, 5u) \
	X(5u, 3u) \
	X(5u, 5u) \
	X(22u, 2u) \
	X(19u, 2u) \
	X(23u, 3u) \
	X(1u, 12u) \
	X(4u, 2u) \
	X(1u, 4u) \
	X(1u, 2u) \
	X(2u, 2u) \
	X(1u, 8u) \
	X(1u, 3u) \
	X(1u, 6u) \
	X(16u, 2u) \
	X(1u, 2u) \
	X(1u, 2u) \
	X(1u, 3u) \
	X(2u, 3u) \
	X(1u, 3u) \
	X(2u, 6u) \
	X(1u, 5u) \
	X(1u, 2u) \
	X(10u, 2u) \
	X(1u, 4u) \
	X(6u, 4u) \
	X(2u, 2u) \
	X(1u, 5u) \
	X(1u, 8u) \
	X(1u, 7u) \
	X(1u, 3u) \
	X(4u, 3u) \
	X(13u, 3u) \
	X(3u, 5u) \
	X(1u, 4u) \
	X(3u, 3u) \
	X(1u, 2u) \
	X(5u, 2u) \
	X(1u, 3u) \
	X(1u, 3u) \
	X(3u, 2u) \
	X(1u, 2u) \
	X(5u, 3u) \
	X(5u, 2u) \
	X(1u, 2u)
#define MT7996_CHSET_TBL_B_ROWS(X) \
	X(MT7996_PHY_WFCOMM_2004(0), 0x0050c3d9u, 0x0050c2d9u) \
	X(MT7996_PHY_BN_8210(0), 0xf0a1282au, 0x70a1282au) \
	X(MT7996_PHY_BN_8380(0), 0x11120001u, 0x10120001u) \
	X(MT7996_PHY_WFCOMM_F00C(0), 0x05050518u, 0x04040518u) \
	X(MT7996_PHY_BN_8D84(0), 0x0000d4d4u, 0x8000d4d4u) \
	X(MT7996_PHY_WFCOMM_21E0(0), 0xeca8aba9u, 0xeca8a9a9u, 0xecabababu) \
	X(MT7996_PHY_CHAIN_E07C(0, 0), 0x00d84295u, 0x00d95295u, 0x00d74295u, 0x00d75295u) \
	X(MT7996_PHY_CHAIN_E07C(0, 1), 0x00d84295u, 0x00d95295u, 0x00d74295u, 0x00d75295u) \
	X(MT7996_PHY_CHAIN_E07C(0, 2), 0x00d84295u, 0x00d95295u, 0x00d74295u, 0x00d75295u) \
	X(MT7996_PHY_CHAIN_E07C(0, 3), 0x00d84295u, 0x00d95295u, 0x00d74295u, 0x00d75295u) \
	X(MT7996_PHY_CHAIN_E07C(0, 4), 0x00d84295u, 0x00d95295u, 0x00d74295u, 0x00d75295u) \
	X(MT7996_PHY_CSD_0FC(0, 0), 0x52045000u, 0x52056000u) \
	X(MT7996_PHY_CSD_0FC(0, 1), 0x52045000u, 0x52056000u) \
	X(MT7996_PHY_CSD_0FC(0, 2), 0x52045000u, 0x52056000u) \
	X(MT7996_PHY_CSD_0FC(0, 3), 0x52045000u, 0x52056000u) \
	X(MT7996_PHY_CSD_0FC(0, 4), 0x52045000u, 0x52056000u) \
	X(MT7996_PHY_BASE, 0x54804369u, 0x54837577u) \
	X(MT7996_PHY_CSD_00(0, 1), 0x54804369u, 0x54837577u) \
	X(MT7996_PHY_CSD_00(0, 2), 0x54804369u, 0x54837577u) \
	X(MT7996_PHY_CSD_00(0, 3), 0x54804369u, 0x54837577u) \
	X(MT7996_PHY_CSD_00(0, 4), 0x54804369u, 0x54837577u) \
	X(MT7996_PHY_CSD_0EC(0, 0), 0x0610a208u, 0x0000008du) \
	X(MT7996_PHY_CSD_0EC(0, 1), 0x0610a208u, 0x0000008du) \
	X(MT7996_PHY_CSD_0EC(0, 2), 0x0610a208u, 0x0000008du) \
	X(MT7996_PHY_CSD_0EC(0, 3), 0x0610a208u, 0x0000008du) \
	X(MT7996_PHY_CSD_0EC(0, 4), 0x0610a208u, 0x0000008du) \
	X(MT7996_PHY_CSD_400(0, 0), 0xb5cf3a00u, 0xb5cf3e02u) \
	X(MT7996_PHY_CSD_1E0(0, 0), 0x0000003au, 0x0000003eu) \
	X(MT7996_PHY_CSD_1DC(0, 0), 0x3a3a3a3au, 0x3e3e3e3eu) \
	X(MT7996_PHY_CSD_400(0, 1), 0xb5cf3a00u, 0xb5cf3e02u) \
	X(MT7996_PHY_CSD_1E0(0, 1), 0x0000003au, 0x0000003eu) \
	X(MT7996_PHY_CSD_1DC(0, 1), 0x3a3a3a3au, 0x3e3e3e3eu) \
	X(MT7996_PHY_CSD_400(0, 2), 0xb5cf3a00u, 0xb5cf3e02u) \
	X(MT7996_PHY_CSD_1E0(0, 2), 0x0000003au, 0x0000003eu) \
	X(MT7996_PHY_CSD_1DC(0, 2), 0x3a3a3a3au, 0x3e3e3e3eu) \
	X(MT7996_PHY_CSD_400(0, 3), 0xb5cf3a00u, 0xb5cf3e02u) \
	X(MT7996_PHY_CSD_1E0(0, 3), 0x0000003au, 0x0000003eu) \
	X(MT7996_PHY_CSD_1DC(0, 3), 0x3a3a3a3au, 0x3e3e3e3eu) \
	X(MT7996_PHY_CSD_400(0, 4), 0xb5cf3a00u, 0xb5cf3e02u) \
	X(MT7996_PHY_CSD_1E0(0, 4), 0x0000003au, 0x0000003eu) \
	X(MT7996_PHY_CSD_1DC(0, 4), 0x3a3a3a3au, 0x3e3e3e3eu) \
	X(MT7996_PHY_AGC_TH0(0, 0), 0x003a797fu, 0x003e777fu) \
	X(MT7996_PHY_AGC_TH0(0, 1), 0x003a797fu, 0x003e777fu) \
	X(MT7996_PHY_AGC_TH0(0, 2), 0x003a797fu, 0x003e777fu) \
	X(MT7996_PHY_AGC_TH0(0, 3), 0x003a797fu, 0x003e777fu) \
	X(MT7996_PHY_AGC_TH0(0, 4), 0x003a797fu, 0x003e777fu) \
	X(MT7996_PHY_CSD_004(0, 0), 0x00363535u, 0x00363636u) \
	X(MT7996_PHY_CSD_008(0, 0), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_004(0, 1), 0x00363535u, 0x00363636u) \
	X(MT7996_PHY_CSD_008(0, 1), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_004(0, 2), 0x00363535u, 0x00363636u) \
	X(MT7996_PHY_CSD_008(0, 2), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_004(0, 3), 0x00363535u, 0x00363636u) \
	X(MT7996_PHY_CSD_008(0, 3), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_004(0, 4), 0x00363535u, 0x00363636u) \
	X(MT7996_PHY_CSD_008(0, 4), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_038(0, 0), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_038(0, 1), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_038(0, 2), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_038(0, 3), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_038(0, 4), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_024(0, 0), 0x00353535u, 0x00363636u) \
	X(MT7996_PHY_CSD_028(0, 0), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_024(0, 1), 0x00353535u, 0x00363636u) \
	X(MT7996_PHY_CSD_028(0, 1), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_024(0, 2), 0x00353535u, 0x00363636u) \
	X(MT7996_PHY_CSD_028(0, 2), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_024(0, 3), 0x00353535u, 0x00363636u) \
	X(MT7996_PHY_CSD_028(0, 3), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_024(0, 4), 0x00353535u, 0x00363636u) \
	X(MT7996_PHY_CSD_028(0, 4), 0x35000000u, 0x36000000u) \
	X(MT7996_PHY_CSD_054(0, 0), 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_058(0, 0), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_054(0, 1), 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_058(0, 1), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_054(0, 2), 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_058(0, 2), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_054(0, 3), 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_058(0, 3), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_054(0, 4), 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_058(0, 4), 0x39000000u, 0x3a000000u) \
	X(MT7996_PHY_CSD_1D0(0, 0), 0x00000002u, 0x08420842u) \
	X(MT7996_PHY_CSD_1D0(0, 1), 0x00000002u, 0x08420842u) \
	X(MT7996_PHY_CSD_1D0(0, 2), 0x00000002u, 0x08420842u) \
	X(MT7996_PHY_CSD_1D0(0, 3), 0x00000002u, 0x08420842u) \
	X(MT7996_PHY_CSD_1D0(0, 4), 0x00000002u, 0x08420842u) \
	X(MT7996_PHY_CSD_D4(0, 0), 0x0d3fccfcu, 0x0ccfccfcu) \
	X(MT7996_PHY_CSD_D4(0, 1), 0x0d3fccfcu, 0x0ccfccfcu) \
	X(MT7996_PHY_CSD_D4(0, 2), 0x0d3fccfcu, 0x0ccfccfcu) \
	X(MT7996_PHY_CSD_D4(0, 3), 0x0d3fccfcu, 0x0ccfccfcu) \
	X(MT7996_PHY_CSD_D4(0, 4), 0x0d3fccfcu, 0x0ccfccfcu) \
	X(MT7996_PHY_BN_6138(0), 0x048da324u, 0x048da2d1u) \
	X(MT7996_PHY_BN_825C(0), 0x810a9c23u, 0x810a9c03u) \
	X(MT7996_PHY_BN_8D08(0), 0x00d4d4c9u, 0x00d4d4c8u) \
	X(MT7996_PHY_CSD_RXGOFS_188(0, 0), 0x0b131e26u, 0x0b11181eu, 0x0b11171du, 0x0b131b23u, 0x0a10161cu) \
	X(MT7996_PHY_CSD_RXGOFS_0F8(0, 0), 0x2e000000u, 0x24000000u, 0x23000000u, 0x2b000000u, 0x22000000u) \
	X(MT7996_PHY_CSD_RXGOFS_188(0, 1), 0x0b131e26u, 0x0b11181eu, 0x0b11171du, 0x0b131b23u, 0x0a10161cu) \
	X(MT7996_PHY_CSD_RXGOFS_0F8(0, 1), 0x2e000000u, 0x24000000u, 0x23000000u, 0x2b000000u, 0x22000000u) \
	X(MT7996_PHY_CSD_RXGOFS_188(0, 2), 0x0b131e26u, 0x0b11181eu, 0x0b11171du, 0x0b131b23u, 0x0a10161cu) \
	X(MT7996_PHY_CSD_RXGOFS_0F8(0, 2), 0x2e000000u, 0x24000000u, 0x23000000u, 0x2b000000u, 0x22000000u) \
	X(MT7996_PHY_CSD_RXGOFS_188(0, 3), 0x0b131e26u, 0x0b11181eu, 0x0b11171du, 0x0b131b23u, 0x0a10161cu) \
	X(MT7996_PHY_CSD_RXGOFS_0F8(0, 3), 0x2e000000u, 0x24000000u, 0x23000000u, 0x2b000000u, 0x22000000u) \
	X(MT7996_PHY_CSD_RXGOFS_188(0, 4), 0x0b131e26u, 0x0b11181eu, 0x0b11171du, 0x0b131b23u, 0x0a10161cu) \
	X(MT7996_PHY_CSD_RXGOFS_0F8(0, 4), 0x2e000000u, 0x24000000u, 0x23000000u, 0x2b000000u, 0x22000000u) \
	X(MT7996_PHY_CSD_RXGOFS_160(0, 0), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_RXGOFS_164(0, 0), 0x01de01f3u, 0x01ea01fau, 0x01eb01fau, 0x01e401f4u, 0x01f001fcu) \
	X(MT7996_PHY_CSD_RXGOFS_160(0, 1), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_RXGOFS_164(0, 1), 0x01de01f3u, 0x01ea01fau, 0x01eb01fau, 0x01e401f4u, 0x01f001fcu) \
	X(MT7996_PHY_CSD_RXGOFS_160(0, 2), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_RXGOFS_164(0, 2), 0x01de01f3u, 0x01ea01fau, 0x01eb01fau, 0x01e401f4u, 0x01f001fcu) \
	X(MT7996_PHY_CSD_RXGOFS_160(0, 3), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_RXGOFS_164(0, 3), 0x01de01f3u, 0x01ea01fau, 0x01eb01fau, 0x01e401f4u, 0x01f001fcu) \
	X(MT7996_PHY_CSD_RXGOFS_160(0, 4), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_RXGOFS_164(0, 4), 0x01de01f3u, 0x01ea01fau, 0x01eb01fau, 0x01e401f4u, 0x01f001fcu) \
	X(MT7996_PHY_CSD_1B8(0, 0), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_1BC(0, 0), 0x01de00c4u, 0x01ea00c4u, 0x01eb00c4u, 0x01e400c4u, 0x01f000c4u) \
	X(MT7996_PHY_CSD_1B8(0, 1), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_1BC(0, 1), 0x01de00c4u, 0x01ea00c4u, 0x01eb00c4u, 0x01e400c4u, 0x01f000c4u) \
	X(MT7996_PHY_CSD_1B8(0, 2), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_1BC(0, 2), 0x01de00c4u, 0x01ea00c4u, 0x01eb00c4u, 0x01e400c4u, 0x01f000c4u) \
	X(MT7996_PHY_CSD_1B8(0, 3), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_1BC(0, 3), 0x01de00c4u, 0x01ea00c4u, 0x01eb00c4u, 0x01e400c4u, 0x01f000c4u) \
	X(MT7996_PHY_CSD_1B8(0, 4), 0x01be01ceu, 0x01d301dfu, 0x01d601e1u, 0x01c401d4u, 0x01d801e4u) \
	X(MT7996_PHY_CSD_1BC(0, 4), 0x01de00c4u, 0x01ea00c4u, 0x01eb00c4u, 0x01e400c4u, 0x01f000c4u) \
	X(MT7996_PHY_CSD_RXGOFS_390(0, 0), 0x2b2b2b00u, 0x2a2b2b00u, 0x292b2b00u, 0x282b2b00u) \
	X(MT7996_PHY_CSD_RXGOFS_390(0, 1), 0x2b2b2b00u, 0x2a2b2b00u, 0x292b2b00u, 0x282b2b00u) \
	X(MT7996_PHY_CSD_RXGOFS_390(0, 2), 0x2b2b2b00u, 0x2a2b2b00u, 0x292b2b00u, 0x282b2b00u) \
	X(MT7996_PHY_CSD_RXGOFS_390(0, 3), 0x2b2b2b00u, 0x2a2b2b00u, 0x292b2b00u, 0x282b2b00u) \
	X(MT7996_PHY_CSD_RXGOFS_390(0, 4), 0x2b2b2b00u, 0x2a2b2b00u, 0x292b2b00u, 0x282b2b00u) \
	X(MT7996_PHY_CSD_RXGOFS_408(0, 0), 0x01d401d4u, 0x01c801c8u, 0x01ca01cau, 0x01cc01ccu) \
	X(MT7996_PHY_CSD_RXGOFS_408(0, 1), 0x01d401d4u, 0x01c801c8u, 0x01ca01cau, 0x01cc01ccu) \
	X(MT7996_PHY_CSD_RXGOFS_408(0, 2), 0x01d401d4u, 0x01c801c8u, 0x01ca01cau, 0x01cc01ccu) \
	X(MT7996_PHY_CSD_RXGOFS_408(0, 3), 0x01d401d4u, 0x01c801c8u, 0x01ca01cau, 0x01cc01ccu) \
	X(MT7996_PHY_CSD_RXGOFS_408(0, 4), 0x01d401d4u, 0x01c801c8u, 0x01ca01cau, 0x01cc01ccu) \
	X(MT7996_PHY_CSD_104(0, 0), 0x01234800u, 0x01345800u, 0x01345bf0u) \
	X(MT7996_PHY_CSD_104(0, 1), 0x01234800u, 0x01345800u, 0x01345bf0u) \
	X(MT7996_PHY_CSD_104(0, 2), 0x01234800u, 0x01345800u, 0x01345bf0u) \
	X(MT7996_PHY_CSD_104(0, 3), 0x01234800u, 0x01345800u, 0x01345bf0u) \
	X(MT7996_PHY_CSD_104(0, 4), 0x01234800u, 0x01345800u, 0x01345bf0u) \
	X(MT7996_PHY_CSD_1C4(0, 0), 0x01f30000u, 0x01fa0000u, 0x01f40000u, 0x01fc0000u) \
	X(MT7996_PHY_CSD_1C4(0, 1), 0x01f30000u, 0x01fa0000u, 0x01f40000u, 0x01fc0000u) \
	X(MT7996_PHY_CSD_1C4(0, 2), 0x01f30000u, 0x01fa0000u, 0x01f40000u, 0x01fc0000u) \
	X(MT7996_PHY_CSD_1C4(0, 3), 0x01f30000u, 0x01fa0000u, 0x01f40000u, 0x01fc0000u) \
	X(MT7996_PHY_CSD_1C4(0, 4), 0x01f30000u, 0x01fa0000u, 0x01f40000u, 0x01fc0000u) \
	X(MT7996_PHY_CSD_1B4(0, 0), 0x04000000u, 0x0400003du, 0x05000000u, 0x0500003du, 0x0500003eu) \
	X(MT7996_PHY_CSD_1B4(0, 1), 0x04000000u, 0x0400003du, 0x05000000u, 0x0500003du, 0x0500003eu) \
	X(MT7996_PHY_CSD_1B4(0, 2), 0x04000000u, 0x0400003du, 0x05000000u, 0x0500003du, 0x0500003eu) \
	X(MT7996_PHY_CSD_1B4(0, 3), 0x04000000u, 0x0400003du, 0x05000000u, 0x0500003du, 0x0500003eu) \
	X(MT7996_PHY_CSD_1B4(0, 4), 0x04000000u, 0x0400003du, 0x05000000u, 0x0500003du, 0x0500003eu) \
	X(MT7996_PHY_CSD_034(0, 0), 0x00393a39u, 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_034(0, 1), 0x00393a39u, 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_034(0, 2), 0x00393a39u, 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_034(0, 3), 0x00393a39u, 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_034(0, 4), 0x00393a39u, 0x00393939u, 0x003a3a3au) \
	X(MT7996_PHY_CSD_16C(0, 0), 0x000f3800u, 0x000f3a00u, 0x000f3d00u, 0x000f3f00u, 0x000f7f00u) \
	X(MT7996_PHY_CSD_16C(0, 1), 0x000f3800u, 0x000f3a00u, 0x000f3d00u, 0x000f3f00u, 0x000f7f00u) \
	X(MT7996_PHY_CSD_16C(0, 2), 0x000f3800u, 0x000f3a00u, 0x000f3d00u, 0x000f3f00u, 0x000f7f00u) \
	X(MT7996_PHY_CSD_16C(0, 3), 0x000f3800u, 0x000f3a00u, 0x000f3d00u, 0x000f3f00u, 0x000f7f00u) \
	X(MT7996_PHY_CSD_16C(0, 4), 0x000f3800u, 0x000f3a00u, 0x000f3d00u, 0x000f3f00u, 0x000f7f00u) \
	X(MT7996_PHY_CSD_14C(0, 0), 0x00380000u, 0x003a0000u) \
	X(MT7996_PHY_CSD_10C(0, 0), 0x1000300cu, 0x1000340cu) \
	X(MT7996_PHY_CSD_14C(0, 1), 0x00380000u, 0x003a0000u) \
	X(MT7996_PHY_CSD_10C(0, 1), 0x1000300cu, 0x1000340cu) \
	X(MT7996_PHY_CSD_14C(0, 2), 0x00380000u, 0x003a0000u) \
	X(MT7996_PHY_CSD_10C(0, 2), 0x1000300cu, 0x1000340cu) \
	X(MT7996_PHY_CSD_14C(0, 3), 0x00380000u, 0x003a0000u) \
	X(MT7996_PHY_CSD_10C(0, 3), 0x1000300cu, 0x1000340cu) \
	X(MT7996_PHY_CSD_14C(0, 4), 0x00380000u, 0x003a0000u) \
	X(MT7996_PHY_CSD_10C(0, 4), 0x1000300cu, 0x1000340cu) \
	X(MT7996_PHY_BN_83C0(0), 0x00000203u, 0x00000201u) \
	X(MT7996_PHY_BN_8308(0), 0x0ed8ec16u, 0x0ed8ac18u) \
	X(MT7996_PHY_BN_830C(0), 0x0006dd6au, 0x00071d6au) \
	X(MT7996_PHY_BN_8300(0), 0x0ad8e616u, 0x0ad8a618u) \
	X(MT7996_PHY_BN_8304(0), 0x09c6d964u, 0x09c71964u) \
	X(MT7996_PHY_BN_8324(0), 0x160e3453u, 0x180a3454u) \
	X(MT7996_PHY_BN_83F0(0), 0x00000000u, 0x02000008u) \
	X(MT7996_PHY_BN_83F4(0), 0x00000000u, 0x05000001u) \
	X(MT7996_PHY_BN_8D00(0), 0x00000000u, 0x01000004u) \
	X(MT7996_PHY_BN_8D04(0), 0x00000000u, 0x07000001u) \
	X(MT7996_PHY_WFCOMM_702C(0), 0x00c4c9ceu, 0x00000000u) \
	X(MT7996_PHY_BN_83C0(0), 0x00000203u, 0x00000201u) \
	X(MT7996_PHY_CSD_148(0, 0), 0x003c0000u, 0x003d0000u) \
	X(MT7996_PHY_CSD_244(0, 0), 0x00000038u, 0x0000003au) \
	X(MT7996_PHY_CSD_248(0, 0), 0x00000070u, 0x00000074u) \
	X(MT7996_PHY_CSD_148(0, 1), 0x003c0000u, 0x003d0000u) \
	X(MT7996_PHY_CSD_244(0, 1), 0x00000038u, 0x0000003au) \
	X(MT7996_PHY_CSD_248(0, 1), 0x00000070u, 0x00000074u) \
	X(MT7996_PHY_CSD_148(0, 2), 0x003c0000u, 0x003d0000u) \
	X(MT7996_PHY_CSD_244(0, 2), 0x00000038u, 0x0000003au) \
	X(MT7996_PHY_CSD_248(0, 2), 0x00000070u, 0x00000074u) \
	X(MT7996_PHY_CSD_148(0, 3), 0x003c0000u, 0x003d0000u) \
	X(MT7996_PHY_CSD_244(0, 3), 0x00000038u, 0x0000003au) \
	X(MT7996_PHY_CSD_248(0, 3), 0x00000070u, 0x00000074u) \
	X(MT7996_PHY_CSD_148(0, 4), 0x003c0000u, 0x003d0000u) \
	X(MT7996_PHY_CSD_244(0, 4), 0x00000038u, 0x0000003au) \
	X(MT7996_PHY_CSD_248(0, 4), 0x00000070u, 0x00000074u) \
	X(MT7996_PHY_DFE_F160(0), 0x80016016u, 0x80001001u) \
	X(MT7996_PHY_WFCOMM_7094(0), 0x0201004du, 0x02010125u) \
	X(MT7996_PHY_WFCOMM_2008(0), 0x938e5a28u, 0x92cb5a28u) \
	X(MT7996_PHY_BN_83B4(0), 0x000007bdu, 0x0000063fu) \
	X(MT7996_PHY_CSD_1C0(0, 0), 0xc4000000u, 0xc400003du, 0xc400003eu) \
	X(MT7996_PHY_CSD_1C0(0, 1), 0xc4000000u, 0xc400003du, 0xc400003eu) \
	X(MT7996_PHY_CSD_1C0(0, 2), 0xc4000000u, 0xc400003du, 0xc400003eu) \
	X(MT7996_PHY_CSD_1C0(0, 3), 0xc4000000u, 0xc400003du, 0xc400003eu) \
	X(MT7996_PHY_CSD_1C0(0, 4), 0xc4000000u, 0xc400003du, 0xc400003eu) \
	X(MT7996_PHY_WFCOMM_7278(0), 0x01b4c4f6u, 0x01b7c4f6u, 0x01bac4f6u) \
	X(MT7996_PHY_CSD_140(0, 0), 0xa9088801u, 0xac088801u, 0xaf088801u) \
	X(MT7996_PHY_CSD_140(0, 1), 0xa9088801u, 0xac088801u, 0xaf088801u) \
	X(MT7996_PHY_CSD_140(0, 2), 0xa9088801u, 0xac088801u, 0xaf088801u) \
	X(MT7996_PHY_CSD_140(0, 3), 0xa9088801u, 0xac088801u, 0xaf088801u) \
	X(MT7996_PHY_CSD_140(0, 4), 0xa9088801u, 0xac088801u, 0xaf088801u) \
	X(MT7996_PHY_BN_83C8(0), 0x00000011u, 0x00000013u, 0x00000015u) \
	X(MT7996_PHY_BN_62A0(0), 0x7ad5aad5u, 0x8af5eb56u, 0xab362c57u) \
	X(MT7996_PHY_BN_62A4(0), 0xdbb562c5u, 0x5eb96ed5u, 0x61bf76e5u) \
	X(MT7996_PHY_BN_62A8(0), 0xb56ab56eu, 0xbd7ad5b0u, 0xcd8b15f1u) \
	X(MT7996_PHY_BN_62AC(0), 0xed58b15eu, 0xae5bb562u, 0x6fddb96au) \
	X(MT7996_PHY_BN_62B0(0), 0x56ad5bb6u, 0x5ab55c17u, 0x5eb56c58u) \
	X(MT7996_PHY_BN_62B4(0), 0xd5ab55abu, 0x15ebd6adu, 0x566c57afu) \
	X(MT7996_PHY_BN_62B8(0), 0xab56c57au, 0xad56d59bu, 0xad5ae5bbu) \
	X(MT7996_PHY_BN_62BC(0), 0x5ab56ad5u, 0x7af5ab56u, 0x8b15eb57u) \
	X(MT7996_PHY_BN_62C0(0), 0xd5af5eb5u, 0xd5b362bdu, 0x56b76acdu) \
	X(MT7996_PHY_BN_62C4(0), 0xad5ab56au, 0xb56ad5aau, 0xbd7ad5abu) \
	X(MT7996_PHY_BN_62C8(0), 0x2b56ab56u, 0x2c57af5au, 0x2cd8b15eu) \
	X(MT7996_PHY_BN_8200(0), 0xc22145d7u, 0xc22155d7u, 0xc221a7a3u, 0xc22147e7u, 0xe20145d7u, 0xe20155d7u, 0xe201a7a3u, 0xe20147e7u, 0xc20145d7u, 0xc20155d7u, 0xc201a7a3u, 0xc20147e7u) \
	X(MT7996_PHY_BN_829C(0), 0x00000002u, 0x80000002u) \
	X(MT7996_PHY_BN_82D0(0), 0x00500156u, 0x00500150u) \
	X(MT7996_PHY_WFCOMM_7448(0), 0x302d0404u, 0x3024c304u) \
	X(MT7996_PHY_BN_8900(0), 0x0b033c55u, 0xeb033c55u) \
	X(MT7996_PHY_BN_8D60(0), 0x200c1865u, 0x200c1c65u, 0xa00c1865u, 0xa00c1c65u) \
	X(MT7996_PHY_BN_8D70(0), 0xf2200000u, 0x32200000u) \
	X(MT7996_PHY_WFCOMM_6108(0), 0x21071255u, 0x21071455u) \
	X(MT7996_PHY_WFCOMM_6124(0), 0xa2884210u, 0xc3084210u) \
	X(MT7996_PHY_BN_8554(0), 0xd8c5c2c1u, 0xd8c5c2c0u, 0xd8c1bebdu, 0xd8c1bebcu, 0xd8c1bebbu, 0xd8c1bebau, 0xd8c1beb9u, 0xd8c1beb8u) \
	X(MT7996_PHY_BN_838C(0), 0xcdcdcdcdu, 0xc8c8c8c8u, 0xc6c6c6c6u) \
	X(MT7996_PHY_BN_8010(0), 0x150b0a0au, 0x150b0a0du, 0x150b0d0cu, 0x150b0908u, 0x150b0808u, 0x150b0b0au) \
	X(MT7996_PHY_BN_8014(0), 0xea0b0b0bu, 0xea070b0bu) \
	X(MT7996_PHY_BN_8018(0), 0x00f6128fu, 0x00f5728fu) \
	X(MT7996_PHY_DFE_F130(0), 0x40000000u, 0x00000000u) \
	X(MT7996_PHY_BN_60B0(0), 0x44400000u, 0x44440000u) \
	X(MT7996_PHY_WFCOMM_7064(0), 0x22222220u, 0x22222224u) \
	X(MT7996_PHY_BN_83B8(0), 0x00000f00u, 0x00000a00u) \
	X(MT7996_PHY_CSD_11C(0, 0), 0x00f9bcefu, 0x00f7bcefu) \
	X(MT7996_PHY_CSD_11C(0, 1), 0x00f9bcefu, 0x00f7bcefu) \
	X(MT7996_PHY_CSD_11C(0, 2), 0x00f9bcefu, 0x00f7bcefu) \
	X(MT7996_PHY_CSD_11C(0, 3), 0x00f9bcefu, 0x00f7bcefu) \
	X(MT7996_PHY_CSD_11C(0, 4), 0x00f9bcefu, 0x00f7bcefu) \
	X(MT7996_PHY_CSD_124(0, 0), 0x13000000u, 0x0f000000u) \
	X(MT7996_PHY_CSD_124(0, 1), 0x13000000u, 0x0f000000u) \
	X(MT7996_PHY_CSD_124(0, 2), 0x13000000u, 0x0f000000u) \
	X(MT7996_PHY_CSD_124(0, 3), 0x13000000u, 0x0f000000u) \
	X(MT7996_PHY_CSD_124(0, 4), 0x13000000u, 0x0f000000u) \
	X(MT7996_PHY_WFCOMM_0000(0), 0x03ff2102u, 0x03ff2101u) \
	X(MT7996_PHY_WFCOMM_0004(0), 0x13331413u, 0x03331413u) \
	X(MT7996_PHY_WFCOMM_0050(0), 0x00000214u, 0x00000210u, 0x00000215u) \
	X(MT7996_PHY_BN_820C(0), 0x900120b5u, 0x90011d30u, 0x90011120u) \
	X(MT7996_PHY_BN_82E0(0), 0x013920b5u, 0x01391d30u, 0x01391120u) \
	X(MT7996_PHY_DFE_F170(0), 0x0c500000u, 0x19a00000u, 0x1ec00000u) \
	X(MT7996_PHY_BN_8D78(0), 0x00201017u, 0x0020121au, 0x00200f16u, 0x00281017u, 0x0028121au, 0x00280f16u) \
	X(MT7996_PHY_BN_8D7C(0), 0x14220018u, 0x17220020u, 0x11220018u, 0x14250018u, 0x17250020u, 0x11250018u) \
	X(MT7996_PHY_BN_82BC(0), 0x0881a405u, 0x2881a405u, 0x4881a405u, 0x6881a405u, 0x6880f405u) \
	X(MT7996_PHY_WFCOMM_7160(0), 0x0040104cu, 0x0048104cu) \
	X(MT7996_PHY_CSD_1FC(0, 0), 0x906b707au, 0x906b747au) \
	X(MT7996_PHY_CSD_1FC(0, 1), 0x906b707au, 0x906b747au) \
	X(MT7996_PHY_CSD_1FC(0, 2), 0x906b707au, 0x906b747au) \
	X(MT7996_PHY_CSD_1FC(0, 3), 0x906b707au, 0x906b747au) \
	X(MT7996_PHY_CSD_1FC(0, 4), 0x906b707au, 0x906b747au) \
	X(MT7996_PHY_CSD_230(0, 0), 0x38003e00u, 0x3a003e00u) \
	X(MT7996_PHY_CSD_230(0, 1), 0x38003e00u, 0x3a003e00u) \
	X(MT7996_PHY_CSD_230(0, 2), 0x38003e00u, 0x3a003e00u) \
	X(MT7996_PHY_CSD_230(0, 3), 0x38003e00u, 0x3a003e00u) \
	X(MT7996_PHY_CSD_230(0, 4), 0x38003e00u, 0x3a003e00u) \
	X(MT7996_PHY_BN_60C4(0), 0x00000a65u, 0x00000a4du, 0x00000bfdu, 0x00000addu) \
	X(MT7996_PHY_BN_8204(0), 0x88312555u, 0x782d3555u, 0xaa398721u, 0x676d2763u) \
	X(MT7996_PHY_BN_8910(0), 0x1420128cu, 0x1520137bu, 0x1a2818aeu, 0x141d126bu) \
	X(MT7996_PHY_BN_8914(0), 0x0a404135u, 0x0a403a30u, 0x0d403a30u, 0x0a402220u) \
	X(MT7996_PHY_BN_8934(0), 0x0a404135u, 0x0a403a30u, 0x0d403a30u, 0x0a402220u) \
	X(MT7996_PHY_BN_8954(0), 0x0a404135u, 0x0a403a30u, 0x0d403a30u, 0x0a402220u) \
	X(MT7996_PHY_BN_8974(0), 0x0a404135u, 0x0a403a30u, 0x0d403a30u, 0x0a402220u) \
	X(MT7996_PHY_BN_8208(0), 0x0a9c9c00u, 0x0d9c9c00u) \
	X(MT7996_PHY_BN_8D74(0), 0x10112200u, 0x10132200u) \
	X(MT7996_PHY_BN_8E14(0), 0x33000000u, 0x00000000u, 0x11000000u, 0x30000000u, 0x31000000u) \
	X(MT7996_PHY_BN_82D4(0), 0x000545d7u, 0x000555d7u, 0x0005a7a3u, 0x000547e7u, 0x000587e7u, 0x000585d7u, 0x0005c7a3u, 0x0005c7e7u) \
	X(MT7996_PHY_BN_82D8(0), 0x88312555u, 0x782d3555u, 0xaa398721u, 0x676d2763u, 0x782d6555u, 0xaa39a721u, 0x876d6763u) \
	X(MT7996_PHY_BN_82DC(0), 0x14000000u, 0x1a000000u, 0x18000000u) \
	X(MT7996_PHY_BN_8E18(0), 0x30000000u, 0x00000000u, 0x10000000u) \
	X(MT7996_PHY_BN_8E1C(0), 0x30000000u, 0x00000000u, 0x10000000u) \
	X(MT7996_PHY_BN_8E20(0), 0x30000000u, 0x00000000u, 0x10000000u) \
	X(MT7996_PHY_BN_8E24(0), 0x30000000u, 0x00000000u, 0x10000000u) \
	X(MT7996_PHY_BN_8918(0), 0x17151715u, 0x1e1c2321u, 0x1f1d2723u) \
	X(MT7996_PHY_BN_8938(0), 0x17151715u, 0x1e1c2321u, 0x1f1d2723u) \
	X(MT7996_PHY_BN_8958(0), 0x17151715u, 0x1e1c2321u, 0x1f1d2723u) \
	X(MT7996_PHY_BN_8978(0), 0x17151715u, 0x1e1c2321u, 0x1f1d2723u) \
	X(MT7996_PHY_BN_8D80(0), 0x01500000u, 0x014c0000u, 0x014a0000u) \
	X(MT7996_PHY_BN_8924(0), 0x02111710u, 0x02131a12u, 0x0211160fu) \
	X(MT7996_PHY_BN_8928(0), 0x14182220u, 0x17202c20u, 0x1118281du) \
	X(MT7996_PHY_BN_8944(0), 0x02111710u, 0x02131a12u, 0x0211160fu) \
	X(MT7996_PHY_BN_8948(0), 0x14182220u, 0x17202c20u, 0x1118281du) \
	X(MT7996_PHY_BN_8964(0), 0x08111710u, 0x08131a12u, 0x0811160fu) \
	X(MT7996_PHY_BN_8968(0), 0x14182220u, 0x17202c20u, 0x1118281du) \
	X(MT7996_PHY_BN_8984(0), 0x02111710u, 0x02131a12u, 0x0211160fu) \
	X(MT7996_PHY_BN_8988(0), 0x14182220u, 0x17202c20u, 0x1118281du) \
	X(MT7996_PHY_BN_8930(0), 0x1420128cu, 0x1520137bu, 0x1a2818aeu, 0x141d126bu, 0x181d126bu) \
	X(MT7996_PHY_BN_8950(0), 0x1420128cu, 0x1520137bu, 0x1a2818aeu, 0x141d126bu, 0x181d126bu) \
	X(MT7996_PHY_BN_8970(0), 0x1420128cu, 0x1520137bu, 0x1a2818aeu, 0x141d126bu, 0x181d126bu) \
	X(MT7996_PHY_BN_8920(0), 0x07015622u, 0x07015022u, 0x01015622u, 0x01015022u) \
	X(MT7996_PHY_BN_8940(0), 0x07015622u, 0x07015022u, 0x01015622u) \
	X(MT7996_PHY_BN_8960(0), 0x07015622u, 0x07015022u, 0x01015622u) \
	X(MT7996_PHY_BN_8980(0), 0x07015612u, 0x07015012u, 0x01015612u) \
	X(MT7996_PHY_BN_82F8(0), 0x00aa542cu, 0x00aa342cu) \
	X(MT7996_PHY_DFE_F020(0), 0x020d0001u, 0x020d0005u) \
	X(MT7996_PHY_DFE_F024(0), 0x00150028u, 0x00110013u) \
	X(MT7996_PHY_DFE_F028(0), 0x000a07bau, 0x07f307c4u) \
	X(MT7996_PHY_DFE_F02C(0), 0x07a80046u, 0x07d70065u) \
	X(MT7996_PHY_DFE_F030(0), 0x014e01d2u, 0x012b018au) \
	X(MT7996_PHY_WFCOMM_70BC(0), 0x329244d3u, 0x320a44d3u, 0x328a44d3u) \
	X(MT7996_PHY_BN_60C8(0), 0x1fd00100u, 0x1bd00100u, 0x1ed00100u) \
	X(MT7996_PHY_BN_8590(0), 0x0cab3222u, 0x0cabf222u) \
	X(MT7996_PHY_BN_85A0(0), 0x0cab3222u, 0x0cabf222u) \
	X(MT7996_PHY_BN_8E30(0), 0x0b020000u, 0xcb020000u) \
	X(MT7996_PHY_BN_8D68(0), 0x44449000u, 0x4444d000u) \
	X(MT7996_PHY_CSD_23C(0, 0), 0x0000003cu, 0x0000003du, 0x0000003bu) \
	X(MT7996_PHY_CSD_23C(0, 1), 0x0000003cu, 0x0000003du, 0x0000003bu) \
	X(MT7996_PHY_CSD_23C(0, 2), 0x0000003cu, 0x0000003du, 0x0000003bu) \
	X(MT7996_PHY_CSD_23C(0, 3), 0x0000003cu, 0x0000003du, 0x0000003bu) \
	X(MT7996_PHY_CSD_23C(0, 4), 0x0000003cu, 0x0000003du, 0x0000003bu) \
	X(MT7996_PHY_CSD_014(0, 0), 0x00003535u, 0x00003835u) \
	X(MT7996_PHY_CSD_014(0, 1), 0x00003535u, 0x00003835u) \
	X(MT7996_PHY_CSD_014(0, 2), 0x00003535u, 0x00003835u) \
	X(MT7996_PHY_CSD_014(0, 3), 0x00003535u, 0x00003835u) \
	X(MT7996_PHY_CSD_014(0, 4), 0x00003535u, 0x00003835u) \
	X(MT7996_PHY_WFCOMM_726C(0), 0x42d0b018u, 0x5ad6b018u)
#define MT7996_CHSET_TBL_B_KEYS { \
	0u, 15u, 30u, 45u, 60u, 3u, 18u, 33u, 48u, 63u, 1u, 16u, 31u, 46u, 61u, 4u, \
	19u, 34u, 49u, 64u, 7u, 22u, 37u, 52u, 67u, 10u, 25u, 40u, 55u, 70u, 2u, 17u, \
	32u, 47u, 62u, 5u, 20u, 35u, 50u, 65u, 8u, 23u, 38u, 53u, 68u, 11u, 26u, 41u, \
	56u, 71u, 14u, 29u, 44u, 59u, 74u, 75u, 90u, 105u, 120u, 135u, 78u, 93u, 108u, 123u, \
	138u, 76u, 91u, 106u, 121u, 136u, 79u, 94u, 109u, 124u, 139u, 82u, 97u, 112u, 127u, 142u, \
	85u, 100u, 115u, 130u, 145u, 77u, 92u, 107u, 122u, 137u, 80u, 95u, 110u, 125u, 140u, 83u, \
	98u, 113u, 128u, 143u, 86u, 101u, 116u, 131u, 146u, 89u, 104u, 119u, 134u, 149u, \
}
#define MT7996_CHSET_TBL_B_COLMAP { \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, \
	0u, 1u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, \
	1u, 1u, 0u, 0u, 2u, 1u, 2u, 2u, 2u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 3u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 0u, 2u, 1u, \
	1u, 0u, 0u, 3u, 0u, 1u, 3u, 3u, 0u, 1u, 2u, 3u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 3u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 0u, 2u, 1u, 1u, \
	0u, 0u, 3u, 0u, 1u, 3u, 3u, 0u, 1u, 2u, 3u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, \
	0u, 0u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, \
	1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, 1u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 0u, 1u, \
	1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, \
	0u, 1u, 1u, 0u, 0u, 0u, 1u, 2u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 2u, \
	1u, 2u, 2u, 2u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, \
	1u, 1u, 0u, 0u, 0u, 1u, 3u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, 1u, 0u, 0u, 2u, 0u, 2u, 1u, 1u, 0u, 1u, 3u, 0u, \
	1u, 3u, 3u, 0u, 1u, 2u, 3u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, \
	1u, 0u, 0u, 0u, 1u, 3u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, 1u, 0u, 0u, 2u, 0u, 2u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, \
	3u, 3u, 0u, 1u, 2u, 3u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, \
	1u, 0u, 0u, 0u, 4u, 0u, 2u, 1u, 0u, 2u, 1u, 2u, 0u, 0u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 1u, \
	0u, 0u, 0u, 5u, 1u, 3u, 0u, 0u, 2u, 1u, 2u, 0u, 0u, 1u, 1u, 1u, 1u, 3u, 1u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, 1u, \
	0u, 1u, 0u, 1u, 3u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 1u, 0u, \
	0u, 0u, 6u, 1u, 3u, 0u, 0u, 2u, 1u, 2u, 0u, 0u, 1u, 1u, 1u, 1u, 4u, 1u, 1u, 0u, 0u, 2u, 1u, 2u, 2u, 2u, 1u, \
	2u, 1u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 1u, 0u, 0u, \
	0u, 7u, 1u, 3u, 0u, 0u, 2u, 1u, 2u, 0u, 0u, 1u, 1u, 2u, 1u, 5u, 1u, 1u, 0u, 0u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, \
	2u, 4u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 1u, 0u, 0u, 0u, \
	7u, 1u, 3u, 0u, 0u, 2u, 1u, 2u, 0u, 0u, 1u, 1u, 2u, 1u, 5u, 1u, 1u, 0u, 0u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, 2u, \
	4u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 2u, 2u, 0u, 0u, 1u, 4u, \
	0u, 0u, 1u, 0u, 3u, 1u, 3u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	2u, 2u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 2u, 2u, 0u, 0u, 1u, 5u, 1u, \
	1u, 0u, 0u, 3u, 1u, 3u, 1u, 1u, 0u, 2u, 1u, 1u, 3u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 3u, \
	1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 2u, 2u, 0u, 0u, 1u, 6u, 1u, 1u, \
	0u, 0u, 3u, 1u, 3u, 1u, 1u, 0u, 2u, 1u, 1u, 4u, 1u, 1u, 0u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 2u, 1u, 2u, 1u, 1u, \
	0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 2u, 2u, 0u, 0u, 1u, 7u, 1u, 1u, 0u, \
	0u, 3u, 1u, 3u, 1u, 1u, 0u, 2u, 2u, 1u, 5u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, 2u, 4u, 1u, 1u, 0u, \
	1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 3u, 2u, 2u, 0u, 0u, 1u, 7u, 1u, 1u, 0u, 0u, \
	3u, 1u, 3u, 1u, 1u, 0u, 2u, 2u, 1u, 5u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, \
	2u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, 8u, 0u, 0u, 1u, 0u, 4u, \
	1u, 4u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, 2u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 1u, 0u, \
	1u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, 9u, 1u, 1u, 0u, 0u, 4u, 1u, \
	4u, 1u, 0u, 1u, 1u, 1u, 1u, 3u, 3u, 1u, 0u, 1u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 1u, 0u, 1u, \
	0u, 0u, 0u, 0u, 0u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, 10u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, \
	1u, 0u, 1u, 1u, 1u, 1u, 4u, 3u, 1u, 0u, 1u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, \
	0u, 0u, 0u, 0u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, 11u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, \
	0u, 1u, 1u, 2u, 1u, 5u, 3u, 1u, 0u, 1u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, \
	0u, 0u, 0u, 1u, 1u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, 11u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, \
	1u, 1u, 2u, 1u, 5u, 3u, 1u, 0u, 1u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 8u, 0u, 0u, 1u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, \
	1u, 0u, 1u, 0u, 2u, 1u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, \
	0u, 1u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 9u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, \
	1u, 1u, 3u, 3u, 1u, 0u, 2u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, \
	1u, 2u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 10u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, 1u, \
	1u, 4u, 3u, 1u, 0u, 2u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, \
	2u, 2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, 2u, 1u, \
	5u, 4u, 1u, 0u, 2u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 2u, \
	2u, 1u, 1u, 1u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, 2u, 1u, 5u, \
	4u, 1u, 0u, 2u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, \
	1u, 2u, 2u, 1u, 1u, 2u, 2u, 1u, 0u, 0u, 0u, 4u, 0u, 2u, 1u, 0u, 5u, 1u, 2u, 0u, 0u, 1u, 1u, 0u, 2u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, \
	2u, 2u, 1u, 1u, 2u, 2u, 1u, 0u, 0u, 0u, 5u, 1u, 3u, 0u, 0u, 5u, 1u, 2u, 0u, 0u, 1u, 1u, 1u, 2u, 3u, 1u, 1u, \
	0u, 0u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 3u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 2u, \
	2u, 1u, 1u, 2u, 2u, 1u, 0u, 0u, 0u, 6u, 1u, 3u, 0u, 0u, 5u, 1u, 2u, 0u, 0u, 1u, 1u, 1u, 2u, 4u, 1u, 1u, 0u, \
	0u, 2u, 1u, 2u, 2u, 2u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 2u, 2u, \
	1u, 1u, 2u, 2u, 1u, 0u, 0u, 0u, 7u, 1u, 3u, 0u, 0u, 5u, 1u, 2u, 0u, 0u, 1u, 1u, 2u, 2u, 5u, 1u, 1u, 0u, 0u, \
	3u, 0u, 1u, 4u, 3u, 0u, 1u, 2u, 4u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 2u, 2u, 1u, \
	1u, 2u, 2u, 1u, 0u, 0u, 0u, 7u, 1u, 3u, 0u, 0u, 5u, 1u, 2u, 0u, 0u, 1u, 1u, 2u, 2u, 5u, 1u, 1u, 0u, 0u, 3u, \
	0u, 1u, 4u, 3u, 0u, 1u, 2u, 4u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 2u, 2u, 1u, 1u, \
	3u, 2u, 2u, 0u, 0u, 1u, 4u, 0u, 0u, 1u, 0u, 6u, 1u, 3u, 1u, 0u, 1u, 1u, 0u, 2u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 2u, 2u, 1u, 1u, 3u, \
	2u, 2u, 0u, 0u, 1u, 5u, 1u, 1u, 0u, 0u, 6u, 1u, 3u, 1u, 1u, 0u, 2u, 1u, 2u, 3u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, \
	1u, 1u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 2u, 2u, 1u, 1u, 3u, 2u, \
	2u, 0u, 0u, 1u, 6u, 1u, 1u, 0u, 0u, 6u, 1u, 3u, 1u, 1u, 0u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 2u, 1u, 2u, 2u, \
	2u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 2u, 2u, 1u, 1u, 3u, 2u, 2u, \
	0u, 0u, 1u, 7u, 1u, 1u, 0u, 0u, 6u, 1u, 3u, 1u, 1u, 0u, 2u, 2u, 2u, 5u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, 4u, 3u, \
	0u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 2u, 2u, 1u, 1u, 3u, 2u, 2u, 0u, \
	0u, 1u, 7u, 1u, 1u, 0u, 0u, 6u, 1u, 3u, 1u, 1u, 0u, 2u, 2u, 2u, 5u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, 4u, 3u, 0u, \
	1u, 2u, 4u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, \
	2u, 8u, 0u, 0u, 1u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 0u, 2u, 0u, 2u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 2u, 2u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, \
	9u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 1u, 2u, 3u, 3u, 1u, 0u, 1u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, \
	1u, 3u, 1u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, 10u, \
	1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 1u, 2u, 4u, 3u, 1u, 0u, 1u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, \
	1u, 1u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, 11u, 1u, \
	1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 2u, 2u, 5u, 3u, 1u, 0u, 1u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, \
	1u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 0u, 2u, 11u, 1u, 1u, \
	0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 2u, 2u, 5u, 3u, 1u, 0u, 1u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, \
	0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 8u, 0u, 0u, 1u, \
	0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 0u, 2u, 0u, 2u, 1u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, \
	1u, 0u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 9u, 1u, 1u, 0u, 0u, \
	7u, 2u, 4u, 1u, 0u, 1u, 1u, 1u, 2u, 3u, 3u, 1u, 0u, 2u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 1u, \
	0u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 10u, 1u, 1u, 0u, 0u, 7u, \
	2u, 4u, 1u, 0u, 1u, 1u, 1u, 2u, 4u, 3u, 1u, 0u, 2u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 0u, \
	1u, 1u, 0u, 0u, 1u, 1u, 1u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 7u, 2u, \
	4u, 1u, 0u, 1u, 1u, 2u, 2u, 5u, 4u, 1u, 0u, 2u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, \
	1u, 1u, 0u, 1u, 1u, 1u, 2u, 2u, 1u, 2u, 2u, 1u, 1u, 4u, 2u, 3u, 0u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, \
	1u, 0u, 1u, 1u, 2u, 2u, 5u, 4u, 1u, 0u, 2u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, \
	1u, 0u, 1u, 1u, 1u, 2u, 3u, 1u, 2u, 2u, 2u, 1u, 4u, 2u, 4u, 1u, 1u, 2u, 8u, 0u, 0u, 1u, 0u, 7u, 2u, 4u, 1u, \
	0u, 1u, 1u, 0u, 2u, 0u, 2u, 1u, 1u, 3u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0u, 2u, 1u, 0u, \
	1u, 0u, 1u, 1u, 2u, 3u, 1u, 2u, 2u, 2u, 1u, 4u, 2u, 4u, 1u, 1u, 2u, 9u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, \
	1u, 1u, 1u, 2u, 3u, 3u, 1u, 1u, 3u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 0u, 0u, 2u, 1u, 0u, 1u, \
	0u, 1u, 1u, 2u, 3u, 1u, 2u, 2u, 2u, 1u, 4u, 2u, 4u, 1u, 1u, 2u, 10u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, \
	1u, 1u, 2u, 4u, 3u, 1u, 1u, 3u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 0u, 0u, 2u, 1u, 0u, 1u, 0u, \
	1u, 1u, 2u, 3u, 1u, 2u, 2u, 2u, 1u, 4u, 2u, 4u, 1u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, \
	2u, 2u, 5u, 4u, 1u, 1u, 3u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 0u, 0u, 2u, 1u, 1u, 1u, 0u, 1u, \
	1u, 2u, 3u, 1u, 2u, 2u, 2u, 1u, 4u, 2u, 4u, 1u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 2u, \
	2u, 5u, 4u, 1u, 1u, 3u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 0u, 0u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, \
	0u, 1u, 0u, 3u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	1u, 0u, 3u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, \
	1u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, \
	0u, 3u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 1u, \
	1u, 0u, 0u, 2u, 1u, 2u, 2u, 2u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, \
	3u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 3u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 0u, 2u, 1u, 1u, \
	0u, 0u, 3u, 0u, 1u, 3u, 3u, 0u, 1u, 2u, 3u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 3u, \
	0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 3u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 0u, 2u, 1u, 1u, 0u, \
	0u, 3u, 0u, 1u, 3u, 3u, 0u, 1u, 2u, 3u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 3u, 0u, \
	0u, 2u, 1u, 1u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 4u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 1u, 0u, 3u, 0u, 0u, \
	2u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 0u, 1u, 1u, 0u, 4u, 1u, 1u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 1u, 0u, 3u, 0u, 0u, 2u, \
	1u, 1u, 0u, 0u, 0u, 1u, 2u, 1u, 1u, 0u, 1u, 1u, 0u, 4u, 1u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, 1u, 1u, 1u, 2u, 1u, \
	2u, 2u, 2u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 1u, 0u, 3u, 0u, 0u, 2u, 1u, \
	1u, 0u, 0u, 0u, 1u, 3u, 1u, 1u, 0u, 1u, 1u, 0u, 4u, 1u, 1u, 0u, 0u, 2u, 0u, 2u, 1u, 1u, 1u, 1u, 3u, 0u, 1u, \
	3u, 3u, 0u, 1u, 2u, 3u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 1u, 0u, 3u, 0u, 0u, 2u, 1u, 1u, \
	0u, 0u, 0u, 1u, 3u, 1u, 1u, 0u, 1u, 1u, 0u, 4u, 1u, 1u, 0u, 0u, 2u, 0u, 2u, 1u, 1u, 1u, 1u, 3u, 0u, 1u, 3u, \
	3u, 0u, 1u, 2u, 3u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 2u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 2u, 2u, 1u, \
	0u, 0u, 0u, 4u, 0u, 2u, 1u, 0u, 2u, 1u, 5u, 0u, 0u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 2u, 2u, 1u, 0u, \
	0u, 0u, 5u, 1u, 3u, 0u, 0u, 2u, 1u, 5u, 0u, 0u, 1u, 1u, 1u, 1u, 3u, 1u, 1u, 0u, 0u, 1u, 0u, 1u, 1u, 1u, 0u, \
	1u, 0u, 1u, 3u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 2u, 2u, 1u, 0u, 0u, \
	0u, 6u, 1u, 3u, 0u, 0u, 2u, 1u, 5u, 0u, 0u, 1u, 1u, 1u, 1u, 4u, 1u, 1u, 0u, 0u, 2u, 1u, 2u, 2u, 2u, 1u, 2u, \
	1u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 2u, 2u, 1u, 0u, 0u, 0u, \
	7u, 1u, 3u, 0u, 0u, 2u, 1u, 5u, 0u, 0u, 1u, 1u, 2u, 1u, 5u, 1u, 1u, 0u, 0u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, 2u, \
	4u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 2u, 2u, 1u, 0u, 0u, 0u, 7u, \
	1u, 3u, 0u, 0u, 2u, 1u, 5u, 0u, 0u, 1u, 1u, 2u, 1u, 5u, 1u, 1u, 0u, 0u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, 2u, 4u, \
	1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, 2u, 0u, 0u, 1u, 4u, 0u, \
	0u, 1u, 0u, 3u, 1u, 4u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, \
	2u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, 2u, 0u, 0u, 1u, 5u, 1u, 1u, \
	0u, 0u, 3u, 1u, 4u, 1u, 1u, 0u, 2u, 1u, 1u, 3u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 3u, 1u, \
	0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, 2u, 0u, 0u, 1u, 6u, 1u, 1u, 0u, \
	0u, 3u, 1u, 4u, 1u, 1u, 0u, 2u, 1u, 1u, 4u, 1u, 1u, 0u, 1u, 2u, 1u, 2u, 2u, 2u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, \
	1u, 2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, 2u, 0u, 0u, 1u, 7u, 1u, 1u, 0u, 0u, \
	3u, 1u, 4u, 1u, 1u, 0u, 2u, 2u, 1u, 5u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, \
	2u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, 2u, 0u, 0u, 1u, 7u, 1u, 1u, 0u, 0u, 3u, \
	1u, 4u, 1u, 1u, 0u, 2u, 2u, 1u, 5u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 2u, \
	1u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 8u, 0u, 0u, 1u, 0u, 4u, 1u, \
	4u, 1u, 0u, 1u, 1u, 0u, 1u, 0u, 2u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 1u, 0u, 1u, \
	0u, 0u, 0u, 0u, 0u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 9u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, \
	1u, 0u, 1u, 1u, 1u, 1u, 3u, 3u, 1u, 0u, 1u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 1u, 0u, 1u, 0u, \
	0u, 0u, 0u, 0u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 10u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, \
	0u, 1u, 1u, 1u, 1u, 4u, 3u, 1u, 0u, 1u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, \
	0u, 0u, 0u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 11u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, \
	1u, 1u, 2u, 1u, 5u, 3u, 1u, 0u, 1u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 11u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, \
	1u, 2u, 1u, 5u, 3u, 1u, 0u, 1u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, \
	0u, 1u, 2u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 8u, 0u, 0u, 1u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, \
	0u, 1u, 0u, 2u, 1u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, \
	1u, 2u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 9u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, 1u, \
	1u, 3u, 3u, 1u, 0u, 2u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, \
	2u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 10u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, 1u, 1u, \
	4u, 3u, 1u, 0u, 2u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 0u, 1u, 2u, \
	2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, 2u, 1u, 5u, \
	4u, 1u, 0u, 2u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 2u, 2u, \
	1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 4u, 1u, 4u, 1u, 0u, 1u, 1u, 2u, 1u, 5u, 4u, \
	1u, 0u, 2u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 1u, 1u, \
	4u, 3u, 1u, 3u, 2u, 2u, 1u, 0u, 0u, 0u, 4u, 0u, 2u, 1u, 0u, 5u, 1u, 5u, 0u, 0u, 1u, 1u, 0u, 2u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, \
	3u, 1u, 3u, 2u, 2u, 1u, 0u, 0u, 0u, 5u, 1u, 3u, 0u, 0u, 5u, 1u, 5u, 0u, 0u, 1u, 1u, 1u, 2u, 3u, 1u, 1u, 0u, \
	0u, 1u, 0u, 1u, 1u, 1u, 0u, 1u, 0u, 1u, 3u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, 3u, \
	1u, 3u, 2u, 2u, 1u, 0u, 0u, 0u, 6u, 1u, 3u, 0u, 0u, 5u, 1u, 5u, 0u, 0u, 1u, 1u, 1u, 2u, 4u, 1u, 1u, 0u, 0u, \
	2u, 1u, 2u, 2u, 2u, 1u, 2u, 1u, 2u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, \
	3u, 2u, 2u, 1u, 0u, 0u, 0u, 7u, 1u, 3u, 0u, 0u, 5u, 1u, 5u, 0u, 0u, 1u, 1u, 2u, 2u, 5u, 1u, 1u, 0u, 0u, 3u, \
	0u, 1u, 4u, 3u, 0u, 1u, 2u, 4u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, \
	2u, 2u, 1u, 0u, 0u, 0u, 7u, 1u, 3u, 0u, 0u, 5u, 1u, 5u, 0u, 0u, 1u, 1u, 2u, 2u, 5u, 1u, 1u, 0u, 0u, 3u, 0u, \
	1u, 4u, 3u, 0u, 1u, 2u, 4u, 1u, 1u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, \
	2u, 2u, 0u, 0u, 1u, 4u, 0u, 0u, 1u, 0u, 6u, 1u, 4u, 1u, 0u, 1u, 1u, 0u, 2u, 0u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, \
	0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, \
	2u, 0u, 0u, 1u, 5u, 1u, 1u, 0u, 0u, 6u, 1u, 4u, 1u, 1u, 0u, 2u, 1u, 2u, 3u, 1u, 1u, 0u, 1u, 1u, 0u, 1u, 1u, \
	1u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, 2u, \
	0u, 0u, 1u, 6u, 1u, 1u, 0u, 0u, 6u, 1u, 4u, 1u, 1u, 0u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 2u, 1u, 2u, 2u, 2u, \
	1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, 2u, 0u, \
	0u, 1u, 7u, 1u, 1u, 0u, 0u, 6u, 1u, 4u, 1u, 1u, 0u, 2u, 2u, 2u, 5u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, 4u, 3u, 0u, \
	1u, 2u, 4u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 1u, 1u, 4u, 3u, 1u, 3u, 3u, 2u, 2u, 0u, 0u, \
	1u, 7u, 1u, 1u, 0u, 0u, 6u, 1u, 4u, 1u, 1u, 0u, 2u, 2u, 2u, 5u, 1u, 1u, 0u, 1u, 3u, 0u, 1u, 4u, 3u, 0u, 1u, \
	2u, 4u, 1u, 1u, 0u, 1u, 2u, 1u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, \
	8u, 0u, 0u, 1u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 0u, 2u, 0u, 2u, 1u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, \
	0u, 2u, 2u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 9u, \
	1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 1u, 2u, 3u, 3u, 1u, 0u, 1u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, \
	3u, 1u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 10u, 1u, \
	1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 1u, 2u, 4u, 3u, 1u, 0u, 1u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, \
	1u, 0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 11u, 1u, 1u, \
	0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 2u, 2u, 5u, 3u, 1u, 0u, 1u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, \
	0u, 1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 1u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 0u, 2u, 11u, 1u, 1u, 0u, \
	0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 2u, 2u, 5u, 3u, 1u, 0u, 1u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, \
	1u, 0u, 1u, 1u, 0u, 0u, 0u, 1u, 1u, 2u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 8u, 0u, 0u, 1u, 0u, \
	7u, 2u, 4u, 1u, 0u, 1u, 1u, 0u, 2u, 0u, 2u, 1u, 0u, 2u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 1u, \
	0u, 1u, 1u, 0u, 0u, 1u, 1u, 1u, 2u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 9u, 1u, 1u, 0u, 0u, 7u, \
	2u, 4u, 1u, 0u, 1u, 1u, 1u, 2u, 3u, 3u, 1u, 0u, 2u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 1u, 0u, \
	1u, 1u, 0u, 0u, 1u, 1u, 1u, 2u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 10u, 1u, 1u, 0u, 0u, 7u, 2u, \
	4u, 1u, 0u, 1u, 1u, 1u, 2u, 4u, 3u, 1u, 0u, 2u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 1u, 0u, 1u, \
	1u, 0u, 0u, 1u, 1u, 1u, 2u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, \
	1u, 0u, 1u, 1u, 2u, 2u, 5u, 4u, 1u, 0u, 2u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, \
	1u, 0u, 1u, 1u, 1u, 2u, 2u, 1u, 4u, 3u, 1u, 3u, 4u, 2u, 3u, 0u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, \
	0u, 1u, 1u, 2u, 2u, 5u, 4u, 1u, 0u, 2u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 1u, 0u, 1u, 1u, 1u, \
	0u, 1u, 1u, 1u, 2u, 2u, 1u, 4u, 3u, 2u, 3u, 4u, 2u, 4u, 1u, 1u, 2u, 8u, 0u, 0u, 1u, 0u, 7u, 2u, 4u, 1u, 0u, \
	1u, 1u, 0u, 2u, 0u, 2u, 1u, 1u, 3u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 2u, 2u, 0u, 0u, 0u, 2u, 1u, 0u, 1u, \
	0u, 1u, 1u, 2u, 2u, 1u, 4u, 3u, 2u, 3u, 4u, 2u, 4u, 1u, 1u, 2u, 9u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, \
	1u, 1u, 2u, 3u, 3u, 1u, 1u, 3u, 1u, 0u, 3u, 5u, 4u, 0u, 1u, 0u, 1u, 3u, 1u, 0u, 0u, 0u, 2u, 1u, 0u, 1u, 0u, \
	1u, 1u, 2u, 2u, 1u, 4u, 3u, 2u, 3u, 4u, 2u, 4u, 1u, 1u, 2u, 10u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, \
	1u, 2u, 4u, 3u, 1u, 1u, 3u, 2u, 1u, 4u, 6u, 5u, 1u, 2u, 1u, 2u, 1u, 1u, 0u, 0u, 0u, 2u, 1u, 0u, 1u, 0u, 1u, \
	1u, 2u, 2u, 1u, 4u, 3u, 2u, 3u, 4u, 2u, 4u, 1u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 2u, \
	2u, 5u, 4u, 1u, 1u, 3u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 0u, 0u, 2u, 1u, 1u, 1u, 0u, 1u, 1u, \
	2u, 2u, 1u, 4u, 3u, 2u, 3u, 4u, 2u, 4u, 1u, 1u, 2u, 11u, 1u, 1u, 0u, 0u, 7u, 2u, 4u, 1u, 0u, 1u, 1u, 2u, 2u, \
	5u, 4u, 1u, 1u, 3u, 3u, 0u, 3u, 7u, 6u, 2u, 1u, 2u, 4u, 1u, 1u, 0u, 0u, 0u, 2u, 1u, 1u, 1u, 0u, 1u, \
}

/* AGC table entries of the channel set (V:e00d1a5e), written whole to MT7996_PHY_CSD_AGC_TBL of
 * chains 0..3 (band 0) / 0..4 (bands 1, 2), entry by entry, chain by chain, 1 us after each write.
 * Column = MT7996_CHSET_AGC_COL[BW class] (u8 table 0x022182e4). 2.4 GHz: u32 [50][2] at 0x022197d4;
 * 5 / 6 GHz: u32 [48][5] at 0x02219964. MediaTek WM .data (VMA). [static] */
#define MT7996_CHSET_AGC_COL { \
	0u, 1u, 2u, 3u, 4u, 2u, 0u, 0u, \
}
#define MT7996_CHSET_AGC_COLS_2G4 2u
#define MT7996_CHSET_AGC_COLS_5G6G 5u
#define MT7996_CHSET_AGC_2G4(X) \
	X(0x800004e3u, 0x80000513u) \
	X(0x800104e4u, 0x80010514u) \
	X(0x800204e5u, 0x80020515u) \
	X(0x800304e6u, 0x80030516u) \
	X(0x800404e8u, 0x80040518u) \
	X(0x800504ecu, 0x8005051cu) \
	X(0x800604f0u, 0x80060520u) \
	X(0x800704fau, 0x8007052au) \
	X(0x80080503u, 0x80080533u) \
	X(0x80090510u, 0x80090540u) \
	X(0x800a04e3u, 0x800a0513u) \
	X(0x800b04e4u, 0x800b0514u) \
	X(0x800c04e5u, 0x800c0515u) \
	X(0x800d04e6u, 0x800d0516u) \
	X(0x800e04e8u, 0x800e0518u) \
	X(0x800f04ecu, 0x800f051cu) \
	X(0x801004f0u, 0x80100520u) \
	X(0x801104fau, 0x8011052au) \
	X(0x80120503u, 0x80120533u) \
	X(0x80130510u, 0x80130540u) \
	X(0x801404e3u, 0x80140513u) \
	X(0x801504e4u, 0x80150514u) \
	X(0x801604e5u, 0x80160515u) \
	X(0x801704e7u, 0x80170517u) \
	X(0x801804eau, 0x8018051au) \
	X(0x801904edu, 0x8019051du) \
	X(0x801a04f3u, 0x801a0523u) \
	X(0x801b04fdu, 0x801b052du) \
	X(0x801c0508u, 0x801c0538u) \
	X(0x801d0517u, 0x801d0547u) \
	X(0x801e04e3u, 0x801e0513u) \
	X(0x801f04e4u, 0x801f0514u) \
	X(0x802004e6u, 0x80200516u) \
	X(0x802104e8u, 0x80210518u) \
	X(0x802204ebu, 0x8022051bu) \
	X(0x802304f0u, 0x80230520u) \
	X(0x802404f7u, 0x80240527u) \
	X(0x80250502u, 0x80250532u) \
	X(0x8026050eu, 0x8026053eu) \
	X(0x8027051eu, 0x8027054eu) \
	X(0x802804e7u, 0x80280517u) \
	X(0x802904eau, 0x8029051au) \
	X(0x802a04eeu, 0x802a051eu) \
	X(0x802b04f5u, 0x802b0525u) \
	X(0x802c04fdu, 0x802c052du) \
	X(0x802d0509u, 0x802d0539u) \
	X(0x802e0519u, 0x802e0549u) \
	X(0x802f052du, 0x802f055du) \
	X(0x80300543u, 0x80300573u) \
	X(0x8031055cu, 0x8031058cu)
#define MT7996_CHSET_AGC_5G6G(X) \
	X(0x800004eeu, 0x8000051eu, 0x8000054eu, 0x8000057eu, 0x800005aeu) \
	X(0x800104efu, 0x8001051fu, 0x8001054fu, 0x8001057fu, 0x800105afu) \
	X(0x800204f0u, 0x80020520u, 0x80020550u, 0x80020580u, 0x800205b0u) \
	X(0x800304f2u, 0x80030522u, 0x80030552u, 0x80030582u, 0x800305b2u) \
	X(0x800404f5u, 0x80040525u, 0x80040555u, 0x80040585u, 0x800405b5u) \
	X(0x800504f9u, 0x80050529u, 0x80050559u, 0x80050589u, 0x800505b9u) \
	X(0x80060500u, 0x80060530u, 0x80060560u, 0x80060590u, 0x800605c0u) \
	X(0x80070508u, 0x80070538u, 0x80070568u, 0x80070598u, 0x800705c8u) \
	X(0x800804eeu, 0x8008051eu, 0x8008054eu, 0x8008057eu, 0x800805aeu) \
	X(0x800904efu, 0x8009051fu, 0x8009054fu, 0x8009057fu, 0x800905afu) \
	X(0x800a04f0u, 0x800a0520u, 0x800a0550u, 0x800a0580u, 0x800a05b0u) \
	X(0x800b04f2u, 0x800b0522u, 0x800b0552u, 0x800b0582u, 0x800b05b2u) \
	X(0x800c04f5u, 0x800c0525u, 0x800c0555u, 0x800c0585u, 0x800c05b5u) \
	X(0x800d04f9u, 0x800d0529u, 0x800d0559u, 0x800d0589u, 0x800d05b9u) \
	X(0x800e0500u, 0x800e0530u, 0x800e0560u, 0x800e0590u, 0x800e05c0u) \
	X(0x800f0508u, 0x800f0538u, 0x800f0568u, 0x800f0598u, 0x800f05c8u) \
	X(0x801004edu, 0x8010051du, 0x8010054du, 0x8010057du, 0x801005adu) \
	X(0x801104eeu, 0x8011051eu, 0x8011054eu, 0x8011057eu, 0x801105aeu) \
	X(0x801204efu, 0x8012051fu, 0x8012054fu, 0x8012057fu, 0x801205afu) \
	X(0x801304f1u, 0x80130521u, 0x80130551u, 0x80130581u, 0x801305b1u) \
	X(0x801404f5u, 0x80140525u, 0x80140555u, 0x80140585u, 0x801405b5u) \
	X(0x801504f9u, 0x80150529u, 0x80150559u, 0x80150589u, 0x801505b9u) \
	X(0x80160500u, 0x80160530u, 0x80160560u, 0x80160590u, 0x801605c0u) \
	X(0x80170509u, 0x80170539u, 0x80170569u, 0x80170599u, 0x801705c9u) \
	X(0x801804ebu, 0x8018051bu, 0x8018054bu, 0x8018057bu, 0x801805abu) \
	X(0x801904ecu, 0x8019051cu, 0x8019054cu, 0x8019057cu, 0x801905acu) \
	X(0x801a04eeu, 0x801a051eu, 0x801a054eu, 0x801a057eu, 0x801a05aeu) \
	X(0x801b04f0u, 0x801b0520u, 0x801b0550u, 0x801b0580u, 0x801b05b0u) \
	X(0x801c04f3u, 0x801c0523u, 0x801c0553u, 0x801c0583u, 0x801c05b3u) \
	X(0x801d04f8u, 0x801d0528u, 0x801d0558u, 0x801d0588u, 0x801d05b8u) \
	X(0x801e04ffu, 0x801e052fu, 0x801e055fu, 0x801e058fu, 0x801e05bfu) \
	X(0x801f0509u, 0x801f0539u, 0x801f0569u, 0x801f0599u, 0x801f05c9u) \
	X(0x802004ecu, 0x8020051cu, 0x8020054cu, 0x8020057cu, 0x802005acu) \
	X(0x802104eeu, 0x8021051eu, 0x8021054eu, 0x8021057eu, 0x802105aeu) \
	X(0x802204f0u, 0x80220520u, 0x80220550u, 0x80220580u, 0x802205b0u) \
	X(0x802304f3u, 0x80230523u, 0x80230553u, 0x80230583u, 0x802305b3u) \
	X(0x802404f8u, 0x80240528u, 0x80240558u, 0x80240588u, 0x802405b8u) \
	X(0x802504ffu, 0x8025052fu, 0x8025055fu, 0x8025058fu, 0x802505bfu) \
	X(0x80260509u, 0x80260539u, 0x80260569u, 0x80260599u, 0x802605c9u) \
	X(0x80270516u, 0x80270546u, 0x80270576u, 0x802705a6u, 0x802705d6u) \
	X(0x802804f0u, 0x80280520u, 0x80280550u, 0x80280580u, 0x802805b0u) \
	X(0x802904f3u, 0x80290523u, 0x80290553u, 0x80290583u, 0x802905b3u) \
	X(0x802a04f8u, 0x802a0528u, 0x802a0558u, 0x802a0588u, 0x802a05b8u) \
	X(0x802b04ffu, 0x802b052fu, 0x802b055fu, 0x802b058fu, 0x802b05bfu) \
	X(0x802c0509u, 0x802c0539u, 0x802c0569u, 0x802c0599u, 0x802c05c9u) \
	X(0x802d0516u, 0x802d0546u, 0x802d0576u, 0x802d05a6u, 0x802d05d6u) \
	X(0x802e0527u, 0x802e0557u, 0x802e0587u, 0x802e05b7u, 0x802e05e7u) \
	X(0x802f053bu, 0x802f056bu, 0x802f059bu, 0x802f05cbu, 0x802f05fbu)

/* Gain-step tables of the A-die gain-step programmer V:e00d1dc4 (WM .rodata). [static]
 * STEP: s8 pairs [8 classes][11 steps] { first, second } at 0xe0151288; THR: s8 pairs [8 classes][6] at 0xe0151338 (only the
 * first byte of each pair is read); LIN: u16 [30] at 0xe015124c (V:e00d1d14); the TOP word and bit
 * position of WF 1..4 at 0x022182d8 (u16) / 0x022182e0 (u8). Class: V:e00d1dc4 from the group. */
#define MT7996_CHSET_GSTEP(X) \
	X({ 11, 0 }, { 15, -2 }, { 19, -6 }, { 23, -10 }, { 27, -14 }, { 31, -18 }, { 35, -22 }, { 39, -26 }, { 43, -30 }, { 43, -30 }, { 43, -30 }) \
	X({ 10, 4 }, { 13, 1 }, { 16, -2 }, { 19, -5 }, { 22, -9 }, { 25, -10 }, { 28, -13 }, { 31, -16 }, { 34, -19 }, { 37, -22 }, { 40, -26 }) \
	X({ 10, 4 }, { 13, 1 }, { 16, -2 }, { 19, -5 }, { 22, -9 }, { 25, -10 }, { 28, -13 }, { 31, -16 }, { 34, -19 }, { 37, -22 }, { 40, -26 }) \
	X({ 10, 4 }, { 13, 1 }, { 16, -2 }, { 19, -5 }, { 22, -9 }, { 25, -10 }, { 28, -13 }, { 31, -16 }, { 34, -19 }, { 37, -22 }, { 40, -26 }) \
	X({ 10, 4 }, { 13, 1 }, { 16, -2 }, { 19, -5 }, { 22, -9 }, { 25, -10 }, { 28, -13 }, { 31, -16 }, { 34, -19 }, { 37, -22 }, { 40, -26 }) \
	X({ 10, 4 }, { 13, 1 }, { 16, -2 }, { 19, -5 }, { 22, -9 }, { 25, -10 }, { 28, -13 }, { 31, -16 }, { 34, -19 }, { 37, -22 }, { 40, -26 }) \
	X({ 10, 4 }, { 13, 1 }, { 16, -2 }, { 19, -5 }, { 22, -9 }, { 25, -10 }, { 28, -13 }, { 31, -16 }, { 34, -19 }, { 37, -22 }, { 40, -26 }) \
	X({ 10, 4 }, { 13, 1 }, { 16, -2 }, { 19, -5 }, { 22, -9 }, { 25, -10 }, { 28, -13 }, { 31, -16 }, { 34, -19 }, { 37, -22 }, { 40, -26 })
#define MT7996_CHSET_GTHR(X) \
	X(11, 19, 27, 35, 43, 43) \
	X(10, 16, 22, 28, 34, 40) \
	X(10, 16, 22, 28, 34, 40) \
	X(10, 16, 22, 28, 34, 40) \
	X(10, 16, 22, 28, 34, 40) \
	X(10, 16, 22, 28, 34, 40) \
	X(10, 16, 22, 28, 34, 40) \
	X(10, 16, 22, 28, 34, 40)
#define MT7996_CHSET_GLIN { \
	0x005u, 0x007u, 0x008u, 0x00au, 0x00du, 0x010u, 0x014u, 0x01au, 0x020u, 0x028u, \
	0x033u, 0x03eu, 0x050u, 0x066u, 0x07cu, 0x0a0u, 0x0ccu, 0x0f8u, 0x147u, 0x19bu, \
	0x1eeu, 0x28bu, 0x332u, 0x3d8u, 0x4f5u, 0x653u, 0x7b1u, 0x9e8u, 0xca3u, 0xf5du, \
}

#endif /* HW_MT7996_PHY_CHAN_H */
