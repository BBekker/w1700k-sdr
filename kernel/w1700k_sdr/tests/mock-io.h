/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * A struct sdr_mmio over a register file: begin/end model the device lock
 * (with an injectable begin error), every access is logged in order.
 */
#ifndef W1700K_SDR_TEST_MOCK_IO_H
#define W1700K_SDR_TEST_MOCK_IO_H
#include "harness.h"
#include "mmio.h"

static u32 io_regs[0x200000 / 4];
static bool io_locked;
static int io_begin_error;
static unsigned int io_begins, io_ends;

static inline int io_begin(void *ctx)
{
	(void)ctx;
	CHECK(!io_locked);
	io_begins++;
	if (io_begin_error)
		return io_begin_error;
	io_locked = true;
	return 0;
}

static inline void io_end(void *ctx)
{
	(void)ctx;
	CHECK(io_locked);
	io_locked = false;
	io_ends++;
}

static inline u32 io_read32(void *ctx, u32 bar)
{
	(void)ctx;
	CHECK(bar % 4 == 0 && bar < sizeof(io_regs));
	log_event("read32", bar, io_regs[bar / 4]);
	return io_regs[bar / 4];
}

static inline void io_write32(void *ctx, u32 bar, u32 value)
{
	(void)ctx;
	CHECK(bar % 4 == 0 && bar < sizeof(io_regs));
	log_event("write32", bar, value);
	io_regs[bar / 4] = value;
}

static const struct sdr_mmio_ops io_ops = {
	io_begin, io_end, io_read32, io_write32,
};

static inline struct sdr_mmio mock_io(u64 bar_size)
{
	return (struct sdr_mmio){ &io_ops, NULL, bar_size };
}

static inline void io_reset(void)
{
	memset(io_regs, 0, sizeof(io_regs));
	io_locked = false;
	io_begin_error = 0;
	io_begins = io_ends = 0;
	reset_events();
}

/* The next logged event is `kind` at `address` with `value`. */
static inline unsigned int expect_event(unsigned int i, const char *kind, u32 address,
				 u32 value)
{
	CHECK(i < event_count);
	CHECK(!strcmp(events[i].kind, kind));
	CHECK(events[i].address == address);
	CHECK(events[i].value == value);
	return i + 1;
}
#endif
