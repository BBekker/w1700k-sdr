/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Allocator, page and DMA-API mocks with ownership checks, failure injection
 * and fault injection on BAR reads/writes; then dma-ring.c itself at a small
 * stream-ring geometry (512 descriptors over 1024 one-page slices) and ring
 * helpers shared by the ring and executor tests.
 */
#ifndef W1700K_SDR_TEST_DMA_MOCKS_H
#define W1700K_SDR_TEST_DMA_MOCKS_H
#include "harness.h"
#include "mt76_vectors.h"

#define GFP_KERNEL 0
#define __GFP_ZERO 0
#define DMA_BIT_MASK(n) ((UINT64_C(1) << (n)) - 1)
#define DMA_TO_DEVICE 1
#define DMA_FROM_DEVICE 2
#define PAGE_SHIFT 12
#define PAGE_SIZE (UINT64_C(1) << PAGE_SHIFT)
#define PAGE_ALIGN(x) (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define smp_store_release(p, v) __atomic_store_n(p, v, __ATOMIC_RELEASE)

struct device { int unused; };
struct pci_dev { struct device dev; bool offline; };
static struct pci_dev test_pdev;
#define DEVICE (&test_pdev.dev)

static unsigned int heap_count, map_count, coherent_count, calls, fail_at;
static unsigned int cpu_syncs, device_syncs, page_blocks;
static int mask_fail;
static dma_addr_t coherent_address = 0x100000;
static dma_addr_t stream_address = 0xa12345000ULL;
static u64 read_override = UINT64_MAX;
static unsigned int read_count, write_count, corrupt_read_at, drop_write_at;
static u32 corrupt_read_xor;
static bool all_ones_read;

struct mapping {
	void *data;
	dma_addr_t dma;
	size_t bytes;
	int direction;
	bool live, cpu;
};
static struct mapping dma_maps[4096];
static unsigned int maps_high;	/* entries in use start below this */

static inline bool fail(void)
{
	return ++calls == fail_at;
}

static inline void *kmalloc(size_t bytes, int flags)
{
	void *p;

	(void)flags;
	if (fail())
		return NULL;
	p = malloc(bytes);
	CHECK(p);
	heap_count++;
	return p;
}

static inline void *kzalloc(size_t bytes, int flags)
{
	void *p = kmalloc(bytes, flags);

	if (p)
		memset(p, 0, bytes);
	return p;
}

static inline void *kcalloc(size_t count, size_t bytes, int flags)
{
	return kzalloc(count * bytes, flags);
}

static inline void kfree(const void *p)
{
	unsigned int i;

	if (!p)
		return;
	for (i = 0; i < maps_high; i++)
		if (dma_maps[i].live && dma_maps[i].data == p)
			CHECK(!"freed while mapped");
	CHECK(heap_count);
	heap_count--;
	free((void *)p);
}

static inline void *kvcalloc(size_t count, size_t bytes, int flags)
{
	return kcalloc(count, bytes, flags);
}

static inline void kvfree(const void *p)
{
	kfree(p);
}

/* One block per allocation; split_page gives each page its own count, and
 * the block is freed when the last page's count drops.
 */
struct page_block;
struct page { void *addr; int refs; struct page_block *block; };
struct page_block { void *base; size_t bytes; unsigned int live; struct page pages[]; };

static inline unsigned int get_order(size_t bytes)
{
	unsigned int o = 0;

	while ((PAGE_SIZE << o) < bytes)
		o++;
	return o;
}

static inline struct page *alloc_pages(int flags, unsigned int order)
{
	struct page_block *b;
	unsigned int i;

	(void)flags;
	if (fail())
		return NULL;
	b = calloc(1, sizeof(*b) + (sizeof(struct page) << order));
	CHECK(b);
	b->bytes = PAGE_SIZE << order;
	b->base = aligned_alloc(PAGE_SIZE, b->bytes);
	CHECK(b->base);
	memset(b->base, 0, b->bytes);
	for (i = 0; i < 1U << order; i++)
		b->pages[i] = (struct page){ (char *)b->base + i * PAGE_SIZE, !i, b };
	b->live = 1U << order;
	page_blocks++;
	return b->pages;
}

static inline void split_page(struct page *p, unsigned int order)
{
	unsigned int i;

	for (i = 1; i < 1U << order; i++) {
		CHECK(!p[i].refs);
		p[i].refs = 1;
	}
}

static inline void *page_address(struct page *p)
{
	return p->addr;
}

static inline void get_page(struct page *p)
{
	CHECK(p->refs > 0);
	p->refs++;
}

static inline void put_page(struct page *p)
{
	struct page_block *b = p->block;
	unsigned int i;

	CHECK(p->refs > 0);
	if (--p->refs || --b->live)
		return;
	for (i = 0; i < maps_high; i++)
		if (dma_maps[i].live && (char *)dma_maps[i].data >= (char *)b->base &&
		    (char *)dma_maps[i].data < (char *)b->base + b->bytes)
			CHECK(!"page freed while mapped");
	free(b->base);
	free(b);
	page_blocks--;
}

static inline int dma_set_mask(struct device *dev, u64 mask)
{
	CHECK(dev == DEVICE && mask == DMA_BIT_MASK(36));
	return mask_fail == 1 ? -EIO : 0;
}

static inline int dma_set_coherent_mask(struct device *dev, u64 mask)
{
	CHECK(dev == DEVICE && mask == DMA_BIT_MASK(32));
	return mask_fail == 2 ? -EIO : 0;
}

static inline void *dma_alloc_coherent(struct device *dev, size_t bytes, dma_addr_t *dma,
				       int flags)
{
	void *p = kzalloc(bytes, flags);

	(void)dev;
	if (p) {
		*dma = coherent_address;
		coherent_count++;
	}
	return p;
}

static inline void dma_free_coherent(struct device *dev, size_t bytes, void *p,
				     dma_addr_t dma)
{
	(void)bytes;
	CHECK(dev == DEVICE && dma == coherent_address && coherent_count);
	coherent_count--;
	kfree(p);
}

static inline dma_addr_t dma_map_single(struct device *dev, void *p, size_t bytes,
					int direction)
{
	unsigned int i;

	CHECK(dev == DEVICE && p && bytes && (direction == 1 || direction == 2));
	if (fail())
		return UINT64_MAX;
	for (i = 0; i < ARRAY_SIZE(dma_maps); i++) {
		if (dma_maps[i].live)
			continue;
		dma_maps[i] = (struct mapping){ p, stream_address + i * 4096, bytes,
						direction, true, false };
		map_count++;
		if (i >= maps_high)
			maps_high = i + 1;
		return dma_maps[i].dma;
	}
	CHECK(false);
	return 0;
}

static inline bool dma_mapping_error(struct device *dev, dma_addr_t dma)
{
	(void)dev;
	return dma == UINT64_MAX;
}

static inline struct mapping *mapping(dma_addr_t dma, size_t bytes, int direction)
{
	unsigned int i;

	for (i = 0; i < maps_high; i++) {
		struct mapping *m = &dma_maps[i];

		if (m->live && m->dma == dma) {
			CHECK(m->bytes == bytes && m->direction == direction);
			return m;
		}
	}
	CHECK(false);
	return NULL;
}

static inline void dma_unmap_single(struct device *dev, dma_addr_t dma, size_t bytes,
				    int direction)
{
	CHECK(dev == DEVICE && map_count);
	mapping(dma, bytes, direction)->live = false;
	map_count--;
}

static inline void dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma,
					   size_t bytes, int dir)
{
	struct mapping *m = mapping(dma, bytes, dir);

	CHECK(dev == DEVICE && dir == DMA_FROM_DEVICE);
	m->cpu = true;
	cpu_syncs++;
	log_event("sync_cpu", 0, bytes);
}

/* Repeated CPU syncs are allowed: the stream ring invalidates each lap
 * without handing the slice back (the CPU never writes it).
 */
static inline void dma_sync_single_range_for_cpu(struct device *dev, dma_addr_t dma,
						 unsigned long offset, size_t bytes, int dir)
{
	struct mapping *m = NULL;
	unsigned int i;

	for (i = 0; i < maps_high && !m; i++)
		if (dma_maps[i].live && dma_maps[i].dma == dma)
			m = &dma_maps[i];
	CHECK(m && dev == DEVICE && dir == DMA_FROM_DEVICE && m->direction == dir);
	CHECK(bytes && offset + bytes <= m->bytes);
	m->cpu = true;
	cpu_syncs++;
	log_event("sync_cpu", 0, bytes);
}

static inline void dma_sync_single_for_device(struct device *dev, dma_addr_t dma,
					      size_t bytes, int dir)
{
	struct mapping *m = mapping(dma, bytes, dir);

	CHECK(dev == DEVICE && dir == DMA_FROM_DEVICE && m->cpu);
	m->cpu = false;
	device_syncs++;
	log_event("sync_device", 0, bytes);
}

/* Copies out of a mapped RX buffer only after a CPU sync; TX data is copied
 * before it is mapped and never into a mapped buffer.
 */
static inline void *checked_copy(void *dst, const void *src, size_t bytes)
{
	unsigned int i;

	for (i = 0; i < maps_high; i++) {
		struct mapping *m = &dma_maps[i];

		if (!m->live)
			continue;
		if (m->data == src) {
			CHECK(m->direction == DMA_FROM_DEVICE && m->cpu && bytes <= m->bytes);
			log_event("copy_rx", 0, bytes);
		}
		CHECK(m->data != dst);
	}
	return memcpy(dst, src, bytes);
}

static inline u32 checked_readl(const void *p)
{
	u32 value = mock_readl(p);

	read_count++;
	if (read_override != UINT64_MAX)
		value = (u32)read_override;
	if (read_count == corrupt_read_at)
		value = all_ones_read ? UINT32_MAX : value ^ corrupt_read_xor;
	events[event_count - 1].value = value;
	return value;
}

static inline void checked_writel(u32 value, void *p)
{
	if (++write_count == drop_write_at) {
		log_event("write32", bar_offset(p, 4), value);
		return;
	}
	mock_writel(value, p);
}

#undef readl
#undef writel
#define readl checked_readl
#define writel checked_writel
#define memcpy checked_copy
#define SDR_DMA_WM_RX_COUNT 512
#define SDR_DMA_WM_RX_BYTES 4096
#define SDR_DMA_STREAM_BYTES (UINT64_C(4096) * 1024)
#include "dma-ring.c"
#undef memcpy
#undef readl
#undef writel
#define readl mock_readl
#define writel mock_writel

static inline void no_leaks(void)
{
	CHECK(!heap_count && !map_count && !coherent_count && !page_blocks);
}

static inline void clean_state(void)
{
	no_leaks();
	maps_high = 0;
	calls = fail_at = 0;
	mask_fail = 0;
	coherent_address = 0x100000;
	stream_address = 0xa12345000ULL;
	read_override = UINT64_MAX;
	read_count = write_count = corrupt_read_at = drop_write_at = 0;
	corrupt_read_xor = 0;
	all_ones_read = false;
	cpu_syncs = device_syncs = 0;
	reset_events();
	memset(mock_bar, 0, sizeof(mock_bar));
}

/* The device's DMA_IDX for this ring. */
static inline void set_index(struct sdr_dma_ring *r, u32 index)
{
	put_unaligned_le32(index, mock_bar + r->bar_offset + 12);
}

/* Ring register blocks and sizes, as mt76 configures them; the WM event
 * ring's size is this driver's own (the stream ring).
 */
static inline struct mt76_ring_vector ring_reference(enum sdr_dma_queue queue)
{
	switch (queue) {
	case SDR_DMA_FWDL_TX: return mt76_ring_fwdl_tx;
	case SDR_DMA_WM_TX: return mt76_ring_wm_tx;
	case SDR_DMA_WM_RX:
		return (struct mt76_ring_vector){ mt76_ring_wm_rx.base, sdr_dma_wm_rx_count };
	}
	CHECK(false);
	return (struct mt76_ring_vector){ 0, 0 };
}

/* Bind and check the exact register sequence: zero indices, count, base,
 * read back all four; an RX ring then publishes its head.
 */
static inline void bind_ring_queue(struct sdr_dma_ring *r, enum sdr_dma_queue queue)
{
	static const u32 offsets[] = { 8, 12, 4, 0, 0, 4, 8, 12 };
	const u32 values[] = { 0, 0, r->ndesc, (u32)r->desc_dma,
			       (u32)r->desc_dma, r->ndesc, 0, 0 };
	struct mt76_ring_vector ref = ring_reference(queue);
	unsigned int i;

	CHECK(r->bar_offset == ref.base && r->ndesc == ref.count);
	reset_events();
	CHECK(!sdr_dma_ring_bind(r, mock_bar, sizeof(mock_bar)));
	CHECK(event_count == (r->rx ? 11U : 8U));
	for (i = 0; i < 8; i++) {
		CHECK(!strcmp(events[i].kind, i >= 4 ? "read32" : "write32"));
		CHECK(events[i].address == r->bar_offset + offsets[i]);
		CHECK(events[i].value == values[i]);
	}
	if (r->rx) {
		CHECK(!strcmp(events[8].kind, "wmb"));
		CHECK(!strcmp(events[9].kind, "write32") && events[9].value == r->ndesc - 1);
		CHECK(!strcmp(events[10].kind, "read32") && events[10].value == r->ndesc - 1);
		CHECK(events[9].address == r->bar_offset + 8 &&
		      events[10].address == r->bar_offset + 8);
	}
	reset_events();
	CHECK(sdr_dma_ring_bind(r, mock_bar, sizeof(mock_bar)) == -EBUSY && !event_count);
	CHECK(r->exposed);
}

static inline void bind_ring(struct sdr_dma_ring *r)
{
	enum sdr_dma_queue queue;

	if (r->rx)
		queue = SDR_DMA_WM_RX;
	else if (r->bar_offset == mt76_ring_fwdl_tx.base)
		queue = SDR_DMA_FWDL_TX;
	else
		queue = SDR_DMA_WM_TX;
	bind_ring_queue(r, queue);
}

/* The device completes its next posted descriptor (the first one from the
 * tail not yet done), writing `data` into its buffer.
 */
static inline void complete_rx(struct sdr_dma_ring *r, const void *data, size_t bytes,
			       u32 ctrl)
{
	u32 slot = r->tail;
	struct sdr_dma_buffer *b;
	struct mapping *m;

	while (le32_to_cpu(r->desc[slot].ctrl) & SDR_DMA_DONE) {
		slot = sdr_dma_next(r, slot);
		CHECK(slot != r->head);		/* every posted buffer is full */
	}
	b = &r->buffers[slot];
	m = mapping(b->dma, b->bytes, DMA_FROM_DEVICE);
	m->cpu = false;
	if (data) {
		CHECK(bytes <= b->bytes);
		memcpy(b->data, data, bytes);
	}
	r->desc[slot].ctrl = cpu_to_le32(ctrl | SDR_DMA_DONE);
}
#endif
