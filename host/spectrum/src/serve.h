// SPDX-License-Identifier: MIT
// w1700k-spectrum's options.
#pragma once

#include <string>

struct ServeOptions {
    std::string listen = "127.0.0.1:8073";    // HTTP address:port
    std::string web_dir;                      // serve the UI from here instead of the built-in copy
    std::string bind = "10.77.0.10";          // the stream's destination: this host on the 10G link
    unsigned port = 7700;                     // the export's destination port
    unsigned rx_threads = 8;                  // UDP sockets and threads
    unsigned fft_threads = 4;                 // FFT workers
    unsigned ring_slices = 1u << 16;          // 8 KiB slices in the sample ring (0.42 s at 320 Msps)
    std::string controller = "w1700k serve";  // the receiver's controller; empty: a passive viewer
    // The stream's tuning until the controller reports it, and always for a passive viewer.
    double center_hz = 5250e6;
    double sample_rate = 320e6;
    // SIGUSR1 writes the newest dump_seconds of samples (raw stream words) and a .json beside them.
    double dump_seconds = 0.3;
    std::string dump_dir = "/tmp";
};

int command_serve(const ServeOptions &options);
