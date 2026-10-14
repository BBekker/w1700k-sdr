# Three-channel BLE DSP receiver

Receive LE 1M packets on primary advertising channels 37, 38 and 39 at the same
time, and browse CRC-verified advertisements and scan responses on the router.
This is a separate experiment on `codex/ble-advertising-dsp`; it does not change
the upstream DMA pull request.

## Data path

One band-0 capture at **80 Msps, LO 2441 MHz, analog filter 80 MHz** covers all
three centers: 2402, 2426 and 2480 MHz. There is no channel hopping.

1. `tools/dsp/wm-ble.c` continuously moves the radio's ICAP ring to the host's
   WFDMA ring using MCU_DMA0. Raw samples stay inside the router.
2. `channel.c` uses one ARM worker per channel to translate its frequency, apply
   an 8-sample averaging filter and a third-order CIC decimator, and produce
   2 Msps complex I/Q. ARM NEON accelerates this stage. Filter and oscillator
   state continue across DMA blocks.
3. Bounded queues let filtering overlap transfers. Each 2048-sample block is
   scaled by a common power of two to signed 8-bit I/Q. The input mailbox transfer
   is **12 MB/s total**, versus 24 MB/s for 16-bit samples. This trades quantization
   precision for the measured host-to-radio SRAM bandwidth limit.
4. The driver writes the block and publishes its request in one `BLOCK` ioctl.
   MEM_DMA transfers it from SRAM to the assigned DSP's DTCM. DSP 0 receives
   channel 37, DSP 1 channel 38, and DSP 2 channel 39.
5. DSP operation 8 runs the phase discriminator, two symbol-timing hypotheses,
   advertising access-address search, dewhitening, PDU length validation and
   CRC24 verification. Decoder state survives block boundaries. Only verified
   packets return to the CPU; the CPU parses advertising data for display.
6. `server.py` serves the local website on port **8074**, with names, addresses,
   channel, packet type, advertising fields, raw PDU/CRC, search and JSONL export.

The DSP decoder uses integer instructions; it does not use the FFT routine or
claim vectorized DSP decoding. Channel filtering still runs on the ARM CPU.
Input uses MEM_DMA; the small decoded results are copied through SRAM by WM and
read by the host. This is not an entirely CPU-free or end-to-end zero-copy path.

### Ownership and timing

Each core has one input/output mailbox. The host consumes the previous output
before reusing the input. WM publishes the result sequence **after** all result
bytes, and does not accept another request for that core until decoding ends.
The host polls one completion word; additional result reads happen only when
packets were decoded. Publication writes occur after the bulk input copy while
the driver's I/O lock remains held.

| Radio address | Purpose |
| --- | --- |
| `0090b000`, `0090c000`, `0090d000` | 4096-byte input mailboxes |
| `0090e000`, `0090e400`, `0090e800` | 1024-byte result mailboxes |
| `0090ec00` | WM status and DMA counts |
| `0090fe90 + 32*core` | Request/ack, reset, sample count and result metadata |
| `00910800`, `00910900` | MEM_DMA TX/RX descriptors |
| `00912000` | WM helper entry |
| DSP local `201000`, `204000`, `208000` | Decoder state, input, output |

Result word 1 is `(sequence << 4) | packet_count`; word 6 gives total result
bytes. Sequence wraps at 28 bits, skipping zero. `BLOCK` permits only aligned
scratch-SRAM transfers of at most 4096 bytes, plus up to four existing CTL-range
publication writes. It never exposes powered DSP memory directly to the host.

The router kernel has a 4 ms tick and no high-resolution timers. Receiver
workers therefore do not use submillisecond sleeps. Dedicated transfer threads
use `SCHED_FIFO` priority 1 on ARM cores 0–2 so completions preempt filtering;
they block during each driver transfer. The raw stream pump remains under normal
scheduling on core 3. Root/CAP_SYS_NICE is required, and failure to obtain the
required scheduling policy is reported.

## Build and stage

Use the existing repository setup to prepare the cross compiler and kernel:

```sh
tools/macos/run make -C platform/linux module
PATH=/opt/homebrew/opt/llvm/bin:/opt/homebrew/opt/lld/bin:$PATH python3 tools/ble/build.py
```

The bundle is `tools/ble/build/bundle/`. Copy its contents to `/work/ble/` on the
router using the already configured SSH access. Copy the router's existing
`/opt/w1700k/config.json` to `/work/ble/config.json`, changing only `unit.module`
to `/work/ble/w1700k_sdr.ko`. Keep calibration and unit-specific paths intact.
No credentials, calibration, captures or generated firmware belong in Git.

Stop the existing SDR/FFT viewer before starting BLE. On this installation that
is `/work/dsp-live/dsp-viewer stop`. Then run on the router:

```sh
export PYTHONHOME=/opt/w1700k PYTHONPATH=/opt/w1700k/host PYTHONDONTWRITEBYTECODE=1
/opt/w1700k/bin/python3 /work/ble/service.py start
```

Initialization loads the research DSP image through the normal firmware loader
and configures band 0. It takes about a minute. Open `http://<router-address>:8074/`.
Use the same service command with `status` or `stop`; diagnostics are in
`/work/ble/service.log`. The capture is bounded to 24 hours, after which the
website retains the last results; stop/start begins another capture.

This installation is in RAM and does not survive reboot. To return to FFT, stop
BLE and run `/work/dsp-live/dsp-viewer start`, which loads its own saved image.

For a bounded hardware check after loading/configuring the DSPs:

```sh
/opt/w1700k/bin/python3 /work/ble/receive.py \
  --config /work/ble/config.json --helper /work/ble/wm-ble.bin \
  --receiver /work/ble/ble-receiver --events /work/ble/events.jsonl --seconds 90
```

## Validation and limits

A 90-second live run with the website open decoded **6556 CRC-valid packets**:
1526 on channel 37, 3523 on channel 38, and 1507 on channel 39. All 267 periodic
channel reports showed **zero input overruns**. The largest sampled raw-input
backlog was 18.688 ms; final periodic reports were 0.154–0.256 ms. Each DSP
processed 87880 blocks. WM sent and retired all 3515644 raw DMA buffers, with
status zero; the receiver stopped with no malformed buffers or failures. This
is a bounded hardware validation, not a long-term reliability claim.

`tests/test_ble.py` checks the Bluetooth specification's published channel-38
air vector, CRC rejection, wrong-channel rejection, frequency offsets, block
boundaries and quantized input. A synthetic wideband waveform contains three
simultaneous GFSK advertisements and passes through the actual channelizer and
decoder. NumPy is needed for that test. `tests/test_ble_web.py` checks parsing,
CRC gating, retention, status and HTTP responses. Driver helper tests cover
mapping, bulk byte order, rejection before bus access, ownership and pump behavior.

```sh
PYTHONPATH=tests python3 -m unittest test_ble test_ble_web -v
```

The website reports input overruns and processing backlog; zero overruns means
the software consumed its capture, **not** that every nearby transmission was
received. RF collisions, interference, weak signals, oscillator error and filter
selectivity still affect packet reception. The lightweight channelizer and
8-bit handoff are not a calibrated Bluetooth measurement receiver. Sensitivity
at the outer channels has not been characterized.

This receiver is passive: it transmits no scan requests and can show scan
responses only when another scanner solicits them. It does not decode LE Coded
or follow secondary extended-advertising channels or established connections.
Primary LE 1M extended headers are retained as raw packets. Display timestamps
record host publication time, not an RF-synchronized arrival time. DSP timing
statistics sample blocks containing packets, not every empty block.

The browser retains 2000 packets and displays the latest 500. The router event
log rotates at 16 MiB and keeps one previous file. Stop boundaries can leave a
partial block or queued samples; explicit `tail` records report these separately
from overruns during reception. Advertising contents are treated as text in the
browser, and the HTTP service has no radio-control or transmission endpoints.

Protocol references: [Bluetooth Core sample data, §4.2.1](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-54/out/en/low-energy-controller/sample-data.html)
and [LE Link Layer specification](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/low-energy-controller/link-layer-specification.html).
