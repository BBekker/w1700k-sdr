/* SPDX-License-Identifier: MIT */
/* pack_words(src, dst, bytes): `bytes` (a multiple of 64) of raw beats to 7/8 of that in the
 * lean 28-bit layout. Of each 8 words w0..w7 (14-bit I in [29:16], Q in [13:0], spare bits
 * clear), w7's 28 bits go into the spare bits of w0..w6:
 * o_k = w_k | ((w7 << (14 - 2k)) & 0xc000c000).
 * Two groups per pass, list-scheduled so a memory op pairs with an ALU op: from cold
 * pool into local memory, 7.0k cycles per 8 KiB on the WM against 8.1k for the group-at-a-time
 * loop; 9.8k against 10.0k on the WA. Caller-saved registers only.
 */
	.text
	.globl pack_words
pack_words:
	li a7, 0xc000c000
	add t5, a0, a2		/* a2 is free after this: the second group's w7 */
1:	lw a4, 28(a0)
	lw a2, 60(a0)
	slli a5, a4, 14
	lw t0, 0(a0)
	slli a6, a2, 14
	lw t3, 32(a0)
	and a5, a5, a7
	lw t1, 4(a0)
	and a6, a6, a7
	lw t4, 36(a0)
	or t0, t0, a5
	sw t0, 0(a1)
	or t3, t3, a6
	sw t3, 28(a1)
	slli a3, a4, 12
	lw t2, 8(a0)
	slli a6, a2, 12
	lw t6, 40(a0)
	and a3, a3, a7
	lw t0, 12(a0)
	and a6, a6, a7
	lw t3, 44(a0)
	or t1, t1, a3
	sw t1, 4(a1)
	or t4, t4, a6
	sw t4, 32(a1)
	slli a5, a4, 10
	lw t1, 16(a0)
	slli a6, a2, 10
	lw t4, 48(a0)
	and a5, a5, a7
	and a6, a6, a7
	or t2, t2, a5
	sw t2, 8(a1)
	or t6, t6, a6
	sw t6, 36(a1)
	slli a3, a4, 8
	lw t2, 20(a0)
	slli a6, a2, 8
	lw t6, 52(a0)
	and a3, a3, a7
	and a6, a6, a7
	or t0, t0, a3
	sw t0, 12(a1)
	or t3, t3, a6
	sw t3, 40(a1)
	slli a5, a4, 6
	lw t0, 24(a0)
	slli a6, a2, 6
	lw t3, 56(a0)
	and a5, a5, a7
	and a6, a6, a7
	or t1, t1, a5
	sw t1, 16(a1)
	or t4, t4, a6
	sw t4, 44(a1)
	slli a3, a4, 4
	slli a6, a2, 4
	and a3, a3, a7
	and a6, a6, a7
	or t2, t2, a3
	sw t2, 20(a1)
	or t6, t6, a6
	sw t6, 48(a1)
	slli a5, a4, 2
	slli a6, a2, 2
	and a5, a5, a7
	and a6, a6, a7
	or t0, t0, a5
	sw t0, 24(a1)
	or t3, t3, a6
	sw t3, 52(a1)
	addi a0, a0, 64
	addi a1, a1, 56
	bltu a0, t5, 1b
	ret
