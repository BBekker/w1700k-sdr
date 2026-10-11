// SPDX-License-Identifier: GPL-2.0-only
/*
 * Host-only shim: exposes sizeof(struct airoha_bmt) so the ctypes test can
 * assert its mirror of the struct has not drifted from the shipped C. Never
 * compiled into the loader or the kernel.
 */
#include <stddef.h>
#include "airoha_bmt.h"

size_t airoha_bmt_sizeof(void)
{
	return sizeof(struct airoha_bmt);
}
