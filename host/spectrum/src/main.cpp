// SPDX-License-Identifier: MIT
// w1700k-spectrum: a live spectrum and waterfall of the W1700K's exported stream, in a browser,
// with the receiver's controls.
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "serve.h"

namespace {

void usage()
{
    std::fprintf(stderr,
                 "usage: w1700k-spectrum [options]\n"
                 "  --listen ADDR:PORT  the web page (default 127.0.0.1:8073)\n"
                 "  --web DIR           serve the page from DIR instead of the built-in copy\n"
                 "  --bind IP           the stream's destination address (default 10.77.0.10)\n"
                 "  --port N            its UDP port (default 7700)\n"
                 "  --rx-threads N      UDP sockets and threads (default 8)\n"
                 "  --fft-threads N     FFT workers (default 4)\n"
                 "  --ring-mib N        sample ring, a power of two (default 512)\n"
                 "  --controller CMD    the receiver's controller, run with sh -c (default \"w1700k serve\")\n"
                 "  --no-controller     a passive viewer: no receiver controls\n"
                 "  --center MHZ        the stream's centre frequency until the controller reports it (default 5250)\n"
                 "  --rate MSPS         its sample rate until the controller reports it (default 320)\n"
                 "  --dump-dir DIR      where SIGUSR1 writes a snapshot of the newest samples (default /tmp)\n"
                 "  --dump-seconds S    its length, at most the ring's (default 0.3)\n");
}

unsigned positive(const std::string &option, const std::string &text)
{
    size_t used = 0;
    unsigned long v = 0;
    try {
        v = std::stoul(text, &used);
    } catch (const std::logic_error &) {
        used = 0;
    }
    if (!used || used != text.size() || v == 0 || v > 1u << 20)
        throw std::runtime_error(option + " needs a positive integer, not \"" + text + "\"");
    return unsigned(v);
}

double number(const std::string &option, const std::string &text)
{
    size_t used = 0;
    double v = 0;
    try {
        v = std::stod(text, &used);
    } catch (const std::logic_error &) {
        used = 0;
    }
    if (!used || used != text.size() || !(v > 0))
        throw std::runtime_error(option + " needs a positive number, not \"" + text + "\"");
    return v;
}

}  // namespace

int main(int argc, char **argv)
{
    ServeOptions o;
    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto value = [&]() -> std::string {
                if (i + 1 >= argc) throw std::runtime_error(arg + " needs a value");
                return argv[++i];
            };
            if (arg == "--listen") o.listen = value();
            else if (arg == "--web") o.web_dir = value();
            else if (arg == "--bind") o.bind = value();
            else if (arg == "--port") o.port = positive(arg, value());
            else if (arg == "--rx-threads") o.rx_threads = positive(arg, value());
            else if (arg == "--fft-threads") o.fft_threads = positive(arg, value());
            else if (arg == "--ring-mib") o.ring_slices = positive(arg, value()) * 128;
            else if (arg == "--controller") o.controller = value();
            else if (arg == "--no-controller") o.controller.clear();
            else if (arg == "--center") o.center_hz = number(arg, value()) * 1e6;
            else if (arg == "--rate") o.sample_rate = number(arg, value()) * 1e6;
            else if (arg == "--dump-dir") o.dump_dir = value();
            else if (arg == "--dump-seconds") o.dump_seconds = number(arg, value());
            else if (arg == "-h" || arg == "--help") {
                usage();
                return 0;
            } else {
                throw std::runtime_error("unknown option " + arg);
            }
        }
        if (o.port > 65535) throw std::runtime_error("--port must be at most 65535");
        if (o.ring_slices & (o.ring_slices - 1)) throw std::runtime_error("--ring-mib must be a power of two");
        if (o.controller.find_first_not_of(" \t") == std::string::npos) o.controller.clear();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "w1700k-spectrum: %s\n", e.what());
        usage();
        return 2;
    }
    try {
        return command_serve(o);
    } catch (const std::exception &e) {
        std::fprintf(stderr, "w1700k-spectrum: %s\n", e.what());
        return 1;
    }
}
