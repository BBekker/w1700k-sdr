/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef W1700K_SDR_WFDMA_H
#define W1700K_SDR_WFDMA_H
#include "mmio.h"

#define SDR_WFDMA_IDLE_POLLS 101U

enum sdr_wfdma_state {
	SDR_WFDMA_NEW, SDR_WFDMA_RESET, SDR_WFDMA_PREPARED,
	SDR_WFDMA_ENABLED, SDR_WFDMA_STOPPED, SDR_WFDMA_FAILED,
};

/*
 * One engine session: NEW -> RESET -> PREPARED -> ENABLED, each step attempted
 * once; any failure latches FAILED with first_error. The caller serializes
 * calls, holds the device lock, and binds the rings between reset and
 * prepare. No call enables bus mastering. wait_poll is a bounded 10 us wait.
 */
struct sdr_wfdma {
	const struct sdr_mmio *io;
	int (*wait_poll)(void *ctx);
	enum sdr_wfdma_state state;
	int first_error;
	unsigned int stop_attempted;
};

int sdr_wfdma_init(struct sdr_wfdma *dma, const struct sdr_mmio *io,
		   int (*wait_poll)(void *ctx));
/* Mask host interrupts, pulse the logic/scheduler reset, clear the enables. */
int sdr_wfdma_reset(struct sdr_wfdma *dma);
/* Prefetch windows, CALC_MODE, FIFO busy sources, wait for HIF idle,
 * RX write-back and flow-control modes, RX pause thresholds.
 */
int sdr_wfdma_prepare(struct sdr_wfdma *dma);
/* Set TX/RX DMA enable and the omit-info bits. */
int sdr_wfdma_enable(struct sdr_wfdma *dma);
/*
 * Clear the enables and wait (at most SDR_WFDMA_IDLE_POLLS reads) for
 * HIF_MISC busy to clear. One attempt per session, allowed after any reset
 * attempt, including a failed one; a prior failure is kept.
 */
int sdr_wfdma_stop_observe(struct sdr_wfdma *dma);
#endif
