/* SPDX-License-Identifier: MIT */
/* Our WA: interrupts off, its own stack at the top of the DLM, then the dispatch loop (wa.c).
 * The ROM starts the WM only once the WM, WA and DSP have all been started, so the WA always
 * has an image; ours serves the stream producer's packer.
 */
#include "wa.h"

	.section .text.entry, "ax"
	.globl _start
_start:
	csrci	mstatus, 8
	csrw	mie, zero
	li	sp, WA_MAIN_STACK
	call	wa_main
1:	wfi
	j	1b
