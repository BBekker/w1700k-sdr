/* SPDX-License-Identifier: MIT */
/* The ROM patch's radio init (MediaTek MCU_Patch_init), run by our WM first at bring-up. */
#ifndef RADIO_PATCH_INIT_H
#define RADIO_PATCH_INIT_H

#include <stdint.h>

#include "radio_cmd.h"

/* RADIO_OK, or RADIO_ERR_HW (an unsupported A-die combination or chip, a failed access). */
uint32_t patch_init(void);

#endif
