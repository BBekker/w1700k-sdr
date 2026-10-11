// SPDX-License-Identifier: GPL-2.0-only
/* fwdl_wire.c: packets against mt76, argument bounds, reply parsing. */
#include <sys/mman.h>
#include <unistd.h>
#include "harness.h"
#include "fwdl_wire.c"
#include "mt76_vectors.h"

static size_t unhex(const char *hex, u8 *out, size_t capacity)
{
	size_t n = 0;
	unsigned int byte;

	while (hex[0] && hex[1]) {
		CHECK(n < capacity && sscanf(hex, "%2x", &byte) == 1);
		out[n++] = (u8)byte;
		hex += 2;
	}
	CHECK(!*hex);
	return n;
}

static int encode(const struct mt76_packet_vector *v, u8 *out, size_t capacity)
{
	if (!strcmp(v->kind, "init"))
		return mt7996_fwdl_init(out, capacity, v->seq, v->a, v->b, v->c);
	if (!strcmp(v->kind, "start")) {
		/* mt76's option carries OVERRIDE itself; ours derives it. */
		CHECK(!!(v->b & MT7996_FWDL_START_OVERRIDE) == !!v->a);
		return mt7996_fwdl_start(out, capacity, v->seq, v->a,
					 v->b & ~MT7996_FWDL_START_OVERRIDE);
	}
	if (!strcmp(v->kind, "patch_sem"))
		return mt7996_fwdl_patch_sem(out, capacity, v->seq, v->a);
	CHECK(!strcmp(v->kind, "patch_finish"));
	return mt7996_fwdl_patch_finish(out, capacity, v->seq, v->a);
}

static void packets_match_mt76(void)
{
	u8 expected[400], out[402];
	size_t i, n;
	int len;

	for (i = 0; i < ARRAY_SIZE(mt76_packets); i++) {
		n = unhex(mt76_packets[i].hex, expected, sizeof(expected));
		memset(out, 0xad, sizeof(out));
		len = encode(&mt76_packets[i], out + 1, sizeof(out) - 2);
		CHECK(len == (int)n);
		CHECK(!memcmp(out + 1, expected, n));
		CHECK(out[0] == 0xad && out[n + 1] == 0xad);
		/* One byte short: refused, output untouched. */
		memset(out, 0xad, sizeof(out));
		CHECK(encode(&mt76_packets[i], out, n - 1) == MT7996_FWDL_CAPACITY);
		for (size_t k = 0; k < sizeof(out); k++)
			CHECK(out[k] == 0xad);
	}
}

static void bounds(void)
{
	u8 out[4096];
	u32 seq;

	for (seq = 0; seq <= 15; seq++)
		CHECK(mt7996_fwdl_next_seq(seq) == (int)(seq == 15 ? 1 : seq + 1));
	CHECK(mt7996_fwdl_next_seq(16) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_init_command(0x200000) == MT7996_FWDL_CMD_PATCH_START);
	CHECK(mt7996_fwdl_init_command(0x900000) == MT7996_FWDL_CMD_PATCH_START);
	CHECK(mt7996_fwdl_init_command(0x916400) == MT7996_FWDL_CMD_TARGET_ADDRESS_LEN);
	CHECK(mt7996_fwdl_init(out, sizeof(out), 0, 0x916400, 4, 0) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_init(out, sizeof(out), 16, 0x916400, 4, 0) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_init(out, sizeof(out), 1, 0x916400, 0, 0) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_init(out, sizeof(out), 1, 0xfffffff0, 17, 0) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_init(out, sizeof(out), 1, 0, 0x80000000U, 0) == MT7996_FWDL_INVALID);
	for (u32 f = 0; f < 256; f++)
		CHECK((mt7996_fwdl_init(out, sizeof(out), 1, 0x916400, 4, f) < 0) == !!(f & ~0x37U));
	CHECK(mt7996_fwdl_init(NULL, 76, 1, 0x916400, 4, 0) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_start(out, sizeof(out), 1, 0, 2) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_start(out, sizeof(out), 1, 0, 0xc) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_patch_sem(out, sizeof(out), 1, 2) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_patch_finish(out, sizeof(out), 1, 2) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_scatter_length(0, 0) == 0);
	CHECK(mt7996_fwdl_scatter_length(5000, 0) == 4096);
	CHECK(mt7996_fwdl_scatter_length(5000, 4096) == 904);
	CHECK(mt7996_fwdl_scatter_length(5000, 5000) == 0);
	CHECK(mt7996_fwdl_scatter_length(5000, 5001) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_scatter_length(0x80000000U, 0) == MT7996_FWDL_INVALID);
}

static void make_reply(u8 *r, u8 seq, u8 eid, u8 status)
{
	memset(r, 0, MT7996_FWDL_REPLY_BYTES);
	put_unaligned_le32(0x3800002c, r);	/* 44 bytes, RXD0 type 7 */
	r[32] = 12;
	r[34] = 7;
	r[36] = eid;
	r[37] = seq;
	r[40] = status;
}

static void replies(void)
{
	static const u8 must_be_zero[] = { 38, 39, 41, 42, 43 };
	u8 r[64];
	u32 status;
	size_t i;

	for (status = 0; status < 256; status++) {
		make_reply(r, 9, 1, (u8)status);
		CHECK(mt7996_fwdl_reply_status(r, 44, 9, MT7996_FWDL_EID_GENERIC) == (int)status);
		CHECK(mt7996_fwdl_reply_status(r, 44, 9, MT7996_FWDL_EID_ANY) == (int)status);
	}
	make_reply(r, 9, 1, 0);
	CHECK(mt7996_fwdl_reply_status(r, 44, 8, MT7996_FWDL_EID_ANY) == MT7996_FWDL_UNRELATED_REPLY);
	CHECK(mt7996_fwdl_reply_status(r, 44, 9, MT7996_FWDL_EID_PATCH_SEM) ==
	      MT7996_FWDL_UNRELATED_REPLY);
	CHECK(mt7996_fwdl_reply_status(r, 43, 9, MT7996_FWDL_EID_ANY) == MT7996_FWDL_MALFORMED_REPLY);
	CHECK(mt7996_fwdl_reply_status(r, 45, 9, MT7996_FWDL_EID_ANY) == MT7996_FWDL_MALFORMED_REPLY);
	CHECK(mt7996_fwdl_reply_status(r, 44, 0, MT7996_FWDL_EID_ANY) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_reply_status(r, 44, 16, MT7996_FWDL_EID_ANY) == MT7996_FWDL_INVALID);
	CHECK(mt7996_fwdl_reply_status(NULL, 44, 9, MT7996_FWDL_EID_ANY) == MT7996_FWDL_INVALID);
	for (i = 0; i < ARRAY_SIZE(must_be_zero); i++) {
		make_reply(r, 9, 1, 0);
		r[must_be_zero[i]] = 1;
		CHECK(mt7996_fwdl_reply_status(r, 44, 9, MT7996_FWDL_EID_ANY) ==
		      MT7996_FWDL_MALFORMED_REPLY);
	}
	make_reply(r, 9, 1, 0);
	r[3] = 0x10;		/* RXD0 type 2: a received frame, not an event */
	CHECK(mt7996_fwdl_reply_status(r, 44, 9, MT7996_FWDL_EID_ANY) == MT7996_FWDL_UNRELATED_REPLY);
	make_reply(r, 9, 1, 0);
	r[32] = 13;
	CHECK(mt7996_fwdl_reply_status(r, 44, 9, MT7996_FWDL_EID_ANY) == MT7996_FWDL_MALFORMED_REPLY);
}

/* Reads stay inside the packet: place it against inaccessible pages. */
static void reply_reads_stay_in_bounds(void)
{
	size_t page = (size_t)sysconf(_SC_PAGESIZE), offsets[3], i, n;
	u8 *area = mmap(NULL, 3 * page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	u8 reply[44];

	CHECK(area != MAP_FAILED);
	for (n = 0; n < 44; n++)
		CHECK(mt7996_fwdl_reply_status(area, n, 9, 1) == MT7996_FWDL_MALFORMED_REPLY);
	make_reply(reply, 9, 1, 254);
	offsets[0] = page;
	offsets[1] = 2 * page - sizeof(reply);
	offsets[2] = page + 3;
	for (i = 0; i < ARRAY_SIZE(offsets); i++) {
		CHECK(!mprotect(area + page, page, PROT_READ | PROT_WRITE));
		memcpy(area + offsets[i], reply, sizeof(reply));
		CHECK(!mprotect(area + page, page, PROT_READ));
		CHECK(mt7996_fwdl_reply_status(area + offsets[i], 44, 9, 1) == 254);
	}
	CHECK(!munmap(area, 3 * page));
}

int main(void)
{
	packets_match_mt76();
	bounds();
	replies();
	reply_reads_stay_in_bounds();
	printf("wire: %lu checks\n", checks);
	return 0;
}
