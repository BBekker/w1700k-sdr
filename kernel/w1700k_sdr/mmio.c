// SPDX-License-Identifier: GPL-2.0-only
#include "mmio.h"
#ifdef __KERNEL__
#include <linux/array_size.h>
#include <linux/errno.h>
#else
#include <errno.h>
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
#endif
#include "hw/mt7996/host.h"
#include "hw/mt7996/mcu_map.h"

struct sdr_fixed_map { u32 phys, mapped, size; };

/* First match wins and the order, including overlaps, is significant. */
#define SDR_FIXED(bus, bar, size, name) { bus, bar, size },
static const struct sdr_fixed_map sdr_fixed[] = { MT7996_HOST_FIXED_MAP(SDR_FIXED) };
#undef SDR_FIXED

/* The READ_MEM windows (see the uapi header): what a diagnostic of our flow
 * reads, and nothing inside the WiFi power island.
 */
#define SDR_WINDOW(base, bytes) { (base), (base) + (bytes) - 1 }
static const struct { u32 first, last; } sdr_read_windows[] = {
	SDR_WINDOW(MT7996_TOP_BASE, 0x1000),		/* host CSR page: LPCR, FW_STATE */
	SDR_WINDOW(MT7996_WM_ROM_BASE, 0x10000),	/* MCU ROM */
	SDR_WINDOW(MT7996_WM_ULM1_BASE, 0x10000),	/* patch SRAM */
};
#undef SDR_WINDOW

bool sdr_mem_read_allowed(u32 address, u32 length)
{
	u64 last = (u64)address + length - 1;
	size_t i;

	if (!length)
		return false;
	for (i = 0; i < ARRAY_SIZE(sdr_read_windows); i++)
		if (address >= sdr_read_windows[i].first &&
		    last <= sdr_read_windows[i].last)
			return true;
	return false;
}

void sdr_map(u32 address, struct sdr_mapping *map)
{
	size_t i, selected = ARRAY_SIZE(sdr_fixed);
	u32 target = address;
	u64 end;

	*map = (struct sdr_mapping){0};
	if (address < MT7996_HOST_DIRECT_LIMIT) {
		map->bar = address;
		map->span = MT7996_HOST_DIRECT_LIMIT - address;
		return;
	}
	for (i = 0; i < selected; i++) {
		const struct sdr_fixed_map *m = &sdr_fixed[i];

		if (address >= m->phys && (u64)address < (u64)m->phys + m->size) {
			selected = i;
			map->bar = m->mapped + address - m->phys;
			end = (u64)m->phys + m->size;
			goto clip;
		}
	}
	/* CONN_INFRA outside the fixed map is reached at its 0x18xxxxxx alias. */
	if (address >= MT7996_BUS_INFRA_MCU_BASE && address <= MT7996_BUS_INFRA_MCU_END)
		target = MT7996_BUS_INFRA_MCU_TO_L1(address);
	if ((target >= MT7996_BUS_INFRA_BASE && target <= MT7996_BUS_INFRA_END) ||
	    (target >= MT7996_BUS_CBTOP1_BASE && target <= MT7996_BUS_CBTOP1_END) ||
	    target >= MT7996_BUS_CBTOP2_BASE) {
		map->bar = MT7996_HOST_L1_BAR(target);
		map->remap_reg = MT7996_HOST_L1_REMAP;
		map->remap_mask = HOST_L1_REMAP_BASE;
		map->remap_value = MT7996_HOST_L1_SEL(target);
		end = ((u64)address | (MT7996_HOST_L1_WIN_SIZE - 1)) + 1;
	} else {
		map->bar = MT7996_HOST_L2_BAR(target);
		map->remap_reg = MT7996_HOST_L2_REMAP;
		map->remap_mask = HOST_L2_REMAP_BASE;
		map->remap_value = MT7996_HOST_L2_SEL(target);
		end = ((u64)address | (MT7996_HOST_L2_WIN_SIZE - 1)) + 1;
	}
clip:
	/* An earlier fixed entry starting inside this span takes over there:
	 * first match wins, as in mt76.
	 */
	for (i = 0; i < selected; i++)
		if (sdr_fixed[i].phys > address && sdr_fixed[i].phys < end)
			end = sdr_fixed[i].phys;
	map->span = end - address;
}

static bool sdr_map_fits(const struct sdr_mmio *io, const struct sdr_mapping *map,
			 u32 count)
{
	return count && !((count | map->bar) & 3) &&
	       (u64)map->bar + count <= io->bar_size &&
	       (!map->remap_reg || (u64)map->remap_reg + 4 <= io->bar_size);
}

/* Program the window selector, if the mapping uses one, and flush it. */
static void sdr_select(const struct sdr_mmio *io, const struct sdr_mapping *map)
{
	u32 value;

	if (!map->remap_reg)
		return;
	value = io->ops->read32(io->ctx, map->remap_reg);
	io->ops->write32(io->ctx, map->remap_reg,
			 (value & ~map->remap_mask) | map->remap_value);
	io->ops->read32(io->ctx, map->remap_reg);
}

int sdr_mmio_read(const struct sdr_mmio *io, u32 address, void *dst, u32 length)
{
	struct sdr_mapping map;
	u32 done, count, offset, value;
	u8 *out = dst;
	int ret;

	if ((address | length) & 3 || length > SDR_MMIO_MAX_READ ||
	    (u64)address + length > (1ULL << 32) || (length && !dst))
		return -EINVAL;
	if (!length)
		return 0;
	for (done = 0; done < length; done += count) {
		sdr_map(address + done, &map);
		count = map.span < length - done ? map.span : length - done;
		if (!sdr_map_fits(io, &map, count))
			return -ERANGE;
	}
	ret = io->ops->begin(io->ctx);
	if (ret)
		return ret;
	for (done = 0; done < length; done += count) {
		sdr_map(address + done, &map);
		count = map.span < length - done ? map.span : length - done;
		sdr_select(io, &map);
		for (offset = 0; offset < count; offset += 4) {
			value = io->ops->read32(io->ctx, map.bar + offset);
			out[done + offset] = value;
			out[done + offset + 1] = value >> 8;
			out[done + offset + 2] = value >> 16;
			out[done + offset + 3] = value >> 24;
		}
	}
	io->ops->end(io->ctx);
	return 0;
}

static int sdr_map_word(const struct sdr_mmio *io, u32 address,
			struct sdr_mapping *map)
{
	if (address & 3)
		return -EINVAL;
	sdr_map(address, map);
	return sdr_map_fits(io, map, 4) && map->span >= 4 ? 0 : -ERANGE;
}

int sdr_mmio_rmw_held(const struct sdr_mmio *io, u32 address, u32 mask, u32 value,
		      u32 *before, u32 *after)
{
	struct sdr_mapping map;
	u32 old;
	int ret;

	if (!before || !after || !mask || (value & ~mask))
		return -EINVAL;
	ret = sdr_map_word(io, address, &map);
	if (ret)
		return ret;
	sdr_select(io, &map);
	old = io->ops->read32(io->ctx, map.bar);
	io->ops->write32(io->ctx, map.bar, (old & ~mask) | value);
	*after = io->ops->read32(io->ctx, map.bar);
	*before = old;
	return 0;
}

int sdr_mmio_write32_held(const struct sdr_mmio *io, u32 address, u32 value)
{
	struct sdr_mapping map;
	int ret = sdr_map_word(io, address, &map);

	if (ret)
		return ret;
	sdr_select(io, &map);
	io->ops->write32(io->ctx, map.bar, value);
	return 0;
}

int sdr_mmio_read32_held(const struct sdr_mmio *io, u32 address, u32 *out)
{
	struct sdr_mapping map;
	int ret;

	if (!out)
		return -EINVAL;
	ret = sdr_map_word(io, address, &map);
	if (ret)
		return ret;
	sdr_select(io, &map);
	*out = io->ops->read32(io->ctx, map.bar);
	return 0;
}
