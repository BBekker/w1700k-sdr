/* SPDX-License-Identifier: MIT */
/*
 * MT7996 WM/WA interrupt controller (CIRQ), the firmware's IRQ descriptor
 * table and the unused IRQ5 slot, software interrupts (host -> WM, WM -> WA,
 * system assert), inter-core handshake words, CCIF mailboxes and the
 * CONN_INFRA hardware semaphores.
 *
 * ADDRESS SPACE: MCU bus (WM and WA identical). The CIRQ, EXCP and WA SWI
 * words (0x89xxxxxx) and the semaphores (0x7c07xxxx) are also host-writable
 * through the host's remap windows; this is how the host drives the IRQ5
 * exec primitive (see mcu_dic.h).
 *
 * Confidence tags: [measured] verified on hardware, [static] from firmware/
 * ROM code or driver sources, [inferred].
 */
#ifndef HW_MT7996_MCU_IRQ_H
#define HW_MT7996_MCU_IRQ_H

#include "../hw_bits.h"

/* ========================================================================
 * WM CIRQ (interrupt controller)
 * ======================================================================== */
#define MT7996_MCU_WM_CIRQ_BASE           0x89010000u
/* Selector/identity table: reads 0x43424140... (one index byte per line).
 * HAZARD: do not write it; mask and unmask through the one-bit command
 * registers below. [measured] */
#define MT7996_MCU_CIRQ_SEL(base)         ((base) + 0x40u)
/* Unmask: write 1 << line (one-bit command, WO). [measured] */
#define MT7996_MCU_CIRQ_IRQ_MASK_CLR(base) ((base) + 0x80u)
/* Mask: write 1 << line (one-bit command, WO). [measured] */
#define MT7996_MCU_CIRQ_IRQ_MASK_SET(base) ((base) + 0x90u)
/* Software IRQ trigger, one bit per line. [static] */
#define MT7996_MCU_CIRQ_IRQ_SOFT(base)    ((base) + 0xc0u)
/* Number of the IRQ being dispatched, read by the ROM dispatcher. RO. [static] */
#define MT7996_MCU_CIRQ_IRQ_NUM(base)     ((base) + 0xd8u)
/* External-interrupt unmask and software trigger. [static] */
#define MT7996_MCU_CIRQ_EINT_MASK_CLR(base) ((base) + 0x108u)
#define MT7996_MCU_CIRQ_EINT_SOFT(base)   ((base) + 0x118u)
#define   MT7996_MCU_CIRQ_LINE(n)         HW_BIT(n)

/* WA CIRQ: base inferred from the WM->WA soft-interrupt word at +0xc0. */
#define MT7996_MCU_WA_CIRQ_BASE           0x89090000u	/* [inferred] */

/* ========================================================================
 * Firmware IRQ descriptor table (WM, ULM2; retail ROM data)
 * ======================================================================== */
/* desc[line] = {u32 line, u32 handler, ...}, 32-byte stride. [measured] */
#define MT7996_MCU_IRQ_DESC_TABLE         0x022086dcu
#define MT7996_MCU_IRQ_DESC(line)         (MT7996_MCU_IRQ_DESC_TABLE + 32u * (line))
#define MT7996_MCU_IRQ_HANDLER(line)      (MT7996_MCU_IRQ_DESC(line) + 4u)
/*
 * IRQ5: logical line 5 is mapped to this slot by the retail registration
 * routine and nothing takes it, with or without firmware up. Descriptor
 * 0x0220877c = {5, 0x0080d64c, 0x00815498, 0...}. The handler word is
 * writable from the host. Writing an MCU address there, unmasking line 5
 * and pulsing the host software IRQ makes the WM call that address as a
 * function: entry ra 0x0080a0c0, sp 0x0220dfe0; one assert/deassert = one
 * entry. [measured]
 */
#define MT7996_MCU_IRQ5_LINE              5u
#define MT7996_MCU_IRQ5_DESC              0x0220877cu
#define MT7996_MCU_IRQ5_SLOT              0x02208780u
#define   MT7996_MCU_IRQ5_STOCK_HANDLER   0x0080d64cu	/* retail ROM default */
#define   MT7996_MCU_IRQ5_BIT             0x00000020u	/* for MASK_SET / MASK_CLR */
/*
 * Exec sequence: MASK_SET = 0x20; slot = target; MASK_CLR = 0x20; pulse
 * MT7996_MCU_DBGIF_SW_IRQ bit 0 (assert, hold ~100 us, deassert). Restore:
 * MASK_SET = 0x20; slot = 0x0080d64c; read it back. RESET_MT also restores
 * the slot. [measured]
 * HAZARD: the ROM dispatcher (retail 0x0080a06e) turns MIE ON around the
 * handler call and off after it. Code entered through IRQ5 must clear
 * mstatus.MIE itself for its whole run and restore it on exit. [static]
 * HAZARD: a line left asserted re-enters the handler; always deassert,
 * even after an error. [measured]
 */

/* Firmware interrupt nesting counter: incremented on every trap entry,
 * decremented on exit; 0 in tasks, >= 1 in an ISR. The ROM's
 * save/restore-interrupts takes a pure CSR path (MIE bit 3) when it is
 * nonzero, and the scheduler path (which can yield and set MIE) when it is
 * 0, so our code must run with the count >= 1, i.e. entered through the
 * dispatcher. Trap entry sets gp = 0x02212800. [static] */
#define MT7996_MCU_COS_IRQ_COUNT          0x022002e8u	/* retail */

/* Named firmware IRQ codes (WM) [static] */
#define MT7996_MCU_IRQ_WF_RBIST_TOP       0x37u	/* RBIST / ICAP into the WM */
#define MT7996_MCU_IRQ_THERM              0x3cu
#define MT7996_MCU_IRQ_INST2_RSPI         0x41u
#define MT7996_MCU_IRQ_INST2_THERM        0x42u
#define MT7996_MCU_IRQ_PTP_THERM          0x45u	/* the patch registers a handler here */
#define MT7996_MCU_IRQ_INST3_RSPI         0x4au
#define MT7996_MCU_IRQ_INST3_THERM        0x4bu
#define MT7996_MCU_EINT_RF_DIS            8u	/* WF_RF_DIS external interrupt */

/* ========================================================================
 * Host -> WM software interrupt (CONN_MCU_EXCP_CON, MCU_DBG_IF_SW_IRQ)
 * ======================================================================== */
/* Bit 0 asserts the WM interrupt that dispatches through the IRQ5 slot
 * (and the ROM diagnostic path). Host-writable. RW. [measured] */
#define MT7996_MCU_DBGIF_SW_IRQ           0x8905009cu
#define   MT7996_MCU_DBGIF_SW_IRQ_ASSERT  HW_BIT(0)

/* ========================================================================
 * WM -> WA soft interrupt (the WA entry and handshake)
 * ======================================================================== */
/*
 * The WA handler (vendor 2026 WA at 0xe00009cc) walks {bit, fn} pairs at
 * WA DLM 0x102003b0 and calls fn for each action bit set. Raise: set the
 * action bit in MT7996_MCU_SWDEF_WA_ACTION, then write 0x10 (bit 4) to the
 * WA CIRQ software-IRQ word. The WA clears the action bit and logs progress
 * in MT7996_MCU_SWDEF_WA_TRACE. Action bit 1 has no firmware sender, so its
 * slot is free for our code (the WA calls it as a function, on its interrupt
 * stack). The WA guards the action/SWI words with hardware semaphore 0x11.
 * Handler runtime 2.1 ms. [measured]
 */
#define MT7996_MCU_WA_SWI                 0x890900c0u
#define   MT7996_MCU_WA_SWI_RAISE         0x00000010u
#define MT7996_MCU_WA_SWI_SLOT(bit)       (0x102003b0u + 8u * (bit) + 4u)	/* [inferred] for bit != 1 */
#define   MT7996_MCU_WA_SWI_SLOT1         0x102003bcu
#define   MT7996_MCU_WA_SWI_SLOT1_STOCK   0xe0001470u	/* vendor 2026 WA */
/*
 * HAZARD: raising the WA into our code leaves the firmware command path
 * (host -> WA -> WM) dead until the next firmware load, every time, however
 * the WA was raised or left; afterwards the WM keeps raising the WA every
 * ~0.3 s and the WA re-runs its init. Read results through the BAR while the
 * WM is still held in our code. [measured]
 * HAZARD: after such a hold the host's BAR window can poison (0xdead2117,
 * PCIe completion timeouts); recovery is a power cycle. [measured]
 */

/* ========================================================================
 * WF_SW_DEF shared control block (MCU SYSRAM; host-visible)
 * ======================================================================== */
/* WA trace: last bit << 16 | module << 8 | step. Written by the WA. [measured] */
#define MT7996_MCU_SWDEF_WA_TRACE         0x00401400u
#define   MT7996_MCU_SWDEF_TRACE_BIT      HW_GENMASK(23, 16)
#define   MT7996_MCU_SWDEF_TRACE_MODULE   HW_GENMASK(15, 8)
#define   MT7996_MCU_SWDEF_TRACE_STEP     HW_GENMASK(7, 0)
/* WM -> WA action bitmask (1 << k selects table entry k). At rest 0. [measured] */
#define MT7996_MCU_SWDEF_WA_ACTION        0x00401408u
/* Bit 1, no firmware sender: selects MT7996_MCU_WA_SWI_SLOT1. [measured] */
#define   MT7996_MCU_SWDEF_WA_ACTION_SLOT1 HW_BIT(1)	/* [guess] name */
/* The FWOPMODE word (0x0040143c) and the firmware statistics words are in
 * host.h (MT7996_SWDEF_*). */
/* 0x00401440..0x00401454: published by WM region-0 code, read by WM main
 * code. 0x00401980/0x00401984: written by the WA only (recovery progress).
 * MediaTek's WA writes 0x00401400 (boot trace 0x200 | step) and clears
 * bits 0 and 1 of 0x00401404 from its CRAM init code. [static] */

/* ========================================================================
 * System-assert software interrupts (firmware crash/coredump trigger)
 * ======================================================================== */
/* Vendor driver: write 0x20 to +0x108 then +0x118 of these blocks. [static]
 * HAZARD: deliberately crashes the target firmware; reload required. */
#define MT7996_MCU_WM_ASSERT_BASE         0x89018000u
#define MT7996_MCU_WA_ASSERT_BASE         0x89098000u
#define MT7996_MCU_ASSERT_A(base)         ((base) + 0x108u)
#define MT7996_MCU_ASSERT_B(base)         ((base) + 0x118u)
#define   MT7996_MCU_ASSERT_VAL           0x00000020u

/* ========================================================================
 * CCIF inter-processor mailboxes (per peer) [static]
 * ======================================================================== */
/* Per-peer ISRs bgf2wf / ap2wf (host) / md2wm / wa2wm. 48 channels
 * (id <= 0x2f); an unknown peer id traps (ecall). */
#define MT7996_MCU_CCIF_PEER1_BASE        0x7c019000u
#define MT7996_MCU_CCIF_PEER3_BASE        0x820a1000u
#define MT7996_MCU_CCIF_ACK(base)         ((base) + 0x14u)
#define   MT7996_MCU_CCIF_ACK_CH          HW_GENMASK(7, 0)
#define MT7996_MCU_CCIF_CHANNELS          48u

/* ========================================================================
 * CONN_INFRA hardware semaphores
 * ======================================================================== */
/* Registers: conn.h (MT7996_CONN_SEMA_ACQUIRE/RELEASE). Acquire is a READ,
 * granted when (value & 0xfd) == 1; release writes 1. Known indices:
 * 1 calibration, 12 A-die SPI, 13 eFuse, 14 thermal, 0x11 the WA's
 * soft-IRQ action/SWI words. [static]
 * HAZARD: a host or proxy read of ACQUIRE(12) takes the A-die SPI semaphore
 * and breaks every later firmware RF access (see conn.h). */

#endif /* HW_MT7996_MCU_IRQ_H */
