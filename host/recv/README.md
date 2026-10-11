# sdr-recv

Records, or validates, the unit's UDP sample export on the host. It needs only a C11 compiler
and POSIX threads: `make` builds `build/sdr-recv`. Licensed GPL-2.0-only.

```sh
sdr-recv --bind 10.77.0.10 --port 7700 --seconds 10 --capacity-mib 4096 --out stream.bin
sdr-recv --bind 10.77.0.10 --port 7700 --seconds 600     # validate only, any length
```

| option | default | |
|---|---|---|
| `--bind IP` | required | a destination address; give it twice for a second path |
| `--port P` | required | UDP port |
| `--threads N` | 8 | sockets and threads per address (`SO_REUSEPORT`), at most 32 |
| `--seconds S` | 10 | stop S seconds after the first datagram |
| `--idle-ms I` | 1000 | stop after I ms without one |
| `--wait S` | 120 | give up if none ever comes |
| `--capacity-mib M` | 4096 | with `--out`: memory for the whole run, allocated up front |
| `--out PATH` | | keep every completion; without it, validate as it arrives |
| `--report S` | 10 | a progress line every S seconds |
| `--horizon N` | 262144, or the capacity | completions the finalizer stays behind the newest |

With `--out`, PATH holds every completion from the first `seq` to the last, in order, each its
own length (packed slices stay packed), and `PATH.lengths` one native `u32` length per
completion, 0 for one that never arrived. Unlike w1700k-spectrum it reassembles pieces of a
slice.

Every completion is classified whole, short, missing or doubled (`over`). Raw slices must have
their spare bits clear (`spare_words`); a whole slice equal to the one 128 slices earlier (one
lap of the 1 MiB capture ring) is `stale`, and so is a 32-byte line (28 packed) equal to the same
line a lap earlier (`stale_lines`), which a reader that overtook the capture or a packer reading
a stale cache line would produce. Progress and the final result are JSON lines on stdout,
including the UDP receive errors `/proc/net/snmp` counted meanwhile.
