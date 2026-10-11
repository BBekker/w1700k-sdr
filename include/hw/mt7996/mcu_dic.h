/* SPDX-License-Identifier: MIT */
/*
 * MT7996 MCU debug and access paths: the ROM diagnostic mailbox ("DIC"),
 * its RAM hooks, the MCU debug-interface command words, the pre-firmware
 * download-dispatch pointer, and the firmware REG_ACCESS command together
 * with the one-instruction WM edit that turns it into a memory reader.
 *
 * ADDRESS SPACES: the DIC mailbox is in CONN_INFRA (0x7c05bxxx), written by
 * the host through its remap windows and read by the ROM on the MCU bus.
 * Hook slots and gadgets are MCU-bus addresses in ULM2 / ROM. REG_ACCESS is
 * a firmware command (host -> WA -> WM); its addresses are the WM's own view.
 *
 * ROM addresses are for the retail ROM. Firmware addresses are for the
 * vendor 2026 test-mode WM. Confidence tags: [measured] verified on
 * hardware, [static] from firmware/ROM code, [inferred].
 */
#ifndef HW_MT7996_MCU_DIC_H
#define HW_MT7996_MCU_DIC_H

#include "../hw_bits.h"

/* ========================================================================
 * ROM diagnostic mailbox (DIC)
 * ======================================================================== */
/*
 * Served by the ROM with or without firmware up, including on unpatched
 * vendor firmware. Protocol: host writes ARG0/ARG1 then COMMAND and pulses
 * the host software IRQ (mcu_irq.h, MT7996_MCU_DBGIF_SW_IRQ); the ROM reads
 * command + both args, zeroes both results, runs the handler, clears COMMAND
 * and writes STATUS. Completion = STATUS counter advanced by one. [measured]
 */
#define MT7996_DIC_COMMAND                0x7c05b068u	/* RW */
#define MT7996_DIC_STATUS                 0x7c05b06cu	/* RO to the host */
#define   MT7996_DIC_STATUS_COUNT         HW_GENMASK(3, 0)	/* +1 per command, mod 16 */
#define   MT7996_DIC_STATUS_ERR           HW_GENMASK(7, 4)	/* 0 ok, 7 error/unknown */
#define MT7996_DIC_ARG0                   0x7c05b070u	/* RW */
#define MT7996_DIC_ARG1                   0x7c05b074u	/* RW */
#define MT7996_DIC_RESULT0                0x7c05b078u	/* RO */
#define MT7996_DIC_RESULT1                0x7c05b07cu	/* RO */
/* Commands [static unless noted] */
#define   MT7996_DIC_CMD_READ1            0x1001u
#define   MT7996_DIC_CMD_READ2            0x1002u
#define   MT7996_DIC_CMD_READ4            0x1003u
#define   MT7996_DIC_CMD_PING             0x1008u	/* RESULT0 = RESULT1 = 0x5e5e5e5e [measured] */
#define     MT7996_DIC_PING_VAL           0x5e5e5e5eu
#define   MT7996_DIC_CMD_PING2            0x1009u	/* 0xa1a1a1a1 */
#define   MT7996_DIC_CMD_NOP              0x100du
#define   MT7996_DIC_CMD_HOOK_1201        0x1201u	/* c.jalr *MT7996_DIC_HOOK_1201 */
#define   MT7996_DIC_CMD_HOOK_1202        0x1202u	/* c.jalr *MT7996_DIC_HOOK_1202 */
#define   MT7996_DIC_CMD_HOOK_1203        0x1203u	/* c.jalr *MT7996_DIC_HOOK_1203 */
/* CALL_READ: call fn = ARG0 with a0 = ARG1, publish the return value in
 * RESULT0 (the ROM's DIC_SYSTEM_FUNC_CALL). With ARG0 = the read gadget
 * this is an arbitrary 32-bit MCU read. [measured] */
#define   MT7996_DIC_CMD_CALL_READ        0x1204u
#define   MT7996_DIC_CMD_CALL             0x1205u	/* same shape, result not published */
#define   MT7996_DIC_CMD_PARK             0x1206u	/* parks ARG0 + ARG1 in a 16-byte buffer */
#define   MT7996_DIC_CMD_ASSERT           0x1207u	/* assert/exception */
#define   MT7996_DIC_CMD_HOOK_1208        0x1208u	/* c.jalr *MT7996_DIC_HOOK_1208 */
/*
 * At a hook command the callee sees a0 = ctx, a1 = ARG1, a2 = ARG0,
 * a3 = command. The one-argument limit belongs to 0x1204 alone (its
 * byte-checking helper hands the callee only a0 = ARG1). [static]
 */
/* The four RAM hooks (host-writable function pointers in ULM2): */
#define MT7996_DIC_HOOK_1201              0x02208348u
#define MT7996_DIC_HOOK_1202              0x02200524u
#define MT7996_DIC_HOOK_1203              0x02200520u
#define MT7996_DIC_HOOK_1208              0x022002fcu
#define MT7996_DIC_HOOK_1205_HELPER       0x02200350u
#define MT7996_DIC_PARK_BUF               0x02209b88u	/* 16 bytes */
/*
 * ROM handler table (ULM2), validated live: index 2 0x0080b1ae, 3 0x0080b156
 * (the two register accessors), 5 0x0080b224 (0x1204 byte checker), 6
 * 0x0080b458 (processor), 7 0x0080b1fe (field accessor), 8 0x0080b212
 * (result clear), 10 0x0080b262 (basic dispatcher). The patched firmware
 * points the extended-handler slot (0x02200364) at a stub returning 7. [measured]
 */
#define MT7996_DIC_HOOKS_TABLE            0x02200340u
#define MT7996_DIC_HOOK_ENTRY(i)          (MT7996_DIC_HOOKS_TABLE + 4u * (i))
#define MT7996_DIC_EXT_HANDLER_SLOT       0x02200364u
/* ROM gadgets [measured, retail ROM] */
#define MT7996_DIC_GADGET_LOAD            0x0081c336u	/* c.lw a0,0(a0); ret */
#define MT7996_DIC_ROM_DCACHE_WB          0x008088f2u
#define MT7996_DIC_ROM_DCACHE_INVAL       0x008088d8u
/*
 * Cost: ~9.4 s per word over the full DIC protocol (one word per
 * CALL_READ). [measured]
 * HAZARD: a refused IRQ pulse leaves COMMAND set with no handler and the
 * next run's precondition fails, costing a boot. Clear COMMAND only when
 * the IRQ5 handler is the stock one and COMMAND is 0x1008 or 0x1204. [measured]
 * HAZARD: keep STATUS/RESULT read-only from the host (they are by design,
 * so completion cannot be faked). [static]
 * HAZARD: reading an unmapped MCU address through the gadget may fault the
 * WM; recovery is RESET_MT. [measured]
 */

/* ========================================================================
 * MCU debug interface command words (CONN_MCU_EXCP_CON) [static]
 * ======================================================================== */
#define MT7996_MCU_DBGIF_CMD              0x89050090u
#define MT7996_MCU_DBGIF_ARG0             0x89050094u
#define MT7996_MCU_DBGIF_ARG1             0x89050098u
/* +0x9c is the host software IRQ: MT7996_MCU_DBGIF_SW_IRQ in mcu_irq.h. */

/* ========================================================================
 * Pre-firmware entry: the download-command dispatch pointer
 * ======================================================================== */
/*
 * Four writable words in the ROM's data (retail live values 0x00010000,
 * 0x0083878e, 0x0083868a, 0x0083896e); word 3 is the default
 * download-command handler. Redirecting word 3 and sending an unhandled
 * download command (0xff) makes the ROM call the target as a function
 * before any firmware runs: entry ra 0x00838eae, sp 0x02201e60. [measured]
 */
#define MT7996_MCU_DL_DISPATCH_BLOCK      0x0222b4e0u
#define MT7996_MCU_DL_DISPATCH_HANDLER    0x0222b4ecu	/* word 3 */
#define   MT7996_MCU_DL_TRIGGER_CMD       0xffu
/*
 * HAZARD: read the block live and rewrite only word 3; values from another
 * ROM revision are wrong (relocated). [measured]
 * HAZARD: usable only before firmware is up: the WM owns the mailbox
 * afterwards and the redirect breaks any download still to come. The
 * triggering command never gets a reply (-ETIMEDOUT is the expected
 * result) and the host process may hang in uninterruptible sleep. RESET_MT
 * restores the pointer. [measured]
 */

/* ========================================================================
 * Firmware REG_ACCESS (UNI command 0x0d)
 * ======================================================================== */
/*
 * Protocol (UNI 0x0d, BASIC TLVs, up to 124 per command, refused address =
 * all-zero reply slot): mcu_cmd.h. The stock WM serves only
 * [0x70000000, 0xe0000000): its leaf computes addr + 0x90000000 and refuses
 * anything below 0x70000000 (unsigned), and behind that one check is a plain
 * lw (QUERY) or sw (SET) in the WM's own address view. [static/measured]
 */
/*
 * THE EDIT POINT (2026 test-mode WM): the BASIC leaf at 0xe00b85c4; the
 * range check is the single `bltu a4, a3, 0xe00b8600` at 0xe00b85d0
 * (bytes 63 68 d7 02). Replacing it with `j 0xe00b8600` (6f 00 00 03) makes
 * REG_ACCESS read and write ANY mapped MCU address in the WM's own view,
 * 124 words per command, ~1.4 s per command round trip (~350 B/s). The
 * edited WM loads only behind the PDA-CRC ROM-patch hook (rom_patch.h);
 * otherwise START fails with status 2. [measured]
 */
#define MT7996_REGACC_WM_LEAF             0xe00b85c4u
#define MT7996_REGACC_WM_EDIT_ADDR        0xe00b85d0u
#define   MT7996_REGACC_WM_EDIT_STOCK     0x02d76863u	/* bltu a4,a3,+0x30 */
#define   MT7996_REGACC_WM_EDIT_NEW       0x0300006fu	/* j +0x30 */
/*
 * HAZARD: an unmapped address read through the edited reader is an MCU
 * load and may fault the WM; recovery is RESET_MT. [measured]
 * HAZARD: SET writes its value with no firmware-side restriction; never send
 * OPT_SET without an explicit value (a careless SET once hit the WFSYS
 * software-reset register). [measured]
 */

/* Pre-authentication ROM download command INIT_CMD_ACCESS_REG (0x03):
 * arbitrary register read before firmware; its write path is gated by
 * security word 0x74140024 bit 6 (clear = not fused shut on unprovisioned
 * parts). [static/measured] */
#define MT7996_MCU_INIT_CMD_ACCESS_REG    0x03u

#endif /* HW_MT7996_MCU_DIC_H */
