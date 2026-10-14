# MT7996 DSP FFT and direct sample DMA research

An experimental [DSP0 web viewer backend](LIVE.md) now builds on this research.
The current viewer pipelines three DSPs with two input and output banks each,
using MEM_DMA for inputs and MCU_DMA0 for outputs into the host DMA ring. It
measured about 9,047 FFTs/s with no result packet loss in the bounded checks;
[details and limitations](pipeline-results.json). Capture still has gaps because
compute capacity is below the 40 Msps source. The measurements below describe
the original transfer/FFT experiments.
The newer float viewer kernel takes about 328 us for a complete 2048-point
windowed power spectrum, down from 606 us; see [performance-results.json](performance-results.json).
An opt-in custom-MAC fixed-point experiment is slower and less accurate, so the
viewer uses the optimized scalar float/FMA path. Vector execution is not established.

MEM_DMA can move received IQ from the radio capture pool directly into DSP0
DTCM. WM submits descriptors and polls completion; neither WM nor the application
CPU copies the sample payload. DSP0 then unpacks signed 14-bit IQ and runs an FFT.
This is a measured research harness, not a continuous receiver or viewer backend.

## Measured results

On one W1700K2 / MT7996, band 0:

| Experiment | Result |
| --- | --- |
| WM scalar copy, 8 KiB | 352 us (23.27 MB/s) |
| MEM_DMA, received pool to DSP0, 8 KiB | 8.69–8.71 us |
| MEM_DMA, two received window pages, 2 × 4 KiB | 9.52 us total |
| Repeated 8 KiB pattern transfers | 933 MB/s transfer interval; 914 MB/s including descriptor recycling and polling |
| DSP0 unpack + 2048-point scalar FFT | 493–499 us |

Timing conversions assume the upstream WM 720 MHz and DSP 640 MHz clocks.
The repeated benchmark covers 1,280 transfers: 128 repetitions at each of five
sizes (64, 256, 1024, 4096, 8192 bytes), with two patterns per size. Full destination
contents and boundary guards are checked after each 128-transfer run. Five
received captures (one window, four pool) pass independent direct DFT comparisons
of all 2048 bins, with relative L2 errors around 1e-7.

The transfer rate exceeds the 160 MB/s required for raw 40 Msps IQ, but this does
**not** establish continuous operation. Captures are stopped before DMA; repeated
benchmarks reuse static buffers. The scalar FFT still cannot process every sample
at 40 Msps. Producer/consumer ownership, capture overwrite prevention, simultaneous
host streaming, and multiple DSP cores remain untested.

[dma-results.json](dma-results.json) records the measured timings, pattern checks,
FFT comparisons and source hashes. [hardware-results.json](hardware-results.json)
records startup, CPU-copy and memory-window experiments;
[research.json](research.json) links the evidence and next experiments. Raw received
captures and vendor firmware are not included.

## How the transfer works

```text
ICAP -> stopped SYSRAM window / capture pool
     -> MEM_DMA TX0 -> RX0 -> DSP0 raw-IQ buffer -> unpack -> FFT
                              ^
                         WM descriptors only
```

MEM_DMA registers start at `0x58000000`. With the project's own firmware idle,
the measured initial `GLO_CFG` is `0x18003075`: 16-byte descriptors and omitted
TX/RX info. The helpers retain the ROM prefetch allocation (`TX0_EXT_CTRL =
0x00800002`, `RX0_EXT_CTRL = 0x02200002`) instead of invoking a vendor allocator.

The helper disables TX/RX, binds two eight-entry rings, resets their indices,
posts seven receive slots, enables the engine, then offers one TX descriptor at
a time. A TX descriptor contains the source address and `LS0 | (length << 16)`;
an RX descriptor contains the destination address and `(capacity << 16)`.
After matching TX/RX DIDX **and RX DDONE**, it checks the received length, restores
the RX capacity, fences, and returns that slot through RX CIDX. DIDX alone is not
a completion guarantee: it was observed advancing before RX DDONE became visible.

The helper uses bounded polling and stops both queues on exit. It leaves their
scratch allocations reserved until a normal firmware reload restores the ROM
ring topology. Source and destination ranges are explicitly limited in
[wm-dma.c](wm-dma.c). Validation reads occur after transfer completion and are
outside the transfer timing. For the cached capture pool, the helper invalidates
clean WM source cache lines after measurement so validation sees the new capture.

| Radio address | Purpose |
| --- | --- |
| `0x00910000..0x009107ff` | WM helper code, at most 2 KiB |
| `0x00910800`, `0x00910900` | TX0 and RX0 descriptor rings |
| `0x00910a00` | Timing/completion result words |
| `0x00912000` | Known-pattern source |
| `0x00464000`, `0x00466000` | Two stopped path-0 capture window pages |
| `0xe0400000` | Stopped normal streaming pool source |
| `0x8620d000..0x8620efff` | DSP0 raw input, 8192 bytes |

DSP-local DTCM holds the mailbox at `0x200000`, complex float data at `0x201000`,
twiddles at `0x209000`, raw IQ at `0x20d000`, and stack at `0x20f000`.
Mailbox op 1 transforms supplied floats (up to 4096 points); op 3 unpacks raw IQ
and transforms it (up to 2048 points). Op 2 supports the earlier guarded RAM-window
experiment; it is not needed for DMA.

## Build and host-side validation

Run from the repository root. Requirements are Python 3.10+, a native C compiler,
LLVM `clang` with RISC-V support, `ld.lld`, `llvm-objcopy`, and `openssl` on PATH.
On macOS, Homebrew LLVM and LLD can provide the cross tools; no Mac-specific
repository changes are required for this DSP build.

```sh
python3 tools/dsp/build.py
python3 -m unittest discover -s tests -p test_dsp_fft.py -v
python3 tools/check_hw_names.py
```

The build produces `tools/dsp/build/w1700k_dsp_test.bin`, `wm-dma.bin`, and the
optional CPU baseline `wm-copy.bin`. The DSP container has code and mailbox
regions for all three cores, but these experiments exercise only DSP0.
The native tests check FFT arithmetic against direct DFT sums and analytic
impulse/DC/tone inputs; they do not test hardware access or DMA.

## Hardware reproduction

Requires an already installed upstream SDR system, its own working radio firmware,
calibration, and host SSH configuration. These tools use the existing `host/w1700k`
API. Stop the viewer/service first so nothing can restart a capture concurrently.
Do not run against stock vendor firmware or an active stream.

Set `CONFIG` to your existing configuration. Output stays under the ignored build
directory. Load the DSP through the normal PDA firmware downloader, between WM
and WA, followed by normal RX bring-up:

```sh
export PYTHONPATH="$PWD/host"
CONFIG="$HOME/.config/w1700k/config.json"
python3 host/bin/w1700k --config "$CONFIG" stop
python3 tools/dsp/load.py --config "$CONFIG" \
  --container tools/dsp/build/w1700k_dsp_test.bin
python3 host/bin/w1700k --config "$CONFIG" band 0 --rate 40
python3 tools/dsp/memdma.py --config "$CONFIG" \
  --out tools/dsp/build/scratch.json
python3 tools/dsp/memdma.py --config "$CONFIG" --destination dsp \
  --out tools/dsp/build/dsp.json
python3 tools/dsp/dma_bench.py --config "$CONFIG" \
  --helper tools/dsp/build/wm-dma.bin --out tools/dsp/build/bench.json
python3 tools/dsp/capture.py --config "$CONFIG" --pool \
  --dma-helper tools/dsp/build/wm-dma.bin --out tools/dsp/build/received.json
python3 tools/dsp/verify_capture.py tools/dsp/build/received.json
```

Omit `--pool` for the two-page capture-window experiment. `run.py` provides
synthetic FFT checks. `capture.py --copy-image tools/dsp/build/wm-copy.bin` is the
CPU-copy baseline and requires a **fresh firmware load** before changing helper
images at the same scratch address, because of WM instruction caching.

After the experiment, including a failed transfer, restore the normal firmware
before restarting the viewer:

```sh
python3 host/bin/w1700k --config "$CONFIG" load --band 0
```

All research changes are volatile; this procedure does not flash the router.
Earlier direct TCM reads while DSP reset was asserted caused `0xdead2117` bus
faults that required physical power removal. Do not substitute direct TCM loading
or manually hold DSP reset. `load.py` uses the working downloader path and checks
readiness before proceeding. On an invalid bus response, stop probing; a normal
reload cannot reliably recover that fault. The optional `registers.py` and
`probe_capture.py` tools preserve the earlier research, not prerequisites for DMA.
