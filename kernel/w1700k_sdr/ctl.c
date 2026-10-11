// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt7996-ctl DEVICE [--timeout MS] [ADDR=VALUE ...] [--read ADDR COUNT]
 * mt7996-ctl DEVICE --serve
 *
 * Word writes, then word reads, in 0x0090b000-0x0090fffc through CTL: done
 * by the stream pump between sweeps while a stream runs, by the ioctl
 * otherwise. The stream producer's RF control mailbox lives there.
 *
 * --serve reads one request per stdin line, in the argument syntax
 * ("0x0090f404=0x1 0x0090f400=0x7 --read 0x0090f400 8"), and answers each
 * with one JSON line:
 *   {"at":"ctl","ok":true,"errno":"","result":0,"via_pump":1,"writes":2,
 *    "read_address":"0x0090f400","read":["0x...",...]}
 * EOF ends the session.
 */
#include "client.h"

#define WORDS_MAX 200

/* Fill `c` from words; 0, or -1 with the expected syntax in *why. */
static int build(struct w1700k_sdr_ctl *c, int n, char **w, const char **why)
{
	int i;

	memset(c, 0, sizeof(*c));
	c->timeout_ms = 2000;
	for (i = 0; i < n; i++) {
		char *eq = strchr(w[i], '=');

		if (!strcmp(w[i], "--timeout")) {
			if (i + 1 >= n || client_u32(w[++i], &c->timeout_ms))
				return *why = "--timeout MS", -1;
		} else if (!strcmp(w[i], "--read")) {
			if (i + 2 >= n || client_u32(w[i + 1], &c->read_address) ||
			    client_u32(w[i + 2], &c->read_count))
				return *why = "--read ADDR COUNT", -1;
			i += 2;
		} else if (eq) {
			*eq = 0;
			if (c->count >= W1700K_SDR_CTL_MAX ||
			    client_u32(w[i], &c->address[c->count]) ||
			    client_u32(eq + 1, &c->value[c->count]))
				return *why = "ADDR=VALUE (at most 64)", -1;
			c->count++;
		} else {
			return *why = "unknown word", -1;
		}
	}
	return 0;
}

static void answer(const struct w1700k_sdr_ctl *c, int rc, int err)
{
	uint32_t i;

	printf("{\"at\":\"ctl\",\"ok\":%s,\"errno\":\"%s\",\"result\":%" PRId32
	       ",\"via_pump\":%" PRIu32 ",\"writes\":%" PRIu32 ",\"read_address\":\"0x%08" PRIx32
	       "\",\"read\":[", rc ? "false" : "true", rc ? strerror(err) : "", c->result,
	       c->via_pump, c->count, c->read_address);
	for (i = 0; !rc && i < c->read_count; i++)
		printf("%s\"0x%08" PRIx32 "\"", i ? "," : "", c->read[i]);
	printf("]}\n");
	fflush(stdout);
}

static int one(int fd, int n, char **w)
{
	struct w1700k_sdr_ctl c;
	const char *why = "";
	int rc;

	if (build(&c, n, w, &why)) {
		printf("{\"at\":\"ctl\",\"ok\":false,\"errno\":\"usage: %s\"}\n", why);
		fflush(stdout);
		return 1;
	}
	rc = ioctl(fd, W1700K_SDR_CTL, &c);
	answer(&c, rc, errno);
	return rc ? 1 : 0;
}

static int serve(int fd)
{
	char line[4096], *w[WORDS_MAX], *tok, *save;
	int n, ret = 0;

	while (fgets(line, sizeof(line), stdin)) {
		n = 0;
		for (tok = strtok_r(line, " \t\r\n", &save); tok && n < WORDS_MAX;
		     tok = strtok_r(NULL, " \t\r\n", &save))
			w[n++] = tok;
		if (n)
			ret |= one(fd, n, w);
	}
	return ret;
}

int main(int argc, char **argv)
{
	int fd, ret;

	if (argc < 3) {
		fprintf(stderr, "usage: %s DEVICE [--timeout MS] [ADDR=VALUE ...] [--read ADDR COUNT]\n"
			"       %s DEVICE --serve\n", argv[0], argv[0]);
		return 2;
	}
	fd = client_open(argv[1], O_RDWR, W1700K_SDR_CAP_CTL);
	if (fd < 0)
		return 1;
	ret = strcmp(argv[2], "--serve") ? one(fd, argc - 2, argv + 2) : serve(fd);
	close(fd);
	return ret;
}
