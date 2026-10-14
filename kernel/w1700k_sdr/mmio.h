/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * MT7996 device-address access through PCIe BAR0, using the mt76 translation:
 * addresses below 0x100000 are BAR offsets; otherwise the fixed map
 * (hw/mt7996/host.h), else the 64 KiB L1 window (selector BAR 0x155024, aperture
 * 0x130000) or the 4 KiB L2 window (selector BAR 0x1b4, aperture 0x1000).
 * The selector is programmed (read, merge, write, read back) on every access.
 */
#ifndef W1700K_SDR_MMIO_H
#define W1700K_SDR_MMIO_H
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
#endif

#define SDR_MMIO_MAX_READ 4096U

struct sdr_mapping {
	u32 bar, span;		/* BAR offset and bytes valid from there */
	u32 remap_reg, remap_mask, remap_value;	/* selector, 0 = none */
};

/*
 * begin/end bracket every access: begin takes the device lock and checks that
 * the device is live and host-owned; a failed begin holds nothing.
 */
struct sdr_mmio_ops {
	int (*begin)(void *ctx);
	void (*end)(void *ctx);
	u32 (*read32)(void *ctx, u32 bar);
	void (*write32)(void *ctx, u32 bar, u32 value);
	/* Optional bulk store into SRAM, with one barrier after the copy. */
	void (*write_block)(void *ctx, u32 bar, const void *data, u32 length);
};

struct sdr_mmio {
	const struct sdr_mmio_ops *ops;
	void *ctx;
	u64 bar_size;
};

void sdr_map(u32 address, struct sdr_mapping *map);

/* True when [address, address + length) lies inside one READ_MEM window. */
bool sdr_mem_read_allowed(u32 address, u32 length);

/*
 * Word-aligned read of up to SDR_MMIO_MAX_READ bytes in one begin/end,
 * reprogramming the window at each window boundary. The whole request is
 * validated before any access. Bytes keep the device's little-endian order.
 */
int sdr_mmio_block_held(const struct sdr_mmio *io, u32 address, void *data, u32 length, bool read);

int sdr_mmio_read(const struct sdr_mmio *io, u32 address, void *dst, u32 length);

/* One masked store, (before & ~mask) | value, read back, for a caller inside
 * begin/end.
 */
int sdr_mmio_rmw_held(const struct sdr_mmio *io, u32 address, u32 mask, u32 value,
		      u32 *before, u32 *after);

/*
 * One plain store / one load for a caller inside begin/end. Stores are not
 * read back, so a run of them reaches the device in order as posted writes;
 * the next load flushes them.
 */
int sdr_mmio_write32_held(const struct sdr_mmio *io, u32 address, u32 value);
int sdr_mmio_read32_held(const struct sdr_mmio *io, u32 address, u32 *out);
#endif
