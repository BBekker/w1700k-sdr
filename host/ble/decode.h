/* SPDX-License-Identifier: MIT */
#ifndef BLE_DECODE_H
#define BLE_DECODE_H
#include <stdint.h>
#define BLE_MAX_PDU 257
#define BLE_MAX_EVENTS 12
struct ble_packet {
    uint32_t sample;
    uint16_t length;
    uint8_t channel, reserved;
    uint8_t pdu[BLE_MAX_PDU + 3];
};
struct ble_phase {
    uint32_t shift, crc, start;
    uint16_t bits, target;
    uint8_t whitening, active;
    uint8_t bytes[BLE_MAX_PDU + 3];
};
struct ble_decoder {
    struct ble_phase phase[2];
    uint32_t samples, crc_fail, valid, last_end, last_crc;
    int32_t prev_i, prev_q;
    uint32_t channel;
};
struct ble_output {
    uint32_t count, dropped, samples, crc_fail, valid;
    struct ble_packet packets[BLE_MAX_EVENTS];
};
void ble_reset(struct ble_decoder *s, unsigned channel);
void ble_decode(struct ble_decoder *s, const int16_t *iq, unsigned n, struct ble_output *out);
void ble_decode8(struct ble_decoder *s, const int8_t *iq, unsigned n, struct ble_output *out);
#endif
