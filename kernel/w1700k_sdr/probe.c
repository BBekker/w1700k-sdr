// SPDX-License-Identifier: GPL-2.0-only
/*
 * mt7996-probe DEVICE ADDRESS LENGTH [--acquire] [--raw FILE]
 *
 * One READ_MEM of a word-aligned window (at most 4096 bytes, inside a
 * permitted window), optionally after taking host ownership. Prints an
 * `attempt` line, flushed before the read so a hang is attributable, then a
 * `read` line. --raw writes the bytes to FILE.
 */
#include "client.h"

#define PROBE_MAX 4096U

static uint32_t word_at(const unsigned char *a, uint32_t off)
{
	return (uint32_t)a[off] | (uint32_t)a[off + 1] << 8 |
	       (uint32_t)a[off + 2] << 16 | (uint32_t)a[off + 3] << 24;
}

static void report(const unsigned char *a, uint32_t n)
{
	uint32_t i, nonzero = 0, allones = 0;

	for (i = 0; i < n; i++)
		nonzero += a[i] != 0;
	for (i = 0; i < n; i += 4)
		allones += word_at(a, i) == UINT32_MAX;
	printf("{\"at\":\"read\",\"pass\":1,\"ok\":true,\"word0\":\"0x%08" PRIx32
	       "\",\"word1\":\"0x%08" PRIx32 "\",\"last_word\":\"0x%08" PRIx32
	       "\",\"nonzero_bytes\":%" PRIu32 ",\"allones_words\":%" PRIu32 "}\n",
	       word_at(a, 0), n >= 8 ? word_at(a, 4) : 0, word_at(a, n - 4),
	       nonzero, allones);
}

static int write_raw(const char *path, const unsigned char *a, uint32_t n)
{
	FILE *f = fopen(path, "wb");

	if (!f)
		return client_fail(path);
	if (fwrite(a, 1, n, f) != n || fflush(f) || fsync(fileno(f))) {
		client_fail(path);
		fclose(f);
		return 1;
	}
	return fclose(f) ? client_fail(path) : 0;
}

static int usage(const char *me)
{
	fprintf(stderr, "usage: %s DEVICE ADDRESS LENGTH [--acquire] [--raw FILE]\n", me);
	return 2;
}

int main(int argc, char **argv)
{
	unsigned char data[PROBE_MAX];
	struct w1700k_sdr_mem_read req;
	const char *raw_path = NULL;
	uint32_t address, length;
	int acquire = 0, fd, i, ret = 1;

	if (argc < 4)
		return usage(argv[0]);
	for (i = 4; i < argc; i++) {
		if (!strcmp(argv[i], "--acquire"))
			acquire = 1;
		else if (!strcmp(argv[i], "--raw") && i + 1 < argc)
			raw_path = argv[++i];
		else
			return usage(argv[0]);
	}
	if (client_u32(argv[2], &address) || (address & 3) ||
	    client_u32(argv[3], &length) || !length || (length & 3) ||
	    length > PROBE_MAX || address > UINT32_MAX - length + 1) {
		fprintf(stderr, "ADDRESS must be word aligned and LENGTH a multiple of 4 "
			"in 4..%u\n", PROBE_MAX);
		return 2;
	}
	fd = client_open(argv[1], O_RDONLY, W1700K_SDR_CAP_MEM |
			 (acquire ? W1700K_SDR_CAP_HOST_OWN : 0));
	if (fd < 0)
		return 1;
	/* Not fatal: ownership may already be held. The read checks it. */
	if (acquire && ioctl(fd, W1700K_SDR_ACQUIRE_HOST, 0) < 0)
		printf("{\"at\":\"acquire\",\"errno\":\"%s\"}\n", strerror(errno));
	printf("{\"at\":\"attempt\",\"address\":\"0x%08" PRIx32 "\",\"length\":%" PRIu32 "}\n",
	       address, length);
	if (fflush(stdout))
		goto done;
	memset(&req, 0, sizeof(req));
	req.address = address;
	req.length = length;
	req.data = (uint64_t)(uintptr_t)data;
	if (ioctl(fd, W1700K_SDR_READ_MEM, &req) < 0) {
		printf("{\"at\":\"read\",\"pass\":1,\"ok\":false,\"errno\":\"%s\"}\n",
		       strerror(errno));
		goto done;
	}
	report(data, length);
	ret = raw_path ? write_raw(raw_path, data, length) : 0;
done:
	if (fflush(stdout))
		ret = 1;
	close(fd);
	return ret;
}
