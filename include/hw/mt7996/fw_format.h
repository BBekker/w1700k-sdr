/* SPDX-License-Identifier: MIT */
/*
 * MT7996 firmware container formats (ROM patch and RAM images).
 *
 * This is a FILE format, not a register block: offsets below are byte offsets
 * into a container file as shipped, and "destination" values are MCU (WM) bus
 * addresses carried inside the container. Nothing here is host-BAR relative.
 *
 * Bring-up needs four containers, downloaded in this order, each RAM type with
 * its own FW_START_REQ (see mcu_cmd.h for the commands):
 *
 *   1. ROM patch  -- big-endian header at the FRONT, one INFO section to patch
 *                    SRAM; bracketed by PATCH_SEM_CONTROL get/release and
 *                    PATCH_FINISH_REQ.                                [measured]
 *   2. WM RAM     -- little-endian trailer at the END, 9 regions; start with
 *                    option OVERRIDE (entry from the feature-bit-5 region).
 *   3. DSP RAM    -- 6 regions (one image loaded once per band, three
 *                    byte-identical ITCM/DTCM pairs); start option
 *                    OVERRIDE | WORKING_PDA_DSP.
 *   4. WA RAM     -- 3 regions; start option OVERRIDE | WORKING_PDA_CR4.
 *
 * FW_STATE (host.h) reads 1 (FW_DOWNLOAD) through steps 1..3 and 7 (RDY) after
 * the WA start.                                                     [measured]
 *
 * The ROM starts no core until every RAM type has had its FW_START: its
 * handler (retail fwDownloadWifiStart 0x00838c7a) sets bit (option >> 2) & 3
 * (0 WM, 1 WA, 2 DSP) in a downloaded mask (u16 at 0x0222b53e) and compares
 * it with a required mask (u16 at 0x0222b540) that ROM 0x00839446 ORs with 7.
 * Every FW_START but the completing one is answered at once; on the
 * completing one the ROM keeps its sequence (u8 at 0x0222b588), goes to state
 * 2 (FW_STATE 2) and starts the cores, and the started firmware sends the
 * reply: the download event routine (ROM 0x0083891c, (seq, status)). The WM
 * entry (region with feature bit 5, else 0x00918000) is kept at 0x0222b4fc.
 * The required mask is set before any patch applies; a redirect of the
 * downloaded-mask store (0x00838d6c, shgp s0) can add the DSP's bit, and then
 * no DSP image is needed (firmware/rom_patch/hook-start-mask.S). The ROM
 * starts only the WM; MediaTek's WM starts the WA through
 * MT7996_MCU_CFG_WM_WA (mcu_periph.h).                    [measured]
 *                                                        [static, measured]
 * All three processors (WM, WA, DSP) are RISC-V; "CR4" is a legacy name.
 *
 * Encryption: connac3 RAM regions are AES-128-CBC with a 16-byte all-zero key
 * and an all-zero IV, the IV reset per region (not chained), over the region
 * MINUS its last 16 bytes, which are a plaintext trailer sent to the device
 * unchanged. The ROM patch section is encrypted whole with no plaintext
 * trailer. The device decrypts; the host sends ciphertext as stored.  [static]
 */
#ifndef HW_MT7996_FW_FORMAT_H
#define HW_MT7996_FW_FORMAT_H

#include "../hw_bits.h"

/* ------------------------------------------------------------------------ */
/* Common crypto parameters                                                 */
/* ------------------------------------------------------------------------ */

/* AES-128-CBC, key = 16 x 0x00, IV = 16 x 0x00, IV reset per region. [static] */
#define MT7996_FW_AES_KEY_BYTES			16u
#define MT7996_FW_AES_BLOCK_BYTES		16u
/* Plaintext tail at the end of every RAM region; not part of the ciphertext,
 * downloaded with the region (region length includes it).           [static] */
#define MT7996_FW_RAM_REGION_TAIL_BYTES		16u

/* ------------------------------------------------------------------------ */
/* RAM container (WM / WA / DSP)                                            */
/* ------------------------------------------------------------------------ */
/*
 * File layout, front to back:
 *   [region 0 payload][region 1 payload]...      concatenated, descriptor order
 *   [release-information block]                  metadata, never downloaded
 *   [n_region x 40-byte region descriptors]
 *   [36-byte common trailer]                     last 36 bytes of the file
 * All multi-byte fields little-endian. Every observed image has format_ver 2,
 * format_flag 1.                                                    [static]
 */

/* Common trailer: last MT7996_FW_RAM_TRAILER_BYTES of the file. */
#define MT7996_FW_RAM_TRAILER_BYTES		36u
#define MT7996_FW_RAM_TRL_CHIP_ID		0u	/* u8 */
#define MT7996_FW_RAM_TRL_ECO_CODE		1u	/* u8 */
#define MT7996_FW_RAM_TRL_N_REGION		2u	/* u8, 3 (WA), 6 (DSP), 9 (WM) */
#define MT7996_FW_RAM_TRL_FORMAT_VER		3u	/* u8, 2 observed */
#define MT7996_FW_RAM_TRL_FORMAT_FLAG		4u	/* u8, 1 observed */
#define   MT7996_FW_RAM_FORMAT_FLAG_RELEASE_INFO	HW_BIT(0)	/* release block present */
#define MT7996_FW_RAM_TRL_FW_VER		7u	/* char[10], not NUL-terminated */
#define MT7996_FW_RAM_TRL_FW_VER_BYTES		10u
#define MT7996_FW_RAM_TRL_BUILD_DATE		17u	/* char[15], e.g. "20260311120504" */
#define MT7996_FW_RAM_TRL_BUILD_DATE_BYTES	15u
/* le32 standard CRC-32 (reflected 0x04c11db7, init ~0, final xor ~0) over the
 * whole file except these last four bytes. Corruption check only, not
 * authenticity.                                                     [static] */
#define MT7996_FW_RAM_TRL_CRC32			32u
#define MT7996_FW_RAM_FORMAT_VER_CONNAC3	2u

/* Region descriptor, 40 bytes; descriptor i is at
 * file_size - 36 - (n_region - i) * 40. Payload of region i starts at the sum
 * of the lengths of regions 0..i-1.                                 [static] */
#define MT7996_FW_RAM_REGION_DESC_BYTES		40u
#define MT7996_FW_RAM_REGION_DESC_OFFSET(file_size, n, i) \
	((file_size) - MT7996_FW_RAM_TRAILER_BYTES - \
	 ((n) - (i)) * MT7996_FW_RAM_REGION_DESC_BYTES)
#define MT7996_FW_RAM_RGN_DECOMP_CRC		0x00u	/* le32, 0 in every image seen */
#define MT7996_FW_RAM_RGN_DECOMP_LEN		0x04u	/* le32, 0 = not compressed */
#define MT7996_FW_RAM_RGN_DECOMP_BLK_SZ		0x08u	/* le32 */
#define MT7996_FW_RAM_RGN_ADDR			0x10u	/* le32 MCU destination */
#define MT7996_FW_RAM_RGN_LEN			0x14u	/* le32 bytes sent, tail included */
#define MT7996_FW_RAM_RGN_FEATURE_SET		0x18u	/* u8, see below */
#define MT7996_FW_RAM_RGN_TYPE			0x19u	/* u8 */
/* bytes 0x0c..0x0f and 0x1a..0x27 reserved */

/* Region feature_set byte.                                          [static] */
#define FW_FEATURE_SET_ENCRYPT			HW_BIT(0)	/* AES-CBC as above */
#define FW_FEATURE_SET_KEY_IDX			HW_GENMASK(2, 1)	/* -> DL mode bits 2:1 */
#define FW_FEATURE_SET_ENCRY_MODE		HW_BIT(4)	/* -> DL mode bit 6 (connac3 mode) */
/* Entry override: the start command's entry address is this region's
 * destination; when several regions carry it the last one wins. WM region 0
 * observed with feature 0x21, all other regions of all images 0x01. */
#define FW_FEATURE_SET_OVERRIDE_ADDR		HW_BIT(5)
/* Generic connac "do not download" flag; the MT7996 loop does NOT honour it. */
#define FW_FEATURE_SET_NON_DL			HW_BIT(6)

/*
 * Release-information block, between the end of the last payload and the
 * first descriptor (36..160 bytes observed):
 *   16 x '#', le32 body_len, body[body_len], 16 x '#'
 * Body is a sequence of records:
 *   le16 content_len, u8 pad_len, u8 tag, content[content_len], pad[pad_len]
 * Record length = 4 + content_len + pad_len. Tag 1 carries build /
 * project strings, tag 3 "CMD_VER:xxxx.xxxx.xxxx.xxxx". A 36-byte block has an
 * empty body. Not downloaded.                              [static, inferred]
 */
#define MT7996_FW_RAM_RELINFO_MARK		0x23u	/* '#' */
#define MT7996_FW_RAM_RELINFO_MARK_BYTES	16u
#define MT7996_FW_RAM_RELINFO_HDR_BYTES		20u	/* 16 marks + le32 body_len */
#define MT7996_FW_RAM_RELINFO_TAG_BUILD		1u
#define MT7996_FW_RAM_RELINFO_TAG_CMD_VER	3u

/* Observed region destinations (MCU bus addresses).                 [static] */
/* WM: 9 regions into 0x0040xxxx (WF MCU SYSRAM), 0x0091xxxx, 0x0220e800 and
 * 0xe0xxxxxx; entry (override) = region 0's address, e.g. 0x00916400. */
/* WA: 0x10000000, 0x10200000, 0xe0000000; ELF entry 0x10000000. */
/* DSP: core b ITCM 0x86000000 + b * 0x400000 and DTCM 0x86200000 + b * 0x400000,
 * b = 0..2; the three pairs carry identical bytes. */
#define MT7996_FW_DSP_ITCM_DEST(b)		(0x86000000u + (b) * 0x400000u)
#define MT7996_FW_DSP_DTCM_DEST(b)		(0x86200000u + (b) * 0x400000u)
#define MT7996_FW_WA_DEST_ILM			0x10000000u
#define MT7996_FW_WA_DEST_DLM			0x10200000u

/* ------------------------------------------------------------------------ */
/* ROM patch container                                                      */
/* ------------------------------------------------------------------------ */
/*
 * Layout: 96-byte header, then n_region x 64-byte section descriptors, then
 * section data at the descriptors' file offsets. ALL header and descriptor
 * words are BIG-endian. Field-for-field identical to the connac2 patch
 * header. Neither checksum field is verified by the reference host loader.
 *                                                                   [static]
 */
#define MT7996_FW_PATCH_HDR_BYTES		96u
#define MT7996_FW_PATCH_HDR_BUILD_DATE		0x00u	/* char[16], e.g. "20240726161535a" */
#define MT7996_FW_PATCH_HDR_BUILD_DATE_BYTES	16u
#define MT7996_FW_PATCH_HDR_PLATFORM		0x10u	/* char[4], "ALPS" in every file */
#define MT7996_FW_PATCH_HDR_PLATFORM_BYTES	4u
#define MT7996_FW_PATCH_HDR_HW_SW_VER		0x14u	/* be32 */
#define MT7996_FW_PATCH_HDR_PATCH_VER		0x18u	/* be32 */
#define MT7996_FW_PATCH_HDR_CHECKSUM		0x1cu	/* be16, algorithm unverified */
#define MT7996_FW_PATCH_HDR_DESC_PATCH_VER	0x20u	/* be32 */
#define MT7996_FW_PATCH_HDR_DESC_SUBSYS		0x24u	/* be32 */
#define MT7996_FW_PATCH_HDR_DESC_FEATURE	0x28u	/* be32 */
#define MT7996_FW_PATCH_HDR_DESC_N_REGION	0x2cu	/* be32, 1 observed */
#define MT7996_FW_PATCH_HDR_DESC_CRC		0x30u	/* be32, unverified */
/* 0x34..0x5f reserved (11 words) */
#define MT7996_FW_PATCH_PLATFORM_ALPS		0x414c5053u	/* "ALPS" as be32 */

/* Section descriptor i at 96 + i * 64. */
#define MT7996_FW_PATCH_SEC_BYTES		64u
#define MT7996_FW_PATCH_SEC_OFFSET(i)		(MT7996_FW_PATCH_HDR_BYTES + \
						 (i) * MT7996_FW_PATCH_SEC_BYTES)
#define MT7996_FW_PATCH_SEC_TYPE		0x00u	/* be32 */
#define   PATCH_SEC_TYPE_MASK			HW_GENMASK(15, 0)
#define   PATCH_SEC_TYPE_INFO			0x2u	/* only type the loader accepts */
#define MT7996_FW_PATCH_SEC_OFFS		0x04u	/* be32 file offset of data */
#define MT7996_FW_PATCH_SEC_SIZE		0x08u	/* be32 storage size, not used to size download */
#define MT7996_FW_PATCH_SEC_ADDR		0x0cu	/* be32 MCU destination */
#define MT7996_FW_PATCH_SEC_LEN			0x10u	/* be32 bytes downloaded, footer included */
#define MT7996_FW_PATCH_SEC_KEY_IDX		0x14u	/* be32 sec_key_idx, see below */
#define MT7996_FW_PATCH_SEC_ALIGN_LEN		0x18u	/* be32 */
/* 0x1c..0x3f reserved */

/* sec_key_idx word. All-ones or ENC_TYPE PLAIN -> no encryption flags.
 * Otherwise DL mode = ENCRYPT | RESET_SEC_IV | KEY_IDX, key from bits 7:0
 * (AES) or 15:8 (other); a key above 3 does not fit the 2-bit wire field.
 *                                                                   [static] */
#define PATCH_SEC_NOT_SUPPORT			0xffffffffu
#define PATCH_SEC_ENC_TYPE			HW_GENMASK(31, 24)
#define   PATCH_SEC_ENC_TYPE_PLAIN		0x00u
#define   PATCH_SEC_ENC_TYPE_AES		0x01u
#define   PATCH_SEC_ENC_TYPE_SCRAMBLE		0x02u
#define PATCH_SEC_ENC_AES_KEY			HW_GENMASK(7, 0)
#define PATCH_SEC_ENC_SCRAMBLE_KEY		HW_GENMASK(15, 8)	/* key index, other modes */
#define PATCH_SEC_ENC_SCRAMBLE_INFO		HW_GENMASK(15, 0)

/* Patch destinations. An INIT to either address carries PATCH_START_REQ
 * (0x05) instead of TARGET_ADDRESS_LEN_REQ (0x01); payload identical.
 * Observed patch: one section, file offset 0xa0, length 0x90c0, to 0x00900000.
 *                                                                   [static] */
#define MT7996_FW_PATCH_ADDR_LEGACY		0x00200000u
#define MT7996_FW_PATCH_ADDR			0x00900000u
/* The ROM copies the WHOLE decrypted section (footer included) into patch
 * SRAM from MT7996_FW_PATCH_ADDR; anything staged below
 * MT7996_FW_PATCH_ADDR + section length is overwritten by the copy.
 *                                                                 [measured] */

/*
 * Security footer: the LAST 0x400 bytes of the decrypted patch section, inside
 * info.len, so it is downloaded with the patch. Offsets relative to the footer
 * start; the two first words are little-endian.                     [static]
 */
#define MT7996_FW_PATCH_FOOTER_BYTES		0x400u
#define MT7996_FW_FOOTER_MODULUS_OFFS		0x000u	/* le32, 0x40 observed */
#define MT7996_FW_FOOTER_SIG_OFFS		0x004u	/* le32, 0x300 observed */
#define MT7996_FW_FOOTER_KEY_SEL		0x00au	/* key selector, 0 observed */
#define MT7996_FW_FOOTER_MODULUS		0x040u	/* 256-byte big-endian RSA modulus */
#define MT7996_FW_FOOTER_SIGNATURE		0x300u	/* 256-byte big-endian signature */
#define MT7996_FW_RSA_BYTES			256u
/*
 * Signature: RSASSA-PSS, RSA-2048, e = 65537, SHA-256, MGF1-SHA-256, salt 32.
 * Signed message = decrypted bytes [0, footer + 0x300), i.e. everything but
 * the signature, modulus included. The modulus is NOT anchored to an eFuse key
 * hash by the patch-finish path; key-hash banks: see secure.h.
 * Signed length must be a multiple of 64 bytes.                     [static]
 */

/* ROM verification result codes (internal; a failing patch makes the ROM send
 * init completion status 6 and spin forever).                       [static] */
#define MT7996_FW_SECBOOT_OK			0x64u
#define MT7996_FW_SECBOOT_KEY_INVALID		0x65u	/* selector/revocation/hash */
#define MT7996_FW_SECBOOT_RSA_EXP_FAIL		0x66u
#define MT7996_FW_SECBOOT_RSA_SER_FAIL		0x67u
#define MT7996_FW_SECBOOT_PSS_FAIL		0x68u
#define MT7996_FW_SECBOOT_LEN_UNALIGNED		0x69u	/* signed length % 64 != 0 */
#define MT7996_FW_SECBOOT_SHA_FAIL		0x6au
#define MT7996_FW_SECBOOT_DISABLED		0x6bu	/* sec_boot_en() false; accepted */
#define MT7996_FW_SECBOOT_SHA_MISMATCH		0x6cu
#define MT7996_FW_PATCH_FAIL_STATUS		6u	/* init completion status on reject */
/*
 * HAZARD: a rejected patch leaves the ROM spinning in its failure loop; only
 *         the WF subsystem reset (conn.h) recovers the MCU.          [static]
 *
 * HAZARD: EFUSE BURN. ROM patches built from the 2025-02 SDK onward (header
 *         build_date >= "202502...", upstream linux-firmware patches included)
 *         contain an eFuse FT-mapping "recovery" that runs on every MCU start,
 *         before any host command. On a chip whose eFuse word 0x2b0 has an FT
 *         version (bits 7:0) of 1 or 2, bits 14/15 clear and valid FT
 *         signatures, it PROGRAMS eFuse: zero-data program cycles of rows
 *         0x3e0/0x3d0/0x3c0, junk mapping bytes burned to 0xff, then bit 14
 *         (healthy) or bit 15 (recovered) of word 0x2b0, which closes the gate.
 *         Earlier patches (e.g. build 20240726161535a) do not. Read the gate
 *         word through firmware (UNI EFUSE_CTRL, mcu_cmd.h) with an old patch
 *         before first bring-up with a new one. The A-die variant also needs
 *         strap 0x700056f0 bit 15.                                   [static]
 */
#define MT7996_FW_EFUSE_FT_GATE_WORD		0x2b0u
#define   EFUSE_FT_GATE_FT_VER			HW_GENMASK(7, 0)
#define   EFUSE_FT_GATE_HEALTHY			HW_BIT(14)
#define   EFUSE_FT_GATE_RECOVERED		HW_BIT(15)

#endif /* HW_MT7996_FW_FORMAT_H */
