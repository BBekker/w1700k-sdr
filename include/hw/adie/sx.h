/* SPDX-License-Identifier: MIT */
/*
 * A-die synthesizers: the register family shared by the MT7976 and MT7977
 * fractional-N PLLs ("SX"), the frequency-word arithmetic, the lock
 * procedure, and the separate crystal-referenced TTG PLL. Die-specific
 * ratios, presets, ranges and sequences are in mt7976.h and mt7977.h.
 *
 * ADDRESS SPACE: 12-bit A-die register indices (see adie.h) inside the bank
 * that holds the synthesizer:
 *   MT7976 (die 0)   2.4 GHz SX in k 0 (drives WF0..3); a 5 GHz SX in k 4,
 *                    dormant on this board
 *   MT7977 (dies 1, 2)  SX in k 4 (drives every chain of the die)
 * The MT7977 chain banks k 0/1 carry a copy of the SX register set (247 of
 * 269 words in 0xa00..0xbff equal the SX bank; idle VCO word 0x06f06811 at
 * 0xa4b, 0xa40 = 0x15, 80 MHz-PFD style) with no PLL behind it: no VCO word
 * from 5 to 12 GHz with the lock sequence sets 0xca7[16] or moves 0xca4.
 * [measured]
 *
 * MODEL. f_VCO = N * f_PFD, N = INT + FRAC / 2^24 (24-bit fraction);
 * f_LO = f_VCO * ratio (a fixed divider/SSB network per mode). The VCO first
 * needs a capacitor-bank code ("band code"), found by a successive
 * approximation: in software on the MT7977 (mt7977.h), inside the 0xb81
 * lock step on the MT7976. Then the loop locks within that band. The closed
 * loop's edges are the bank's end codes plus the charge pump's Vtune range:
 * a lock held while the target moves stops exactly where a cold lock does,
 * and no single bit in either SX bank moves an edge by more than about
 * 10 MHz of LO. [measured]
 *
 * Writing only INT then FRAC moves a locked LO at once, within the current
 * band code's hold range (about +-9 MHz of VCO). [measured] A full MT7977
 * retune takes 0.8 ms of WM time, 1.5 ms when presets change; samples taken
 * during a retune are invalid. [measured] Register programs written at run
 * time never transmit (contrast a radio reload, mt7976.h/mt7977.h). [measured]
 *
 * Confidence tags: [measured], [static], [inferred]; "HAZARD:" as in adie.h.
 */
#ifndef HW_ADIE_SX_H
#define HW_ADIE_SX_H

#include "../hw_bits.h"

/* ------------------------------------------------------------------------
 * Frequency words
 * ------------------------------------------------------------------------ */

/* 0xa4b  INT word. RW. Write before ADIE_SX_FRAC. [static][measured]
 * [29:20] integer part of N (10 bits); [18:16] SUB, nonzero only with a
 * nonzero SDM split (0xa55[9:8] on the MT7977, 0x133[1:0] on the MT7976),
 * keep 0; [15:0] VT = VCO target in 1/3 MHz: floor(3 * f_VCO/MHz + 1.5).
 * Observed 0x03406271 at 5250 MHz
 * (die 1), 0x06105b94 at 2442 MHz (die 0), 0x036065a5 at 6505 MHz (die 2). */
#define ADIE_SX_INT                     0xa4bu
#define ADIE_SX_INT_N                   HW_GENMASK(29, 20)
#define ADIE_SX_INT_SUB                 HW_GENMASK(18, 16)
#define ADIE_SX_INT_VT                  HW_GENMASK(15, 0)

/* 0xa4a  FRAC word. Write after ADIE_SX_INT. [static][measured]
 * [23:0] FRAC (all 24 bits are honoured: a 1.234568 MHz step moved the LO
 * 1.234736 MHz, within the reference's drift; about 6 Hz per LSB at an
 * MT7977 LO). [31] LOAD: write 1; reads back 0, so never verify this word by
 * readback. [measured] */
#define ADIE_SX_FRAC                    0xa4au
#define ADIE_SX_FRAC_F                  HW_GENMASK(23, 0)
#define ADIE_SX_FRAC_LOAD               HW_BIT(31)
/* The LO moves in steps of 2 FRAC LSBs: bit 0 is ignored on both dies (2026-10-11, the tone's
 * phase across FRAC switches in one live stream: +1 LSB from an even base 0.02 +- 0.27 Hz on die 0,
 * -0.09 +- 0.78 Hz on die 1; +2 LSB -2.965 +- 0.34 Hz (die 0, 5/16: expected -2.980); 1024 LSB
 * 1.4912 Hz and 5.9607 Hz per LSB against 1.4901 / 5.9605). Smallest LO step = 2 f_PFD / 2^24 x
 * ratio: MT7976 2.98 Hz (5/16), 3.58 Hz (3/8); MT7977 11.92 Hz (5/8), 14.31 Hz (3/4). MediaTek's
 * arithmetic computes 23 bits and shifts them up one (sxchan.h). With the SDM split d = 1..3 and
 * words computed as its arithmetic would (SUB = the fraction's top d bits), die 0 does not lock
 * ("VCO too low"): no finer step found. [measured] */
#define ADIE_SX_FRAC_STEP_LSB           2u

/* Arithmetic (64-bit; frequencies in Hz).
 * N in Q24, rounded: INT = q >> 24, FRAC = q & 0xffffff. */
#define ADIE_SX_N_Q24(vco_hz, pfd_hz)                                   \
	((((unsigned long long)(vco_hz) << 24) +                        \
	  (unsigned long long)(pfd_hz) / 2u) / (unsigned long long)(pfd_hz))
#define ADIE_SX_VT_OF_VCO_KHZ(vco_khz)  ((3u * (unsigned int)(vco_khz) + 1500u) / 1000u)
#define ADIE_SX_INT_WORD(nq24, vt)                                      \
	(HW_PREP(ADIE_SX_INT_N, (unsigned int)((nq24) >> 24)) |         \
	 HW_PREP(ADIE_SX_INT_VT, (vt)))
#define ADIE_SX_FRAC_WORD(nq24)                                         \
	(ADIE_SX_FRAC_LOAD | ((unsigned int)(nq24) & ADIE_SX_FRAC_F))
/* f_VCO back from the words (Hz). */
#define ADIE_SX_VCO_HZ(int_word, frac_word, pfd_hz)                     \
	(((((unsigned long long)HW_GET(ADIE_SX_INT_N, (int_word))) << 24) + \
	  ((frac_word) & ADIE_SX_FRAC_F)) * (unsigned long long)(pfd_hz) >> 24)

/* ------------------------------------------------------------------------
 * VCO band (capacitor bank) code
 * ------------------------------------------------------------------------ */

/* 0xa01  Band code and preset. RW. [static][measured on the MT7977]
 * [31:20] band code as the SAR writes it (0x800 + 10 bits):
 *   [31]    code bit 11: must be 1; with it clear the VCO sits at
 *           8.32..8.36 GHz whatever the other bits are;
 *   [30]    code bit 10: not implemented (reads back 0);
 *   [29:20] 10-bit capacitor code; larger = lower VCO. MT7977: 0x3ff (code
 *           0xbff) = VCO 7.44 GHz .. code 0x800 = 9.84 GHz, no hidden
 *           capacitance beyond; one code holds about +-9 MHz of VCO.
 * [19:0] per-frequency preset (MT7977 table, 0x10cXX); kept by the SAR.
 * Observed 0x9c910cdd (die 2, 6505 MHz), 0xa1110cff (die 1, 5250 MHz).
 * The MT7976 searches internally and leaves this field alone (observed
 * 0x20010044); its result reads at ADIE_SX_STATUS[31:20]. */
#define ADIE_SX_BAND                    0xa01u
#define ADIE_SX_BAND_WORD               HW_GENMASK(31, 20)
#define ADIE_SX_BAND_EN                 HW_BIT(31)
#define ADIE_SX_BAND_B10                HW_BIT(30)
#define ADIE_SX_BAND_CODE               HW_GENMASK(29, 20)
#define ADIE_SX_BAND_PRESET             HW_GENMASK(19, 0)
#define ADIE_SX_BAND_WORD_LOWEST        0xbffu	/* lowest VCO */
#define ADIE_SX_BAND_WORD_HIGHEST       0x800u

/* 0xa02  Band code shadow. RW. [31:20] written with the same code as 0xa01
 * at every SAR step; [19:0] written 0. [static] */
#define ADIE_SX_BAND2                   0xa02u
#define ADIE_SX_BAND2_WORD              HW_GENMASK(31, 20)

/* ------------------------------------------------------------------------
 * Lock control and status
 * ------------------------------------------------------------------------ */

/* 0xb81  SX command. RW. Observed 0x10 at rest. [static][measured]
 * In the SX bank: 0x100 prepare, 0x101 measure (one SAR step, or the final
 * lock), 0x010 release (run). In a chain bank: 0x100, 0x2101 (per-chain lock
 * step), 0x010. The firmware waits 10 us and then polls ADIE_SX_DONE after
 * each 0x101 / 0x2101. */
#define ADIE_SX_CMD                     0xb81u
#define ADIE_SX_CMD_PREP                0x00000100u
#define ADIE_SX_CMD_MEASURE             0x00000101u
#define ADIE_SX_CMD_RUN                 0x00000010u
#define ADIE_SX_CMD_WF_LOCK             0x00002101u
#define ADIE_SX_SETTLE_US               10u
#define ADIE_SX_LOCK_TIMEOUT_US         1000u

/* 0xca4  SX status. RO. [static][measured]
 * [1:0] VCO state after a lock: 0 locked, 1 VCO too low (target above the
 *       band's reach), 2 VCO too high. The flag gives up about 5 MHz of LO
 *       before the real edge: meter an edge with a tone, not the flag.
 * [31:20] a band code while the SX runs (MT7976: presumably its internal
 *       search result; on the MT7977 not equal to 0xa01[31:20]).
 * A dormant SX reads 0x00010000 and does not change when a lock is tried
 * (the MT7977 chain-bank copies read 0x20010000 the same way). */
#define ADIE_SX_STATUS                  0xca4u
#define ADIE_SX_STATUS_VCO              HW_GENMASK(1, 0)
#define ADIE_SX_STATUS_VCO_LOCKED       0u
#define ADIE_SX_STATUS_VCO_LOW          1u
#define ADIE_SX_STATUS_VCO_HIGH         2u
#define ADIE_SX_STATUS_B16              HW_BIT(16)
#define ADIE_SX_STATUS_BAND             HW_GENMASK(31, 20)
#define ADIE_SX_STATUS_DORMANT          0x00010000u

/* 0xca5  SAR comparator. RO. [7] = keep the trial bit (MT7977 SAR).
 * Other bits vary (observed 0x22700800, 0x00000800). [static][measured] */
#define ADIE_SX_CMP                     0xca5u
#define ADIE_SX_CMP_KEEP                HW_BIT(7)

/* 0xca7  Measurement/lock done. RO. [16] = done. Observed 0x08010000.
 * [static][measured] */
#define ADIE_SX_DONE                    0xca7u
#define ADIE_SX_DONE_FLAG               HW_BIT(16)

/* 0xcd1  Vtune monitor. RO. [16:12] 5-bit tuning-voltage reading
 * [inferred]; the firmware rewrites the frequency words when it is outside
 * 3..28 even if they did not change [static]. It reads 29..30 at a band's
 * lower edge and about 13 mid-band. [measured] */
#define ADIE_SX_VTMON                   0xcd1u
#define ADIE_SX_VTMON_VT                HW_GENMASK(16, 12)
#define ADIE_SX_VTMON_VT_MIN            3u
#define ADIE_SX_VTMON_VT_MAX            28u

/*
 * Generic lock step (both dies), after the frequency words and band code:
 *   CMD = PREP; CMD = MEASURE; wait 10 us; poll DONE[16] up to 1 ms (the
 *   firmware only counts a timeout); CMD = RUN; read STATUS[1:0] == 0.
 * The MT7977 adds the software SAR and mode words, then a per-chain step on
 * k 3..0 (CMD = PREP, CMD = WF_LOCK, wait, poll DONE in that chain, CMD = RUN).
 */

/* ------------------------------------------------------------------------
 * Mode, loop and preset registers (values per die in mt7976.h / mt7977.h)
 * ------------------------------------------------------------------------ */

/* 0xa00  Observed 0x00000015 in every bank of both die types. On the MT7977
 * SX, setting [1] gives the 3/4 ratio like 0xa40[0]; setting [21], [28] or
 * [31] makes the flag read "too high" everywhere. [measured] */
#define ADIE_SX_A00                     0xa00u
#define ADIE_SX_A00_RATIO34             HW_BIT(1)

/* 0xa03 / 0xa04 / 0xa05  SAR and lock mode words; the MT7977 retune writes
 * fixed values (mt7977.h). 0xa05 [6] flipped: the flag reads "locked" at
 * every target while the LO parks (MT7977, LO +4.09 MHz): a detector fault,
 * not a lock. [static][measured] */
#define ADIE_SX_A03                     0xa03u
#define ADIE_SX_A04                     0xa04u
#define ADIE_SX_A05                     0xa05u
#define ADIE_SX_A05_FALSE_LOCK          HW_BIT(6)

/* 0xa0f  [1:0] = 3 is set on the SX banks during calibrations and at each
 * channel set, then cleared (SX output force-on, inferred). Observed 0.
 * Setting it while streaming had no visible effect. [static][measured] */
#define ADIE_SX_OUT_FORCE               0xa0fu
#define ADIE_SX_OUT_FORCE_ON            HW_GENMASK(1, 0)

/* 0xa40  Ratio / VCO-core select; fields in mt7976.h and mt7977.h. */
#define ADIE_SX_CFG                     0xa40u

/* 0xa55  Die-specific: the MT7977 SDM split field (keep [9:8] = 0), the
 * MT7976 SSB balance trim. See the die headers. */
#define ADIE_SX_A55                     0xa55u
#define ADIE_SX_A55_SPLIT               HW_GENMASK(9, 8)

/* Preset registers the firmware loads only from its frequency tables (no
 * code writes them); values in the die headers. [static] */
#define ADIE_SX_A08                     0xa08u
#define ADIE_SX_A1B                     0xa1bu
#define ADIE_SX_A44                     0xa44u
#define ADIE_SX_A48                     0xa48u
#define ADIE_SX_A4C                     0xa4cu
#define ADIE_SX_A4E                     0xa4eu
#define ADIE_SX_A51                     0xa51u
#define ADIE_SX_A52                     0xa52u
#define ADIE_SX_A57                     0xa57u
#define ADIE_SX_A5A                     0xa5au
#define ADIE_SX_A5B                     0xa5bu
#define ADIE_SX_A63                     0xa63u
/* 0xa64  Write-only or gated: every preset writes 0x00448424, it reads 0.
 * Never verify it by readback. [static][measured] */
#define ADIE_SX_A64                     0xa64u
#define ADIE_SX_A68                     0xa68u	/* 0xa68..0xa6b */
#define ADIE_SX_A69                     0xa69u
#define ADIE_SX_A6A                     0xa6au
#define ADIE_SX_A6B                     0xa6bu
#define ADIE_SX_132                     0x132u
#define ADIE_SX_133                     0x133u
#define ADIE_SX_134                     0x134u
#define ADIE_SX_138                     0x138u
#define ADIE_SX_B84                     0xb84u
#define ADIE_SX_B88                     0xb88u
/* Synthesizer-family words written only by calibration item 0's RF tables
 * (adie/rfinit.h; the comment names them). Role unknown. [static][measured] */
#define ADIE_SX_A07                     0xa07u	/* RF_COMMON, MT7977 RF_INIT */
#define ADIE_SX_A47                     0xa47u	/* RF_COMMON */
#define ADIE_SX_A53                     0xa53u	/* RF_COMMON */
#define ADIE_SX_A54                     0xa54u	/* RF_COMMON */
#define ADIE_SX_B82                     0xb82u	/* MT7976 RF_INIT */
#define ADIE_SX_B86                     0xb86u	/* RF_COMMON */
#define ADIE_SX_B87                     0xb87u	/* RF_COMMON */
#define ADIE_SX_B8D                     0xb8du	/* MT7977 RF_INIT */
#define ADIE_SX_B90                     0xb90u	/* MT7976 RF_INIT */
#define ADIE_SX_B94                     0xb94u	/* MT7977 RF_INIT */
#define ADIE_SX_B95                     0xb95u	/* MT7977 RF_INIT */
#define ADIE_SX_B99                     0xb99u	/* MT7976 RF_INIT */
#define ADIE_SX_BA3                     0xba3u	/* MT7977 RF_INIT */
#define ADIE_SX_BB4                     0xbb4u	/* RF_COMMON */

/* 0xb80  Loop control. RW. Observed 0. [7] is pulsed when the die-0 monitor
 * receiver starts in 5 GHz mode [static]. MT7977 SX: every bit changes the
 * lock flags alike (write-sensitive) [measured]. MT7976: see the restore
 * HAZARD in mt7976.h. */
#define ADIE_SX_B80                     0xb80u
#define ADIE_SX_B80_PULSE7              HW_BIT(7)

/* 0xb83 / 0xb8a / 0xb8b  Loop-filter / charge-pump configuration words; the
 * retunes write fixed values (die headers). MT7977 SX: every bit of 0xb82..
 * 0xb8a and 0xb9c..0xb9f changes the lock flags alike. [static][measured] */
#define ADIE_SX_B83                     0xb83u
#define ADIE_SX_B8A                     0xb8au
#define ADIE_SX_B8B                     0xb8bu

/* 0xbb0  Loop configuration. RW. Observed 0x00640a02 (MT7977 SX),
 * 0x00a40f02 (MT7976). [measured]
 * [31] OPEN: opens the loop. The target is ignored and the VCO parks low:
 * MT7977 band 0xbff -> LO 4555.7 MHz (88 MHz below the closed-loop floor),
 * band 0xbc0 -> about 80 MHz below that code's locked centre; stable to one
 * 19.5 kHz bin over seconds, about 30 dB weaker than a locked LO, with two
 * internal spurs. About 1.9 % below what the charge pump reaches. [measured]
 * MT7977: every bit of 0xbb1..0xbbf stops the lock. [measured] */
#define ADIE_SX_BB0                     0xbb0u
#define ADIE_SX_BB0_OPEN                HW_BIT(31)

/* 0xbcb  MT7977 SX. [5] flipped: false "locked" flag (detector fault, like
 * 0xa05[6]). Observed 0x71000d82. [measured] */
#define ADIE_SX_BCB                     0xbcbu
#define ADIE_SX_BCB_FALSE_LOCK          HW_BIT(5)

/* ------------------------------------------------------------------------
 * TTG PLL: a second fractional-N PLL referenced to the 40 MHz crystal.
 * On die 0 it sits in k 4 and is the DFS/SM monitor receiver's LO; the
 * calibrations also use it as an injected test tone. The MT7977 SX bank has
 * the same registers (routing in mt7977.h). [static][measured]
 * N = mult * F / 40 MHz, INT 9 bits, FRAC 23 bits; mult 2 for a 2.4 GHz
 * target (PLL at 2F, then /2), 1 for 5 GHz. The firmware quantises F to
 * 125 kHz; direct FRAC writes are finer.
 * Measured on die 0 with mult 2: locks for F 2100..2740 MHz (PLL
 * 4.2..5.48 GHz, the whole span observable) with no calibration pulse.
 * On the MT7977 the TTG RX calibrations (phy_cal.h T1{18..23}) compute N =
 * F / 80 MHz (mult 1, reference word P[0x10] = 80 MHz, mt7977.h
 * MT7977_TTG_REF_KHZ): with it every stock TTG word regenerates (die 2,
 * 0x941 = 0x28f00000: N 81.875, 6550 MHz). [static][measured]
 * ------------------------------------------------------------------------ */
#define ADIE_TTG_REF_KHZ                40000u

/* 0x940  [8] gate on (with 0x606[16] = 0); [7:0] written by a cal helper.
 * Observed 0x0000001f. [static][measured] The TTG RX calibrations' route
 * helper (V:e0063f86 MT7976, V:e0067a8e MT7977) clears [8] in every chain
 * bank of the band at each gain step (its flag 1); its flag 0, unused there,
 * sets [8] and writes [7:0] = a level from WM tables (phy_cal.h).
 * [static][measured] */
#define ADIE_TTG_GATE                   0x940u
#define ADIE_TTG_GATE_ON                HW_BIT(8)
#define ADIE_TTG_GATE_LEVEL             HW_GENMASK(7, 0)

/* 0x941  Frequency word. RW. Reset 0x69800000 (N 211). 0x3ceccccc = 2437 MHz
 * with mult 2 (the firmware's monitor channel 6). TTG RX on die 0, LO 2442
 * MHz: 0x3d2ccccc (2447 MHz, DC phase), 0x3c8ccccc (2422 MHz, IQ phase).
 * [static][measured] */
#define ADIE_TTG_FREQ                   0x941u
#define ADIE_TTG_FREQ_INT               HW_GENMASK(31, 23)
#define ADIE_TTG_FREQ_FRAC              HW_GENMASK(22, 0)
#define ADIE_TTG_FREQ_2437MHZ           0x3cecccccu
#define ADIE_TTG_N_Q23(f_hz, mult)                                      \
	((((unsigned long long)(f_hz) * (mult)) << 23) / (ADIE_TTG_REF_KHZ * 1000ull))

/* 0x942  [27:24] = 6 written before the frequency word. Observed 0x2f20000c
 * idle, 0x2620000c running. [static][measured] */
#define ADIE_TTG_CFG                    0x942u
#define ADIE_TTG_CFG_MODE               HW_GENMASK(27, 24)
#define ADIE_TTG_CFG_MODE_RUN           6u

/* 0x943  [0] set before writing 0x941, cleared after (hold). Observed 1
 * idle, 0 running. [static][measured] */
#define ADIE_TTG_HOLD                   0x943u
#define ADIE_TTG_HOLD_B0                HW_BIT(0)

/* 0x944  [31] cleared at start; pulsed by some calibrations (not needed for
 * lock). [static][measured] The TTG RX calibrations in TTG mode 5 (the IQ
 * phase of T1{18..22}) set [31] and write [22:0] = a tone offset word (Q23
 * of N like 0x941's FRAC: 0x100000 = 2.5 MHz on die 0 with mult 2, 0x40000
 * = 2.5 MHz on the MT7977), with k 4's 0x010 [30] (rfmodes.h
 * ADIE_CHAIN_EN_AUX) cleared around the write; in mode 4 (the DC phase, both phases of T1[23])
 * [31] = 0. Observed 0x80100000 (die 0), 0x80040000 (die 2).
 * [static][measured] */
#define ADIE_TTG_CAL                    0x944u
#define ADIE_TTG_CAL_PULSE              HW_BIT(31)
#define ADIE_TTG_CAL_OFFSET             HW_GENMASK(22, 0)

/* 0x606  TTG distribution, per chain bank (MT7976). RW. Observed 0x10000000
 * on die-0 banks, 0 on the MT7977. [static][measured]
 * [31:30] = 3 on the PLL's own bank: PLL on. [17] route the TTG into this
 * chain. [16] set on one target chain by some calibrations, cleared on the
 * PLL bank by the gate-on step. 5 GHz monitor mode writes 0xc0020001 to
 * die-0 k 4.
 * With [17] on WF0..4, [16] on WF0, WF0 in the RX + aux-input state
 * (rfmodes.h) and k 4 0x940[8] set, the TTG appears in WF0 as a tone at
 * F - LO (-84.5 dBFS), plus its 2F output against 2 x LO (-65 dBFS).
 * [measured]
 * TTG RX calibrations, MT7976: per gain step the route helper writes [5:4]
 * = a route select from WM tables (phy_cal.h; 1 in the stock load), then
 * [16] = 0, in every chain bank; the PLL programming (V:e0064292, 2.4 GHz,
 * all chains) then sets [17] on k 0..4 and [16] on every bank. The MT7977
 * route helper writes 0x606 only in its unused flag-0 path (mt7977.h).
 * [static][measured] */
#define ADIE_TTG_ROUTE                  0x606u
#define ADIE_TTG_ROUTE_PLL_ON           HW_GENMASK(31, 30)
#define ADIE_TTG_ROUTE_TO_CHAIN         HW_BIT(17)
#define ADIE_TTG_ROUTE_B16              HW_BIT(16)
#define ADIE_TTG_ROUTE_SEL              HW_GENMASK(5, 4)
#define ADIE_TTG_ROUTE_MON5G            0xc0020001u

/*
 * TTG programming (firmware order, PLL bank):
 *   0x944 &= ~BIT(31); 0x943 |= 1; 0x606 |= 0xc0000000;
 *   0x942[27:24] = 6; 0x941 = INT << 23 | FRAC; 0x943 &= ~1.
 * The TTG RX calibrations' mode 5 (V:e0064292 MT7976; the MT7977 path in
 * mt7977.h) starts with 0x944 |= BIT(31) instead and, 10 us after the
 * routing, adds the offset: k 4 0x010 [30] = 0, 0x944 |= BIT(31), 0x944
 * [22:0] = offset, 0x010 [30] = 1; the MT7976 path ends with TOP 0x000 |=
 * 0x10 (adie.h). [static][measured]
 */

#endif /* HW_ADIE_SX_H */
