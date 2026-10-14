/* SPDX-License-Identifier: MIT */
/* CPU cost of the exact decoder used by the DSP. No radio access.
 * One published channel-38 packet per 2048-sample block (~977 packets/s).
 * CLOCK_THREAD_CPUTIME_ID excludes scheduling delays; wall time is also shown. */
#define _POSIX_C_SOURCE 200809L
#include "../../host/ble/decode.h"
#include <math.h>
#include <stdio.h>
#include <time.h>
static double now(clockid_t clock)
{
    struct timespec t;
    clock_gettime(clock, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
int main(void)
{
    /* Core 5.4, Vol 6 Part C 4.2.1, bits in transmission order. */
    const char *bits = "0101010101101011011111011001000101110001001010010011001101000111"
                       "1010000110111111101111101100001001110010010110001110010100110101"
                       "111101111111001110100101";
    int16_t iq[4096];
    int8_t packed[4096];
    double phase = 0;
    for (unsigned k = 0; k < 2048; k++) {
        unsigned symbol = k / 2;
        phase += symbol < 152 && bits[symbol] == '1' ? 0.7853981633974483 : -0.7853981633974483;
        iq[2 * k] = (int16_t)lround(1000 * cos(phase));
        iq[2 * k + 1] = (int16_t)lround(1000 * sin(phase));
        packed[2 * k] = iq[2 * k] >> 3;
        packed[2 * k + 1] = iq[2 * k + 1] >> 3;
    }
    for (unsigned mode = 0; mode < 2; mode++) {
        struct ble_decoder state;
        struct ble_output out;
        ble_reset(&state, 38);
        double wall = now(CLOCK_MONOTONIC), cpu = now(CLOCK_THREAD_CPUTIME_ID);
        unsigned packets = 0, repeats = 10000;
        for (unsigned n = 0; n < repeats; n++) {
            if (mode) ble_decode8(&state, packed, 2048, &out);
            else ble_decode(&state, iq, 2048, &out);
            packets += out.count;
        }
        cpu = now(CLOCK_THREAD_CPUTIME_ID) - cpu;
        wall = now(CLOCK_MONOTONIC) - wall;
        printf("{\"bits\":%u,\"blocks\":%u,\"packets\":%u,\"cpu_us_per_block\":%.3f,"
               "\"wall_us_per_block\":%.3f,\"core_fraction_at_2Msps\":%.5f}\n",
               mode ? 8 : 16, repeats, packets, cpu * 1e6 / repeats,
               wall * 1e6 / repeats, cpu / (repeats * 0.001024));
        if (packets != repeats || out.dropped || out.crc_fail) return 1;
    }
    return 0;
}
