/* SPDX-License-Identifier: MIT */
/* The receive-only channel set of one band: MediaTek's channel flow (WM V:e00d708a, run by the
 * channel-switch handler V:e00b0a74 for UNI 0x34 tag 0) reduced to what reception needs
 * (hw/mt7996/phy_cal.h "Channel set", R1..R7), in MediaTek's order, the synthesizer by TUNE's program.
 *
 * chanset() does, for band b (= die b):
 *   R1 the P block (V:e00d4a1e mode 2: group from the LO, BW class, EPA / ELNA from EEPROM
 *      0x196 + b, WF range); ours: no channel record, no band / group check (V:e00d406c): an
 *      MT7977 band takes any group its synthesizer reaches;
 *   per WF: R2 item 1 on the MT7977 (the per-channel A-die tables, V:e0040fe2), R7 the first
 *      V:e00d2540 pass (gain-step inputs and A-die gain-step fields);
 *   R3 PHY table B (V:e00d9702) with its fields;
 *   R4 the RX front end (V:e00d188e, V:e00d196a, V:e00d1a5e, V:e00d1b72);
 *   R5 the D-die RX setup (V:e00ddd0e) with its A-die part;
 *   R6 ours: the synthesizer by TUNE's program (rfctl.h), not item 10;
 *   per WF: R7 the second V:e00d2540 pass (the per-chain RX gain offsets from the EEPROM).
 * Left out (transmit only, or MediaTek's bookkeeping): TX power (V:e00b5588, V:e00b0a42), the
 * temperature read and the thermal path, item 16, the channel-set calibrations (V:e00d4732),
 * the DPD state read-backs (V:e00d34d2), op 7 / op 8 (stored-calibration write-back; DC offset
 * is a separate module run after the channel set), the T2 TX calibrations (V:e00d6438), the BF /
 * TMAC words of the handler, and the trigger word 0x8301fc00 (never read here). Also left out:
 * the 320 MHz class (P[4] = 4, band 2 only: 640 Msps, which a stream carried only as every other
 * sample, aliased) and what MediaTek writes only at it: the background receiver around the flow
 * (V:e00ad640, V:e00ad4de), item 1's BW320 group keys, table B's 6 GHz key on a 5 GHz channel and
 * WF_COMM +0xc104, the BW320 gain-index selector values, DFE +0xe338 [1:0] = 0, the channel-95
 * A-die words, BN_MODE [8], TMAC TCR0 BW320M_LOC and RATE BWCR's 320 MHz code.
 *
 * Off by default, each behind a flag (the bench decides whether reception needs them):
 *   CHANSET_F_TABLE_A  PHY table A (V:e00d9238) with its fields, before table B;
 *   CHANSET_F_MAC      the handler's MAC words: RMAC SMPS_CTRL (V:e00b1006), the old RATE BWCR,
 *                      band 0's RX stream count and LMAC ARB_SCR (TX / RX disabled) before the
 *                      flow; TMAC TFCR0, RATE BWCR, WFSYS_ON TICKCR0, RMAC CHFREQ, BN +0x8554 /
 *                      +0x8d08 (EDCCA thresholds, written back), ARB_SCR, band 0's BN_BGRX [6:4]
 *                      and the BWCR write-back after it.
 * Band 0 at 80 and 160 MHz is ours, not MediaTek's: its 2.4 GHz rows end at BW40 (table B's keys,
 * the AGC table's two columns). Everything else follows the class as on the other bands (BN_MODE,
 * DFE_F000, D0AC). The analog filter is not the class: the FILTER command sets it. Table B is the
 * 2.4 GHz BW40 column with the bits MediaTek changes between its 5 GHz BW40 and 5 GHz bandwidth
 * columns, where its 2.4 and 5 GHz BW40 values agree: the RX front end words (DFE +0xf020..+0xf030,
 * +0xf130, +0xf160, BN +0x62a0..+0x62c8, ...) depend on the class alone and come over whole; five
 * AGC / CCA threshold words also differ by RF type (BN +0x8010, +0x8200, +0x82d4, +0x8554, WF_COMM
 * +0x0050) and keep their 2.4 GHz bits where they do. The AGC entries continue the 2.4 GHz table's
 * own per-class step.
 *
 * The module keeps MediaTek's WM-RAM state between channel sets as a static: band 0's RX chain
 * count as CHANSET_F_MAC last wrote it to BN_BGRX [6:4].
 *
 * Prerequisites: radio_identify(), the EEPROM in radio_ee, and the bring-up (item 0, the per-band
 * PHY init, the A-die RF init) on this band. Afterwards the SX is locked at the centre and the
 * RX DC-offset calibration (dcoc.h) can run.
 */
#ifndef RADIO_CHANSET_H
#define RADIO_CHANSET_H

#include <stdint.h>

#include "radio_cmd.h"

#define CHANSET_F_TABLE_A	(1u << 0)
#define CHANSET_F_MAC		(1u << 2)
#define CHANSET_F_ALL		(CHANSET_F_TABLE_A | CHANSET_F_MAC)
#define CHANSET_HZ_PER_KHZ	1000u

/* SETUP: the band at LO hz and bandwidth class bw (0..3 = 20 / 40 / 80 / 160 MHz, MediaTek's P[4]:
 * the sample rate; radio_cmd.h RADIO_NBW). No channel numbers: everything the channel set wrote
 * from its channel record follows from hz (lo_plan); the synthesizer is TUNE's program (rfctl_tune)
 * at the exact LO, on the step the SX honours (sx.h ADIE_SX_FRAC_STEP_LSB). The analog filter is
 * not part of it (rx_adie; rfctl.h rfctl_filter). RADIO_OK, RADIO_ERR_ARG (nothing written: a class
 * above 3, an LO the band's synthesizer does not reach, a combination the tables do not cover),
 * RADIO_ERR_ORDER (radio_identify() has not run) or RADIO_ERR_HW (an access failed or the VCO did
 * not lock: radio_failed()). */
uint32_t chanset(uint32_t band, uint64_t hz, uint32_t bw, uint32_t flags);

/* RATE: the band's bandwidth class alone: the flow at its current LO (the last SETUP or TUNE,
 * radio_band) without the synthesizer (R6), so the LO and the analog filter stay.
 * RADIO_ERR_ORDER before a SETUP on the band; else as chanset(). No DCOC: the DC offset depends on
 * the LO and the filter, not on the rate. */
uint32_t chanset_rate(uint32_t band, uint32_t bw);

/* ---------------------------------------------------------------------------------------------
 * Internal: the channel set's context, shared by chanset.c and sx.c.
 * --------------------------------------------------------------------------------------------- */

/* MediaTek's P block (V:e00d4a1e, mode 2) and channel record (the struct V:e00b0a74 hands the
 * channel flow), the fields the receive path uses. */
struct chanset_ctx {
	uint32_t band;		/* P[0x20]; also the A-die (die = band) */
	uint32_t mt7977;	/* the band's A-die is an MT7977 (else an MT7976) */
	uint64_t hz;		/* the LO */
	uint32_t ch;		/* the LO's number on the band type's 5 MHz raster, 0 off it (lo_plan) */
	uint32_t base_khz;	/* record u32[0]: the band type's channel-0 frequency */
	uint32_t khz;		/* P.u32[0xc] = record u32[4] = u32[8]: the LO in kHz */
	uint32_t bw;		/* P[4], record [0x11]: BW class 0..3 */
	uint32_t group;		/* P[0]: V:e00d41f6(khz) */
	uint32_t epa, elna;	/* P[6], P[7]: EEPROM 0x196 + band [1], [0] */
	uint32_t temp;		/* P[8]: temperature class (fixed: see chanset.c) */
	uint32_t fem;		/* P[0x1c]: EPA and EEPROM 0x196 + band [3] */
	uint32_t wf_first, wf_last;	/* V:e00d6232: 0..3, band 2 0..4 */
	uint32_t sec;		/* record [0x10] (V:e00e55b0): the primary 20 MHz channel's index in the
				 * bandwidth; ours always 0 (no primary channel) */
	uint32_t nss_tx, nss_rx;	/* record [0x12], [0x13] */
	uint32_t flags;
};

#endif
