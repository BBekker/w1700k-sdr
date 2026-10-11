// SPDX-License-Identifier: GPL-2.0-only
/* mmio.c: address translation against mt76, access sequences, READ_MEM windows. */
#include "mock-io.h"
#include "mmio.c"
#include "mt76_vectors.h"

static const struct mt76_map_vector *vector(u32 address)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(mt76_map_vectors); i++)
		if (mt76_map_vectors[i].address == address)
			return &mt76_map_vectors[i];
	return NULL;
}

static void translation_matches_mt76(void)
{
	struct sdr_mapping map;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(mt76_map_vectors); i++) {
		const struct mt76_map_vector *v = &mt76_map_vectors[i];

		sdr_map(v->address, &map);
		CHECK(map.bar == v->bar);
		CHECK(map.remap_reg == v->remap_reg);
		CHECK(map.remap_mask == v->remap_mask);
		CHECK(map.remap_value == v->remap_value);
		CHECK(map.span == v->span);
	}
}

/*
 * Read `length` bytes at `address`, where every word address is a reference
 * vector, and check the exact bus sequence: one begin, per window chunk a
 * selector read/merge/write/read-back when remapped, then the aperture words.
 */
static void read_sequence(u32 address, u32 length)
{
	struct sdr_mmio io = mock_io(0x200000);
	u8 out[16 + 2];
	u32 done = 0, i, n, selector[2] = { 0xa5a55a5a, 0xa5a55a5a };
	unsigned int e = 0;

	CHECK(length <= 16);
	io_reset();
	io_regs[0x155024 / 4] = selector[0];
	io_regs[0x1b4 / 4] = selector[1];
	for (i = 0; i < length; i += 4) {
		const struct mt76_map_vector *v = vector(address + i);

		if (!v)
			fprintf(stderr, "no reference vector for 0x%08x\n", address + i);
		CHECK(v);
		io_regs[v->bar / 4] = (address + i) ^ 0x89abcdef;
	}
	memset(out, 0xcc, sizeof(out));
	CHECK(sdr_mmio_read(&io, address, out + 1, length) == 0);
	CHECK(io_begins == 1 && io_ends == 1 && !io_locked);
	while (done < length) {
		const struct mt76_map_vector *v = vector(address + done);
		u32 *sel = v->remap_reg == 0x155024 ? &selector[0] : &selector[1];

		n = v->span < length - done ? v->span : length - done;
		if (v->remap_reg) {
			e = expect_event(e, "read32", v->remap_reg, *sel);
			*sel = (*sel & ~v->remap_mask) | v->remap_value;
			e = expect_event(e, "write32", v->remap_reg, *sel);
			e = expect_event(e, "read32", v->remap_reg, *sel);
		}
		for (i = 0; i < n; i += 4) {
			/* Contiguous within a chunk, as mt76 translates each word. */
			CHECK(vector(address + done + i)->bar == v->bar + i);
			e = expect_event(e, "read32", v->bar + i, (address + done + i) ^ 0x89abcdef);
			CHECK(get_unaligned_le32(out + 1 + done + i) == ((address + done + i) ^ 0x89abcdef));
		}
		done += n;
	}
	CHECK(e == event_count);
	CHECK(out[0] == 0xcc && out[length + 1] == 0xcc);
}

static void reads_follow_windows(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(sdr_fixed); i++) {
		read_sequence(sdr_fixed[i].phys - 4, 12);
		read_sequence(sdr_fixed[i].phys + sdr_fixed[i].size - 4, 12);
	}
	read_sequence(0x000ffffc, 4);
	read_sequence(0xfffffffc, 4);
	read_sequence(0x0090f008, 4);
}

static void rejected(u32 address, u32 length, u64 bar_size, int error)
{
	struct sdr_mmio io = mock_io(bar_size);
	u8 out[SDR_MMIO_MAX_READ + 8];

	io_reset();
	memset(out, 0xcc, sizeof(out));
	CHECK(sdr_mmio_read(&io, address, out, length) == error);
	CHECK(!io_begins && !event_count);
	for (size_t i = 0; i < sizeof(out); i++)
		CHECK(out[i] == 0xcc);
}

static void reads_reject_before_any_access(void)
{
	struct sdr_mmio io = mock_io(0x200000);
	u8 out[4] = { 1, 2, 3, 4 };

	rejected(1, 4, 0x200000, -EINVAL);
	rejected(0x00800000, 3, 0x200000, -EINVAL);
	rejected(0x00800000, SDR_MMIO_MAX_READ + 4, 0x200000, -EINVAL);
	rejected(0xfffffffc, 8, 0x200000, -EINVAL);
	rejected(0x820ccffc, 8, 0x10000, -ERANGE);	/* second chunk out of the BAR */
	rejected(0x18000000, 4, 0x140000, -ERANGE);	/* selector out of the BAR */
	rejected(0x00800000, 0, 0x200000, 0);
	io_reset();
	io_begin_error = -EBUSY;
	CHECK(sdr_mmio_read(&io, 0x00800000, out, 4) == -EBUSY);
	CHECK(io_begins == 1 && !io_ends && !event_count);
	CHECK(out[0] == 1 && out[3] == 4);
}

static void read_windows(void)
{
	static const struct { u32 address, length; bool ok; } cases[] = {
		{ 0x000e00f0, 4, true }, { 0x000e0010, 4, true },
		{ 0x000e0000, 4096, true }, { 0x000e0ffc, 8, false },
		{ 0x000dfffc, 4, false }, { 0x00000000, 4, false },
		{ 0x000d4208, 4, false },	/* WFDMA: not a read window */
		{ 0x00800000, 64, true }, { 0x0080fffc, 4, true },
		{ 0x0080fffc, 8, false }, { 0x007ffffc, 4, false },
		{ 0x00900000, 64, true }, { 0x00901000, 4, true },
		{ 0x0090b000, 2048, true }, { 0x0090f000, 2048, true },
		{ 0x0090fffc, 4, true }, { 0x0090fffc, 8, false },
		{ 0x00910000, 4, false },
		/* The vendor flow's IRQ5 slot and OSC_MASK words: no longer read. */
		{ 0x02208780, 4, false }, { 0x81021104, 4, false },
		{ 0x40000000, 4, false },	/* UMAC SYSRAM: inside the WiFi island */
		{ 0x820c0000, 4, false }, { 0x7c000104, 4, false },
		{ 0xfffffffc, 4, false }, { 0x00800000, 0, false },
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(cases); i++)
		CHECK(sdr_mem_read_allowed(cases[i].address, cases[i].length) == cases[i].ok);
}

static void word_access(u32 address)
{
	struct sdr_mmio io = mock_io(0x200000);
	struct sdr_mapping map;
	u32 before, after, value, sel = 0x12345678;
	unsigned int e = 0;

	sdr_map(address, &map);
	io_reset();
	if (map.remap_reg)
		io_regs[map.remap_reg / 4] = sel;
	io_regs[map.bar / 4] = 0xf0f0f0f0;
	CHECK(sdr_mmio_rmw_held(&io, address, 0x0000ff00, 0x00001200, &before, &after) == 0);
	CHECK(before == 0xf0f0f0f0 && after == 0xf0f012f0);
	if (map.remap_reg) {
		e = expect_event(e, "read32", map.remap_reg, sel);
		sel = (sel & ~map.remap_mask) | map.remap_value;
		e = expect_event(e, "write32", map.remap_reg, sel);
		e = expect_event(e, "read32", map.remap_reg, sel);
	}
	e = expect_event(e, "read32", map.bar, 0xf0f0f0f0);
	e = expect_event(e, "write32", map.bar, 0xf0f012f0);
	e = expect_event(e, "read32", map.bar, 0xf0f012f0);
	CHECK(e == event_count);

	/* Posted store: no read-back of the word itself. */
	reset_events();
	CHECK(sdr_mmio_write32_held(&io, address, 0x11223344) == 0);
	CHECK(!strcmp(events[event_count - 1].kind, "write32"));
	CHECK(events[event_count - 1].address == map.bar);
	CHECK(sdr_mmio_read32_held(&io, address, &value) == 0 && value == 0x11223344);
	CHECK(!io_begins && !io_ends);	/* held variants never take the lock */
}

static void word_accesses(void)
{
	struct sdr_mmio io = mock_io(0x200000);
	u32 before, after;

	word_access(0x0090f008);	/* L2 */
	word_access(0x70028600);	/* L1 */
	word_access(0x81021104);	/* fixed */
	word_access(0x000e00f0);	/* direct */
	io_reset();
	CHECK(sdr_mmio_rmw_held(&io, 0x0090f008, 0, 0, &before, &after) == -EINVAL);
	CHECK(sdr_mmio_rmw_held(&io, 0x0090f008, 1, 2, &before, &after) == -EINVAL);
	CHECK(sdr_mmio_rmw_held(&io, 0x0090f00a, 1, 1, &before, &after) == -EINVAL);
	CHECK(sdr_mmio_rmw_held(&io, 0x0090f008, 1, 1, NULL, &after) == -EINVAL);
	CHECK(sdr_mmio_write32_held(&io, 0x0090f00a, 1) == -EINVAL);
	CHECK(sdr_mmio_read32_held(&io, 0x0090f00a, &before) == -EINVAL);
	io = mock_io(0x1000);
	CHECK(sdr_mmio_rmw_held(&io, 0x70028600, 1, 1, &before, &after) == -ERANGE);
	CHECK(!event_count && !io_begins);
}

int main(void)
{
	translation_matches_mt76();
	reads_follow_windows();
	reads_reject_before_any_access();
	read_windows();
	word_accesses();
	printf("mmio: %lu checks\n", checks);
	return 0;
}
