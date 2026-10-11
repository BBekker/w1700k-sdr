/* SPDX-License-Identifier: MIT */
/*
 * MT7996 PHY register map as data: block bases and roles, the ROM's
 * register-mapping tables (module ids, selectors), the WM's PHY table
 * programmer (which per-chain windows it writes on channel set and
 * calibration restore, with row counts and defaults), the per-band init's
 * register tables and the channel set's PHY tables A and B (format,
 * addresses, keys and what they write).
 *
 * ADDRESS SPACE: WF bus as the WM sees it (see phy.h for the address formula,
 * DSP and host views). Bases are given for band 0, chain 0; add band << 20
 * (and chain << 16 for per-chain blocks).
 *
 * Registers with known fields are defined in phy.h (RX/AGC/RXFE/WF_COMM),
 * phy_cal.h (calibration, TSSI, TPC) and dfe.h (DFE capture region).
 *
 * Confidence tags: [measured], [static], [inferred]. All tables here are
 * [static] (ROM register tables, WM code and data) unless marked.
 */
#ifndef HW_MT7996_PHY_MAP_H
#define HW_MT7996_PHY_MAP_H

#include "../hw_bits.h"

/* ========================================================================
 * Block map
 *
 * X(name, base, size, scope, rom_modules, role)
 *   base:  band 0 (and chain 0) address
 *   size:  window the known registers fall in, not a decoded block limit;
 *          windows of sub-blocks may overlap
 *   scope: 'C' per chain (+ chain << 16), 'B' per band, 'F' fixed address
 *   rom_modules: ROM module ids that resolve into the block ("-" none)
 * Per-chain blocks repeat for chains 0..3 (band 2: 0..4); position 4 of
 * band 0 (0x83040000..) is the monitor receiver's chain.
 * ======================================================================== */

#define MT7996_PHY_BLOCK_LIST(X) \
	X(CSD,        0x83000000u, 0x1000u, 'C', "0x38 (on 0x83040000)", \
	  "RX gain force (sets A/B/C), AGC, CAGC force, gain/RSSI status") \
	X(IRPI,       0x83001000u, 0x1000u, 'C', "-", \
	  "in-band power (IRPI) histogram") \
	X(RFINTF,     0x83007000u, 0x1000u, 'C', "0x4b..0x52 (no table)", \
	  "RF interface: TX-DAC/RX-ADC clocks, TTG RX cal words, debug override, " \
	  "WM table windows +0x600..+0xc8c (per-gain RX IQ, RX IQ filter bank)") \
	X(CHAIN_8000, 0x83008000u, 0x4000u, 'C', "-", \
	  "0x83008000..0x8300bfff: WM address constants only, no names") \
	X(TSSI,       0x8300c000u, 0x1000u, 'C', "0x3e", \
	  "TX power detector (TSSI), 13 instances (4+4+5)") \
	X(CHAIN_D000, 0x8300d000u, 0x1000u, 'C', "-", \
	  "unnamed table base; calibration bits; WM table item 20 window") \
	X(CHAIN_E000, 0x8300e000u, 0x1000u, 'C', "-", \
	  "calibration words (+0x108), RX-cal bits, DFE engine pulse (+0x81c)") \
	X(CHAIN_F000, 0x8300f000u, 0x1000u, 'C', "-", \
	  "+0x028 mode word; chain 0: synthesizer bookkeeping +0xc00..+0xc0c") \
	X(BN_CTRL,    0x83080000u, 0x0100u, 'B', "0x33, 0x3a, 0x65, 0x69/0x6a (no table)", \
	  "band clocks, RST_CTRL, per-packet reset, MISC IRQs, background RX") \
	X(BN_CSI,     0x83080c00u, 0x0020u, 'B', "0x6b", \
	  "CSI manual channel select, CSI SRAM word port") \
	X(BN_TOAE,    0x83080c58u, 0x03acu, 'B', "0x61", \
	  "TOAE control, calibration, AFE/DFE latency, TOAE SRAM port, latches") \
	X(BN_PHYCTRL, 0x83081000u, 0x0100u, 'B', "0x61, 0x63, 0x6f", \
	  "band mode word, PHYCTRL rate checks, GID table, HE-MU rules") \
	X(BN_PHYINTF, 0x83082000u, 0x0100u, 'B', "0x33, 0x65", \
	  "PHYINTF_RX_CTRL_1: IPI/RPI/RDD/STSCNT enables") \
	X(BN_BF,      0x83085000u, 0x0300u, 'B', "0x63, 0x67, 0x6f", \
	  "beamforming control, MU-TX control and link-quality words") \
	X(BN_RXTD,    0x83088000u, 0x1000u, 'B', "-", \
	  "RX time domain: AGC/SW-LNA words, RXTD12 IRPI clear") \
	X(BN_B000,    0x8308b000u, 0x1000u, 'B', "-", \
	  "touched by the channel-set D-die RX calibration") \
	X(BN_C000,    0x8308c000u, 0x1000u, 'B', "-", \
	  "+0x1a0 touched by the WM on all three bands") \
	X(DFE_BN_TX,  0x8308e000u, 0x0800u, 'B', "0x72/0x73, 0x6d", \
	  "TX precompensation (+0x204), TPC (+0x400..+0x7d0)") \
	X(DFE_BN_DSP, 0x8308ef00u, 0x0190u, 'B', "-", \
	  "DSP-side words +0xf04..+0x1088 (DSP trigger enable/pulse +0xf08/+0xf10)") \
	X(DFE_BN_RXFE, 0x8308f000u, 0x0200u, 'B', "0x72/0x73", \
	  "digital RX front end: RXFE_CTRL +0x110, WF_RXFE_01 +0x114, RXFE_MAN +0x118") \
	X(DFE_BN_TSSI, 0x8308fc00u, 0x0300u, 'B', "0x6d", \
	  "band TSSI core and thresholds +0xd00..+0xe60; WM words +0xc20, +0xcb0/4") \
	X(BN_TWIN,    0x83090000u, 0x10000u, 'B', "-", \
	  "twin of 0x83080000: the WM writes the same offsets in the same sequences") \
	X(WFC_DSP,    0x830a0000u, 0x1000u, 'B', "-", \
	  "DSP-visible words: RO status +0x25c..+0x268, DSP SP/PC mirrors +0x2b4/+0x2b8") \
	X(WFC_TRIG,   0x830a1000u, 0x0100u, 'B', "-", \
	  "capture trigger block (capture headers); +0x94 free-running counter") \
	X(WFC_RDD,    0x830a2000u, 0x0200u, 'B', "0x36, 0x37, 0x53", \
	  "radar detectors RDD0..2: TOP_5 (test pulse, IRQ), TOP_12 (antenna select)") \
	X(WFC_CTL,    0x830a3000u, 0x0100u, 'B', "-", \
	  "control words +0x000..+0x008, CSI clocks +0x050/+0x054, background RX +0x078/+0x07c") \
	X(WFC_CSI,    0x830a4800u, 0x0020u, 'B', "0x55", \
	  "CSI control, CSI dump start/status") \
	X(WFC_AHB,    0x830a5000u, 0x0058u, 'B', "0x36, 0x37, 0x53, 0x55, 0x57", \
	  "AHB bridge: PHY -> MCU-memory writer for RDD0..2 and CSI_TOAE") \
	X(WFC_TOAE,   0x830a5bc4u, 0x0028u, 'B', "0x59", \
	  "TOAE accelerator: mode enables, H peak, tau") \
	X(WFC_BGRX,   0x830a5c00u, 0x0220u, 'B', "-", \
	  "background receiver enables/bandwidth, IPI reset (+0xdfc), TOAE BW (+0xe10)") \
	X(WFC_AGC,    0x830a7000u, 0x1000u, 'B', "-", \
	  "band AGC words retuned by SW-LNA profiles") \
	X(WFC_DFE,    0x830ad000u, 0x1000u, 'B', "-", \
	  "DFE capture engine, capture lane mux, WM<->DSP interface (dfe.h); " \
	  "TTG RX DC/IQ engines (phy.h)") \
	X(WFC_E000,   0x830ae000u, 0x0100u, 'B', "-", \
	  "'Mask Coeff' words +0x0bc..+0x0c8") \
	X(DFS_B0,     0x830af000u, 0x0100u, 'F', "0x5f", \
	  "DFS_5/16/55, band 0") \
	X(DFS_B1,     0x830bf000u, 0x0100u, 'F', "0x5f", \
	  "DFS_5/16/55, band 1 (ROM resolves here, not 0x831af000)")

/* ========================================================================
 * ROM register-mapping tables
 *
 * The ROM accesses PHY fields through selectors:
 *   selector = module << 16 | group << 5 | field
 * A module's base address comes from a lui in its _hal<X>GetCRControlParam;
 * each 8-byte table entry is one register (its group), with a {lsb, msb}
 * list per field. Band-aware wrappers add (DBDC band << 16) to the selector,
 * i.e. select module + band (0x72 band 0 -> 0x73 band 1). Bits [31:24] of a
 * selector are ignored. Example: 0x72068b = module 0x72 (PHYDFE_CTRL_BN
 * band 0), group 52 (WF_RXFE_01), field 11 (RXFE_AFIFO_RD_EN). Retail and
 * SDK ROMs carry identical tables; the WM carries none of its own. [static]
 * ======================================================================== */

#define MT7996_PHY_ROMSEL(module, group, field) \
	(((module) << 16) | ((group) << 5) | (field))
#define   MT7996_PHY_ROMSEL_MODULE        HW_GENMASK(23, 16)
#define   MT7996_PHY_ROMSEL_GROUP         HW_GENMASK(15, 5)
#define   MT7996_PHY_ROMSEL_FIELD         HW_GENMASK(4, 0)

/*
 * X(table, module, first_reg, last_reg, registers, fields, role)
 * first_reg/last_reg: band-0 addresses in table order (not sorted).
 * 21 tables, 1,630 fields cover the PHY and capture.
 */
#define MT7996_PHY_ROM_TABLE_LIST(X) \
	X(WFRDM_PHYRX_CSD,          0x38u, 0x83040108u, 0x830400d0u,   3u,   6u, \
	  "monitor-chain AGC: AGC_28NMRF_0, AGC_311, AGC_14") \
	X(WFRDM_PHYRX,              0x33u, 0x8308003cu, 0x83080040u,   5u,   7u, \
	  "RST_CTRL, MISC_CTRL/_1/IRQ_STA, PHYINTF_RX_CTRL_1 (radar subset)") \
	X(WFRDM_PHYRX_BAND1,        0x3au, 0x83180038u, 0x83180040u,   3u,   5u, \
	  "band-1 MISC_CTRL/_1/IRQ_STA (RDD1 bits)") \
	X(WFRDM_PHYRX_COMM,         0x36u, 0x830a5000u, 0x830a2030u,  17u,  23u, \
	  "AHB bridge TOP/RDDn, RDD_TOP_5/12, RDD1/2_TOP_5") \
	X(WFRDM_PHYRX_COMM1,        0x37u, 0x831a500cu, 0x831a5024u,  17u,  17u, \
	  "band-1 AHB bridge RDDn, RDD tops") \
	X(WFRDM_WTBLOFF,            0x3bu, 0x820e9010u, 0x820e9018u,   3u,   9u, \
	  "LMAC WTBLOFF (not PHY)") \
	X(WFICAP,                   0x74u, 0x80023090u, 0x800230c0u,  12u,  37u, \
	  "ICAPCR0..12 (capture headers)") \
	X(PHYDFE_CTRL_BN_CMM,       0x72u, 0x8308e204u, 0x8308f114u,  53u, 213u, \
	  "TX precomp, TPC control/beamforming gain/MCS, WF_RXFE_01") \
	X(PHYDFE_CTRL_BN_PWR_REGU,  0x6du, 0x8308e478u, 0x8308fe60u, 120u, 413u, \
	  "TPC SKU2 FCC limits, band TSSI core/thresholds") \
	X(PHYRX_CTRL_BN_BF,         0x6fu, 0x83080038u, 0x83085230u,   5u,  80u, \
	  "MISC_CTRL, PHYCTRL_RATE_CTRL, BF_CONTROL, MUTX_CONTROL_0") \
	X(PHYRX_CTRL_BN_CSI,        0x6bu, 0x83080c04u, 0x83080c1cu,   7u,  21u, \
	  "CSI manual channel select, CSI SRAM port") \
	X(PHYRX_CTRL_BN_MULQ,       0x67u, 0x83085234u, 0x83085240u,   4u,  25u, \
	  "MU-TX link quality") \
	X(PHYRX_CTRL_BN_MURU,       0x63u, 0x83080040u, 0x83085244u,  13u,  41u, \
	  "MISC_IRQ_STA, GID table, HE-MU rules, MUTX_PN") \
	X(PHYRX_CTRL_BN_RDD,        0x65u, 0x83080014u, 0x83082004u,   2u,  27u, \
	  "RST_CTRL, PHYINTF_RX_CTRL_1") \
	X(PHYRX_CTRL_BN_TOAE,       0x61u, 0x83080c58u, 0x83081000u,  25u,  57u, \
	  "TOAE control/cal/latency/outputs/SRAM port/latches") \
	X(PHYDFS_WF_COMM_RDD,       0x5fu, 0x830af014u, 0x830af0e8u,   3u,  15u, \
	  "DFS_5, DFS_16, DFS_55") \
	X(PHYRX_CTRL_WF_COMM_CMM,   0x57u, 0x830a5000u, 0x830a5004u,   2u,  13u, \
	  "AHB bridge TOP and ISR") \
	X(PHYRX_CTRL_WF_COMM_CSI,   0x55u, 0x830a4800u, 0x830a5054u,  11u,  21u, \
	  "CSI_CTRL/AHB_CTRL/STATUS, CSI_TOAE bridge client, AHB_RSV0..3") \
	X(PHYRX_CTRL_WF_COMM_RDD,   0x53u, 0x830a2014u, 0x830a5030u,  14u,  60u, \
	  "RDD tops, AHB bridge RDDn clients") \
	X(PHYRX_CTRL_COMM_TOAE,     0x59u, 0x830a5bc4u, 0x830a5be8u,  10u,  28u, \
	  "TOAE_ACC_0..2, TOAE_ACC_R0..R6") \
	X(PHYDFE_CTRL_WF_TSSI,      0x3eu, 0x8300c00cu, 0x8300c3dcu, 141u, 512u, \
	  "per-chain TSSI, 13 instances: 0x830{0..3}c, 0x831{0..3}c, 0x832{0..4}c")

/*
 * ROM modules enumerated without a mapping table (registers unnamed in any
 * corpus read). [static]
 *   0x4b..0x52  PHYDFE_RFINTF_WF0..7_CMM   per-chain RF interface (0x83007000)
 *   0x69, 0x6a  PHYRX_CTRL_BN0/1_CMM
 *   0x5d, 0x5e  PHYRX_POST_BN0/1_CMM       holds the ROM RX async-FIFO "on TX"
 *               switch (halToaeSetTRXPort / halToaeForceBestTRX); register
 *               not identified
 *   0x5b, 0x5c  PHYRX_CSD_WF_COMM0/1_CMM
 *   0x35        WFRDM_PHYDFE
 *   0x09..0x0c  WFMIMO, WFCTRL
 * Fields named in the ROM enums but absent from every table:
 * SW_TOAE_LATCH_DATA_3/4/5 (incl. TOAE_AGC_{LPF,LNA}_GAIN_IDX),
 * TOAE_TRIG_GEL_EN, TOAE_TRIG_GL_CNT_0/1, PHY_CTRL_0.
 * Table quirks: WF_RXFE_01 lists bit 3 in two fields; some tables carry
 * unnamed extra fields (TOAE_CAL_3, TOAE_OUT_LSB, SW_TOAE_LATCH_DATA_2,
 * RDD_TOP_12 [13:7]). The ROM TOAE helpers insert the band at bit 16, not 20.
 */
#define MT7996_PHY_ROMMOD_RFINTF_WF(n)    (0x4bu + (n))	/* n 0..7 */
#define MT7996_PHY_ROMMOD_PHYRX_POST_BN(b) (0x5du + (b))
#define MT7996_PHY_ROMMOD_PHYDFE_CTRL_BN(b) (0x72u + (b))

/* ========================================================================
 * WM PHY table programmer (channel set / calibration-data restore)
 *
 * A WM routine takes a record (request byte, chain byte C at +1, mode at +4,
 * item flags, payload pointer) and, for each item flag set, either writes
 * the item's DEFAULT words or unpacks the record's packed payload into the
 * item's per-chain destination rows. Destinations are
 *   prefix + band << 20 + row_offset,  prefix = 0x83000000 + C << 16,
 * i.e. offsets below are within the chain's per-chain block. The payload
 * record is chosen by request and mode (e.g. per bandwidth class). The
 * packed path is taken when the payload's presence halfword (+4) is nonzero
 * and the item's bit in the caller's mask is clear; for items 5, 6 and 8 a
 * set bit in a second WM enable word additionally requires the chain's
 * DBG_OVR magic (phy.h), else defaults are written. [static] For items 6
 * and 5 that word is WM 0xe01ec1ac bits 6 / 5, which the TTG RX result
 * range checks set on failure (phy_cal.h MT7996_PHY_DCOC_FAIL_FLAGS); for
 * item 8 it is bit 8 (the TX gain-step save check). The loop-DC write-back
 * (indirect item 28 below) tests bit 28 the same way, and that bit is set in
 * the stock load. Item 9's write-back has no such condition; its save
 * checks DBG_OVR instead (phy_cal.h restore flag 0x200). [static, measured]
 *
 * Destination record format (the shared programmer):
 *   u16 offset; then up to F pairs {u8 dest_shift, u8 src_width}; a zero
 *   width ends the row. Stride 2 * (F + 1) bytes.
 * Packed payload: a little-endian bit stream, cursor persists across fields
 * and rows; each field takes src_width bits and ORs them in at dest_shift.
 * A field narrower than the "reference row" first-pair width (reference row
 * cursor advances per row and rewinds every 'period' rows) is sign-extended
 * to that width. Rows are written whole (no read-modify-write). An indirect
 * mode instead calls the ROM A-die write API (halPhyRfRegWriteAnt) with
 * (prefix + offset, word, ant = C + die << 4).
 *
 * X(item_bit, first_off, last_off, rows, period, fields, defaults)
 *   rows: rows written on the packed path (default path in 'defaults')
 * ======================================================================== */

#define MT7996_PHY_TBLPROG_LIST(X) \
	X( 9, 0x7600u, 0x769cu, 25u, 25u, 4u, \
	  "40 words of 0; packed: 10/15/20/25 rows (5 * ceil(n/4)), period = rows; " \
	  "rows in groups of 5: 0x7600+8g, 0x7604+8g, 0x7640+4g, 0x7660+4g, 0x7680+4g") \
	X( 8, 0x76b0u, 0x76b8u,  3u,  3u, 4u, \
	  "3 words of 0") \
	X(17, 0x76bcu, 0x77d8u, 72u,  8u, 2u, \
	  "72 words: 0x10000000 where row % 8 == 3, else 0") \
	X( 5, 0x7820u, 0x7868u, 15u, 12u, 4u, \
	  "5 triplets (0x7820+8i, 0x7824+8i, 0x7858+4i) = 0x02000200, 0x02000200, 0") \
	X( 6, 0x78d0u, 0x7c8cu, 240u, 20u, 2u, \
	  "120 or 240 words: 0x10000000 where row % 20 == 5, else 0; " \
	  "rows alternate 10 at 0x78d0+0x28j and 10 at 0x7ab0+0x28j") \
	X(20, 0xd034u, 0xd044u,  5u,  0u, 0u, \
	  "5 words in order d044,d034,d038,d03c,d040 = 0x200,0,0,0,0; payload copied word-for-word") \
	X(21, 0xc218u, 0xc254u, 16u,  0u, 0u, \
	  "16 words (TSSI_PWR_DNL_GT0..7_OFST) = 0; payload copied word-for-word") \
	X(22, 0xc05cu, 0xc068u,  4u,  0u, 0u, \
	  "4 words at +record offset (TSSI_PRIM_CORE_PA_TSSI_GC_OFST_00..03): " \
	  "request != 0: 0x58,0x341000f0,0x60,0x381000f0; request 0: 0x6048,0x301800e8 x2")

/*
 * Item 6 is the RX IQ compensation filter bank (phy.h MT7996_PHY_RXIQC_A /
 * _B), the result of the TTG RX calibrations T1{18..22}, written through
 * calibration restore flag 0x40 (phy_cal.h MT7996_PHY_CAL_RESTORE_RXIQC):
 * one 20-row period per slot (10 rows half A, 10 half B), 120 words = slots
 * 0..5, 240 = slots 0..11 (BW320); row % 20 == 5 is half A word 5, the main
 * tap. Item 5 is the per-gain RX IQ table of T1[23] (phy.h
 * MT7996_PHY_FIQ_*, restore flag 0x20, MT7996_PHY_CAL_RESTORE_FIQ). Item 9
 * is the TX LO-leakage and TX IQ codes of T1{9,13,17} (phy.h
 * MT7996_PHY_TXLO_I / _Q, MT7996_PHY_TXLOIQ_A / _B, restore flag 0x200,
 * MT7996_PHY_CAL_RESTORE_TXLOIQ): 8 row groups g of 5 rows (steps 4g..4g +
 * 3), 40 words; the packed path writes the first 5 * ceil(n / 4) rows (n =
 * 16 steps on the MT7976: 20 rows; 12 on the MT7977: 15) with period = rows,
 * so no field is sign-extended; the WM row table is 0xe006ffe4
 * (phy_cal.h MT7996_PHY_TXLOIQ_ROWS). Item 8
 * is the TX gain-step code table of T1{7,11,15} (phy.h
 * MT7996_PHY_TXSTEP_CODES, restore flag 0x100, MT7996_PHY_CAL_RESTORE_TXSTEP).
 * Item 17 is the TX IQ rows of T1{8,12,16} (phy.h MT7996_PHY_TXIQC, restore
 * flag 0x20000, MT7996_PHY_CAL_RESTORE_TXIQ): 9 row groups of 8 words, row %
 * 8 == 3 the main-tap word; the packed path sign-extends the 8-bit side taps
 * of groups 1..8 to 14 bits (period 8). Items 20, 21, 22 are the T2{6},
 * T2{4}, T2{5} results (restore flags 0x100000, 0x200000, 0x400000).
 * At every channel set phyCalBasicConfig item 16 (V:e004489e, not gated by
 * the mask) writes the defaults of table items 8, 9, 17 and 20 and the DPD
 * LUTs for the WF after each phyCalBasicConfig item-8 backup (phy_cal.h mask
 * bit 3), before op 8 reapplies the stored records (census). [static][measured]
 */

/*
 * Indirect items: A-die register writes through halPhyRfRegWriteAnt; low 12
 * bits of the argument are the A-die register index. [static; index reading
 * inferred]
 *   item 2:  0x8e2, 0x8e8, 0x8e1, 0x8e7, 0x8e0, 0x8e6   default 0x40404040
 *   item 1:  0x8f2, 0x8f8, 0x8f1, 0x8f7, 0x8f0, 0x8f6   default 0
 *   item 28: 0x8ef, 0x8ee, 0x8ed, 0x8ec, 0x8ff, 0x8fe, 0x8fd, 0x8fc
 *                                                       default 0x40404040
 * Item 28 is the TX loop-DC result rows (adie.h ADIE_LOOPDC_RES*, restore
 * flag 0x10000000, phy_cal.h MT7996_PHY_CAL_RESTORE_LOOPDC): V:e0044dde
 * writes the defaults via V:e0044e30 or a record via V:e0044234 ->
 * V:e0044362. [static, measured] Items 2 and 1 are the fine / coarse RX DC
 * results (restore flags 4 / 2) [inferred].
 */

/*
 * PHY selector programmer: request byte r (1..7) -> code, written to
 * BN_MODE[27:24] and RXFE_CTRL[7:4] (phy.h), then the caller's mode into
 * BN_MODE[30:28]. Requests 0 and >= 8 give code 0. The table is relocated by
 * the WM's data initializer; the bytes in the downloaded image at its
 * runtime address are different. Code meaning not decoded. [static]
 * After the writes the WM pulses a per-band gate (band 0: bit 8 of
 * 0x8102610c, band 1: 0x81026110, band 2: WFSYS_ON 0x81033004/08 = 16;
 * fields of 0x81026800; bit n of 0x8102000c); conn.h has the calibration
 * tail's sequence on the same words (at MT7996_CONN_PWR_800).
 */
#define MT7996_PHY_SELCODE_INIT           { 1u, 2u, 3u, 4u, 0u, 0u, 4u }	/* r = 1..7 */

/*
 * Configuration byte selecting the BN 0xfcb0/0xfcb4 pattern (phy.h
 * MT7996_PHY_DFE_FCB_PAT_*): table[row][band] in WM data; 5 and 6 select a
 * pattern when the preparation flag is 0, 7 when it is 1. [static]
 * The table is the WM's AdieSel table (u1CurrAdie per band): s8
 * 0x02219d9c[mode * 3 + band], mode = WM byte 0xe01ec164 (V:e00d4344: chip
 * SKU & 7 == 4 -> 6; else configuration byte 0xe0093075 == 0xf -> 5, == 7 ->
 * 4; else SKU & 3). Stock mode 0: { 6, 7, 7 }. The per-band PHY init
 * branches on the same byte: gain-index selectors 5 / 6 = MT7976, other =
 * MT7977 (phy.h MT7996_PHY_GSEL_*); TOP init 6 = MT7976, other = MT7977,
 * TOP 0xdc4 for 6 and 7 (adie.h "TOP init"). [static][measured]
 */
#define MT7996_PHY_FCB_CFG_ROWS           7u
#define MT7996_PHY_FCB_CFG_INIT \
	{ { 6u, 7u, 7u }, { 5u, 7u, 7u }, { 6u, 0u, 7u }, { 5u, 0u, 7u }, \
	  { 0xdau, 0xdau, 7u }, { 0x0cu, 0x0cu, 7u }, { 0u, 0u, 7u } }

/* AdieSel (u1CurrAdie): the A-die type byte of a band, s8 table
 * MT7996_PHY_FCB_CFG_INIT[row][band] (above). Row (WM byte 0xe01ec164,
 * V:e00d4344): 6 when the SKU (conn.h CBTOP_PAD_GPIO_SKU) [2:0] == 4; else
 * 5 when the configuration byte 0xe0093075 is 0x0f, 4 when it is 7; else
 * SKU [1:0]. The configuration
 * byte is written just before by halPhyCal op 24 (V:e004171e): die 2's ECO
 * byte (adie.h ADIE_TOP_CHIP_ID_VER, radio.h radio_eco()); the ROM patch's
 * hook it asks first returns neither 0xda nor 0xc. With ECO [2:0] == 7,
 * V:e00d41e4 gives band 2 die 2 and the other bands die 0; else band b
 * uses die b. Types: 5 MT7975, 6 MT7976, 7 MT7977; the per-band init's
 * gain-index selectors take the MT7976 values for 5 and 6, its TOP init
 * clears / sets TOP 0xdc4 for 6 and 7 and runs the MT7976 branch for 6,
 * the MT7975 branch for 5 and the MT7977 branch otherwise. Stock row 0:
 * { 6, 7, 7 }. [static][measured] */
#define MT7996_PHY_ADIESEL_ROW_SKU        6u
#define MT7996_PHY_ADIESEL_ECO_ROW5       0x0fu
#define MT7996_PHY_ADIESEL_ROW_ECO5       5u
#define MT7996_PHY_ADIESEL_ECO_ROW4       0x07u
#define MT7996_PHY_ADIESEL_ROW_ECO4       4u
#define MT7996_PHY_ADIESEL_ECO_REMAP      HW_GENMASK(2, 0)	/* == 7: the die remap above */
#define MT7996_PHY_ADIESEL_MT7975         5u
#define MT7996_PHY_ADIESEL_MT7976         6u
#define MT7996_PHY_ADIESEL_MT7977         7u

/*
 * WM PHY init, applied to all three bands (B = 0x83000000 + band << 20):
 *   B+0x80004 = 0, B+0x80008 = 0          (band clock words)
 *   B+0x8fc20 = 0
 *   B+0x8fd00 |= 0x80000000               (TSSI_CORE_00 TSSI_PKT_MODE)
 *   B+0x8fd34 = 0
 *   B+0xad4d0 = 0x01010101, B+0xad4d8 = 0x01010101   (DFE region, dfe.h)
 * [static] With the firmware up the band-0 clock words read 0 and
 * 0x830ad4d0/d8 read 0x01010101. [measured]
 */

/*
 * WM register tables (V:e00d8f80(entries, values, n, chain)), applied by the
 * per-band PHY init (phy.h "Per-band PHY init") to every band (+ band <<
 * 20), after its own words: entry i = {u32 address, u16 value index, u16 0}
 * at entries + 8 i; value = u32 values[index]; MT7996_PHY_REGTBL_SKIP =
 * skip the row. With chain != 0xf an address in 0x83000000..0x8303ffff is
 * relocated to 0x83000000 + chain << 16 + (address & 0xffff). Tables in WM
 * .data (VMA; load image at VMA - 0xbdc):
 *   per-chain: entries MT7996_PHY_REGTBL_PERCHAIN_ENT (70 rows: 45 live, 25
 *     skip rows at band words BN +0xfd88..+0xffa4, the TSSI / DPD section,
 *     never written here), values MT7996_PHY_REGTBL_PERCHAIN_VAL;
 *     V:e00d8ffe for chains 0..3: the per-chain +0x2000 page (9 rows), TSSI
 *     (6), CHAIN_D000 (30);
 *   absolute: entries MT7996_PHY_REGTBL_ABS_ENT (312 rows, all live),
 *     values MT7996_PHY_REGTBL_ABS_VAL; V:e00d9028 (chain 0xf): WFC_AGC
 *     (82), BN_RXTD (50), CSD (55; chains 0..4 explicit), CHAIN_E000 (30),
 *     DFE_BN_RXFE +0xf088..+0xf19c (10) and +0xf200..+0xf240 (17), BN_BF
 *     (19), WF_COMM +0x6000 (18), BN +0x6000 (10), RDD / DFS (12), WFC_DSP
 *     (6), WFC_DFE (2), BN_CTRL (1).
 * The same values on all three bands; 492 stores per band. Rows of named
 * registers carry their value at the register (phy.h, the _INIT macros and
 * "register table" notes); every row, with its value, is in phy_init.h
 * (MT7996_PHY_INIT_REGTBL_PERCHAIN / _ABS), the rows of unknown role under
 * offset names in phy.h and phy_cal.h (MT7996_PHY_TSSI_*). [static][measured]
 */
#define MT7996_PHY_REGTBL_SKIP            0xf1f2f3f7u
#define MT7996_PHY_REGTBL_PERCHAIN_ENT    0x0221e540u
#define MT7996_PHY_REGTBL_PERCHAIN_VAL    0x0221e1f4u
#define MT7996_PHY_REGTBL_PERCHAIN_N      70u
#define MT7996_PHY_REGTBL_ABS_ENT         0x0221ce30u
#define MT7996_PHY_REGTBL_ABS_VAL         0x0221c564u
#define MT7996_PHY_REGTBL_ABS_N           312u

/* ========================================================================
 * Channel-set PHY tables A and B (V:e00d9238, V:e00d9702; phy_cal.h
 * "Channel set" step 9). Decoded against MediaTek's cold stock load (its
 * three channel sets regenerated record for record). [static][measured]
 *
 * Column walker V:e00d917e(desc, addrs, vals, colmap, ncols, key, ngroups,
 * chain), the format of the A-die item-1 walker V:e0051af2: for group g
 * the column is colmap[ncols * key + g]; desc[g] = {u32 idx_array, u8
 * nrows at +4, u8 ncols at +5}; row r's value = vals[u16 idx_array[desc.ncols
 * * r + column]] (u32), its address addrs[i], i counting every row of
 * every group; the value MT7996_PHY_REGTBL_SKIP writes nothing (i still
 * advances). Addresses are relocated as in the register tables above (a
 * per-chain address 0x8300xxxx..0x8303xxxx to the walk's chain, chain 0xf:
 * none) and get + band << 20. Whole words, no read-modify-write. The key
 * selects by its position in a u8 list. HAZARD: a key not in its list
 * logs and calls cos_assert_preprocess (ROM slot 0x0081ac00), and the scan
 * runs on past the list: a channel / EPA / stream combination outside the
 * list asserts in the WM. The value arrays are those of the register
 * tables above (table A MT7996_PHY_REGTBL_PERCHAIN_VAL, table B
 * MT7996_PHY_REGTBL_ABS_VAL). Tables in WM .data (VMA). With the log key,
 * MT7996_PHY_RFIF_D414_SKIP_CHTBL (phy.h) skips both walks and the fields
 * noted below.
 *
 * Table A (247 / 320 / 320 writes on bands 0 / 2 / 1): one walk per chain
 * c = first WF..3, key MT7996_PHY_CHTBL_A_KEY (V:e00d9046) of the key
 * record V:e00d17da(P): k0 = 0 for group 0, else 1, and 2 when the centre
 * is above 6,585,000 kHz with P[6] = 0; k1 = P[6] (EPA); k2 = 1 at
 * 2,484,000 kHz, 0 elsewhere on 2.4 GHz, 2 on 5 / 6 GHz; k3 = c. Stock
 * keys (0, 1, 0, c) on band 0, (1, 1, 2, c) on bands 1 and 2. The walks
 * (196 / 257 / 257 words) write per chain the +0x2000 block
 * (+0x2008..+0x2080, phy.h MT7996_PHY_CHAIN_2000), TSSI +0x04c / +0x200 /
 * +0x258 and the CHAIN_D000 TX power words +0xd100..+0xd2d4 (phy_cal.h
 * MT7996_PHY_TXPWR_SAVE_*); band words BN +0xd01c..+0xd0bc, DFE_BN_TX
 * +0xe200..+0xe764 (TPC, TPC_MCS), DFE_BN_TSSI +0xfd08..+0xffc0 (TSSI core,
 * DFE gain compensation, DPD LUT select) and WF_COMM +0xb000, +0xb538,
 * +0xb580, +0xb59c, +0xc058, +0xd4ec..+0xd4f8, +0xe010 (phy_cal.h
 * MT7996_PHY_DPD_CFG4). Then explicit fields (V:e00d92c2..V:e00d9540),
 * read-modify-writes unless "whole":
 *  - with MT7996_CBTOP_HW_REV != 0x8a00, MT7996_PHY_DPD_FORCE_TBL != 1 and
 *    k1 = 1 (phy_cal.h MT7996_PHY_DFE_GAIN_COMP): +0xfdf8 [9:0] / [25:16]
 *    = 0xb0 / 0x50, +0xfdf4 = 0x80 / 0xa0; +0xfd70 = 0x47655544 (whole);
 *    +0xff14 [23:0] = 0x575757, +0xff18 [31:8] = 0x575754, +0xff80 [15:8]
 *    = 0x51; +0xff20 [23:0] = 0x565656, +0xff24 [31:8] = 0x565653, +0xff80
 *    [7:0] = 0x50; +0xff2c [23:0] = 0x525252, +0xff30 [31:24] = 0x52;
 *  - with CBTOP_HW_REV != 0x8a00 and k1 = 1: TPC_MCS +0xe738 [18:16] = 3;
 *    +0xe738, +0xe73c, +0xe74c, then +0xe750, +0xe758: [2:0], [6:4],
 *    [10:8], [14:12] = 3; per chain c..3 MT7996_PHY_TSSI_PWR_CTRL [24] = 1
 *    and, with k0 = 1, MT7996_PHY_CHAIN_2000 +0x0c / +0x18 / +0x28 [23:0] =
 *    0x20d0d0;
 *  - with CBTOP_HW_REV != 0x8a00: WF_COMM +0xc058 [16], then [20] = 1;
 *    phy.h MT7996_PHY_WFCOMM_B580 [13:4] = 0;
 *  - always: MT7996_PHY_TPC_CTL(band, 9) (+0xe424) [14], [13], [6:4] = 0;
 *    BN +0xff68 [22:20], [17:16], [28], [27:24], [29], [30], [31] = 0, one
 *    read-modify-write each.
 * The D414 skip jumps over the walks and the first three groups. Stock:
 * every field keeps its walk value except +0xfdf4 [25:16] (0xb0 -> 0xa0).
 * Mostly TX.
 *
 * Table B (665 writes on every band): not per chain (chain 0xf), key
 * MT7996_PHY_CHTBL_B_KEY (V:e00d90b6; k0 := 2 when k1 = 4 and k0 = 1) of
 * the key record V:e00d1836(P, streams): k0 = 0 for group 0, 1 for groups
 * 1..8 and g' 24..27, 2 for groups 9..23 and g' 28..; k1 = P[4] (BW class;
 * class 5 counts as 2); k2 = streams - 1 (3); k5 = (P[7] == 0) (no ELNA).
 * Stock keys (0, 0, 3, 0), (2, 4, 3, 0), (1, 3, 3, 0) on bands 0, 2, 1.
 * The walk runs twice back to back (V:e00d9774, V:e00d979a, each after its
 * own key computation) and writes the same 331 words (330 addresses) both
 * times: on chains 0..4 (band 0's chain 4 included) 42 CSD words
 * +0x000..+0x408 (among them CSD_00, CSD_D4, CSD_104, AGC_TH0, CSD_1B4 /
 * _1BC / _1C0 and the RX gain offset words MT7996_PHY_CSD_RXGOFS_*) and
 * CHAIN_E000 +0x07c; BN +0x60b0..+0x62c8; BN_RXTD +0x8010..+0x8e30 (among
 * them MT7996_PHY_BN_8014, _8200, _8554, _8590, _85A0, _8D60, _8E30);
 * DFE_BN_RXFE +0xf020..+0xf170 (MT7996_PHY_DFE_F028); WF_COMM +0x0000,
 * +0x0004, +0x0050, +0x2004..+0x21e0 (RDD), +0x6108, +0x6124,
 * +0x702c..+0x7448 (WFC_AGC), +0xf00c. RX. Then explicit fields: unless
 * MT7996_CBTOP_HW_REV == 0x8a00, BN +0x85a0 [15:14] and BN +0x8e30 [31:30]
 * = 3 on 6 GHz (k0 = 2) at k1 >= 2, else 0 (phy.h MT7996_PHY_BN_85A0,
 * MT7996_PHY_BN_8E30); always, WF_COMM +0xc104 [3:0] = 4 on band 2 at k1 =
 * 4 with EEPROM 0x1a4 [6] set, else 0 (phy.h MT7996_PHY_WFCOMM_C104). The
 * D414 skip jumps over both walks and the BN fields. In the receive-only
 * subset.
 * ======================================================================== */
#define MT7996_PHY_CHTBL_A_DESC           0x0221d7f0u
#define MT7996_PHY_CHTBL_A_ADDRS          0x0221df44u	/* u32 addresses */
#define MT7996_PHY_CHTBL_A_VALS           MT7996_PHY_REGTBL_PERCHAIN_VAL	/* u32 values */
#define MT7996_PHY_CHTBL_A_COLMAP         0x0221dc34u	/* u8 [28][27] */
#define MT7996_PHY_CHTBL_A_KEYS           0x0221df28u	/* u8 [28] */
#define MT7996_PHY_CHTBL_A_NKEYS          28u
#define MT7996_PHY_CHTBL_A_NGROUPS        27u
#define MT7996_PHY_CHTBL_A_KEY(cls, epa, sub, chain) \
	((cls) + 3u * (epa) + 6u * (sub) + 18u * (chain))
#define MT7996_PHY_CHTBL_B_DESC           0x0221a02cu
#define MT7996_PHY_CHTBL_B_ADDRS          0x0221c038u	/* u32 addresses */
#define MT7996_PHY_CHTBL_B_VALS           MT7996_PHY_REGTBL_ABS_VAL	/* u32 values */
#define MT7996_PHY_CHTBL_B_COLMAP         0x0221a900u	/* u8 [110][53] */
#define MT7996_PHY_CHTBL_B_KEYS           0x0221bfc8u	/* u8 [110] */
#define MT7996_PHY_CHTBL_B_NKEYS          110u
#define MT7996_PHY_CHTBL_B_NGROUPS        53u
#define MT7996_PHY_CHTBL_B_KEY(rf, bw, nss1, noelna) \
	(3u * (bw) + (rf) + 15u * (nss1) + 75u * (noelna))

#endif /* HW_MT7996_PHY_MAP_H */
