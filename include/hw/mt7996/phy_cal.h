/* SPDX-License-Identifier: MIT */
/*
 * MT7996 PHY calibration: the WM calibration item set and the D-die
 * registers each item writes, the calibration init, the group loops and the
 * channel set outside the calibration calls with their firmware words and
 * the channel set's receive-only subset, the WM's PHY
 * preparation sequences, RX gain /
 * DC / IQ calibration controls, TOAE calibration registers, per-chain and
 * per-band TSSI (TX power detector) registers and the band TX power control
 * (TPC) block; the channel-set TX calibrations' registers: the TSSI
 * measurement path, the TX flatness FIR, the band tone generator, the
 * OTFIQ configuration, and the DPD LUTs and training engine; the TTG-tone
 * RX IQ calibrations' gain-step and tone tables; the TX LO-leakage / TX IQ
 * calibration engine and its TX power meter.
 *
 * ADDRESS SPACE: WF bus as the WM sees it (phy.h has the address formula and
 * the DSP/host views). Macros build on phy.h. A-die (RF) calibration
 * registers are in the adie/ headers; the DFE calibration capture engine the
 * RX-gain and OTFIQ calibrations use is in dfe.h (DPD trains with its own
 * engine, the DPD section below). The TTG-tone RX IQ calibrations' D-die
 * engines and result tables (the RX IQ compensation filter bank, the
 * per-gain RX IQ table) are in phy.h.
 *
 * The calibration MASK below is a WM firmware RAM word, not hardware; it is
 * documented here because it decides which of these register sequences run.
 *
 * Confidence tags: [measured], [static], [inferred].
 */
#ifndef HW_MT7996_PHY_CAL_H
#define HW_MT7996_PHY_CAL_H

#include "../hw_bits.h"
#include "phy.h"

/* ========================================================================
 * WM calibration item mask (firmware RAM word; image default 0x00ffbf39,
 * also stored in the release image after the tag "WFSUBMIT"). Bit set =
 * item runs. Items are table entries T0[i], T1{i}, T2{i} of the WM
 * calibration dispatcher. Purposes inferred from the register writes.
 * [static]
 * ======================================================================== */

#define MT7996_PHY_CALMASK_DEFAULT        0x00ffbf39u
/* Items 0 and 1: A-die RF init (RF_INIT tables, chain and synthesizer-bank
 * defaults, the D-die RF-interface words 0x83Xad400..414; item 1 the
 * per-channel RF tables) and the T0 slots below run from inside it. On a
 * power-cycled radio, MediaTek's firmware receives with only this bit and
 * bit 17 (mask 0x00020001). [static, measured] Item 0's order: adie.h
 * "RF_INIT"; its D-die words: phy.h MT7996_PHY_RF_TBL_KEY,
 * MT7996_PHY_RFIF_D414; where it runs: "Calibration init" below. */
#define   MT7996_PHY_CALMASK_RF_INIT      HW_BIT(0)
/* Item 10: synthesizer programming on every channel set (frequency words,
 * VCO band search, lock); needs bit 0's synthesizer-bank defaults. [static,
 * measured] */
#define   MT7996_PHY_CALMASK_SX           HW_BIT(17)
/* Backup and stored-result reapplication. Gates the backup before every
 * calibration (phyCalBasicConfig item 8: 431 A-die registers on an MT7977,
 * 219 on the MT7976, 430 D-die PHY words and the three LMAC ARB_SCR words,
 * into one shared WM RAM buffer set) and op 8's reapplication of stored
 * results. The restore at the end of every calibration body is NOT gated:
 * without this bit it writes the zero image back, which locks an MT7977 die
 * against further SPI writes (adie.h HAZARD). [static, measured] */
#define   MT7996_PHY_CALMASK_TBL_RESTORE  HW_BIT(3)
/* T0[0] and T0[1] run only from item 0, once per die each time item 0 runs
 * (the band's first channel group, P[0] = 0, 3 or 0x10), after RF_INIT.
 * Item 0 resets both states first, so they rerun with it. No synthesizer
 * tuning (flag 0); table 0 maps to no restore flags. [static][measured] */
/* T0[0]: apply the A-die resistor-cal code from the EEPROM image
 * (MT7996_PHY_EE_RCAL) to A-die 0xe40 (adie.h). */
#define   MT7996_PHY_CALMASK_RCAL         HW_BIT(4)
/* T0[1]: A-die RC (filter) calibration; sequence in adie.h. */
#define   MT7996_PHY_CALMASK_RC           HW_BIT(5)
/*
 * T0[2]: D-die AFE/RXFE calibration, OFF by default. Writes CONN AFE
 * +0x14 [6:3] = 0xf and [18] = 1, RXFE_CTRL [7:4] = 4 and [1:0] = 3 (+band),
 * and pulses per-chain +0xe108 [31:29] (+band). Needs an internal cal
 * source into the ADC [inferred]. The CONN AFE word is band 0's (0x7c043014)
 * whatever the band, and it is not restored. +0xe108: [31:29] = 0, [31]
 * pulsed 1 us, [30] pulsed 10 us, [29] left set; the word is then stored per
 * WF and written back by the WM's op 22; the item-9 restore runs inline, so
 * it also needs bit 3's backup. [static]
 */
#define   MT7996_PHY_CALMASK_DDIE_AFE_RXFE HW_BIT(6)
/* T1{0,2,4}: A-die RX DC offset, fine (adie.h DCOC: the A-die's coarse and
 * fine DC engines, triggered by the firmware); T1[0] at BW class 0 (20 MHz)
 * for every group of the band, T1[2] at class 3 (160 MHz, BW160 groups),
 * T1[4] at class 4 (320 MHz, BW320 groups, band 2). Results 0x8e0..0x8e2 /
 * 0x8e6..0x8e8 (7-bit codes).
 * T1{1,3,5}: the same passes, coarse DC per gain step (step index chain
 * 0x600[11:8], 5..0 MT7977, 4..0 MT7976, one coarse conversion each); results
 * 0x8f0..0x8f2 / 0x8f6..0x8f8 (signed bytes).
 * Per die and pass, per WF: wrapper backup (bit 3), then T1[even], T1[odd]
 * back to back, each tuning the SX to the calibration frequency first (item
 * 10, bit 17). Calibration frequency (kHz, P[0xc]) from u16 MHz tables:
 * T1[0] 0xe006f858[group] (group 0: 2350), T1[1] 0xe006f828[group] (group 0:
 * 2442; groups 1..23 equal T1[0]), T1[2,3] 0xe006f7e0[g'], T1[4,5]
 * 0xe006f7ba[g']. 384 calls in the stock common phase (die 0 8, die 1 96,
 * die 2 280). EEPROM 0x1ae bit 1 / bit 0 clears bit 8 / bit 9. No D-die
 * measurement; the D-die accesses are the tail's restore and pulses. Result
 * handling, the P block and the tail: "Calibration call" below.
 * [static][measured] */
#define   MT7996_PHY_CALMASK_RX_DCOC      HW_BIT(8)
#define   MT7996_PHY_CALMASK_RX_DCOC2     HW_BIT(9)
/*
 * T1{6,10,14}: MT7977 TX loop-DC calibration, one WF per call (body
 * V:e004ffb0 -> V:e004ca36; MT7976 V:e005fb6c; MT7975 V:e005df30, not
 * decoded). T1[6] at the 20 MHz groups (WF 0..3; die 0 group 0, die 1
 * groups 3, 7, 8, 1, die 2 groups 16..23, 9..15; 80 calls), T1[10] at the
 * 160 MHz g' (25, 27 die 1; 32, 33, 34, 28..31 die 2; 36 calls), T1[14] at
 * the 320 MHz g'' (38, 39, 40, 35, 36, 37, die 2; 24 calls); die 2's WF4 is
 * never called; groups 2, 4, 5, 6 and g' 24, 26 are skipped in the common
 * phase (byte 0xe01ec153, as for T1{18..22}). The band-1 channel set (BW160,
 * g' 24) runs T1[10] once per WF. Each call: item
 * 10 retunes the SX (P[1] = WF) to the group's frequency P[0xc] (T1[6]
 * 0xe006f828[g], T1[10] 0xe006f810[g' - 24], T1[14] 0xe006f800[g'' - 35],
 * u16 MHz), P[4] = 0 / 3 / 4; then on the MT7977 with P[6] = 1 (EPA board,
 * the stock case), A-die chain bank k = WF unless noted, in firmware order
 * (adie.h "TX loop-DC calibration"):
 *   0x924 [15] = 1, [13:12] = 0; 0x026 [31] = 0; the WF pair's first bank
 *   0x000 = 0x82000000 (k0 for WF 0/1, k2 for WF 2/3); 0x010 = 0x01fa23c5
 *   (ADIE_CHAIN_EN_TX | bit 17; ADIE_CHAIN_EN_TX for group 0); 0x201 [9:0] =
 *   0x300; 0x4a0 [31] = 1; 0x924 [3:0] = 9; 0x011 [31] = 1; 0x935 [9:0] =
 *   0x270, then [21:12] = 0x270; 0x101 = 0x103 = (group != 0); the FEM
 *   helper V:e00526c2(WF, 0) (TOP 0x4a8 [4:0] = 0, [5] = 1: no WF's
 *   external PA selected); 0x923 [21] = 1, codes 0; 0x4a0 [10:9] = 3; WF 1
 *   only: k0 0x4a0 = 0x85080600; 0x001 [17:16] = 3; 0x024 [31:30] = 2; 0x744
 *   = 0x100, 0x745 = 0x03003000, 0x746 = 0x00011414, 0x747 = 0x20000000
 *   (mt7977.h); 0x922 = 0x09028000 (ADIE_DCOC_FINE_LOOPDC);
 *   8 steps s = 0..7: 0x4a0 [27:24] = s, 0x922 [31] = 1, poll 0x98e [15] (3
 *   matching reads, 1 ms deadline; 8..11 reads here), 0x922 [31] = 0; the
 *   engine leaves the step's 7-bit code in 0x98e [6:0] and stores it in
 *   byte s of the result rows (0x8ef bytes 0..3, 0x8ee bytes 0..3, adie.h
 *   ADIE_LOOPDC_RES*);
 *   then 0x024 [31:30] = 0, 0x744 = 0x745 = 0x747 = 0, 0x746 = 0x1414, 0x026
 *   = 0x80880000; the result check and save (below); the common tail ("Tail
 *   of every DCOC body" below, steps 2..6: everything restored).
 * MT7976 with P[6] = 1 (die 0, stock): no measurement and no TX: 0x924,
 * 0x011 [31] (0x010 stays at rest, 0: the chain is fully off for the call),
 * 0x101 = 0x103 = 0, TOP 0x744 FEM word (mode 0, all four fields 2), 0x923
 * as above, 0x041 [27], 0x042 [24], 0x024 [31:30] = 2, 0x200 [6:3] = 0, k0
 * 0xa0f [1:0] = 3, 0x922 = 0x09028000, k0 0xa0f [1:0] = 0, then the save of
 * the untouched rows (0x40404040 / 0); the measurement is skipped at
 * V:e005ff00. With P[6] = 0 (not run here) the MT7976 runs 8 steps of 0x200
 * [6:3] = s with 0x010 = ADIE_CHAIN_EN_TX_DC and 0xe90 [31:30] = 1, and the
 * MT7977 16 steps of 0x200 [6:3] = i with 0x4a4 [31:28] = byte 0xe007192c[i]
 * (0xf..0x8 twice), 0x010 = ADIE_CHAIN_EN_TX_DC, 0x924 = 0x8809, 0x935 =
 * 0xf0000248 -> 0xf0000270, 0x121 [27], ending 0x000 = 0x84000000. [static]
 * No D-die measurement: the D-die accesses are the log gate (WF_COMM
 * +0xd414, read twice), the failure log (phy.h MT7996_PHY_CALLOG_*), the SX
 * mailbox and the tail.
 * Result handling (restore flag 0x10000000, MT7996_PHY_CAL_RESTORE_LOOPDC):
 *  check  V:e0045a3a(P, 0x10000000, 0) -> V:e00454ee: the 16 fields of
 *         0x8ef, 0x8ee, 0x8ed, 0x8ec (rows MT7996_PHY_LOOPDC_ROWS, 8-bit
 *         field masks as for DCOC) each in [0x0a, 0x60]; a failure sets WM
 *         word 0xe01ec1ac [28] and logs (V:e00453ee, code 2). Stock: die 1
 *         WF 0 fails at groups 3, 7, 8, 1, g' 25, 27 and g' 24 (band 1):
 *         codes 0x08 in 0x8ee bytes 1 / 2 (PA steps 5, 6). The flag is
 *         never cleared (only ORs in the image), so from die 1's first group
 *         on every loop-DC write-back of the load, on every die and channel
 *         set, writes the defaults (the same in three cold loads).
 *  save   V:e0043fce -> V:e0043d76: the 8 rows read once more, 4 x 7-bit
 *         fields packed into the record of (die, group, WF, BW class) ([P +
 *         0x14] + 0x314 for group 0, else +0x438 / +0x1bc / +0x29c for P[4]
 *         < 3 / 3 / 4), valid halfword set.
 *  write-back  V:e004085a maps idx 6 / 10 / 14 with mask bit 10 to the flag;
 *         V:e004243c -> slot 0xe0081240 = V:e0044dde: a3 [28] set, no valid
 *         record, or 0xe01ec1ac [28] set without the DBG_OVR override
 *         (V:e0044c18, phy.h MT7996_PHY_DBG_OVR) -> 0x40404040 into all 8
 *         rows (V:e0044e30); else the record's fields (V:e0044234 ->
 *         V:e0044362).
 *  reapply  op 8 at every channel set (V:e0041618 -> V:e004243c, mask bit
 *         3): the same write-back per WF; stock census 32 / 40 / 32 writes on
 *         bands 0 / 2 / 1, all 0x40404040 (band 2's WF4 has no record and
 *         gets the defaults without the override check).
 * Stock outcome: no measured loop-DC code is ever applied; every MT7977 chain
 * ends each call and each channel set with 0x40404040 in all 8 rows; die 0
 * gets its (unmeasured) record 0x40404040 / 0 at calibration and 0x40404040
 * in all 8 rows at the band-0 channel set. Every other register the body
 * writes is restored (only the item-10 SX retune persists). The only WM
 * consumer of the rows is T2{5} (0x920 [26:20] = [6:0] of 0x8ef for PA steps
 * 0..3 and of 0x8ee for 4..7, field 0 only); no other reader of 0x8ec..0x8ff
 * besides the generic save/restore walkers: not part of the receive path
 * [static: every A-die site of 0x8ec..0x8ff in the WM, ROM and patch].
 * Whether the loop-mode engine runs disturb the RX DC results 0x8e0.. is not
 * established; op 8 rewrites those at every channel set. The failure log
 * lands in RX IQ filter bank slot 8 of die 1 chain 0 (phy.h
 * MT7996_PHY_CALLOG_*). Needs item 0, item 10 / bit 17, bit 3 (backup), P[6];
 * reads no other calibration's result. HAZARD: transmits: an MT7977 chain in
 * TX (blocks on, no baseband stimulus, external PA not selected) for 8
 * engine runs per call; the only output is the TX path's own DC / LO leakage
 * at the calibration LO (5.21..6.905 GHz); antenna emission not measured.
 * The MT7976 does not transmit (0x010 stays 0 under the manual override).
 * [static, measured]
 */
#define   MT7996_PHY_CALMASK_LOOP_DC      HW_BIT(10)
#define MT7996_PHY_LOOPDC_ROWS            0xe00703d8u	/* WM rows {u16 idx, 4 x (lsb, w)}: 0x8ef..0x8ec, 0x8ff..0x8fc */
#define MT7996_PHY_LOOPDC_MIN             0x0au
#define MT7996_PHY_LOOPDC_MAX             0x60u
/*
 * T1{7,11,15}: TX gain-step calibration through the chain's own receiver
 * (the same-chain TX -> RX loopback), one WF per call (P[1]; P[0xa] = 0, so
 * phyCalDo does not force P[1] and item 10 retunes the SX with P[1] = WF),
 * body V:e004d8c6: T1[7] at BW class 0 in the 20 MHz group flow (V:e00d4eb4,
 * list {7, 8} after {0, 1}; groups 0, 3, 7, 8, 1, 16..23, 9..15), T1[11] at
 * class 3 in the 160 MHz flow (g' 25, 27, 32..34, 28..31; the band-1 phase
 * runs g' 24), T1[15] at class 4 in the 320 MHz flow (g'' 38..40, 35..37);
 * 144 calls. P[4] = byte 0xe0068f50[idx - 2] (0 / 3 / 4) only selects the
 * A-die BW code (chain 0x100 = 0x102 = P[4] on every bank) and the result
 * record. Only WF < byte 0xe01ec163 runs (WF 0..3 on every die: band 2's
 * chain 4 is never calibrated [measured]). Calibration frequency P[0xc]:
 * T1[7] 0xe006f828[group] (as T1[1]: 2442, 5210..7095 MHz), T1[11]
 * 0xe006f7e0[g'], T1[15] 0xe006f7ba[g''] (as T1[2] / T1[4]).
 * Sequence (V:e004d8c6):
 *  1. FEM helper V:e00526c2(WF, 0) when P[6] (EPA, EEPROM 0x197/0x198 bit
 *     1): MT7976 TOP 0x744 all four per-WF fields 2 (as the RX DC
 *     calibration; the TX calibrations set the WF's field to 3); MT7977
 *     TOP 0x4a8 [4:0] = 0, [5] = 1 (no WF selected).
 *  2. PHY preparation V:e004908e mode 1; A-die BW code; A-die mode 1
 *     (rfmodes.h "TX gain-step calibration"); chain state
 *     ADIE_CHAIN_EN_LOOP on the WF, standby on the other banks, all banks
 *     manual; selector code 2 (V:e0048a70), DFE path mode 1, RXFE select,
 *     routing V:e0049472 mode 1, PHY helper V:e0048c4e mode 1 (all as the
 *     T2 bodies, mode 1).
 *  3. Measurement setup: MT7996_PHY_DFE_FC04 [11:10] = chain;
 *     MT7996_PHY_TSSI_MEAS_CTL [31:28] = 1 << chain; MT7996_PHY_TSSI_MEAS_CFG
 *     range [27:24] = 0xa; MT7996_PHY_TSSI_MEAS_D824 [31:28] = 7, [27:20] =
 *     0x3f; RFINTF +0x300 |= 6; RFINTF +0x304 [21:16] = 0, [8:0] = 0.
 *  4. Steps s = 0..n-1 (n = 7 MT7976, 6 MT7977; MT7975 8 at group 0): reset
 *     pulse V:e0047e36; A-die TX gain index g_s (MT7976 0x200 [16] = 1,
 *     [6:0]; MT7977 0x240 [30] = 1, 0x241 [27:21]); digital TX gain
 *     MT7996_PHY_DFE_FD34 [31] = 1, [23:16] = d_s; MT7996_PHY_TX_GAIN_MAN
 *     [27] = 1, [26:20] = r_s (tables below: one gain index down and +0x10
 *     digital gain per step). Step 0 only: one measurement read for the
 *     range (MT7996_PHY_TSSI_MEAS_RESULT; > 0x1feff: range 0xc and measure
 *     again), then the range moved by one per factor 4 until the reading
 *     lies in (0x1c00, 0x7000) (limits 2 and 0xe), and D824 [17] = 1 (the
 *     reference); steps >= 1: D824 [17] = 0. Then the measurement (trigger
 *     MT7996_PHY_TSSI_MEAS_CTL [3], wait MT7996_PHY_TSSI_MEAS_STAT [16] ==
 *     0, 100 ms deadline, release [3]): the D-die compares it with the
 *     reference and writes the step's code into field s of
 *     MT7996_PHY_TXSTEP_CODES (phy.h). The firmware reads the word: code
 *     (5-bit signed) + 8 > 16 -> +0x76b8 [0] = 1 and the field cleared (none
 *     in the stock load).
 *  5. RFINTF +0x300 [2:1] = 0; save (slot 0xe00811ec = V:e0045a3a(P, 0x100,
 *     0): every field in [-12, 12], else WM 0xe01ec1ac |= 0x100 and a log;
 *     the three words into the record); the common tail (430-word D-die
 *     list, ARB_SCR, the A-die lists: every register the body changed is
 *     restored; the result words are not in the lists).
 *  6. phyCalDo restore: V:e004243c(P, 0x100, 0) -> V:e0044e98 writes the
 *     three words back from the record (MT7996_PHY_CAL_RESTORE_TXSTEP).
 * Measures the TX gain-step error: not TX IQ, RX IQ or LO leakage. It writes
 * no RX compensation (no RX IQ filter, DC or gain table; the forced RX gain
 * and the chain states are restored); its only result is
 * MT7996_PHY_TXSTEP_CODES, read only inside T1{7,11,15}, T2{4} and T2{5}:
 * not part of the receive path [inferred: TX-side hardware input].
 * Measured codes (stock): MT7976 0 / +1, MT7977 0, -1, -2 (+1 rarely), all
 * well inside the +-8 limit; field 0 always 0; between cold loads +-1 LSB.
 * Needs bit 0 (RF init, item 1's per-group tables), bit 17 (SX inside
 * phyCalDo), bit 3 (backup; without it the restore writes zeros: MT7977
 * lock hazard, ARB_SCR cleared); reads no other calibration's result; runs
 * after DCOC T1{0,1} in each WF pass. Unmeasured fields (MT7976 field 7,
 * MT7977 fields 6..7, +0x76b8 [4:0]) are checked only by the save (+-12).
 * HAZARD: transmits: the WF's TX chain runs at its top gain indices (MT7976
 * 0x3e..0x38, MT7977 0x2e..0x29) for n + 1 short measurements per call (each
 * ends after 14..24 status reads in the stock load); the signal reaches the
 * WF's receiver on-die (ADIE_CHAIN_EN_LOOP bit 30). Whether power leaves the
 * antenna port is not measured (the FEM word is in its RX-calibration state,
 * step 1 [inferred: external PA not selected]). The 100 ms waits end
 * silently on their deadline. [static][measured]
 */
#define   MT7996_PHY_CALMASK_TXSTEP       HW_BIT(11)	/* was _LOOPBACK */
#define MT7996_PHY_TXSTEP_IDX(bw)         ((bw) == 0u ? 7u : (bw) == 3u ? 11u : 15u)	/* BW class 0 / 3 / 4 */
#define MT7996_PHY_TXSTEP_STEPS_MT7976    7u
#define MT7996_PHY_TXSTEP_STEPS_MT7977    6u
#define MT7996_PHY_TXSTEP_RANGE0          0xau	/* TSSI_MEAS_CFG [27:24] start */
#define MT7996_PHY_TXSTEP_RANGE_RETRY     0xcu	/* if the first reading > 0x1feff */
#define MT7996_PHY_TXSTEP_WIN_LO          0x1c00u	/* reading kept in (LO, HI) */
#define MT7996_PHY_TXSTEP_WIN_HI          0x7000u
#define MT7996_PHY_TXSTEP_RANGE_MIN       2u
#define MT7996_PHY_TXSTEP_RANGE_MAX       0xeu
#define MT7996_PHY_TXSTEP_CODE_LIMIT      8	/* body: |code| <= 8 */
#define MT7996_PHY_TXSTEP_SAVE_LIMIT      12	/* save check V:e00454ee */
/* Per-step gains, WM tables (row r = 0xe P[6] + 7 g5 for the MT7976, 0xc P[6]
 * + 6 g5 for the MT7977, g5 = (group != 0); stock P[6] = 1). Stock rows:
 * MT7976 PA 0x3e..0x38, DFE 0xe0, 0xf0, 0, 0x10, .., 0x40; MT7977 PA
 * 0x2e..0x29, DFE 0xc0..0x10, RF 0x3e..0x39. [static] */
#define MT7996_PHY_TXSTEP_T76_PA          0xe0071910u	/* u8: 0x200 [6:0] and +0x250 [26:20] */
#define MT7996_PHY_TXSTEP_T76_DFE         0xe00718f4u	/* u8: DFE_FD34 [23:16] */
#define MT7996_PHY_TXSTEP_T77_PA          0xe0072c00u	/* u16: 0x241 [27:21] */
#define MT7996_PHY_TXSTEP_T77_DFE         0xe0072be8u	/* u8 */
#define MT7996_PHY_TXSTEP_T77_RF          0xe0072bdcu	/* u8 [6 g5 + s]: +0x250 [26:20] */
/*
 * T1{8,12,16}: per-TX-gain-step TX IQ-imbalance (image) calibration,
 * observed through the TX-detector-B loopback (body V:e0049b82, HAL mode
 * 0xc); not a TX detector calibration. One WF per call, WF 0..3 (byte
 * 0xe01ec163); T1[8] BW class 0, T1[12] class 3 (160 MHz g'), T1[16] class 4
 * (320 MHz g''). It runs once per (die, WF, item): V:e00401ae looks the
 * item's state up at a fixed group (T1[8]: group 0 / 3 / 0x10 on die 0 / 1
 * / 2; T1[12]: g' 0x19 / 0x20; T1[16]: g'' 0x26), so the die's first pass
 * calibrates (24 calls) and every later group's call (116) finds state 3
 * and only restores (V:e0040ae6 -> V:e004085a -> V:e0040dcc: no SX retune,
 * no body). Calibration frequency P[0xc]: T1[8] 0xe006f828[g], T1[12]
 * 0xe006f810 + 2g - 0x30, T1[16] 0xe006f800 + 2g - 0x46 (u16 MHz).
 * [static][measured]
 * Setup: FEM word (V:e00526c2 mode 0, P[6]); WF_COMM +0xd870 [19] = 1
 * (phy.h MT7996_PHY_WFCOMM_D870), MT7996_PHY_DFE_FC04 [11:10] = 0,
 * MT7996_PHY_IQE_CFG3 [0] = 0, MT7996_PHY_TSSI_MEAS_CTL [19:16] = 0 and
 * [31:28] = 1; PHY preparation mode 0xc; A-die BW code (MT7976 3, MT7977
 * P[4]) into 0x100 / 0x102 of every bank; WF_COMM +0xd49c [2:0] = 0; TOP
 * 0x060 [26:24] = 0; A-die mode 0xc (rfmodes.h "TX IQ calibration");
 * selector code 3 (4 for class 4); chain state ADIE_CHAIN_EN_TX_DETB on the
 * WF, standby on the others; DFE path / RXFE select / BN +0xfc0c ([13],
 * [12], [5:4] = 2, [11:8] = 3 or 4), BN +0xfc04 [7:0] = 0, [8] = 1;
 * MT7996_PHY_IQE_CFG2 [5:4] = 3, [3:0] = 0; chain routing mode 12; RFINTF
 * +0x300 [2:0] = 7, +0x304 [22:16] = 0, [8:0] = 0, +0x308 [15:0] = 0; PHY
 * helper mode 12; then the loopback receiver (MT7996_PHY_TXIQ_RX_CHAIN):
 * RFINTF +0x254 [7] on chains 0..3 and [6] on the receiver, BN +0x0070
 * [11:8] = 1 << receiver, RFINTF(band, 0) +0x240 [29:28] = receiver; chain
 * +0xd3a0 [16] = 0; the IQ engine words (phy.h, "TX IQ mode 0xc"); chain
 * +0xd010 [24] = 0. [static][measured]
 * Per step j (MT7976 7, MT7977 6; tables in MT7996_PHY_TXIQ_STEP_*): TX
 * gain code c_j into MT7996_PHY_TX_GAIN_MAN ([27] = 1, [26:20]) and the
 * A-die PA gain (MT7976 0x200 [16] = 1, [6:0]; MT7977 0x240 [30] = 1, 0x241
 * [27:21]) with the 0x4a0 [23] pulse; reset pulse V:e0047e36; DFE gain
 * MT7996_PHY_DFE_FD34 [31] = 1, [23:16] = d_j; the WF's 0x600 [16] = 1,
 * [11:8] = 0, [4:1] = l_j and 0x027 [27] = 1, [11:8] = l_j; IQE_CTRL [2] =
 * 1, wait RFINTF(band, 0) +0x3bc [30] == 0 (100 ms, silent deadline; ~1,950
 * reads per run at 2.4 GHz); then
 *  1. V:e00430bc: 13 engine taps (IQE_CFG0 [15:8] = 0xfa..0xff, 0xc0..0xc6,
 *     the [13:0] field of RFINTF(band, 0) +0x3c0, 14-bit signed: taps -6..6
 *     [inferred]); resampled by 3/2 with the 64-tap Q9 filter 0xe006f8c4
 *     (V:e004306a: y[n] = sum_q x[q] h[n - 3q], / 512 toward zero, even n
 *     kept: z[0..31]); z[17..31] -> step j's row group (phy.h
 *     MT7996_PHY_TXIQC);
 *  2. V:e00446e8: IQE_CTRL [3] pulse, [2] = 0; chain +0xd3a0 [16] = 1;
 *     reset pulse V:e0049416; IQE_CTRL [2] = 1, wait +0x3bc [30];
 *     V:e0043b8e scores the image: for s = 1..n (n = 4 at group 0, 8 for
 *     groups 1..0x17, 16 above) the powers I^2 + Q^2 of bins s / 0x40 - s
 *     and 0x80 - s / 0x40 + s (signal / image), r = 10 log10(mean(P_img /
 *     P_sig)) dB (Q12 log10 V:e005b8d2); a signal power <= 0x7ff, an image
 *     power > 0x10000 or a sum >= 0x80000 scores 0x7fffffff; r >= -50 dB
 *     fails: the step's rows = identity, WM byte MT7996_PHY_TXIQ_FAIL_FLAG =
 *     1 and a bit in the failure log below; +0xd3a0 and +0x250 written back;
 *  3. IQE_CTRL [3] pulse, [2] = 0.
 * End: save V:e0045a3a(P, 0x20000) (a failed step: V:e00453ee log, code 4,
 * into the WF's RFINTF +0xa14.., phy.h MT7996_PHY_CALLOG_*; the byte
 * cleared), every one of the 72 rows read and packed; the common exit (D-die
 * list, WFSYS gate, ARB_SCR, pulse V:e0047e36, A-die lists): every A-die
 * register (bit-3 lists), every D-die PHY word (430-word list) and ARB_SCR
 * are restored; not restored are the result rows, the WFSYS gate words and
 * the prelude's SX state. Stock: all 148 steps pass, -64..-54 dB; main tap
 * 4024..4165 (Q12, mostly constant per chain), side taps -22..+23; the save
 * / restore round trip is exact; WF 4 is never calibrated.
 * [static][measured]
 * Not part of the receive path: its only result is the TX IQ rows (phy.h
 * MT7996_PHY_TXIQC), one row group per TX gain step, which T2{8} also writes
 * during its TX gain sweep; without bit 12 the rows stay at identity
 * [inferred: TX side]. Needs bit 0, bit 17 (prelude SX), bit 3 (backup) and
 * phyCalBasicConfig item 16 (V:e004489e, ungated: the identity rows before
 * each WF's pass); reads no other calibration's result; runs after T1[7]
 * (mask bit 11) and before T1[6] in each WF pass, so that item's TX
 * gain-step codes are live. Not exercised: MT7975, WF 4, P[2] = 1, the
 * failure path. HAZARD: transmits: the WF's TX chain is on with PA codes
 * 0x3e..0x38 and the D-die test signal (no tone word at BN +0xfc00); the
 * chain state lacks 0x010 [24:23], which every PA-on state has, and the FEM
 * is in the RX-calibration state of mask bit 11 [inferred: on-die loopback
 * only; radiation not measured]. [static][measured]
 */
#define   MT7996_PHY_CALMASK_TXIQ         HW_BIT(12)	/* was _TX_DET */
#define MT7996_PHY_TXIQ_IDX(cls)          ((cls) == 4u ? 16u : (cls) == 3u ? 12u : 8u)
#define MT7996_PHY_TXIQ_STEPS_MT7976      7u
#define MT7996_PHY_TXIQ_STEPS_MT7977      6u
#define MT7996_PHY_TXIQ_PASS_DB           (-50)	/* r < -50 passes (V:e0044784) */
#define MT7996_PHY_TXIQ_FAIL_FLAG         0xe01ec1b0u	/* WM RAM byte, 1 = a step failed */
/* Per-step bytes, row g5 = (group != 0) [static]:
 * MT7976 TX code 0xe00718bc[7 g5 + j] (0x3e..0x38), DFE gain 0xe007189c[..]
 * (0, 0, 16, 32, 32, 32, 32), LPF 0xe00718ac[..] (1, 2, 2, 2, 3, 3, 3);
 * MT7977 TX code low byte of u16 0xe0072bac[6 g5 + j] (0x3e..0x39), DFE gain
 * 0xe0072b94[..] (0, 16, 16, 16, 16, 32), LPF 0xe0072ba0[..] (0, 0, 1, 2, 3,
 * 3); MT7975 0xe0071438 / 0xe0071418 / 0xe0071428 [8 g5 + j], 8 steps at
 * group 0 (6 above). */
#define MT7996_PHY_TXIQ_STEP_CODE_76      0xe00718bcu	/* WM RAM/ELF tables, not hardware */
#define MT7996_PHY_TXIQ_STEP_DFE_76       0xe007189cu
#define MT7996_PHY_TXIQ_STEP_LPF_76       0xe00718acu
#define MT7996_PHY_TXIQ_STEP_CODE_77      0xe0072bacu	/* u16 */
#define MT7996_PHY_TXIQ_STEP_DFE_77       0xe0072b94u
#define MT7996_PHY_TXIQ_STEP_LPF_77       0xe0072ba0u
#define MT7996_PHY_TXIQ_RESAMPLE_H        0xe006f8c4u	/* 64 x s16, Q9, centre 30 = 512 */
/* Loopback receiver chain of WF w: byte 0x022182f0[w] for w <= 2 (1, 0, 3),
 * 2 for w = 3 (pairs 0-1, 2-3): the paired chain's ADC digitises the WF's
 * TX through detector B. [static][measured] */
#define MT7996_PHY_TXIQ_RX_CHAIN(w)       ((w) == 0u ? 1u : (w) == 1u ? 0u : (w) == 2u ? 3u : 2u)
/* Failure log of V:e00446e8 (never written in stock) [static]: bit (WF +
 * step) of a word chosen by the group class and WF pair (WF <= 1 / >= 2):
 * g'' > 0x22: WF_COMM +0xd428 / +0xd42c; 0x1c..0x22: chain-3 +0xfc08 /
 * +0xfc0c; 0x18..0x1b: chain-2 +0xfc08 / +0xfc0c; 2..0x17: chain-3 +0xfc00
 * / +0xfc04; 1: chain-2 +0xfc00 / +0xfc04; 0: WF_COMM +0xd420 / +0xd424
 * (all + band << 20). These are the chain-2/3 words at the offsets of
 * chain 0's SX bookkeeping (phy.h MT7996_PHY_SX0_FREQ_KHZ / _SX0_PARAM). */
/*
 * T1{9,13,17}: TX LO-leakage (carrier feedthrough) and TX IQ-imbalance
 * calibration per TX gain step, one WF per call (body V:e00539fa, HAL mode
 * 0), with TX detector A as the receiver [inferred] (A-die chain WF 0x010 =
 * ADIE_CHAIN_EN_TX_DETA, rfmodes.h) and a D-die search engine
 * (MT7996_PHY_TXCAL_*, "TX LO / IQ calibration engine" below) that leaves
 * its codes in the WM table programmer's item-9 window (phy.h
 * MT7996_PHY_TXLO_I / _Q, MT7996_PHY_TXLOIQ_A / _B; phy_map.h item 9). Not RX
 * IQ, not RX DC and not the image calibration of T1{8,12,16}. T1[9] BW class
 * 0 (every 20 MHz group of the band, P[0xc] from 0xe006f828[g]), T1[13]
 * class 3 (160 MHz g' 24..34, 0xe006f7e0[g']), T1[17] class 4 (320 MHz g''
 * 35..40, die 2, 0xe006f7ba[g'']); P[4] = 0 / 3 / 4. Stock: 80 + 36 + 24
 * calls in the common phase (die 0: group 0, WF 0..3; die 1: groups 3, 7, 8,
 * 1 / g' 25, 27; die 2: groups 16..23, 9..15 / g' 32, 33, 34, 28..31 / g''
 * 38, 39, 40, 35, 36, 37; WF 4 never) and 4 more T1[13] calls (g' 24, WF
 * 0..3) at the band-1 channel set; every call calibrates (no "done" state as
 * in T1{8,12,16}) and returns 1. Each call first tunes the SX (item 10, P[1]
 * = WF). Decoded against MediaTek's cold stock load: all 144 calls
 * (1,116,796 records) and 3 second samples (22,822 records) regenerated
 * record for record. [static][measured]
 * Setup: the 25 item-9 words of steps 0..19 cleared (+0x600..+0x624,
 * +0x640..+0x650, +0x660..+0x670, +0x680..+0x690); n steps, MT7976 16 (14
 * with P[6] = 0), MT7977 12 (18 with P[6] = 0); the FEM word (V:e00526c2 mode
 * 0, P[6]: MT7976 TOP 0x744 = 0xaa888822, MT7977 TOP 0x4a8 [4:0] = 0, as
 * T1{6,10,14} and T1{7,11,15}); PHY preparation V:e004908e; the A-die BW code
 * (mode 0); A-die mode 0 and the chain state ADIE_CHAIN_EN_TX_DETA on the WF,
 * standby on the others (rfmodes.h "TX LO-leakage and TX IQ calibration");
 * MT7996_PHY_DFE_FC04 [11:10] = chain; MT7996_PHY_TSSI_MEAS_CTL [31:28] = 1
 * << chain ([27:0] kept); the BN table MT7996_PHY_TXCAL_TBL; MT7976 only: the
 * A-die RX DC engine on the WF's own chain in the TX-detector state
 * (V:e005ef86, HAZARD (RX) below), 0x213 / 0x218 saved, 0x218 = 0 (P[6]),
 * 0x280..0x286 read (mt7976.h MT7976_TXTRIM_*), then WF 0x010 =
 * ADIE_CHAIN_EN_TX_DETA_76, 0x041 = 0x08000000, 0x042 = 0x01000000.
 * Per step s, both phases: the TX gain index from the per-step tables below
 * (MT7976 0x36..0x30, 0x26..0x20, 0x29, 0x28; MT7977 0x2e..0x29, 0x3e..0x39)
 * into the A-die PA gain and MT7996_PHY_TX_GAIN_MAN [26:20]; the DFE digital
 * gain MT7996_PHY_DFE_FD34 [23:16] = the table's code + 8 x the offset the
 * last anchor's power search found, clamped to [-0x80, 0x48]
 * (MT7996_PHY_TXLOIQ_DFE_MIN / _MAX); the WF's RX gain / LPF; on the MT7976
 * the 0x213 trim word (mt7976.h); chain +0xd008 [7:6] = 1, then 3 (phy.h
 * MT7996_PHY_CHAIN_D008); the reset pulse V:e0049416 (phy.h) before every
 * engine run and every meter reading.
 * Anchor steps (MT7976 0, 7, 14; MT7977 0, 6, 12: MT7996_PHY_TXLOIQ_ANCHOR_*)
 * run the power search V:e005332e: the PA at step + 3; the TX power meter
 * (MT7996_PHY_TXPM_*, the TSSI measurement path below) at settings 0x20 /
 * 0xe0 (LO phase, meter mode 0) or 0x20 / 0x1e0 (IQ phase, mode 1); the
 * detector range (MT7996_PHY_TSSI_MEAS_RANGE; start 8 in the LO phase, 0xa
 * MT7976 / 8 MT7977 in the IQ phase) +3 after a reading > 0x1feff, -2 after
 * one <= 0xfe (33 tries at most), then moved until the reading lies in
 * [0x1c00, 0x7000); a peak search of the reading over the DFE code in steps
 * of 8 within [-120, 0x48]; three final readings; the PA back to the step.
 * The result is an offset in DFE units of 8 (stock: LO -8..+6, IQ -7..+8;
 * MT7977 IQ step 0 always at the +8 limit), applied to every step until the
 * next anchor; the detector range of the other steps is the anchor's,
 * stepped +1 / -1 by step (MT7976 +1 at 0, 1, 7, 8, 14, 15, -1 at 5, 6, 12,
 * 13; MT7977 +1 at 0, 1, 6, 7, 12, 13, -1 at 5, 11, 17).
 * Phase 1 (LO leakage), every step: chain +0xd3a0 [16] = 0 (MT7977 also
 * [0], [1], [17] = 0); the engine words of run [0]; run [0]
 * (MT7996_PHY_TSSI_MEAS_TXCAL_LO) -> the step's LO I and Q codes (bytes of
 * +0x660 / +0x680). Phase 2 (TX IQ), every step: the same TX gain indices
 * (MT7977: separate DFE, LPF, 0x244 and 0x241 tables), +0xd3a0 [16] = 1;
 * run [1] -> code A (signed 9-bit, halves of +0x600..), run [2] -> code B
 * (signed 7-bit, bytes of +0x640..); which of A and B is gain and which phase
 * is not established. A step that is not an anchor starts its search 12 (LO)
 * / 8 (IQ) codes below the previous step's result (MT7996_PHY_TXCAL_CTRL_MODE
 * = 1); V:e0052998 then re-runs the engine from its default start when the
 * result jumped (LO I / Q outside [prev - 14, prev + 13], IQ A or B outside
 * prev +- 16; never in stock). Phase 2 step 0 re-runs [1] after [2] and
 * copies its B byte (+0x640 [7:0]) into all 32 B bytes. MT7976 with P[6] =
 * 1 then overwrites the IQ codes of steps 7..15 with those of steps 0..6,
 * 5, 6 (V:e0055118..e005528c: the second PA range's measured IQ codes are
 * discarded). End: MT7976 0x218 / 0x213 written back; the save V:e0045a3a(P,
 * 0x200, 0) (restore flag 0x200 in "Restore flags" below); the common exit;
 * the phyCalDo tail writes the record back (MT7996_PHY_CAL_RESTORE_TXLOIQ).
 * Leaves: the WF's item-9 rows (phy.h), BN +0xfc30..+0xfc6c
 * (MT7996_PHY_TXCAL_TBL, not restored), RFINTF +0x3b0 = 0, the prelude's SX
 * state, band 2's WFSYS gate words and one WM RAM record per (band, group,
 * WF, BW class); everything else, every engine and meter word included, is
 * restored by the bit-3 lists (the 430-word D-die list, the A-die lists,
 * ARB_SCR). Reapplied at every channel set (op 8; measured in another cold
 * stock load): phyCalBasicConfig item 16 zeroes all 40 rows of each WF, then
 * op 8 writes the record back: band 0 group 0 (20 rows per chain), band 2
 * g'' 35 (15 per chain; chain 4 40 zeros, no record), band 1 g' 24 (15 per
 * chain, calibrated by the band-1 channel set itself); within +-2 LSB of the
 * calibration results (the spread between two cold loads). With bit 13 clear
 * the rows stay 0. Stock: no re-run, no range-check failure; results in
 * phy.h. [static][measured]
 * Not part of the receive path: no code outside the body, its check, the
 * save, the restore and item 16 touches the rows in any phase, and they are
 * indexed by TX gain step [inferred: TX side, the TX DC (LO) and IQ
 * compensation per TX gain index; which block applies them is not
 * established]. Needs bit 0 (RF init; the MT7976 0x280..0x286 constants),
 * bit 17 (SX inside phyCalDo), bit 3 (backup) and P[6] (EPA, EEPROM 0x197 /
 * 0x198 bit 1); reads no other calibration's result; runs after DCOC
 * T1{0,1} and before T1[18..23] in the group pass. Not exercised: MT7975,
 * P[6] = 0, P[2] != 0, the MT7977 P[6] = 0 limits, the re-run and the
 * save-failure paths. Not established: the engine's algorithm and
 * sub-fields, the role of the BN +0xfc30 table, antenna emission.
 * HAZARD: transmits: the WF's TX chain on (ADIE_CHAIN_EN_TX_DETA; the MT7976
 * then ADIE_CHAIN_EN_TX_DETA_76) with the PA at each step's TX gain index,
 * ~4 ms per step [inferred from the status reads at 1 us: engine run [0]
 * 1.4..2.7k, runs [1] / [2] 0.67..1.3k each, meter readings 14..54 each],
 * 12..16 steps per call; the FEM word as above [inferred: external PA not
 * selected]; antenna emission not measured. The band-1 channel set runs
 * T1[13] for g' 24 itself, so it transmits too. The 100 ms waits end
 * silently on their deadline. Unmasked shifts: a negative DFE code also
 * writes BN +0xfd34 [31:24] (0xfff80000 observed; phy.h MT7996_PHY_DFE_FD34)
 * and the MT7977 0x244 [18:16] table byte is shifted unmasked (mt7977.h).
 * HAZARD (RX): on the MT7976 the body runs the A-die RX DC engine on the
 * WF's own chain in the TX-detector state (0x010 = 0xa0f803c7, V:e005ef86;
 * adie.h DCOC); those result registers are in no restore list. Op 8 rewrites
 * the DCOC results at every channel set, so reception after a channel set
 * should be unaffected [inferred]; whether T1[18..23], later in the same
 * group pass, see changed die-0 DC codes is not established.
 * [static][measured]
 */
#define   MT7996_PHY_CALMASK_TXLOIQ       HW_BIT(13)	/* was _TX */
#define MT7996_PHY_TXLOIQ_IDX(cls)        ((cls) == 4u ? 17u : (cls) == 3u ? 13u : 9u)
#define MT7996_PHY_TXLOIQ_STEPS_MT7976    16u	/* P[6] = 1 (14 without) */
#define MT7996_PHY_TXLOIQ_STEPS_MT7977    12u	/* P[6] = 1 (18 without) */
#define MT7996_PHY_TXLOIQ_ANCHOR_MT7976   0x4081u	/* steps 0, 7, 14 (bit mask) */
#define MT7996_PHY_TXLOIQ_ANCHOR_MT7977   0x1041u	/* steps 0, 6, 12 */
#define MT7996_PHY_TXLOIQ_DFE_MIN         (-0x80)	/* table + 8 x offset, clamped */
#define MT7996_PHY_TXLOIQ_DFE_MAX         0x48
/* Per-step tables (ELF, not hardware) [static]: MT7976 row (g5 + 2 P[6]) * 16
 * + step, the same tables in both phases; MT7977 row g5 * 0x12 + P[6] * 0x24
 * + step, one table per phase (_LO, _IQ); g5 = (group != 0). The 0x244 and
 * 0x241 bytes are the MT7977 TX gain HAL's stack arguments (rfmodes.h). */
#define MT7996_PHY_TXLOIQ_T76_PA          0xe007181cu	/* u16: 0x200 [6:0], RFINTF +0x250 [26:20] */
#define MT7996_PHY_TXLOIQ_T76_DFE         0xe007179cu	/* u8: DFE_FD34 [23:16] before the offset */
#define MT7996_PHY_TXLOIQ_T76_LPF         0xe00717dcu	/* u8: 0x600 [4:1] (9 in phase 2 without P[6]) */
#define MT7996_PHY_TXLOIQ_T76_202         0xe0087184u	/* u8 WM RAM (0 in the image): 0x202 [22:20], [18:16] */
#define MT7996_PHY_TXLOIQ_T77_PA          0xe007290cu	/* u8: 0x241 [27:21], RFINTF +0x250 [26:20] */
#define MT7996_PHY_TXLOIQ_T77_DFE_LO      0xe007224cu
#define MT7996_PHY_TXLOIQ_T77_DFE_IQ      0xe0072324u
#define MT7996_PHY_TXLOIQ_T77_LPF_LO      0xe007275cu	/* u8: 0x741 [19:16] */
#define MT7996_PHY_TXLOIQ_T77_LPF_IQ      0xe0072834u
#define MT7996_PHY_TXLOIQ_T77_244_LO      0xe00729e4u	/* u8: 0x244 [9] = bit 3, [21:19] = [2:0] */
#define MT7996_PHY_TXLOIQ_T77_244_IQ      0xe0072abcu
#define MT7996_PHY_TXLOIQ_T77_244B_LO     0xe0071d3cu	/* u8: 0x244 [18:16] (7) */
#define MT7996_PHY_TXLOIQ_T77_244B_IQ     0xe0071e14u
#define MT7996_PHY_TXLOIQ_T77_241_LO      0xe00723fcu	/* u16: 0x241 [11:1] (0x0fc) */
#define MT7996_PHY_TXLOIQ_T77_241_IQ      0xe00725acu	/* u16 (0x038 steps 0..5, 0x7fc 6..11) */
/* Save of the item-9 rows (flag 0x200; "Restore flags" below): the WM row
 * table, the range-check limits (|v| >= limit fails; group 0, other groups)
 * and the patterns a failing class is overwritten with. [static] */
#define MT7996_PHY_TXLOIQ_ROWS            0xe006ffe4u	/* 40 x {u16 off, 4 x (u8 lsb, u8 w)} */
#define MT7996_PHY_TXLOIQ_LIM_A           0x80	/* group 0; 0x7f otherwise */
#define MT7996_PHY_TXLOIQ_LIM_B           0x3f	/* 0x3e */
#define MT7996_PHY_TXLOIQ_LIM_LO          0x78	/* 0x77 */
#define MT7996_PHY_TXLOIQ_CLAMP_A         0x01ff0001u	/* +0x600..+0x63c */
#define MT7996_PHY_TXLOIQ_CLAMP_B         0x7f017f01u	/* +0x640..+0x65c */
#define MT7996_PHY_TXLOIQ_CLAMP_LO        0xff01ff01u	/* +0x660..+0x69c */
/*
 * T1{18..22}: TTG-tone RX IQ calibration, one item per RX bandwidth (not
 * per WF): T1[18] 20 MHz, [19] 40, [20] 80, [21] 160, [22] 320 MHz (P[4] =
 * byte 0xe0068f50[idx - 2] = 0..4). Each call calibrates every chain of the
 * band at once: under this mask bit V:e004011a sets P[0xa] = 1 for idx
 * 18..22, so of the group's WF0..3/4 calls only WF0 runs; it marks every
 * WF's state 3, and the WF1..4 calls return that 3 without touching
 * hardware (phyCalDo return value below). T1[18..20] run in the 20 MHz
 * group flow (V:e00d4eb4, list 18, 23, 19, 20; T1[20] not on 2.4 GHz),
 * T1[21] in the 160 MHz flow (V:e00d4dfa, g' 25, 27, 28..34), T1[22] in the
 * 320 MHz flow (V:e00d4d8c, g' 35..40, die 2 only); with byte 0xe01ec153
 * set, groups 2, 4, 5, 6 and g' 24, 26 are skipped. The band1 phase runs
 * T1[21] once more for g' 24 (5250 MHz). Each call first tunes the SX
 * (item 10) to the calibration frequency P[0xc] from the DCOC items' u16
 * MHz tables: 0xe006f828[group] (as T1[1]; group 0: 2442, groups 1..23:
 * 5210..7095), T1[21] 0xe006f7e0[g'], T1[22] 0xe006f7ba[g'].
 * [static][measured]
 * Per RX gain step j (6 steps; 10 at 320 MHz) the A-die gain is forced
 * (chain 0x600, adie.h ADIE_DCOC_STEP; tables in the TTG RX section below),
 * the TTG PLL is tuned next to the LO and fed into every chain's RX aux
 * input (rfmodes.h ADIE_CHAIN_EN_RX_AUX), a D-die DC engine runs with the
 * tone at LO + 5 MHz, then the D-die IQ engine runs with the tone at LO -
 * SUB (MT7996_PHY_TTG_RX_SUB_KHZ) and leaves a compensation filter pair in
 * each chain's live result slot (phy.h MT7996_PHY_RXIQC_*). Above 2.4 GHz
 * at 40..320 MHz (P[0] != 0, P[4] 1..4) the per-bin refinement V:e004a11e
 * follows (phy.h MT7996_PHY_IQBIN_*): it measures the IQ engine's per-bin
 * response, loads it as a table and has the engine fit the filter pair
 * again; that fit replaces the live slot. The firmware then packs the live
 * slot into its record for (group, bandwidth). D-die writes also:
 * WF_RXFE_01 RXFDIQ_TAP_NUM = 2 at 0x8308f114 and twin 0x8309f114 (+band),
 * RXFE_CTRL [19] = 0, CSD RXGAIN [2:0] = 7 on every chain; the engine
 * words are in phy.h. Needs bit 0, bit 17 (the SX inside phyCalDo) and
 * bit 3 (backup); no other calibration's result is an input. [static]
 * [measured]
 * Storage and use: the WM keeps one record per group, with one sub-record
 * per bandwidth: group 0 (2.4 GHz) 20 MHz at +0x5c, 40 MHz at +0x158;
 * groups 1..23 20 MHz at +0, 40 at +0xfc, 80 at +0x1f8; the 160 MHz set in
 * g' 24..34's record at +0, the 320 MHz set (12 slots) in g' 35..41's
 * record (V:e0044182 store, V:e004500a restore, the same selection by P[0]
 * and P[4]). Restore (V:e004243c, restore flag 0x40 -> V:e004500a, below)
 * writes slots 0..5 of the RX IQ filter bank (phy.h MT7996_PHY_RXIQC_*),
 * 0..11 for P[4] = 4; V:e004243c itself sets P[4] = 3 for P[0] 24..34 and
 * P[4] = 4 for P[0] > 34. It runs (a) at the end of every calibration
 * call, so after the common phase each die's bank holds the last item that
 * ran on it, and (b) at every channel set (V:e00d708a -> V:e00d7428 ->
 * V:e00d62d6 op 8 -> V:e004243c per WF with the channel's P and the flag
 * set of op 7), which rewrites the bank with the stored set of the
 * channel's group and bandwidth. [static]
 * Measured (the band phases of another cold stock load, ungated): the
 * 2.4 GHz channel set writes 4 chains x 6 slots with half B = 0
 * everywhere, i.e. the 20 MHz set (|delta| to the oracle load's T1[18]
 * group-0 result 156 summed over 720 taps, to T1[19]'s 1881), although
 * T1[19] ran last; the 6 GHz channel set writes 5 chains x 12 slots, the
 * 320 MHz set. The stock census agrees: V:e004243c runs outside the
 * calibration calls 8 / 5 / 8 times in the band0 / band2 / band1 phases,
 * and its writer V:e00442ba reaches slot 5 / 11 / 5. [measured]
 * T1[22] measures 10 steps and saves 10; the restore reads 12 slots of
 * field widths from the same per-chain bit stream, so slots 10 and 11
 * decode bits the call never wrote: zero after a cold boot. Observed:
 * every slot-10/11 write of every T1[22] call (200 per call) is 0, and all
 * 20 words of slots 10 and 11 are 0 on all 5 chains at the 320 MHz channel
 * set: at 320 MHz those two slots hold an all-zero filter (main tap 0).
 * Whether the RX gain path ever selects slots 10 and 11 is not
 * established. [measured]
 */
#define   MT7996_PHY_CALMASK_TTG_RX       HW_BIT(15)
#define MT7996_PHY_TTG_RX_BW_IDX(bw)      (18u + (bw))	/* bw code 0..4 = 20..320 MHz */
#define MT7996_PHY_TTG_RX_STEPS(bw)       ((bw) == 4u ? 10u : 6u)
#define MT7996_PHY_TTG_RX_SLOTS(bw)       ((bw) == 4u ? 12u : 6u)	/* restored per chain */
/*
 * T1[23]: per-gain frequency-independent RX IQ calibration, 20 MHz only,
 * same groups and the same WF0-only pattern (P[0xa] = 1) as T1[18]. Its
 * body V:e004b49c is T1{18..22}'s body V:e004a876 run with P[9] = 0xff
 * (saved and put back around the call). The "CONN AFE +0x14 |= 0x4000"
 * once attributed to this item belongs to the next function, V:e004b4b4
 * (mode 0xf): T1[23] writes no CONN AFE word. With P[9] > 0xfc the body
 * runs in mode 0x16: 11 gain steps on the MT7976, 12 on the MT7977 (tables
 * in the TTG RX section below), A-die mode 4 instead of 5, the TTG at LO +
 * 5 MHz in both phases (no offset word, A-die 0x944 [31] = 0; LO + 10 MHz
 * when (sp8f, bw) is (2, 2), (1, 3) or (0, 4), not used), no V:e004a11e,
 * and the IQ engine in a scalar mode (phy.h): per chain RFINTF_CALSEL
 * [1:0] = 2 (P[9] 0xfe -> 1, 0xfd -> 0), DCE_CTRL [31] = 1, [29] = 0,
 * IQE_CFG3 [11:8] = 9, [3:1] = 1, [12] = 0, [13] = 0, BN +0xf100 [0] = 0,
 * WF_RXFE_01 [19] = 0; per step and chain RFINTF +0x258 (phy.h
 * MT7996_PHY_RFINTF_7258), WF_COMM +0xd890 [3:0], +0xd898 [1:0], +0xd8a0
 * [4:0], +0xd8c0 [3:0], +0xd8c8 [1:0] = 0 and +0xd8d0 [4:0] = step table
 * (V:e00503aa). The engine leaves a gain and a phase in RFINTF +0x820
 * [9:0] / +0x858 [7:0]; V:e00427ae moves step s's pair into entry s of the
 * per-gain table (phy.h MT7996_PHY_FIQ_*) and puts step 0's pair back into
 * the live fields; after the last step V:e0045a3a (flag 0x20: gain in
 * [0x180, 0x280], phase within +-0x60) saves the 15 words and the
 * end-of-call restore (flag 0x20, V:e0044f48) writes them back. Measured
 * tables: 2.4 GHz entries 0..10 gain 512..518, phase -4..-1; 5/6 GHz
 * entries 0..11 gain 501..518, phase -2..+15; the rest default.
 * [static][measured]
 * Use: only a 20 MHz channel has a stored table (group 0: record +0x28 at
 * P[4] = 0; groups 1..23: +0x36c at P[4] = 0); for any other P[4]
 * V:e0044f48 writes the defaults instead (A/B 0x02000200, C 0). Measured
 * (the other cold load's band phases): the 2.4 GHz channel set restores
 * the stored table (gains 512..519, phases -4..-1, entries 11.. default),
 * the 320 MHz channel set writes the defaults on all 5 chains.
 * [static][measured]
 */
#define   MT7996_PHY_CALMASK_TTG_RX_FIQ   HW_BIT(16)	/* was _TTG_RX_AFE */
#define MT7996_PHY_TTG_RX_FIQ_IDX         23u
#define MT7996_PHY_TTG_RX_FIQ_P9          0xffu
#define MT7996_PHY_TTG_RX_FIQ_STEPS_MT7976 11u
#define MT7996_PHY_TTG_RX_FIQ_STEPS_MT7977 12u
/* T2{4..8} (bits 18, 19, 21, 22, 23) run on every channel set, one WF per
 * phyCalDo call, each after item 10 retunes the SX to the call's frequency
 * (P[0xc]); decoded against MediaTek's cold stock load (all 56 calls of its
 * three band phases regenerated record for record). They leave only TX
 * power-control and TX-FIR words (and T2{8}'s state below); reception needs
 * none of them. Save/restore of their results: "Restore flags" below;
 * their registers: the T2 sections at the end. [static][measured] */
/*
 * T2{4}: TSSI gain-table (DNL) calibration, one WF per call (body V:e0055e88).
 * The WF transmits the DFE test signal (WF_COMM +0xd9c0 [3] pulse) at a set
 * of TX gain steps; the WF's TSSI detector measures each one (RFINTF +0x73a0
 * [16] busy, +0x73b0 power); per step i the firmware computes
 *   d_i = round(10 * log10(P_ref / P_i) * 8) + 8 * (6 * (steps - i) - 6)
 * (1/8 dB; P_ref = the last step on the MT7976, step steps - 3 on the
 * MT7977), subtracts 8 * a per-step target table byte and writes the result
 * into all six PA-bias bytes of TSSI PWR_DNL_GT<i>_OFST_00/01 (MT7977: GT0..7
 * of 8 steps; MT7976: GT1..6 of 6 steps). Bytes outside [-0x40, 0x40] zero
 * all 16 words. The call saves the words in its result record and its
 * restore orchestration writes them back after the body's backup restore
 * (restore flag 0x200000, below). MT7976: the WF transmits with TX +
 * feedback-RX enabled on itself (A-die 0x010 = 0x09fa3bcf); MT7977: the WF
 * in TX (0x01f823c5) with its feedback partner (k4 for WF 0/1, k0 for WF
 * 2/3) in feedback receive (0x087a3bcf) through the loop network
 * (rfmodes.h). TX only; everything else is restored. Band 1, WF 0/1: also
 * turns on the band-1 background-RX block (as T2{0..3}), restored. Results
 * vary between cold loads by 1..2 LSB (1/8 dB): measured.
 * HAZARD: transmits (a TX gain sweep on the WF). [static, measured]
 */
#define   MT7996_PHY_CALMASK_TSSI_DNL     HW_BIT(18)	/* was _DPD_AUX */
/*
 * T2{5}: TSSI PA-cell gain-code calibration, one WF per call, MT7977 only
 * (bands 1, 2; body V:e0056a0c, mode 0x17). Sweeps the TX gain index
 * 1..0x39 (A-die 0x241 [27:21] and RFINTF +0x7250 [26:20], the same value),
 * picks the index whose TSSI reading is closest to 0xf20 in log10, moves the
 * TSSI range (WF_COMM +0xd9c4 [27:24]) until the reading lies in
 * 0x800..0x27ff, then measures 8 PA steps s (A-die 0x4a0 [27:24] = s, 0x920
 * [26:20] = [6:0] of A-die 0x8ef (s 0..3) / 0x8ee (s 4..7), field 0 only:
 * the loop-DC rows of mask bit 10, 0x40 on this unit). Cell byte s =
 * (DNL >> 1) + v_s / 2 + 4 * table byte, v_s = 10 log10(m_s / m_5) in
 * 0.25 dB, DNL = T2{4}'s GT byte read back (same WF); written into all 8 HEPA
 * and all 8 HLPA cells of TSSI PRIM_CORE_PA_TSSI_GC_OFST_00..03, each set
 * then normalised so step 5 is 0; on HEPA/HLPA_0..3 a per-chain offset of +8
 * (chains 0, 3) or +4 (chains 1, 2) is added. Result restore flag 0x400000.
 * TX only. Quirk: at frequency class 2 (above 6.585 GHz) the step-gain bytes
 * come from stale WM RAM. HAZARD: transmits (a TX gain sweep on the WF).
 * [static, measured]
 */
#define   MT7996_PHY_CALMASK_TSSI         HW_BIT(19)
/*
 * T2{0..3}: DPD (LUT-based digital predistortion, "LDPD"), one TX chain per
 * call (WF = P[1]; table 2 idx 0 in the stock load), body V:e004ddd8, every
 * channel set after T2{4}/T2{5}; mode 2 of the calibration HAL. Decoded
 * against MediaTek's cold stock load (all 12 calls, 203,444 records, record
 * for record, plus 6 second samples). Order: SX retuned to F - 20 MHz
 * (MT7976, VCO ratio 3/8) / F - 10 MHz (MT7977); TOP 0x744 [21:20] = 3
 * (MT7976); the PHY preparation; then the steps of the step-enable table
 * (V:e005cb48: steps 0..7 of 0..9 run on every die here; MemDPD steps
 * 10..15 instead when DPD_STATE [11:0] == 0x60 at BW320, or BW160 with P[7];
 * not decoded), each training LUT n = step of the chain (MT7996_PHY_DPD_LUT):
 * the TX chain transmits the DPD engine's training signal at a step-specific
 * TX gain while a feedback receiver captures it (MT7976: the next chain,
 * WF3 -> die 0's WF4 monitor receiver with the TTG PLL as LO; MT7977: bank
 * k4 for WF 0/1, chain k0 for WF 2/3, through the loop network, rfmodes.h);
 * a SW AGC sets the feedback gain and the TX gain until the feedback /
 * reference correlation ratio is 0..1/8 dB above the target (15 rounds at
 * most); then up to 8 hardware training runs fill the LUT (WF_COMM +0xe000
 * [0], done +0xe024 [31]), the WM checks it for compression and moves the TX
 * gain, records the TX gain of the LUT (MT7996_PHY_DPD_LUT_GAIN) and saves
 * both (restore flag 0x80). Last, when the WM scratch bit 0x8301fc0c [31 -
 * band] is set (EEPROM 0x199 + band [5]; set on all bands here) and P[6], a
 * TX power sweep: the band test-signal generator (MT7996_PHY_SIGGEN_*) at 4
 * TX gain codes x 1 (MT7976) / 8 (MT7977: A-die 0x4a0 [27:24]) detector
 * steps x every bandwidth up to the channel's (2 / 4 / 5 on bands 0 / 1 /
 * 2), five A-die detector readings each, the median kept in WM RAM.
 * Everything the body changes is put back by its exit (backup lists, mask
 * bit 3), except: the LUTs and LUT gain words (the result), RXFE_CTRL [7:4]
 * (left at the sweep's last bandwidth code: 1 / 3 / 4 on bands 0 / 1 / 2;
 * T2{6} then writes the same; phy.h), WF_COMM +0xe018 = 0xf, the
 * test-signal generator words +0xa008..+0xa054 (+0xb004 = 0x30000000 on
 * band 2), the chain's TX-flatness FIR set to the identity (T2{6} rewrites
 * it next) and the SX words of the last retune (back to F; the MT7976 stays
 * on 3/8 until a later prelude retunes).
 * MT7977 steps 0, 2, 3, 5 run with a fixed TX gain (byte table 0xe0080e88;
 * 3 AGC rounds, 1 training run, one unfixed retry when the LUT shows
 * compression > 2). On the MT7977 at BW160/BW320 (P[4] > 2; bands 1 and 2
 * here) the steps that loaded the amplitude-bin table (6, 7) clear their
 * LUT (16 entries + 1 word) before the save unless WM byte 0xe01ec19f
 * (band 1) / 0xe01ec19e (band 2) is 1 (0 here): their LUTs 6 and 7 end
 * all-zero.
 * Results: per chain 8 LUTs in D-die RAM and a WM RAM record ([P + 0x14] +
 * WF * 0xce4); the phyCalDo tail writes all 8 back from the record at once;
 * no other writer in the stock load.
 * Background-RX blocks (phy.h). Band 1, chains 0/1: every step also sets the
 * band-1 block (0x831a5c00 [0] = 1, 0x831a5df0 [3:0] = 3, 0x831ada20 [11:8]
 * = 3, 0x831ada2c [25] = 1, 0x831a3078 / 307c [12:0] = 0x1800, 0x831ad250
 * [16] = 1), restored by the D-die list. The band-0 block (0x830a5c00 = 1,
 * 0x830a5df0 = 0x2c303, BGRX_FE0 = 0x1a100001, BGRX_FE = 0x33008180,
 * 0x830a3078 = 0x830a307c = 0x1800, 0x830ad250 = 0x10400, set up for the
 * WF4 feedback receiver, saved and restored by the body) runs only for
 * MT7976 WF3, group 0, with WM flag 0xe01ec1c4 [7] ("LDPD", EEPROM 0x1a9
 * [7] [inferred]); not in the stock load (EEPROM 0x1a9 = 0x31).
 * Table set: the WM picks "E1" tables when CBTOP_HW_REV (conn.h) == 0x8a00,
 * else "E2" (forced by WF_COMM +0xd7a8 = 1 / 2, MT7996_PHY_DPD_FORCE_TBL);
 * this unit runs the E2 tables (8 trained steps; step 0 PA code 0x28, E1
 * would be 0x30), so the WM does not read 0x8a00 from 0x70010204 here.
 * [inferred: 0x70010204 is not in the trace; the E2 tables regenerate it]
 * HAZARD: transmits: ~50..70 captures per call (AGC 0.15..0.3 ms, training
 * 1.5..3 ms each) with the PA on (A-die TX gain code 0x20..0x32, DFE gain
 * -77..0 units of BN +0xfd38), plus 8 / 128 / 160 sweep tones of ~100 us on
 * bands 0 / 1 / 2. The FEM is in its DPD state (TOP 0x744 [21:20] = 3 on the
 * MT7976): assume power reaches the antenna port.
 * HAZARD (RX): the MT7976 feedback chain's RX DC engine is re-run inside the
 * call at the DPD gain (adie.h DCOC: 0x922 [31], done 0x98e [15]); its
 * result registers are in no restore list. Whether this replaces the
 * chain's normal-RX DC codes until the next op 8 is not established.
 * [static, measured]
 */
#define   MT7996_PHY_CALMASK_DPD          HW_BIT(20)
/*
 * T2{6}: TX flatness calibration of one WF (body V:e0059ad8, mode 0x15): the
 * band tone generator steps a single tone across the channel (4 rounds x 6
 * tones BW20, 18 tones BW160/320; bins from table 0xe0069f30), the A-die TX
 * power detector (0x477 [15] strobe, 0x5c4 reading) measures each, the
 * deficit to the reference tone becomes a 17-tap symmetric FIR (9 unique
 * 11-bit taps, inverse DFT) in chain +0xd034..+0xd044; identity FIR when a
 * tone deviates > 6 dB (> 10 dB at BW320). Before the sweep: a TX gain search
 * (MT7976; stock 0x390 / 0x29) or a detector attenuator search (MT7977, A-die
 * 0x4a0 [27:24]). Also writes its record word (MT7996_PHY_TXFLAT_REC below).
 * Saved/restored as flag 0x100000. TX only.
 * HAZARD: transmits CW tones. [static, measured]
 */
#define   MT7996_PHY_CALMASK_TX_FLAT      HW_BIT(21)
/* T2{7}: MemDPD retry. Reads DPD_STATE; runs V:e00db826 only when [11:0] ==
 * 0x60 and the channel is BW320 (P[4] == 4) or BW160 with P[7] == 1. The
 * stock oracle never reaches it (band 0 is BW20; bands 1 and 2 read 0), so on
 * the stock load the item is one read. 0x60 is written to DPD_STATE of bands 1
 * and 2 by V:e00d98ac (below). On band 0 the word is also entry (band 0,
 * chain 0) of the table T2{6} writes at 0x830ad760 + 0x10 band + 4 chain. No
 * A-die writes of its own; V:e00db826 decoded only structurally. [static,
 * measured] */
#define   MT7996_PHY_CALMASK_MEMDPD_RETRY HW_BIT(22)
/*
 * T2{8}: wideband TX-loop IQ calibration run by the band DSP (OTFIQ)
 * [inferred: the WM arms the DFE capture engine into the DSP's buffer, fixes
 * the TX gain index, starts the band test-signal generator, commands the DSP
 * and copies the 11-word result into 7 slots of a per-chain table]. Body
 * V:e0046360. The default configuration (written by the WM on every call)
 * runs it only above 5954999 kHz at BW320 (band 2 on this unit); other
 * channels: clear CHAIN_D000 +0x3a0 [1], [0], [17] and return. The long path
 * backs up 23 D-die and 5 A-die words and restores them; the DSP result words
 * differ between cold loads by 1..2 LSB. Registers: "OTFIQ" below; needs the
 * vendor DSP image. HAZARD: transmits (the band test-signal generator).
 * HAZARD (RX capture): on band 2 it leaves the DFE capture engine enabled
 * (XCAP CTRL 0x281, EN2 9, BD0 0x101, START/END at the DSP buffer, dfe.h),
 * CHAIN_D000 +0x3a0 [17], [16], [1], [0] set and A-die 0x03a = 0x1003 on
 * k 0..4 (role unknown; no earlier value seen). [static, measured]
 */
#define   MT7996_PHY_CALMASK_OTFIQ        HW_BIT(23)

/* Per-die A-die R-cal byte in the host EEPROM image: [7] valid, [4:0] code
 * (MT7977), [3:0] code (MT7976). The WM copies the three bytes to its RAM
 * before any calibration. Observed 0x87, 0x92, 0x96 (dies 0..2).
 * [static][measured] */
#define MT7996_PHY_EE_RCAL(die)           ((die) == 0 ? 0xbaau : (die) == 1 ? 0x57eu : 0x127eu)
#define   MT7996_PHY_EE_RCAL_VALID        HW_BIT(7)
#define   MT7996_PHY_EE_RCAL_CODE_MT7977  HW_GENMASK(4, 0)
#define   MT7996_PHY_EE_RCAL_CODE_MT7976  HW_GENMASK(3, 0)

/* WF_COMM +0xd7d8..+0xd7e8: firmware words the WM writes on all three bands
 * at calibration init (V:e00d4560, V:e00d45f0 / V:e00d45f8 / V:e00d4620).
 * +0xd7d8 = EEPROM 0x1a6 << 16 | 0x1a7 << 8 | 0x1a8 (V:e00e09cc(i) = the
 * host EEPROM image byte 0x1a6 + i; observed 0), so the MemDPD flags [8] /
 * [0] are EEPROM 0x1a7 [0] / 0x1a8 [0]; +0xd7dc, +0xd7e0, +0xd7e4, +0xd7e8 =
 * the host EEPROM image words at 0x1bb0, 0x1bb4, 0x1bb8, 0x1bbc,
 * byte-swapped (observed 0x44444444, then 0x21090400 three times). Band 0's
 * +0xd7e0 [31] enables the WM's "CalTime" log after every calibration body
 * (phyCalDo reads it after every call, whatever the band). +0xd7d8 [8] / [0]
 * make V:e00d98ac set DPD_STATE = 0x60 on band 1 / band 2 (below).
 * V:e00d98ac takes no band: the per-band PHY init calls it at the end of
 * every band's pass, and each call reads band 1's and band 2's
 * +0xd7d8..+0xd7e8 (five loads each, only +0xd7d8 used). [static][measured] */
#define MT7996_PHY_FW_D7D8(band)          MT7996_PHY_WFCOMM(band, 0xd7d8u)
#define   MT7996_PHY_FW_D7D8_MEMDPD_B1    HW_BIT(8)
#define   MT7996_PHY_FW_D7D8_MEMDPD_B2    HW_BIT(0)
#define MT7996_PHY_EE_FW_D7D8             0x1a6u	/* host EEPROM image, 3 bytes: [23:16] [15:8] [7:0] */
#define MT7996_PHY_FW_EECFG(band, n)      MT7996_PHY_WFCOMM(band, 0xd7dcu + 4u * (n))	/* n 0..3 */
#define   MT7996_PHY_FW_EECFG1_CALTIME_LOG HW_BIT(31)	/* n = 1 (+0xd7e0) */
#define MT7996_PHY_EE_FW_EECFG            0x1bb0u	/* host EEPROM image offset */

/* WF_COMM +0xd434 (band 0 only, no band term): WM control word, written
 * 0x70303000 at calibration init (V:e00d6c9a). Readers: [31] enables a
 * phyCalBasicConfig log (V:e0041180); [2] makes the channel set skip item 1,
 * the per-channel RF tables (V:e00d74a0 -> V:e00d751a), and selects the TX
 * power field write-back V:e00cc278(.., 0x3f) (V:e00cc86c;
 * MT7996_PHY_TXPWR_SAVE_* below). Other bits: no reader found.
 * Firmware-defined, not a hardware control [inferred]. [static][measured] */
#define MT7996_PHY_FW_D434                MT7996_PHY_WFCOMM(0, 0xd434u)
#define   MT7996_PHY_FW_D434_LOG          HW_BIT(31)
#define   MT7996_PHY_FW_D434_SKIP_ITEM1   HW_BIT(2)
#define   MT7996_PHY_FW_D434_INIT         0x70303000u

/* DPD state word in the DFE region: [11:0] == 0x60 (MemDPD state) triggers
 * the MemDPD retry (T2{7}) and is also tested by the calibration-restore
 * orchestration when choosing callees (the MemDPD restore, "Restore flags"
 * below); the result save reads it too. On bands 1 and 2 only V:e00d98ac
 * writes it: 0x60 when MT7996_PHY_FW_D7D8 [8] (band 1) / [0] (band 2) is set
 * and the CBTOP hardware revision (conn.h MT7996_CBTOP_HW_REV) is not
 * 0x8a00; the stock load reads 0. On band 0 the word is also entry (band 0,
 * chain 0) of the T2{6} record table (MT7996_PHY_TXFLAT_REC below; stock
 * 0x390290ba). DPD_STATUS is read with it on every orchestration pass; [17]
 * set skips the MemDPD restore. [static][measured] */
#define MT7996_PHY_DPD_STATE(band)        MT7996_PHY_WFCOMM(band, 0xd760u)
#define   MT7996_PHY_DPD_STATE_VAL        HW_GENMASK(11, 0)
#define   MT7996_PHY_DPD_STATE_MEMDPD     0x60u
#define MT7996_PHY_DPD_STATUS(band)       MT7996_PHY_WFCOMM(band, 0xd798u)
#define   MT7996_PHY_DPD_STATUS_B17       HW_BIT(17)

/* ========================================================================
 * Calibration init and the group loops (outside phyCalDo). Decoded against
 * MediaTek's cold stock load: its common phase outside the calibration
 * calls (the WM idle loop and host command path, the PHY-init routine, this
 * calibration init, the per-band PHY init, item 0 and the group loops;
 * 120,889 records) is regenerated record for record. [static][measured]
 * ======================================================================== */

/*
 * PHY-init routine V:e00b0428, run by the host's UNI EFUSE_CONTROL command
 * in buffer mode (V:e00b076e -> V:e00b06a2), up to the calibration request:
 * per band the thermal manager V:e00aefca (15 temperature conversions per
 * band here; each reads die 2's TOP 0x02c four times and the band's own
 * die's once);
 * V:e00eb5f6 -> V:e00e1316 (MT7996_PHY_DPD_SCRATCH_SWEEP below); the LMAC
 * TX / RX disable V:e01245ec(4, band 0 / 1) (MT7996_LMAC_ARB_SCR below,
 * already 0xf0007300, written back unchanged); the FEM / antenna-switch pin
 * mux V:e00ec21e(4) -> V:e00cea48 on dies 0, 1, 2; one band-0 conversion;
 * then the calibration request V:e00e5922 -> V:e00e57da -> V:e00d7794 ->
 * V:e00d6c3e (issued by V:e00e5c72(0x15, 0) [inferred from order]).
 *
 * Calibration init V:e00d6c3e (host request 0x20000000 / 0x80000000 through
 * the request handler V:e00d7794), in order:
 *  1. WF_COMM band 0 +0xd434 = 0x70303000 (MT7996_PHY_FW_D434).
 *  2. halPhyCal op 24 (V:e004171e): the ROM patch hook (slot 0x0081b490,
 *     "Patch_HalPhyCalFunc") prints "==>Patch_HalPhyCalFunc(), FF" on the
 *     ROM console (mcu_periph.h MT7996_MCU_DBGPORT_DATA); die 2's TOP 0x02c
 *     is read twice: [7:0] -> the ECO byte, WM 0xe01ec180 (0x10 on this
 *     unit; > 9 selects the SX80_E3 presets and the 0x77e40312 RF_INIT
 *     variant of both MT7977 dies, adie.h "RF_INIT", mt7977.h).
 *  3. 0x8301fc10 = EEPROM 0x1ad [5], then [1] = EEPROM 0x1a9 [6]
 *     (MT7996_PHY_FW_1FC10).
 *  4. The R-cal bytes (MT7996_PHY_EE_RCAL) to WM RAM; MT7996_PHY_FW_D7D8 and
 *     MT7996_PHY_FW_EECFG on all three bands.
 *  5. Per band: band / die / AdieSel (ops 25, 26, 30), then the per-band PHY
 *     and TOP init (V:e00de622, V:e00dbfe6, the register tables V:e00d8ffe /
 *     V:e00d9028).
 *  6. V:e00cc3aa: the MT7996_PHY_TXPWR_SAVE_* fields read into WM RAM (no
 *     write).
 *  7. Per band b with the band configuration byte 0xe01a7ff8[4 b] != 0
 *     (MT7996_WM_BAND_RF_REC [7:0], "Channel set" below): the
 *     P block (V:e00d4a1e; P[6] = EEPROM 0x196 + b [1] (EPA), P[7] = [0]
 *     (ELNA)), the WF range (V:e00d6232: 0..3, band 2 0..4), item 0
 *     (V:e00d4cf0 when P[2] == 0 and P[0] is 0, 3 or 0x10; adie.h
 *     "RF_INIT"), T0[0] and T0[1] from inside it, then the group loop
 *     V:e00d5238 per group (below).
 * No item 10 (SX), op 7 or op 8 runs outside phyCalDo in this phase: the
 * stock census has no site of V:e004243c..V:e0044500, V:e004b7a4..V:e004d884
 * or V:e005e896..V:e005ee1e outside the calls there.
 *
 * After the calibration returns, the PHY-init routine's tail (V:e00b0428 and
 * its per-band handler table, V:e00b06bc): thermal conversions; the PHY
 * register backup V:e012c210 (89 masked reads); HEMU_RULE4 on all three
 * bands (phy.h MT7996_PHY_HEMU_RULE_*); the TX / RX state machine's event 7
 * for bands 0 and 1 (MT7996_LMAC_ARB_SCR below); per band the TMAC frame
 * power limits (mac.h MT7996_TMAC_*), chain 0xe90 [30] (adie.h
 * ADIE_CHAIN_E90), the FEM words TOP 0xb08 / 0x664 / 0x668 (mt7976.h
 * MT7976_TOP_B08 / _664, mt7977.h MT7977_TOP_TTG_668), the crystal trim on
 * die 2's TOP 0x654 / 0x658 (adie.h ADIE_EE_XTAL_TRIM) and V:e012ee16 (phy.h
 * MT7996_PHY_CHAIN_2000); then the WM idle loop again (mac.h).
 * [static][measured]
 */

/*
 * Group loop V:e00d5238(P, first WF, last WF, table) per group (group 0 on
 * band 0; 3..8, 1, 2 on band 1; 16..23, 9..15 on band 2), outside phyCalDo:
 *  per WF: phyCalBasicConfig item 1 (MT7977 only: the per-channel RF tables
 *    of (group, BW, WF), with the RF table key record, phy.h
 *    MT7996_PHY_RF_TBL_KEY), then V:e00d2540: MT7996_PHY_FW_GAINSTEP_IN /
 *    _P (below), then V:e00d1dc4: the A-die gain-step flags (TOP 0x4a4 [4:0]
 *    for group 0, else 6-bit fields of TOP 0x4a4 / 0x4a5 / 0x4a6 per WF;
 *    chain 0x631 (group 0) / 0x636 nibbles);
 *  per WF the wrapper V:e00d4eb4: item 8 backup (mask bit 3: 430 D-die
 *    words, ARB_SCR x 3, the A-die lists), item 16 (the defaults of the
 *    table programmer's items 8, 9, 17, 20 and the DPD LUTs, phy_map.h),
 *    T0[2] (groups 0, 3, 0x10; empty slot), the T1 calls {0,1} {7,8} {6,9}
 *    {18,23,19,20} (each gated by the mask);
 *  BW160 pass (groups 1, 3, 4, 7, 10, 11, 13, 15, 17, 19, 21 -> g' 24..34):
 *    P[4] = 3, P[0xc] from 0xe006f7e0; per WF item 1 and V:e00d2540, then
 *    per WF V:e00d4dfa: item 8, item 16, T1 {2,3} {11,12} {10,13} {21};
 *  BW320 pass (band 2; groups 5, 10, 12, 14, 16, 18, 20 -> g'' 41, 35..40):
 *    P[4] = 4, P[0xc] from 0xe006f7ba (kept afterwards); per WF item 1 and
 *    V:e00d2540, then per WF V:e00d4d8c: item 8, item 16, T1 {4,5} {14..17}
 *    {22}.
 * With WM byte 0xe01ec153 != 0 (stock) groups 2, 4, 5, 6, 24 and 26 (mask
 * 0x05000074) stop after T1 {0,1} / {2,3}; T1 {6..17} run only for WF < WM
 * byte 0xe01ec163 (4). T1[18..23] return 3 for WF >= 1, every other T1 call
 * returns 1 and T0[2] 0 ("Calibration call" below). Stock: 192 wrapper calls
 * (4 / 48 / 140 on dies 0 / 1 / 2), each with one item-8 backup and one
 * item-16 run (96 DPD-LUT words each). Every A-die and D-die register the
 * group loop writes outside phyCalDo is rewritten by the channel sets (the
 * band phases, also outside phyCalDo): it is calibration state only, not
 * part of a receiver's cold init. [static][measured]
 */

/* ========================================================================
 * Channel set (outside phyCalDo). Decoded against MediaTek's cold stock
 * load: its three channel switches (band 0 ch 7 BW20, band 2 ch 1 BW320
 * centre 31, band 1 ch 36 BW160 centre 50, in that order; 8,526 / 9,926 /
 * 10,031 records) are regenerated record for record by running MediaTek's
 * own instruction stream, except one TSSI pass per band of a thermal
 * callback registered at run time (8 records, TX only) and band 2's A-die
 * eFuse read through the ROM patch (6 records). [static][measured]
 * ======================================================================== */

/*
 * The host's UNI 0x34 (mcu_cmd.h) is handled by V:e00b103c. Tag 1
 * (UNI_CHSW_TAG_RX_PATH) writes MAC words only (RMAC SMPS_CTRL, MIB, TMAC /
 * RATE, BF), no PHY word. Tag 0 (UNI_CHSW_TAG_SWITCH): RMAC SMPS_CTRL,
 * V:e00b0a74, the handler tail. V:e00b0a74, in order: the channel compare
 * V:e00e5f00; LMAC ARB_SCR through the TX / RX state machine V:e01245ec
 * (MT7996_LMAC_ARB_SCR below; 0xf0007300 written back unchanged); TX power
 * V:e00b5588 (1,664 / 1,487 / 1,547 writes on bands 0 / 2 / 1: rate tables
 * by EEPROM 0x196 + band [1], TSSI, the thermal compensation V:e00cbd28,
 * MT7996_WM_TXPWR_THERM_STATE below); the temperature class V:e00af328 ->
 * P[8]; band 2 only V:e00ad640; the channel flow V:e00e5626 -> V:e00d708a
 * (below); the thermal TX power V:e00b0a42 (382 / 322 / 382 writes); at
 * BW320 only the background receiver V:e00ad4de (54 writes: band 0's
 * monitor receiver, phy.h MT7996_PHY_BGRX_* and MT7996_PHY_BN_BGRX, the AHB
 * window MT7996_PHY_AHB_START / _END = 0x0041f400 / 0x0041f600, conn.h
 * MT7996_WF_TOP_ON_SLPPROT_REQ2 = 0x00140008, A-die eFuse 0x471 through the
 * ROM patch V:00903f1a / V:00901bac); the MAC words (mac.h "Channel set":
 * TMAC TFCR0, RATE BWCR, WFSYS_ON TICKCR0, RMAC CHFREQ); BN +0x8554 /
 * +0x8d08 (V:e00afca2, unchanged); ARB_SCR again; band 0 only
 * MT7996_PHY_BN_BGRX [6:4] = 3 (V:e00caf72). The handler tail: MIB
 * (V:e00af2e0), TMAC / RATE (V:e00f9b96), the BF words 0x8308521c.. from
 * EEPROM 0xc00..0xc23 (V:e00facc2), AGG (V:e00a7e38), TMAC (V:e00f044e).
 * [static][measured]
 *
 * Channel flow V:e00d708a, in order (writes on bands 0 / 2 / 1):
 *  1. HAL ops 0x20, 0x23; V:e00d3f78 (a per-WF EEPROM table, WM RAM only).
 *  2. One read of MT7996_PHY_DPD_SCRATCH_FREEZE(0) (0x8301fc00, the same
 *     address on every band): MT7996_PHY_CHSET_TRIG_* below.
 *  3. P block 0xe0184f98 (V:e00d4a1e, mode 2): P[0] = the group
 *     V:e00d41f6(centre kHz) (0 below 4850 MHz, else the MT7977 frequency
 *     groups 1..23, mt7977.h), P[4] = BW class, P[6] / P[7] = EEPROM 0x196
 *     + band [1] / [0] (EPA / ELNA, V:e00e05c6; phy.h MT7996_PHY_EE_196),
 *     P[5] = P[6] | P[7], P[8] = temperature class, P.u32[0xc] = centre
 *     kHz, P[0x20] = band; copied to 0xe0184f2c + 0x24 band. Stock (P[0],
 *     P[4], P[6], P[7], kHz): (0, 0, 1, 1, 2442000), (10, 4, 1, 1,
 *     6105000), (1, 3, 1, 1, 5250000).
 *  4. WF range V:e00d6232 (0..3; band 2 0..4); HAL op 0x1f.
 *  5. V:e00d406c: MT7996_WM_BAND_RF_REC's type checked against the group;
 *     band and A-die installed (HAL ops 0x19..0x1b).
 *  6. The key records of the two PHY tables (phy_map.h "Channel-set PHY
 *     tables"): V:e00d17da (table A), V:e00d1836 (table B).
 *  7. Per WF: item 1 (MT7977 only, with MT7996_PHY_FW_D434_SKIP_ITEM1
 *     clear: EPA_COMMON, COMMON_BW (BW160 / BW320 only), XLNA, XPA,
 *     EPA_GROUP; 0 / 869 / 700 writes); item 16 (not gated: the TX-table
 *     defaults, 221 writes per WF); V:e00d2540 flag 1
 *     (MT7996_PHY_FW_GAINSTEP_IN / _P and the A-die gain-step fields TOP
 *     0x4a4..0x4a6, chain 0x631 / 0x636, as in "Group loop" above).
 *  8. V:e00d4732: the channel-set calibrations, chosen by the calibration
 *     state arrays (WM 0xe00816f0, 0xe0082498, 0xe0082f84); stock: band 1
 *     only, per WF item 16 and T1[10, 11, 13] at g' 24, T1[21] once, then
 *     op 8 with MT7996_PHY_CAL_RESTORE_TXIQ (13 phyCalDo calls).
 *  9. PHY table A V:e00d9238 (247 / 320 / 320) and table B V:e00d9702
 *     (665 on every band): phy_map.h.
 * 10. The RX front end: V:e00d188e (phy.h MT7996_PHY_BN_6188,
 *     MT7996_PHY_DFE_F168), V:e00d196a (MT7996_PHY_CHAIN_D0AC), V:e00d1a5e
 *     (MT7996_PHY_CSD_AGC_TBL, 50 / 48 / 48 entries per chain), V:e00d1b72
 *     (2.4 GHz only: MT7996_PHY_WFCOMM_0080 [7:4], the primary channel).
 * 11. V:e00d34d2 (bands 1, 2): DPD state read-backs, values unchanged.
 * 12. V:e00ddd0e: "Channel-set D-die RX calibration" below (90 / 107 / 97).
 * 13. V:e00d62d6: op 7 V:e004085a(P, 0xff, 1), then op 8 V:e004243c(P,
 *     MT7996_PHY_CHSET_RESTORE, X) per WF with P[0] = the channel's group
 *     at its bandwidth (0 / g'' 35 / g' 24), on 4 / 5 / 4 WFs; X = 0
 *     except on band 2's WF 4 (no TX calibration): X =
 *     MT7996_PHY_CHSET_RESTORE_TX, its TX items written with their
 *     defaults. Per item: "Restore flags" below.
 * 14. V:e00d5820 (item 10's P[1]: 0 on every MT7977 band, A-die type 7,
 *     at every bandwidth; 3 on the MT7976; sxchan.h SXCH_P1_*), V:e00d8f7c,
 *     then item 10, the SX (44 / 128 / 128; the lock poll needs 3 matching
 *     reads before its deadline).
 * 15. V:e00d6438: the T2 calibrations, one WF per phyCalDo call: band 0
 *     T2{4,0,6,7,8}, then op 8 with MT7996_PHY_CAL_RESTORE_TSSI_PAGC (the
 *     defaults on the MT7976, 8 writes per WF); bands 1 and 2
 *     T2{4,5,0,6,7,8}.
 * 16. Per WF V:e00d2540 flag 0: the gain-step inputs again, then the RX
 *     gain offsets (phy.h MT7996_PHY_CSD_RXGOFS_*).
 * [static][measured]
 *
 * Receive-only subset. Evidence: MediaTek's minimal-mask cold load (mask
 * 0x00020001, items 0 and 10 only; it receives), every D-die write's (pc,
 * address) compared with the stock channel sets. Prerequisites: item 0 and
 * the per-band PHY init ("Calibration init" above). A receiving channel
 * set does: [measured]
 *  R1 the P block (group from the centre frequency, BW class, EPA / ELNA,
 *     WF range) and the band / A-die selection (steps 3..5);
 *  R2 item 1 on the MT7977 (the per-channel A-die RF tables and their key
 *     words), present in the minimal load;
 *  R3 PHY table B and its explicit fields: all 333 (pc, address) pairs of
 *     the stock channel set are present on every band;
 *  R4 MT7996_PHY_BN_6188, MT7996_PHY_DFE_F168, MT7996_PHY_CHAIN_D0AC, the
 *     AGC table entries MT7996_PHY_CSD_AGC_TBL and band 0's
 *     MT7996_PHY_WFCOMM_0080 [7:4];
 *  R5 the D-die RX calibration V:e00ddd0e with its A-die part (the RX
 *     bandwidth, 0x933, the MT7976 TOP 0x4b0 / 0x4b4 and 0x172, the MT7977
 *     SX out-force): every (pc, address) pair present;
 *  R6 item 10, the SX (mask bit 17);
 *  R7 both V:e00d2540 passes (the gain-step fields, the RX gain offsets).
 * Not required, for RX quality: op 8's RX bits MT7996_PHY_CHSET_RESTORE_RX
 * (the stored DCOC, FIQ and RXIQC results reapplied; they need mask bit 3,
 * absent from the minimal load) and band 1's T1[21] (160 MHz RX IQ). TX
 * only: the TX power V:e00b5588 / V:e00b0a42 and the thermal path, item
 * 16, op 8's TX bits MT7996_PHY_CHSET_RESTORE_TX, the T2 calibrations
 * V:e00d6438 (they drive the transmitter), V:e00d34d2, band 1's T1[10, 11,
 * 13], the BF and TMAC words. Not established: PHY table A (mostly TSSI /
 * TPC / DPD / TX-power words, but it runs under the minimal mask and also
 * writes the per-chain +0x2000 block, role unknown); at BW320 the
 * background receiver V:e00ad4de (present in the minimal load); the MAC
 * words (ARB_SCR, CHFREQ, BWCR, BN_BGRX [6:4]), used only by the MAC
 * receive path. [measured][inferred]
 *
 * HAZARD: transmits: with the stock mask every channel set runs the T2
 * calibrations (step 15) and the thermal callback's TSSI pass.
 * HAZARD: band 2 at BW320 reads A-die eFuse 0x471 through the ROM patch;
 * the eFuse controller path is the burn hazard of secure.h
 * (MT7996_EFUSE_CTRL_BASE).
 * Other inputs the channel set reads: MT7996_PHY_FW_D434 [2] skips item 1
 * without a log; MT7996_CBTOP_HW_REV == 0x8a00 selects other table fields
 * (phy_map.h; this unit is not 0x8a00); a table key outside its list
 * asserts in the WM (phy_map.h). [static][measured]
 */

/* Op 8's flag set at the channel set (V:e00d62d6, per WF; "Restore flags"
 * below) and its RX and TX parts. [static][measured] */
#define MT7996_PHY_CHSET_RESTORE          0x10020366u
#define MT7996_PHY_CHSET_RESTORE_RX       0x00000066u	/* DCOC coarse (2), fine (4), FIQ, RXIQC */
#define MT7996_PHY_CHSET_RESTORE_TX       0x10020300u	/* TXSTEP, TXLOIQ, TXIQ, LOOPDC; band 2 WF 4's X */
/* HAZARD: every channel set reads 0x8301fc00 (MT7996_PHY_DPD_SCRATCH_FREEZE(0),
 * also DPD's freeze-bit scratch word, below): 0x3333 reruns the whole
 * calibration init V:e00d6c3e inside the channel set (its TX calibrations
 * transmit), 0xffff takes HAL op 0xa0 instead; stock 0. A value left there
 * changes every later stock channel set. [static][measured] */
#define MT7996_PHY_CHSET_TRIG_CALINIT     0x3333u
#define MT7996_PHY_CHSET_TRIG_OPA0        0xffffu

/* WM per-band RF record (WM RAM 0xe01a7ff8 + 4 band, set at boot; its
 * [7:0] is the calibration init's band configuration byte, above): [7:0]
 * RF band type, checked by V:e00d406c against the group (1 = 2.4 GHz:
 * group 0; 2 = 5 GHz: groups 1..8; 3 = 6 GHz: groups 9..23); [15:8] the
 * band's A-die index (installed as the calibration HAL's band / A-die, ops
 * 0x1a / 0x1b); [23:16] the A-die type (6 MT7976, 7 MT7977, as the AdieSel
 * byte, phy_map.h MT7996_PHY_FCB_CFG_INIT; read by V:e00d5820). Firmware
 * state, not hardware. [static] Stock 0x00060001, 0x00070102, 0x00070203
 * on bands 0..2 [inferred: these reproduce the stock channel sets; not
 * read out]. */
#define MT7996_WM_BAND_RF_REC(band)       (0xe01a7ff8u + 4u * (band))
#define   MT7996_WM_BAND_RF_REC_TYPE      HW_GENMASK(7, 0)
#define   MT7996_WM_BAND_RF_REC_ADIE      HW_GENMASK(15, 8)
#define   MT7996_WM_BAND_RF_REC_ADIE_TYPE HW_GENMASK(23, 16)

/* WM TX-power thermal state bytes 0xe01ec128 + 2 band + c, c = (BW class
 * >= 3) (V:e00cc7b4): byte 2 band < 0 selects the full temperature
 * compensation path; byte 2 band + c selects the register list V:e00cbd28
 * writes (0xff: the 0x830ad4f4.. list, 0: the 0x8328ff14.. list). TX
 * only. [static] Stock 0xff except band 2, c = 1 (0) [inferred: these
 * reproduce the stock TX power writes; not read out]. */
#define MT7996_WM_TXPWR_THERM_STATE(band, c) (0xe01ec128u + 2u * (band) + (c))

/* CHAIN_D000 TX power fields [name guess: TX power defaults], saved by
 * V:e00cc3aa into WM RAM at calibration init (all bands, chains 0..3; reads
 * only) and written back by V:e00cc278 (V:e00cbd28 write mode;
 * MT7996_PHY_FW_D434 [2]). Field tables {u32 address, u8 msb, u8 lsb}:
 * 0xe01eb830 (+0xd270 [31:24] [23:16], +0xd2ac [31:24] [23:16]), 0xe01eb7d0
 * (+0xd29c [28:24] [23:16] [15:8]), 0xe01eb710 (+0xd258 [31:24], +0xd284
 * [31:24] [23:16]), 0xe01eb6b0 (+0xd290 [12:8] [7:0], +0xd294 [31:24]),
 * 0xe01eb5b0 (+0xd104 [25:16] [9:0], +0xd108 [25:16] [9:0], +0xd10c
 * [25:16], +0xd110 [25:16], +0xd118 [24:16] [8:0], +0xd11c [24:16] [8:0],
 * +0xd120 [24:16], +0xd124 [24:16]), 0x0222499c (relocated .data; +0xd120
 * [8:0]): 26 fields. Stock reads e.g. +0xd270 = 0x80000080, +0xd104 =
 * 0x01430143, +0xd118 = 0x00e400e4 on every band and chain. TX only.
 * [static][measured] */
#define MT7996_PHY_TXPWR_SAVE_FIRST       0xd104u	/* chain block offset */
#define MT7996_PHY_TXPWR_SAVE_LAST        0xd2acu

/* ========================================================================
 * Calibration call (phyCalDo), DCOC result handling, the table-2 restore
 * flags, the calibration tail. Decoded against MediaTek's cold stock load:
 * its 384 DCOC calls, 144 loop-DC calls T1{6,10,14} (113,375 records), 144
 * TX gain-step calls T1{7,11,15} (180,788 records), 140 TX IQ calls
 * T1{8,12,16} (111,416 records), 144 TX LO / IQ calls T1{9,13,17}
 * (1,116,796 records), 449
 * TTG RX calls T1[18..23] (plus one in the band-1 phase; 1,669,451 records),
 * 56 T2{4..8} calls and 12 T2{0..3} calls are regenerated record for record.
 * [static][measured]
 * ======================================================================== */

/* P block at phyCalDo entry (T1 DCOC calls): [0] group (= a1: 0..23, g' 24..41),
 * [1] WF, [2] 0, [3] 1, [4] BW class (0, or 3 / 4 set by the BW160 / BW320
 * pass), [5..7] 1 ([6] EPA from EEPROM 0x197/0x198 bit 1), [8] 0, [9] 0
 * (0xff for T1[23]), [0xa] stale (V:e004011a rewrites it: 0 for T1[0..5],
 * 1 for T1[18..23]), [0xb] 0, [0xc] kHz: 0, or the pass frequency (BW160
 * restores it afterwards, BW320 does not). phyCalDo sets P[0xc] = calibration frequency and P[4] = byte
 * 0xe0068f50[idx - 2] (T1[0,1] 0, [2,3] 3, [4,5] 4) and restores both. [static][measured] */

/* phyCalDo(idx, a1, P, flag, table) return value (mark6 of the oracle):
 * 1 = body ran and the restore orchestration (V:e004243c) completed; 0 = empty
 * slot (state 0); 3 or 5 = skipped with the item's state: P[0xa] = 1 (an
 * all-chains item, word 0xe006f8a8 = 0x18000 = mask bits 15, 16: T1[18..23])
 * on a call for WF >= 1
 * (e0040a74..e0040a86), or the done state 5 (e0040a70). Every DCOC call
 * returns 1, every T1{9,13,17} call too; every TTG RX call on WF0 returns 1,
 * on WF1..4 3.
 * [static][measured] */

/* DCOC result handling (every DCOC call). [static][measured]
 *  check  V:e00454ee: one read per field; field mask ((1 << (lsb+w)) - 1) |
 *         (1 << (lsb+w)) with the RISC-V shift taken mod 32: 7-bit fields
 *         are checked as 8 bits; a 32-bit field (0x8f1/0x8f7) checks bit 0
 *         only and [31:24] of 0x8f2/0x8f8 nothing. A failure ORs the item
 *         flag (4 / 2) into WM word 0xe01ec1ac and logs into the WF's
 *         RFINTF +0xa14..+0xa28 (phy.h MT7996_PHY_CALLOG_*). None in the
 *         stock load.
 *  save   V:e0043d76 (slots 0xe0081254 / 0xe008124c): the six words read once
 *         more, fields packed into the WM RAM result record of (die, group,
 *         WF, BW class) chosen by V:e00423be from P[0x14]; valid flag set.
 *  write-back  phyCalDo's restore slot 0xe00811e8 = V:e004243c(P, F, 0),
 *         F = 4 (fine) / 2 (coarse): the record's fields written back whole
 *         (other bits 0); defaults 0x40404040 / 0 without a valid record
 *         (phy_map.h's indirect table items 2 / 1 [inferred]).
 *  reapply  op 8 = V:e004243c with F bits 2 and 1, outside phyCalDo, at every
 *         channel set: 12 A-die words per chain (V:e0044362), from the
 *         channel's group record. [measured: stock census, band 0 = group 0
 *         record, band 1 = g' 24 record] */
#define MT7996_PHY_DCOC_FINE_ROWS         0xe007039cu	/* WM rows {u16 idx, 4 x (lsb, w)} */
#define MT7996_PHY_DCOC_STEP_ROWS         0xe0070360u
/* WM word 0xe01ec1ac: one bit per result flag whose range check (V:e00454ee)
 * failed: [2] fine DC, [1] coarse DC, [28] loop DC, [8] TX gain-step codes
 * (flag 0x100), [6] (0x40, RX IQ filter bank), [5] (0x20, per-gain RX IQ).
 * Set by V:e00454ee only (OR); 0 in the image; nothing clears it. Each
 * item's write-back writes its defaults while its bit is set unless DBG_OVR
 * overrides (phy.h MT7996_PHY_DBG_OVR; V:e0044c62 bit 2, V:e0044d24 bit 1,
 * V:e0044dde bit 28, V:e0044e98 bit 8 (zeros), V:e004500a bit 6, V:e0044f48
 * bit 5). No bit for flag 0x200 (T1{9,13,17}): its check clamps and logs
 * without the OR. [static, measured: bit 28 set in the stock load] */
#define MT7996_PHY_DCOC_FAIL_FLAGS        0xe01ec1acu	/* WM RAM */
#define   MT7996_PHY_DCOC_FAIL_LOOPDC     HW_BIT(28)

/* Restore flags of the table-2 and the other T1 items (the F of V:e004243c
 * above; V:e004085a sets the call's flag when the item's mask bit is set)
 * and the result words their save/restore pair moves (saved by slot
 * 0xe00811ec = V:e0045a3a inside the body, written back by slot 0xe00811e8
 * = V:e004243c at the end of the same phyCalDo call, after the body's own
 * backup restore):
 *   T1{6,10,14} 0x10000000: the WF's 8 loop-DC result rows (adie.h
 *                   ADIE_LOOPDC_RES*), 4 x 7-bit fields each; restore slot
 *                   0xe0081240 = V:e0044dde (mask bit 10 above), defaults
 *                   0x40404040; range check V:e00454ee flag 0x10000000
 *   T1{7,11,15} 0x100: RFINTF +0x76b0, +0x76b4, +0x76b8 of the WF (phy.h
 *                   MT7996_PHY_TXSTEP_CODES), the WM table programmer's
 *                   item 8 window (phy_map.h); save V:e0045a3a ->
 *                   V:e0043d76 (reads at V:e0043e1c); restore V:e0044e98 ->
 *                   V:e0044234 (stores at V:e00442ba). Record by P: P[0]
 *                   == 0: +0x254; P[4] 0..2: +0x3a0 (the 20 MHz result is
 *                   also the BW40 / BW80 one); P[4] == 3: +0x124; P[4] ==
 *                   4: +0x204; other P[4]: nothing written. Without a valid
 *                   record, or with WM 0xe01ec1ac [8] set and no DBG_OVR
 *                   override: 0 to all three (V:e0044ec8 / V:e0044ed2 /
 *                   V:e0044edc). All fields are 5 bits, written back
 *                   without sign extension. Set by V:e004085a when mask bit
 *                   11 and idx & 0xf7 == 7 or idx == 11 (or 0xff).
 *                   Reapplied at every channel set (op 8): measured in
 *                   another cold stock load, band 0 chains 0..3 from the
 *                   group-0 record, band 2 chains 0..3 from g'' 35 and
 *                   chain 4 zeros (no record), band 1 chain 0 from g' 24;
 *                   before that, V:e004489e (after every item-8 backup)
 *                   zeroes the three words of the WF
 *   T1{8,12,16} 0x20000: the WF's 72 TX IQ rows (phy.h MT7996_PHY_TXIQC,
 *                   steps 0..8), the WM table programmer's item 17 window
 *                   (phy_map.h). Flag set by V:e004085a for table 1, mask
 *                   bit 12 and idx & 0xfb == 8 or idx == 0x10. Save slot
 *                   0xe008122c = V:e00440fc: record halfword +4 = 1,
 *                   V:e0043d76 packs the 72 rows (fields from 0xe0070174:
 *                   step 0 14-bit fields, steps 1..8 8-bit side taps and a
 *                   14-bit main tap) from record + 8. Restore slot
 *                   0xe0081228 = V:e004446c (from V:e004243c bit 17): a
 *                   valid record -> V:e0044234 (period 8, 8-bit fields
 *                   sign-extended to 14) writes the 72 rows (V:e00442ba);
 *                   else the defaults (V:e00444a4 / V:e00444b4). Record
 *                   (V:e00423be): group 0 [P + 0x14] + 0xa4 WF; else
 *                   V:e004222e by class: 0 -> [P + 0x14] + 0xa4 WF, 3 -> +
 *                   0x8e10 (band 1) / + 0x14984 (band 2), 4 -> + 0x18db4,
 *                   each + 0xa4 WF: one record per (band, WF, class), the
 *                   group does not enter. Runs at the end of every
 *                   T1{8,12,16} call (the 116 "done" calls: two reads and
 *                   72 row writes) and at every channel set (op 8, per WF):
 *                   measured in another cold stock load, band 0 chains 0..3
 *                   and band 1 chains 0..3 from the records, band 2 chains
 *                   0..3 from the class-4 record and chain 4 the defaults
 *                   (taps within 6..17 LSB of this load's); before it
 *                   phyCalBasicConfig item 16 (V:e004489e) writes the
 *                   identity rows (census)
 *   T1{9,13,17} 0x200: the WF's 40 item-9 rows (phy.h MT7996_PHY_TXLO_I /
 *                   _Q, MT7996_PHY_TXLOIQ_A / _B), the WM table programmer's
 *                   item 9 window (phy_map.h); row table
 *                   MT7996_PHY_TXLOIQ_ROWS: 8 groups g of 5 rows, +0x600 +
 *                   8g and +0x604 + 8g (0, 9) (16, 9), +0x640 + 4g (0, 7)
 *                   (8, 7) (16, 7) (24, 7), +0x660 + 4g (0, 32), +0x680 +
 *                   4g (0, 32); group g holds steps 4g..4g + 3. Flag set by
 *                   V:e004085a for table 1, mask bit 13 and idx & 0xfb == 9
 *                   or idx == 0x11. Save V:e0045a3a -> V:e00454ee -> range
 *                   check V:e004512a over all 32 steps (reads +0x600..
 *                   +0x63c, then 8 x +0x640 / +0x660 / +0x680 + 4i): a
 *                   class fails at |A| >= MT7996_PHY_TXLOIQ_LIM_A, |B| >=
 *                   _LIM_B, |LO I| or |LO Q| >= _LIM_LO; then V:e0044c18
 *                   reads DBG_OVR (phy.h) twice, on every call; without its
 *                   signature each failing class is overwritten
 *                   (MT7996_PHY_TXLOIQ_CLAMP_*) and V:e00453ee logs code 5
 *                   (phy.h MT7996_PHY_CALLOG_CODE_TXLOIQ), with no OR into
 *                   WM word 0xe01ec1ac; then slot 0xe0081234 = V:e0044096:
 *                   record halfword +4 = 1, V:e0043d76 packs all 40 rows
 *                   (128 bits per group) from record + 8. Restore slot
 *                   0xe0081230 = V:e004438a (from V:e004243c bit 9): X bit
 *                   9 or no valid record -> all 40 rows 0 (V:e0044430);
 *                   else V:e0044234 writes the first 5 * ceil(n / 4) rows
 *                   (20 MT7976, 15 MT7977; period = rows: no sign
 *                   extension); no DBG_OVR check and no 0xe01ec1ac
 *                   condition. Record (V:e00423be, V:e004222e): one per
 *                   (band, group, WF, class), the group entering (unlike
 *                   flag 0x20000); R = [P + 0x14] + 0x290: group 0 R + 0x338
 *                   WF + 0x264; class 0..2 g 1..8 (band != 2) R + 0x45c WF
 *                   + 0x1170 (g - 1) + 0x3b0, g 9..23 R + 0x45c WF + 0x15cc
 *                   (g - 9) + 0x3b0; class 3 g' 24..27 (band != 2) R +
 *                   0x8e10 + 0x1e0 WF + 0x780 (g - 24) + 0x134, g' 28..34 R
 *                   + 0x14984 + 0x1e0 WF + 0x960 (g - 28) + 0x134; class 4
 *                   g'' 35..40 R + 0x18db4 + 0x2c0 WF + 0xdc0 (g - 35) +
 *                   0x214; a 20 MHz group number with class 3 / 4 selects
 *                   g' 24 (band 1) / g' 28, g'' 35 (band 2). Runs at the end
 *                   of every T1{9,13,17} call and at every channel set (op
 *                   8, per WF; mask bit 13 above); stock: no range-check
 *                   failure
 *   T1{18..22} 0x40: the chain's RX IQ compensation filter bank (phy.h
 *                   MT7996_PHY_RXIQC_A / _B), slots 0..5 (0..11 at P[4] =
 *                   4) from the (group, bandwidth) record; restore
 *                   V:e004500a -> V:e0044234 (stores at V:e00442ba), 120
 *                   words per chain (240 at P[4] = 4); range check
 *                   V:e00454ee flag 0x40
 *   T1[23] 0x20:    the chain's per-gain RX IQ table (phy.h
 *                   MT7996_PHY_FIQ_*), 15 words; restore V:e0044f48, the
 *                   stored table only at P[4] = 0 (20 MHz)
 *   T2{4} 0x200000: 16 words TSSI +0x218..+0x254 (V:e0042a66 / V:e0042aba)
 *   T2{5} 0x400000: 4 words TSSI +0x05c..+0x068 (V:e0042b1e / V:e0042b6e)
 *   T2{6} 0x100000: 5 words chain +0xd044, +0xd034..+0xd040 (V:e004291a /
 *                   V:e004299e)
 *   T2{7} 0x1000000: MemDPD restore V:e0043ab0 (DPD_STATE [11:0] == 0x60,
 *                   DPD_STATUS [17] clear, BW320 or BW160 with P[7] == 1)
 *   T2{0..3} 0x80: the chain's 8 LUTs and their gain words (each step's
 *                   save V:e0042c98 copies LUT n and MT7996_PHY_DPD_LUT_GAIN
 *                   n [9:0] into the record; the write-back covers all 8,
 *                   LUT count byte 0xe0081268)
 *   T2{8} none.
 * Without a valid record the restore writes defaults: 0 (T2{4}); 0x58,
 * 0x341000f0, 0x60, 0x381000f0 (T2{5}, P[0] != 0); 0x200, 0, 0, 0, 0
 * (T2{6}); 0x10000000 in half A word 5 of every slot, all else 0
 * (T1{18..22}, phy.h MT7996_PHY_RXIQC_DEFAULT_MAIN); 0x02000200,
 * 0x02000200, 0 per triplet (T1[23]); 0x40404040 in all 8 rows (T1{6,10,14});
 * 0, 0, 0 (T1{7,11,15}); 0x10000000 in word 3 of every row group, all else
 * 0 (T1{8,12,16}, phy.h MT7996_PHY_TXIQC_DEFAULT_MAIN); 0 in all 40 rows
 * (T1{9,13,17}). Bits 21, 22, 20, 17, 9, 8, 6 and 5 are the WM table
 * programmer's items with these windows and defaults (phy_map.h), as F bits
 * 2 / 1 are for the DCOC records and bit 28 for the loop-DC rows (indirect
 * item 28). A failed T1 range check ORs its flag (0x40 / 0x20 / 0x100 /
 * 0x10000000) into WM word 0xe01ec1ac
 * (MT7996_PHY_DCOC_FAIL_FLAGS) and logs, as the DCOC check does; the
 * flag-0x200 check logs without the OR and clamps instead (above); in the
 * stock load only the loop-DC check fails (mask bit 10 above).
 * [static, measured] */
#define MT7996_PHY_CAL_RESTORE_LOOPDC     HW_BIT(28)
#define MT7996_PHY_CAL_RESTORE_TXSTEP     HW_BIT(8)
#define MT7996_PHY_CAL_RESTORE_TXIQ       HW_BIT(17)
#define MT7996_PHY_CAL_RESTORE_TXLOIQ     HW_BIT(9)	/* V:e004085a: idx & 0xfb == 9 or idx == 0x11 */
#define MT7996_PHY_CAL_RESTORE_TSSI_DNL   HW_BIT(21)
#define MT7996_PHY_CAL_RESTORE_TSSI_PAGC  HW_BIT(22)
#define MT7996_PHY_CAL_RESTORE_TX_FLAT    HW_BIT(20)
#define MT7996_PHY_CAL_RESTORE_MEMDPD     HW_BIT(24)
#define MT7996_PHY_CAL_RESTORE_DPD        HW_BIT(7)
#define MT7996_PHY_CAL_RESTORE_RXIQC      HW_BIT(6)
#define MT7996_PHY_CAL_RESTORE_FIQ        HW_BIT(5)
#define MT7996_PHY_CAL_RESTORE_DCOC_FINE  HW_BIT(2)	/* F = 4, "DCOC result handling" */
#define MT7996_PHY_CAL_RESTORE_DCOC_COARSE HW_BIT(1)	/* F = 2 */

/* Tail of every DCOC body, in order (e.g. MT7977 fine: e004c56e..e004c58c):
 *  1. result check and save (above), V:e0045a3a;
 *  2. WF_COMM +0xd4d0 RMW to 0x10101010 (phy.h), V:e0048b26;
 *  3. A-die 0x031 / 0x032 (adie.h ADIE_CALTAIL_*), V:e00486fc;
 *  4. D-die restore V:e00489c2: the 430 backed-up PHY words, the band's
 *     WFSYS gate sequence (conn.h, at MT7996_CONN_PWR_800), then ARB_SCR x3;
 *  5. the calibration-end PHY pulse (phy.h), V:e0047e36 / V:e0049416;
 *  6. A-die restore from the backup: chain lists (V:e00482f0), then the TOP
 *     list (V:e0048486); zeros without bit 3 (adie.h HAZARD).
 * T2{4..6} end with steps 2..6 too (V:e0049416; T2{6} restores the A-die
 * lists before the D-die words). A-die lists: 71 and 69 chain words and 15
 * TOP words (MT7977), 39, 35 and 5 (MT7976). T1{18..23} (all chains in one
 * call) end with steps 2, 3, 4 and 6: the D-die list, then the A-die lists
 * of every chain (214 writes on the MT7976, 416 on the MT7977) and TOP;
 * phyCalDo then reads WF_COMM +0xd7e0 (MT7996_PHY_FW_EECFG1_CALTIME_LOG)
 * and runs V:e004243c once per WF. [static][measured] */

/* LMAC ARB_SCR (0x820e3000 / 0x820f3000 / 0x830e3000 for band 0/1/2; mac.h
 * MT7996_LMAC_BASE + 0x3000): the PHY preparation sets [8] (TX disable) and
 * [9] (RX disable); every calibration tail writes all three back from the
 * backup (no band term); stock value 0xf0007300 ([9:8] TX/RX disable from
 * the PHY preparation still set). With mask bit 3 clear the tail writes 0 to
 * all three. [static][measured]
 * GCI names (module = band, conn.h "GCI"): [8] MAC0_TX_DIS, [9] MAC0_RX_DIS,
 * [28] BCNQ_EMPTY_CTRL, [29] TTTT_BTIM_CTRL, [30] TBTT_BCN_CTRL, [1:0] /
 * [3:2] / [5:4] / [7:6] BCNQ_OP_MODE0..3. Outside the calibrations [8] and
 * [9] are written through the WM's per-band TX / RX state machine
 * V:e01245ec -> V:e01244b2 (TX_DIS = !tx_en, RX_DIS = !rx_en, one GCI
 * transaction: one read, one write): event 4 (forced disable, the PHY-init
 * routine before the calibration request) and event 7 (re-evaluate, its
 * tail) leave both set until the host starts the band; both rewrite
 * 0xf0007300 unchanged in the stock load, as does every channel set (twice,
 * "Channel set" above). [static][measured] */
#define MT7996_LMAC_ARB_SCR(band)         ((band) == 2 ? 0x830e3000u : (band) == 1 ? 0x820f3000u : 0x820e3000u)
#define   MT7996_LMAC_ARB_SCR_BCNQ_OP_MODE(n) HW_GENMASK(2u * (n) + 1u, 2u * (n))	/* n 0..3 */
#define   MT7996_LMAC_ARB_SCR_TX_DIS      HW_BIT(8)
#define   MT7996_LMAC_ARB_SCR_RX_DIS      HW_BIT(9)
#define   MT7996_LMAC_ARB_SCR_BCNQ_EMPTY_CTRL HW_BIT(28)
#define   MT7996_LMAC_ARB_SCR_TTTT_BTIM_CTRL  HW_BIT(29)
#define   MT7996_LMAC_ARB_SCR_TBTT_BCN_CTRL   HW_BIT(30)
#define   MT7996_LMAC_ARB_SCR_CAL         0xf0007300u

/* ========================================================================
 * Channel-set D-die RX calibration V:e00ddd0e
 *
 * Runs on every channel set (channel switch, specific cal, thermal re-cal,
 * test-mode channel set, PHY init). It forces a known A-die filter / BW
 * state (A-die TOP 0x4b0 / 0x4b4, chain 0x172 [31:28] = 0xa, 0x100 / 0x102
 * = BW code, 0x933 / 0x934, synthesizer 0xa0f = 3) and writes the D-die
 * words below with the band's WFSYS gate closed. [static] Decoded against
 * MediaTek's cold stock load (90 / 107 / 97 writes on bands 0 / 2 / 1,
 * regenerated record for record; "Channel set" above), in order:
 * [static][measured]
 *  1. The gate closed, the calibration tail's field sequence (conn.h at
 *     MT7996_CONN_PWR_800): band 0 MT7996_CONN_PWR_10C [8] = 0, then
 *     MT7996_CONN_PWR_800 [3], [5] = 0; band 1 MT7996_CONN_PWR_110 [8] =
 *     0, then [9], [11]; band 2 MT7996_WFSYS_ON_LPCR_DIS(2) = 0x10, then
 *     [16], [18], [20] (the tail's further fields read-modified, already
 *     0).
 *  2. Chains: MT7996_PHY_CHAIN_D010 [7] = 1 (chains 0..3, and chain 4 on
 *     bands 1 and 2), cleared instead at BW40 (V:e00dddec; not in the stock
 *     sets); MT7996_PHY_CHAIN_E01C [7] = 1
 *     (chains 0..4); MT7996_PHY_BN_CLK0 / _CLK1 [17] = 1; CHAIN_E034 [29]
 *     = 1, then 0, on chains 0..4; BN_CLK1 / BN_CLK0 [17] = 0; CHAIN_E024
 *     [0] = 1 (chains 0..4).
 *  3. V:e00dd40a: MT7996_PHY_DFE_FCB0 = MT7996_PHY_DFE_FCB4 = the AdieSel
 *     pattern, then MT7996_PHY_TXCAL_TBL(i) (code immediates, V:e00dd69a.. /
 *     V:e00dd9c4..V:e00dd646; phy_chan.h MT7996_PHY_TXCAL_TBL_*): MT7976 only
 *     with EPA on the 2.4 GHz base, MT7996_PHY_DFE_FCB_PAT_CFG6 = 0x00123456
 *     (band 0) and 16 words; MT7977 off the 2.4 GHz base, _CFG7 = 0x00123455
 *     (bands 1 and 2) and 16 words with EPA, 32 (i 0..31) without.
 *  4. Read-modify-writes that leave the stock values: MT7996_PHY_BN_MODE
 *     [8] (0x80000000), MT7996_PHY_WFCOMM_3000 [31] (0x00212000),
 *     MT7996_PHY_BN_B000 twice (0xcc000000), MT7996_PHY_BN_8238 [15]
 *     (0x6920a8e0), MT7996_PHY_DFE_F000 [3:1] cleared (0x0107d630); then,
 *     one branch per BW class (V:e00de088..V:e00de4d6; nothing for classes
 *     >= 5), BN_MODE [27:24] (MT7996_PHY_BN_MODE_CODE) = the BW class itself,
 *     not a selector code (0 / 3 / 4 here: BW20 / BW160 / BW320 on bands 0 /
 *     1 / 2); at BW160 DFE_F000 [3:1] = 5 (band 1 here); off band 0
 *     MT7996_PHY_WFCOMM_D24C [15] = 1 at BW80, else 0 (0, unchanged); at
 *     BW320 BN_MODE [8] on the second channelization's centres.
 *  5. MT7996_WF_TOP_ON_BAND_RST bit band = 0, then 1.
 *  6. The gate opened as in the tail: MT7996_CONN_PWR_800 [3], [5] / [9],
 *     [11] / [16], [18], [20] = 1, then the gate word (MT7996_CONN_PWR_10C
 *     / _110 [8] = 1; band 2 MT7996_WFSYS_ON_LPCR_EN(2) = 0x10).
 *  7. BN_MODE [23:20] (MT7996_PHY_BN_MODE_PRI) = the primary 20 MHz
 *     channel's index; WFCOMM_3000 [16] = 1; BN_MODE [29:28]
 *     (MT7996_PHY_BN_MODE_RF) by the record's base frequency: 0 at 2,407,000
 *     kHz, 1 at 5,000,000 / 4,000,000 kHz, 2 else ([31:30] kept: 0x80000000
 *     / 0x93000000 / 0xa4000000 on bands 0 / 1 / 2, the [31:28] values 8 / 9
 *     / 0xa that DPD's sweep writes per group); on 2.4 GHz WFCOMM_3000
 *     [31:28] = 0; MT7996_PHY_BN_8200 [21] by band: set on 2.4 GHz, cleared
 *     on 5 / 6 GHz (unchanged here); MT7996_PHY_DFE_FD08 [31] pulsed;
 *     MT7996_PHY_DFE_E338 [0] = 1 below BW320, [1:0] = 0 at BW320.
 *  8. MT7977 only (AdieSel byte 7, WM byte 0x02224a93): the gain-index
 *     selector set 1 (phy.h MT7996_PHY_GSEL_*) rewritten with the per-band
 *     PHY init's values (MAP 0x76543210, 0xfedcba98; GRP 0, 0; STEP
 *     0x04000005, 0x00030000, 0x200, 1; whole words), at BW320 then
 *     read-modified to GRP 0x11110000, 0 and STEP 0x08000909, 0x07060008,
 *     0x00050400, 0x302, with STEP [4] (+0xd8e0) = 1.
 *  9. MT7996_PHY_BN_6098 [0] = 1; DFE_E338 read-modified again
 *     (unchanged).
 * 10. The A-die (V:e00dcd88): MT7976 (die 0): ADIE_TOP_CHSET0 /
 *     ADIE_TOP_CHSET1 = 0x1f / 0x1f; per chain k0..3 ADIE_CHSET [31:28] =
 *     0xa (0x2a000003 -> 0xaa000003), ADIE_RX_BW = the BW code (0),
 *     ADIE_CHSET_933 = ADIE_CHSET_933_MT7976. MT7977: k0 ADIE_SX_OUT_FORCE
 *     = 3, then per bank k0..4 ADIE_RX_BW = the BW code (3 at BW160, 4 at
 *     BW320) and ADIE_CHSET_933 = ADIE_CHSET_933_MT7977(code) (2, 3).
 * 11. MT7996_PHY_WFCOMM_B580 [24:23] = 1 (V:e00de3fc), on every channel
 *     set whatever MT7996_CBTOP_HW_REV (0x01000007 -> 0x00800007 here).
 * ======================================================================== */

/* ========================================================================
 * WM PHY preparation (before calibration / mode change), band B:
 * [static] Every step is a fresh read-modify-write unless "=".
 *  1. WF_COMM +0xd874 |= 0x10000000; +0xd480 |= 0x00100000;
 *     BN +0x0000 [12] = 0; twin BN +0x0000 [12] = 0.
 *  2. LMAC ARB_SCR (0x820e3000 / 0x820f3000 / 0x830e3000 for band 0/1/2)
 *     |= 0x100 (TX disable), then |= 0x200 (RX disable).
 *  3. WF_COMM +0xd4d0 = 0x1010101f; +0xd4e4 |= 0xf00, [23:12] cleared.
 *  4. WF_COMM +0x3000 [31] = 0; +0x3004 [10:8] = 2, [14:12] = 3,
 *     [27:16] = 0.
 *  5. Chains 0..4: CSD +0x154 [28:24] = 0.
 *  6. RST_CTRL |= 0x3ffff then = 0; same on the twin (+0x90014).
 *     WF_COMM +0x3008 |= [18], [19], [23] then = 0.
 *  7. BN_MODE [30:28] = flag ? 1 : 0; then FCB0/FCB4 = pattern chosen by a
 *     configuration byte (phy_map.h).
 *  8. Bands 1 and 2 only: WF_COMM +0xd24c [15] = 0; +0xd234 [8] = (mode 16);
 *     +0xd230 [5:4] = mode 16 ? 1 : 0; +0xd240 [14:0] = mode 16 ? 0x11d4 :
 *     0x11f4 (phy.h MT7996_PHY_WFCOMM_D230 / _D234 / _D240; the per-band
 *     PHY init writes the mode-16 values).
 *  9. Modes 4, 5, 12: chains 0..4 +0xf028 = 0x7c00.
 * 10. Except modes 2, 21, 27: selected chain TSSI_PWR_DFE_BACKOFF_01
 *     MAN = 1, then BACKOFF = 7.
 *
 * WM PHY register helper (mode, selected chain), band B: [static]
 *  1. BN_CLK0/1 and twin CLK0/1 = 0x01b60000 (whole words).
 *  2. WF_COMM +0x3000 [3] = 1.
 *  3. Chains 0..4 TRX_CLK [7], then [6]. Modes 4/5: selected chain [4] = 0,
 *     [5] = 1; other modes: [5] on all chains, then [4] on the selected one.
 *  4. BN +0x70 |= 0xff000000, then |= 0x0000ff00.
 *  5. BN +0x1044: modes 5, 12 |= 0x20 then 0x30; modes 0, 1, 2, 4, 5, 12,
 *     20, 23 [0] then [1].
 *  6. Unless mode 16: RXFE +0xfc20 [0] = 1.
 *  7. Modes 4/5: WF_COMM +0xd4d0 = 0x1010101f, +0xd4e4 as in preparation.
 *     Other modes: +0xd4d0 [4] set, [3:0] = 0, [11:8] = chain one-hot
 *     (modes 2, 20, 23) or 0xf, [12] set, [19:16] = 0xf, [20] set, [27:24] =
 *     0xf (modes 2, 20, 23) else 0, [28] = (modes 1, 2, 20, 23);
 *     +0xd4e4 [11:8] = 0.
 *  8. Chains 0..3 +0xd008 [6] = 1; mode 16: [7] = 0 on all and [6] = 0 on
 *     the selected chain; other modes: [7] = 1 on the selected chain.
 * ======================================================================== */

/* ========================================================================
 * RX gain, DC and IQ calibration controls
 *
 * RX gain calibration (A-die RF_TEST_RX_GAIN_CAL) forces, per chain and LNA
 * index L: RXGAIN |= B_EN | C_EN with LPF fields 0 and B_LNA = C_LNA = L;
 * RXGAIN_D EN = 1, [15:12] = 0, D_LNA = L; pulses RST_CTRL RXTD; samples
 * through the DFE capture engine. A second calibration sets all three force
 * enables. [static] The result is a per-gain-state table (host EEPROM
 * image, MT7996_PHY_EE_RX_GAIN_CAL_*), so a forced index uses the calibrated
 * entry. DC offset and frequency-domain IQ compensation are calibrated per
 * gain state, so a gain change can move DC. [inferred] The TTG-tone RX IQ
 * calibrations leave one IQ compensation filter pair per RX gain step and
 * bandwidth (phy.h MT7996_PHY_RXIQC_*) and one gain/phase pair per gain step
 * (phy.h MT7996_PHY_FIQ_*). [static][measured]
 *
 * iBF phase calibration forces set C on chains 3, 2, 0, 1: RXGAIN = C_EN |
 * L << 24 | P << 28 and AGC_RFDGC C_GAIN = G, from per-level (L, P, G)
 * tuples, e.g. (1,9,3) (2,3,7) (3,3,0x3f) (4,3,0x37) (5,3,1) (3,7,5)
 * (2,7,0xb) (1,9,0xd) (4,3,7); RXGAIN and AGC_RFDGC of all band-0/1 chains
 * are saved before and restored after. [static]
 *
 * WF_RXFE_01 (phy.h): ADCDC_COMP_VAL [31:28] (ADC DC compensation) and
 * RXFDIQ_TAP_NUM [27:26] (frequency-domain IQ compensation taps; every
 * calibration writes 2) are the only RXFE fields with a calibration role.
 * ======================================================================== */

#define MT7996_PHY_EE_RX_GAIN_CAL_OFF     0x5b00u	/* host EEPROM image offset [static] */
#define MT7996_PHY_EE_RX_GAIN_CAL_LEN     0x0440u

/* ========================================================================
 * TTG-tone RX IQ calibrations (T1{18..23}, mask bits 15 and 16): per-step
 * A-die gain and TTG tone placement. Their D-die engines and result tables
 * are in phy.h (RFINTF and WF_COMM TTG RX words), the A-die side in
 * rfmodes.h, sx.h, mt7976.h and mt7977.h. [static][measured]
 * ======================================================================== */

/* Per-step forced A-die gain (chain 0x600 [11:8] LNA step, [4:1] LPF step,
 * adie.h ADIE_DCOC_STEP), byte tables in WM data, row = (group != 0); the
 * MT7977 reads index 2 j except at 320 MHz (j). [static][measured]
 * Mode 0x13 (T1{18..22}): MT7976 rows of 10 at 0xe0071728 (LNA) /
 * 0xe0071714 (LPF): steps 0..5 = LNA 4,4,3,2,1,0, LPF 7,1,1,1,1,1. MT7977
 * rows of 12 at 0xe0071cc4 / 0xe0071cac: below 320 MHz LNA 5,4,3,2,1,0,
 * LPF 2 x6; 320 MHz LNA 5,5,4,4,3,3,2,2,1,0, LPF 2,5,2,5,2,5,2,5,2,2.
 * Mode 0x16 (T1[23]): MT7976 rows of 11 at 0xe0071784 / 0xe007176c: steps
 * 0..10 = LNA 4,4,3,3,2,2,1,1,0,0,4, LPF 1,3,1,3,1,3,1,3,1,3,7. MT7977
 * rows of 12 at 0xe0071d24 / 0xe0071d0c, row 1 read at index 2 j for j =
 * 0..11, i.e. up to 11 bytes past the row's end: LNA 5,4,3,2,1,0 then 7 x6
 * (bytes of the next table, 0xe0071d3c..), LPF 0 x6 then 5,4,3,2,1,0 (the
 * LNA table's row 0); the written 0x600 values confirm it (LNA 5..0 at LPF
 * 0, then LNA 7 at LPF 5..0).
 * TTG level / route select per step (A-die 0x940 [7:0] / 0x606 [5:4], sx.h;
 * the MT7977 route helper ignores both): mode 0x13 MT7976 0xe0071700 /
 * 0xe00716ec, MT7977 0xe0071c94 / 0xe0071c7c; mode 0x16 MT7976 0xe0071754 /
 * 0xe007173c, MT7977 0xe0071cf4 / 0xe0071cdc.
 * WF_COMM +0xd8d0 [4:0] per step in mode 0x16 (V:e00503aa; phy.h
 * MT7996_PHY_IQE_STEP): MT7976 0xe0071500 = 1,1,2,2,3,3,4,4,5,5,0; MT7977
 * 0xe0071950 = 0,0,1,1,2,2,3,3,4,4,5,5 (0xe007193c = 0,1,1,2,3,3,4,5,5,6,7,7
 * at bw 4). */
#define MT7996_PHY_TTG_RX_LNA_MT7976       { 4u, 4u, 3u, 2u, 1u, 0u }
#define MT7996_PHY_TTG_RX_LPF_MT7976       { 7u, 1u, 1u, 1u, 1u, 1u }
#define MT7996_PHY_TTG_RX_LNA_MT7977       { 5u, 4u, 3u, 2u, 1u, 0u }
#define MT7996_PHY_TTG_RX_LPF_MT7977       2u	/* every step */
#define MT7996_PHY_TTG_RX_LNA_MT7977_BW320 { 5u, 5u, 4u, 4u, 3u, 3u, 2u, 2u, 1u, 0u }
#define MT7996_PHY_TTG_RX_LPF_MT7977_BW320 { 2u, 5u, 2u, 5u, 2u, 5u, 2u, 5u, 2u, 2u }
#define MT7996_PHY_TTG_RX_FIQ_LNA_MT7976   { 4u, 4u, 3u, 3u, 2u, 2u, 1u, 1u, 0u, 0u, 4u }
#define MT7996_PHY_TTG_RX_FIQ_LPF_MT7976   { 1u, 3u, 1u, 3u, 1u, 3u, 1u, 3u, 1u, 3u, 7u }
#define MT7996_PHY_TTG_RX_FIQ_LNA_MT7977   { 5u, 4u, 3u, 2u, 1u, 0u, 7u, 7u, 7u, 7u, 7u, 7u }
#define MT7996_PHY_TTG_RX_FIQ_LPF_MT7977   { 0u, 0u, 0u, 0u, 0u, 0u, 5u, 4u, 3u, 2u, 1u, 0u }
#define MT7996_PHY_TTG_RX_FIQ_D8D0_MT7976  { 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u, 0u }
#define MT7996_PHY_TTG_RX_FIQ_D8D0_MT7977  { 0u, 0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u }

/* TTG tone placement (V:e004859e / V:e0048534). i = sp8f * 5 + bw with
 * sp8f = 2 for bw 0, 1; 1 for bw 2 (above 2.4 GHz, V:e004a830; mode 0x16
 * skips that adjustment); 0 for bw 3, 4. DC phase: tone = LO + 5000 kHz,
 * no offset (TTG programming mode 4). IQ phase (mode 0x13, TTG mode 5):
 * tone = LO - SUB[i], offset word = OFF[i] (A-die 0x944 [22:0], the same
 * Q23 format as 0x941's fraction, sx.h). Mode 0x16: both phases LO + 5000
 * kHz, TTG mode 4 (V:e00486a0 maps 0x13 -> 5, 0x16 -> 4); TOP 0x060 = 3 in
 * the DC phase, 1 in the IQ phase of both modes; the IQ phase's front-end
 * mode word (V:e0049b40, WF_COMM +0xd49c, phy.h) is 0x13's in both modes.
 * N = mult * F / ref:
 * MT7976 ref 40 MHz, mult 2 (2.4 GHz); MT7977 ref 80 MHz (P[0x10]), mult
 * 1; INT = 0x941 [31:23], FRAC = 0x941 [22:0]. kHz tables at 0xe00692c0
 * (OFF), 0xe00692fc (SUB). E.g. die 0 group 0 (LO 2442 MHz): 0x941 =
 * 0x3d2ccccc (DC), 0x3c8ccccc (IQ), 0x944 = 0x80100000; die 2 group 16:
 * 0x28f00000 / 0x28c80000, 0x944 = 0x80040000. [static][measured]
 * (P[0x10] itself is not traced: 40 MHz on die 0 and 80 MHz on dies 1, 2
 * reproduce every TTG word of the stock load.) */
#define MT7996_PHY_TTG_RX_SUB_KHZ \
	{ 15000u, 30000u, 60000u, 120000u, 240000u, \
	  16250u, 32500u, 65000u, 130000u, 260000u, \
	  20000u, 40000u, 80000u, 160000u, 320000u }
#define MT7996_PHY_TTG_RX_OFF_KHZ \
	{ 625u, 1250u, 2500u, 5000u, 10000u, \
	  1250u, 2500u, 5000u, 10000u, 20000u, \
	  2500u, 5000u, 10000u, 20000u, 40000u }
#define MT7996_PHY_TTG_RX_DC_TONE_KHZ     5000u

/* ========================================================================
 * TOAE calibration, band block 0x83080c60.. (+band << 20). [static: ROM]
 * The TOAE calibration measures AFE<->DFE latency by looping TX into RX
 * [inferred]; CAL_PATH_SEL is the only named path mux near the ADC.
 * ======================================================================== */

#define MT7996_PHY_TOAE_CAL0(band)        MT7996_PHY_BN(band, 0x0c60u)
#define   MT7996_PHY_TOAE_CAL0_TX_PHASE   HW_GENMASK(26, 24)	/* RO */
#define   MT7996_PHY_TOAE_CAL0_FSM        HW_GENMASK(18, 16)	/* RO */
#define   MT7996_PHY_TOAE_CAL0_PATH_SEL   HW_GENMASK(3, 2)
#define   MT7996_PHY_TOAE_CAL0_MODE       HW_BIT(1)
#define   MT7996_PHY_TOAE_CAL0_ST         HW_BIT(0)	/* CAL_ST */
#define MT7996_PHY_TOAE_CAL1(band)        MT7996_PHY_BN(band, 0x0c64u)
#define   MT7996_PHY_TOAE_CAL1_DCR        HW_GENMASK(28, 16)
#define   MT7996_PHY_TOAE_CAL1_CP         HW_GENMASK(12, 0)
#define MT7996_PHY_TOAE_CAL2(band)        MT7996_PHY_BN(band, 0x0c68u)
#define   MT7996_PHY_TOAE_CAL2_BUFF       HW_GENMASK(12, 0)
/* TOAE_CAL_3: [17:16] RO AVG_TOAE_OUT, [14] RO CAL_A2E, [13] and [12]
 * unnamed. */
#define MT7996_PHY_TOAE_CAL3(band)        MT7996_PHY_BN(band, 0x0c70u)
#define   MT7996_PHY_TOAE_CAL3_AVG_OUT    HW_GENMASK(17, 16)
#define   MT7996_PHY_TOAE_CAL3_A2E        HW_BIT(14)

/* ========================================================================
 * Per-chain TSSI block (PHYDFE_CTRL_WF_TSSI, ROM module 0x3e): 0x8300c000 +
 * band << 20 + chain << 16, 13 instances (bands 0/1 chains 0..3, band 2
 * chains 0..4). 141 registers / 512 fields in the ROM table. [static: ROM]
 * Register families (offsets within the block, count):
 *   0x00c CORE_MOD_DBV (1)         0x010 CORE_DATA_DBV (2)
 *   0x018 CORE_LMS (1)             0x020 CORE_TEMP_DBV (2)
 *   0x028 ER_OFFSET, 0x02c ER_PWR_TH, 0x030 PLR_OFFSET, 0x034 PLR_PWR_TH
 *   0x038 CORE_CH_CBW160_DBV (4)   0x048 FINE_OFFSET_CTRL (2)
 *   0x054 PRIM_CORE_CURVE_OFST     0x058 PRIM_CORE_LMS_SIZE
 *   0x05c PRIM_CORE_PA_TSSI_GC_OFST (4)
 *   0x06c DCOC                     0x070 PRIM_OFFSET (4)
 *   0x080 FINE_PRIM_OFFSET (4)     0x090 LD_PRIM_OFFSET (4)
 *   0x0a0 LD_FINE_PRIM_OFFSET (4)  0x0b0 PRIM_CORE_CH_DBV
 *   0x0b4 PRIM_CORE_SEG0_CH_DBV (20)
 *   0x200 PWR_CTRL                 0x204 PWR_DATA_OFST
 *   0x208 PWR_BW_OFST (3)          0x214 PWR_CVT_OFST
 *   0x218 PWR_DNL_GT0..7_OFST (2 each, to 0x254)
 *   0x258 PWR_DFE_BACKOFF (2)      0x260 PWR_ALIGN_OFFSET
 *   0x264 PWR_EPA_FRONT_GAIN (2)   0x26c PWR_SYNAMIC_TX_LPF
 *   0x270 ETSSI_AC_MODE (2)        0x278 EPA_GAIN
 *   0x27c TXPWR_PROTECTION         0x2f8 RSVD (2)
 *   0x320 PWR_OFS (23)             0x380 CURVE_SLOPE (8)
 *   0x3a0 ADAPTIVE_STEP (8)        0x3c0 TARGET_BW_OFFSET (8)
 * The WM table programmer writes PA_TSSI_GC_OFST (item 22) and
 * PWR_DNL_GTn_OFST (item 21) from calibration data (phy_map.h); T2{5} and
 * T2{4} (mask bits 19, 18) measure them.
 * ======================================================================== */

#define MT7996_PHY_TSSI(band, chain, off) MT7996_PHY_CHAIN_REG(band, chain, 0xc000u + (off))

/* CORE_MOD_DBV_00: per-mode target offsets. */
#define MT7996_PHY_TSSI_MOD_DBV(band, chain) MT7996_PHY_TSSI(band, chain, 0x00cu)
#define   MT7996_PHY_TSSI_MOD_DBV_CCK     HW_GENMASK(15, 8)
#define   MT7996_PHY_TSSI_MOD_DBV_OFDM    HW_GENMASK(7, 0)
/* PRIM_CORE_PA_TSSI_GC_OFST_00..03: eight 8-bit PA-cell gain-code offsets
 * per PA mode (00/01 HEPA 0..7, 02/03 HLPA 0..7), byte 0 in [31:24]: word n
 * byte (3 - j) = PA cell 4 (n & 1) + j (ROM
 * WF_TSSI_PRIM_PA_CELL_TSSI_GC_OFFSET_*). T2{5}'s result (restore flag
 * 0x400000). [static, measured] */
#define MT7996_PHY_TSSI_PA_GC_OFST(band, chain, n) MT7996_PHY_TSSI(band, chain, 0x05cu + 4u * (n))
#define   MT7996_PHY_TSSI_PA_GC_CELL(j)   HW_GENMASK(31 - 8 * (j), 24 - 8 * (j))	/* j 0..3 */
/* DCOC_00: TSSI detector DC offsets. */
#define MT7996_PHY_TSSI_DCOC(band, chain) MT7996_PHY_TSSI(band, chain, 0x06cu)
#define   MT7996_PHY_TSSI_DCOC_LD         HW_GENMASK(28, 16)
#define   MT7996_PHY_TSSI_DCOC_VAL        HW_GENMASK(12, 0)
/* PWR_CTRL_00. The WM's TX gain setter (V:e0049a8c) sets [2] in modes 2 /
 * 0x1b; T2{8} sets TX_GC_IDX_MAN with TX_GC_IDX = OTFIQ CFG2 STEP + 8 * SLOT
 * (below). [static] */
#define MT7996_PHY_TSSI_PWR_CTRL(band, chain) MT7996_PHY_TSSI(band, chain, 0x200u)
#define   MT7996_PHY_TSSI_PWR_LATCH_EN    HW_BIT(31)
#define   MT7996_PHY_TSSI_PWR_LATCH_METHOD HW_GENMASK(29, 28)
#define   MT7996_PHY_TSSI_DPD_GN_TRK      HW_BIT(25)
#define   MT7996_PHY_TSSI_DPD_SHIFT_RM    HW_BIT(24)
#define   MT7996_PHY_TSSI_TX_GC_IDX_MAN   HW_BIT(16)
#define   MT7996_PHY_TSSI_TX_GC_IDX       HW_GENMASK(14, 8)
#define   MT7996_PHY_TSSI_ETSSI_EN        HW_BIT(5)
#define   MT7996_PHY_TSSI_GAIN_MAP_DPC_ONLY_FREE_RUN HW_BIT(4)
#define   MT7996_PHY_TSSI_ETSSI_PWR_IS_POST HW_BIT(3)
#define   MT7996_PHY_TSSI_GAIN_MAP_FREE_RUN HW_BIT(2)
#define   MT7996_PHY_TSSI_TX_GC_IDX_METHOD HW_GENMASK(1, 0)
/* PWR_DNL_GT0..7_OFST_00/01: 16 words, 0x218..0x254; word n is
 * GT(n >> 1)_OFST_0(n & 1), at 0x218 + 8 GT (_00) and 0x21c + 8 GT (_01).
 * Six signed 8-bit PA-bias offsets per TX gain table: _00 [31:24] BIAS0,
 * [23:16] BIAS1, [15:8] BIAS2, [7:0] BIAS3; _01 [31:24] BIAS4, [23:16] BIAS5
 * ([15:0] unused). T2{4} writes one measured value into all six (restore
 * flag 0x200000); T2{5} reads a GT's _00 [7:0] back. [static, measured] */
#define MT7996_PHY_TSSI_DNL_OFST(band, chain, n) MT7996_PHY_TSSI(band, chain, 0x218u + 4u * (n))
#define   MT7996_PHY_TSSI_DNL_BIAS0       HW_GENMASK(31, 24)	/* _00, n even */
#define   MT7996_PHY_TSSI_DNL_BIAS1       HW_GENMASK(23, 16)
#define   MT7996_PHY_TSSI_DNL_BIAS2       HW_GENMASK(15, 8)
#define   MT7996_PHY_TSSI_DNL_BIAS3       HW_GENMASK(7, 0)
#define   MT7996_PHY_TSSI_DNL_BIAS4       HW_GENMASK(31, 24)	/* _01, n odd */
#define   MT7996_PHY_TSSI_DNL_BIAS5       HW_GENMASK(23, 16)
/* PWR_DFE_BACKOFF_00: DFE TX back-off per bandwidth. */
#define MT7996_PHY_TSSI_DFE_BACKOFF0(band, chain) MT7996_PHY_TSSI(band, chain, 0x258u)
#define   MT7996_PHY_TSSI_BACKOFF_CBW320_SMALL_RU HW_GENMASK(29, 24)
#define   MT7996_PHY_TSSI_BACKOFF_CBW320_LARGE_RU HW_GENMASK(21, 16)
#define   MT7996_PHY_TSSI_BACKOFF_CBW160  HW_GENMASK(13, 8)
#define   MT7996_PHY_TSSI_BACKOFF_CBW20_40_80 HW_GENMASK(5, 0)
/* PWR_DFE_BACKOFF_01: WM PHY preparation sets MAN = 1, BACKOFF = 7. */
#define MT7996_PHY_TSSI_DFE_BACKOFF1(band, chain) MT7996_PHY_TSSI(band, chain, 0x25cu)
#define   MT7996_PHY_TSSI_BACKOFF_ENB     HW_BIT(31)
#define   MT7996_PHY_TSSI_BACKOFF_MAN     HW_BIT(28)
#define   MT7996_PHY_TSSI_BACKOFF_VAL     HW_GENMASK(26, 20)
/* PWR_SYNAMIC_TX_LPF_00 (sic): dynamic TX LPF power offsets. */
#define MT7996_PHY_TSSI_TX_LPF(band, chain) MT7996_PHY_TSSI(band, chain, 0x26cu)
#define   MT7996_PHY_TSSI_TX_LPF2_OFFSET  HW_GENMASK(15, 8)
#define   MT7996_PHY_TSSI_TX_LPF1_OFFSET  HW_GENMASK(7, 0)
/* Offset names of the per-band PHY init's per-chain register-table rows (phy_init.h
 * MT7996_PHY_INIT_REGTBL_PERCHAIN, written whole; ROM names in the comment). Role
 * unknown. [static][measured] */
#define MT7996_PHY_TSSI_014(band, chain)   MT7996_PHY_TSSI(band, chain, 0x014u)	/* init; CORE_DATA_DBV_01 */
#define MT7996_PHY_TSSI_018(band, chain)   MT7996_PHY_TSSI(band, chain, 0x018u)	/* init; CORE_LMS */
#define MT7996_PHY_TSSI_260(band, chain)   MT7996_PHY_TSSI(band, chain, 0x260u)	/* init; PWR_ALIGN_OFFSET */
#define MT7996_PHY_TSSI_278(band, chain)   MT7996_PHY_TSSI(band, chain, 0x278u)	/* init; EPA_GAIN */
#define MT7996_PHY_TSSI_27C(band, chain)   MT7996_PHY_TSSI(band, chain, 0x27cu)	/* init; TXPWR_PROTECTION */

/* Band TSSI core, PHYDFE_CTRL_BN (+band << 20). [static: ROM] Further band
 * TSSI words: PWR_GT_TH_00..11 0x8308fd40..6c, PWR_GT_PA_GAIN_MODE_00
 * 0x8308fd78, PWR_ETSSI_TH_00/01 0x8308fe50/54, ETSSI_AC_MODE_00..02
 * 0x8308fe58..60. The DFE gain compensation words 0x8308fdf0..0c that DPD
 * reads: MT7996_PHY_DFE_GAIN_COMP (the DPD section). */
#define MT7996_PHY_TSSI_CORE0(band)       MT7996_PHY_BN(band, 0xfd00u)
#define   MT7996_PHY_TSSI_CORE0_PKT_MODE  HW_BIT(31)	/* set by WM PHY init; T1{18..23} write 0 */
#define MT7996_PHY_TSSI_CORE1(band)       MT7996_PHY_BN(band, 0xfd04u)
#define   MT7996_PHY_TSSI_CORE1_EN_DBM_OFDM HW_GENMASK(27, 20)
#define   MT7996_PHY_TSSI_CORE1_EN_DBM_CCK HW_GENMASK(19, 12)
#define   MT7996_PHY_TSSI_CORE1_B11       HW_BIT(11)	/* cleared by the channel-set RX setup when P[0x1c] == 2 (V:e00de218) */
#define MT7996_PHY_TSSI_CORE3(band)       MT7996_PHY_BN(band, 0xfd10u)
#define   MT7996_PHY_TSSI_CORE3_OFFSET_MAN_EN HW_BIT(8)
#define   MT7996_PHY_TSSI_CORE3_OFFSET_MAN HW_GENMASK(7, 0)

/* ========================================================================
 * Band TX precompensation and TX power control (PHYDFE_CTRL_BN, +band << 20).
 * [static: ROM] Families: TPC_BFG_{BW20,40,80,160,RU26,52,106}_00/01
 * 0x8308e440..74 (4-bit beamforming gains per STS/TX count); TPC_SKU2_FCC_*
 * 0x8308e478..0x8308e734 (FCC limits per bandwidth/RU); TPC_MCS_{LG,HT,VHT,
 * HE} 0x8308e738..58 (3-bit per-MCS classes); TPC_MCS_CTL_00 0x8308e764;
 * TPC_RSVD_00/01 0x8308e798/9c; TPC_CTRL_STAT_00..12 0x8308e7a0..d0 (status).
 * ======================================================================== */

/* TX_PRECOMP_0: CFO estimate filter constants; _1.._4 [20:0] SW averages. */
#define MT7996_PHY_TX_PRECOMP0(band)      MT7996_PHY_BN(band, 0xe204u)
#define   MT7996_PHY_TX_PRECOMP0_ALPHA_NORMAL HW_GENMASK(31, 27)
#define   MT7996_PHY_TX_PRECOMP0_ALPHA_TF HW_GENMASK(26, 22)
#define MT7996_PHY_TX_PRECOMP_AVG(band, n) \
	MT7996_PHY_BN(band, (n) == 0 ? 0xe208u : 0xe20cu + 4u * (n))	/* n 0..3 */
#define   MT7996_PHY_TX_PRECOMP_AVG_VAL   HW_GENMASK(20, 0)

/* DFE_BN_TX words of the per-band PHY init (phy.h "Per-band PHY init"; not
 * in the ROM PHYDFE_CTRL_BN_CMM map). Band 0 only (V:e00de6f8..V:e00de8bc):
 * 0x8308e334 [7:0] = 0x14 (absolute address, no band term; 0x64 -> 0x14);
 * for n = 0..2: +0xe2a4 + 0x2c n [23:16] = 0x5a, [15:8] = 0x53, [7:0] = 0x64
 * (0x000a0314 -> 0x005a5364); +0xe2c0 + 0x2c n [31:24] = 0x67, [23:16] =
 * 0x64, [15:8] = 0x78 (0x1714281e -> 0x6764781e). All bands: +0xe330
 * [23:16] = 0x18, [15:8] = 0x18, [7:0] = 0x1a (0x0028282c -> 0x0018181a).
 * [static][measured] [inferred: TX, from the block role] */
#define MT7996_PHY_DFE_E334               0x8308e334u	/* band 0 */
#define MT7996_PHY_DFE_E2A4(band, n)      MT7996_PHY_BN(band, 0xe2a4u + 0x2cu * (n))	/* n 0..2 */
#define MT7996_PHY_DFE_E2C0(band, n)      MT7996_PHY_BN(band, 0xe2c0u + 0x2cu * (n))	/* n 0..2 */
#define MT7996_PHY_DFE_E2X_N              3u	/* E2A4 / E2C0 n 0..2 */
#define MT7996_PHY_DFE_E330(band)         MT7996_PHY_BN(band, 0xe330u)
#define   MT7996_PHY_DFE_BYTE3            HW_GENMASK(31, 24)
#define   MT7996_PHY_DFE_BYTE2            HW_GENMASK(23, 16)
#define   MT7996_PHY_DFE_BYTE1            HW_GENMASK(15, 8)
#define   MT7996_PHY_DFE_BYTE0            HW_GENMASK(7, 0)
#define   MT7996_PHY_DFE_E334_BYTE0_INIT  0x14u	/* e00de708, band 0 */
#define   MT7996_PHY_DFE_E2A4_BYTE2_INIT  0x5au	/* e00de720, band 0 */
#define   MT7996_PHY_DFE_E2A4_BYTE1_INIT  0x53u	/* e00de73c */
#define   MT7996_PHY_DFE_E2A4_BYTE0_INIT  0x64u	/* e00de760 */
#define   MT7996_PHY_DFE_E2C0_BYTE3_INIT  0x67u	/* e00de772 */
#define   MT7996_PHY_DFE_E2C0_BYTE2_INIT  0x64u	/* e00de784 */
#define   MT7996_PHY_DFE_E2C0_BYTE1_INIT  0x78u	/* e00de79e */
#define   MT7996_PHY_DFE_E330_BYTE2_INIT  0x18u	/* e00de8d8 */
#define   MT7996_PHY_DFE_E330_BYTE1_INIT  0x18u	/* e00de8f2 */
#define   MT7996_PHY_DFE_E330_BYTE0_INIT  0x1au	/* e00de90e; all bands */
/* +0xe338: the channel-set D-die RX calibration sets [0] below BW320 (bands
 * 0 and 1 here: 0 -> 1) and clears [1:0] at BW320 (band 2: 0, unchanged),
 * then read-modifies it once more (V:e00de592 / V:e00de3cc). Not decoded.
 * [static][measured] */
#define MT7996_PHY_DFE_E338(band)         MT7996_PHY_BN(band, 0xe338u)
#define   MT7996_PHY_DFE_E338_B0          HW_BIT(0)
#define   MT7996_PHY_DFE_E338_LO          HW_GENMASK(1, 0)

/* TPC_CTL_00: total TX power, latch. */
#define MT7996_PHY_TPC_CTL0(band)         MT7996_PHY_BN(band, 0xe400u)
#define   MT7996_PHY_TPC_CTL0_PWR_LATCH_MAN HW_BIT(31)
#define   MT7996_PHY_TPC_CTL0_PWR_LATCH   HW_BIT(30)
#define   MT7996_PHY_TPC_CTL0_HETB_MAN    HW_BIT(24)
#define   MT7996_PHY_TPC_CTL0_HETB        HW_GENMASK(23, 16)
#define   MT7996_PHY_TPC_CTL0_NON_HETB_MAN HW_BIT(8)
#define   MT7996_PHY_TPC_CTL0_NON_HETB    HW_GENMASK(7, 0)
/* TPC_CTL_01..07: 0x8308e404..1c (PHY2MAC forces, MU scaling, back-offs). */
#define MT7996_PHY_TPC_CTL(band, n)       MT7996_PHY_BN(band, 0xe400u + 4u * (n))
/* TPC_CTL (band, 9): every channel set clears [14], [13], [6:4] (table A's explicit
 * fields, one read-modify-write each; phy_chan.h). Roles unknown. [static][measured] */
#define   MT7996_PHY_TPC_CTL9_B14         HW_BIT(14)
#define   MT7996_PHY_TPC_CTL9_B13         HW_BIT(13)
#define   MT7996_PHY_TPC_CTL9_F6_4        HW_GENMASK(6, 4)
/* TPC_CTL_04: manual TX power. */
#define MT7996_PHY_TPC_CTL4(band)         MT7996_PHY_BN(band, 0xe410u)
#define   MT7996_PHY_TPC_CTL4_MINTXPWR_MAN HW_BIT(31)
#define   MT7996_PHY_TPC_CTL4_MINTXPWR    HW_BIT(30)
#define   MT7996_PHY_TPC_CTL4_TXPWR_MAN   HW_BIT(24)
#define   MT7996_PHY_TPC_CTL4_TXPWR       HW_GENMASK(23, 16)
#define   MT7996_PHY_TPC_CTL4_DBM_MAN     HW_BIT(8)
#define   MT7996_PHY_TPC_CTL4_DBM         HW_GENMASK(7, 0)

/* ========================================================================
 * TSSI measurement path of T2{4}, T2{5} and the TX gain-step calibration
 * T1{7,11,15}: the WF transmits the DFE test signal and its TSSI detector
 * measures it; the same words as the TX power meter of the TX LO / IQ
 * calibration T1{9,13,17}, whose search engine follows this section. Names
 * are [guess] unless cited.
 * The TX gain words BN +0xfd34 / +0xfd38 and the debug word WF_COMM +0xd414
 * that both items test first are in phy.h.
 * ======================================================================== */

/* WF_COMM +0xd9c0: TSSI measurement control (T2{4}, T2{5}) [name guess],
 * also the chain select of the TTG RX IQ engine (T1{18..23}, phy.h).
 * [31:28] chain one-hot (0x10000000 << chain; chain 4 is [16], set by
 * V:e0049472 in modes 4/5); [3] start: set, wait for RFINTF +0x73a0 [16] ==
 * 0, read +0x73b0, clear. The transmitter sends the DFE test signal while
 * [3] is set [inferred]. In the 430-word D-die backup list (mask bit 3).
 * T1{18..23}: [31:28] = 0, then [28 + chain] per configured chain (chains
 * 0..3) / [16] (chain 4), then [31:28] = 0xf; [27] = 1 while the per-bin
 * table (phy.h MT7996_PHY_IQBIN_*) is read or loaded. Observed 0xf10f0000.
 * T1{9,13,17} (mask bit 13 above): [31:28] = 1 << chain at setup ([27:0]
 * kept); starts of the TX LO / IQ engine (below): [0] LO run, [1] IQ A run,
 * [2] IQ B run: set, wait RFINTF +0x394 [3] == 0 (MT7996_PHY_TXCAL_STAT,
 * V:e00445fc, 100 ms), clear; TX power meter [8] mode 0 and [10] mode 1
 * (RFINTF +0x7300 below): set, wait RFINTF +0x394 [3:0] == 0, read +0x73b0,
 * clear. Observed 0x110f0000 with [0], [1], [2], [8] or [10] added.
 * [static, measured] */
#define MT7996_PHY_TSSI_MEAS_CTL(band)    MT7996_PHY_WFCOMM(band, 0xd9c0u)
#define   MT7996_PHY_TSSI_MEAS_WF_SEL     HW_GENMASK(31, 28)
#define   MT7996_PHY_TSSI_MEAS_IQBIN      HW_BIT(27)
#define   MT7996_PHY_TSSI_MEAS_WF4        HW_BIT(16)
#define   MT7996_PHY_TSSI_MEAS_PM1        HW_BIT(10)	/* TX power meter, mode 1 */
#define   MT7996_PHY_TSSI_MEAS_PM0        HW_BIT(8)	/* TX power meter, mode 0 */
#define   MT7996_PHY_TSSI_MEAS_TRIG       HW_BIT(3)
#define   MT7996_PHY_TSSI_MEAS_TXCAL_IQB  HW_BIT(2)	/* TX LO / IQ engine run [2] */
#define   MT7996_PHY_TSSI_MEAS_TXCAL_IQA  HW_BIT(1)	/* run [1] */
#define   MT7996_PHY_TSSI_MEAS_TXCAL_LO   HW_BIT(0)	/* run [0] */
/* WF_COMM +0xd9c4: TSSI measurement configuration. [27:24] detector range:
 * one step = 6 dB [inferred: T2{4} scores step i, measured at range r0 + i,
 * against the top step as 10 log10(P_ref / P_i) + 6 dB per range step]; a
 * higher value lowers the reading. T2{4} starts at 9, then searches 8 down
 * to steps + 1 for a reading > 0x400, or 10..12 for one < 0x1000 (else 13);
 * stock: 8 on the MT7976, 13 on the MT7977. T2{5} starts at 8 and moves it
 * (down to 2, up to 14) until the reading lies in 0x800..0x27ff. [15:0] =
 * 0x1008 (T2{5}). T1{7,11,15}: start 0xa; one reading; > 0x1feff -> 0xc and
 * read again; then +1 per >> 2 while the reading is >= 0x7000 (up to 0xe),
 * or -1 per << 2 while it is <= 0x1c00 (down to 2); stock 0xa on the MT7976
 * (readings 0x1dac..0x2e9b), 5..8 on the MT7977 (readings 0x18..0x421: the
 * MT7977 loopback signal is ~30 dB weaker). T1{9,13,17}: the anchor steps'
 * power search starts at 8 (LO phase) / 0xa MT7976, 8 MT7977 (IQ phase),
 * +3 after a reading > 0x1feff, -2 after one <= 0xfe, then moves it until
 * the reading lies in [0x1c00, 0x7000); the other steps use the anchor's
 * range +-1 (mask bit 13 above). [static, measured] */
#define MT7996_PHY_TSSI_MEAS_CFG(band)    MT7996_PHY_WFCOMM(band, 0xd9c4u)
#define   MT7996_PHY_TSSI_MEAS_RANGE      HW_GENMASK(27, 24)
#define   MT7996_PHY_TSSI_MEAS_CFG_LO     HW_GENMASK(15, 0)
/* WF_COMM +0xd824 [guess: test-signal shape]: [31:28] = 7, [27:20] = 0x3f
 * for the TSSI measurements; [17] set for T2{4}'s first step, cleared for
 * the others and before every re-measure. [static] In T1{7,11,15} [17]
 * marks the reference measurement: set for step 0 only; the D-die then
 * reports every later step relative to that one
 * (MT7996_PHY_TSSI_MEAS_STAT_CODE, phy.h MT7996_PHY_TXSTEP_CODES).
 * [static][measured] */
#define MT7996_PHY_TSSI_MEAS_D824(band)   MT7996_PHY_WFCOMM(band, 0xd824u)
#define   MT7996_PHY_TSSI_D824_HI         HW_GENMASK(31, 28)
#define   MT7996_PHY_TSSI_D824_MID        HW_GENMASK(27, 20)
#define   MT7996_PHY_TSSI_D824_FIRST      HW_BIT(17)
/* RFINTF +0x73a0: TSSI measurement status [name guess]; [16] busy (T2{4}/{5}
 * wait for 0, 100 ms deadline; the stock waits end after 20..23 reads;
 * T1{7,11,15} after 14..24). [7:0] measurement index: 0 for the reference
 * (D824 [17] set: the counter restarts), then s for step s (the range
 * reading before the reference also shows 0). [26:24] the measurement's
 * code, 3-bit two's complement (stock: 0, 1, 7 = -1, 6 = -2, always equal to
 * the code written into the step's MT7996_PHY_TXSTEP_CODES field; 7 after
 * the range reading, which has no reference; [31:27] never set) [measured].
 * RFINTF +0x73b0: TSSI measured power, linear [inferred] (observed
 * 0x141..0x53fe on the MT7976 chains, 0x434..0x1ffe0 on the MT7977 chains;
 * the firmware compares the whole word). Not read-only: T1{9,13,17} write
 * it 0 before every power-meter reading (V:e005322a; 7,756 writes in the
 * stock load); whether the write clears it is not established. Their meter
 * readings use [16:0] (MT7996_PHY_TXPM_RESULT; stock 4..0x1fd00).
 * [static, measured] */
#define MT7996_PHY_TSSI_MEAS_STAT(band, chain) MT7996_PHY_RFINTF(band, chain, 0x3a0u)
#define   MT7996_PHY_TSSI_MEAS_BUSY       HW_BIT(16)
#define   MT7996_PHY_TSSI_MEAS_STAT_IDX   HW_GENMASK(7, 0)
#define   MT7996_PHY_TSSI_MEAS_STAT_CODE  HW_GENMASK(26, 24)
#define MT7996_PHY_TSSI_MEAS_RESULT(band, chain) MT7996_PHY_RFINTF(band, chain, 0x3b0u)
#define   MT7996_PHY_TXPM_RESULT          HW_GENMASK(16, 0)	/* T1{9,13,17} meter reading */
/* RFINTF +0x7300 = 6, +0x7304 = 0 before the measurements (T2{4}, T2{5}).
 * T1{7,11,15} OR 6 into +0x7300 ([2:1] = 3) for the step-code measurements
 * and clear [2:1] after the steps [inferred: [2:1] enables the
 * reference-relative step codes], and clear +0x7304 [21:16] and [8:0]
 * before the steps. The TX IQ calibration (T1{8,12,16}, mode 0xc) sets
 * +0x7300 [2:0] = 7 and clears +0x7304 [22:16] and [8:0] and +0x7308
 * [15:0]. All three in the D-die backup list (mask bit 3). [static]
 * TX power meter of T1{9,13,17} (V:e0053194, the anchor steps' power search
 * V:e005332e; mask bit 13 above) [names guess]: the routine saves +0x7300,
 * +0x7304, +0x7308 and TSSI_MEAS_CTL, writes +0x73b0 = 0, then
 *   mode 0 (LO phase): +0x7300 [0] = 1; +0x7308 [7:0] = setting (0x20, 0xe0,
 *     0), [15:8] = 0; the reset pulse V:e0049416 (phy.h); TSSI_MEAS_CTL [8]
 *     pulse;
 *   mode 1 (IQ phase): +0x7300 [2:1] = 3 (|= 6, as T1{7,11,15}); +0x7304
 *     [8:0] = setting (0x20, 0x1e0, 0), [21:16] = 0; chain +0xd008 [7:6] =
 *     1, then 3; the reset pulse; TSSI_MEAS_CTL [10] pulse;
 * reading = +0x73b0 [16:0] [inferred: the power at the selected setting,
 * +32 / -32 / 0: a tone offset or bin]; the four saved words are written
 * back. Stock readings 4..0x1fd00, kept in [0x1c00, 0x7000) by the range
 * search. [static, measured] */
#define MT7996_PHY_RFINTF_7300(band, chain) MT7996_PHY_RFINTF(band, chain, 0x300u)
#define   MT7996_PHY_RFINTF_7300_STEPCODE HW_GENMASK(2, 1)	/* also TX power meter mode 1 */
#define   MT7996_PHY_RFINTF_7300_PM0      HW_BIT(0)	/* TX power meter mode 0 */
#define MT7996_PHY_RFINTF_7304(band, chain) MT7996_PHY_RFINTF(band, chain, 0x304u)
#define   MT7996_PHY_RFINTF_7304_A        HW_GENMASK(21, 16)
#define   MT7996_PHY_RFINTF_7304_B        HW_GENMASK(8, 0)	/* meter mode 1 setting */
#define MT7996_PHY_RFINTF_7308(band, chain) MT7996_PHY_RFINTF(band, chain, 0x308u)
#define   MT7996_PHY_RFINTF_7308_158      HW_GENMASK(15, 8)
#define   MT7996_PHY_RFINTF_7308_SET      HW_GENMASK(7, 0)	/* meter mode 0 setting */
#define MT7996_PHY_TXPM_WIN_LO            0x1c00u	/* range search: reading in [LO, HI) */
#define MT7996_PHY_TXPM_WIN_HI            0x7000u
#define MT7996_PHY_TXPM_SAT               0x1feffu	/* above: range + 3 */
#define MT7996_PHY_TXPM_LOW               0xfeu		/* at or below: range - 2 */
#define MT7996_PHY_TXPM_PEAK_HI           0xea60u	/* during the peak search: range + 1, reading / 4 */
/* RFINTF +0x7250: manual TX gain [name guess] (V:e0049a5a): [27] enable,
 * [26:20] the A-die TX gain index (the same table value the A-die PA gain
 * gets: MT7976 0x200 [6:0] from 0xe00715ec, MT7977 0x241 [27:21] from
 * 0xe0071a7c or 0xe007ff88). T1{7,11,15} (mask bit 11 above) differ on the
 * MT7977: [26:20] from a separate RF index table (0x3e..0x39) while 0x241
 * gets 0x2e..0x29; the MT7976 gets the PA table value on both. RFINTF +0x76b0
 * / +0x76b4 / +0x76b8 (phy.h MT7996_PHY_TXSTEP_CODES,
 * WM table item 8, phy_map.h): T2{4} and T2{5} save them first and write
 * them back after every TX step and at the end. [static] */
#define MT7996_PHY_TX_GAIN_MAN(band, chain) MT7996_PHY_RFINTF(band, chain, 0x250u)
#define   MT7996_PHY_TX_GAIN_MAN_EN       HW_BIT(27)
#define   MT7996_PHY_TX_GAIN_MAN_IDX      HW_GENMASK(26, 20)
/* BN +0xfc04 (band TSSI core, 0x8308fc04 + band << 20; not the chain-0
 * SX0_PARAM word 0x8300fc04, phy.h): [11:10] the TSSI-measured chain (T2{4},
 * T2{5}; the metered chain of T1{9,13,17}); [8] = 1 and [7:0] = a4 from
 * V:e004977e; DPD sets [21] and [23].
 * BN +0xff00 (MT7996_PHY_DPD_LUT_SEL, the DPD section) [9] = 1, [8] = 0
 * (T2{5}; T2{6}'s calibration reset with argument 2). BN +0xfc00 / +0xfc0c:
 * the band tone generator (T2{6} below; T2{5} writes +0xfc00 = 0xc2200100).
 * [static] */
#define MT7996_PHY_DFE_FC04(band)         MT7996_PHY_BN(band, 0xfc04u)
#define   MT7996_PHY_DFE_FC04_TSSI_CHAIN  HW_GENMASK(11, 10)

/* ========================================================================
 * TX LO / IQ calibration engine of T1{9,13,17} (mask bit 13 above): WF_COMM
 * +0xd800..+0xd81c, band words in the 430-word D-die backup list (restored
 * after every calibration), started by MT7996_PHY_TSSI_MEAS_CTL [0] / [1] /
 * [2]; status RFINTF +0x394; results in the item-9 rows (phy.h
 * MT7996_PHY_TXLO_*, MT7996_PHY_TXLOIQ_A / _B). Field roles are [guess] from
 * the three configurations the firmware writes; values [static, measured].
 * Codes are 9-bit two's complement.
 * ======================================================================== */

/* WF_COMM +0xd800: engine configuration [name guess]. LO runs: [17:16] = 0,
 * [15:14] = 1, [13:12] = 0, [11:4] = 0x3f, [3:0] = 7; IQ runs: [19:18] = 0,
 * [13:12] = 0, [11:4] = 0x7f, [3:0] = 7. Observed 0x00004ff9 -> 0x000043f7
 * (LO) -> 0x000047f7 (IQ). [static, measured] */
#define MT7996_PHY_TXCAL_CFG(band)        MT7996_PHY_WFCOMM(band, 0xd800u)
#define   MT7996_PHY_TXCAL_CFG_1918       HW_GENMASK(19, 18)
#define   MT7996_PHY_TXCAL_CFG_1716       HW_GENMASK(17, 16)
#define   MT7996_PHY_TXCAL_CFG_1514       HW_GENMASK(15, 14)
#define   MT7996_PHY_TXCAL_CFG_1312       HW_GENMASK(13, 12)
#define   MT7996_PHY_TXCAL_CFG_LEN        HW_GENMASK(11, 4)	/* 0x3f LO, 0x7f IQ [guess: averaging] */
#define   MT7996_PHY_TXCAL_CFG_30         HW_GENMASK(3, 0)	/* 7 */
/* WF_COMM +0xd804: engine control [name guess]. [31] = 1; [29:28] search
 * mode: 0 = full search from the default start, 1 = search from the start
 * code (the previous step's result minus 12 (LO) / 8 (IQ A)); [27:26] = 1
 * (LO, IQ B), 0 (IQ A); [24:16] start code A: LO 0x190 (-112) or prev I -
 * 12, IQ A 0x180 (-128) or prev A - 8, IQ B 0x1c8 (-56) or prev B - 8;
 * [14:12] / [10:8] / [6:4] / [2:0] [guess: iteration counts]: LO 7/3/7/3,
 * IQ A 4/4/4/4 (incremental: [10:8] = 1), IQ B 7/4/4/4 (incremental:
 * [14:12] = 1, mode 0). Observed 0x85907373 (LO full), 0x95f47373 (LO
 * incremental, start 0x1f4). [static, measured] */
#define MT7996_PHY_TXCAL_CTRL(band)       MT7996_PHY_WFCOMM(band, 0xd804u)
#define   MT7996_PHY_TXCAL_CTRL_EN        HW_BIT(31)
#define   MT7996_PHY_TXCAL_CTRL_MODE      HW_GENMASK(29, 28)
#define   MT7996_PHY_TXCAL_CTRL_SEL       HW_GENMASK(27, 26)
#define   MT7996_PHY_TXCAL_CTRL_START_A   HW_GENMASK(24, 16)
#define   MT7996_PHY_TXCAL_CTRL_N3        HW_GENMASK(14, 12)
#define   MT7996_PHY_TXCAL_CTRL_N2        HW_GENMASK(10, 8)
#define   MT7996_PHY_TXCAL_CTRL_N1        HW_GENMASK(6, 4)
#define   MT7996_PHY_TXCAL_CTRL_N0        HW_GENMASK(2, 0)
#define   MT7996_PHY_TXCAL_START_LO       0x190u	/* -112 */
#define   MT7996_PHY_TXCAL_START_IQA      0x180u	/* -128 */
#define   MT7996_PHY_TXCAL_START_IQB      0x1c8u	/* -56 */
/* WF_COMM +0xd808 / +0xd814 / +0xd818: per-run steps and offsets [guess],
 * [24:16] for code A, [8:0] for code B: +0xd808 LO 0x20 / 8, IQ A 0x40 /
 * 0x10, IQ B 0x10 / 4 [guess: coarse step]; +0xd814 LO 1 / 1, IQ A 4 / 1, IQ
 * B 1 / 1 [guess: fine step]; +0xd818 LO 0x1f4 / 0x1fc (-12 / -4), IQ A
 * 0x1e0 / 0x1f8 (-32 / -8), IQ B 0x1f8 / 0x1fe (-8 / -2) [guess: window
 * offsets]. [static, measured] */
#define MT7996_PHY_TXCAL_STEP0(band)      MT7996_PHY_WFCOMM(band, 0xd808u)
#define MT7996_PHY_TXCAL_STEP1(band)      MT7996_PHY_WFCOMM(band, 0xd814u)
#define MT7996_PHY_TXCAL_OFS(band)        MT7996_PHY_WFCOMM(band, 0xd818u)
#define   MT7996_PHY_TXCAL_A              HW_GENMASK(24, 16)
#define   MT7996_PHY_TXCAL_B              HW_GENMASK(8, 0)
/* WF_COMM +0xd81c: [24:16] = 0x1fe (LO, IQ A), 0 (IQ B) [guess: offset];
 * [8:0] start code B of the LO run (0x190, or prev Q - 12). Fields
 * MT7996_PHY_TXCAL_A / _B. [static, measured] */
#define MT7996_PHY_TXCAL_START_B(band)    MT7996_PHY_WFCOMM(band, 0xd81cu)
/* RFINTF +0x394: engine / meter status [name guess], per chain. Before a
 * run 0x2a or 0x3b, 0 when done; the firmware waits (V:e00445fc: three
 * matching reads 1 us apart, 100 ms silent deadline) for [3] == 0 after an
 * engine start (stock 670..2701 reads) and for [3:0] == 0 after a meter
 * start (14..54 reads). Never written [inferred: read-only].
 * [static, measured] */
#define MT7996_PHY_TXCAL_STAT(band, chain) MT7996_PHY_RFINTF(band, chain, 0x394u)
#define   MT7996_PHY_TXCAL_STAT_BUSY      HW_BIT(3)
#define   MT7996_PHY_TXCAL_STAT_PM_BUSY   HW_GENMASK(3, 0)
/* BN +0xfc30..+0xfc6c (16 words; +0xfcac, 32, as the channel set writes it
 * on an MT7977 band without EPA): table written by V:e0052c5c before the
 * engine runs [name guess: TX calibration per-step setting]; not read back
 * by the WM, not in the backup list (left as written). MT7976 (P[6] != 0,
 * group 0): 0x0a0b0c0d / 0x07070809 x 5, 0x03040e0f, 0x102, 0x03040506,
 * 0x102, 0x03040506, 0x102; MT7977 (group != 0, P[6] != 0): 0x03040505 /
 * 0x102 x 7, 0x090a0b0b, 0x06060708. The channel-set D-die RX calibration
 * writes the words with the same values on every channel set (MT7976 band
 * 0 V:e00dd69a.., MT7977 bands 1 and 2 V:e00dd9c4..V:e00dd646, code
 * immediates; 16 words, 32 on an MT7977 band without EPA; "Channel-set
 * D-die RX calibration" above). Its role is not
 * established.
 * [static, measured] */
#define MT7996_PHY_TXCAL_TBL(band, i)     MT7996_PHY_BN(band, 0xfc30u + 4u * (i))	/* i 0..31 */

/* ========================================================================
 * TX flatness (T2{6}): the per-chain TX FIR, the record table and the band
 * tone generator. A-die TX power detector: rfmodes.h.
 * ======================================================================== */

/* Chain +0xd030 [0]: TX flatness FIR enable [inferred]; T2{6} clears it on
 * chains 0..3, loads the identity into the WF's taps and sets it again
 * (observed 0xd1). The D-die chain routing of the T2 preparation
 * (V:e0049472) also clears it, on every chain in the TTG RX calibrations
 * (T1{18..23}: 0xd1 -> 0xd0). [static, measured] */
#define MT7996_PHY_TXFLAT_CTRL(band, chain)   MT7996_PHY_CHAIN_REG(band, chain, 0xd030u)
#define   MT7996_PHY_TXFLAT_CTRL_EN           HW_BIT(0)
/* Chain +0xd034..+0xd044: taps h0..h8 of the symmetric 17-tap TX flatness
 * FIR, 11-bit two's complement, +0xd034 + 4 (i >> 1) [10:0] (i even) /
 * [26:16] (i odd); h8 = centre (+0xd044 [10:0]), 0x200 = unity (Q9); clamped
 * +-0x3ff. Result of T2{6} (restore flag 0x100000); the WM table item 20
 * window (phy_map.h), whose default is the identity. [static, measured] */
#define MT7996_PHY_TXFLAT_TAPS(band, chain, n) MT7996_PHY_CHAIN_REG(band, chain, 0xd034u + 4u * (n))	/* n 0..4 */
#define   MT7996_PHY_TXFLAT_TAP_EVEN          HW_GENMASK(10, 0)
#define   MT7996_PHY_TXFLAT_TAP_ODD           HW_GENMASK(26, 16)
#define   MT7996_PHY_TXFLAT_UNITY             0x200u
/* T2{6} record, in band 0's WF_COMM block for every band (no band << 20).
 * [31:30] P[8], [29:20] TX gain found (0x390 MT7976 / 0x3c0 MT7977, stock),
 * [19:12] RF gain (0x29), [11:0] detector slope of one 8-code (1 dB) TX-gain
 * step, OR'ed unmasked (MT7976).
 * HAZARD: entry (0, 0) is DPD_STATE(0), which T2{0}, T2{7}, the result save
 * and the restore orchestration read; WF_COMM +0xd760 means two things by
 * band and chain. [static, measured] */
#define MT7996_PHY_TXFLAT_REC(band, chain)    (0x830ad760u + 0x10u * (band) + 4u * (chain))
#define   MT7996_PHY_TXFLAT_REC_P8            HW_GENMASK(31, 30)
#define   MT7996_PHY_TXFLAT_REC_TXGAIN        HW_GENMASK(29, 20)
#define   MT7996_PHY_TXFLAT_REC_RFGAIN        HW_GENMASK(19, 12)
#define   MT7996_PHY_TXFLAT_REC_SLOPE         HW_GENMASK(11, 0)
/* BN +0xfc00: single-tone TX generator of the band TSSI core [inferred].
 * [31] on [guess], [30] start [guess], [25] = 1, [21] flag, [19:16] bin class
 * (3: BW20/160, 4: BW320 [inferred: 5 / 10 MHz per bin]; 0 for marker
 * tones), [15:8] bin modulo 0x60. [static, measured] */
#define MT7996_PHY_TONEGEN(band)              MT7996_PHY_BN(band, 0xfc00u)
#define   MT7996_PHY_TONEGEN_ON               HW_BIT(31)
#define   MT7996_PHY_TONEGEN_GO               HW_BIT(30)
#define   MT7996_PHY_TONEGEN_FLAG             HW_BIT(21)
#define   MT7996_PHY_TONEGEN_CLASS            HW_GENMASK(19, 16)
#define   MT7996_PHY_TONEGEN_BIN              HW_GENMASK(15, 8)
/* BN +0xfc0c: setup 0x3420 ([13], [12] set and [5:4] = 1 or 2 by mode,
 * V:e004977e), [0] load pulse [inferred] (V:e0048b8a sets it, and the twin
 * 0x8309fc0c's, in modes 4/5, e.g. in the TTG RX calibrations), [11:8] measurement BW class (0 BW20 row 0, 1
 * rows 1-2, 2 rows 3-6, 3 rows 7-8 BW160, 4 row 9 BW320). BN +0xfc08 = 0.
 * [static] */
#define MT7996_PHY_TONEGEN_CTL(band)          MT7996_PHY_BN(band, 0xfc0cu)
#define   MT7996_PHY_TONEGEN_CTL_LOAD         HW_BIT(0)
#define   MT7996_PHY_TONEGEN_CTL_BWCLS        HW_GENMASK(11, 8)
/* During T2{6} (all restored): BN +0xe764 TPC_MCS_CTL_00 [9:8] = 2; WF_COMM
 * +0xd4a0 [20] = 1; A-die TOP 0x4ac [17] = 1; chain +0x87ac [31] = 1, [30] =
 * 0 (MT7977); chain +0xd008 (phy.h) [7:6] = 3 on the measured chain, 1 on
 * the others; BN +0xfd34 [25:16] = a 10-bit TX gain code (0x390..0x3f0;
 * [25:24] read back 0 [measured]). T2{6} reads 0x8301fc10 (no band term)
 * [0], the calibration init's copy of EEPROM 0x1ad [5]
 * (MT7996_PHY_FW_1FC10 below); 0x8302fc10 + band << 20 [0]: MT7976 flag
 * written with P[6] = 1 and read back. Storage words [guess]. [static] */

/* ========================================================================
 * OTFIQ (T2{8}): configuration words, result slots, the band test-signal
 * generator and the DSP lock. The DSP command word is in dsp.h
 * (MT7996_DSP_CTRL_CMD), the DFE capture engine in dfe.h, the A-die words in
 * rfmodes.h.
 * ======================================================================== */

/* T2{8} configuration, WF_COMM +0xd6e8 / +0xd6ec / +0xd6f0 / +0xd704
 * (+band << 20): the WM writes 38 default fields on every call unless CFG0
 * [22]; no other WM writer. Defaults 0x82220096, 0x1cf266e0, 0xbfe74054, 0.
 * CFG0 RFINTF_CLR: per step of the TX gain sweep, row group 6 - step of the
 * TX IQ rows (phy.h MT7996_PHY_TXIQC): words 0..6 [13:0] and [29:16] = 0
 * except word 3 [29:16] = 0x1000, word 7 [13:0] = 0 (not in the stock
 * load).
 * Long-path gate: CFG0 EN, then BW320 with CFG0 BW320 and HI and P[0xc] >
 * 5954999 kHz, or BW160 with BW160 and HI above that frequency / BW160 and
 * LO at or below it; and CFG1 WF_SKIP clear for the WF. Fields marked
 * "cmd" go to the DSP command word. [static][measured] */
#define MT7996_PHY_OTFIQ_CFG0(band)       MT7996_PHY_WFCOMM(band, 0xd6e8u)
#define   MT7996_PHY_OTFIQ_CFG0_EN        HW_BIT(31)
#define   MT7996_PHY_OTFIQ_CFG0_STEP_B29  HW_BIT(30)	/* -> cmd [29], step mode */
#define   MT7996_PHY_OTFIQ_CFG0_RFINTF_CLR HW_BIT(28)	/* reset TX IQ row group 6 - step (above) */
#define   MT7996_PHY_OTFIQ_CFG0_NO_TOP    HW_BIT(26)	/* skip TOP 0x4a8 */
#define   MT7996_PHY_OTFIQ_CFG0_BW320     HW_BIT(25)
#define   MT7996_PHY_OTFIQ_CFG0_BW160     HW_BIT(24)
#define   MT7996_PHY_OTFIQ_CFG0_KEEP      HW_BIT(22)	/* WM keeps the words as found */
#define   MT7996_PHY_OTFIQ_CFG0_HI        HW_BIT(21)	/* freq > 5954999 kHz */
#define   MT7996_PHY_OTFIQ_CFG0_LO        HW_BIT(20)	/* BW160, freq <= 5954999 kHz */
#define   MT7996_PHY_OTFIQ_CFG0_FCB_CLR   HW_BIT(19)	/* BN +0xfcb0/+0xfcb4 = 0 */
#define   MT7996_PHY_OTFIQ_CFG0_SWEEP     HW_BIT(18)	/* gain sweep + RFINTF fold */
#define   MT7996_PHY_OTFIQ_CFG0_BURST     HW_BIT(17)	/* one run of COUNT vs COUNT runs */
#define   MT7996_PHY_OTFIQ_CFG0_COUNT     HW_GENMASK(11, 0)	/* WM writes [12:0] = 0x96 */
/* CFG1: the step loop runs STEP_BEG..STEP_END (both = CFG2 STEP [2:0]
 * unless CFG0 SWEEP). DB14 fields go to WF_COMM +0xdb14; at BW160 the WM
 * writes them minus 1 without masking, so a zero field corrupts +0xdb14. */
#define MT7996_PHY_OTFIQ_CFG1(band)       MT7996_PHY_WFCOMM(band, 0xd6ecu)
#define   MT7996_PHY_OTFIQ_CFG1_STBY      HW_BIT(28)	/* other chains + k4 to manual standby */
#define   MT7996_PHY_OTFIQ_CFG1_CMD23     HW_BIT(27)	/* -> cmd [23] */
#define   MT7996_PHY_OTFIQ_CFG1_KEEP_SLOT HW_BIT(26)	/* sweep mode */
#define   MT7996_PHY_OTFIQ_CFG1_SEL       HW_GENMASK(25, 24)	/* -> cmd [22:21]; 3: alt SG words */
#define   MT7996_PHY_OTFIQ_CFG1_CMD20_19  HW_GENMASK(23, 22)	/* -> cmd [20:19] */
#define   MT7996_PHY_OTFIQ_CFG1_STEP_END  HW_GENMASK(21, 19)
#define   MT7996_PHY_OTFIQ_CFG1_STEP_BEG  HW_GENMASK(18, 16)
#define   MT7996_PHY_OTFIQ_CFG1_DB14_9_6  HW_GENMASK(15, 12)	/* -> +0xdb14 [9:6] */
#define   MT7996_PHY_OTFIQ_CFG1_DB14_13_10 HW_GENMASK(11, 8)	/* -> +0xdb14 [13:10] */
#define   MT7996_PHY_OTFIQ_CFG1_CMD27_24  HW_GENMASK(7, 4)	/* -> cmd [27:24] */
#define   MT7996_PHY_OTFIQ_CFG1_WF_SKIP   HW_GENMASK(3, 0)
/* CFG2. VALID clear: slot 5, step 4, FD38_EN with FD38 = 0x3e7, NO_FINE
 * clear. NO_FINE clear: TSSI_CORE3 OFFSET_MAN_EN = 1 with MAN = 0, TSSI
 * FINE_OFFSET_CTRL_00 (+0x048) [31] = 1 and [23:16] = 0, PWR_CVT_OFST_00
 * (+0x214) [24:16] = 0. FD38_EN: BN +0xfd38 (phy.h) [12] = 1, [9:0] = FD38.
 * PRERUN: one single-count DSP run before the counted runs. A241_LO /
 * A241_HI: A-die 0x241 [11:1] / [20:12] from tables 0xe0069200 /
 * 0xe0069218 indexed by CFG3 [15:0] / [31:16] (an index >= 6 reads past the
 * 6-word table), with 0x240 [28] / [29] set.
 * SLOTSEL: the CHAIN_D000 +0x600 / +0x604 select fields. SLOT also picks
 * the result slot copied to all seven. */
#define MT7996_PHY_OTFIQ_CFG2(band)       MT7996_PHY_WFCOMM(band, 0xd6f0u)
#define   MT7996_PHY_OTFIQ_CFG2_VALID     HW_BIT(31)	/* else slot 5, step 4, FD38 0x3e7 */
#define   MT7996_PHY_OTFIQ_CFG2_NO_FINE   HW_BIT(30)
#define   MT7996_PHY_OTFIQ_CFG2_FD38_EN   HW_BIT(29)
#define   MT7996_PHY_OTFIQ_CFG2_PRERUN    HW_BIT(28)
#define   MT7996_PHY_OTFIQ_CFG2_A241_HI   HW_BIT(27)
#define   MT7996_PHY_OTFIQ_CFG2_A241_LO   HW_BIT(26)
#define   MT7996_PHY_OTFIQ_CFG2_FD38      HW_GENMASK(25, 16)
#define   MT7996_PHY_OTFIQ_CFG2_SLOTSEL   HW_GENMASK(15, 12)
#define   MT7996_PHY_OTFIQ_CFG2_SLOT      HW_GENMASK(8, 4)	/* TX_GC_IDX = STEP + 8 * SLOT */
#define   MT7996_PHY_OTFIQ_CFG2_STEP      HW_GENMASK(3, 0)
#define MT7996_PHY_OTFIQ_CFG3(band)       MT7996_PHY_WFCOMM(band, 0xd704u)	/* [31:16]/[15:0] table idx */
/* CHAIN_D000 +0x3a0: [17][16][1][0] set by the T2{8} long path and left set;
 * [16] also by T2{4}'s V:e0049472 (modes 0, 1, 20, 23); the TX IQ
 * calibration (T1{8,12,16}) clears [16] for the tap estimate and sets it for
 * the image score (D-die backup list); the TX LO / IQ calibration
 * (T1{9,13,17}) clears it for the LO phase and sets it for the IQ phase (the
 * MT7977 also clears [0], [1], [17] in both). The short path clears [1],
 * [0], [17]. [static][measured] */
#define MT7996_PHY_CHAIN_D3A0(band, chain) MT7996_PHY_CHAIN_REG(band, chain, 0xd3a0u)
/* CHAIN_D000 +0x600..+0x77c: T2{8} slot select (+0x600 6 x 5 bit, +0x604
 * 3 x 5 bit, all = SLOTSEL; +0x608 = 0x07654321, +0x60c = 0) and 7 result
 * slots of 11 words at +0x610 + (n ? 0x2c n + 0x38 : 0); words 1..10 = 3
 * signed 10-bit fields [29:20][19:10][9:0]. [static][measured] */
#define MT7996_PHY_OTFIQ_SLOT(band, chain, n) \
	MT7996_PHY_CHAIN_REG(band, chain, 0xd610u + ((n) ? 0x2cu * (n) + 0x38u : 0u))	/* n 0..6 */
/* BN +0xa004: test-signal generator start, (n + 0x30400000) << 2 (BW160:
 * (n + 0x60800000) << 1), then 0; +0xa008..+0xa054 configuration (whole
 * words per bandwidth; T2{8} writes up to +0xa050); BN +0xb004 =
 * 0x30000000, BN +0xb000 [31:30] = 3. DPD's TX power sweep starts it with
 * 0xc1000000 and its own configuration words (the DPD section).
 * [static][inferred role] */
#define MT7996_PHY_SIGGEN_START(band)     MT7996_PHY_BN(band, 0xa004u)
#define MT7996_PHY_SIGGEN_CFG(band, n)    MT7996_PHY_BN(band, 0xa008u + 4u * (n))	/* n 0..19 */
/* WF_COMM +0xd6c4: DSP lock word, bit per owner ([0]: the calibration take
 * V:e00dfa56). The take (shared with the WM's other DSP users) saves and
 * zeroes BN +0x0d40, BN +0xc1a0 and WF_COMM +0x0260, writes the DSP trigger
 * enable 0x8308ef08 (dsp.h) = 2, then sets [0]; the release clears [0] and
 * writes the four words back. [static] */
#define MT7996_PHY_DSP_LOCK(band)         MT7996_PHY_WFCOMM(band, 0xd6c4u)
#define   MT7996_PHY_DSP_LOCK_CAL         HW_BIT(0)
/* Also [static], roles unknown (values in the T2[8] decode): WF_COMM
 * +0xdb00..+0xdb30, +0xdd28/38/44, +0xc0fc, +0xd49c [31], +0xd4a8 [15]; BN
 * +0xe33c..+0xe344; BN +0xfa00 / +0xfa0c (chain select: +0xfa0c [10:8] =
 * WF, +0xfa00 [0] enable); MT7977 A-die 0x03a (k0..k4 = 0x1003), 0x14b (k4
 * = 0x20), 0x920 (= 0x10014241), 0x240 [29:28] and 0x241 [20:12] / [11:1]
 * (tables 0xe0069200 / 0xe0069218; rfmodes.h). */

/* ========================================================================
 * DPD (T2{0..3}): the LUT RAM, the DPD training engine (WF_COMM
 * +0xe000..+0xe0f0) and the words the steps program. Write order and values
 * are in the T2{0..3} decode (dpd.py capture_cfg). The A-die side is in
 * rfmodes.h. Names [guess] unless cited; [static, measured] unless marked.
 * ======================================================================== */

/* DPD LUT n of a chain: 0x8300a000 + band << 20 + chain << 16 + n << 8, n
 * 0..9 (the step index; the stock load trains n 0..7). Entries: 32 for n
 * 0..2 ("early" steps), 16 for n 3..7. Each word: [8:0] signed amplitude
 * term, [26:16] signed phase term [inferred from the WM checks: the last
 * entry's [8:0] equals the AGC target 0x40 (unity), |[8:0]| <= 48 and
 * |[26:16]| <= 56 for the first 14 (7) entries]. Filled by the training
 * engine; the WM writes it only to clear it (the AGC's flat start: entries
 * 0 except the last two = the target T) and to restore it. Stock: e.g. band
 * 0 chain 0 LUT 0 ends 0x00010034, 0x00020040. */
#define MT7996_PHY_DPD_LUT(band, chain, n)  MT7996_PHY_CHAIN_REG(band, chain, 0xa000u + ((n) << 8))
#define   MT7996_PHY_DPD_LUT_AMP            HW_GENMASK(8, 0)	/* signed */
#define   MT7996_PHY_DPD_LUT_PHASE          HW_GENMASK(26, 16)	/* signed */
#define   MT7996_PHY_DPD_LUT_N              8u
#define   MT7996_PHY_DPD_LUT_LEN(n)         ((n) < 3 ? 32u : 16u)
/* LUT gain word of LUT n: [9:0] = the TX gain the LUT was trained at, mod
 * 0x400:
 *   16 (code & 7) - DFE_COMP(code >> 3) + FD38 - TSSI_TERM (if
 *   MT7996_PHY_TSSI_DPD_SHIFT_RM) - 4 * MT7996_PHY_TSSI_BACKOFF_VAL (if
 *   MT7996_PHY_TSSI_BACKOFF_ENB) + offset table byte (0 in the stock
 *   tables),
 * code = the A-die TX gain code, FD38 = MT7996_PHY_DFE_FD38_VAL (signed),
 * DFE_COMP = MT7996_PHY_DFE_GAIN_COMP below, TSSI_TERM = a signed byte of
 * the TSSI PWR_DNL_GT<k>_OFST words (MT7996_PHY_TSSI_DNL_OFST, T2{4}'s
 * result; V:e005c8ce). Stock 0x32b..0x3de. The LUT select at transmit time
 * is [inferred] to compare these with the live TX gain. */
#define MT7996_PHY_DPD_LUT_GAIN(band, chain, n) MT7996_PHY_CHAIN_REG(band, chain, 0xaf20u + 4u * (n))
#define   MT7996_PHY_DPD_LUT_GAIN_VAL       HW_GENMASK(9, 0)
/* Chain +0xaf00 / +0xaf04 / +0xaf08: DPD path control [guess]. Per step:
 * +0xaf04 [25] = 1; +0xaf00 [9], [24], [8], [16] = 0; +0xaf04 [24] = 1,
 * [19:10] = 0, [29] = 1, [31] = 1; +0xaf08 [29], [25], [31], [27] = 1. In
 * the D-die backup list (restored). The TTG RX calibrations (T1{18..23})
 * set +0xaf04 [29], [28], [31], [30] on every chain, in that order
 * (observed 0 -> 0xf0000000). */
#define MT7996_PHY_DPD_PATH0(band, chain)   MT7996_PHY_CHAIN_REG(band, chain, 0xaf00u)
#define MT7996_PHY_DPD_PATH1(band, chain)   MT7996_PHY_CHAIN_REG(band, chain, 0xaf04u)
#define MT7996_PHY_DPD_PATH2(band, chain)   MT7996_PHY_CHAIN_REG(band, chain, 0xaf08u)
/* BN +0xff00: [10] = 1, [3:0] = the LUT the engine trains (V:e004903c); [9]
 * = 1, [8] = 0 (calibration reset mode 2, also T2{5} and T2{6}). BN
 * +0xff44: [2:0] = 7, [9:3] = 0. Restored. */
#define MT7996_PHY_DPD_LUT_SEL(band)        MT7996_PHY_BN(band, 0xff00u)
#define   MT7996_PHY_DPD_LUT_SEL_EN         HW_BIT(10)
#define   MT7996_PHY_DPD_LUT_SEL_IDX        HW_GENMASK(3, 0)
#define MT7996_PHY_DPD_FF44(band)           MT7996_PHY_BN(band, 0xff44u)

/* DPD training engine, WF_COMM +0xe000 + band << 20 ("MemDPD"/"LDPD" engine
 * [guess]; not dfe.h's DFE calibration capture engine at +0xd594, which the
 * stock DPD does not touch). All in the D-die backup list except +0xe018. */
/* +0xe000 CTRL: [0] run (set, wait +0xe024 [31], clear); [23] correlation
 * path on, [24] latch (set, read +0xe028 / +0xe02c, clear); [22:16] = 0x0b
 * per step and 0 during the AGC; [15:12] = 2; [11] = 1; [10:8], [7:6], [5:2]
 * = 0; [28:25] = 2. */
#define MT7996_PHY_DPD_CTRL(band)           MT7996_PHY_WFCOMM(band, 0xe000u)
#define   MT7996_PHY_DPD_CTRL_RUN           HW_BIT(0)
#define   MT7996_PHY_DPD_CTRL_MEAS          HW_BIT(23)
#define   MT7996_PHY_DPD_CTRL_LATCH         HW_BIT(24)
#define   MT7996_PHY_DPD_CTRL_22_16         HW_GENMASK(22, 16)
/* +0xe004: [24:20] = [29:25] = 0xd (steps 0..2) / 6; [24:20] is also the
 * AGC reference's accumulation select and the first LUT entry the
 * compression check counts; [19:10], [9:0] = 0. */
#define MT7996_PHY_DPD_CFG1(band)           MT7996_PHY_WFCOMM(band, 0xe004u)
#define   MT7996_PHY_DPD_CFG1_START         HW_GENMASK(24, 20)
#define   MT7996_PHY_DPD_CFG1_29_25         HW_GENMASK(29, 25)
/* +0xe008: [31] = 1, [30] = (step < 3), [29:28] = 0, [23] = 0, [22:18] =
 * coefficient select (table 0xe0080a70 / 0xe0080aa0: 6 MT7976, 3 or 2
 * MT7977), [17] = 1, [10:8] = 0, [7:0] = 0. */
#define MT7996_PHY_DPD_CFG2(band)           MT7996_PHY_WFCOMM(band, 0xe008u)
#define   MT7996_PHY_DPD_CFG2_COEF          HW_GENMASK(22, 18)
#define   MT7996_PHY_DPD_CFG2_EARLY         HW_BIT(30)
/* +0xe00c: [24:16] amplitude target T (0x80 per step, then the AGC's table
 * value 0x20..0x60; lowered by 0x20 on TX-gain saturation, >= 0x20), [15:8]
 * = 1, [7:0] = 0x40. */
#define MT7996_PHY_DPD_TARGET(band)         MT7996_PHY_WFCOMM(band, 0xe00cu)
#define   MT7996_PHY_DPD_TARGET_T           HW_GENMASK(24, 16)
/* +0xe010: [4:0] = coefficient select (as +0xe008 [22:18]); [16] = 0 per
 * step, 1 during the AGC; [18] = 1 (LUT select); [8], [31], [30:29] = 0. */
#define MT7996_PHY_DPD_CFG4(band)           MT7996_PHY_WFCOMM(band, 0xe010u)
/* +0xe014: [21] = 1, [20:8] = 0x1ff. */
#define MT7996_PHY_DPD_CFG5(band)           MT7996_PHY_WFCOMM(band, 0xe014u)
/* +0xe018: [7:0] correlation length - 1 (0x0d / 0x1f / 0x0f). Not restored:
 * left 0x0f. */
#define MT7996_PHY_DPD_MEAS_LEN(band)       MT7996_PHY_WFCOMM(band, 0xe018u)
/* +0xe024 [31]: run done (100 ms deadline; stock 147..296 reads for an AGC
 * capture, 1516..3076 for a training run). +0xe028 / +0xe02c [21:0]:
 * complex correlation I / Q, signed (the WM takes its CORDIC magnitude,
 * V:e005b530). Read-only. */
#define MT7996_PHY_DPD_DONE(band)           MT7996_PHY_WFCOMM(band, 0xe024u)
#define   MT7996_PHY_DPD_DONE_FLAG          HW_BIT(31)
#define MT7996_PHY_DPD_CORR_I(band)         MT7996_PHY_WFCOMM(band, 0xe028u)
#define MT7996_PHY_DPD_CORR_Q(band)         MT7996_PHY_WFCOMM(band, 0xe02cu)
#define   MT7996_PHY_DPD_CORR_VAL           HW_GENMASK(21, 0)	/* signed */
/* +0xe0b0: [31] = 1, [23:20] = LUT index (byte table 0xe0080448[step]),
 * [17:14] = byte 0xe0080ae0[..], [13] = 1, [12:11] = 0, [10] = 1 unless the
 * amplitude-bin table was loaded this step, [9:8] = 0, [7:6] = 2, [1:0] = 1.
 * +0xe0b4 / +0xe0b8: four 5-bit fields ([4:0], [12:8], [20:16], [28:24]) =
 * 2. +0xe0bc..+0xe0c8: the amplitude-bin table [inferred], written on some
 * steps only (values at phy.h MT7996_PHY_MASK_COEFF). +0xe0e0: [31] = 1,
 * [22:0] = 0x7fffff. +0xe0ec: [16:12] = 0, then [11] = 1, [10:0] = 0, then
 * [16] = 1. +0xe0f0: [31] = 1, [2:0] = 7, [9:3] = 0. */
#define MT7996_PHY_DPD_CFG_B0(band)         MT7996_PHY_WFCOMM(band, 0xe0b0u)
#define   MT7996_PHY_DPD_CFG_B0_LUT         HW_GENMASK(23, 20)
#define MT7996_PHY_DPD_CFG_B4(band)         MT7996_PHY_WFCOMM(band, 0xe0b4u)
#define MT7996_PHY_DPD_CFG_B8(band)         MT7996_PHY_WFCOMM(band, 0xe0b8u)
#define MT7996_PHY_DPD_CFG_E0(band)         MT7996_PHY_WFCOMM(band, 0xe0e0u)
#define MT7996_PHY_DPD_CFG_EC(band)         MT7996_PHY_WFCOMM(band, 0xe0ecu)
#define MT7996_PHY_DPD_CFG_F0(band)         MT7996_PHY_WFCOMM(band, 0xe0f0u)
/* Other words a step writes (all restored): WF_COMM +0xda14 [3:0] = 0;
 * +0xd4c0 [31] = 1, [27:24] = 1; +0xd49c [2:0] = mode code (2 -> 5, 4 -> 3,
 * 0xc -> 0, 0x13 -> 1; V:e0049b40). BN +0xf11c [6:4] = 4; +0xf120 [30] = 1;
 * +0xf124 [31] = 1, [30:16] = 0x0804; BN +0xb024 [6:4] = 1; BN +0xb010
 * [31:28] = 1; BN +0xfc00 (MT7996_PHY_TONEGEN) [30] = 1; BN +0xfc04
 * (MT7996_PHY_DFE_FC04) [21], [23] = 1; BN +0xfc0c (MT7996_PHY_TONEGEN_CTL)
 * [0] cleared then set. Chain +0xd000 [22] = 1, [10:8] = 7; +0xd004 [25] =
 * 1, [6:4] = 3; +0xd010 [27] = 1, [21] = 0; +0xd008 [12] = 0; +0xd048 [0] =
 * 1; +0xd04c [9] = 1, [2] = 0, [8:3] = byte 0xe0080ae1[..] (2 MT7976, 1
 * MT7977); +0xd050 [21:16] = 6; RFINTF +0x7290 [9] = 1, [8] = 0; TSSI
 * +0x25c (MT7996_PHY_TSSI_DFE_BACKOFF1) [28] BACKOFF_MAN = 1. */

/* BN +0xfd38 (phy.h MT7996_PHY_DFE_FD38) in DPD: start 0x3d0 = -48; DPD steps
 * the A-die gain index (0x10 units each) to keep it within [(0x80 - T) / 4
 * - 0x30, (0x80 - T) / 4 - 0x10] as far as the code table allows, and clips
 * it at the upper bound and at -0x80. BN +0xfdf0 + 4 (i >> 1), i <= 15: DFE
 * gain compensation of A-die gain group i, [25:16] (i even) / [9:0] (i odd)
 * (stock 0xb0 for i = 5 on band 0). DPD only reads it; its writer is the
 * channel set: PHY table A writes six of the words whole by its key's
 * column (+0xfdf0..+0xfdfc, +0xfe08, +0xfe0c: rows MT7996_PHY_DFE_GAIN_COMP(0,
 * i) with i 0, 2, 4, 6, 12, 14; phy_chan.h), then table A's explicit fields
 * entries 5, 4, 3, 2 (phy_chan.h MT7996_CHSET_GCOMP). */
#define MT7996_PHY_DFE_GAIN_COMP(band, i)   MT7996_PHY_BN(band, 0xfdf0u + 4u * ((i) >> 1))
#define   MT7996_PHY_DFE_GAIN_COMP_EVEN     HW_GENMASK(25, 16)
#define   MT7996_PHY_DFE_GAIN_COMP_ODD      HW_GENMASK(9, 0)

/* DPD TX power sweep (V:e004e3e4..), on the band test-signal generator per
 * bandwidth code k: MT7996_PHY_SIGGEN_CFG n 0 (+0xa008) = 0x012c4000 (k <
 * 4) / 0x0120f400 (k = 4) + word 0xe0069438[chain] (0, 1, 3, 9), then
 * [10:8] = k (bandwidth code 0..4); +0xa00c = 0x7f; +0xa010 = 0; k < 4:
 * +0xa014 = 0x401, +0xa018 = 0x0020006c, +0xa01c = 0x00020f00, +0xa040 = 7,
 * +0xa048 = 0x40d, +0xa04c = 2; k = 4: +0xa014 = 0x20d00402, +0xa018 =
 * 0x20045045, +0xa01c = 0x0480082e, +0xa038 = 0xa0, +0xa040 = 0x018b008d,
 * +0xa048 = 0x4306, +0xa04c = 0x3ac, BN +0xb004 = 0x30000000; the other
 * words 0; MT7996_PHY_SIGGEN_START = 0xc1000000 starts it, 0 stops it. Not
 * restored except +0xa004. Per k the PHY selector programmer (V:e0048a70)
 * writes BN_MODE [27:24] and RXFE_CTRL [7:4]; only BN_MODE is restored.
 * During the sweep (restored): the chain's TSSI FINE_OFFSET_CTRL_00 (+0x048)
 * [31] = 1, [23:16] = 0; MT7996_PHY_TSSI_CORE3 OFFSET_MAN_EN = 1, OFFSET_MAN
 * = 0; BN +0xb000 |= 0xcc000000; BN_MODE [31:28] = 8 (group 0) / 9 (group
 * <= 8) / 0xa; BN +0xfd38 = 0x13d0; MT7996_PHY_TX_GAIN_MAN EN with IDX = the
 * TX gain code; A-die 0x200 / 0x240 / 0x241 / 0x4a0 (rfmodes.h). */

/* WM scratch words in the PHY space read by DPD (no hardware role; stock
 * values): 0x8301fc00 + B: per-step-class bits (byte map 0xe006a330) that
 * freeze the TX gain (0); band 0's word is also the trigger every channel
 * set reads (MT7996_PHY_CHSET_TRIG_*, HAZARD); 0x8301fc0c + B [31 - band]:
 * post sweep enable = EEPROM image byte 0x199 + band [5] (V:e00e1316, run
 * by the PHY-init routine before the calibration request, ORs [31 - i] =
 * EEPROM 0x199 + i [5] and [28 - i] = [6], i = 0..2, into all three bands'
 * words, then EEPROM 0x608 + i << 8 i (MT7996_PHY_EE_DPD_SWEEP2: [7:0],
 * [15:8], [23:16]; 0 on this unit); 0xe0000000 stock); 0x8301fc14 + B: manual
 * A-die / D-die bandwidth and table-index overrides, used with the WF_COMM
 * +0xd414 key 0xbb1 (phy.h MT7996_PHY_RFIF_D414) only; 0x8301fc18 + B:
 * per-step target override (key + [8]); 0x8301fc1c / 0x8301fc20 + B:
 * per-step LUT-gain offset bytes (key + [9]; 0, 1 stock); WF_COMM +0xd7a8:
 * 1 forces the E1 tables, 2 the E2 tables (0); WF_COMM +0xd430: the
 * per-band PHY init writes 0x395832a4 on every band (V:e00de674) before any
 * calibration, and DPD writes 0x88888888 when the live A-die TX gain code is
 * not in the step's code table (not in the stock load). B = band << 20.
 * [static, measured values] */
#define MT7996_PHY_DPD_SCRATCH_FREEZE(band) (0x8301fc00u + ((band) << 20))
#define MT7996_PHY_DPD_SCRATCH_SWEEP(band)  (0x8301fc0cu + ((band) << 20))
#define   MT7996_PHY_DPD_SWEEP_EN(band)     HW_BIT(31u - (band))
#define MT7996_PHY_EE_DPD_SWEEP2            0x608u	/* host EEPROM image, 3 bytes -> [7:0] [15:8] [23:16] */
#define MT7996_PHY_DPD_FORCE_TBL(band)      MT7996_PHY_WFCOMM(band, 0xd7a8u)
#define MT7996_PHY_DPD_GAIN_ERR(band)       MT7996_PHY_WFCOMM(band, 0xd430u)
#define   MT7996_PHY_DPD_GAIN_ERR_MARK      0x88888888u
#define   MT7996_PHY_DPD_GAIN_ERR_INIT      0x395832a4u

/* 0x8301fc10 (no band term; the chain-1 block of band 0): WM scratch word,
 * written at calibration init (V:e00d6d5c..V:e00d6d72): [0] = EEPROM 0x1ad
 * [5] (V:e00e04d6(5): bit 5 of the calibration-disable byte 0x1ad), [1] =
 * EEPROM 0x1a9 [6]; 0 on this unit. T2{6} reads [0] (above). No hardware
 * role [inferred]. [static][measured] */
#define MT7996_PHY_FW_1FC10                 0x8301fc10u
#define   MT7996_PHY_FW_1FC10_EE1AD_B5      HW_BIT(0)
#define   MT7996_PHY_FW_1FC10_EE1A9_B6      HW_BIT(1)

/* 0x8303fc10 / 0x8303fc14 (no band term; the chain-3 block of band 0): the
 * inputs of the A-die gain-step programmer V:e00d1dc4, mirrored by
 * V:e00d2540 before each call (every item-1 pass of the group loops and
 * every channel set): fc10 = E(1, 0) << 24 | E(1, 2) << 16 | E(0, 0) << 8 |
 * E(0, 2), with E(x, y) the signed RX gain EEPROM byte of (subband, WF, x,
 * y) at base + 2 (x + 2 s + 2 n WF) + y / 2 (V:e00e0774): base / n / s =
 * 0x65e / 1 / 0 (2.4 GHz), 0x66e / 3 / subband 0..2 (5 GHz), 0x6aa / 4 /
 * subband - 3 (5.95 GHz, subbands 3..6); fc14 = P[7] << 24 | P[6] << 16 |
 * group << 8 | flag (1 in the calibration and the first channel-set pass, 0
 * in the second, which then writes the per-chain RX gain offsets from the
 * same EEPROM bytes, phy.h MT7996_PHY_CSD_RXGOFS_*). Stock band 0, WF 0..3:
 * 0x070a1109 / 0x01010001. No reader found: a debug mirror [inferred].
 * [static][measured] */
#define MT7996_PHY_FW_GAINSTEP_IN           0x8303fc10u
#define   MT7996_PHY_FW_GAINSTEP_IN_E10   HW_GENMASK(31, 24)
#define   MT7996_PHY_FW_GAINSTEP_IN_E12   HW_GENMASK(23, 16)
#define   MT7996_PHY_FW_GAINSTEP_IN_E00   HW_GENMASK(15, 8)
#define   MT7996_PHY_FW_GAINSTEP_IN_E02   HW_GENMASK(7, 0)
#define MT7996_PHY_FW_GAINSTEP_P            0x8303fc14u
#define   MT7996_PHY_FW_GAINSTEP_P_ELNA   HW_GENMASK(31, 24)
#define   MT7996_PHY_FW_GAINSTEP_P_EPA    HW_GENMASK(23, 16)
#define   MT7996_PHY_FW_GAINSTEP_P_GROUP  HW_GENMASK(15, 8)
#define   MT7996_PHY_FW_GAINSTEP_P_FLAG   HW_GENMASK(7, 0)

#endif /* HW_MT7996_PHY_CAL_H */
