/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host shims for compiling driver sources into unit tests: kernel integer
 * types, a CHECK macro, and a recording mock of BAR0 (readl/writel on a byte
 * array). The tests include the driver .c files directly.
 */
#ifndef W1700K_SDR_TEST_HARNESS_H
#define W1700K_SDR_TEST_HARNESS_H
#include <endian.h>
#include <errno.h>
#include <linux/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef uint64_t dma_addr_t;

#define BIT(n) (UINT32_C(1) << (n))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define U32_MAX UINT32_MAX
#define __iomem
#define READ_ONCE(x) (x)
#define WRITE_ONCE(dst, v) ((dst) = (v))
#define cpu_to_le32(v) htole32((u32)(v))
#define le32_to_cpu(v) le32toh(v)
#define lower_32_bits(x) ((u32)(x))
#define upper_32_bits(x) ((u32)((u64)(x) >> 32))
#define min_t(t, a, b) ((t)(a) < (t)(b) ? (t)(a) : (t)(b))
#define max_t(t, a, b) ((t)(a) > (t)(b) ? (t)(a) : (t)(b))
#define static_assert _Static_assert

static unsigned long checks;
#define CHECK(expr) do { checks++; if (!(expr)) { \
	fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #expr); \
	exit(1); } } while (0)

/* Bus event log: every MMIO access and barrier, in order. */
struct event { const char *kind; u32 address, value; };
static struct event events[256];
static unsigned int event_count;

static inline void log_event(const char *kind, u32 address, u32 value)
{
	CHECK(event_count < ARRAY_SIZE(events));
	events[event_count++] = (struct event){ kind, address, value };
}

static inline void reset_events(void)
{
	event_count = 0;
}

static u8 mock_bar[0x200000];

static inline u32 bar_offset(const void *ptr, size_t len)
{
	const u8 *p = ptr;

	CHECK(p >= mock_bar && p <= mock_bar + sizeof(mock_bar));
	CHECK(len <= sizeof(mock_bar) - (size_t)(p - mock_bar));
	return (u32)(p - mock_bar);
}

static inline u32 get_unaligned_le32(const void *p)
{
	u32 v;

	memcpy(&v, p, sizeof(v));
	return le32toh(v);
}

static inline void put_unaligned_le32(u32 v, void *p)
{
	v = htole32(v);
	memcpy(p, &v, sizeof(v));
}

static inline u32 mock_readl(const void *p)
{
	u32 offset = bar_offset(p, 4), value = get_unaligned_le32(p);

	log_event("read32", offset, value);
	return value;
}

static inline void mock_writel(u32 value, void *p)
{
	log_event("write32", bar_offset(p, 4), value);
	put_unaligned_le32(value, p);
}

#define readl mock_readl
#define writel mock_writel
#define wmb() log_event("wmb", 0, 0)
#define dma_rmb() log_event("dma_rmb", 0, 0)

/* IEEE CRC32 as Linux crc32_le(seed, ...) computes it (no final inversion). */
static inline u32 crc32_le(u32 crc, const u8 *data, size_t bytes)
{
	unsigned int k;

	while (bytes--) {
		crc ^= *data++;
		for (k = 0; k < 8; k++)
			crc = (crc >> 1) ^ (0xedb88320U & -(crc & 1));
	}
	return crc;
}
#endif
