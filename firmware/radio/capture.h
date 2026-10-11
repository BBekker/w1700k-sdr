/* SPDX-License-Identifier: MIT */
/* The capture path set-up that MediaTek's SPECTRUM prime leaves on a band (hw/mt7996/capture_setup.h),
 * done by our WM itself, and the ICAP source select.
 *
 * capture_setup() programs one band's capture path as MediaTek's SPECTRUM capture arm (node
 * MT7996_DFE_NODE_STOCK, u4BW by bandwidth, band index = PHY index = band) leaves it after the
 * prime's retire: the band's capture clocks, DFE capture mux and lane mux, and the capture trigger
 * block. It leaves the ICAP engines, the route block and the RX gain alone (capture.c). Run it per
 * band after that band's channel set; bands keep their set-up independently, as three sequential
 * primes do.
 *
 * capture_select() points ICAP0..2 at a band (ICAPCR8), as the arm does for every instance; the
 * stream then captures that band. Mid-stream switches are the stream's own (stream/ctl.h).
 */
#ifndef RADIO_CAPTURE_H
#define RADIO_CAPTURE_H

#include <stdint.h>

#include "radio.h"
#include "radio_cmd.h"

/* bw: 0..3 = 20 / 40 / 80 / 160 MHz (chanset.h), the band's channel bandwidth.
 * RADIO_OK; RADIO_ERR_ARG (band or bw out of range, nothing done); RADIO_ERR_HW (an access failed:
 * the failure record). */
uint32_t capture_setup(uint32_t band, uint32_t bw);

/* band 0..2; out of range: HAL_FAIL_ARG recorded, nothing done. */
void capture_select(uint32_t band);

#endif
