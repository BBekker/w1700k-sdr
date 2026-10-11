// SPDX-License-Identifier: GPL-2.0-only
/*
 * WFDMA0 MCU rings. Descriptor layout and ring registers as mt76 dma.[ch] and
 * mt7996/{dma.c,regs.h,mt7996.h}: BASE +0, CNT +4, CPU_IDX +8, DMA_IDX +12.
 * No data queues, TXWI, WED or RRO.
 */
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include "dma-ring.h"
#include "hw/mt7996/host.h"
#include "hw/mt7996/wfdma.h"

#define SDR_DMA_DONE WFDMA_DESC_CTRL_DMA_DONE
#define SDR_DMA_LAST WFDMA_DESC_CTRL_LAST_SEC0
/*
 * The WM event ring is also the stream ring: the MCU's TX ring 0 feeds it.
 * Behind its descriptors is one 1 GiB circular buffer cut into 8 KiB slices
 * (the largest power of two the 14-bit length field carries, and the
 * producer's chunk); 1 GiB is 0.84 s at 1.28 GB/s. The descriptor count only
 * bounds how far the device runs ahead of re-arming. It must stay 1536: the
 * engine accepts a 4096-entry ring size but then never delivers.
 */
#ifndef SDR_DMA_WM_RX_BYTES
#define SDR_DMA_WM_RX_BYTES 8192
#endif
#ifndef SDR_DMA_WM_RX_COUNT
#define SDR_DMA_WM_RX_COUNT 1536
#endif
#ifndef SDR_DMA_STREAM_BYTES
#define SDR_DMA_STREAM_BYTES (1ULL << 30)
#endif
/* The export keeps 64 slices of margin behind the device's write position. */
static_assert(SDR_DMA_STREAM_BYTES / SDR_DMA_WM_RX_BYTES > SDR_DMA_WM_RX_COUNT + 64);

/* Ring register blocks (WFDMA0 TX 16/17, RX 0) and sizes, as mt76. */
static const struct {
	u32 offset, count;
} sdr_dma_queues[] = {
	[SDR_DMA_FWDL_TX] = { MT7996_WFDMA0_TX_RING_BASE(MT7996_TXQ_FWDL), 128 },
	[SDR_DMA_WM_TX] = { MT7996_WFDMA0_TX_RING_BASE(MT7996_TXQ_MCU_WM), 256 },
	[SDR_DMA_WM_RX] = { MT7996_WFDMA0_RX_RING_BASE(MT7996_RXQ_MCU_WM), SDR_DMA_WM_RX_COUNT },
};

/* Variables rather than literals so the host tests can use a small geometry. */
static u32 sdr_dma_wm_rx_count = SDR_DMA_WM_RX_COUNT;
static u32 sdr_dma_wm_rx_bytes = SDR_DMA_WM_RX_BYTES;
static u64 sdr_dma_stream_bytes = SDR_DMA_STREAM_BYTES;

struct sdr_dma_desc {
	__le32 buf0, ctrl, buf1, info;
};
static_assert(sizeof(struct sdr_dma_desc) == MT7996_WFDMA_DESC_BYTES);

struct sdr_dma_buffer {
	void *data;
	dma_addr_t dma;
	size_t bytes;
	u32 id;			/* the slice it views (stream ring only) */
};

/*
 * A TX ring owns a private copy of each packet in flight. The RX ring is the
 * stream ring: the mapping's pages, header first, then slice i at
 * header_pages + (i << order), each slice mapped once for the ring's life;
 * its buffers[] entries are views of slices, not owners. completed counts
 * every completion the ring has had.
 */
struct sdr_dma_ring {
	struct device *dev;
	struct sdr_dma_desc *desc;
	dma_addr_t desc_dma;
	struct sdr_dma_buffer *buffers;
	struct page **pages;
	dma_addr_t *slice_dma;
	struct sdr_dma_stream_header *header;
	u64 completed;
	void __iomem *regs;
	u32 bar_offset, ndesc, head, tail, queued, rx_bytes, order, header_pages;
	u32 slices, slices_ready;
	bool rx, exposed, bound, fault, fragment;
};

/* Validate the entire DMA extent, not just the descriptor's start address. */
static bool sdr_dma_fits(dma_addr_t address, size_t bytes, unsigned int bits)
{
	u64 last = DMA_BIT_MASK(bits);

	return bytes && (u64)address <= last && bytes - 1 <= last - address;
}

static u32 sdr_dma_next(const struct sdr_dma_ring *ring, u32 index)
{
	return index + 1 == ring->ndesc ? 0 : index + 1;
}

static void sdr_dma_buffer_free(struct sdr_dma_ring *ring,
				struct sdr_dma_buffer *buffer)
{
	if (!buffer->data)
		return;
	dma_unmap_single(ring->dev, buffer->dma, buffer->bytes, DMA_TO_DEVICE);
	kfree(buffer->data);
	*buffer = (struct sdr_dma_buffer){};
}

/* A user mapping or a queued export datagram holds its own page references. */
static void sdr_dma_pages_put(struct sdr_dma_ring *ring, u32 first, u32 count)
{
	u32 i;

	for (i = first; i < first + count; i++) {
		put_page(ring->pages[i]);
		ring->pages[i] = NULL;
	}
}

void sdr_dma_ring_free_quiesced(struct sdr_dma_ring *ring)
{
	u32 i;

	if (!ring)
		return;
	for (i = 0; ring->buffers && !ring->rx && i < ring->ndesc; i++)
		sdr_dma_buffer_free(ring, &ring->buffers[i]);
	kfree(ring->buffers);
	for (i = 0; i < ring->slices_ready; i++) {
		dma_unmap_single(ring->dev, ring->slice_dma[i], ring->rx_bytes,
				 DMA_FROM_DEVICE);
		sdr_dma_pages_put(ring, ring->header_pages + (i << ring->order),
				  1U << ring->order);
	}
	if (ring->header)
		sdr_dma_pages_put(ring, 0, ring->header_pages);
	kvfree(ring->slice_dma);
	kvfree(ring->pages);
	if (ring->desc)
		dma_free_coherent(ring->dev, ring->ndesc * sizeof(*ring->desc),
				  ring->desc, ring->desc_dma);
	kfree(ring);
}

/* `count` zeroed, physically contiguous pages at pages[first], split into
 * order-0 pages so each carries its own count and a mapping can hold it.
 */
static void *sdr_dma_pages_alloc(struct sdr_dma_ring *ring, u32 first, u32 count)
{
	unsigned int order = get_order((size_t)count << PAGE_SHIFT);
	struct page *page = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
	u32 i;

	if (!page)
		return NULL;
	split_page(page, order);
	for (i = 0; i < 1U << order; i++) {
		if (i < count)
			ring->pages[first + i] = page + i;
		else
			put_page(page + i);	/* order rounding */
	}
	return page_address(page);
}

/* A private, device-mapped copy of one TX packet. */
static int sdr_dma_buffer_alloc(struct sdr_dma_ring *ring,
				struct sdr_dma_buffer *buffer,
				const void *packet, size_t bytes)
{
	void *data = kmalloc(bytes, GFP_KERNEL);
	dma_addr_t dma;
	int ret;

	if (!data)
		return -ENOMEM;
	memcpy(data, packet, bytes);
	dma = dma_map_single(ring->dev, data, bytes, DMA_TO_DEVICE);
	if (dma_mapping_error(ring->dev, dma)) {
		ret = -EIO;
		goto fail;
	}
	if (!sdr_dma_fits(dma, bytes, MT7996_DMA_STREAMING_BITS)) {
		dma_unmap_single(ring->dev, dma, bytes, DMA_TO_DEVICE);
		ret = -ERANGE;
		goto fail;
	}
	*buffer = (struct sdr_dma_buffer){ data, dma, bytes, 0 };
	return 0;
fail:
	kfree(data);
	return ret;
}

/* The stream ring's header and every slice: zeroed pages, each slice mapped
 * for the device once, for the ring's life.
 */
static int sdr_dma_stream_alloc(struct sdr_dma_ring *ring)
{
	u32 count = ring->ndesc, i, first;
	size_t header = PAGE_ALIGN(sizeof(*ring->header) +
				   (size_t)ring->slices * sizeof(u32));
	dma_addr_t dma;
	void *data;

	ring->header_pages = header >> PAGE_SHIFT;
	ring->pages = kvcalloc(ring->header_pages + ((size_t)ring->slices << ring->order),
			       sizeof(*ring->pages), GFP_KERNEL);
	ring->slice_dma = kvcalloc(ring->slices, sizeof(*ring->slice_dma), GFP_KERNEL);
	if (!ring->pages || !ring->slice_dma)
		return -ENOMEM;
	ring->header = sdr_dma_pages_alloc(ring, 0, ring->header_pages);
	if (!ring->header)
		return -ENOMEM;
	ring->header->slots = count;
	ring->header->slices = ring->slices;
	ring->header->slice_bytes = ring->rx_bytes;
	ring->header->header_bytes = header;
	for (i = 0; i < ring->slices; i++) {
		first = ring->header_pages + (i << ring->order);
		data = sdr_dma_pages_alloc(ring, first, 1U << ring->order);
		if (!data)
			return -ENOMEM;
		dma = dma_map_single(ring->dev, data, ring->rx_bytes, DMA_FROM_DEVICE);
		if (dma_mapping_error(ring->dev, dma)) {
			sdr_dma_pages_put(ring, first, 1U << ring->order);
			return -EIO;
		}
		ring->slice_dma[i] = dma;
		ring->slices_ready++;
		if (!sdr_dma_fits(dma, ring->rx_bytes, MT7996_DMA_STREAMING_BITS))
			return -ERANGE;
	}
	return 0;
}

/* Point the empty slot's view at a slice. */
static void sdr_dma_stream_view(struct sdr_dma_ring *ring, u32 slot, u32 slice)
{
	ring->buffers[slot] = (struct sdr_dma_buffer){
		page_address(ring->pages[ring->header_pages + (slice << ring->order)]),
		ring->slice_dma[slice], ring->rx_bytes, slice };
}

static void sdr_dma_post_rx(struct sdr_dma_ring *ring)
{
	struct sdr_dma_desc *desc = &ring->desc[ring->head];
	struct sdr_dma_buffer *buffer = &ring->buffers[ring->head];

	WRITE_ONCE(desc->buf0, cpu_to_le32(lower_32_bits(buffer->dma)));
	WRITE_ONCE(desc->buf1, cpu_to_le32(upper_32_bits(buffer->dma)));
	WRITE_ONCE(desc->ctrl, cpu_to_le32(buffer->bytes << HW_SHIFT(WFDMA_DESC_CTRL_SD_LEN0)));
	WRITE_ONCE(desc->info, cpu_to_le32(0));
	ring->head = sdr_dma_next(ring, ring->head);
	ring->queued++;
}

int sdr_dma_ring_alloc(struct device *dev, enum sdr_dma_queue queue,
		       struct sdr_dma_ring **out)
{
	struct sdr_dma_ring *ring;
	u32 count, offset, i;
	int ret;

	if (!dev || !out || (unsigned int)queue >= ARRAY_SIZE(sdr_dma_queues))
		return -EINVAL;
	offset = sdr_dma_queues[queue].offset;
	count = queue == SDR_DMA_WM_RX ? sdr_dma_wm_rx_count : sdr_dma_queues[queue].count;
	ret = dma_set_mask(dev, DMA_BIT_MASK(MT7996_DMA_STREAMING_BITS));
	if (ret)
		return ret;
	ret = dma_set_coherent_mask(dev, DMA_BIT_MASK(MT7996_DMA_COHERENT_BITS));
	if (ret)
		return ret;
	ring = kzalloc(sizeof(*ring), GFP_KERNEL);
	if (!ring)
		return -ENOMEM;
	ring->dev = dev;
	ring->ndesc = count;
	ring->bar_offset = offset;
	ring->rx = queue == SDR_DMA_WM_RX;
	ring->rx_bytes = ring->rx ? sdr_dma_wm_rx_bytes : 0;
	ring->desc = dma_alloc_coherent(dev, count * sizeof(*ring->desc),
					 &ring->desc_dma, GFP_KERNEL);
	ret = -ENOMEM;
	if (!ring->desc)
		goto fail;
	ret = -ERANGE;
	if (!sdr_dma_fits(ring->desc_dma, count * sizeof(*ring->desc),
			  MT7996_DMA_COHERENT_BITS) ||
	    (ring->desc_dma & 3))
		goto fail;
	ring->buffers = kcalloc(count, sizeof(*ring->buffers), GFP_KERNEL);
	ret = -ENOMEM;
	if (!ring->buffers)
		goto fail;
	if (ring->rx) {
		ret = -EINVAL;
		ring->order = get_order(ring->rx_bytes);
		if (ring->rx_bytes != PAGE_SIZE << ring->order ||
		    sdr_dma_stream_bytes % ring->rx_bytes ||
		    sdr_dma_stream_bytes / ring->rx_bytes < count ||
		    sdr_dma_stream_bytes / ring->rx_bytes > U32_MAX >> ring->order)
			goto fail;
		ring->slices = sdr_dma_stream_bytes / ring->rx_bytes;
		ret = sdr_dma_stream_alloc(ring);
		if (ret)
			goto fail;
	}
	for (i = 0; i < count; i++)
		WRITE_ONCE(ring->desc[i].ctrl, cpu_to_le32(SDR_DMA_DONE));
	/* One descriptor stays empty, as in mt76. */
	for (i = 0; ring->rx && i < count - 1; i++) {
		sdr_dma_stream_view(ring, i, i);
		sdr_dma_post_rx(ring);
	}
	*out = ring;
	return 0;
fail:
	sdr_dma_ring_free_quiesced(ring);	/* not yet bound */
	return ret;
}

static void sdr_dma_kick(struct sdr_dma_ring *ring)
{
	wmb();	/* descriptors before the CPU index that publishes them */
	writel(ring->head, ring->regs + MT7996_WFDMA_RING_CIDX);
}

int sdr_dma_ring_bind(struct sdr_dma_ring *ring, void __iomem *bar,
		      size_t bar_bytes)
{

	if (!ring || !bar || bar_bytes < ring->bar_offset + MT7996_WFDMA_RING_REGS_BYTES)
		return -EINVAL;
	if (ring->exposed)
		return -EBUSY;
	ring->regs = bar + ring->bar_offset;
	ring->exposed = true;
	writel(0, ring->regs + MT7996_WFDMA_RING_CIDX);
	writel(0, ring->regs + MT7996_WFDMA_RING_DIDX);
	writel(ring->ndesc, ring->regs + MT7996_WFDMA_RING_CNT);
	writel(lower_32_bits(ring->desc_dma), ring->regs + MT7996_WFDMA_RING_BASE);
	/* The engine is stopped, so every word must read back exactly. */
	if (readl(ring->regs + MT7996_WFDMA_RING_BASE) != lower_32_bits(ring->desc_dma) ||
	    readl(ring->regs + MT7996_WFDMA_RING_CNT) != ring->ndesc ||
	    readl(ring->regs + MT7996_WFDMA_RING_CIDX) != 0 ||
	    readl(ring->regs + MT7996_WFDMA_RING_DIDX) != 0)
		goto fault;
	if (ring->rx) {
		sdr_dma_kick(ring);
		if (readl(ring->regs + MT7996_WFDMA_RING_CIDX) != ring->head)
			goto fault;
	}
	ring->bound = true;
	return 0;
fault:
	ring->fault = true;	/* still exposed: kept until reset */
	return -EIO;
}

int sdr_dma_ring_check(struct sdr_dma_ring *ring, struct device *dev,
		       enum sdr_dma_queue queue)
{
	if ((unsigned int)queue >= ARRAY_SIZE(sdr_dma_queues) || !ring || !dev ||
	    ring->dev != dev || ring->bar_offset != sdr_dma_queues[queue].offset)
		return -EINVAL;
	if (ring->fault)
		return -EIO;
	if (!ring->bound)
		return -ENXIO;
	return !ring->rx && ring->queued ? -EBUSY : 0;
}

static int sdr_dma_ready(struct sdr_dma_ring *ring, bool rx)
{
	if (!ring || ring->rx != rx)
		return -EINVAL;
	if (ring->fault)
		return -EIO;
	return ring->bound ? 0 : -ENXIO;
}

int sdr_dma_tx_submit(struct sdr_dma_ring *ring, const void *packet, size_t bytes)
{
	struct sdr_dma_buffer *buffer;
	struct sdr_dma_desc *desc;
	int ret = sdr_dma_ready(ring, false);

	if (ret)
		return ret;
	if (!packet || !bytes || bytes > MT7996_WFDMA_TX_CMD_MAX)
		return -EINVAL;
	/* Two slots stay unused, as in mt76. */
	if (ring->queued >= ring->ndesc - 2)
		return -ENOSPC;
	buffer = &ring->buffers[ring->head];
	ret = sdr_dma_buffer_alloc(ring, buffer, packet, bytes);
	if (ret)
		return ret;
	desc = &ring->desc[ring->head];
	WRITE_ONCE(desc->buf0, cpu_to_le32(lower_32_bits(buffer->dma)));
	WRITE_ONCE(desc->buf1, cpu_to_le32(0));
	WRITE_ONCE(desc->info, cpu_to_le32(upper_32_bits(buffer->dma)));
	WRITE_ONCE(desc->ctrl, cpu_to_le32((bytes << HW_SHIFT(WFDMA_DESC_CTRL_SD_LEN0)) |
					   SDR_DMA_LAST));
	ring->head = sdr_dma_next(ring, ring->head);
	ring->queued++;
	sdr_dma_kick(ring);
	return 0;
}

int sdr_dma_tx_reap(struct sdr_dma_ring *ring)
{
	u32 index, count, i;
	int ret = sdr_dma_ready(ring, false);

	if (ret)
		return ret;
	index = readl(ring->regs + MT7996_WFDMA_RING_DIDX);
	if (index >= ring->ndesc)
		goto fault;
	count = (index + ring->ndesc - ring->tail) % ring->ndesc;
	if (count > ring->queued)
		goto fault;
	for (i = 0; i < count; i++) {
		sdr_dma_buffer_free(ring, &ring->buffers[ring->tail]);
		ring->tail = sdr_dma_next(ring, ring->tail);
		ring->queued--;
	}
	return count;
fault:
	ring->fault = true;
	return -EIO;
}

/* mt76_dma_should_drop_buf: ctrl bit 12 TO_HOST_A and bit 14 DROP; with the
 * version bit 7, buf1[15:12] is the indication reason.
 */
static bool sdr_dma_rx_drop(u32 ctrl, u32 buf1, u32 info)
{
	bool drop = ctrl & (WFDMA_DESC_CTRL_TO_HOST_A | WFDMA_DESC_CTRL_DROP);
	u32 reason;

	if (!(ctrl & WFDMA_DESC_CTRL_VER))
		return drop;
	reason = HW_GET(WFDMA_DESC_RX_BUF1_REASON, buf1);
	if (reason == WFDMA_DESC_RX_BUF1_REASON_REPEAT)
		return true;
	if (reason == WFDMA_DESC_RX_BUF1_REASON_OLDPKT)
		return !(info & WFDMA_DESC_INFO_DMA_FRAG);
	return !!(ctrl & WFDMA_DESC_CTRL_PN_CHK_FAIL);
}

/* The completed descriptor at the tail, if any: its length, its ctrl word
 * and whether it is a well-formed single packet. No buffer is touched.
 */
static int sdr_dma_rx_tail(struct sdr_dma_ring *ring, u32 *bytes, u32 *ctrl,
			   bool *bad)
{
	struct sdr_dma_desc *desc = &ring->desc[ring->tail];
	u32 buf1, info;
	bool more;

	*ctrl = le32_to_cpu(READ_ONCE(desc->ctrl));
	if (!(*ctrl & SDR_DMA_DONE))
		return -EAGAIN;
	dma_rmb();	/* DONE before the rest of the descriptor */
	buf1 = le32_to_cpu(READ_ONCE(desc->buf1));
	info = le32_to_cpu(READ_ONCE(desc->info));
	*bytes = HW_GET(WFDMA_DESC_CTRL_SD_LEN0, *ctrl);
	more = !(*ctrl & SDR_DMA_LAST);
	*bad = !*bytes || *bytes > ring->buffers[ring->tail].bytes || more ||
		ring->fragment || sdr_dma_rx_drop(*ctrl, buf1, info);
	return 0;
}

/* Publish the tail completion to the header and arm the empty slot with the
 * slice ndesc - 2 completions ahead; the CPU never writes slices, so no sync
 * for the device is needed.
 */
static void sdr_dma_rx_recycle(struct sdr_dma_ring *ring, u32 ctrl)
{
	u32 slice = ring->buffers[ring->tail].id;

	ring->fragment = !(ctrl & SDR_DMA_LAST);
	WRITE_ONCE(ring->header->ctrl[slice], ctrl);
	/* ctrl word before the count that publishes the slice */
	smp_store_release(&ring->header->completed, ++ring->completed);
	ring->tail = sdr_dma_next(ring, ring->tail);
	ring->queued--;
	sdr_dma_stream_view(ring, ring->head,
			    (ring->completed + ring->ndesc - 2) % ring->slices);
	sdr_dma_post_rx(ring);
	sdr_dma_kick(ring);
}

int sdr_dma_rx_read(struct sdr_dma_ring *ring, void *packet, size_t capacity)
{
	struct sdr_dma_buffer buffer;
	u32 ctrl, bytes;
	bool bad;
	int ret = sdr_dma_ready(ring, true);

	if (ret)
		return ret;
	if (!packet || !capacity)
		return -EINVAL;
	ret = sdr_dma_rx_tail(ring, &bytes, &ctrl, &bad);
	if (ret)
		return ret;
	if (!bad && bytes > capacity)
		return -ENOSPC;
	buffer = ring->buffers[ring->tail];
	dma_sync_single_for_cpu(ring->dev, buffer.dma, buffer.bytes, DMA_FROM_DEVICE);
	if (!bad)
		memcpy(packet, buffer.data, bytes);
	sdr_dma_rx_recycle(ring, ctrl);
	return bad ? -EBADMSG : (int)bytes;
}

int sdr_dma_rx_next(struct sdr_dma_ring *ring, u32 *ctrl, u64 *seq)
{
	struct sdr_dma_buffer *buffer;
	u32 bytes, word;
	bool bad;
	int ret = sdr_dma_ready(ring, true);

	if (ret)
		return ret;
	ret = sdr_dma_rx_tail(ring, &bytes, &word, &bad);
	if (ret)
		return ret;
	buffer = &ring->buffers[ring->tail];
	/* Invalidate what the device wrote, so a mapping reader sees it. */
	bytes = min_t(u32, bytes, buffer->bytes);
	if (bytes)
		dma_sync_single_range_for_cpu(ring->dev, buffer->dma, 0, bytes,
					      DMA_FROM_DEVICE);
	if (ctrl)
		*ctrl = word;
	if (seq)
		*seq = ring->completed;
	sdr_dma_rx_recycle(ring, word);
	return bad ? -EBADMSG : 0;
}

struct page **sdr_dma_rx_pages(const struct sdr_dma_ring *ring, u32 *count)
{
	*count = ring && ring->header ?
		ring->header_pages + (ring->slices << ring->order) : 0;
	return *count ? ring->pages : NULL;
}

