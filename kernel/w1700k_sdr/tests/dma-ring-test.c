// SPDX-License-Identifier: GPL-2.0-only
/* dma-ring.c: allocation, binding, mt76 descriptors, ownership, the stream ring. */
#include "dma-mocks.h"
#include "fwdl_wire.c"

/* Descriptor words as mt76 writes them (checked against its own bytes below). */
static struct sdr_dma_desc tx_desc(dma_addr_t dma, u32 bytes)
{
	return (struct sdr_dma_desc){
		.buf0 = cpu_to_le32(lower_32_bits(dma)),
		.ctrl = cpu_to_le32(bytes << 16 | SDR_DMA_LAST),
		.buf1 = 0,
		.info = cpu_to_le32(upper_32_bits(dma)),
	};
}

static struct sdr_dma_desc rx_desc(dma_addr_t dma, u32 bytes)
{
	return (struct sdr_dma_desc){
		.buf0 = cpu_to_le32(lower_32_bits(dma)),
		.ctrl = cpu_to_le32(bytes << 16),
		.buf1 = cpu_to_le32(upper_32_bits(dma)),
		.info = 0,
	};
}

static void descriptor_layout_matches_mt76(void)
{
	struct sdr_dma_desc d;

	d = tx_desc(0xb12345000ULL, 4096);
	CHECK(!memcmp(&d, mt76_tx_desc_4096, 16));
	d = tx_desc(0xb12345000ULL, 76);
	CHECK(!memcmp(&d, mt76_tx_desc_76, 16));
	d = rx_desc(0xa12345000ULL, 2048);
	CHECK(!memcmp(&d, mt76_rx_desc_2048, 16));
	d = rx_desc(0xa12345000ULL, 8192);
	CHECK(!memcmp(&d, mt76_rx_desc_8192, 16));
}

/* A failed binding keeps the ring exposed: no reuse, no I/O, nothing freed. */
static void failed_bind_retains(struct sdr_dma_ring *r, enum sdr_dma_queue queue)
{
	unsigned int heaps = heap_count, maps = map_count, coherent = coherent_count;
	unsigned int before = event_count;
	u8 output = 0x5a;

	CHECK(r->fault && r->exposed && !r->bound);
	CHECK(sdr_dma_ring_check(r, DEVICE, queue) == -EIO);
	CHECK(sdr_dma_ring_bind(r, mock_bar, sizeof(mock_bar)) == -EBUSY);
	if (r->rx) {
		CHECK(sdr_dma_rx_read(r, &output, 1) == -EIO && output == 0x5a);
	} else {
		CHECK(sdr_dma_tx_submit(r, &output, 1) == -EIO);
		CHECK(sdr_dma_tx_reap(r) == -EIO);
	}
	CHECK(heap_count == heaps && map_count == maps && coherent_count == coherent);
	CHECK(event_count == before);
}

static void binding_faults(void)
{
	enum sdr_dma_queue queue;
	struct sdr_dma_ring *r;
	unsigned int at, bit, reads;

	for (queue = SDR_DMA_FWDL_TX; queue <= SDR_DMA_WM_RX; queue++) {
		reads = queue == SDR_DMA_WM_RX ? 5 : 4;
		/* Any failed or altered read-back, including after the RX head
		 * publication, faults the ring.
		 */
		for (at = 1; at <= reads; at++) {
			for (bit = 0; bit <= 32; bit++) {
				clean_state();
				CHECK(!sdr_dma_ring_alloc(DEVICE, queue, &r));
				corrupt_read_at = at;
				all_ones_read = bit == 32;
				corrupt_read_xor = bit < 32 ? UINT32_C(1) << bit : 0;
				CHECK(sdr_dma_ring_bind(r, mock_bar, sizeof(mock_bar)) == -EIO);
				CHECK(read_count == at && write_count == (at == 5 ? 5U : 4U));
				failed_bind_retains(r, queue);
				sdr_dma_ring_free_quiesced(r);
				no_leaks();
			}
		}
		/* A lost register write is caught by the read-back. */
		for (at = 1; at <= reads; at++) {
			clean_state();
			CHECK(!sdr_dma_ring_alloc(DEVICE, queue, &r));
			memset(mock_bar + r->bar_offset, 0xa5, 16);
			drop_write_at = at;
			CHECK(sdr_dma_ring_bind(r, mock_bar, sizeof(mock_bar)) == -EIO);
			failed_bind_retains(r, queue);
			sdr_dma_ring_free_quiesced(r);
			no_leaks();
		}
	}
}

static void allocation(void)
{
	struct sdr_dma_ring *r = NULL, *sentinel = (void *)1;
	unsigned int all_calls, n, i;
	u8 byte = 0;

	clean_state();
	CHECK(sdr_dma_ring_alloc(NULL, SDR_DMA_WM_RX, &sentinel) == -EINVAL);
	CHECK(sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX + 1, &sentinel) == -EINVAL);
	CHECK(sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, NULL) == -EINVAL);
	CHECK(sentinel == (void *)1 && !calls);
	for (mask_fail = 1; mask_fail <= 2; mask_fail++)
		CHECK(sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &sentinel) == -EIO);
	CHECK(sentinel == (void *)1 && !calls);
	clean_state();
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_TX, &r));
	all_calls = calls;
	CHECK(r->ndesc == mt76_ring_wm_tx.count && !r->queued && !r->rx && !map_count &&
	      coherent_count == 1 && !event_count);
	sdr_dma_ring_free_quiesced(r);
	/* Every allocation failure, including the last. */
	for (n = 1; n <= all_calls; n++) {
		clean_state();
		fail_at = n;
		CHECK(sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_TX, &sentinel) < 0);
		CHECK(sentinel == (void *)1 && !event_count);
		no_leaks();
	}
	/* The same for the stream ring, at 8 slots over 16 slices. */
	sdr_dma_wm_rx_count = 8;
	sdr_dma_stream_bytes = 16 * 4096;
	clean_state();
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &r));
	all_calls = calls;
	CHECK(r->slices == 16 && r->queued == 7 && map_count == 16 && page_blocks == 17);
	sdr_dma_ring_free_quiesced(r);
	for (n = 1; n <= all_calls; n++) {
		clean_state();
		fail_at = n;
		CHECK(sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &sentinel) < 0);
		CHECK(sentinel == (void *)1 && !event_count);
		no_leaks();
	}
	/* Geometry the stream ring refuses: partial pages, fewer slices than
	 * slots, a size that is not whole slices.
	 */
	{
		const u64 bad[][2] = { { 2048, 16 * 4096 }, { 4096, 4 * 4096 },
				       { 4096, 16 * 4096 + 100 } };

		for (i = 0; i < ARRAY_SIZE(bad); i++) {
			clean_state();
			sdr_dma_wm_rx_bytes = bad[i][0];
			sdr_dma_stream_bytes = bad[i][1];
			CHECK(sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &sentinel) == -EINVAL);
			no_leaks();
		}
	}
	sdr_dma_wm_rx_count = SDR_DMA_WM_RX_COUNT;
	sdr_dma_wm_rx_bytes = SDR_DMA_WM_RX_BYTES;
	sdr_dma_stream_bytes = SDR_DMA_STREAM_BYTES;
	{
		const dma_addr_t bad[] = { 0x100001, 0x100000000ULL, 0xfffffff0ULL };

		for (i = 0; i < ARRAY_SIZE(bad); i++) {
			clean_state();
			coherent_address = bad[i];
			CHECK(sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_TX, &sentinel) == -ERANGE);
			no_leaks();
		}
	}
	{
		const dma_addr_t bad[] = { 0x1000000000ULL, 0xffffffc00ULL };

		for (i = 0; i < ARRAY_SIZE(bad); i++) {
			clean_state();
			stream_address = bad[i];
			CHECK(sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &sentinel) == -ERANGE);
			no_leaks();
		}
	}
	clean_state();
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_FWDL_TX, &r));
	CHECK(sdr_dma_tx_submit(r, &byte, 1) == -ENXIO);
	CHECK(sdr_dma_tx_reap(r) == -ENXIO);
	CHECK(sdr_dma_rx_read(r, &byte, 1) == -EINVAL);
	CHECK(sdr_dma_ring_bind(r, mock_bar, r->bar_offset + 15) == -EINVAL);
	CHECK(sdr_dma_ring_bind(r, NULL, sizeof(mock_bar)) == -EINVAL);
	CHECK(!event_count && !r->exposed);
	sdr_dma_ring_free_quiesced(r);
	no_leaks();
}

static void tx(void)
{
	static u8 packet[4096];
	struct sdr_dma_ring *r;
	struct sdr_dma_desc want;
	u32 round, n, first, slot;
	size_t len;
	int queue;

	for (n = 0; n < sizeof(packet); n++)
		packet[n] = (u8)n;
	for (queue = SDR_DMA_FWDL_TX; queue <= SDR_DMA_WM_TX; queue++) {
		clean_state();
		CHECK(!sdr_dma_ring_alloc(DEVICE, queue, &r));
		bind_ring_queue(r, queue);
		CHECK(sdr_dma_tx_submit(r, packet, 0) == -EINVAL);
		CHECK(sdr_dma_tx_submit(r, packet, 4097) == -EINVAL);
		CHECK(sdr_dma_tx_submit(r, NULL, 1) == -EINVAL);
		for (n = 1; n <= 2; n++) {
			calls = 0;
			fail_at = n;
			CHECK(sdr_dma_tx_submit(r, packet, sizeof(packet)) < 0);
			CHECK(!r->queued && !map_count && !event_count);
		}
		fail_at = 0;
		stream_address = 0xffffff800ULL;
		CHECK(sdr_dma_tx_submit(r, packet, sizeof(packet)) == -ERANGE);
		stream_address = 0xb12345000ULL;
		/* Several wraps, full and short packets; two slots stay unused. */
		for (round = 0; round < 4; round++) {
			first = r->head;
			for (n = 0; n < r->ndesc - 2; n++) {
				slot = r->head;
				len = n & 1 ? 76 : sizeof(packet);
				reset_events();
				CHECK(!sdr_dma_tx_submit(r, packet, len));
				CHECK(!memcmp(r->buffers[slot].data, packet, len));
				want = tx_desc(r->buffers[slot].dma, len);
				CHECK(!memcmp(&r->desc[slot], &want, 16));
				CHECK(event_count == 2 && !strcmp(events[0].kind, "wmb"));
				CHECK(!strcmp(events[1].kind, "write32") &&
				      events[1].address == r->bar_offset + 8 &&
				      events[1].value == r->head);
				packet[0]++;	/* the ring keeps its own copy */
				CHECK(((u8 *)r->buffers[slot].data)[0] != packet[0]);
			}
			reset_events();
			CHECK(sdr_dma_tx_submit(r, packet, 1) == -ENOSPC && !event_count);
			set_index(r, first);
			CHECK(!sdr_dma_tx_reap(r) && map_count == r->ndesc - 2);
			set_index(r, (first + 7) % r->ndesc);
			CHECK(sdr_dma_tx_reap(r) == 7 && map_count == r->ndesc - 9);
			set_index(r, r->head);
			CHECK(sdr_dma_tx_reap(r) == (int)r->ndesc - 9);
			CHECK(!map_count && !r->queued && r->head == r->tail);
		}
		sdr_dma_ring_free_quiesced(r);
		no_leaks();
	}
	/* An impossible DMA index latches a fault and keeps the buffer mapped. */
	for (n = 0; n < 3; n++) {
		const u32 bad[] = { 2, 128, UINT32_MAX };
		u8 byte = 42;

		clean_state();
		CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_FWDL_TX, &r));
		bind_ring(r);
		CHECK(!sdr_dma_tx_submit(r, &byte, 1));
		set_index(r, bad[n]);
		reset_events();
		CHECK(sdr_dma_tx_reap(r) == -EIO && map_count == 1 && r->queued == 1);
		set_index(r, r->head);
		CHECK(sdr_dma_tx_reap(r) == -EIO && map_count == 1);
		CHECK(sdr_dma_tx_submit(r, &byte, 1) == -EIO);
		sdr_dma_ring_free_quiesced(r);
		no_leaks();
	}
}

/* Download replies read (copied) through the stream ring. */
static void rx(void)
{
	static const u32 bad[] = {
		SDR_DMA_LAST, (4097U << 16) | SDR_DMA_LAST, (0x3fffU << 16) | SDR_DMA_LAST,
		(44U << 16) | SDR_DMA_LAST | BIT(14), (44U << 16) | SDR_DMA_LAST | BIT(12),
		44U << 16, 44U << 16, (44U << 16) | SDR_DMA_LAST,
	};
	static u8 out[4098], payload[4096];
	struct sdr_dma_ring *r;
	u32 n, i, len, head, tail, bits, reason, frag, ctrl, ahead;

	clean_state();
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &r));
	CHECK(r->desc[r->ndesc - 1].ctrl == cpu_to_le32(SDR_DMA_DONE));
	bind_ring(r);
	memset(payload, 0x42, sizeof(payload));
	CHECK(sdr_dma_rx_read(r, out, sizeof(out)) == -EAGAIN && !cpu_syncs && !event_count);
	CHECK(sdr_dma_rx_read(r, NULL, 1) == -EINVAL);
	CHECK(sdr_dma_rx_read(r, out, 0) == -EINVAL);
	for (n = 0; n < 1100; n++) {	/* past a lap of the slots and of the slices */
		len = n & 1 ? 44 : 4096;
		complete_rx(r, payload, len, (len << 16) | SDR_DMA_LAST);
		memset(out, 0xa5, sizeof(out));
		head = r->head;
		tail = r->tail;
		CHECK(sdr_dma_rx_read(r, out + 1, len - 1) == -ENOSPC);
		CHECK(r->head == head && r->tail == tail && r->header->completed == n);
		reset_events();
		CHECK(sdr_dma_rx_read(r, out + 1, len) == (int)len);
		CHECK(out[0] == 0xa5 && out[len + 1] == 0xa5 && !memcmp(out + 1, payload, len));
		CHECK(r->queued == r->ndesc - 1 && r->header->completed == n + 1);
		/* Barrier, invalidate, copy, publish; nothing handed back. */
		CHECK(!strcmp(events[0].kind, "dma_rmb") && !strcmp(events[1].kind, "sync_cpu"));
		CHECK(!strcmp(events[2].kind, "copy_rx") && !strcmp(events[3].kind, "wmb"));
		CHECK(!strcmp(events[4].kind, "write32") && events[4].value == r->head &&
		      event_count == 5);
		ahead = (n + r->ndesc - 1) % r->slices;
		CHECK(r->buffers[head].id == ahead &&
		      le32_to_cpu(r->desc[head].buf0) == (u32)r->slice_dma[ahead] &&
		      r->desc[head].ctrl == cpu_to_le32(4096U << 16));
	}
	CHECK(cpu_syncs == 1100 && !device_syncs);
	for (i = 0; i < ARRAY_SIZE(bad); i++) {
		complete_rx(r, NULL, 0, bad[i]);
		memset(out, 0xa5, sizeof(out));
		reset_events();
		CHECK(sdr_dma_rx_read(r, out, sizeof(out)) == -EBADMSG);
		for (n = 0; n < sizeof(out); n++)
			CHECK(out[n] == 0xa5);
		CHECK(event_count == 4 && r->queued == r->ndesc - 1);
	}
	CHECK(!r->fragment && r->header->completed == 1100 + ARRAY_SIZE(bad));
	complete_rx(r, payload, 44, (44U << 16) | SDR_DMA_LAST);
	CHECK(sdr_dma_rx_read(r, out, sizeof(out)) == 44);
	sdr_dma_ring_free_quiesced(r);
	no_leaks();
	/* The drop rule, every version/reason/flag combination against mt76. */
	for (bits = 0; bits < 32; bits++)
		for (reason = 0; reason < 16; reason++)
			for (frag = 0; frag < 2; frag++) {
				ctrl = ((bits & 1) ? BIT(7) : 0) | ((bits & 2) ? BIT(12) : 0) |
				       ((bits & 4) ? BIT(13) : 0) | ((bits & 8) ? BIT(14) : 0) |
				       ((bits & 16) ? BIT(8) : 0);
				CHECK(sdr_dma_rx_drop(ctrl, reason << 12, frag << 9) ==
				      (mt76_rx_drop_table[bits * 32 + reason * 2 + frag] == '1'));
			}
}

/* A download reply through real rings and the wire parser. */
static void wire_composition(void)
{
	u8 packet[76], reply[44] = { 0 }, out[44];
	struct sdr_dma_ring *tx, *rx;
	unsigned int status;
	int len;

	clean_state();
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_TX, &tx));
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &rx));
	bind_ring(tx);
	bind_ring(rx);
	CHECK(mt7996_fwdl_init(packet, sizeof(packet), 1, 0x916400, 4096, 0) == 76);
	CHECK(!sdr_dma_tx_submit(tx, packet, sizeof(packet)));
	CHECK(!memcmp(tx->buffers[0].data, packet, sizeof(packet)));
	set_index(tx, tx->head);
	CHECK(sdr_dma_tx_reap(tx) == 1);
	CHECK(sdr_dma_rx_read(rx, out, sizeof(out)) == -EAGAIN);
	put_unaligned_le32(0x3800002c, reply);
	reply[32] = 12;
	reply[34] = 7;
	reply[36] = reply[37] = 1;
	for (status = 0; status < 256; status++) {
		reply[40] = (u8)status;
		complete_rx(rx, reply, sizeof(reply), (44U << 16) | SDR_DMA_LAST);
		reset_events();
		len = sdr_dma_rx_read(rx, out, sizeof(out));
		CHECK(len == 44 && mt7996_fwdl_reply_status(out, len, 1, MT7996_FWDL_EID_GENERIC) ==
		      (int)status);
	}
	sdr_dma_ring_free_quiesced(tx);
	sdr_dma_ring_free_quiesced(rx);
	no_leaks();
}

/* Completion c lands in slice c % slices whichever slot carries it; the
 * header publishes every completion; a mapping's page references outlive
 * the ring; events still read through it.
 */
static void stream(void)
{
	static u8 payload[4096], out[2048];
	struct sdr_dma_stream_header *h;
	struct sdr_dma_ring *r;
	struct page **pages, *mapped;
	u32 ctrl, count, n, len, slice, head, ahead, tail, syncs, i;
	u64 seq;

	clean_state();
	CHECK(!sdr_dma_ring_alloc(DEVICE, SDR_DMA_WM_RX, &r));
	h = r->header;
	pages = sdr_dma_rx_pages(r, &count);
	CHECK(h == page_address(pages[0]) && r->header_pages == 2 && count == 2 + 1024);
	CHECK(h->slots == 512 && h->slices == 1024 && h->slice_bytes == 4096 &&
	      h->header_bytes == 8192 && !h->completed);
	CHECK(!sdr_dma_rx_pages(NULL, &count) && !count);
	CHECK(map_count == 1024 && page_blocks == 1 + 1024 && r->queued == 511);
	for (i = 0; i < 511; i++)
		CHECK(r->buffers[i].id == i && r->buffers[i].dma == r->slice_dma[i] &&
		      r->buffers[i].data == page_address(pages[2 + i]) &&
		      le32_to_cpu(r->desc[i].buf0) == (u32)r->slice_dma[i] &&
		      r->desc[i].ctrl == cpu_to_le32(4096U << 16));
	bind_ring(r);
	CHECK(sdr_dma_rx_next(r, &ctrl, &seq) == -EAGAIN && !cpu_syncs);
	mapped = pages[2 + 700];
	get_page(mapped);		/* a user mapping */
	for (n = 0; n < 2600; n++) {	/* 2.5 laps of the slices */
		len = n % 7 ? 4096 : 100 + n % 50;
		slice = n % 1024;
		memset(payload, (int)(n * 13 + 1), len);
		complete_rx(r, payload, len, (len << 16) | SDR_DMA_LAST);
		CHECK(r->buffers[r->tail].id == slice);
		head = r->head;
		reset_events();
		seq = ~0ULL;
		CHECK(!sdr_dma_rx_next(r, &ctrl, &seq));
		CHECK(seq == n && ctrl == ((len << 16) | SDR_DMA_LAST | SDR_DMA_DONE));
		CHECK(h->completed == n + 1 && h->ctrl[slice] == ctrl);
		CHECK(!memcmp(page_address(pages[2 + slice]), payload, len));
		/* Invalidate, publish, re-arm the empty slot 511 completions
		 * ahead; nothing is handed back to the device.
		 */
		CHECK(!strcmp(events[0].kind, "dma_rmb") && !strcmp(events[1].kind, "sync_cpu") &&
		      events[1].value == len);
		CHECK(!strcmp(events[2].kind, "wmb") && !strcmp(events[3].kind, "write32") &&
		      events[3].value == r->head && event_count == 4);
		ahead = (n + 511) % 1024;
		CHECK(r->buffers[head].id == ahead &&
		      le32_to_cpu(r->desc[head].buf0) == (u32)r->slice_dma[ahead]);
		CHECK(r->desc[head].ctrl == cpu_to_le32(4096U << 16) && r->queued == 511);
	}
	CHECK(!device_syncs && cpu_syncs == 2600);
	complete_rx(r, payload, 44, (44U << 16) | SDR_DMA_LAST);
	CHECK(sdr_dma_rx_read(r, out, sizeof(out)) == 44 && h->completed == 2601);
	/* Larger than an event: the reader leaves it, next consumes it. */
	complete_rx(r, payload, 3000, (3000U << 16) | SDR_DMA_LAST);
	tail = r->tail;
	CHECK(sdr_dma_rx_read(r, out, sizeof(out)) == -ENOSPC && r->tail == tail);
	CHECK(!sdr_dma_rx_next(r, NULL, NULL) && r->tail != tail && h->completed == 2602);
	/* Malformed: consumed, published, and its continuation is tainted. */
	complete_rx(r, payload, 100, 100U << 16);
	CHECK(sdr_dma_rx_next(r, &ctrl, &seq) == -EBADMSG && seq == 2602 && r->fragment);
	complete_rx(r, payload, 100, (100U << 16) | SDR_DMA_LAST);
	CHECK(sdr_dma_rx_next(r, &ctrl, &seq) == -EBADMSG && !r->fragment);
	complete_rx(r, NULL, 0, SDR_DMA_LAST);
	syncs = cpu_syncs;
	CHECK(sdr_dma_rx_next(r, &ctrl, &seq) == -EBADMSG && cpu_syncs == syncs);
	CHECK(sdr_dma_rx_next(r, NULL, NULL) == -EAGAIN);
	/* Freed after a reset, the mapped page lives on until unmapped. */
	sdr_dma_ring_free_quiesced(r);
	CHECK(!map_count && page_blocks == 1);
	put_page(mapped);
	no_leaks();
}

int main(void)
{
	descriptor_layout_matches_mt76();
	allocation();
	binding_faults();
	tx();
	rx();
	stream();
	wire_composition();
	printf("dma-ring: %lu checks\n", checks);
	return 0;
}
