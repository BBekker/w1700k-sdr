/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Host WFDMA0 rings for the MCU queues, with the mt76 descriptor format. The
 * caller serializes every call and owns the device lifetime. Nothing here
 * enables WFDMA, interrupts or bus mastering.
 */
#ifndef W1700K_SDR_DMA_RING_H
#define W1700K_SDR_DMA_RING_H

#include <linux/types.h>

struct device;
struct page;
struct sdr_dma_ring;

enum sdr_dma_queue {
	SDR_DMA_FWDL_TX,	/* TX 16: firmware scatter chunks */
	SDR_DMA_WM_TX,		/* TX 17: download commands */
	SDR_DMA_WM_RX,		/* RX 0: download replies, and the stream ring */
};

int sdr_dma_ring_alloc(struct device *dev, enum sdr_dma_queue queue,
		       struct sdr_dma_ring **out);

/*
 * Program BASE/CNT/CPU_IDX/DMA_IDX on a stopped engine, read all four back,
 * and publish the RX head. From the first register write the storage is
 * exposed to the device: on failure too, the ring must be kept until the
 * engine is reset.
 */
int sdr_dma_ring_bind(struct sdr_dma_ring *ring, void __iomem *bar,
		      size_t bar_bytes);

/* No MMIO: the ring is bound to this device and queue, not faulted, and a TX
 * ring has nothing outstanding.
 */
int sdr_dma_ring_check(struct sdr_dma_ring *ring, struct device *dev,
		       enum sdr_dma_queue queue);

/* Free after the engine that could reach the ring has been reset. */
void sdr_dma_ring_free_quiesced(struct sdr_dma_ring *ring);

/* TX: one descriptor carrying a private copy of 1..4096 bytes, kept mapped
 * until DMA_IDX passes it. Reap returns the number of completed descriptors.
 */
int sdr_dma_tx_submit(struct sdr_dma_ring *ring, const void *packet, size_t bytes);
int sdr_dma_tx_reap(struct sdr_dma_ring *ring);

/*
 * The WM event ring, the only RX ring, is the stream ring: behind its
 * descriptors is one circular buffer of `slices` slices, and completion c
 * lands in slice c % slices because each re-armed descriptor points at the
 * next slice in turn. The header (the mmap ABI struct w1700k_sdr_stream_ring)
 * records each slice's ctrl word and publishes the completion count.
 */
struct sdr_dma_stream_header {
	u64 completed;
	u32 slots, slices, slice_bytes, header_bytes, reserved[10];
	u32 ctrl[];
};

/* Consume the tail completion in place: invalidate it for the CPU, record
 * its ctrl word, publish (release) and re-arm. 0, -EBADMSG for a malformed
 * completion (still consumed and published), or -EAGAIN. ctrl and seq may be
 * NULL.
 */
int sdr_dma_rx_next(struct sdr_dma_ring *ring, u32 *ctrl, u64 *seq);

/*
 * The same for a download reply, also copied out. Returns the length,
 * -EAGAIN when none, -ENOSPC (completion kept) when it does not fit, or
 * -EBADMSG for a malformed, dropped or fragmented completion (consumed).
 */
int sdr_dma_rx_read(struct sdr_dma_ring *ring, void *packet, size_t capacity);

/* The stream ring's pages in mapping order (header, then slices), each an
 * order-0 page with its own reference count. NULL for other rings.
 */
struct page **sdr_dma_rx_pages(const struct sdr_dma_ring *ring, u32 *count);

#endif
