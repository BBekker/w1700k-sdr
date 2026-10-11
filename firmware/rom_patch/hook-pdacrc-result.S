/* SPDX-License-Identifier: MIT */
/* PDA download CRC hook: report a CRC match, so a WM container with edited plaintext loads.
 *
 * SITE. ROM halWfdmaPdaCheckCRC:
 *
 *     008264d0  lui   a5, 0x54001
 *     008264d4  lw    a4, -0x7f8(a5)       ; PDA status
 *     008264d8  c.li  a0, 0x0              ; default FAIL
 *     008264da  bbc   a4, #0x5, 0x008264e8 ; valid bit clear -> return 0
 *     008264de  lw    a0, -0x7ec(a5)       ; the accumulated CRC
 *     008264e2  c.sub a0, a1               ; a1 = 0 from the caller
 *     008264e4  sltiu a0, a0, 0x1          ; return (crc == expected)   <- here
 *     008264e8  ret
 *
 * Zeroing a0 before the copied `sltiu` makes the ROM compute 1 and its own `ret` return it.
 * Only the CRC comparison is defeated: halWfdmaPdaWaitIdle, halWfdmaPdaCheckLength and the
 * valid-bit test above (which skips this instruction) still refuse a download that did not
 * land. The neighbouring 0x008264e2 refuses a redirect (the WM never starts), so the site is
 * exactly this instruction.
 *
 * CLOBBERS a0 (overwritten by the copy) and SCRATCH. ra is untouched: the return is
 * `jalr zero`, and the ROM's `ret` four bytes later reads ra, which this function never saved.
 *
 * Build-supplied: COPY_WORD, COPY_LEN, RET_HI/RET_LO, SCRATCH.
 */

	.text
	.globl	hook_replacement
hook_replacement:
	li	a0, 0

	/* The hooked instruction. */
	.if	COPY_LEN == 2
	.half	COPY_WORD
	.else
	.word	COPY_WORD
	.endif

	/* Back into ROM after the hooked instruction. */
	lui	SCRATCH, RET_HI
	jalr	zero, SCRATCH, RET_LO
