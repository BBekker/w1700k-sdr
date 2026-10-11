/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Firmware download (ROM patch and RAM containers) over the DMA rings, and
 * the hand-over of the WM event ring to the stream pump. One command is
 * outstanding at a time; completions and replies are polled.
 */
#ifndef W1700K_SDR_FWDL_H
#define W1700K_SDR_FWDL_H

#include <linux/types.h>
#include "w1700k_sdr.h"

struct pci_dev;
struct sdr_dma_ring;
struct sdr_fwdl;
struct sdr_mmio;

#define SDR_FWDL_MAX_REGIONS 14U

struct sdr_fwdl_region {
	const void *data;
	u32 address, length, feature_set;
};

/* Reported as `phase` by DOWNLOAD (W1700K_SDR_PHASE_*). */
enum sdr_fwdl_phase {
	SDR_FWDL_PREFLIGHT = W1700K_SDR_PHASE_PREFLIGHT,
	SDR_FWDL_DISCARD_EVENTS = W1700K_SDR_PHASE_DISCARD_EVENTS,
	SDR_FWDL_INIT = W1700K_SDR_PHASE_INIT,
	SDR_FWDL_SCATTER = W1700K_SDR_PHASE_SCATTER,
	SDR_FWDL_START = W1700K_SDR_PHASE_START,
	SDR_FWDL_COMPLETE = W1700K_SDR_PHASE_COMPLETE,
	SDR_FWDL_WAIT_DOWNLOAD = W1700K_SDR_PHASE_WAIT_DOWNLOAD,
	SDR_FWDL_PATCH_SEM_GET = W1700K_SDR_PHASE_PATCH_SEM_GET,
	SDR_FWDL_PATCH_FINISH = W1700K_SDR_PHASE_PATCH_FINISH,
	SDR_FWDL_PATCH_SEM_RELEASE = W1700K_SDR_PHASE_PATCH_SEM_RELEASE,
};

/* The container selects the schedule: a patch is bracketed by
 * PATCH_SEM_CONTROL and PATCH_FINISH_REQ and never started; a RAM container
 * ends with FW_START_REQ.
 */
enum sdr_fwdl_container {
	SDR_FWDL_RAM = W1700K_SDR_CONTAINER_RAM,
	SDR_FWDL_PATCH = W1700K_SDR_CONTAINER_PATCH,
};

/* Selects FW_START_REQ's working-PDA bit. */
enum sdr_fwdl_ram_type {
	SDR_FWDL_RAM_WM = W1700K_SDR_RAM_WM,
	SDR_FWDL_RAM_WA = W1700K_SDR_RAM_WA,
	SDR_FWDL_RAM_DSP = W1700K_SDR_RAM_DSP,
};

struct sdr_fwdl_request {
	const struct sdr_fwdl_region *regions;
	u32 count, container, ram_type;
};

/* Per call, except last_seq, which continues across the session. */
struct sdr_fwdl_result {
	enum sdr_fwdl_phase phase;
	u32 container;
	u32 region, offset, last_seq;
	u32 tx_submitted, tx_completed, replies;
	u32 discarded_events, malformed_events;
	u32 last_fw_state;
	int result;
	int sem_get_status, sem_release_status;	/* -1 when not sent */
};

/*
 * A session over bound rings: FWDL TX (scatter chunks), WM TX (download
 * commands) and WM RX (replies, and the stream). io must stay valid for the
 * session; the executor reads FW_STATE with io->ops->read32 while the caller
 * holds the device lock.
 */
int sdr_fwdl_alloc(struct pci_dev *pdev, const struct sdr_mmio *io,
		   struct sdr_dma_ring *fwdl,
		   struct sdr_dma_ring *wm, struct sdr_dma_ring *rx,
		   struct sdr_fwdl **out);
/* Frees the session only; the rings stay with the caller. */
void sdr_fwdl_free(struct sdr_fwdl *loader);

/*
 * Download one container's regions. Waits for FW_STATE == FW_DOWNLOAD, then
 * per region INIT on the WM ring and raw scatter chunks on the FWDL ring; a
 * patch is wrapped in the semaphore and PATCH_FINISH, a RAM container ends
 * with FW_START_REQ carrying the working-PDA bit of its RAM type and the
 * entry of the last region with feature bit 5. Each RAM type is started once
 * per session (-EALREADY after); a failure after anything was sent latches
 * the session (-EALREADY) until it is freed.
 *
 * Returns 0, a positive device status byte or a negative errno; *result
 * records how far it got.
 */
int sdr_fwdl_run(struct sdr_fwdl *loader, const struct sdr_fwdl_request *req,
		 struct sdr_fwdl_result *result);

/* Detect the container (RAM first, its CRC is conclusive; then the patch),
 * parse it and run it. A parse failure performs no I/O and leaves *result.
 */
int sdr_fwdl_run_image(struct sdr_fwdl *loader, const void *image, size_t bytes,
		       u32 ram_type, struct sdr_fwdl_result *result);

/* Exclusive use of the WM event ring for streaming; -EBUSY while a download
 * runs. Pump it with sdr_dma_rx_next, then call stream_end.
 */
int sdr_fwdl_stream_begin(struct sdr_fwdl *loader, struct sdr_dma_ring **rx);
void sdr_fwdl_stream_end(struct sdr_fwdl *loader);

#endif
