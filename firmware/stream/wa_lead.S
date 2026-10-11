/* SPDX-License-Identifier: MIT */
/* The WA's entry from its soft-interrupt table: the packer runs on its own stack at the top of
 * our ILM area, not on the interrupt stack it was called on.
 */
#include "layout.h"

	.section .text.entry, "ax"
	.globl entry
entry:
	mv t0, sp
	li sp, WA_STACK_TOP
	addi sp, sp, -16
	sw t0, 0(sp)
	sw ra, 4(sp)
	call packer
	lw ra, 4(sp)
	lw sp, 0(sp)
	ret
