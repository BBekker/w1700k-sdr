/* SPDX-License-Identifier: MIT */
/* Entry from the ROM's FW_START: interrupts off, the hand-over state recorded, our trap
 * handler, our stack, then wm_main (which sets the ROM's gp, so ROM routines can be called).
 */
#include "wm.h"

	.section .text.entry, "ax"
	.globl _start
_start:
	csrrci	t0, mstatus, 8			/* MIE off; t0 = mstatus as handed over */
	li	t1, WM_STATUS
	sw	sp, WM_ENTRY_SP(t1)
	sw	ra, WM_ENTRY_RA(t1)
	sw	gp, WM_ENTRY_GP(t1)
	sw	t0, WM_ENTRY_MSTATUS(t1)
	csrr	t0, mtvec
	sw	t0, WM_ENTRY_MTVEC(t1)
	csrr	t0, mie
	sw	t0, WM_ENTRY_MIE(t1)
	sw	a0, WM_ENTRY_A0(t1)
	sw	a1, WM_ENTRY_A1(t1)
	sw	zero, WM_TRAPS(t1)
	la	t0, trap
	csrw	mtvec, t0			/* direct mode: trap is 4-byte aligned */
	li	sp, WM_STACK_TOP
	call	wm_main
park:
	j	park

/* A load or store fault (or a misaligned one), from a mailbox read or write of an address
 * that does not answer: record it and resume after the instruction. An interrupt (ROM routines
 * turn interrupts back on): record it and return with interrupts off. Anything else: record it
 * and park, with WM_STATE_TRAPPED for the host to see.
 */
	.align	2
trap:
	addi	sp, sp, -16
	sw	t0, 0(sp)
	sw	t1, 4(sp)
	sw	t2, 8(sp)
	li	t0, WM_STATUS
	csrr	t1, mcause
	sw	t1, WM_TRAP_CAUSE(t0)
	csrr	t2, mepc
	sw	t2, WM_TRAP_EPC(t0)
	csrr	t2, mtval
	sw	t2, WM_TRAP_TVAL(t0)
	lw	t2, WM_TRAPS(t0)
	addi	t2, t2, 1
	sw	t2, WM_TRAPS(t0)
	bltz	t1, interrupt
	addi	t1, t1, -4			/* causes 4..7: misaligned/faulting load or store */
	li	t2, 3
	bgtu	t1, t2, fatal
	csrr	t1, mepc
	lhu	t2, 0(t1)
	andi	t2, t2, 3
	addi	t1, t1, 2			/* a compressed instruction is 2 bytes */
	li	t0, 3
	bne	t2, t0, 1f
	addi	t1, t1, 2			/* a full one 4 */
1:	csrw	mepc, t1
	lw	t0, 0(sp)
	lw	t1, 4(sp)
	lw	t2, 8(sp)
	addi	sp, sp, 16
	mret
interrupt:
	li	t1, 0x80			/* MPIE: mret leaves interrupts off */
	csrc	mstatus, t1
	lw	t0, 0(sp)
	lw	t1, 4(sp)
	lw	t2, 8(sp)
	addi	sp, sp, 16
	mret
fatal:
	li	t1, WM_STATE_TRAPPED
	sw	t1, WM_STATE_AT(t0)
	j	park
