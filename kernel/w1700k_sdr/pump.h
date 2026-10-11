/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef W1700K_SDR_PUMP_H
#define W1700K_SDR_PUMP_H
#include "w1700k_sdr.h"

#define SDR_PUMP_QUIET_NS 300000000ULL		/* 300 ms */
#define SDR_PUMP_DRAIN_NS 5000000000ULL		/* 5 s */

/* What the pump needs from the device; all called with the device held. */
struct sdr_pump_ops {
	/* Consume one completion: 0, -EBADMSG (malformed, still consumed),
	 * -EAGAIN (none) or a ring error.
	 */
	int (*next)(void *ctx, u32 *ctrl, u64 *seq);
	/* Write the producer's stop word. */
	int (*stop)(void *ctx, u32 address, u32 value);
	/* Serve CTL requests; `idle` after an empty sweep (may yield the CPU). */
	void (*service)(void *ctx, bool idle);
	/* A signal is pending or the device is going away; *fatal for a fatal
	 * signal.
	 */
	bool (*interrupted)(void *ctx, bool *fatal);
	u64 (*now)(void *ctx);
};

/*
 * Pump the stream ring until the stream ends (timeout_ms, idle_ms without a
 * completion once one has arrived, an interruption, or a ring error), filling
 * the output fields of `req`. When it ends and stop_address is set, the stop
 * word is written and the ring kept armed until max(idle_ms, 300 ms) pass
 * without a completion, at most 5 s; after a ring error the stop word is
 * written without draining. Returns 0, -EINTR for a fatal signal, or the
 * ring or stop-write error.
 */
int sdr_pump_run(const struct sdr_pump_ops *ops, void *ctx,
		 struct w1700k_sdr_stream_rx *req);
#endif
