/* SPDX-License-Identifier: MIT */
/* FW_START mask hook: the ROM starts the cores once the WM and the WA have had their FW_START,
 * without a DSP image, so none needs to be loaded.
 *
 * SITE. fwDownloadWifiStart (retail 0x00838c7a), after a FW_START was accepted, records its RAM
 * type and compares the record with the required mask (WM | WA | DSP, set before any patch is
 * applied):
 *
 *     00838d5c  lhgp  a3, 0x18d3e          ; the downloaded mask
 *     00838d60  c.li  a5, 0x1
 *     00838d62  sll   s0, a5, s0           ; this FW_START's type bit
 *     00838d66  c.or  s0, a3
 *     00838d68  bfoz  s0, s0, #0xf, #0x0
 *     00838d6c  shgp  s0, 0x18d3e          ; store it                    <- here
 *     00838d70  lhugp a4, 0x18d40          ; the required mask
 *     00838d74  bne   a4, s0, ...          ; not all yet: reply and wait
 *
 * Setting bit 2 (the DSP's) before the copied store makes WM + WA complete the set.
 *
 * CLOBBERS SCRATCH only; s0 gains bit 2, which the ROM then compares and stores, as intended.
 *
 * Build-supplied: COPY_WORD, COPY_LEN, RET_HI/RET_LO, SCRATCH.
 */

	.text
	.globl	hook_replacement
hook_replacement:
	ori	s0, s0, 4

	/* The hooked instruction. */
	.if	COPY_LEN == 2
	.half	COPY_WORD
	.else
	.word	COPY_WORD
	.endif

	/* Back into ROM after the hooked instruction. */
	lui	SCRATCH, RET_HI
	jalr	zero, SCRATCH, RET_LO
