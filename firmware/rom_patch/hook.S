/* SPDX-License-Identifier: MIT */
/* SYSRAM layout hook: give the ICAP capture engine its own 1 MiB of the UMAC SYSRAM pool.
 *
 * SITE. ROM 0x00822b94, the second instruction of halUmacLibPlePseSysramInit (entry 0x00822b92):
 *
 *     00822b92  exec.it 0x0c80          ; CoDense frame push: saves ra/s0/s1
 *     00822b94  shgp a0, 0x18c38        ; g_u2PLEBankSysramNumber = a0   <- here
 *     00822b98  c.mv s0,a2 ; c.mv s1,a3
 *     00822b9c  shgp a2, 0x18c36        ; g_u2PSEBankSysramNumber = a2
 *     00822ba0  c.jal halUmacLibPLEInit (a0, a1)
 *     00822ba8  c.jal halUmacLibPSEInit (s0, s1)
 *
 * The second instruction, not the entry: `shgp` is gp-relative and so position-independent,
 * while the entry's exec.it takes its operand from the CoDense table; and the prologue has
 * already saved ra, so the two calls below need no stack of their own.
 *
 * WHAT IT DOES. halUmacLibSetSysramLayout(1, 2, 0) puts ICAP at pool position 0 (the function
 * returns 1 and stores nothing if two positions are equal), halUmacLibSetWsIcapBankSysramNum
 * gives ICAP its banks, and the init's own PLE/PSE bank-count arguments are rewritten so the
 * three partitions total the 704-bank pool. The hooked instruction stores a0, so the copy runs
 * last, after a0 is rewritten.
 *
 * STATUS WORDS at MARK_HI << 12 (0x0090a000): magic, call count, the incoming a0..a3, and the
 * two calls' results (pre-seeded with -1 so "not reached" and "returned 0" differ).
 *
 * CLOBBERS ra, t0, t1, a5 and the argument registers it rewrites; ra/s0/s1 are already on the
 * stack. a3 is untouched; a1 is restored from the status words. Both callees only touch a0/a5
 * and their gp-relative stores.
 *
 * The build supplies every address as an assembled-in constant (no relocations): COPY_WORD,
 * COPY_LEN, RET_HI/RET_LO, LAYOUT_HI/LO, BANKS_HI/LO, MARK_HI, MARK_MAGIC and the
 * PROFILE_*_BANKS counts.
 */

#define LAYOUT_PLE_POS		1
#define LAYOUT_PSE_POS		2
#define LAYOUT_WSICAP_POS	0

	.text
	.globl	hook_replacement
hook_replacement:
	lui	t0, MARK_HI
	li	t1, MARK_MAGIC
	sw	t1, 0(t0)

	lw	t1, 4(t0)
	addi	t1, t1, 1
	sw	t1, 4(t0)

	sw	a0, 8(t0)
	sw	a1, 12(t0)
	sw	a2, 16(t0)
	sw	a3, 20(t0)

	li	t1, -1
	sw	t1, 24(t0)
	sw	t1, 28(t0)

	/* halUmacLibSetSysramLayout(1, 2, 0): ICAP at SYSRAM position 0. */
	li	a0, LAYOUT_PLE_POS
	li	a1, LAYOUT_PSE_POS
	li	a2, LAYOUT_WSICAP_POS
	lui	t0, LAYOUT_HI
	jalr	ra, t0, LAYOUT_LO
	lui	t0, MARK_HI
	sw	a0, 24(t0)

	/* halUmacLibSetWsIcapBankSysramNum(ICAP banks). */
	li	a0, PROFILE_ICAP_BANKS
	lui	t0, BANKS_HI
	jalr	ra, t0, BANKS_LO
	lui	t0, MARK_HI
	sw	a0, 28(t0)

	/* PLE and PSE counts are this function's own arguments. */
	lw	a1, 12(t0)
	li	a0, PROFILE_PLE_BANKS
	li	a2, PROFILE_PSE_BANKS

	/* The hooked instruction. */
	.if	COPY_LEN == 2
	.half	COPY_WORD
	.else
	.word	COPY_WORD
	.endif

	/* Back into ROM after the hooked instruction. */
	lui	t1, RET_HI
	jalr	zero, t1, RET_LO
