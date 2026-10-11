// SPDX-License-Identifier: GPL-2.0-only
/* sdr-recv: records or validates the unit's UDP sample export on the host.
 *
 *   sdr-recv --bind IP [--bind IP] --port P [--threads N] [--seconds S]
 *            [--idle-ms I] [--wait S] [--capacity-mib M] [--out PATH]
 *            [--report S] [--horizon N]
 *
 * For each --bind (the 10G port's address, and optionally a second path's), N
 * threads each own a UDP socket on IP:P (SO_REUSEPORT; the unit cycles its
 * source port so the flows spread over sockets and NIC queues) and take
 * datagrams in batches. Each datagram is a struct w1700k_sdr_export_header and
 * a piece of one ring slice. Receiving ends S seconds after the first
 * datagram, after I ms with none, on SIGINT/SIGTERM, or --wait seconds if none
 * ever came.
 *
 * With --out, every completion is kept (slot seq % capacity) and written at
 * the end in seq order, each its slice_bytes long (packed slices stay
 * packed), with a sidecar PATH.lengths of native u32 lengths, 0 for a
 * completion that never arrived.
 *
 * Without --out it validates as it goes, for runs of any length: a
 * completion's slot holds only its seq, bytes received and a hash, and a
 * finalizer --horizon completions behind the newest seq classifies each one
 * whole, short, missing or doubled and frees the slot. Every raw slice's words
 * must have the spare bits [31:30]/[15:14] clear; every whole slice is also
 * compared with the slice one ICAP ring lap (1 MiB, 128 slices) earlier,
 * which a reader that overtook the capture would repeat, and so is every
 * 32-byte line (28 packed) of a slice that came in one datagram, which a
 * packer reading a stale D-cache line would repeat. A JSON line goes to
 * stdout every --report seconds, and one at the end.
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define MAGIC 0x31524453U
#define SLICE 8192U
#define LAP 128U			/* slices in the 1 MiB ICAP ring */
#define BATCH 64
#define DGRAM 9216
#define THREADS_MAX 32
#define BINDS_MAX 2
#define EXAMPLES 8

struct header {
	uint32_t magic;
	uint16_t offset, slice_bytes;
	uint64_t seq;
} __attribute__((packed));

struct worker {
	pthread_t tid;
	int fd;
	uint64_t datagrams, bytes, bad, clashes, drops, spare_words;
	uint64_t lines, stale_lines, flat_lines, line_skips, stale_line_ex[EXAMPLES];
	unsigned n_stale_line_ex;
	uint8_t (*bufs)[DGRAM];
};

static uint8_t *data;			/* --out only */
static _Atomic uint32_t *got;		/* bytes received per slot */
static _Atomic uint64_t *tag;		/* seq + 1 of the slot's owner, 0 = free */
static _Atomic uint64_t *hash;		/* per slot: sum of the pieces' hashes */
static uint16_t *lengths;
static uint64_t capacity, horizon;
static _Atomic uint64_t seq_min = UINT64_MAX, seq_max, first_any_ns, last_any_ns;
static double seconds = 10, wait_s = 120, report_s = 10;
static uint32_t idle_ms = 1000;
static uint64_t started_ns;
static int keep;
static const char *out_path = "";
static volatile sig_atomic_t stop_requested;
static atomic_int receiving = 1;

/* Line-level staleness: a packer that read a line the D-cache kept from the ring's last
 * lap would send it again, so each 32-byte line (28 packed) is fingerprinted and compared with
 * the same line of the slice one lap earlier. Noise never repeats; a flat line (all words
 * equal: the radio does output those) legitimately can, so it is counted, not compared.
 */
#define LINES 256
static uint64_t (*line_fp)[LINES];
static _Atomic uint64_t lap_line_seq[LAP];	/* seq + 1 whose lines the position holds */
static atomic_flag lap_lock[LAP];

/* The finalizer's tally. */
static uint64_t fin_next, whole, shortn, missing, over, stale, lap_hashes[LAP], lap_seq[LAP];
static uint32_t lap_len[LAP];
static uint64_t missing_ex[EXAMPLES], short_ex[EXAMPLES], over_ex[EXAMPLES], stale_ex[EXAMPLES];
static unsigned n_missing_ex, n_short_ex, n_over_ex, n_stale_ex;

static uint64_t now_ns(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (uint64_t)t.tv_sec * 1000000000u + (uint64_t)t.tv_nsec;
}

static void atomic_min(_Atomic uint64_t *a, uint64_t v)
{
	uint64_t o = atomic_load(a);

	while (v < o && !atomic_compare_exchange_weak(a, &o, v))
		;
}

static void atomic_max(_Atomic uint64_t *a, uint64_t v)
{
	uint64_t o = atomic_load(a);

	while (v > o && !atomic_compare_exchange_weak(a, &o, v))
		;
}

static void on_signal(int sig)
{
	(void)sig;
	stop_requested = 1;
}

/* A piece's words: a position-keyed hash, so the pieces of a slice add up to
 * the same value however the slice was cut, and (when `spare` is given) the
 * count of words with spare bits set.
 */
static uint64_t piece(const uint8_t *p, uint32_t len, uint32_t offset, uint64_t *spare)
{
	const uint32_t *w = (const void *)p;
	uint64_t h = 0, s = 0;
	uint32_t i, n = len / 4, k = offset / 4;

	for (i = 0; i < n; i++) {
		s += (w[i] & 0xc000c000u) != 0;
		h += ((uint64_t)w[i] + 0x9e3779b97f4a7c15ull * (k + i + 1)) * 0xff51afd7ed558ccdull;
	}
	if (spare)
		*spare += s;
	return h;
}

static void example(uint64_t *ex, unsigned *n, uint64_t seq);

static void lines_check(struct worker *w, uint64_t seq, const uint8_t *p, uint32_t len, uint32_t line)
{
	uint32_t pos = seq % LAP, i, j, n = len / line, flat;
	const uint32_t *v;
	uint64_t fp, stale = 0;
	int compare;

	if (n > LINES || atomic_flag_test_and_set(&lap_lock[pos])) {
		w->line_skips++;
		return;
	}
	compare = atomic_load(&lap_line_seq[pos]) == seq - LAP + 1;
	for (i = 0; i < n; i++) {
		v = (const void *)(p + i * line);
		for (flat = 1, fp = 0, j = 0; j < line / 4; j++) {
			flat &= v[j] == v[0];
			fp = (fp ^ v[j]) * 0x100000001b3ull + j;
		}
		if (flat) {
			w->flat_lines++;
			fp = 0;
		} else if (compare && line_fp[pos][i] == fp) {
			stale++;
		}
		line_fp[pos][i] = fp;
	}
	atomic_store(&lap_line_seq[pos], seq + 1);
	atomic_flag_clear(&lap_lock[pos]);
	w->lines += n;
	if (stale) {
		w->stale_lines += stale;
		example(w->stale_line_ex, &w->n_stale_line_ex, seq);
	}
}

static void take(struct worker *w, const uint8_t *buf, size_t n)
{
	const struct header *h = (const void *)buf;
	uint64_t slot, owner, expect = 0;
	uint32_t len;

	if (n < sizeof(*h) || h->magic != MAGIC || h->slice_bytes > SLICE ||
	    (size_t)h->offset + (n - sizeof(*h)) > h->slice_bytes) {
		w->bad++;
		return;
	}
	len = (uint32_t)(n - sizeof(*h));
	slot = h->seq % capacity;
	owner = h->seq + 1;
	/* A slot belongs to one seq until the finalizer frees it; another seq
	 * landing there means the window was outrun.
	 */
	if (!atomic_compare_exchange_strong(&tag[slot], &expect, owner) && expect != owner) {
		w->clashes++;
		return;
	}
	if (keep)
		memcpy(data + slot * SLICE + h->offset, buf + sizeof(*h), len);
	/* A packed slice (7168 bytes) carries every 8th sample in those bits. */
	atomic_fetch_add(&hash[slot], piece(buf + sizeof(*h), len, h->offset,
					    h->slice_bytes == SLICE ? &w->spare_words : NULL));
	lengths[slot] = h->slice_bytes;
	if (h->offset == 0 && len == h->slice_bytes)
		lines_check(w, h->seq, buf + sizeof(*h), len, h->slice_bytes == SLICE ? 32 : 28);
	atomic_fetch_add(&got[slot], len);
	atomic_min(&seq_min, h->seq);
	atomic_max(&seq_max, h->seq);
	w->datagrams++;
	w->bytes += len;
}

static void *work(void *arg)
{
	struct worker *w = arg;
	uint8_t (*bufs)[DGRAM] = w->bufs;
	char ctl[BATCH][CMSG_SPACE(sizeof(uint32_t))];
	struct mmsghdr msgs[BATCH];
	struct iovec iov[BATCH];
	struct cmsghdr *c;
	uint64_t t, first;
	int k, n;

	while (atomic_load(&receiving)) {
		for (k = 0; k < BATCH; k++) {
			iov[k] = (struct iovec){ bufs[k], DGRAM };
			msgs[k].msg_hdr = (struct msghdr){ .msg_iov = &iov[k], .msg_iovlen = 1,
							   .msg_control = ctl[k],
							   .msg_controllen = sizeof(ctl[k]) };
		}
		/* recvmmsg's own timeout only runs once a datagram has come; the
		 * socket's SO_RCVTIMEO bounds the wait for the first.
		 */
		n = recvmmsg(w->fd, msgs, BATCH, MSG_WAITFORONE, NULL);
		t = now_ns();
		if (n <= 0) {
			if (n < 0 && errno != EAGAIN && errno != EINTR)
				break;
			continue;
		}
		first = atomic_load(&first_any_ns);
		if (!first) {
			uint64_t zero = 0;

			atomic_compare_exchange_strong(&first_any_ns, &zero, t);
		}
		atomic_max(&last_any_ns, t);
		for (k = 0; k < n; k++) {
			take(w, bufs[k], msgs[k].msg_len);
			for (c = CMSG_FIRSTHDR(&msgs[k].msg_hdr); c; c = CMSG_NXTHDR(&msgs[k].msg_hdr, c))
				if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SO_RXQ_OVFL)
					memcpy(&w->drops, CMSG_DATA(c), sizeof(uint32_t));
		}
	}
	return NULL;
}

static void example(uint64_t *ex, unsigned *n, uint64_t seq)
{
	if (*n < EXAMPLES)
		ex[(*n)++] = seq;
}

/* Classify completion seq and, when validating, free its slot. */
static void finalize(uint64_t seq)
{
	uint64_t slot = seq % capacity, h;
	uint32_t g, l;

	if (atomic_load(&tag[slot]) != seq + 1) {
		missing++;
		example(missing_ex, &n_missing_ex, seq);
		return;
	}
	g = atomic_load(&got[slot]);
	l = lengths[slot];
	h = atomic_load(&hash[slot]);
	if (g < l) {
		shortn++;
		example(short_ex, &n_short_ex, seq);
	} else if (g > l) {
		over++;
		example(over_ex, &n_over_ex, seq);
	} else {
		whole++;
		/* raw or packed: a stale one repeats its length and its words */
		if (lap_seq[seq % LAP] == seq - LAP + 1 && lap_hashes[seq % LAP] == h && lap_len[seq % LAP] == l) {
			stale++;
			example(stale_ex, &n_stale_ex, seq);
		}
		lap_seq[seq % LAP] = seq + 1;
		lap_hashes[seq % LAP] = h;
		lap_len[seq % LAP] = l;
	}
	if (!keep) {
		atomic_store(&got[slot], 0);
		atomic_store(&hash[slot], 0);
		atomic_store(&tag[slot], 0);
	}
}

/* Udp: RcvbufErrors, InCsumErrors and InErrors from /proc/net/snmp. */
static void udp_errors(uint64_t *rcvbuf, uint64_t *csum, uint64_t *inerr)
{
	char line[1024], names[1024] = "";
	FILE *f = fopen("/proc/net/snmp", "r");
	char *a, *b, *sa, *sb;

	*rcvbuf = *csum = *inerr = 0;
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "Udp:", 4))
			continue;
		if (!names[0]) {
			strcpy(names, line);
			continue;
		}
		for (a = strtok_r(names, " \n", &sa), b = strtok_r(line, " \n", &sb); a && b;
		     a = strtok_r(NULL, " \n", &sa), b = strtok_r(NULL, " \n", &sb)) {
			if (!strcmp(a, "RcvbufErrors"))
				*rcvbuf = strtoull(b, NULL, 10);
			else if (!strcmp(a, "InCsumErrors"))
				*csum = strtoull(b, NULL, 10);
			else if (!strcmp(a, "InErrors"))
				*inerr = strtoull(b, NULL, 10);
		}
		break;
	}
	fclose(f);
}

static void list(const char *name, const uint64_t *ex, unsigned n)
{
	unsigned i;

	printf(",\"%s\":[", name);
	for (i = 0; i < n; i++)
		printf("%s%" PRIu64, i ? "," : "", ex[i]);
	printf("]");
}

static void report(const char *at, const char *where, struct worker *w, uint32_t n, uint64_t rb0,
		   uint64_t cs0, uint64_t ie0)
{
	uint64_t dgrams = 0, bytes = 0, bad = 0, drops = 0, clashes = 0, spare = 0, rb, cs, ie;
	uint64_t lines = 0, stale_lines = 0, flat = 0, skips = 0, ex[EXAMPLES];
	uint64_t first = atomic_load(&first_any_ns), last = atomic_load(&last_any_ns);
	uint32_t i, j;
	unsigned nex = 0;

	for (i = 0; i < n; i++) {
		dgrams += w[i].datagrams;
		bytes += w[i].bytes;
		bad += w[i].bad;
		drops += w[i].drops;
		clashes += w[i].clashes;
		spare += w[i].spare_words;
		lines += w[i].lines;
		stale_lines += w[i].stale_lines;
		flat += w[i].flat_lines;
		skips += w[i].line_skips;
		for (j = 0; j < w[i].n_stale_line_ex && nex < EXAMPLES; j++)
			ex[nex++] = w[i].stale_line_ex[j];
	}
	udp_errors(&rb, &cs, &ie);
	printf("{\"at\":\"%s\",\"bind\":\"%s\",\"threads\":%u,\"elapsed_s\":%.1f,\"datagrams\":%" PRIu64
	       ",\"bytes\":%" PRIu64 ",\"mbps\":%.1f,\"bad\":%" PRIu64 ",\"seq_first\":%" PRIu64
	       ",\"seq_last\":%" PRIu64 ",\"finalized\":%" PRIu64 ",\"whole\":%" PRIu64 ",\"short\":%" PRIu64
	       ",\"missing\":%" PRIu64 ",\"over\":%" PRIu64 ",\"stale\":%" PRIu64 ",\"spare_words\":%" PRIu64
	       ",\"clashes\":%" PRIu64 ",\"socket_drops\":%" PRIu64 ",\"udp_rcvbuf_errors\":%" PRIu64
	       ",\"udp_csum_errors\":%" PRIu64 ",\"udp_in_errors\":%" PRIu64,
	       at, where, n, first ? (last - first) / 1e9 : 0.0, dgrams, bytes,
	       last > first ? bytes / ((last - first) / 1e9) / 1e6 : 0.0, bad,
	       dgrams ? atomic_load(&seq_min) : 0, dgrams ? atomic_load(&seq_max) : 0,
	       whole + shortn + missing + over, whole, shortn, missing, over, stale, spare, clashes, drops,
	       rb - rb0, cs - cs0, ie - ie0);
	list("missing_examples", missing_ex, n_missing_ex);
	list("short_examples", short_ex, n_short_ex);
	list("over_examples", over_ex, n_over_ex);
	list("stale_examples", stale_ex, n_stale_ex);
	printf(",\"lines\":%" PRIu64 ",\"stale_lines\":%" PRIu64 ",\"flat_lines\":%" PRIu64
	       ",\"line_skips\":%" PRIu64, lines, stale_lines, flat, skips);
	list("stale_line_examples", ex, nex);
	printf(",\"out\":\"%s\"}\n", out_path);
	fflush(stdout);
}

int main(int argc, char **argv)
{
	struct worker w[THREADS_MAX * BINDS_MAX] = {};
	const char *bind_ip[BINDS_MAX] = {}, *out = NULL;
	uint32_t port = 0, threads = 8, cap_mib = 0, i, binds = 0, n;
	char where[64 * BINDS_MAX] = "";
	uint64_t rb0, cs0, ie0, t, last_report, hi, s;
	struct sockaddr_in sa[BINDS_MAX] = {};
	int one = 1, rcvbuf = 48 << 20, started = 0;
	struct timeval tick = { 0, 50000 };
	struct sigaction act = { .sa_handler = on_signal };
	FILE *f = NULL, *fl = NULL;
	char path[4096];
	int failed = 0;

	for (i = 1; i < (uint32_t)argc; i++) {
		if (!strcmp(argv[i], "--bind") && i + 1 < (uint32_t)argc && binds < BINDS_MAX)
			bind_ip[binds++] = argv[++i];
		else if (!strcmp(argv[i], "--port") && i + 1 < (uint32_t)argc)
			port = (uint32_t)strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--threads") && i + 1 < (uint32_t)argc)
			threads = (uint32_t)strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--seconds") && i + 1 < (uint32_t)argc)
			seconds = strtod(argv[++i], NULL);
		else if (!strcmp(argv[i], "--wait") && i + 1 < (uint32_t)argc)
			wait_s = strtod(argv[++i], NULL);
		else if (!strcmp(argv[i], "--idle-ms") && i + 1 < (uint32_t)argc)
			idle_ms = (uint32_t)strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--capacity-mib") && i + 1 < (uint32_t)argc)
			cap_mib = (uint32_t)strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "--out") && i + 1 < (uint32_t)argc)
			out = argv[++i];
		else if (!strcmp(argv[i], "--report") && i + 1 < (uint32_t)argc)
			report_s = strtod(argv[++i], NULL);
		else if (!strcmp(argv[i], "--horizon") && i + 1 < (uint32_t)argc)
			horizon = strtoull(argv[++i], NULL, 0);
		else
			goto usage;
	}
	if (!binds || !port || port > 0xffff || !threads || threads > THREADS_MAX || report_s <= 0)
		goto usage;
	keep = out != NULL;
	if (keep)
		out_path = out;
	/* Validating: 2^20 slots of metadata (no data) and a 2^18-completion
	 * horizon, 1.7 s of stream. Keeping: the run must fit in capacity.
	 */
	capacity = keep ? (uint64_t)(cap_mib ? cap_mib : 4096) * (1 << 20) / SLICE : 1u << 20;
	if (!horizon)
		horizon = keep ? capacity : 1u << 18;
	if (horizon > capacity)
		goto usage;
	for (i = 0; i < binds; i++) {
		sa[i] = (struct sockaddr_in){ .sin_family = AF_INET, .sin_port = htons((uint16_t)port) };
		if (inet_pton(AF_INET, bind_ip[i], &sa[i].sin_addr) != 1)
			goto usage;
		snprintf(where + strlen(where), sizeof(where) - strlen(where), "%s%s:%u", i ? "," : "",
			 bind_ip[i], port);
	}
	n = threads * binds;
	if (keep) {
		data = mmap(NULL, capacity * SLICE, PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0);
		if (data == MAP_FAILED) {
			perror("memory");
			return 1;
		}
	}
	got = calloc(capacity, sizeof(*got));
	tag = calloc(capacity, sizeof(*tag));
	hash = calloc(capacity, sizeof(*hash));
	lengths = calloc(capacity, sizeof(*lengths));
	line_fp = calloc(LAP, sizeof(*line_fp));
	if (!got || !tag || !hash || !lengths || !line_fp) {
		perror("memory");
		return 1;
	}
	for (i = 0; i < LAP; i++)
		atomic_flag_clear(&lap_lock[i]);
	for (i = 0; i < n; i++) {
		w[i].fd = socket(AF_INET, SOCK_DGRAM, 0);
		if (w[i].fd < 0 || setsockopt(w[i].fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) ||
		    setsockopt(w[i].fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) ||
		    setsockopt(w[i].fd, SOL_SOCKET, SO_RXQ_OVFL, &one, sizeof(one)) ||
		    setsockopt(w[i].fd, SOL_SOCKET, SO_RCVTIMEO, &tick, sizeof(tick)) ||
		    bind(w[i].fd, (struct sockaddr *)&sa[i / threads], sizeof(sa[0])) ||
		    !(w[i].bufs = malloc(sizeof(*w[i].bufs) * BATCH))) {
			perror("socket");
			return 1;
		}
	}
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	udp_errors(&rb0, &cs0, &ie0);
	fprintf(stderr, "sdr-recv: listening on %s, %u threads each, %" PRIu64 " slots, horizon %" PRIu64
		", %s\n", where, threads, capacity, horizon, keep ? "keeping data" : "validating");
	started_ns = last_report = now_ns();
	for (i = 0; i < n; i++)
		pthread_create(&w[i].tid, NULL, work, &w[i]);
	/* The finalizer: this thread. */
	for (;;) {
		usleep(20000);
		t = now_ns();
		if (stop_requested)
			break;
		if (!atomic_load(&first_any_ns)) {
			if (t - started_ns > (uint64_t)(wait_s * 1e9))
				break;
			continue;
		}
		/* Signed: a worker may stamp a datagram after this thread read the clock,
		 * which unsigned would read as an idle stream.
		 */
		if ((int64_t)(t - atomic_load(&last_any_ns)) > (int64_t)idle_ms * 1000000 ||
		    (int64_t)(t - atomic_load(&first_any_ns)) > (int64_t)(seconds * 1e9))
			break;
		/* The run's first completions may arrive a little out of order;
		 * begin finalizing half a second in, at the lowest seen.
		 */
		if (!started) {
			if ((int64_t)(t - atomic_load(&first_any_ns)) < 500000000)
				continue;
			fin_next = atomic_load(&seq_min);
			started = 1;
		}
		if (!keep) {
			hi = atomic_load(&seq_max);
			while (fin_next + horizon <= hi)
				finalize(fin_next++);
		}
		if (t - last_report >= (uint64_t)(report_s * 1e9)) {
			report("progress", where, w, n, rb0, cs0, ie0);
			last_report = t;
		}
	}
	atomic_store(&receiving, 0);
	for (i = 0; i < n; i++)
		pthread_join(w[i].tid, NULL);
	if (atomic_load(&first_any_ns)) {
		if (!started)
			fin_next = atomic_load(&seq_min);
		hi = atomic_load(&seq_max);
		if (keep) {
			snprintf(path, sizeof(path), "%s.lengths", out);
			f = fopen(out, "wb");
			fl = fopen(path, "wb");
			if (!f || !fl) {
				perror(!f ? out : path);
				failed = 1;
			}
		}
		for (s = fin_next; s <= hi; s++) {
			uint64_t slot = s % capacity;
			uint32_t l = atomic_load(&tag[slot]) == s + 1 ? lengths[slot] : 0;

			finalize(s);
			if (f && fwrite(data + slot * SLICE, 1, l, f) != l)
				failed = 1;
			if (fl && fwrite(&l, sizeof(l), 1, fl) != 1)
				failed = 1;
		}
		if ((f && fclose(f)) || (fl && fclose(fl)))
			failed = 1;
		if (failed)
			fprintf(stderr, "sdr-recv: writing %s failed\n", out);
	}
	report("sdr-recv", where, w, n, rb0, cs0, ie0);
	return failed;
usage:
	fprintf(stderr, "usage: sdr-recv --bind IP [--bind IP] --port P [--threads N] [--seconds S] [--idle-ms I] "
		"[--wait S] [--capacity-mib M] [--out PATH] [--report S] [--horizon N]\n");
	return 2;
}
