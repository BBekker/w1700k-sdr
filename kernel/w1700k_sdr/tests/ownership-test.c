// SPDX-License-Identifier: GPL-2.0-only
/*
 * ownership.c: the mt76 mt7996_driver_own handshake. Write DRV_OWN to LPCR,
 * read LPCR until FW_OWN_STAT clears (51 reads, a wait between), then write
 * the ownership acknowledgement.
 */
#include "harness.h"
#include "ownership.c"

static unsigned int reads, waits, ready_at, fail_at;
static u32 pattern;
static int wait_error;

static u32 read_lpcr(void *ctx, u32 bar)
{
	u32 value = pattern | SDR_HOST_FW_OWN_STAT;

	(void)ctx;
	reads++;
	if (reads == ready_at)
		value &= ~SDR_HOST_FW_OWN_STAT;
	if (reads == fail_at)
		value = U32_MAX;
	log_event("read32", bar, value);
	return value;
}

static void write_reg(void *ctx, u32 bar, u32 value)
{
	(void)ctx;
	log_event("write32", bar, value);
}

static int wait_poll(void *ctx)
{
	(void)ctx;
	waits++;
	return wait_error;
}

static const struct sdr_mmio_ops ops = { .read32 = read_lpcr, .write32 = write_reg };

static void reset(unsigned int ready, unsigned int fail, u32 fill)
{
	reset_events();
	reads = waits = 0;
	ready_at = ready;
	fail_at = fail;
	pattern = fill;
	wait_error = 0;
}

static void check_sequence(unsigned int polls, bool acked)
{
	unsigned int i;

	CHECK(event_count == 1 + polls + acked);
	CHECK(!strcmp(events[0].kind, "write32") && events[0].address == 0xe0010 &&
	      events[0].value == 0x2);
	for (i = 1; i <= polls; i++)
		CHECK(!strcmp(events[i].kind, "read32") && events[i].address == 0xe0010);
	if (acked)
		CHECK(!strcmp(events[i].kind, "write32") && events[i].address == 0xe0014 &&
		      events[i].value == 0x1);
}

int main(void)
{
	struct sdr_mmio io = { &ops, NULL, SDR_HOST_IRQ_STAT + 4 };
	struct sdr_mmio_ops missing = ops;
	const u32 fills[] = { 0, 1, 2, 0xaaaaaaaa, 0xfffffffa };
	unsigned int n, p;

	for (p = 0; p < ARRAY_SIZE(fills); p++) {
		for (n = 1; n <= SDR_HOST_OWN_POLLS; n++) {
			reset(n, 0, fills[p]);
			CHECK(sdr_request_host_owned(&io, wait_poll) == 0);
			CHECK(reads == n && waits == n - 1);
			check_sequence(n, true);
		}
	}
	reset(0, 0, 0);
	CHECK(sdr_request_host_owned(&io, wait_poll) == -ETIMEDOUT);
	CHECK(reads == SDR_HOST_OWN_POLLS && waits == SDR_HOST_OWN_POLLS - 1);
	check_sequence(SDR_HOST_OWN_POLLS, false);
	/* All-ones is a dead link: no acknowledgement, no retry. */
	for (n = 1; n <= SDR_HOST_OWN_POLLS; n++) {
		reset(0, n, 0);
		CHECK(sdr_request_host_owned(&io, wait_poll) == -EIO);
		CHECK(reads == n && waits == n - 1);
		check_sequence(n, false);
	}
	reset(2, 0, 0);
	wait_error = -EINTR;
	CHECK(sdr_request_host_owned(&io, wait_poll) == -EINTR);
	CHECK(reads == 1 && waits == 1);
	check_sequence(1, false);
	reset(1, 0, 0);
	CHECK(sdr_request_host_owned(NULL, wait_poll) == -EINVAL);
	CHECK(sdr_request_host_owned(&io, NULL) == -EINVAL);
	io.bar_size--;
	CHECK(sdr_request_host_owned(&io, wait_poll) == -EINVAL);
	io.bar_size++;
	io.ops = &missing;
	missing.write32 = NULL;
	CHECK(sdr_request_host_owned(&io, wait_poll) == -EINVAL);
	CHECK(!event_count);
	printf("ownership: %lu checks\n", checks);
	return 0;
}
