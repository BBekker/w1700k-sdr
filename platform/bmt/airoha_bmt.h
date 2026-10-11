/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Airoha/EcoNet vendor BMT+BBT translation, the read side.
 *
 * The vendor's logical->physical NAND mapping, as stock U-Boot and the stock
 * kernel apply it. The same file is compiled into our U-Boot, our Linux and a
 * host test (tests/test_platform_bmt.py), so the loader and the kernel agree
 * block for block with each other and with stock.
 *
 * Translation is two stages, in the vendor's order:
 *     logical --[BBT shift]--> physical --[BMT substitution]--> final
 *
 * This is not OpenWrt's mtk_bmt: that computes an unclamped reserve pool, fails
 * the checksum of a vendor-written BMT, and then erases and rewrites the table.
 * This core clamps the pool to the table capacity, checks the whole 1000-byte
 * checksum extent, and refuses a table that does not validate. It never
 * writes; replacement lives in airoha_bmt_write.c and airoha_bmt_transaction.c.
 */
#ifndef AIROHA_BMT_H
#define AIROHA_BMT_H

#if defined(AIROHA_BMT_HOST)
#include <stdint.h>
#include <string.h>
typedef uint8_t airoha_u8;
typedef uint16_t airoha_u16;
typedef uint32_t airoha_u32;
#else
#include <linux/types.h>
#include <linux/string.h>
typedef u8 airoha_u8;
typedef u16 airoha_u16;
typedef u32 airoha_u32;
#endif

/* Both tables hold at most 250 entries; the clamp and each table's capacity. */
#define AIROHA_BMT_MAX_ENTRIES 250
/* On-flash table sizes and layouts. */
#define AIROHA_BBT_BYTES 0x200
#define AIROHA_BMT_BYTES 0x3fc
#define AIROHA_BBT_HEADER 0xc
#define AIROHA_BMT_HEADER 0x14
/* The vendor's reserve pool: scan target total*8/100, ceiling total/8. */
#define AIROHA_GOOD_NUMERATOR 8
#define AIROHA_GOOD_DENOMINATOR 100
#define AIROHA_CEILING_DIVISOR 8

/* Negative return codes. Bindings map these onto -EIO/-EINVAL/-EROFS. */
enum {
	AIROHA_BMT_OK = 0,
	AIROHA_BMT_E_SHORT = -1,	/* buffer smaller than the table */
	AIROHA_BMT_E_SIGNATURE = -2,	/* wrong magic */
	AIROHA_BMT_E_VERSION = -3,	/* zero version; no vendor writer makes one */
	AIROHA_BMT_E_SIZE = -4,		/* entry count exceeds the 250 capacity */
	AIROHA_BMT_E_CHECKSUM = -5,	/* checksum mismatch at the pinned extent */
	AIROHA_BMT_E_UNPINNABLE = -6,	/* geometry too small to pin the extent */
	AIROHA_BMT_E_RANGE = -7,	/* block outside the device after mapping */
	AIROHA_BMT_E_ORDER = -8,	/* BBT not strictly ascending */
	AIROHA_BMT_E_NOTLOADED = -9,	/* translate before both tables parsed */
};

struct airoha_bmt {
	airoha_u16 total_blocks;	/* whole chip, e.g. 4096 */
	airoha_u16 system_blocks;	/* total minus the clamped pool (e.g. 3846) */
	airoha_u8 geometry_set;
	airoha_u8 bbt_loaded;
	airoha_u8 bmt_loaded;
	airoha_u8 bbt_ascending;
	airoha_u16 bbt_size;
	airoha_u16 bmt_size;
	airoha_u16 bbt_entries[AIROHA_BMT_MAX_ENTRIES];
	airoha_u16 bmt_from[AIROHA_BMT_MAX_ENTRIES];
	airoha_u16 bmt_to[AIROHA_BMT_MAX_ENTRIES];
};

/* The reserve pool size of a part with `total_blocks` blocks. */
airoha_u32 airoha_bmt_pool_floor(airoha_u32 total_blocks);
airoha_u32 airoha_bmt_pool_ceiling(airoha_u32 total_blocks);
/* The clamped pool, or 0 when the geometry cannot pin it (part < ~3138 blocks). */
airoha_u32 airoha_bmt_clamped_pool(airoha_u32 total_blocks);

/* Set total_blocks and derive system_blocks from the clamped pool. Must precede
 * parse_bmt (the checksum extent depends on the clamp) and translate. Returns
 * AIROHA_BMT_E_UNPINNABLE when the geometry cannot pin the pool. */
int airoha_bmt_set_geometry(struct airoha_bmt *t, airoha_u32 total_blocks);

/* Parse one table from the first page of its reserve block. Idempotent; the
 * parse functions only read the buffer. */
int airoha_bmt_parse_bbt(struct airoha_bmt *t, const airoha_u8 *page, airoha_u32 len);
int airoha_bmt_parse_bmt(struct airoha_bmt *t, const airoha_u8 *page, airoha_u32 len);

/* logical block -> final physical block, or a negative code. Requires geometry
 * and both tables loaded, and a strictly ascending BBT. */
int airoha_bmt_translate(const struct airoha_bmt *t, airoha_u32 logical_block);

/* The shift amount at a logical block: physical-after-shift minus logical. This
 * is `f`. Negative code on error. */
int airoha_bmt_shift(const struct airoha_bmt *t, airoha_u32 logical_block);

/* The lowest logical block that maps to `final`, or AIROHA_BMT_E_RANGE if none.
 * Used by the round-trip check; linear over the usable area. */
int airoha_bmt_untranslate(const struct airoha_bmt *t, airoha_u32 final_block);

/* Usable blocks: system_blocks minus the BBT entry count (what stock publishes). */
int airoha_bmt_usable_blocks(const struct airoha_bmt *t);

#endif /* AIROHA_BMT_H */
