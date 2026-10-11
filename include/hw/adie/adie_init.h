/* SPDX-License-Identifier: MIT */
/*
 * The FEM / antenna-switch pin mux of our own WM's PHY init
 * (firmware/radio/bandinit.c; MediaTek's common PHY init V:e00cea48):
 * MediaTek's FEM tables as named lists and the pin mux's constants. The
 * registers are named in adie.h (TOP 0x490..0x4a6, ADIE_TOP_FEM_*) and
 * mt7976.h (MT7976_TOP_FEM and its pin fields); the values of the per-band
 * TOP init (adie.h "TOP init") and of the PHY-init tail's FEM words
 * (mt7976.h MT7976_TOP_B08 / _664, mt7977.h MT7977_TOP_TTG_668) are at those
 * registers.
 *
 * GENERATED from MediaTek's WM (vendor__mt7996_wm_tm.elf) by the research
 * tree's radio-port/bandinit/gen_headers.py; regenerate rather than edit.
 *
 * ADDRESS SPACE: A-die register indices (adie.h), TOP bank (ADIE_K_TOP).
 * Confidence tags: [measured], [static], [inferred].
 */
#ifndef HW_ADIE_ADIE_INIT_H
#define HW_ADIE_ADIE_INIT_H

#include "../hw_bits.h"
#include "adie.h"
#include "mt7976.h"

/* ========================================================================
 * FEM / antenna-switch pin mux (V:e00cea48 per band, die = band; run by the
 * PHY-init routine before the calibration request, phy_cal.h "Calibration
 * init"). Decoded against MediaTek's cold stock load on all three dies
 * (ddtrace common phase, regenerated record for record). [static][measured]
 *  0. Skipped unless the band has a TX or RX path (MT7996_PHY_EE_CONF_*) and
 *     EEPROM 0x196 + band (phy.h MT7996_PHY_EE_196) has EPA or ELNA set.
 *     Type: 4 on band 0 with CBTOP_PAD_GPIO_SKU_TBTC; else 1 for band select 1
 *     (2.4 GHz), 2 for band select 2..7; else skipped. Pin set:
 *     ADIE_FEM_SET_INIT[sku [1:0]][band].
 *  1. V:e00ce91a: the TOP read-modify-writes of ADIE_FEM_TOP_TBL_744 (set
 *     ADIE_FEM_SET_744) or ADIE_FEM_TOP_TBL (other sets); row 0 (TOP 0x4a0)
 *     takes ADIE_FEM_4A0_TYPE1 / _TYPE4 / _TYPE2 (types 2 and 3).
 *  2. Per pin j = 0..15 of the type's template (ADIE_FEM_TMPL_TYPE*), the
 *     decision tree V:e00cec30..V:e00ced5e. A code's group (ADIE_FEM_CODE_GRP)
 *     names a TX-path, RX-path or other pin, its signal (ADIE_FEM_CODE_SIG)
 *     a WF (V:e00cea26: signal <= 0x13: signal % 5; <= 0x17: 0; else signal
 *     - 0x18). A TX / RX-path pin whose WF is at or beyond the band's TX / RX
 *     path count is unused, unless CBTOP_PAD_GPIO_SKU_TBTC. The pin's field
 *     code is its code, its mode bit 0, except: code 0: off; signal <= 0x17:
 *     mode bit = the group's polarity (MT7996_PHY_EE_19C_POL_*; none for
 *     group 0), off if unused; higher signals with MT7996_PHY_EE_19C_SHARED:
 *     off if unused; else code 0x9b: field ADIE_FEM_FIELD_9B, off if unused;
 *     any other code: off. Off: on set ADIE_FEM_SET_744 the pin's TOP 0x744
 *     field (MT7976_TOP_FEM_PIN) is MT7976_TOP_FEM_PIN_OFF and its field
 *     code stays; on the other sets the field code is ADIE_FEM_FIELD_OFF.
 *  3. V:e00ce9c6: set ADIE_FEM_SET_744: TOP 0x744 (mt7976.h MT7976_TOP_FEM)
 *     = the off pins' 0x744 fields, whole word, no read; set
 *     ADIE_FEM_SET_49D: TOP 0x49d |= ADIE_TOP_FEM_49D_SET with
 *     MT7996_PHY_EE_19C_SHARED, else &= ~ADIE_TOP_FEM_49D_CLR.
 *  4. V:e00ce81e, per pin: TOP 0x4a4 ADIE_TOP_FEM_4A4_PINMODE(j) = the mode
 *     bit, then the pin's field (ADIE_FEM_PIN_FIELDS) = the field code [4:0];
 *     one read-modify-write each.
 *  5. V:e00ce7b6: the set's TOP words read back (ADIE_FEM_READBACK_744 /
 *     ADIE_FEM_READBACK), values unused.
 * Stock: die 0 type 1, set 2 (TOP 0x744 = 0xaa800800); dies 1 and 2 type
 * 2, set 3. TOP 0x490..0x4a6 have no other writer in the stock load except
 * the gain-step flags of TOP 0x4a4..0x4a6 (adie.h ADIE_TOP_FEM_4A4_GSTEP,
 * phy_cal.h "Group loop"; per channel). Register names ours [guess]; roles:
 * the pins drive the external FEM / antenna switches [inferred].
 * ======================================================================== */
#define ADIE_FEM_PINS                     16u
#define ADIE_FEM_SET_744                  2u
#define ADIE_FEM_SET_49D                  3u
#define ADIE_FEM_TYPE1                    1u
#define ADIE_FEM_TYPE2                    2u
#define ADIE_FEM_TYPE4                    4u
#define ADIE_FEM_4A0_TYPE1                0x00001f1fu	/* e00ce95c.. */
#define ADIE_FEM_4A0_TYPE4                0x001c0303u
#define ADIE_FEM_4A0_TYPE2                0x001f001fu
#define ADIE_FEM_CODE_GRP                 HW_GENMASK(6, 5)
#define   ADIE_FEM_GRP_TX                 1u	/* TX-path pin, polarity MT7996_PHY_EE_19C_POL_TX */
#define   ADIE_FEM_GRP_RX                 2u	/* RX-path pin, _POL_RX (_POL_RX_WF4 for WF 4) */
#define   ADIE_FEM_GRP_OTHER              3u	/* _POL_OTHER */
#define ADIE_FEM_CODE_SIG                 HW_GENMASK(4, 0)
#define   ADIE_FEM_SIG_WF_LAST            0x13u	/* signals 0..0x13: WF = signal % 5 */
#define   ADIE_FEM_SIG_KEEP_LAST          0x17u	/* 0x14..0x17: WF 0; up to here the code is kept */
#define   ADIE_FEM_SIG_IDX_FIRST          0x18u	/* above: WF = signal - 0x18 */
#define ADIE_FEM_CODE_9B                  0x9bu
#define ADIE_FEM_FIELD_9B                 0xf7u
#define ADIE_FEM_FIELD_OFF                0x0fu
/* Pin set per (CBTOP_PAD_GPIO_SKU_COMB, band) (WM 0xe01eae28, u32 [4][3];
 * V:e00ce74e keeps the low byte): ADIE_FEM_SET_744 / ADIE_FEM_SET_49D / 1 /
 * 0xff (no 0x744 or 0x49d word). [static] */
#define ADIE_FEM_SET_INIT \
	{ { 0x02u, 0x03u, 0x03u }, \
	  { 0x01u, 0x03u, 0x03u }, \
	  { 0x02u, 0xffu, 0x03u }, \
	  { 0x01u, 0xffu, 0x03u } }
/* V:e00ce91a: TOP read-modify-writes {register, mask, value} for any other
 * set (WM 0xe01eaf0c, 7 rows); row 0's value is replaced by
 * ADIE_FEM_4A0_TYPE*. [static] */
#define ADIE_FEM_TOP_TBL(X) \
	X(ADIE_TOP_FEM_4A0, 0x001f1f1fu, 0x00000000u) \
	X(ADIE_TOP_FEM_4A4, 0x0000ffffu, 0x0000fff8u) \
	X(ADIE_TOP_FEM_4A5, 0xffffffffu, 0xfff8fff8u) \
	X(ADIE_TOP_FEM_4A6, 0xffffffffu, 0xfff8fff8u) \
	X(ADIE_TOP_FEM_490, 0xffffffffu, 0x1f000f08u) \
	X(ADIE_TOP_FEM_49D, 0x0000007du, 0x00000000u) \
	X(ADIE_TOP_FEM_49E, 0xffffffffu, 0x000f0000u)
/* V:e00ce91a: TOP read-modify-writes {register, mask, value} for set
 * ADIE_FEM_SET_744 (WM 0xe01eaf60, 3 rows); row 0's value is replaced by
 * ADIE_FEM_4A0_TYPE*. [static] */
#define ADIE_FEM_TOP_TBL_744(X) \
	X(ADIE_TOP_FEM_4A0, 0x001f1f1fu, 0x00000000u) \
	X(ADIE_TOP_FEM_4A4, 0x0000ffffu, 0x0000fffcu) \
	X(ADIE_TOP_FEM_490, 0xffffffffu, 0x0f000818u)
#define ADIE_FEM_TMPL_TYPE1	/* WM 0xe01eaf84 */ \
	{ 0x30u, 0x46u, 0x2fu, 0x45u, 0x75u, 0x00u, 0x47u, 0x31u, \
	  0x48u, 0x32u, 0x75u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u }
#define ADIE_FEM_TMPL_TYPE2	/* WM 0xe01eaf94 */ \
	{ 0xaau, 0xc0u, 0x98u, 0xabu, 0xc1u, 0x99u, 0xacu, 0xc2u, \
	  0x9au, 0xadu, 0xc3u, 0x9bu, 0x75u, 0xc4u, 0x00u, 0x00u }
#define ADIE_FEM_TMPL_TYPE4	/* WM 0xe01eafa4 */ \
	{ 0x30u, 0x46u, 0x2fu, 0x45u, 0x75u, 0xf7u, 0xc2u, 0xacu, \
	  0xc3u, 0xadu, 0xc4u, 0xaeu, 0x00u, 0x00u, 0x75u, 0xf7u }
/* Pin j's field: register (WM 0xe01eae7c), mask (0xe01eaebc; shift 0xe01eaefc
 * = its low bit). [static] */
#define ADIE_FEM_PIN_FIELDS(X) \
	X(ADIE_TOP_FEM_PINSEL(0), HW_GENMASK(4, 0)) \
	X(ADIE_TOP_FEM_PINSEL(0), HW_GENMASK(9, 5)) \
	X(ADIE_TOP_FEM_PINSEL(0), HW_GENMASK(14, 10)) \
	X(ADIE_TOP_FEM_PINSEL(0), HW_GENMASK(19, 15)) \
	X(ADIE_TOP_FEM_PINSEL(0), HW_GENMASK(24, 20)) \
	X(ADIE_TOP_FEM_PINSEL(0), HW_GENMASK(29, 25)) \
	X(ADIE_TOP_FEM_PINSEL(1), HW_GENMASK(4, 0)) \
	X(ADIE_TOP_FEM_PINSEL(1), HW_GENMASK(9, 5)) \
	X(ADIE_TOP_FEM_PINSEL(1), HW_GENMASK(14, 10)) \
	X(ADIE_TOP_FEM_PINSEL(1), HW_GENMASK(19, 15)) \
	X(ADIE_TOP_FEM_PINSEL(1), HW_GENMASK(24, 20)) \
	X(ADIE_TOP_FEM_PINSEL(1), HW_GENMASK(29, 25)) \
	X(ADIE_TOP_FEM_PINSEL(2), HW_GENMASK(4, 0)) \
	X(ADIE_TOP_FEM_PINSEL(2), HW_GENMASK(9, 5)) \
	X(ADIE_TOP_FEM_PINSEL(2), HW_GENMASK(14, 10)) \
	X(ADIE_TOP_FEM_PINSEL(2), HW_GENMASK(19, 15))
/* V:e00ce7b6: TOP words read back (values unused) for set ADIE_FEM_SET_744
 * (WM 0xe01eae6c). [static] */
#define ADIE_FEM_READBACK_744(X) \
	X(ADIE_TOP_FEM_490) \
	X(ADIE_TOP_FEM_PINSEL(2)) \
	X(ADIE_TOP_FEM_PINSEL(1)) \
	X(ADIE_TOP_FEM_PINSEL(0)) \
	X(ADIE_TOP_FEM_4A0) \
	X(ADIE_TOP_FEM_4A4) \
	X(MT7976_TOP_FEM)
/* V:e00ce7b6: TOP words read back (values unused) for any other set (WM
 * 0xe01eae58). [static] */
#define ADIE_FEM_READBACK(X) \
	X(ADIE_TOP_FEM_490) \
	X(ADIE_TOP_FEM_PINSEL(2)) \
	X(ADIE_TOP_FEM_PINSEL(1)) \
	X(ADIE_TOP_FEM_PINSEL(0)) \
	X(ADIE_TOP_FEM_49D) \
	X(ADIE_TOP_FEM_49E) \
	X(ADIE_TOP_FEM_4A0) \
	X(ADIE_TOP_FEM_4A4) \
	X(ADIE_TOP_FEM_4A5) \
	X(ADIE_TOP_FEM_4A6)

#endif /* HW_ADIE_ADIE_INIT_H */
