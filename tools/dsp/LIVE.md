# DSP spectrum viewer preview

## Current default: input/output DMA with ping-pong buffers

`dsp-viewer start` now runs `pipeline.py`, `wm-pipeline.c` and the native
`dma-reader.c` with all three DSP cores. `dsp-viewer snapshot` retains the older
single-core bulk-read fallback described below.

```
ICAP snapshot -> MEM_DMA -> raw A/B on each DSP
                            DSP FFT workspace
host DMA ring <- MCU_DMA0 <- power A/B on each DSP
      -> native validation of every result -> latest display frame at 20 Hz
```

WM schedules the next input while an earlier FFT and output transfer run. It
reuses raw input only after the DSP acknowledgement, and power output only after
the host-bound DMA descriptor completes. Each numbered result is sent as two packets:
a 64-byte metadata record followed by 8192 power bytes. Neither raw IQ nor powers are
copied through the WM CPU or host MMIO read loop. The host consumes DMA-completed
pages through the existing driver's read-only mmap, checks every sequence and
power value, then publishes the latest display frame.

Measured results ([full evidence](pipeline-results.json)):

- Direct 8 KiB DSP-to-host DMA: 8.15–8.25 us on all three cores; both alternating
  power buffers matched the seeded patterns byte-for-byte over 128 transfers/core.
- 3,000 pipelined FFTs: 9,047 transforms/s, about 110.5 us between completed FFTs
  across the three cores. Input completion averaged 7.51 us; output pair completion
  including WM polling averaged 10.60 us. No missing or reordered result packets.
- Each of the six retained raw/power buffer pairs passed an independent 2048-bin
  direct DFT check; worst relative power L2 error 1.38e-7.
- A 15-second native-consumer run completed 135,756 FFTs and consumed 271,512
  packets, crossing the 131,072-slice host ring twice. Zero sequence errors,
  malformed packets or reader overwrites. All captured blocks were computed and
  delivered. This statement applies to selected snapshots, not the entire RF input.

**This still is not continuous 40 Msps coverage.** The DSPs can currently process
about 18.5 Msps of snapshots. ICAP waits for input credit, leaving capture gaps;
no input decimation or reduction of the configured 40 MHz bandwidth is hidden.
The deployed viewer reported 18.49 million samples/s processed (46.23% coverage),
all three cores, zero CPU FFT workers, and `continuous=false`. The native pump
still polls descriptors: a short CPU sample measured about 1.10 application CPU
cores for pump plus reader, and 0.01 core for the viewer.
The separate continuous-ring experiment below detects this compute shortfall.

The viewer accepts the old `DSPFFT1` file and new `DSPFFT2` file. Both have the
same 56-byte header and 2048 powers; v2 uses header word 12 as the DSP core mask
instead of v1's block count. The DMA reader emits mask 7. Status exposes
`fft.backend="dsp"`, `dsp.core_mask=7`, and `dsp.continuous=false`.

DSP mailbox op 7 selects raw and power banks independently via address bits 0
and 1. Per-core memory (DSP-local): FFT `0x201000..0x204fff`; power A/B
`0x205000..0x206fff` / `0x20b000..0x20cfff`; raw A/B `0x20d000..0x20efff` /
`0x207000..0x208fff`; twiddles `0x209000..0x20afff`; packet headers at
`0x200100` and `0x200140`. Op 6 uses overlapping experimental storage and must
never run concurrently with the pipeline.

Build `dma-reader.c` with the router's Linux C compiler (`-O3 -std=c11 -pthread`)
and install the result as `/work/dsp-live/dsp-dma-reader`. Stage `pipeline.py` and
`wm-pipeline.bin` as well as the DSP container and viewer. Normal firmware loading
must precede switching between helper implementations at `0x00912000`.

For a bounded run after loading/configuring idle band 0 on the router:

```sh
python3 pipeline.py --config /opt/w1700k/config.json --helper wm-pipeline.bin \
  --reader /work/dsp-live/dsp-dma-reader --output /work/dsp-live/spectrum.frame \
  --seconds 15
```

The reader's stream pump sends the stop word and drains DMA. Python waits for
WM retirement before restoring capture configuration. SIGTERM follows the same
shutdown path. The service session is bounded to 24 hours. The service does not
flash firmware or become persistent across reboot.

## Previous single-core fallback


The viewer can now display spectra computed on the radio DSP. This experimental
backend bypasses the router's raw IQ stream receiver and starts no CPU FFT workers.

```text
2048-sample ICAP snapshot -> MEM_DMA -> DSP0 DTCM
    -> signed IQ decode -> DC removal -> periodic Hann -> FFT -> normalized power
    -> WM copies only power results to the host-readable mailbox
    -> router reads powers -> dB conversion -> existing web spectrum / waterfall
```

The raw samples stay on the radio. Power output still crosses the WM/host interface;
this version does not claim zero CPU work. Python controls capture and reads the
power mailbox; the web server converts power to dB and serves browser frames.

## Results and boundaries

Measured on the existing W1700K2, band 0, at 40 Msps:

- Unpaced producer: about 237 spectra/s in the isolated host-I/O profile, with
  2048 bins (19.53 kHz spacing). The browser display independently takes the
  latest snapshot at 20 fps. Earlier producer: about 19.2 spectra/s.
- MEM_DMA: about 8.7 us for 8192 raw bytes.
- DSP decode, DC removal, Hann window, FFT and power: about 328 us after
  optimization (original scalar radix-2 preview: 606–613 us).
- All 2048 power bins from a received capture agree with an independent double
  precision direct DFT; relative power L2 error about 1.06e-7.
- Zero CPU FFT workers. Short system CPU samples fell from 31.7% to 9.6%.
  This compares the previous 32768-point CPU viewer plus raw-stream ingestion
  against the 2048-point DSP snapshot preview; it is not an equal-resolution benchmark.

At 237 transforms/s, only about 1.21% of the 40 Msps input is transformed. ICAP is
stopped before each DMA, so this viewer does not establish continuous all-sample
processing. It uses DSP0; separate arithmetic checks passed on all three cores. Resolution, window,
averaging and DC behavior are fixed. Receiver controls are disabled in this mode
because the snapshot producer owns capture: stop it before changing the radio.

[Live measurements](live-results.json) include source/binary hashes and limits.
Those describe the original preview; [optimization measurements](performance-results.json)
record the newer image, same-input comparisons and arithmetic checks.
The earlier [research results](dma-results.json) refer to the pre-viewer sources
in commit `b51c245`, not the later spectrum image built here.

## Source and memory layout

`main.c` adds mailbox op 4, calling the portable arithmetic in `spectrum.c`.
It uses the existing DTCM FFT workspace, twiddles and raw input, with 2048 power
floats at DSP-local `0x205000..0x206fff`. Native tests exercise the same arithmetic
against analytic tones, DC and independent DFT sums.

The optimized float path uses mixed radix-4/2 butterflies. Four/eight-point
initial transforms stay in registers, and unity twiddles bypass general complex
multiplies. Integer DC sums are exact for 2048 signed 14-bit samples. Windowing
processes opposite halves together (shared cosine, adjacent bit-reversed output
slots), eliminating an intermediate float buffer and the separate permutation
pass. It still uses scalar floating-point/FMA instructions, not vector instructions.

Mailbox op 5 checks MRV5 custom dual-accumulator arithmetic; op 6 runs an explicitly
experimental fixed-point spectrum. Its Q15 twiddles occupy `0x207000..0x208fff`,
which is disjoint from the 2048-point FFT and power buffers. Both operations require
interrupts disabled. On the same received input, op 6 took about 419 us and had
2.7% relative power L2 error, versus 328 us and 1.28e-7 for float. Consequently the
viewer always uses op 4. Fixed-point per-stage rounding particularly affects weak
signals; passing its arithmetic tests is not an accuracy endorsement for the viewer.

With the viewer/producer stopped and the current image loaded, compare both paths
using a private `live.py --validate` capture:

```sh
PYTHONPATH=host python3 tools/dsp/benchmark.py --config CONFIG.json \
  --capture tools/dsp/build/capture.json --out tools/dsp/build/benchmark.json
```

The benchmark times ten executions of each complete spectrum calculation with
resident input; transport is excluded. Fixed-point table conversion is included.
Additional tests of precomputed float plans in instruction/data memory did not
outperform the selected implementation and are not in the production path.
The subsequent host-I/O changes cut the measured producer loop from 51.625 ms
to 4.218 ms (12.24x), without changing the FFT arithmetic. The 980-frame steady
sample, after 20 warmup frames, measured:

| Stage | Before | After |
|---|---:|---:|
| Producer pacing | 27.19 ms | none |
| Read 2048 powers | 10.14 ms | 2.61 ms |
| Health and DSP/DMA status | 6.91 ms | 0.062 ms metadata; health included below |
| Capture command and completion | 4.28 ms | 0.937 ms |
| Validate/publish | 2.87 ms | 0.388 ms publish; validation moved into WM copy |

These are producer wall times; DSP/DMA timings are nested in capture completion.
Browser work and setup are excluded. The unpaced producer consumes roughly one
application CPU core despite doing much less CPU work per FFT.
[Host-I/O measurements](host-io-results.json) record the detailed profile and
independent all-bin DFT check (relative power L2 error 1.22e-7).
Display pacing now belongs to the C++ viewer, so it cannot sleep the producer.
The viewer counts producer sequence advances for sample coverage; intermediate
display snapshots are not averaged or retained.

`wm-live.c` links at `0x00912000`, calls the existing bounded MEM_DMA helper, waits
for DSP acknowledgement, checks traps/results, and copies only power floats into
`0x0090b000..0x0090cfff`, validates nonnegative finite power bits during that
copy, and publishes `{DFP1, request, dsp_cycles, dma_cycles}` at `0x0090d000`. It uses the tested pool route and 2048-sample one-shot
capture. Never patch helper code at an address already executed with different
contents; reload firmware first. No CPU stream pump or other capture client may
run alongside this producer.

`live.py` initializes twiddles and the pool route, calls the helper per frame,
checks results, and atomically replaces a latest-frame file. On the router it uses
the existing READ_MEM ioctl for two bulk 4096-byte power reads, selecting the
PCIe remapping window once per page. A direct CTL write launches the helper and
bulk status reads retain WM magic/state/trap/return-value checks. The compact
metadata record replaces separate DSP and DMA interpreter commands.
The SSH fallback uses the existing control client. Shutdown restores capture route
registers; DMA queues stay reserved/stopped until a normal firmware reload.

`w1700k-spectrum --dsp-input PATH` reads that file without creating FFTW plans or
CPU FFT workers. It rejects malformed, nonfinite and stale frames. The UI identifies
DSP0, shows DMA/compute timings and actual sample coverage, and disables unsupported
settings. The ordinary CPU backend is unchanged when this option is absent.

Wire format: little-endian `<8sIIQddIIQ`, followed by 2048 float32 linear powers:
magic `DSPFFT1\0`, point count, block count, sequence, LO Hz, sample rate Hz,
DSP cycles, DMA cycles, capture result timestamp ns. Total length is 8248 bytes.
Powers are in FFT order and normalized so a full-scale complex tone is 1. The
viewer shifts the frequency order and converts to dBFS.

## Build, stage and run

Build the DSP images with LLVM and build the spectrum server for the router:

```sh
python3 tools/dsp/build.py
# Build host/spectrum with the router's C++ cross toolchain and FFTW/zlib.
```

For the existing router package, stage these into `/work/dsp-live`:

- `tools/dsp/*.py` and `tools/dsp/build/{w1700k_dsp_test,wm-live}.bin`
- the router build of `w1700k-spectrum`
- `host/spectrum/web/` as `web/`
- `tools/router/dsp-viewer`

The service helper expects the existing `/opt/w1700k` Python/controller package,
configuration, own firmware and calibration. Run on the router:

```sh
/work/dsp-live/dsp-viewer start
/work/dsp-live/dsp-viewer status
```

Open the router's port 8073. `start` stops the CPU viewer, loads the DSP image through
the normal downloader, and selects band 0 at 2440 MHz / 40 Msps. Logs are in the
staging directory. It does not modify flash or install an automatic boot service.
The preview and staged files disappear on reboot; the existing CPU viewer returns.

To stop the experiment and restore the regular viewer without rebooting:

```sh
/work/dsp-live/dsp-viewer cpu
```

For a bounded hardware check, after loading the image and configuring idle band 0:

```sh
PYTHONPATH=host python3 tools/dsp/live.py --config CONFIG.json \
  --helper tools/dsp/build/wm-live.bin --output tools/dsp/build/spectrum.frame \
  --frames 100
```

`--rate 0` (the default) removes producer pacing; an explicit positive rate can
cap snapshots for lower CPU usage. This is separate from the 20 fps browser display.

`--validate FILE` additionally saves one raw block and its power output for an
independent DFT check; raw RF captures should remain private. Only this validation
mode reads raw IQ out of the radio. Keep captured files under an ignored build dir.

## Checks

```sh
PYTHONPATH=host:tests python3 -m unittest test_dsp_fft test_dsp_spectrum test_dsp_fixed test_dsp_live_io test_dsp_viewer test_spectrum_server
```

Seventeen tests cover bounded bulk I/O, command/trap/result checks, stream-pump
ownership rejection, FFT math, actual DSP spectrum arithmetic, experimental fixed-point arithmetic, native viewer frame
validation, stale-data handling, rejection of unsupported settings, and the
existing CPU viewer/controller behavior. The snapshot regression test now waits
for enough samples after a retune instead of racing the first FFT against the
larger raw-IQ dump requirement.

## Continuous transport experiment (not the viewer backend)

`wm-continuous.c` / `continuous.py` explore a bounded autonomous ICAP ring ->
MEM_DMA -> three-DSP scheduler. There are no host per-block commands. Descriptors
are reused only after completion; DSP raw buffers are reused only after the
previous FFT acknowledgement. A capture-ring guard detects inability to keep up.

The initial helper transferred all 19,531 complete blocks captured at 40 Msps in
one second across the three DSP buffers; mean DMA time 8.74 us and maximum
backlog 8,240 bytes. With the full spectrum operation, the ring-capacity guard
triggered after 10.50 ms: 93 blocks dispatched, 90 completed, three still active.
This establishes transport capacity, not lossless FFT capacity. The current
source additionally drains complete captured blocks after stopping at the guard;
that follow-up change has not yet been hardware-validated. It is not deployed
as the viewer backend. The driver does not directly expose scratch status at
0x00910b00; the harness reads it through WM only after the helper returns.

Always stop the viewer and reload the DSP firmware before installing a different
helper at 0x00912000. Do not invoke WM interpreter operations while it is running
a helper. The bounded harness accepts `--mode dma|spectrum`, `--mask 1..7`, and
`--ms 1..3000`; DMA-only completion must never be reported as FFT coverage.
