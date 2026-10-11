// SPDX-License-Identifier: GPL-2.0-only
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include "dma-ring.h"
#include "fwdl.h"
#include "fwdl-image.h"
#include "fwdl_wire.h"
#include "mmio.h"
#include "hw/mt7996/fw_format.h"
#include "hw/mt7996/host.h"
#include "hw/mt7996/mcu_cmd.h"

#define SDR_FWDL_TIMEOUT_NS (30ULL * NSEC_PER_SEC)
/* Bounds the loop even when the clock does not advance. */
#define SDR_FWDL_POLL_LIMIT 30000U
/* mt7996_firmware_state(FW_STATE_FW_DOWNLOAD): MT_TOP_MISC[2:0], polled
 * every 10-20 ms, 101 reads.
 */
#define SDR_FWDL_STATE_POLLS 101U
#define SDR_FWDL_EVENT_BYTES 2048U
/* The MCU event header after the RX prefix, as logged. */
#define SDR_FWDL_EVENT_HEADER ((int)(MT7996_MCU_EVT_BODY - MT7996_MCU_RXD_HW_BYTES))

struct sdr_fwdl {
	struct pci_dev *pdev;
	struct sdr_mmio io;
	struct sdr_dma_ring *tx, *wm, *rx;
	struct mutex lock;
	struct sdr_fwdl_result progress;
	u64 began, budget_ns;
	u32 polls;
	u32 started;		/* bitmask of enum sdr_fwdl_ram_type */
	bool spent;
	u8 event[SDR_FWDL_EVENT_BYTES];
};

static int sdr_fwdl_rings(struct sdr_fwdl *loader)
{
	struct device *dev = &loader->pdev->dev;
	int ret = sdr_dma_ring_check(loader->tx, dev, SDR_DMA_FWDL_TX);

	if (!ret)
		ret = sdr_dma_ring_check(loader->wm, dev, SDR_DMA_WM_TX);
	if (!ret)
		ret = sdr_dma_ring_check(loader->rx, dev, SDR_DMA_WM_RX);
	return ret;
}

int sdr_fwdl_alloc(struct pci_dev *pdev, const struct sdr_mmio *io,
		   struct sdr_dma_ring *fwdl,
		   struct sdr_dma_ring *wm, struct sdr_dma_ring *rx,
		   struct sdr_fwdl **out)
{
	struct sdr_fwdl *loader;
	int ret;

	if (!pdev || !io || !io->ops || !io->ops->read32 ||
	    io->bar_size < MT7996_TOP_MISC + 4U || !out)
		return -EINVAL;
	loader = kzalloc(sizeof(*loader), GFP_KERNEL);
	if (!loader)
		return -ENOMEM;
	loader->pdev = pdev;
	loader->io = *io;
	loader->tx = fwdl;
	loader->wm = wm;
	loader->rx = rx;
	loader->budget_ns = SDR_FWDL_TIMEOUT_NS;
	ret = sdr_fwdl_rings(loader);
	if (ret) {
		kfree(loader);
		return ret;
	}
	mutex_init(&loader->lock);
	*out = loader;
	return 0;
}

void sdr_fwdl_free(struct sdr_fwdl *loader)
{
	kfree(loader);
}

int sdr_fwdl_run_image(struct sdr_fwdl *loader, const void *image, size_t bytes,
		       u32 ram_type, struct sdr_fwdl_result *result)
{
	struct sdr_fwdl_image parsed;
	struct sdr_fwdl_request req = {};
	int ram, patch;

	if (!loader || !result)
		return -EINVAL;
	ram = sdr_fwdl_parse_image(image, bytes, &parsed);
	if (!ram) {
		req.container = SDR_FWDL_RAM;
	} else {
		patch = sdr_fwdl_parse_patch(image, bytes, &parsed);
		/* -EOPNOTSUPP: not a patch header at all, so the RAM error
		 * describes the file better.
		 */
		if (patch)
			return patch == -EOPNOTSUPP ? ram : patch;
		req.container = SDR_FWDL_PATCH;
	}
	req.regions = parsed.regions;
	req.count = parsed.count;
	req.ram_type = ram_type;
	return sdr_fwdl_run(loader, &req, result);
}

/*
 * Encode every INIT before any I/O and find the entry: the address of the
 * last region with feature bit 5. Sequence numbers are cyclic 1..15 and a
 * nine-region container reuses them; attribution relies on one outstanding
 * command and on draining stale events before each command.
 */
static int sdr_fwdl_plan(const struct sdr_fwdl_region *regions, u32 count,
			 u32 *entry)
{
	u8 packet[MT7996_FWDL_INIT_BYTES];
	u32 i;

	if (!regions || !count || count > SDR_FWDL_MAX_REGIONS)
		return -EINVAL;
	*entry = 0;
	for (i = 0; i < count; i++) {
		const struct sdr_fwdl_region *r = &regions[i];

		if (!r->data || !r->length ||
		    r->length - 1 > ~0UL - (unsigned long)r->data)
			return -EINVAL;
		if (mt7996_fwdl_init(packet, sizeof(packet), 1, r->address,
				     r->length, r->feature_set) < 0)
			return -EINVAL;
		if (r->feature_set & FW_FEATURE_SET_OVERRIDE_ADDR)
			*entry = r->address;
	}
	return 0;
}

static int sdr_fwdl_checkpoint(struct sdr_fwdl *loader)
{
	if (signal_pending(current))
		return -EINTR;
	if (pci_channel_offline(loader->pdev))
		return -EIO;
	if (ktime_get_ns() - loader->began >= loader->budget_ns ||
	    loader->polls++ >= SDR_FWDL_POLL_LIMIT)
		return -ETIMEDOUT;
	return 0;
}

static int sdr_fwdl_wait_download(struct sdr_fwdl *loader)
{
	u32 poll, value;
	int ret;

	loader->progress.phase = SDR_FWDL_WAIT_DOWNLOAD;
	for (poll = 0; poll < SDR_FWDL_STATE_POLLS; poll++) {
		ret = sdr_fwdl_checkpoint(loader);
		if (ret)
			return ret;
		value = loader->io.ops->read32(loader->io.ctx, MT7996_TOP_MISC);
		loader->progress.last_fw_state = value;
		if (value == U32_MAX)
			return -EIO;
		if (HW_GET(TOP_MISC_FW_STATE, value) == TOP_MISC_FW_STATE_FW_DOWNLOAD)
			return 0;
		if (poll + 1U != SDR_FWDL_STATE_POLLS)
			usleep_range(10000, 20000);
	}
	return -ETIMEDOUT;
}

/* One event into loader->event. The WM event ring is also the stream ring:
 * a completion larger than any MCU event is consumed as malformed rather
 * than left to block every later command.
 */
static int sdr_fwdl_rx_read(struct sdr_fwdl *loader)
{
	int ret = sdr_dma_rx_read(loader->rx, loader->event, sizeof(loader->event));

	if (ret == -ENOSPC) {
		ret = sdr_dma_rx_next(loader->rx, NULL, NULL);
		return ret && ret != -EBADMSG ? ret : -EBADMSG;
	}
	return ret;
}

/* At most one event from the WM event ring. With seq 0 every event is
 * discarded; otherwise a reply with that sequence (and event id) sets
 * *matched and returns its status byte.
 */
static int sdr_fwdl_event(struct sdr_fwdl *loader, u32 seq, u32 eid,
			  bool *matched)
{
	int ret = sdr_fwdl_rx_read(loader);

	if (ret == -EAGAIN)
		return ret;
	if (ret == -EBADMSG) {
		loader->progress.malformed_events++;
		return 0;
	}
	if (ret < 0)
		return ret;
	if (!seq) {
		loader->progress.discarded_events++;
		return 0;
	}
	ret = mt7996_fwdl_reply_status(loader->event, ret, seq, eid);
	if (ret >= 0) {
		*matched = true;
		loader->progress.replies++;
		return ret;	/* the device's status byte */
	}
	if (ret == MT7996_FWDL_MALFORMED_REPLY) {
		loader->progress.malformed_events++;
		return 0;
	}
	if (ret != MT7996_FWDL_UNRELATED_REPLY)
		return -EPROTO;
	loader->progress.discarded_events++;
	dev_dbg_ratelimited(&loader->pdev->dev,
			     "reply not matched to seq %u, header %*ph\n",
			     seq, SDR_FWDL_EVENT_HEADER,
			     loader->event + MT7996_MCU_RXD_HW_BYTES);
	return 0;
}

/* Drain the event ring so a stale event cannot answer the next command. */
static int sdr_fwdl_discard_events(struct sdr_fwdl *loader)
{
	bool ignored;
	int ret;

	loader->progress.phase = SDR_FWDL_DISCARD_EVENTS;
	for (;;) {
		ret = sdr_fwdl_checkpoint(loader);
		if (ret)
			return ret;
		ret = sdr_fwdl_event(loader, 0, MT7996_FWDL_EID_ANY, &ignored);
		if (ret == -EAGAIN)
			return 0;
		if (ret)
			return ret;
		usleep_range(1000, 2000);
	}
}

/*
 * Submit one packet and wait for its TX completion and, for a command, its
 * reply. Scatter chunks carry no header but still consume a sequence number.
 */
static int sdr_fwdl_submit(struct sdr_fwdl *loader, struct sdr_dma_ring *tx,
			   const void *packet, u32 bytes, u32 seq, u32 eid,
			   bool command)
{
	bool completed = false, matched = false;
	int ret = sdr_fwdl_checkpoint(loader);

	if (ret)
		return ret;
	loader->progress.last_seq = seq;
	ret = sdr_dma_tx_submit(tx, packet, bytes);
	if (ret)
		return ret;
	loader->progress.tx_submitted++;
	for (;;) {
		ret = sdr_fwdl_checkpoint(loader);
		if (ret)
			return ret;
		if (!completed) {
			ret = sdr_dma_tx_reap(tx);
			if (ret < 0)
				return ret;
			if (ret > 1)
				return -EPROTO;	/* one submission at a time */
			if (ret) {
				completed = true;
				loader->progress.tx_completed++;
			}
		}
		ret = sdr_fwdl_event(loader, command && !matched ? seq : 0, eid,
				     &matched);
		if (ret && ret != -EAGAIN)
			return ret;
		if (completed && (!command || matched))
			return 0;
		usleep_range(1000, 2000);
	}
}

/* One patch command. The semaphore's reply is an outcome (1..3), returned in
 * *status; PATCH_FINISH's nonzero reply is a refusal.
 */
static int sdr_fwdl_patch_command(struct sdr_fwdl *loader,
				  enum sdr_fwdl_phase phase, bool sem,
				  u32 operand, int *status)
{
	u8 packet[MT7996_FWDL_PATCH_BYTES];
	u32 seq;
	int ret, bytes;

	ret = sdr_fwdl_discard_events(loader);
	if (ret)
		return ret;
	loader->progress.phase = phase;
	seq = mt7996_fwdl_next_seq(loader->progress.last_seq);
	bytes = sem ? mt7996_fwdl_patch_sem(packet, sizeof(packet), seq, operand) :
		      mt7996_fwdl_patch_finish(packet, sizeof(packet), seq, operand);
	if (bytes < 0)
		return -EINVAL;
	ret = sdr_fwdl_submit(loader, loader->wm, packet, (u32)bytes, seq,
			      sem ? MT7996_FWDL_EID_PATCH_SEM :
				    MT7996_FWDL_EID_ANY, true);
	if (ret < 0)
		return ret;
	if (status) {
		*status = ret;
		return 0;
	}
	return ret;
}

static u32 sdr_fwdl_working_pda(u32 ram_type)
{
	if (ram_type == SDR_FWDL_RAM_WA)
		return MT7996_FWDL_START_PDA_CR4;
	if (ram_type == SDR_FWDL_RAM_DSP)
		return MT7996_FWDL_START_PDA_DSP;
	return 0;
}

/* Reset the per-call record; the sequence continues across the session. */
static void sdr_fwdl_begin(struct sdr_fwdl *loader)
{
	u32 last_seq = loader->progress.last_seq;

	memset(&loader->progress, 0, sizeof(loader->progress));
	loader->progress.last_seq = last_seq;
	loader->progress.sem_get_status = -1;
	loader->progress.sem_release_status = -1;
}

/* Start the deadline once the request is admitted, so a refused request
 * neither reads the clock nor uses any of the budget.
 */
static void sdr_fwdl_start_clock(struct sdr_fwdl *loader, u64 budget_ns)
{
	loader->began = ktime_get_ns();
	loader->polls = 0;
	loader->budget_ns = budget_ns;
}

/* Download each region: INIT on the WM ring, then raw chunks on the FWDL ring. */
static int sdr_fwdl_regions(struct sdr_fwdl *loader,
			    const struct sdr_fwdl_region *regions, u32 count)
{
	u8 packet[MT7996_FWDL_INIT_BYTES];
	u32 i, offset, seq;
	int ret, bytes;

	for (i = 0; i < count; i++) {
		loader->progress.region = i;
		loader->progress.offset = 0;
		ret = sdr_fwdl_discard_events(loader);
		if (ret)
			return ret;
		loader->progress.phase = SDR_FWDL_INIT;
		seq = mt7996_fwdl_next_seq(loader->progress.last_seq);
		bytes = mt7996_fwdl_init(packet, sizeof(packet), seq,
					 regions[i].address, regions[i].length,
					 regions[i].feature_set);
		if (bytes < 0)
			return -EINVAL;
		ret = sdr_fwdl_submit(loader, loader->wm, packet, bytes, seq,
				      MT7996_FWDL_EID_GENERIC, true);
		if (ret)
			return ret;
		loader->progress.phase = SDR_FWDL_SCATTER;
		for (offset = 0; offset < regions[i].length; offset += bytes) {
			loader->progress.offset = offset;
			bytes = mt7996_fwdl_scatter_length(regions[i].length, offset);
			if (bytes <= 0)
				return -EINVAL;
			seq = mt7996_fwdl_next_seq(loader->progress.last_seq);
			ret = sdr_fwdl_submit(loader, loader->tx,
					      (const u8 *)regions[i].data + offset,
					      bytes, seq, MT7996_FWDL_EID_ANY, false);
			if (ret)
				return ret;
		}
		loader->progress.offset = offset;
	}
	loader->progress.region = count;
	return 0;
}

/*
 * Patch schedule: take the semaphore, download, PATCH_FINISH, and release the
 * semaphore on every path once taken. A get status of IS_DL means the patch
 * is already present: the regions are skipped, as mt76 does.
 */
static int sdr_fwdl_patch(struct sdr_fwdl *loader, const struct sdr_fwdl_request *req)
{
	struct sdr_fwdl_result *p = &loader->progress;
	int ret, rel;

	ret = sdr_fwdl_patch_command(loader, SDR_FWDL_PATCH_SEM_GET, true,
				     MT7996_FWDL_SEM_GET, &p->sem_get_status);
	if (ret)
		return ret;
	if (p->sem_get_status == MT7996_FWDL_SEM_NOT_DL_SUCCESS) {
		ret = sdr_fwdl_regions(loader, req->regions, req->count);
		if (!ret)
			ret = sdr_fwdl_patch_command(loader, SDR_FWDL_PATCH_FINISH,
						     false, 0, NULL);
	} else if (p->sem_get_status != MT7996_FWDL_SEM_IS_DL) {
		ret = -EBUSY;	/* semaphore refused */
	}
	rel = sdr_fwdl_patch_command(loader, SDR_FWDL_PATCH_SEM_RELEASE, true,
				     MT7996_FWDL_SEM_RELEASE, &p->sem_release_status);
	if (!ret)
		ret = rel;
	if (!ret)
		p->phase = SDR_FWDL_COMPLETE;
	return ret;
}

/* RAM schedule: download, then FW_START_REQ for this RAM type. */
static int sdr_fwdl_ram(struct sdr_fwdl *loader, const struct sdr_fwdl_request *req,
			u32 entry)
{
	u8 packet[MT7996_FWDL_START_BYTES];
	u32 seq;
	int ret, bytes;

	ret = sdr_fwdl_regions(loader, req->regions, req->count);
	if (!ret)
		ret = sdr_fwdl_discard_events(loader);
	if (ret)
		return ret;
	loader->progress.phase = SDR_FWDL_START;
	seq = mt7996_fwdl_next_seq(loader->progress.last_seq);
	bytes = mt7996_fwdl_start(packet, sizeof(packet), seq, entry,
				  sdr_fwdl_working_pda(req->ram_type));
	if (bytes < 0)
		return -EINVAL;
	ret = sdr_fwdl_submit(loader, loader->wm, packet, bytes, seq,
			      MT7996_FWDL_EID_GENERIC, true);
	if (!ret) {
		loader->progress.phase = SDR_FWDL_COMPLETE;
		loader->started |= 1U << req->ram_type;
	}
	return ret;
}

int sdr_fwdl_run(struct sdr_fwdl *loader, const struct sdr_fwdl_request *req,
		 struct sdr_fwdl_result *result)
{
	bool patch;
	u32 entry;
	int ret;

	if (!loader || !result || !req)
		return -EINVAL;
	if ((req->container != SDR_FWDL_RAM && req->container != SDR_FWDL_PATCH) ||
	    req->ram_type > SDR_FWDL_RAM_DSP)
		return -EINVAL;
	patch = req->container == SDR_FWDL_PATCH;
	if (!mutex_trylock(&loader->lock))
		return -EBUSY;
	if (loader->spent || (!patch && (loader->started & (1U << req->ram_type)))) {
		ret = -EALREADY;
		goto out;
	}
	sdr_fwdl_begin(loader);
	loader->progress.container = req->container;
	ret = sdr_fwdl_plan(req->regions, req->count, &entry);
	if (!ret)
		ret = sdr_fwdl_rings(loader);
	if (ret) {
		/* Nothing was sent: the session stays usable. */
		loader->progress.result = ret;
		goto out;
	}
	sdr_fwdl_start_clock(loader, SDR_FWDL_TIMEOUT_NS);
	ret = sdr_fwdl_wait_download(loader);
	if (!ret)
		ret = patch ? sdr_fwdl_patch(loader, req) :
			      sdr_fwdl_ram(loader, req, entry);
	loader->progress.result = ret;
	if (ret)
		loader->spent = true;
out:
	*result = loader->progress;
	mutex_unlock(&loader->lock);
	return ret;
}

int sdr_fwdl_stream_begin(struct sdr_fwdl *loader, struct sdr_dma_ring **rx)
{
	int ret;

	if (!loader || !rx)
		return -EINVAL;
	if (!mutex_trylock(&loader->lock))
		return -EBUSY;
	ret = sdr_fwdl_rings(loader);
	if (ret) {
		mutex_unlock(&loader->lock);
		return ret;
	}
	*rx = loader->rx;
	return 0;
}

void sdr_fwdl_stream_end(struct sdr_fwdl *loader)
{
	mutex_unlock(&loader->lock);
}
