/* SPDX-License-Identifier: GPL-2.0-only */
/* Independent byte expectations and recovery negatives, not a NAND simulator. */
#include <assert.h>
#include <errno.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include "airoha_bmt_write.h"

static void hash(const unsigned char *p, unsigned int len, unsigned char out[32])
{ assert(SHA256(p, len, out)); }
static void used(struct airoha_bmt_intent *r, unsigned block)
{ unsigned i = block - 3846; r->free_pool[i / 8] &= ~(1u << (i % 8)); }
static struct airoha_bmt table, scratch, old;
static unsigned char page[2048], saved[2048], check[2048];

static void base(void)
{
	static const unsigned short bad[] = {5, 11, 20, 40, 100};
	assert(!airoha_bmt_set_geometry(&table, 4096));
	table.bbt_loaded = table.bmt_loaded = table.bbt_ascending = 1;
	table.bbt_size = 5;
	memcpy(table.bbt_entries, bad, sizeof bad);
	table.bmt_size = 1;
	table.bmt_from[0] = 52;
	table.bmt_to[0] = 3847;
}

static void bytes(void)
{
	static const unsigned char bmt_head[24] = {
		'B','M','T',1,255,1,76,255,255,255,255,255,255,255,255,255,
		255,255,255,255,52,0,7,15,
	};
	static const unsigned char bbt_head[22] = {
		'R','A','W','B',182,0,0,0,1,5,255,255,5,0,11,0,20,0,40,0,100,0,
	};
	base();
	assert(!airoha_bmt_encode(&table, page, sizeof page));
	assert(!memcmp(page, bmt_head, sizeof bmt_head));
	for (unsigned i = sizeof bmt_head; i < 1020; i++) assert(!page[i]);
	for (unsigned i = 1020; i < sizeof page; i++) assert(page[i] == 255);
	assert(!airoha_bbt_encode(&table, page, sizeof page));
	assert(!memcmp(page, bbt_head, sizeof bbt_head));
	for (unsigned i = sizeof bbt_head; i < 512; i++) assert(!page[i]);
	for (unsigned i = 512; i < sizeof page; i++) assert(page[i] == 255);
	assert(!airoha_bmt_set_geometry(&scratch, 4096));
	assert(!airoha_bmt_parse_bbt(&scratch, page, sizeof page));
	assert(!airoha_bmt_encode(&table, page, sizeof page));
	assert(!airoha_bmt_parse_bmt(&scratch, page, sizeof page));
	assert(airoha_bmt_translate(&scratch, 48) == 3847);
	memcpy(saved, page, sizeof page);
	assert(airoha_bmt_encode(&table, page, 1020) == -EINVAL);
	assert(!memcmp(saved, page, sizeof page));
	puts("canonical vendor bytes and independent checksum/layout expectations: passed");
}

static void replacement(void)
{
	base();
	assert(!airoha_bmt_write_validate(&table, 64, 3568, 3846, 4095));
	assert(!airoha_bmt_replace(&table, 48, 3848));
	assert(table.bmt_size == 2 && table.bmt_from[0] == 52 && table.bmt_to[0] == 3848);
	assert(table.bmt_from[1] == 3847 && table.bmt_to[1] == 3847);
	assert(!airoha_bmt_replace(&table, 48, 3849));
	assert(table.bmt_size == 3 && table.bmt_from[0] == 52 && table.bmt_to[0] == 3849);
	assert(table.bmt_from[2] == 3848 && table.bmt_to[2] == 3848);
	assert(!airoha_bmt_replace(&table, 49, 3850));
	assert(table.bmt_size == 4 && table.bmt_from[3] == 53);
	old = table;
	assert(airoha_bmt_replace(&table, 49, 3849) == -EINVAL);
	assert(!memcmp(&old, &table, sizeof table));
	assert(airoha_bmt_write_validate(&table, 64, 3846, 3846, 4095) == -EINVAL);
	assert(airoha_bmt_write_validate(&table, 64, 3568, 3846, 3849) == -EINVAL);
	table.bmt_from[1] = 52;
	assert(airoha_bmt_write_validate(&table, 64, 3568, 3846, 4095) == -EINVAL);
	table = old; table.bmt_to[1] = 3849;
	assert(airoha_bmt_write_validate(&table, 64, 3568, 3846, 4095) == -EINVAL);
	table = old; table.bmt_from[1] = 5;
	assert(airoha_bmt_write_validate(&table, 64, 3568, 3846, 4095) == -EINVAL);
	table = old; table.bmt_from[1] = 3846;
	assert(airoha_bmt_write_validate(&table, 64, 3568, 3846, 4095) == -EINVAL);
	base(); table.bmt_size = 250;
	for (unsigned i = 0; i < 250; i++) {
		table.bmt_from[i] = 1000 + i;
		table.bmt_to[i] = 3846 + i;
	}
	/* All 250 destinations are occupied; reusing one is not allocation. */
	old = table;
	assert(airoha_bmt_replace(&table, 48, 3846) == -EINVAL);
	assert(!memcmp(&old, &table, sizeof table));
	puts("original-key replacement, repeated wear, pool alias and ownership negatives: passed");
}

static void intent_base(struct airoha_bmt_intent *r, int mapped)
{
	base();
	if (!mapped) table.bmt_size = 0;
	memset(r, 0, sizeof *r);
	r->operation = AIROHA_INTENT_PROGRAM;
	r->journal_block = 3848;
	r->candidate_block = 3849;
	r->logical_block = 48;
	r->system_key = 52;
	r->old_physical = mapped ? 3847 : 52;
	r->old_table_block = 4095;
	r->bbt_block = 3846;
	r->bbt_size = table.bbt_size;
	r->bmt_size = table.bmt_size;
	r->old_mapping_present = mapped;
	memset(r->free_pool, 255, sizeof r->free_pool);
	r->free_pool[31] = 3;
	used(r, r->bbt_block); used(r, r->old_table_block);
	if (mapped) used(r, r->old_physical);
	assert(!airoha_bmt_encode(&table, page, sizeof page));
	hash(page, AIROHA_BMT_BYTES, r->bmt_digest);
	assert(!airoha_bbt_encode(&table, page, sizeof page));
	hash(page, AIROHA_BBT_BYTES, r->bbt_digest);
	hash((const unsigned char *)"intended block", 14, r->data_digest);
}

static void intents(void)
{
	unsigned corruptions = 0;
	for (int mapped = 0; mapped <= 1; mapped++) {
		struct airoha_bmt_intent r, decoded, sentinel;
		intent_base(&r, mapped);
		assert(!airoha_bmt_intent_encode(&r, page, sizeof page, hash));
		/* Independent fixed header byte positions, including little endian. */
		assert(!memcmp(page, "SDRWB003\3\0\1\0\10\17\11\17\60\0\64\0", 20));
		assert(page[22] == 255 && page[23] == 15 && page[24] == 6 && page[25] == 15);
		assert(!memcmp(page + 128, "\0\0\2\0\0\10\0\0\70\0\0\0\0\20\0\0", 16));
		assert(!airoha_bmt_intent_decode(&decoded, page, sizeof page, hash));
		assert(!memcmp(&r, &decoded, sizeof r));
		assert(!airoha_bmt_intent_match(&r, &table, &scratch, check, hash));
		old = table;
		assert(!airoha_bmt_replace(&table, 48, r.candidate_block));
		assert(!airoha_bmt_intent_match(&r, &table, &scratch, check, hash));
		table.bbt_entries[0] = 6;
		assert(airoha_bmt_intent_match(&r, &table, &scratch, check, hash) == -EINVAL);
		table = old;
		assert(!airoha_bmt_replace(&table, 49, 3850));
		assert(airoha_bmt_intent_match(&r, &table, &scratch, check, hash) == -EINVAL);
		memcpy(saved, page, sizeof page);
		memset(&sentinel, 0xa5, sizeof sentinel);
		for (unsigned i = 0; i < sizeof page; i++) {
			decoded = sentinel;
			page[i] ^= 1;
			assert(airoha_bmt_intent_decode(&decoded, page, sizeof page, hash) < 0);
			assert(!memcmp(&decoded, &sentinel, sizeof decoded));
			page[i] ^= 1;
			corruptions++;
		}
		assert(!memcmp(saved, page, sizeof page));
		/* Re-hashed malicious records still fail semantic validation. */
		page[14] = page[12]; page[15] = page[13]; hash(page, 208, page + 208);
		assert(airoha_bmt_intent_decode(&decoded, page, sizeof page, hash) == -EINVAL);
		memcpy(page, saved, sizeof page);
		page[144] |= 1; /* A forged free bit for the factory BBT is never authority. */
		hash(page, 208, page + 208);
		assert(airoha_bmt_intent_decode(&decoded, page, sizeof page, hash) == -EINVAL);
		for(unsigned version=1;version<=2;version++) {
			memcpy(page, saved, sizeof page);
			page[7] = '0'+version; page[8] = version;
			hash(page, 208, page + 208);
			assert(airoha_bmt_intent_decode(&decoded, page, sizeof page, hash) == -EINVAL);
		}
		r.system_key++;
		assert(airoha_bmt_intent_match(&r, &old, &scratch, check, hash) == -EINVAL);
	}
	printf("journal header/hash/padding corruptions rejected without publishing output: %u\n", corruptions);
	puts("old/new mapping recovery with empty/nonempty tables and changed-factory negatives: passed");
}

int main(void)
{
	bytes(); replacement(); intents();
	return 0;
}
