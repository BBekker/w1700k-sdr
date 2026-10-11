/* SPDX-License-Identifier: MIT */
/* Our WA: where it lives and what it publishes (host/w1700k/wm.py mirrors the status block).
 * Shared by C and assembly (U32 from layout.h); C checks the addresses against the hw/ headers.
 */
#ifndef WA_WA_H
#define WA_WA_H

#include "../stream/layout.h"

#define WA_ENTRY	U32(0x10000000)		/* WA ILM */
#define WA_MAIN_STACK	U32(0x10264000)		/* top of the WA DLM */

/* In the WM mailbox page, after our WM's: the WA reaches it at the same address. */
#define WA_STATUS	U32(0x0090f3e0)
#define WA_MAGIC_AT	0x00			/* WA_MAGIC once the main loop runs */
#define WA_BEATS_AT	0x04			/* main-loop passes / 1024 */
#define WA_CALLS_AT	0x08			/* soft-interrupt actions served */
#define WA_LAST_AT	0x0c			/* the last function called */
/* The core's cache and memory-attribute set-up as the reset left it, before wa_main sets its own. */
#define WA_ENTRY_MCACHE_AT	0x10
#define WA_ENTRY_MMISC_AT	0x14
#define WA_ENTRY_PMACFG0_AT	0x18
#define WA_ENTRY_PMACFG2_AT	0x1c
#define WA_STATUS_WORDS	8

#define WA_MAGIC	U32(0x4157574f)		/* 'OWWA' */

#ifndef __ASSEMBLER__
#include "hw/mt7996/mcu_map.h"
#include "hw/mt7996/probe_page.h"

_Static_assert(WA_ENTRY == MT7996_WA_ILM_BASE && WA_MAIN_STACK == MT7996_WA_DLM_BASE + MT7996_WA_DLM_SIZE,
	       "wa.h and mcu_map.h disagree");
_Static_assert(WA_STATUS == MT7996_PROBE_OWN_WA_STATUS && WA_STATUS + 4 * WA_STATUS_WORDS <= MT7996_PROBE_CTL_BASE,
	       "wa.h and probe_page.h disagree");
#endif

#endif
