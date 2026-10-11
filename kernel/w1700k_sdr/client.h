/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Helpers shared by the unit-side clients (each built as one static binary).
 * Include it first: it selects the POSIX and GNU interfaces they use.
 */
#ifndef W1700K_SDR_CLIENT_H
#define W1700K_SDR_CLIENT_H
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "w1700k_sdr.h"

static inline int client_fail(const char *what)
{
	fprintf(stderr, "%s: %s\n", what, strerror(errno));
	return 1;
}

/* A 32-bit number in any C base; 0 on success. */
static inline int client_u32(const char *text, uint32_t *out)
{
	unsigned long long value;
	char *end;

	errno = 0;
	value = strtoull(text, &end, 0);
	if (errno || end == text || *end || value > UINT32_MAX)
		return -1;
	*out = (uint32_t)value;
	return 0;
}

/* A JSON string literal (quotes included) for `text`. */
static inline void client_json_string(FILE *out, const char *text)
{
	fputc('"', out);
	for (; *text; text++) {
		unsigned char c = (unsigned char)*text;

		if (c == '"' || c == '\\')
			fprintf(out, "\\%c", c);
		else if (c < 0x20)
			fprintf(out, "\\u%04x", c);
		else
			fputc(c, out);
	}
	fputc('"', out);
}

/*
 * Open the device and require this client's ABI version and capabilities.
 * Returns the descriptor, or -1 after printing why.
 */
static inline int client_open(const char *path, int flags, uint32_t caps)
{
	struct w1700k_sdr_info info;
	int fd = open(path, flags | O_CLOEXEC);

	if (fd < 0) {
		client_fail(path);
		return -1;
	}
	memset(&info, 0, sizeof(info));
	if (ioctl(fd, W1700K_SDR_GET_INFO, &info) < 0) {
		if (errno == ENOTTY)
			fprintf(stderr, "%s: driver ABI differs from client ABI %u\n",
				path, W1700K_SDR_ABI_VERSION);
		else
			client_fail("GET_INFO");
		close(fd);
		return -1;
	}
	if (info.abi_version != W1700K_SDR_ABI_VERSION) {
		fprintf(stderr, "%s: driver ABI %" PRIu32 ", client ABI %u; "
			"install the module and clients from one build\n",
			path, info.abi_version, W1700K_SDR_ABI_VERSION);
		close(fd);
		return -1;
	}
	if ((info.capabilities & caps) != caps) {
		fprintf(stderr, "%s: driver lacks capabilities 0x%08" PRIx32 "\n",
			path, caps & ~info.capabilities);
		close(fd);
		return -1;
	}
	return fd;
}
#endif
