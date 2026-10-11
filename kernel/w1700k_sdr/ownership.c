// SPDX-License-Identifier: GPL-2.0-only
#ifdef __KERNEL__
#include <linux/errno.h>
#else
#include <errno.h>
#endif
#include "ownership.h"

int sdr_request_host_owned(const struct sdr_mmio *io,
			   int (*wait_poll)(void *ctx))
{
	u32 poll, status;
	int ret;

	if (!io || !io->ops || !io->ops->read32 || !io->ops->write32 ||
	    !wait_poll || io->bar_size < SDR_HOST_IRQ_STAT + 4U)
		return -EINVAL;

	io->ops->write32(io->ctx, SDR_HOST_LPCR, SDR_HOST_DRV_OWN);
	for (poll = 0; poll < SDR_HOST_OWN_POLLS; poll++) {
		status = io->ops->read32(io->ctx, SDR_HOST_LPCR);
		if (status == (u32)~0U)
			return -EIO;
		if (!(status & SDR_HOST_FW_OWN_STAT)) {
			io->ops->write32(io->ctx, SDR_HOST_IRQ_STAT, SDR_HOST_OWN_ACK);
			return 0;
		}
		if (poll + 1U < SDR_HOST_OWN_POLLS) {
			ret = wait_poll(io->ctx);
			if (ret)
				return ret;
		}
	}
	return -ETIMEDOUT;
}
