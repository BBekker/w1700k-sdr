/* SPDX-License-Identifier: MIT */
/* RX DC-offset calibration (DCOC) of one band at the LO it is set to: MediaTek's calibration
 * mask bits 8 (fine) and 9 (coarse per gain step), phyCalDo T1[0, 2, 4] and T1[1, 3, 5]
 * (hw/mt7996/phy_cal.h MT7996_PHY_CALMASK_RX_DCOC / _RX_DCOC2; registers hw/adie/adie.h "RX
 * DC-offset calibration (DCOC)", hw/adie/dcoc.h).
 *
 * MediaTek calibrates every channel group at power-on, stores the codes and writes them back at
 * each channel set. We instead calibrate after each SETUP, or on the DCOC command, at the LO and
 * analog filter actually set (the LO locked, the filter code in the A-die): no group loop, no
 * stored records, no write-back. The codes the A-die engines leave in the result rows (adie.h ADIE_DCOC_FI* /
 * ADIE_DCOC_FQ* fine, ADIE_DCOC_SI* / ADIE_DCOC_SQ* coarse) stay there.
 *
 * Per chain WF of the band (4 / 4 / 5): our save of exactly the registers the two items change
 * (hw/adie/dcoc.h ADIE_DCOC_SAVE_*, plus WF_COMM +0xd4d0), then the fine item and the coarse
 * per-step item, each followed by MediaTek's calibration tail with our restore in place of its
 * bit-3 backup restore. Codes that fail the range check are replaced by the item's defaults, as
 * MediaTek's write-back does (dcoc_fail). EEPROM 0x1ae [1] / [0] skip the fine / coarse item.
 *
 * Prerequisites: radio_identify(), the EEPROM image, the band's bring-up and a channel set on it
 * (chanset.h): the synthesizer locked and the RX bandwidth class written to the chains. Takes
 * about 890 / 1,420 / 1,770 A-die transactions on band 0 / 1 / 2 plus the engines' poll reads
 * (~0.1 s), and per item a band gate and PHY reset pulse (about 20 us each). A save read that
 * fails stops the run before that chain is touched (RADIO_ERR_HW).
 */
#ifndef RADIO_DCOC_H
#define RADIO_DCOC_H

#include <stdint.h>

#include "radio.h"
#include "radio_cmd.h"

/* RADIO_OK; RADIO_ERR_ARG (band out of range, nothing done); RADIO_ERR_ORDER (radio_identify()
 * has not run); RADIO_ERR_HW (the band's A-die is not the board's: HAL_FAIL_STATE, arg = its chip
 * ID, nothing done; or an access or poll failed during the run: the failure record). */
uint32_t dcoc_cal(uint32_t band);

/* Per band, from its last dcoc_cal(): one bit per chain and item whose codes failed the range
 * check, so that the item's defaults were written instead (fine: mid-scale, coarse: 0). */
extern uint32_t dcoc_fail[RADIO_BANDS];
#define DCOC_FAIL_FINE(wf)	(1u << (2u * (wf)))
#define DCOC_FAIL_STEP(wf)	(2u << (2u * (wf)))

#endif
