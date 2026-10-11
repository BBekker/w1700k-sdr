/* SPDX-License-Identifier: MIT */
/*
 * RX DC-offset calibration (adie.h "RX DC-offset calibration (DCOC)") as our WM runs it
 * (firmware/radio/dcoc.c): the save list that replaces MediaTek's bit-3 backup, and the result
 * rows. The registers, fields, values and timings are in adie.h (the DCOC section, its poll
 * and EEPROM byte), rfmodes.h, mt7976.h, mt7977.h, and for the calibration tail's D-die words
 * conn.h (CONN_PWR_800_GATE_*) and phy.h.
 *
 * ADDRESS SPACE: 12-bit A-die register indices (adie.h), chain bank k = WF unless noted.
 *
 * Source: MediaTek's WM vendor__mt7996_wm_tm.elf (V:), mask bits 8 / 9 (phy_cal.h
 * MT7996_PHY_CALMASK_RX_DCOC / _RX_DCOC2), bodies MT7977 V:e004bde0 (fine) / V:e004c632
 * (coarse per step), MT7976 V:e005f2c2 / V:e005f7b4, their tail (phy_cal.h "Tail of every
 * DCOC body") and the poll V:e00444e0, regenerated record for record against MediaTek's cold
 * stock load (384 calls). [static][measured]
 *
 * Confidence tags: [measured], [static], [inferred].
 */
#ifndef HW_ADIE_DCOC_H
#define HW_ADIE_DCOC_H

#include "../hw_bits.h"
#include "adie.h"
#include "mt7976.h"
#include "mt7977.h"
#include "rfmodes.h"
#include "sx.h"

/* ------------------------------------------------------------------------
 * Save list. MediaTek backs up 431 (MT7977) / 219 (MT7976) A-die registers
 * and 430 D-die words before each pass (mask bit 3, phy_cal.h
 * MT7996_PHY_CALMASK_TBL_RESTORE) and writes all of them back at the end of
 * every calibration body. Our firmware saves and restores only what the two
 * DCOC bodies and their tail change, other than the result rows: the lists
 * below, in MediaTek's restore order (per-chain list on k = WF, all-chain list
 * on k 0..4, TOP list; each register once), and the D-die word
 * MT7996_PHY_WFCOMM_D4D0. The 0x171 strobe (ADIE_STROBE) is pulsed and in no
 * list. Role: ADIE_DCOC_W the calibrated chain (k = WF), ADIE_DCOC_S the
 * die's SX bank (MT7977_K_SX / MT7976_K_SX), ADIE_DCOC_T TOP.
 * HAZARD (adie.h): a write of 0 to MT7977 TOP 0x060 or 0x664 locks the die;
 * neither is in these lists, and every value written back is one read from
 * the same register by the save. [static][measured]
 * ------------------------------------------------------------------------ */
#define ADIE_DCOC_W                     0u
#define ADIE_DCOC_S                     1u
#define ADIE_DCOC_T                     2u

/* BEGIN GENERATED (radio-port/dcoc/gen_header.py from vendor__mt7996_wm_tm.elf; do not edit) */

/* MT7977, WF 0..3 (WF != MT7977_K_SX): 38 registers.
 * MediaTek lists 0xe0070754 (per chain), 0xe0070640 (all chains), 0xe0070604 (TOP). [static][measured] */
#define ADIE_DCOC_SAVE_MT7977(X) \
	X(ADIE_DCOC_W, ADIE_CALTAIL_031) \
	X(ADIE_DCOC_W, ADIE_CALTAIL_032) \
	X(ADIE_DCOC_W, ADIE_RX_BW) \
	X(ADIE_DCOC_W, ADIE_CHAIN_CTL) \
	X(ADIE_DCOC_W, ADIE_DCOC_STEP) \
	X(ADIE_DCOC_W, ADIE_DCOC_COARSE) \
	X(ADIE_DCOC_W, ADIE_DCOC_FINE) \
	X(ADIE_DCOC_W, ADIE_CHAIN_MODE) \
	X(ADIE_DCOC_W, ADIE_DCOC_SETUP) \
	X(ADIE_DCOC_W, ADIE_BW_AUX0) \
	X(ADIE_DCOC_W, ADIE_BW_AUX1) \
	X(ADIE_DCOC_W, ADIE_DCOC_024) \
	X(ADIE_DCOC_W, ADIE_DCOC_927) \
	X(ADIE_DCOC_W, ADIE_DCOC_928) \
	X(ADIE_DCOC_W, ADIE_DCOC_929) \
	X(ADIE_DCOC_W, ADIE_TX_BW) \
	X(ADIE_DCOC_W, ADIE_DCOC_SETUP2) \
	X(ADIE_DCOC_W, ADIE_DCOC_601) \
	X(ADIE_DCOC_W, ADIE_DCOC_602) \
	X(ADIE_DCOC_W, MT7977_RCD_744) \
	X(ADIE_DCOC_W, ADIE_DCOC_11A) \
	X(ADIE_DCOC_W, ADIE_CHAIN_OVR) \
	X(ADIE_DCOC_W, ADIE_CHAIN_EN) \
	X(ADIE_DCOC_W, MT7977_RCD_PRE) \
	X(ADIE_DCOC_W, ADIE_DCOC_11B) \
	X(ADIE_DCOC_S, ADIE_FB_SEL) \
	X(ADIE_DCOC_S, ADIE_CHAIN_MODE) \
	X(ADIE_DCOC_S, MT7977_SX_DCOC_745) \
	X(ADIE_DCOC_S, MT7977_SX_DCOC_746) \
	X(ADIE_DCOC_S, MT7977_SX_DCOC_747) \
	X(ADIE_DCOC_S, MT7977_SX_DCOC_14A) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(0u)) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(1u)) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(2u)) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(3u)) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(4u)) \
	X(ADIE_DCOC_T, MT7977_TOP_7E9) \
	X(ADIE_DCOC_T, MT7977_TOP_EPA)

/* MT7977, chain WF = MT7977_K_SX (k 4): the SX bank is the chain: 37 registers.
 * MediaTek lists 0xe0070754 (per chain), 0xe0070640 (all chains), 0xe0070604 (TOP). [static][measured] */
#define ADIE_DCOC_SAVE_MT7977_SX(X) \
	X(ADIE_DCOC_W, ADIE_CALTAIL_031) \
	X(ADIE_DCOC_W, ADIE_CALTAIL_032) \
	X(ADIE_DCOC_W, ADIE_RX_BW) \
	X(ADIE_DCOC_W, ADIE_CHAIN_CTL) \
	X(ADIE_DCOC_W, ADIE_DCOC_STEP) \
	X(ADIE_DCOC_W, ADIE_DCOC_COARSE) \
	X(ADIE_DCOC_W, ADIE_DCOC_FINE) \
	X(ADIE_DCOC_W, ADIE_FB_SEL) \
	X(ADIE_DCOC_W, ADIE_CHAIN_MODE) \
	X(ADIE_DCOC_W, ADIE_DCOC_SETUP) \
	X(ADIE_DCOC_W, ADIE_BW_AUX0) \
	X(ADIE_DCOC_W, ADIE_BW_AUX1) \
	X(ADIE_DCOC_W, ADIE_DCOC_024) \
	X(ADIE_DCOC_W, ADIE_DCOC_927) \
	X(ADIE_DCOC_W, ADIE_DCOC_928) \
	X(ADIE_DCOC_W, ADIE_DCOC_929) \
	X(ADIE_DCOC_W, ADIE_TX_BW) \
	X(ADIE_DCOC_W, ADIE_DCOC_SETUP2) \
	X(ADIE_DCOC_W, ADIE_DCOC_601) \
	X(ADIE_DCOC_W, ADIE_DCOC_602) \
	X(ADIE_DCOC_W, MT7977_RCD_744) \
	X(ADIE_DCOC_W, MT7977_SX_DCOC_745) \
	X(ADIE_DCOC_W, MT7977_SX_DCOC_746) \
	X(ADIE_DCOC_W, MT7977_SX_DCOC_747) \
	X(ADIE_DCOC_W, ADIE_DCOC_11A) \
	X(ADIE_DCOC_W, MT7977_SX_DCOC_14A) \
	X(ADIE_DCOC_W, ADIE_CHAIN_OVR) \
	X(ADIE_DCOC_W, ADIE_CHAIN_EN) \
	X(ADIE_DCOC_W, MT7977_RCD_PRE) \
	X(ADIE_DCOC_W, ADIE_DCOC_11B) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(0u)) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(1u)) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(2u)) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(3u)) \
	X(ADIE_DCOC_T, MT7977_TOP_DCOC_WF(4u)) \
	X(ADIE_DCOC_T, MT7977_TOP_7E9) \
	X(ADIE_DCOC_T, MT7977_TOP_EPA)

/* MT7976, WF 1..3 (WF != MT7976_K_SX): 20 registers.
 * MediaTek lists 0xe00810cc (per chain), 0xe0081040 (all chains), 0xe008102c (TOP). [static][measured] */
#define ADIE_DCOC_SAVE_MT7976(X) \
	X(ADIE_DCOC_W, ADIE_CALTAIL_031) \
	X(ADIE_DCOC_W, ADIE_CALTAIL_032) \
	X(ADIE_DCOC_W, ADIE_BW_AUX0) \
	X(ADIE_DCOC_W, ADIE_BW_AUX1) \
	X(ADIE_DCOC_W, ADIE_CHAIN_CTL) \
	X(ADIE_DCOC_W, ADIE_DCOC_STEP) \
	X(ADIE_DCOC_W, MT7976_DCOC_E90) \
	X(ADIE_DCOC_W, ADIE_DCOC_COARSE) \
	X(ADIE_DCOC_W, MT7976_DCOC_607) \
	X(ADIE_DCOC_W, ADIE_DCOC_FINE) \
	X(ADIE_DCOC_W, ADIE_DCOC_024) \
	X(ADIE_DCOC_W, ADIE_DCOC_927) \
	X(ADIE_DCOC_W, ADIE_DCOC_928) \
	X(ADIE_DCOC_W, ADIE_DCOC_929) \
	X(ADIE_DCOC_W, ADIE_CHAIN_OVR) \
	X(ADIE_DCOC_W, ADIE_CHAIN_EN) \
	X(ADIE_DCOC_S, ADIE_SX_OUT_FORCE) \
	X(ADIE_DCOC_W, ADIE_DCOC_SETUP) \
	X(ADIE_DCOC_T, ADIE_TOP_CHAIN_IF) \
	X(ADIE_DCOC_T, MT7976_TOP_FEM)

/* MT7976, chain WF = MT7976_K_SX (k 0): the SX bank is the chain: 20 registers.
 * MediaTek lists 0xe00810cc (per chain), 0xe0081040 (all chains), 0xe008102c (TOP). [static][measured] */
#define ADIE_DCOC_SAVE_MT7976_SX(X) \
	X(ADIE_DCOC_W, ADIE_CALTAIL_031) \
	X(ADIE_DCOC_W, ADIE_CALTAIL_032) \
	X(ADIE_DCOC_W, ADIE_BW_AUX0) \
	X(ADIE_DCOC_W, ADIE_BW_AUX1) \
	X(ADIE_DCOC_W, ADIE_CHAIN_CTL) \
	X(ADIE_DCOC_W, ADIE_DCOC_STEP) \
	X(ADIE_DCOC_W, MT7976_DCOC_E90) \
	X(ADIE_DCOC_W, ADIE_SX_OUT_FORCE) \
	X(ADIE_DCOC_W, ADIE_DCOC_COARSE) \
	X(ADIE_DCOC_W, MT7976_DCOC_607) \
	X(ADIE_DCOC_W, ADIE_DCOC_FINE) \
	X(ADIE_DCOC_W, ADIE_DCOC_024) \
	X(ADIE_DCOC_W, ADIE_DCOC_927) \
	X(ADIE_DCOC_W, ADIE_DCOC_928) \
	X(ADIE_DCOC_W, ADIE_DCOC_929) \
	X(ADIE_DCOC_W, ADIE_CHAIN_OVR) \
	X(ADIE_DCOC_W, ADIE_CHAIN_EN) \
	X(ADIE_DCOC_W, ADIE_DCOC_SETUP) \
	X(ADIE_DCOC_T, ADIE_TOP_CHAIN_IF) \
	X(ADIE_DCOC_T, MT7976_TOP_FEM)

/* Result rows of the fine (mask bit 8) item, X(row, fields), in MediaTek's row order
 * (WM table 0xe007039c, {u16 index, 4 x (u8 lsb, u8 width)}). [static] */
#define ADIE_DCOC_FINE_ROWS(X) \
	X(ADIE_DCOC_FI2, ADIE_DCOC_F_SLOT5) \
	X(ADIE_DCOC_FQ2, ADIE_DCOC_F_SLOT5) \
	X(ADIE_DCOC_FI1, ADIE_DCOC_F_SLOTS1_4) \
	X(ADIE_DCOC_FQ1, ADIE_DCOC_F_SLOTS1_4) \
	X(ADIE_DCOC_FI0, ADIE_DCOC_F_SLOT0) \
	X(ADIE_DCOC_FQ0, ADIE_DCOC_F_SLOT0)

/* Result rows of the coarse per step (mask bit 9) item, X(row, fields), in MediaTek's row order
 * (WM table 0xe0070360, {u16 index, 4 x (u8 lsb, u8 width)}). [static] */
#define ADIE_DCOC_STEP_ROWS(X) \
	X(ADIE_DCOC_SI2, ADIE_DCOC_S_SLOT5) \
	X(ADIE_DCOC_SQ2, ADIE_DCOC_S_SLOT5) \
	X(ADIE_DCOC_SI1, ADIE_DCOC_S_SLOTS1_4) \
	X(ADIE_DCOC_SQ1, ADIE_DCOC_S_SLOTS1_4) \
	X(ADIE_DCOC_SI0, ADIE_DCOC_S_SLOT0) \
	X(ADIE_DCOC_SQ0, ADIE_DCOC_S_SLOT0)

/* END GENERATED */

#endif /* HW_ADIE_DCOC_H */
