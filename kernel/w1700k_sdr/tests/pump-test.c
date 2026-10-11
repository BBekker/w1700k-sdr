// SPDX-License-Identifier: GPL-2.0-only
/*
 * pump.c against a simulated producer: whatever ends a stream, a producer
 * with a stop word is told to stop exactly once and the ring is drained.
 */
#include "harness.h"
#include "pump.c"

#define MS 1000000ULL

static u64 t;			/* simulated clock */
static u64 produce_until;	/* the producer publishes until then */
static u64 stop_tail;		/* and for this long after the stop word */
static u64 period, next_at;	/* one completion per period */
static u64 seq_next, signal_at, error_at;
static unsigned int burst, stop_writes, idle_services, busy_services;
static int stop_error, ring_error;
static bool fatal_signal, malformed_every_third;
static u32 stop_seen_address, stop_seen_value;
static u64 stop_seen_at;

static int next(void *ctx, u32 *ctrl, u64 *seq)
{
	(void)ctx;
	if (error_at && t >= error_at)
		return ring_error;
	if (burst) {
		burst--;
	} else {
		if (t >= produce_until || t < next_at)
			return -EAGAIN;
		next_at += period;
	}
	*seq = seq_next++;
	*ctrl = (*seq % 4 ? 8192U : 1000U) << 16 | 0xc0000000;
	return malformed_every_third && !(*seq % 3) ? -EBADMSG : 0;
}

static int stop(void *ctx, u32 address, u32 value)
{
	(void)ctx;
	stop_writes++;
	stop_seen_address = address;
	stop_seen_value = value;
	stop_seen_at = t;
	if (!stop_error && produce_until > t + stop_tail)
		produce_until = t + stop_tail;
	return stop_error;
}

static void service(void *ctx, bool idle)
{
	(void)ctx;
	if (idle) {
		idle_services++;
		t += MS / 10;	/* an empty sweep */
	} else {
		busy_services++;
	}
}

static bool interrupted(void *ctx, bool *fatal)
{
	(void)ctx;
	*fatal = signal_at && t >= signal_at && fatal_signal;
	return signal_at && t >= signal_at;
}

static u64 now(void *ctx)
{
	(void)ctx;
	return t;
}

static const struct sdr_pump_ops ops = { next, stop, service, interrupted, now };

static struct w1700k_sdr_stream_rx request(u32 timeout_ms, u32 idle_ms, bool stop_word)
{
	struct w1700k_sdr_stream_rx req;

	memset(&req, 0, sizeof(req));
	req.timeout_ms = timeout_ms;
	req.idle_ms = idle_ms;
	req.slice_bytes = 8192;
	if (stop_word) {
		req.stop_address = 0x0090f008;
		req.stop_value = 0x504f5453;
	}
	return req;
}

static void producer(u64 until, u64 tail)
{
	t = 1000 * MS;
	produce_until = until ? t + until : UINT64_MAX;
	stop_tail = tail;
	period = MS;
	next_at = t;
	seq_next = 77;
	signal_at = error_at = 0;
	burst = stop_writes = idle_services = busy_services = 0;
	stop_error = ring_error = 0;
	fatal_signal = malformed_every_third = false;
}

/* Sum of lengths and full-slice count over every completion pumped. */
static void check_lengths(const struct w1700k_sdr_stream_rx *req)
{
	u64 seq, bytes = 0, full = 0;

	for (seq = req->first_seq; seq < req->first_seq + req->buffers; seq++) {
		bytes += seq % 4 ? 8192 : 1000;
		full += seq % 4 != 0;
	}
	CHECK(req->bytes == bytes && req->full_buffers == full);
}

/* Stopped once at `at` (relative), then drained until 300 ms quiet. */
static void stopped_and_drained(const struct w1700k_sdr_stream_rx *req, u64 at)
{
	CHECK(stop_writes == 1 && stop_seen_address == 0x0090f008 &&
	      stop_seen_value == 0x504f5453);
	CHECK(req->stop_ns == stop_seen_at && stop_seen_at >= 1000 * MS + at &&
	      stop_seen_at < 1000 * MS + at + MS);
	CHECK(t >= (req->last_ns > req->stop_ns ? req->last_ns : req->stop_ns) + 300 * MS);
	CHECK(t < (req->last_ns > req->stop_ns ? req->last_ns : req->stop_ns) + 301 * MS);
}

static void ends(void)
{
	struct w1700k_sdr_stream_rx req;

	/* Deadline: completions up to it, then 20 ms more after the stop word. */
	producer(0, 20 * MS);
	req = request(100, 0, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == 0);
	stopped_and_drained(&req, 100 * MS);
	CHECK(req.buffers == seq_next - 77 && req.first_seq == 77 && req.buffers > 115);
	CHECK(req.first_ns == 1000 * MS && req.start_ns == 1000 * MS);
	check_lengths(&req);
	/* A signal, a fatal signal, device removal. */
	producer(0, 5 * MS);
	signal_at = t + 30 * MS;
	req = request(100000, 0, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == 0);
	stopped_and_drained(&req, 30 * MS);
	producer(0, 5 * MS);
	signal_at = t + 30 * MS;
	fatal_signal = true;
	req = request(100000, 0, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == -EINTR);
	stopped_and_drained(&req, 30 * MS);
	/* The producer pauses: idle_ms ends the stream, and it is still told. */
	producer(50 * MS, 0);
	req = request(100000, 200, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == 0);
	CHECK(stop_writes == 1 && req.stop_ns >= req.last_ns + 200 * MS &&
	      req.stop_ns < req.last_ns + 201 * MS);
	CHECK(t >= req.stop_ns + 300 * MS);
	/* An idle window longer than 300 ms is also the quiet window. */
	producer(50 * MS, 0);
	req = request(100000, 700, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == 0);
	CHECK(t >= req.stop_ns + 700 * MS && t < req.stop_ns + 701 * MS);
	/* Before any completion idle_ms does not end the stream. */
	producer(1, 0);
	produce_until = 0;
	req = request(50, 10, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == 0 && !req.buffers);
	CHECK(stop_writes == 1 && req.stop_ns >= 1050 * MS);
}

static void failures(void)
{
	struct w1700k_sdr_stream_rx req;

	/* A ring error: the producer is told, no drain is possible. */
	producer(0, 10 * MS);
	error_at = t + 40 * MS;
	ring_error = -EIO;
	req = request(100000, 0, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == -EIO);
	CHECK(stop_writes == 1 && req.stop_ns >= 1040 * MS && t < 1041 * MS);
	/* A failing stop write ends the stream with its error. */
	producer(0, 10 * MS);
	stop_error = -ERANGE;
	req = request(30, 0, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == -ERANGE);
	CHECK(stop_writes == 1 && !req.stop_ns);
	/* A producer that never quiets: the drain is capped at 5 s. */
	producer(0, 3600 * 1000 * MS);
	req = request(20, 0, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == 0);
	CHECK(stop_writes == 1 && t >= req.stop_ns + 5000 * MS && t < req.stop_ns + 5001 * MS);
	/* Without a stop word the stream simply ends. */
	producer(0, 0);
	req = request(20, 0, false);
	CHECK(sdr_pump_run(&ops, NULL, &req) == 0);
	CHECK(!stop_writes && !req.stop_ns && t >= 1020 * MS && t < 1021 * MS);
	producer(0, 0);
	error_at = t + 5 * MS;
	ring_error = -EIO;
	req = request(20, 0, false);
	CHECK(sdr_pump_run(&ops, NULL, &req) == -EIO && !stop_writes);
}

static void accounting(void)
{
	struct w1700k_sdr_stream_rx req;

	/* A pump that is never idle still serves CTL every 1024 completions. */
	producer(1, 0);
	produce_until = 0;
	burst = 3000;
	malformed_every_third = true;
	req = request(10, 0, true);
	CHECK(sdr_pump_run(&ops, NULL, &req) == 0);
	CHECK(req.buffers == 3000 && busy_services == 2 && req.max_backlog == 3000);
	CHECK(req.malformed == 1000);
	CHECK(idle_services > 0);
}

int main(void)
{
	ends();
	failures();
	accounting();
	printf("pump: %lu checks\n", checks);
	return 0;
}
