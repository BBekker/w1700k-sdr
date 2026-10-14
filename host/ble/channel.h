/* SPDX-License-Identifier: MIT */
#ifndef BLE_CHANNEL_H
#define BLE_CHANNEL_H
#include <stdint.h>
/* 80 Msps -> translated 8-point average -> CIC3 /5 -> 2 Msps.
 * Both decimation stages filter; NCO phase and CIC state persist across blocks. */
struct ble_channel {
    int16_t re[160], im[160];
    uint32_t integrator[2][3], comb[2][3];
    unsigned phase, cic_phase;
};
void ble_channel_init(struct ble_channel *s, unsigned channel);
unsigned ble_channel_run(struct ble_channel *s, const uint32_t *raw, unsigned n, int16_t *out);
#endif
