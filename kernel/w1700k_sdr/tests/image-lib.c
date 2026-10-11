// SPDX-License-Identifier: GPL-2.0-only
/* fwdl-image.c as a shared library for the Python container tests. */
#include <sys/mman.h>
#include <unistd.h>
#include "harness.h"
#include "fwdl_wire.c"
#include "fwdl-image.c"

/* Parse `data` placed against an inaccessible page on either side, and every
 * truncated prefix against the following page: no read leaves the buffer.
 * Returns 0 when all parses behave.
 */
int guarded_parse(const void *data, size_t bytes)
{
	size_t page = (size_t)sysconf(_SC_PAGESIZE), n;
	struct sdr_fwdl_image out;
	unsigned int side;
	u8 *map, *p;

	if (bytes >= page)
		return -1;
	map = mmap(NULL, 3 * page, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (map == MAP_FAILED)
		return -1;
	for (side = 0; side < 2; side++) {
		CHECK(!mprotect(map + page, page, PROT_READ | PROT_WRITE));
		p = side ? map + 2 * page - bytes : map + page;
		memcpy(p, data, bytes);
		CHECK(!mprotect(map + page, page, PROT_READ));
		CHECK(!sdr_fwdl_parse_image(p, bytes, &out));
	}
	for (n = 1; n < bytes; n++) {
		CHECK(!mprotect(map + page, page, PROT_READ | PROT_WRITE));
		p = map + 2 * page - n;
		memcpy(p, data, n);
		CHECK(!mprotect(map + page, page, PROT_READ));
		CHECK(sdr_fwdl_parse_image(p, n, &out) < 0);
	}
	return munmap(map, 3 * page);
}
