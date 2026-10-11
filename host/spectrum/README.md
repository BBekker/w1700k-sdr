# w1700k-spectrum

A live spectrum and waterfall of the W1700K's sample stream in a web browser, with the
receiver's controls: run/stop, LO, band and sample rate, analog filter and gain. One process
receives the unit's UDP export, computes the spectrum (FFTW,
several threads) and serves the page; receiver settings go to a controller process, by default
`w1700k serve` from the host package.

## Build

Needs a C++17 compiler, zlib and FFTW3 in single precision (Debian/Ubuntu:
`apt install g++ make zlib1g-dev libfftw3-dev`).

```sh
make                                  # build/w1700k-spectrum, the page compiled in
make CXXFLAGS="-O3 -march=native"     # tuned for this CPU
make install PREFIX=/usr/local
```

The default flags are portable (`-O3`). Warnings are errors; `make WARNINGS=-Wall` relaxes that.

## Run

```sh
build/w1700k-spectrum                 # then open http://127.0.0.1:8073/
```

The host needs the stream's destination address (default 10.77.0.10) on the 10G link, an MTU
of at least 8236 (one 8 KiB slice per datagram; 9000 is usual), and room for 48 MiB socket
buffers: `sysctl -w net.core.rmem_max=67108864`, or the kernel caps them silently and drops
datagrams under load (the page counts them as socket drops). `w1700k` must be on `PATH`, or
name the controller with `--controller`.

| option | default | |
|---|---|---|
| `--listen ADDR:PORT` | `127.0.0.1:8073` | the page; `PORT` alone listens on every address |
| `--web DIR` | built in | serve the page from DIR, read on every request (development) |
| `--bind IP` | `10.77.0.10` | the stream's destination address |
| `--port N` | `7700` | its UDP port |
| `--rx-threads N` | 8 | UDP sockets and receive threads (`SO_REUSEPORT`) |
| `--fft-threads N` | 4 | FFT workers |
| `--ring-mib N` | 512 | sample ring, a power of two (512 MiB holds 0.42 s at 320 Msps) |
| `--controller CMD` | `w1700k serve` | the receiver's controller, run with `sh -c` |
| `--no-controller` | | a passive viewer: no receiver controls |
| `--center MHZ` | 5250 | the stream's centre frequency until the controller reports it |
| `--rate MSPS` | 320 | its sample rate until the controller reports it |
| `--dump-dir DIR` | `/tmp` | where SIGUSR1 writes a snapshot |
| `--dump-seconds S` | 0.3 | the snapshot's length |

One browser is served at a time: opening the page elsewhere takes over (the previous one is
closed with code 4001 and offers to take it back).

`kill -USR1` writes the newest samples to `DIR/w1700k-dump-<unix ms>.bin` (u32 words in sample
order, I in bits 29:16 and Q in 13:0, 14-bit two's complement; packed slices unpacked) and a
`.json` with `center_hz`, `sample_rate`, `tuning_steady` (false if the tuning changed within
the snapshot), the slices covered, and the slices missing (written as zeros) or holding
firmware events (left out).

## The controller

The controller is a child process speaking newline-delimited JSON on its stdin and stdout:
requests `{"id":N,"op":...}` each get one reply `{"id":N,"ok":true|false,...}`; lines without
an `id` are events (`state`, `progress`, `log`). The server asks for `status` first, then sends
one request at a time. A controller that exits, closes its output, or does not answer in time
(configure 600 s, start/stop 120 s, others 30 s) is stopped and restarted after 1, 2, 5, 10,
then 30 s; the page shows it as down meanwhile, and requests that were queued are dropped.

| page control | request |
|---|---|
| run button | `start` or `stop` |
| LO: the header display (wheel, click, keys, typing), the LO box, double-click on the plot | `tune` `hz`, refused outside the band's `rf.lo_range_hz`; any time the radio is loaded |
| Band, Sample rate 40-320 Msps, then Apply | `configure` `band`, `sample_rate` (the LO stays) |
| Filter 20-320 MHz | `filter` `mhz` |
| Automatic gain control, LNA 0-4 (~6 dB steps), Baseband 0-9 (~2 dB steps) | `gain` `agc:true`, or `lna` and `lpf` |

The controller picks the stream's packing from the sample rate (raw up to 160 Msps, half at 320,
lossless either way); the Stream panel shows the share of packed slices. While a configure,
start or stop runs, other requests are refused and the page
disables its controls and shows the controller's progress. Tune, gain and filter requests
queue one at a time; a newer one of the same kind replaces one not yet sent.

The controller's state says what the stream is: `stream.center_hz` and `stream.sample_rate`
label every frame from then on. A tune or configure marks the stream as changing from the
newest slice received; frames are dropped from there until the state reports the new tuning,
plus 1024 slices for datagrams still on their way. A frame never spans a change, so none
carries samples of one tuning under the label of another.

## The stream

UDP datagrams from the unit's export, each a 16-byte little-endian header (`u32` magic
`0x31524453` "SDR1", `u16` offset, `u16` slice_bytes, `u64` seq: the DMA completion number)
and one slice: 8192 bytes raw (2048 samples) or 7168 bytes packed (of each 8 samples, the 8th
rides in the other 7's spare bits [31:30] and [15:14]).

Slices are filed into the ring by `seq`. A run (a stream) begins with the first datagram after
a second of silence, after a jump in `seq`, or after the controller reports the stream stopped
or is asked to start one. Completions shorter than 4096 bytes are firmware events that share
the unit's ring: their slot is marked, the frontier steps over it without counting a loss, and
the samples either side are contiguous. A completion the unit does not send at all (an empty
one) cannot be told from a lost datagram and counts as lost. Pieces of slices (`offset` not 0)
are counted, not reassembled.

## Page and server

HTTP serves the page; everything else is a WebSocket at `/ws` (permessage-deflate). Messages
are JSON text with a type `t`, plus binary spectrum frames.

From the page: `ping {c}`, `ack {seq}` for every frame, `view {start, stop, bins, auto, fps,
kbps}` (the range and resolution wanted, and pacing limits), `fft {size, window, rate,
averaging, peak, dc}`, `set {...}` with any of `lo`; `band, sample_rate`; `filter`; `agc,
lna, lpf`, and `run {on}`.

From the server: `hello {version, windows, max_bins, controller, bands, sample_rates,
filters, max_lna, max_lpf}`, `status` (sections `state`, `message`, `running`, `devices`,
`receiver {lo, sample_rate}`, `controller {mode, command, alive, pid,
restarts, busy, progress, error, queued, state}`, `fft`, `spectrum`, `stream`, `link`; unchanged
ones are left out), `notice {level, text}` and `pong`. A frame is a 48-byte little-endian header
(`u8` kind 1, `u8` flags (bit 0: peak detector), `u16` bins, `u32` sequence, `f64` start and stop
Hz, `f32` base dB, `f32` step dB, `u32` frames merged, `u32` FFT blocks, `f64` server time ms)
and one byte per bin: dBFS = base + code × step.

## Tests

`python3 -m unittest tests.test_spectrum_server tests.test_spectrum_recv` from the repository
root builds both tools and runs them against a fake controller (`tests/spectrum_fake_controller.py`)
and a fake export over loopback.
