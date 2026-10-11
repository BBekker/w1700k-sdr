/* SPDX-License-Identifier: MIT */
/* The PHY init of the cold bring-up outside the calibrations, as MediaTek's WM runs it around its
 * calibration init (decoded against its cold stock load: hw/mt7996/phy.h "Per-band PHY init",
 * phy_cal.h "Calibration init", hw/adie/adie_init.h), reduced to what reception needs.
 *
 * radio.c calls them in this order, after radio_identify() and the ROM patch's radio init:
 *   phy_common_init()   the FEM / antenna-switch pin mux of dies 0, 1, 2 (MediaTek: the PHY-init
 *                       routine before its calibration request)
 *   phy_band_init(b)    b = 0, 1, 2, each before die b's RF init (item 0): the per-band PHY words,
 *                       the TOP init, the WFSYS_ON PLL clock-gate pulse, the per-chain table pages
 *                       and the register tables
 *   phy_post_init()     after every die's RF init and calibrations: per band chain 0xe90 [30],
 *                       the FEM words TOP 0xb08 / 0x664 / 0x668, the crystal trim (die 2)
 * Each entry reads the chip SKU strap (MT7996_CBTOP_PAD_GPIO) first and refuses a chip it has no
 * decoded path for with hal_fail(HAL_FAIL_STATE, BANDINIT_FAIL_*) before any other access: band
 * types (AdieSel, from the strap and die 2's ECO byte, hw/mt7996/phy_init.h) other than the dies'
 * chip IDs, or EEPROM / strap branches MediaTek's decoded load did not take. A bad band argument
 * is hal_fail(HAL_FAIL_ARG, band). radio_identify() must have run (radio_id).
 */
#ifndef RADIO_BANDINIT_H
#define RADIO_BANDINIT_H

#include <stdint.h>

/* hal_fail(HAL_FAIL_STATE, arg) codes */
#define BANDINIT_FAIL_ID		0x01u	/* radio_identify() has not run */
#define BANDINIT_FAIL_SKU_FEM		0x02u	/* strap SKU [0]: the FEM words' other path (V:e00cd44e) */
#define BANDINIT_FAIL_SKU_XTAL		0x03u	/* strap SKU [3]: the crystal trim on die 0 (V:e012f064) */
#define BANDINIT_FAIL_ADIE(band)	(0x10u + (band))	/* AdieSel not the die's chip, or no die */
#define BANDINIT_FAIL_E90(band)		(0x20u + (band))	/* EEPROM 0x199 + band [3]: V:e00e2314's other path */

void phy_common_init(void);
void phy_band_init(uint32_t band);
void phy_post_init(void);

#endif
