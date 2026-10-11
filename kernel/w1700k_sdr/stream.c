// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt7996-stream DEVICE --seconds S [--idle-ms N] [--go ADDR VALUE]
 *               [--stop ADDR VALUE] [--threads N] [--touch]
 *               [--data BYTES FILE] [--wrap] [--meta FILE] [--snap SLICES FILE]
 *               [--export IF SRC_IP DST_IP DST_MAC PORT] [--ports N]
 *               [--drain-ms N] [--export-cpu N] [--cpu N]
 *
 * One stream through the 1 GiB stream ring. The main thread runs the driver's
 * pump (STREAM_RX) for up to S seconds; the driver writes the go word to the
 * waiting producer, and on the deadline, a signal (SIGINT, SIGTERM, SIGHUP)
 * or --idle-ms without data writes the stop word and drains.
 * --cpu pins the pump thread.
 *
 * --threads readers (default 2, at most 3) follow the ring in place and count
 * what they see; --touch makes them read every payload word, as a consumer
 * would. A reader more than slices - slots behind counts as lapped.
 *
 * Afterwards --data writes up to BYTES of the stream's payload still held by
 * the ring, from the oldest completion kept or, with --wrap, centred on the
 * last wrap to slice 0; --meta writes each of those completions' ctrl word
 * (native u32). --snap: each SIGUSR1 copies the newest SLICES slices (64
 * behind the newest) to FILE and writes FILE.json {seq, chunk, slices, taken}.
 *
 * --export sends every slice as UDP from IF (see the uapi header); --ports
 * cycles the source port over N values from PORT, --drain-ms lets the export
 * catch up after pumping, --export-cpu pins the export thread.
 *
 * Prints one `stream` JSON line.
 */
#include "client.h"
#include <arpa/inet.h>
#include <net/if.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include "hw/mt7996/wfdma.h"

#define THREADS_MAX 3U
#define SNAP_MARGIN 64U

struct reader {
	pthread_t tid;
	uint64_t next, buffers, bytes, full, lapped, checksum;
};

struct options {
	double seconds;
	uint32_t idle_ms, threads, data_cap, pump_cpu, snap_slices;
	const char *data_path, *meta_path, *snap_path;
	int touch, wrap, export_set, cpu_set;
	struct w1700k_sdr_stream_rx req;
};

static const struct w1700k_sdr_stream_ring *ring;
static const uint8_t *slices;
static atomic_int pumping = 1;
static uint32_t reader_count, touch_payload;
static uint64_t snap_start;
static uint32_t snap_slices;
static const char *snap_path;

static int usage(void)
{
	fprintf(stderr,
		"usage: mt7996-stream DEVICE --seconds S [--idle-ms N] [--go ADDR VALUE]\n"
		"       [--stop ADDR VALUE] [--threads N] [--touch] [--data BYTES FILE] [--wrap]\n"
		"       [--meta FILE] [--snap SLICES FILE] [--export IF SRC_IP DST_IP DST_MAC PORT]\n"
		"       [--ports N] [--drain-ms N] [--export-cpu N] [--cpu N]\n");
	return 2;
}

static uint64_t completed(void)
{
	return __atomic_load_n(&ring->completed, __ATOMIC_ACQUIRE);
}

static uint32_t slice_len(uint64_t seq)
{
	return HW_GET(WFDMA_DESC_CTRL_SD_LEN0, ring->ctrl[seq % ring->slices]);
}

static const uint8_t *slice_at(uint64_t seq)
{
	return slices + (seq % ring->slices) * ring->slice_bytes;
}

/* Completions next, next + reader_count, ...: each read in place. */
static void *reader(void *arg)
{
	struct reader *r = arg;
	uint64_t c, s0, s1, behind = ring->slices - ring->slots;
	const uint64_t *w;
	uint32_t n, k;

	for (;;) {
		c = completed();
		if (r->next >= c) {
			if (!atomic_load(&pumping) && r->next >= completed())
				break;
			continue;
		}
		if (c - r->next > behind) {
			r->lapped++;
			r->next += reader_count;
			continue;
		}
		n = slice_len(r->next);
		if (touch_payload) {
			w = (const uint64_t *)slice_at(r->next);
			for (s0 = s1 = 0, k = 0; k + 1 < n / 8; k += 2) {
				s0 += w[k];
				s1 ^= w[k + 1];
			}
			r->checksum += s0 ^ s1;
			if (completed() - r->next > behind)	/* overwritten meanwhile */
				r->lapped++;
		}
		r->buffers++;
		r->bytes += n;
		r->full += n >= ring->slice_bytes / 2;
		r->next += reader_count;
	}
	return NULL;
}

static int snap_once(const char *tmp, const char *meta, uint32_t taken)
{
	uint64_t c = completed(), from, seq;
	FILE *f;

	if (c < snap_start + snap_slices + SNAP_MARGIN)
		return 0;
	from = c - SNAP_MARGIN - snap_slices;
	f = fopen(tmp, "wb");
	if (!f)
		return 0;
	for (seq = from; seq < from + snap_slices; seq++)
		fwrite(slice_at(seq), 1, slice_len(seq), f);
	fclose(f);
	if (completed() - from > ring->slices - ring->slots)	/* lapped: discard */
		return 0;
	if (rename(tmp, snap_path))
		return 0;
	f = fopen(meta, "w");
	if (f) {
		fprintf(f, "{\"seq\":%" PRIu64 ",\"chunk\":%" PRIu64 ",\"slices\":%" PRIu32
			",\"taken\":%" PRIu32 "}\n", from, from - snap_start, snap_slices, taken);
		fclose(f);
	}
	return 1;
}

/* SIGUSR1 is blocked everywhere else; this thread takes it. */
static void *snapper(void *arg)
{
	sigset_t *set = arg;
	char tmp[512], meta[512];
	uint32_t taken = 0;
	int sig;

	snprintf(tmp, sizeof(tmp), "%s.tmp", snap_path);
	snprintf(meta, sizeof(meta), "%s.json", snap_path);
	for (;;)
		if (!sigwait(set, &sig))
			taken += snap_once(tmp, meta, taken + 1);
	return NULL;
}

static int dump(const struct options *o, uint64_t from, uint64_t end)
{
	FILE *d = o->data_path ? fopen(o->data_path, "wb") : NULL;
	FILE *m = o->meta_path ? fopen(o->meta_path, "wb") : NULL;
	uint64_t seq, written = 0;
	uint32_t ctrl, n;
	int ret = 0;

	if ((o->data_path && !d) || (o->meta_path && !m))
		ret = -1;
	for (seq = from; !ret && seq < end; seq++) {
		ctrl = ring->ctrl[seq % ring->slices];
		n = HW_GET(WFDMA_DESC_CTRL_SD_LEN0, ctrl);
		if (d && written + n <= o->data_cap) {
			if (fwrite(slice_at(seq), 1, n, d) != n)
				ret = -1;
			written += n;
		}
		if (m && fwrite(&ctrl, sizeof(ctrl), 1, m) != 1)
			ret = -1;
	}
	if (d && fclose(d))
		ret = -1;
	if (m && fclose(m))
		ret = -1;
	return ret;
}

/* IF SRC_IP DST_IP DST_MAC PORT */
static int export_target(char **argv, struct w1700k_sdr_stream_export *e)
{
	struct in_addr src, dst;
	unsigned int mac[6];
	uint32_t port;
	int k;

	e->ifindex = if_nametoindex(argv[0]);
	if (!e->ifindex || inet_pton(AF_INET, argv[1], &src) != 1 ||
	    inet_pton(AF_INET, argv[2], &dst) != 1 ||
	    sscanf(argv[3], "%x:%x:%x:%x:%x:%x", &mac[0], &mac[1], &mac[2], &mac[3],
		   &mac[4], &mac[5]) != 6 ||
	    client_u32(argv[4], &port) || !port || port > 0xffff)
		return -1;
	e->src_ip = src.s_addr;
	e->dst_ip = dst.s_addr;
	e->src_port = htons((uint16_t)port);
	e->dst_port = htons((uint16_t)port);
	for (k = 0; k < 6; k++)
		e->dst_mac[k] = (uint8_t)mac[k];
	return 0;
}

static int parse(int argc, char **argv, struct options *o)
{
	struct w1700k_sdr_stream_rx *req = &o->req;
	uint32_t value;
	int i, left;

	o->threads = 2;
	for (i = 2; i < argc; i++) {
		const char *a = argv[i];

		left = argc - i - 1;
		if (!strcmp(a, "--seconds") && left >= 1) {
			o->seconds = strtod(argv[++i], NULL);
		} else if (!strcmp(a, "--idle-ms") && left >= 1) {
			if (client_u32(argv[++i], &req->idle_ms))
				return -1;
		} else if (!strcmp(a, "--go") && left >= 2) {
			if (client_u32(argv[i + 1], &req->go_address) ||
			    client_u32(argv[i + 2], &req->go_value))
				return -1;
			i += 2;
		} else if (!strcmp(a, "--stop") && left >= 2) {
			if (client_u32(argv[i + 1], &req->stop_address) ||
			    client_u32(argv[i + 2], &req->stop_value))
				return -1;
			i += 2;
		} else if (!strcmp(a, "--threads") && left >= 1) {
			if (client_u32(argv[++i], &o->threads) || o->threads > THREADS_MAX)
				return -1;
		} else if (!strcmp(a, "--touch")) {
			o->touch = 1;
		} else if (!strcmp(a, "--data") && left >= 2) {
			if (client_u32(argv[++i], &o->data_cap))
				return -1;
			o->data_path = argv[++i];
		} else if (!strcmp(a, "--wrap")) {
			o->wrap = 1;
		} else if (!strcmp(a, "--meta") && left >= 1) {
			o->meta_path = argv[++i];
		} else if (!strcmp(a, "--snap") && left >= 2) {
			if (client_u32(argv[++i], &o->snap_slices) || !o->snap_slices ||
			    o->snap_slices > 4096)
				return -1;
			o->snap_path = argv[++i];
		} else if (!strcmp(a, "--export") && left >= 5) {
			if (export_target(argv + i + 1, &req->export))
				return -1;
			i += 5;
		} else if (!strcmp(a, "--ports") && left >= 1) {
			if (client_u32(argv[++i], &value) || !value || value > 256)
				return -1;
			req->export.src_ports = (uint16_t)value;
			o->export_set = 1;
		} else if (!strcmp(a, "--drain-ms") && left >= 1) {
			if (client_u32(argv[++i], &req->export.drain_ms))
				return -1;
			o->export_set = 1;
		} else if (!strcmp(a, "--export-cpu") && left >= 1) {
			if (client_u32(argv[++i], &value) || value > 63)
				return -1;
			req->export.cpu = (uint16_t)(value + 1);
			o->export_set = 1;
		} else if (!strcmp(a, "--cpu") && left >= 1) {
			if (client_u32(argv[++i], &o->pump_cpu) || o->pump_cpu > 63)
				return -1;
			o->cpu_set = 1;
		} else {
			return -1;
		}
	}
	if (o->export_set && !req->export.ifindex)
		return -1;
	if (!(o->seconds > 0 && o->seconds * 1000 <= W1700K_SDR_STREAM_TIMEOUT_MAX))
		return -1;
	req->timeout_ms = (uint32_t)(o->seconds * 1000);
	return 0;
}

static void on_signal(int sig)
{
	(void)sig;	/* interrupts the pump, which then stops the producer */
}

/*
 * The stop signals must reach the pump thread, so the helper threads are
 * created with them blocked. SIGUSR1 stays blocked everywhere when snapping;
 * the snapper takes it with sigwait.
 */
static int start_threads(const struct options *o, struct reader *readers, uint64_t start)
{
	static sigset_t usr1;
	sigset_t stops;
	pthread_t tid;
	uint32_t i;

	sigemptyset(&stops);
	sigaddset(&stops, SIGINT);
	sigaddset(&stops, SIGTERM);
	sigaddset(&stops, SIGHUP);
	sigemptyset(&usr1);
	sigaddset(&usr1, SIGUSR1);
	if (pthread_sigmask(SIG_BLOCK, &stops, NULL))
		return -1;
	if (o->snap_path && (pthread_sigmask(SIG_BLOCK, &usr1, NULL) ||
			     pthread_create(&tid, NULL, snapper, &usr1)))
		return -1;
	for (i = 0; i < o->threads; i++) {
		readers[i] = (struct reader){ .next = start + i };
		if (pthread_create(&readers[i].tid, NULL, reader, &readers[i]))
			return -1;
	}
	return pthread_sigmask(SIG_UNBLOCK, &stops, NULL) ? -1 : 0;
}

static int pin_pump(uint32_t cpu)
{
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	return sched_setaffinity(0, sizeof(set), &set);
}

/* The completion window the dump covers. */
static uint64_t dump_from(const struct options *o, const struct w1700k_sdr_stream_rx *r,
			  uint64_t end)
{
	uint64_t held = (uint64_t)r->slices - r->slots;
	uint64_t kept = end - r->first_seq < held ? r->first_seq : end - held;
	uint64_t half = o->data_cap / r->slice_bytes / 2, wrap;

	if (!o->wrap)
		return kept;
	wrap = end / r->slices * r->slices;
	if (wrap < half || wrap - half < kept || wrap >= end)
		return kept;
	return wrap - half;
}

int main(int argc, char **argv)
{
	static struct options o;
	struct w1700k_sdr_stream_rx *req = &o.req, geometry;
	struct sigaction sa = { .sa_handler = on_signal };
	struct reader readers[THREADS_MAX];
	uint64_t start, end, from, buffers = 0, bytes = 0, full = 0, lapped = 0, checksum = 0;
	size_t map_bytes;
	uint32_t i;
	void *map;
	int fd, rc, err = 0, dumped = 0;

	if (argc < 2 || parse(argc, argv, &o))
		return usage();
	reader_count = o.threads;
	touch_payload = o.touch;
	snap_slices = o.snap_slices;
	snap_path = o.snap_path;
	fd = client_open(argv[1], O_RDWR, W1700K_SDR_CAP_STREAM_RX);
	if (fd < 0)
		return 1;
	memset(&geometry, 0, sizeof(geometry));	/* timeout 0: geometry only */
	if (ioctl(fd, W1700K_SDR_STREAM_RX, &geometry) || !geometry.slices)
		return client_fail("STREAM_RX geometry");
	map_bytes = geometry.header_bytes + (size_t)geometry.slices * geometry.slice_bytes;
	map = mmap(NULL, map_bytes, PROT_READ, MAP_SHARED, fd, 0);
	if (map == MAP_FAILED)
		return client_fail("mmap");
	ring = map;
	slices = (const uint8_t *)map + geometry.header_bytes;
	/* No SA_RESTART: a stop signal only interrupts the pump. */
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	start = snap_start = completed();
	if (start_threads(&o, readers, start))
		return client_fail("pthread_create");
	if (o.cpu_set && pin_pump(o.pump_cpu))
		return client_fail("sched_setaffinity");
	rc = ioctl(fd, W1700K_SDR_STREAM_RX, req);
	if (rc)
		err = errno;
	atomic_store(&pumping, 0);
	for (i = 0; i < o.threads; i++) {
		pthread_join(readers[i].tid, NULL);
		buffers += readers[i].buffers;
		bytes += readers[i].bytes;
		full += readers[i].full;
		lapped += readers[i].lapped;
		checksum += readers[i].checksum;
	}
	end = req->first_seq + req->buffers;
	from = dump_from(&o, req, end);
	if (o.data_path || o.meta_path)
		dumped = dump(&o, from, end) ? -1 : 1;
	printf("{\"at\":\"stream\",\"errno\":%d,\"result\":%" PRId32 ",\"buffers\":%" PRIu64
	       ",\"malformed\":%" PRIu32 ",\"bytes\":%" PRIu64 ",\"full_buffers\":%" PRIu64
	       ",\"first_seq\":%" PRIu64 ",\"first_ns\":%" PRIu64 ",\"last_ns\":%" PRIu64
	       ",\"start_ns\":%" PRIu64 ",\"end_ns\":%" PRIu64 ",\"stop_ns\":%" PRIu64
	       ",\"max_backlog\":%" PRIu32 ",\"slots\":%" PRIu32 ",\"slices\":%" PRIu32
	       ",\"slice_bytes\":%" PRIu32 ",\"readers\":%" PRIu32 ",\"touch\":%d"
	       ",\"read_buffers\":%" PRIu64 ",\"read_bytes\":%" PRIu64 ",\"read_full\":%" PRIu64
	       ",\"lapped\":%" PRIu64 ",\"checksum\":\"%016" PRIx64 "\",\"start_seq\":%" PRIu64
	       ",\"dump_from_seq\":%" PRIu64 ",\"dumped\":%d"
	       ",\"export\":{\"result\":%" PRId32 ",\"slices\":%" PRIu64 ",\"busy\":%" PRIu64
	       ",\"unsent\":%" PRIu64 ",\"last_ns\":%" PRIu64 ",\"max_lag\":%" PRIu32
	       ",\"inflight_max\":%" PRIu32 "}}\n",
	       err, req->result, (uint64_t)req->buffers, req->malformed, (uint64_t)req->bytes,
	       (uint64_t)req->full_buffers, (uint64_t)req->first_seq, (uint64_t)req->first_ns,
	       (uint64_t)req->last_ns, (uint64_t)req->start_ns, (uint64_t)req->end_ns,
	       (uint64_t)req->stop_ns, req->max_backlog, req->slots, req->slices,
	       req->slice_bytes, o.threads, o.touch, buffers, bytes, full, lapped, checksum,
	       start, from, dumped, req->export_result, (uint64_t)req->export_slices,
	       (uint64_t)req->export_busy, (uint64_t)req->export_unsent,
	       (uint64_t)req->export_last_ns, req->export_max_lag, req->export_inflight_max);
	munmap(map, map_bytes);
	close(fd);
	return err || dumped < 0 || fflush(stdout) ? 1 : 0;
}
