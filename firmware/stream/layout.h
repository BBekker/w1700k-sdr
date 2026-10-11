/* SPDX-License-Identifier: MIT */
/* Fixed addresses and protocol words of the stream firmware, as each core sees them (the WM and
 * the WA reach every address below at the same value). The host side
 * (host/w1700k/stream.py, host/w1700k/rf.py) mirrors the mailbox words, which are named in
 * hw/mt7996/probe_page.h; the registers the stream uses are in the other hw/ headers.
 *
 * Shared by C and assembly: U32() adds the unsigned suffix only in C. The hw/ headers' constants
 * carry that suffix, so only C sees them: a value assembly needs is spelled out and checked
 * against its header name.
 */
#ifndef STREAM_LAYOUT_H
#define STREAM_LAYOUT_H

#ifdef __ASSEMBLER__
#define U32(x) x
#else
#define U32(x) x##u
#endif

/* The capture ring: ICAP0 writes the first RING bytes of its 1 MiB SYSRAM partition (the ROM
 * patch's layout hook puts it at pool position 0, MT7996_MCU_POOL_BASE). */
#define RING		U32(0x100000)
#define SAFETY		U32(0x20000)		/* overrun when the capture gets this close to the reader */

/* The WA packer's own stack, below the shared block (wa_lead.S). */
#define WA_STACK_TOP	U32(0x1001b400)

#ifndef __ASSEMBLER__
#include "hw/mt7996/mcu_periph.h"
#include "hw/mt7996/probe_page.h"

_Static_assert(WA_STACK_TOP == MT7996_PROBE_WA_STACK_TOP, "WA_STACK_TOP");

/* WM producer: our WM links it in ULM1 (firmware/own.py); its mailbox page: */
#define DONE_AT		MT7996_PROBE_WM_DONE	/* {DONE_MAGIC, status}, written last */
#define DONE_MAGIC	MT7996_PROBE_DONE_MAGIC	/* 'PROB' */
#define GO_ADDR		MT7996_PROBE_WM_GO	/* the host's go word, then its stop word */
#define GO_MAGIC	MT7996_PROBE_GO_MAGIC	/* 'GOGO' */
#define STOP_MAGIC	MT7996_PROBE_STOP_MAGIC	/* 'STOP' */
#define RES_ADDR	MT7996_PROBE_WM_RES	/* struct res */

/* Packed-chunk buffers in each packer's own local memory: the WM's patch SRAM (free part of the
 * mailbox page and the gap below the image) and the WA's DLM between its data and its stack. */
#define WM_BUFS		MT7996_PROBE_WM_PACKBUF(0), MT7996_PROBE_WM_PACKBUF(1), \
			MT7996_PROBE_WM_PACKBUF(2), MT7996_PROBE_WM_PACKBUF(3)
#define WM_NBUF		MT7996_PROBE_WM_PACKBUF_COUNT
#define WA_BUF		MT7996_PROBE_WA_PACKBUF(0)
#define WA_NBUF		MT7996_PROBE_WA_PACKBUF_COUNT

/* WA packer: code in the WA ILM tail, its own stack below the shared block, entered through
 * bit 1 of the WM->WA soft interrupt (MT7996_MCU_WA_SWI_SLOT1 holds that bit's handler; the WM
 * raises it by writing MT7996_MCU_SWDEF_WA_ACTION and MT7996_MCU_WA_SWI). */
#define WA_CODE		MT7996_PROBE_WA_CODE
#define SHARED		MT7996_PROBE_WA_RES	/* struct shared, uncached for both cores */

/* Timing, in 720 MHz core cycles. */
#define GO_CYCLES	(5u * MT7996_MCU_CLOCK_HZ)	/* 5 s for the host's go word */
#define SPIN		2000000u		/* polls for TX0 to drain at the end */
#define HOLD_CYCLES	(3u * MT7996_MCU_CLOCK_HZ)	/* 3 s: WM held after a WA run, see producer.c */
#define PIECES		4u			/* a WM pack is split so TX0 stays fed */
#endif

#endif
