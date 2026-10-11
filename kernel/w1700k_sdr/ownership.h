/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef W1700K_SDR_OWNERSHIP_H
#define W1700K_SDR_OWNERSHIP_H
#include "mmio.h"
#include "hw/mt7996/host.h"

/* Band-0 host/firmware ownership handshake (mt76 mt7996_driver_own). */
#define SDR_HOST_LPCR MT7996_TOP_LPCR_HOST_BAND(0)
#define SDR_HOST_IRQ_STAT MT7996_TOP_LPCR_HOST_BAND_IRQ_STAT(0)
#define SDR_HOST_DRV_OWN TOP_LPCR_HOST_DRV_OWN
#define SDR_HOST_FW_OWN_STAT TOP_LPCR_HOST_FW_OWN_STAT
#define SDR_HOST_OWN_ACK TOP_LPCR_HOST_BAND_STAT
#define SDR_HOST_OWN_POLLS 51U

/*
 * Request ownership: write DRV_OWN, poll until FW_OWN_STAT clears (at most
 * SDR_HOST_OWN_POLLS reads, wait_poll between them), then acknowledge the
 * ownership interrupt. The caller holds the device lock and has checked that
 * the device is live; this does not call io->ops->begin. All-ones reads give
 * -EIO, exhaustion -ETIMEDOUT, and a wait_poll error is returned as is.
 */
int sdr_request_host_owned(const struct sdr_mmio *io,
			   int (*wait_poll)(void *ctx));
#endif
