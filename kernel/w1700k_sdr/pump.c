// SPDX-License-Identifier: GPL-2.0-only
/*
 * The stream pump. A producer that is still publishing into a ring nobody
 * re-arms stalls with descriptors outstanding on its own TX ring, which
 * wedges the WM until the next firmware load. So however the stream ends,
 * the producer is told to stop and the ring is kept armed while it drains.
 */
#ifdef __KERNEL__
#include <linux/errno.h>
#else
#include <errno.h>
#endif
#include "pump.h"
#include "hw/mt7996/wfdma.h"

#define SDR_PUMP_SERVICE_EVERY 1024U

static bool sdr_pump_should_end(const struct sdr_pump_ops *ops, void *ctx,
				const struct w1700k_sdr_stream_rx *req, u64 now,
				u64 deadline, u64 idle, bool *fatal)
{
	return ops->interrupted(ctx, fatal) || now >= deadline ||
	       (idle && req->buffers && now - req->last_ns >= idle);
}

/* Account one completion. */
static void sdr_pump_count(struct w1700k_sdr_stream_rx *req, u64 now, u32 ctrl,
			   u64 seq, bool malformed)
{
	u32 len = HW_GET(WFDMA_DESC_CTRL_SD_LEN0, ctrl);

	if (!req->buffers) {
		req->first_ns = now;
		req->first_seq = seq;
	}
	req->last_ns = now;
	req->malformed += malformed;
	req->bytes += len;
	req->full_buffers += len >= req->slice_bytes / 2;
	req->buffers++;
}

int sdr_pump_run(const struct sdr_pump_ops *ops, void *ctx,
		 struct w1700k_sdr_stream_rx *req)
{
	u64 deadline, idle, quiet, now, seq = 0, last, drain_by = 0;
	u32 ctrl = 0, sweep = 0;
	bool fatal = false;
	int ret = 0, r;

	req->start_ns = ops->now(ctx);
	deadline = req->start_ns + (u64)req->timeout_ms * 1000000ULL;
	idle = (u64)req->idle_ms * 1000000ULL;
	quiet = idle > SDR_PUMP_QUIET_NS ? idle : SDR_PUMP_QUIET_NS;
	for (;;) {
		r = ops->next(ctx, &ctrl, &seq);
		if (r && r != -EBADMSG && r != -EAGAIN) {
			ret = r;
			if (req->stop_address && !req->stop_ns &&
			    !ops->stop(ctx, req->stop_address, req->stop_value))
				req->stop_ns = ops->now(ctx);
			break;
		}
		now = ops->now(ctx);
		if (r != -EAGAIN) {
			sdr_pump_count(req, now, ctrl, seq, r == -EBADMSG);
			if (!(++sweep % SDR_PUMP_SERVICE_EVERY))
				ops->service(ctx, false);
			continue;
		}
		if (sweep > req->max_backlog)
			req->max_backlog = sweep;
		sweep = 0;
		if (req->stop_ns) {
			last = req->last_ns > req->stop_ns ? req->last_ns : req->stop_ns;
			if (now >= drain_by || now - last >= quiet)
				break;
		} else if (sdr_pump_should_end(ops, ctx, req, now, deadline, idle, &fatal)) {
			if (fatal)
				ret = -EINTR;
			if (!req->stop_address)
				break;
			r = ops->stop(ctx, req->stop_address, req->stop_value);
			if (r) {
				ret = r;
				break;
			}
			req->stop_ns = now;
			drain_by = now + SDR_PUMP_DRAIN_NS;
		}
		ops->service(ctx, true);
	}
	return ret;
}
