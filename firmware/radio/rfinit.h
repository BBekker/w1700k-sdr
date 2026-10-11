/* SPDX-License-Identifier: MIT */
/* Calibration item 0, the A-die RF init, with the two power-on calibrations MediaTek runs from
 * inside it: T0[0] (R-cal) and T0[1] (RC calibration). */
#ifndef RADIO_RFINIT_H
#define RADIO_RFINIT_H

#include <stdint.h>

/* What MediaTek's calibration-init band loop does for die = band (hw/mt7996/phy_cal.h
 * "Calibration init" step 7, item 0): the D-die RF-interface words and RF_INIT tables (MT7976
 * also RF_COMMON, RF_EPA_COMMON, XPA, XLNA and, on a 7976C, the "dap" tables; MT7977 the variant
 * of die 2's ECO byte), then the R-cal from the EEPROM image and the RC calibration, which
 * measures and writes the RC trim codes of every chain of the die.
 *
 * Needs radio_identify() and the EEPROM image (radio_ee); in MediaTek's order it runs after the
 * band's per-band PHY init and A-die TOP init and before its group loop / channel set. A die
 * whose chip ID is not the board's (die 0 MT7976, dies 1 and 2 MT7977; die 2 also supplies the
 * MT7977 ECO byte) is refused with HAL_FAIL_STATE (arg: the chip ID) and nothing is written; a
 * die out of range with HAL_FAIL_ARG. Makes 778 (MT7976) or 628 (MT7977) A-die transactions
 * and one or two 40 us waits on this unit. */
void rf_init(uint32_t die);

#endif
