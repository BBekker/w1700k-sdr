// SPDX-License-Identifier: GPL-2.0-only
/*
 * Host WFDMA0 control for the MCU rings: the register effects of mt76
 * mt7996_dma_disable/mt7996_dma_enable (mt7996/dma.c, regs.h) for the primary
 * PCIe interface, with every control write read back.
 */
#ifdef __KERNEL__
#include <linux/array_size.h>
#include <linux/errno.h>
#else
#include <errno.h>
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
#endif
#include "wfdma.h"
#include "hw/mt7996/wfdma.h"

#define WFDMA_RST MT7996_WFDMA0_RST
#define WFDMA_BUSY_ENA MT7996_WFDMA0_BUSY_ENA
#define WFDMA_IRQ_MASK MT7996_WFDMA0_INT_MASK_CSR
#define WFDMA_GLO_CFG MT7996_WFDMA0_GLO_CFG
#define WFDMA_RST_DTX MT7996_WFDMA0_RST_DTX_PTR
#define WFDMA_EXT0 MT7996_WFDMA0_GLO_CFG_EXT0
#define WFDMA_EXT1 MT7996_WFDMA0_GLO_CFG_EXT1
#define WFDMA_HIF_MISC MT7996_WFDMA_HIF_MISC
#define WFDMA_RESET_BITS (WFDMA0_RST_LOGIC_RST | WFDMA0_RST_DMASHDL_ALL_RST)
#define WFDMA_DMA_ENABLE (WFDMA0_GLO_CFG_TX_DMA_EN | WFDMA0_GLO_CFG_RX_DMA_EN)
#define WFDMA_DISABLE_BITS WFDMA0_GLO_CFG_STOP_BITS
#define WFDMA_START_BITS WFDMA0_GLO_CFG_START_BITS
#define WFDMA_FIFO_BUSY (WFDMA0_BUSY_ENA_TX_FIFO0 | WFDMA0_BUSY_ENA_TX_FIFO1 | \
			 WFDMA0_BUSY_ENA_RX_FIFO)
#define WFDMA_PREFETCH(base, depth) (HW_PREP(WFDMA_RING_EXT_CTRL_BASE, base) | \
				     HW_PREP(WFDMA_RING_EXT_CTRL_DEPTH, depth))
#define WFDMA_PREFETCH_FIELDS (WFDMA_RING_EXT_CTRL_BASE | WFDMA_RING_EXT_CTRL_DEPTH)
#define WFDMA_PAUSE(hi, lo) (HW_PREP(WFDMA0_PAUSE_TH_HI, hi) | HW_PREP(WFDMA0_PAUSE_TH_LO, lo))
#define WFDMA_PAUSE_FIELDS (WFDMA0_PAUSE_TH_HI | WFDMA0_PAUSE_TH_LO)
#define WFDMA_EXT0_RX_BITS (WFDMA0_GLO_CFG_EXT0_RX_WB_RXD | WFDMA0_GLO_CFG_EXT0_WED_MERGE_MODE)

/* Full-word stores; only the documented fields are checked on read-back. */
struct sdr_wfdma_setting {
	u32 reg, value, mask;
};

static const struct sdr_wfdma_setting sdr_wfdma_prefetch[] = {
	{ MT7996_WFDMA0_PRI_DLY_INT_CFG(0), 0, 0xffffffffU },
	{ MT7996_WFDMA0_PRI_DLY_INT_CFG(1), 0, 0xffffffffU },
	{ MT7996_WFDMA0_PRI_DLY_INT_CFG(2), 0, 0xffffffffU },
	/* mt76's sequential prefetch allocator, gaps for unused queues kept. */
	{ MT7996_WFDMA0_TX_RING_EXT_CTRL(MT7996_TXQ_FWDL), WFDMA_PREFETCH(0x000, 2),
	  WFDMA_PREFETCH_FIELDS },
	{ MT7996_WFDMA0_TX_RING_EXT_CTRL(MT7996_TXQ_MCU_WM), WFDMA_PREFETCH(0x020, 2),
	  WFDMA_PREFETCH_FIELDS },
	/* TX 20, mt76's WA command ring: not bound here, but its window is
	 * programmed as mt76 does, so no reset default can overlap ours.
	 */
	{ MT7996_WFDMA0_TX_RING_EXT_CTRL(MT7996_TXQ_MCU_WA), WFDMA_PREFETCH(0x040, 2),
	  WFDMA_PREFETCH_FIELDS },
	{ MT7996_WFDMA0_RX_RING_EXT_CTRL(MT7996_RXQ_MCU_WM), WFDMA_PREFETCH(0x160, 2),
	  WFDMA_PREFETCH_FIELDS },
	/* RX 1, mt76's WA event ring: likewise not bound, window kept. */
	{ MT7996_WFDMA0_RX_RING_EXT_CTRL(MT7996_RXQ_MCU_WA), WFDMA_PREFETCH(0x180, 2),
	  WFDMA_PREFETCH_FIELDS },
};

static const struct sdr_wfdma_setting sdr_wfdma_thresholds[] = {
	{ MT7996_WFDMA0_PAUSE_RX_Q_45_TH, WFDMA_PAUSE(0xc, 0xc), WFDMA_PAUSE_FIELDS },
	{ MT7996_WFDMA0_PAUSE_RX_Q_67_TH, WFDMA_PAUSE(0x1, 0x8), WFDMA_PAUSE_FIELDS },
	{ MT7996_WFDMA0_PAUSE_RX_Q_89_TH, WFDMA_PAUSE(0x1, 0x8), WFDMA_PAUSE_FIELDS },
	{ MT7996_WFDMA0_PAUSE_RX_Q_RRO_TH, WFDMA_PAUSE(0, 0x20), WFDMA0_PAUSE_TH_LO },
};

static int sdr_wfdma_valid(const struct sdr_mmio *io)
{
	return io && io->ops && io->ops->read32 && io->ops->write32 &&
		io->bar_size >= WFDMA_HIF_MISC + 4U ? 0 : -EINVAL;
}

static int sdr_wfdma_read(const struct sdr_mmio *io, u32 reg, u32 *value)
{
	*value = io->ops->read32(io->ctx, reg);
	return *value == (u32)~0U ? -EIO : 0;
}

static int sdr_wfdma_check(const struct sdr_mmio *io, u32 reg, u32 mask,
			   u32 expected)
{
	u32 value;
	int ret = sdr_wfdma_read(io, reg, &value);

	return ret ? ret : (value & mask) == expected ? 0 : -EIO;
}

static int sdr_wfdma_program(const struct sdr_mmio *io,
			    const struct sdr_wfdma_setting *settings,
			    unsigned int count)
{
	unsigned int i;
	int ret;

	for (i = 0; i < count; i++) {
		const struct sdr_wfdma_setting *s = &settings[i];

		io->ops->write32(io->ctx, s->reg, s->value);
		ret = sdr_wfdma_check(io, s->reg, s->mask, s->value);
		if (ret)
			return ret;
	}
	return 0;
}

/* Read-modify-write; the read-back flushes the posted write and checks only
 * the fields this operation changed.
 */
static int sdr_wfdma_update(const struct sdr_mmio *io, u32 reg, u32 mask,
			    u32 value)
{
	u32 old;
	int ret = sdr_wfdma_read(io, reg, &old);

	if (ret)
		return ret;
	io->ops->write32(io->ctx, reg, (old & ~mask) | value);
	return sdr_wfdma_check(io, reg, mask, value);
}

static int sdr_wfdma_mask_irq(const struct sdr_mmio *io)
{
	io->ops->write32(io->ctx, WFDMA_IRQ_MASK, 0);
	return sdr_wfdma_check(io, WFDMA_IRQ_MASK, (u32)~0U, 0);
}

static int sdr_wfdma_disabled(const struct sdr_mmio *io)
{
	u32 value;
	int ret = sdr_wfdma_read(io, WFDMA_IRQ_MASK, &value);

	if (ret || value)
		return ret ? ret : -EBUSY;
	ret = sdr_wfdma_read(io, WFDMA_GLO_CFG, &value);
	if (ret)
		return ret;
	return value & WFDMA_DISABLE_BITS ? -EBUSY : 0;
}

static int sdr_wfdma_idle(const struct sdr_mmio *io,
			 int (*wait_poll)(void *ctx))
{
	u32 poll, value;
	int ret;

	for (poll = 0; poll < SDR_WFDMA_IDLE_POLLS; poll++) {
		ret = sdr_wfdma_read(io, WFDMA_HIF_MISC, &value);
		if (ret || !(value & WFDMA_HIF_MISC_BUSY))
			return ret;
		if (poll + 1U == SDR_WFDMA_IDLE_POLLS)
			break;
		ret = wait_poll(io->ctx);
		if (ret)
			return ret < 0 ? ret : -EIO;
	}
	return -ETIMEDOUT;
}

static int sdr_wfdma_reset_regs(const struct sdr_mmio *io)
{
	int ret = sdr_wfdma_valid(io);

	if (ret)
		return ret;
	ret = sdr_wfdma_mask_irq(io);
	if (!ret)
		ret = sdr_wfdma_update(io, WFDMA_RST, WFDMA_RESET_BITS, 0);
	if (!ret)
		ret = sdr_wfdma_update(io, WFDMA_RST, WFDMA_RESET_BITS,
				       WFDMA_RESET_BITS);
	if (!ret)
		ret = sdr_wfdma_update(io, WFDMA_GLO_CFG, WFDMA_DISABLE_BITS, 0);
	return ret;
}

static int sdr_wfdma_prepare_regs(const struct sdr_mmio *io,
				int (*wait_poll)(void *ctx))
{
	int ret = sdr_wfdma_valid(io);

	if (ret || !wait_poll)
		return -EINVAL;
	ret = sdr_wfdma_disabled(io);
	if (ret)
		return ret;
	io->ops->write32(io->ctx, WFDMA_RST_DTX, (u32)~0U);
	ret = sdr_wfdma_program(io, sdr_wfdma_prefetch, ARRAY_SIZE(sdr_wfdma_prefetch));
	if (!ret)
		ret = sdr_wfdma_update(io, WFDMA_EXT1, WFDMA0_GLO_CFG_EXT1_CALC_MODE,
				       WFDMA0_GLO_CFG_EXT1_CALC_MODE);
	if (!ret)
		ret = sdr_wfdma_update(io, WFDMA_BUSY_ENA, WFDMA_FIFO_BUSY,
				       WFDMA_FIFO_BUSY);
	if (!ret)
		ret = sdr_wfdma_idle(io, wait_poll);
	if (ret)
		return ret;
	ret = sdr_wfdma_update(io, WFDMA_EXT0, WFDMA_EXT0_RX_BITS, WFDMA_EXT0_RX_BITS);
	if (!ret)
		ret = sdr_wfdma_update(io, WFDMA_EXT1, WFDMA0_GLO_CFG_EXT1_TX_FCTRL_MODE,
				       WFDMA0_GLO_CFG_EXT1_TX_FCTRL_MODE);
	if (ret)
		return ret;
	/* mt76's RX pause thresholds. */
	ret = sdr_wfdma_program(io, sdr_wfdma_thresholds, ARRAY_SIZE(sdr_wfdma_thresholds));
	/* RST_DTX_PTR self-clears and is not read back. */
	return ret ? ret : sdr_wfdma_disabled(io);
}

static int sdr_wfdma_enable_regs(const struct sdr_mmio *io)
{
	u32 value;
	int ret = sdr_wfdma_valid(io);

	if (ret)
		return ret;
	ret = sdr_wfdma_disabled(io);
	if (!ret)
		ret = sdr_wfdma_check(io, WFDMA_BUSY_ENA, WFDMA_FIFO_BUSY,
				      WFDMA_FIFO_BUSY);
	if (!ret)
		ret = sdr_wfdma_read(io, WFDMA_HIF_MISC, &value);
	if (ret)
		return ret;
	if (value & WFDMA_HIF_MISC_BUSY)
		return -EBUSY;
	return sdr_wfdma_update(io, WFDMA_GLO_CFG, WFDMA_START_BITS,
				WFDMA_START_BITS);
}

static int sdr_wfdma_stop_regs(const struct sdr_mmio *io,
			     int (*wait_poll)(void *ctx))
{
	int ret = sdr_wfdma_valid(io);

	if (ret || !wait_poll)
		return -EINVAL;
	ret = sdr_wfdma_mask_irq(io);
	if (!ret)
		ret = sdr_wfdma_update(io, WFDMA_GLO_CFG, WFDMA_DISABLE_BITS, 0);
	if (!ret)
		ret = sdr_wfdma_update(io, WFDMA_BUSY_ENA, WFDMA_FIFO_BUSY,
				       WFDMA_FIFO_BUSY);
	return ret ? ret : sdr_wfdma_idle(io, wait_poll);
}

int sdr_wfdma_init(struct sdr_wfdma *dma, const struct sdr_mmio *io,
		   int (*wait_poll)(void *ctx))
{
	if (!dma || sdr_wfdma_valid(io) || !wait_poll)
		return -EINVAL;
	*dma = (struct sdr_wfdma) {
		.io = io, .wait_poll = wait_poll, .state = SDR_WFDMA_NEW,
	};
	return 0;
}

static int sdr_wfdma_session_valid(const struct sdr_wfdma *dma)
{
	return dma && !sdr_wfdma_valid(dma->io) && dma->wait_poll ?
		0 : -EINVAL;
}

static int sdr_wfdma_expect(const struct sdr_wfdma *dma,
			    enum sdr_wfdma_state expected)
{
	int ret = sdr_wfdma_session_valid(dma);

	return ret ? ret : dma->state == expected ? 0 : -EALREADY;
}

static int sdr_wfdma_finish(struct sdr_wfdma *dma, int ret,
			    enum sdr_wfdma_state success)
{
	if (ret && !dma->first_error)
		dma->first_error = ret;
	dma->state = dma->first_error ? SDR_WFDMA_FAILED : success;
	return ret;
}

int sdr_wfdma_reset(struct sdr_wfdma *dma)
{
	int ret = sdr_wfdma_expect(dma, SDR_WFDMA_NEW);

	if (ret)
		return ret;
	return sdr_wfdma_finish(dma, sdr_wfdma_reset_regs(dma->io),
				SDR_WFDMA_RESET);
}

int sdr_wfdma_prepare(struct sdr_wfdma *dma)
{
	int ret = sdr_wfdma_expect(dma, SDR_WFDMA_RESET);

	if (ret)
		return ret;
	return sdr_wfdma_finish(dma,
			sdr_wfdma_prepare_regs(dma->io, dma->wait_poll),
			SDR_WFDMA_PREPARED);
}

int sdr_wfdma_enable(struct sdr_wfdma *dma)
{
	int ret = sdr_wfdma_expect(dma, SDR_WFDMA_PREPARED);

	if (ret)
		return ret;
	return sdr_wfdma_finish(dma, sdr_wfdma_enable_regs(dma->io),
				SDR_WFDMA_ENABLED);
}

int sdr_wfdma_stop_observe(struct sdr_wfdma *dma)
{
	int ret = sdr_wfdma_session_valid(dma);

	if (ret)
		return ret;
	if (dma->state <= SDR_WFDMA_NEW || dma->state > SDR_WFDMA_FAILED ||
	    dma->state == SDR_WFDMA_STOPPED || dma->stop_attempted)
		return -EALREADY;
	dma->stop_attempted = 1;
	return sdr_wfdma_finish(dma,
			sdr_wfdma_stop_regs(dma->io, dma->wait_poll),
			SDR_WFDMA_STOPPED);
}
