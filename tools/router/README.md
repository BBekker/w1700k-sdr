# Router-hosted spectrum viewer

This fork adds a router-local controller and stream input so the browser receives
spectra over HTTP/WebSocket instead of receiving the full raw IQ stream on a Mac.
The router CPU runs the viewer FFT; the DSP research is not integrated here yet.

The changes are:

- `unit.transport: "local"` executes the existing controller commands on the router.
- `export.mode: "local"` runs the capture pump without a UDP network export.
- `w1700k-stream --device /dev/w1700k-sdr` reads completed slices from the mapped
  driver DMA ring, checks overwrite margins, and feeds the existing processing path.
- `--receive-mib` bounds the input buffer. Viewer `--fft-size`, `--fft-rate`, and
  `--fft-average` options bound the initial CPU workload.
- `W1700K_FFTW_ESTIMATE=1` selects quick FFT planning for embedded startup.

`S90spectrum` is the service used by the local deployment. It expects an assembled
package at `/opt/w1700k`, working controller wrappers under its `bin/`, the native
router builds of the stream/viewer, and the existing SDR driver and firmware.
The package also needs Python, FFTW, and their runtime libraries. This source
commit does not add those packages to the generic Buildroot image automatically.

The controller configuration for that package uses the following fields, plus
its own calibration and firmware paths:

```json
{
  "unit": {"transport": "local"},
  "export": {"mode": "local"},
  "sample_rate": 40000000
}
```

The controller wrapper must select that configuration (using `--config` or
`W1700K_CONFIG`). Once deployed, `S90spectrum start` selects band 0 at 2440 MHz
and 40 Msps, uses 32 MiB input/output rings and two processing workers, and starts
the viewer at port 8073 with one FFT worker, 32768 points, 20 spectra/s, and one
block per frame. `S90spectrum stop` stops the service and capture. Logs and
runtime state remain in RAM. The service binds all router interfaces, so use it
on the intended private network.

The service is a deployment example; the private assembled filesystem, device
calibration, SSH keys, flash images, and backups are intentionally excluded.
Native Mac setup and the measured 2.5 GbE stream results are documented in
[../macos/README.md](../macos/README.md).

Validation for this contribution: Mac host programs build; 97 host-control tests
pass; the stream, viewer, recorder, installer and host setup suites run 56 tests
with one systemd-only check skipped on macOS. The Linux-only driver/client harnesses
are not included in that Mac test run. The router-local reader and service were
exercised in the earlier live router deployment, not rerun for publication.
