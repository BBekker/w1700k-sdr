/* SPDX-License-Identifier: MIT */
/* The host test's hal.h: every call of a radio module is checked, in order, against an expected
 * access list, and reads are served from it.
 *
 * Expected list, one access per line (numbers hex, 0x optional; '#' starts a comment):
 *   W addr value        D-die write             R addr value        D-die read (value served)
 *   AW die k idx value  A-die write             AR die k idx value  A-die read (value served)
 *   E addr value        eFuse byte (hal_efuse8), value served
 * k is the bank: 0..4, or f for TOP. A poll (hal_poll/hal_apoll) consumes the R/AR lines of its
 * register up to the first that satisfies it; if none does before another access, it timed out.
 * Delays are not in the list; the mock clock advances by them.
 *
 * mock_open("-") runs free: nothing is checked, every access is printed in the list format, and
 * reads return the register's last written value (0 if never written) unless mock_preset() set one.
 */
#ifndef RADIO_TESTS_MOCK_H
#define RADIO_TESTS_MOCK_H

#include <stdint.h>

#include "../radio.h"

/* Loads the expected list (or "-": free run). Exits on a bad file. */
void mock_open(const char *path);
/* Free run: the value reads of a D-die address / A-die register return until it is written. */
void mock_preset(uint32_t addr, uint32_t value);
void mock_apreset(uint32_t die, uint32_t k, uint32_t idx, uint32_t value);
/* After the module ran: 0 if every expected access was made, else prints where it stopped. */
int mock_finish(void);
/* Loads an EEPROM image into radio_ee; exits on error. */
void mock_eeprom(const char *path);
/* Accesses checked so far. */
extern unsigned long mock_accesses;

#endif
