/* SPDX-License-Identifier: MIT */
/*
 * MT7996 eFuse controller, eFuse/security shadow (key-hash banks, revocation,
 * pre-authentication write gate), the boot SHA engine and the ECC engine.
 *
 * ADDRESS SPACE: device bus addresses.
 *   0x70021000 eFuse controller (CBTOP range: host reaches it through the L1
 *              window, MCU directly)                    -- see HAZARD below
 *   0x74140000 eFuse / security shadow (CBTOP range, host via L1)
 *   0x820a2000 SHA engine (host via the L2 window; no fixed map entry)
 *   0x80030000 ECC engine (MCU bus; data windows 0x61000000 / 0x61000200)
 * See host.h for the windows.
 *
 * Measured state on a production unit: all three key-hash banks read zero
 * and the pre-auth write gate (0x74140024 bit 6) is clear, so the boot ROM's
 * pre-authentication register writes and scatter writes are honoured. The
 * retail ROM does call the key-hash validation; enforcement therefore rests
 * on these banks. A blank bank alone does not prove non-enforcement, nor a
 * populated one rejection.                                 [measured, static]
 */
#ifndef HW_MT7996_SECURE_H
#define HW_MT7996_SECURE_H

#include "../hw_bits.h"

/* ------------------------------------------------------------------------ */
/* eFuse controller (bus 0x70021000)                                        */
/* ------------------------------------------------------------------------ */
/*
 * HAZARD: EFUSE BURN. NEVER DRIVE THIS CONTROLLER FROM THE HOST. A read and an
 *         irreversible program differ by ONE mode bit (0x40000040 vs
 *         0x400000c0). Read eFuse only through firmware (UNI EFUSE_CTRL,
 *         tag 1, option 0x03 -- mcu_cmd.h). The 0x74140000 shadow is not a
 *         substitute: physical reads go through this controller.     [static]
 *
 * Layout from ROM/patch code (D-die; the A-die eFuse uses the same control
 * layout through its own serial registers):                        [static]
 */
#define MT7996_EFUSE_CTRL_BASE			0x70021000u
#define MT7996_EFUSE_CTRL			0x70021008u
#define   EFUSE_CTRL_AOUT			HW_GENMASK(5, 0)	/* logical-read result index */
#define   EFUSE_CTRL_MODE			HW_GENMASK(7, 6)
#define     EFUSE_CTRL_MODE_LOGICAL_READ	0u
#define     EFUSE_CTRL_MODE_PHYSICAL_READ	1u
#define     EFUSE_CTRL_MODE_PROGRAM		3u	/* [inferred] HAZARD: burns */
#define   EFUSE_CTRL_ADDR			HW_GENMASK(27, 16)	/* 16-byte block address */
#define   EFUSE_CTRL_VALID			HW_BIT(29)
#define   EFUSE_CTRL_KICK			HW_BIT(30)	/* set to start; HW clears when done */
#define   EFUSE_CTRL_PRESERVE			0xb000ff3fu	/* bits kept by the ROM's RMW */
#define   EFUSE_CTRL_CMD_PHYS_READ		0x40000040u	/* KICK | MODE 1 */
#define   EFUSE_CTRL_CMD_PROGRAM		0x400000c0u	/* KICK | MODE 3: NEVER */
/* Write data for a program: four words. */
#define MT7996_EFUSE_WDATA(n)			(0x70021010u + (n) * 4u)	/* n = 0..3 */
/* Read data: four words. */
#define MT7996_EFUSE_RDATA(n)			(0x70021030u + (n) * 4u)	/* n = 0..3 */

/* eFuse map facts used by the project. Logical block 0x2a0 is physical
 * 0x2b0 (FT-recovery gate word, fw_format.h HAZARD). Program rows touched by
 * the patch recovery: 0x3c0/0x3d0/0x3e0; FT check rows 0x270/0x280/0x290.
 *                                                                   [static] */
#define MT7996_EFUSE_FT_GATE_LOGICAL		0x2a0u
#define MT7996_EFUSE_FT_GATE_PHYSICAL		0x2b0u
#define MT7996_EFUSE_BLOCK_BYTES		16u

/* The ROM's eFuse byte reader SYSefuse_Read_efuse8(addr, buf, len) (retail
 * ROM, the address MediaTek's patch calls): 0 ok, 1 error. It dispatches on
 * the address through a section table in ROM RAM (0x02200a74, 17 sections
 * of 20 bytes: u16 base, ..., +0x0c read handler) that the ROM installs at
 * boot and MediaTek's patch replaces: section 0 (0x000..0x3bf) is the
 * D-die eFuse (V:00810eca -> V:00810e40: EFUSE_CTRL = (r & 0xb000ff3f) |
 * addr << 16 | KICK | mode 1 (physical) or 0 (logical), poll KICK, RDATA
 * 0..3); sections 1..16 (0x400.., 0x1000..0x29df) the A-dies' eFuses, over
 * A-die SPI (V:00810d90 -> V:0080ea90 -> V:00810a36: the same command shape
 * on an A-die eFuse controller behind TOP 0x03d / 0x124 / 0x144, semaphore
 * 13). Both paths only ever issue read modes; programming is elsewhere
 * (the patch's FT recovery). Under our patch the ROM's own table is the
 * one installed (read back 2026-10-11). [static][measured] */
#define MT7996_ROM_EFUSE_READ8			0x00810fceu
#define MT7996_ROM_EFUSE_SECTIONS		0x02200a74u
/* eFuse bytes our bring-up reads (adie.h "The ROM patch's TOP init and
 * trims"): the A-die trims and the MT7976 band-1 pulse selector. 0x471 is a
 * byte of die 0's A-die eFuse (the ROM's eFuse section 1, read over A-die
 * SPI as above), not a D-die byte. */
#define MT7996_EFUSE_ADIE_TRIM(die)		((die) == 0 ? 0xba6u : (die) == 1 ? 0x1e7au : 0x127au)
#define MT7996_EFUSE_7976_SEL			0x471u
#define   EFUSE_7976_SEL_BAND1			0x0au	/* == : die 0 pulses band 1 too; TOP 0x4ac [4:0] */

/* ------------------------------------------------------------------------ */
/* eFuse / security shadow (bus 0x74140000)                                 */
/* ------------------------------------------------------------------------ */

/* A 2 KiB window from this base reads without side effects (host via L1).
 *                                                                 [measured] */
#define MT7996_SECURE_BASE			0x74140000u
#define MT7996_SECURE_SIZE			0x00000800u

/* Key revocation. RO (fuse shadow). Bits 2/3/4 revoke key selector 0/1/2.
 *                                                                   [static] */
#define MT7996_SECURE_REVOKE			0x74140020u
#define   SECURE_REVOKE_SEL0			HW_BIT(2)
#define   SECURE_REVOKE_SEL1			HW_BIT(3)
#define   SECURE_REVOKE_SEL2			HW_BIT(4)
/* other bits undecoded */

/* Pre-authentication write gate. RO (fuse shadow). Bit 6 set: the ROM's
 * download-phase INIT_ACCESS_REG refuses writes (status 7). Read clear on the
 * production unit examined.                                [measured, static] */
#define MT7996_SECURE_ACCESS_GATE		0x74140024u
#define   SECURE_ACCESS_GATE_WRITE_LOCK		HW_BIT(6)
/* other bits undecoded */

/* SHA-256 key-hash banks, 32 bytes each, compared against SHA-256 of the
 * patch footer's RSA modulus by the ROM key validation. Read all-zero on the
 * production unit examined.                                [measured, static] */
#define MT7996_SECURE_KEYHASH0			0x74140028u	/* ..0x74140044, selector 0 */
#define MT7996_SECURE_KEYHASH1			0x74140048u	/* ..0x74140064, selector 1 */
#define MT7996_SECURE_KEYHASH2			0x74140180u	/* ..0x7414019c, selector 2 */
#define MT7996_SECURE_KEYHASH_BYTES		32u

/* ------------------------------------------------------------------------ */
/* Boot SHA engine (bus 0x820a2000)                                         */
/* ------------------------------------------------------------------------ */
/*
 * Used by the ROM to hash downloaded images. Read-only observation is safe;
 * semantics beyond the offsets are undecoded.                       [static]
 * HAZARD: the ROM's secure-boot-enable probe WRITES 0x7788 to ENTRY_ADDR(0)
 *         and reads it back (equal = secure boot enabled); a host write there
 *         changes the ROM's verification decision. Never write.    [static]
 */
#define MT7996_SHA_BASE				0x820a2000u
#define MT7996_SHA_ENTRY_ADDR(n)		(0x820a2000u + (n) * 4u)	/* input address slots */
#define MT7996_SHA_ENTRY_LEN(n)			(0x820a2040u + (n) * 4u)	/* input length slots */
#define MT7996_SHA_DIGEST(n)			(0x820a2080u + (n) * 4u)	/* n = 0..7 */
#define MT7996_SHA_CTRL				0x820a2100u
#define MT7996_SHA_ENTRY_VALID			0x820a210cu
#define MT7996_SHA_ONEWAY_LOCK			0x820a2110u
#define MT7996_SHA_SECBOOT_PROBE_VALUE		0x7788u

/* ------------------------------------------------------------------------ */
/* ECC (elliptic-curve) engine (MCU bus 0x80030000, ROM halEcc*)            */
/* ------------------------------------------------------------------------ */
/*
 * MediaTek's WM polls it in every pass of its UMAC N9 ISR (V:e00b921c, the
 * UNI ECC_OPER completion; mac.h): a status read, and when it reads 0 the
 * clear is written and pending ECC requests (none here) are collected.
 * halEccCheckIntStatus: [19:16] != 0 (reported) or [28] / [0] set -> 2; all
 * clear -> 0. Stock status 0. Data windows 0x61000000 / 0x61000200
 * (halEccAccessAxi, byte-swapped words). Not needed by our firmware.
 *                                                         [static, measured]
 */
#define MT7996_ECC_BASE				0x80030000u
#define MT7996_ECC_CFG				0x80030010u	/* [8] interface select (halEccConfIf) */
#define MT7996_ECC_INT_CLR			0x80030100u	/* W 1: clear (halEccClearIntSource) */
#define MT7996_ECC_INT_STS			0x80030200u
#define   ECC_INT_STS_CODE			HW_GENMASK(19, 16)
#define MT7996_ECC_SECIF_RX_STS			0x80030208u	/* [3:0], polled (halEccCheckSecIfRxStatus) */
#define MT7996_ECC_DATA_WIN0			0x61000000u
#define MT7996_ECC_DATA_WIN1			0x61000200u

#endif /* HW_MT7996_SECURE_H */
