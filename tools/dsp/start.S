/* SPDX-License-Identifier: MIT */
/* Standalone research image. Interrupts remain disabled throughout.
 * Window register assignments are inferred from the public 2026-03-11
 * mt7996_dsp.bin startup, not established architectural documentation.
 * No vendor instructions or payload are included in the build. */
.section .text.entry,"ax"
.global _start
_start:
    csrci mstatus, 8
    csrw mie, zero
    li t0, 0x7807800
    csrs mstatus, t0
    csrw 0x9dc, zero
    li t0, 0x10000
    csrw 0x9de, t0
    li t0, 0x200000
    csrw 0x9dd, t0
    li t0, 0x210000
    csrw 0x9df, t0
    li t0, 0xc0
    csrw 0x9e0, t0
    li t0, 0x10000
    csrw 0x9f0, t0
    li t0, 0x2000c0
    csrw 0x9e1, t0
    li t0, 0x210000
    csrw 0x9f1, t0
    li t0, 0x400000c0
    csrw 0x9e2, t0
    li t0, 0x80000000
    csrw 0x9f2, t0
    li t0, 0x800002c0
    csrw 0x9e3, t0
    li t0, 0xc0000000
    csrw 0x9f3, t0
    li t0, 0xc00003c0
    csrw 0x9e4, t0
    li t0, -1
    csrw 0x9f4, t0
    li t0, 0x1f
    csrw 0x9c0, t0
    li t0, 0x37fc
    csrw 0x7c0, t0
    fence.i
    li sp, 0x210000
    la t0, trap
    csrw mtvec, t0
    li t0, 0x1f
    csrs 0xbc0, t0
    csrw fcsr, zero
    call dsp_main
1:  j 1b

.balign 4
trap:
    /* No nested interrupts. Only guarded_probe's single lw is recoverable. */
    addi sp, sp, -16
    sw t0, 0(sp)
    sw t1, 4(sp)
    li t0, 0x200000
    csrr t1, mcause
    sw t1, 24(t0)
    csrr t1, mtval
    sw t1, 32(t0)
    csrr t1, mepc
    sw t1, 28(t0)
    la t0, probe_load
    bne t0, t1, fatal
    addi t1, t1, 4
    csrw mepc, t1
    lw t0, 0(sp)
    lw t1, 4(sp)
    addi sp, sp, 16
    mret
fatal:
    li t0, 0x200000
    li t1, 0xbad00001
    sw t1, 40(t0)
2:  j 2b

.global guarded_probe
guarded_probe:
    mv t2, a0
    li a0, 0
probe_load:
    lw a0, 0(t2)
    ret
