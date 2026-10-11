/* SPDX-License-Identifier: MIT */
/* The producer image's first instruction: our WM calls the image at its base. */
	.section .text.entry, "ax"
	.globl entry
entry:
	j producer
