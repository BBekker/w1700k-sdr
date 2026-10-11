# Platform: what runs on the W1700K's SoC

The unit boots in three stages:

```
stock U-Boot (vendor, first stage, never replaced)
  -> our U-Boot, the loader     FIT in the `loader` partition, read by stock's bootcmd
  -> our Linux                  FIT in UBI volume `fit`, root SquashFS in volume `rootfs`
```

Linux runs from a read-only SquashFS under a RAM overlay, loads the
`w1700k_sdr` driver at boot, brings up the management port (`lan3`,
192.168.1.1/24) and the 10 GbE IQ port (`eth1`, 10.77.0.1/24, MTU 9216), and
starts Dropbear for key-only root logins. Nothing at startup writes NAND.

## Building

`make` in the repository root builds everything in order; the pieces are:

| Step | Script | Output (under `platform/build/`) |
| --- | --- | --- |
| toolchain | `rootfs/build.py toolchain` | Buildroot cross toolchain, packages and host tools in `buildroot/` |
| linux | `linux/build.py` | `linux/arch/arm64/boot/Image`, `linux/sdr/an7581-w1700k-sdr.dtb` |
| sdr | `make -C linux module reader` | `kernel/w1700k_sdr/build/`: the module and the four clients |
| rootfs | `rootfs/build.py image` | `buildroot/images/rootfs.squashfs` with the driver and clients |
| loader | `loader/build.py` | `images/w1700k-loader.itb` (and `mkimage/`) |
| images | `images.py` | `images/w1700k-system.itb`, `w1700k-recovery.itb`, `rootfs.squashfs`, `durable.ubifs` |
| dumper | `dumper/build.py` | `images/w1700k-dumper.itb` |

A first build downloads about 500 MB of sources and builds a GCC toolchain;
on a 20-core host the whole `make` takes about ten minutes plus the downloads.
Later builds redo only what changed (a no-change platform build takes seconds). Each script logs to
`platform/build/logs/` and prints the end of the log when a command fails.
`JOBS=N` limits parallel jobs. `make clean` keeps downloads and prepared
sources; `make distclean` removes `platform/build/` entirely.

Host requirements: a C compiler, make, git, patch, Python 3.10+, perl, rsync,
bc, cpio, unzip and the usual Buildroot prerequisites. Bison and flex are built
by Buildroot. If the host's `install` is not GNU coreutils (some distributions
ship uutils), `gnuinstall` or `ginstall` must exist.

## Sources

[`sources.json`](sources.json) pins every upstream input: Buildroot by tag and
commit, Linux and U-Boot by tarball SHA-256, the OpenWrt tree (kernel patches,
the Airoha device tree and the RTL8261N PHY firmware generator) by commit, and
the PHY firmware's own generator files and output by SHA-256.
[`fetch.py`](fetch.py) downloads and checks them; `W1700K_OPENWRT_GIT` points it
at another OpenWrt repository (for example a local mirror) while still
requiring the pinned commit.

## Layout

| Directory | Contents |
| --- | --- |
| `rootfs/` | Buildroot configuration and the overlay: init scripts, network and storage configuration, `verify-storage`, the `durable-*` helpers |
| `linux/` | kernel configuration, the board device tree, our kernel patches, the Linux BMT binding, `board_check.py` |
| `loader/` | our U-Boot: defconfig, default environment, device tree, patches, the U-Boot BMT binding |
| `dumper/` | the write-free dumper: defconfig, environment, device tree, patches |
| `patches/` | U-Boot patches both the loader and the dumper carry |
| `bmt/` | the vendor bad-block translation and replacement engine, shared by both U-Boots and Linux, and its C test harnesses |
| `bootstrap/` | the initramfs `init` that mounts the root |
| `partitions.py` | the NAND layout, generated into both device trees and the BMT policy header |

## NAND

512 MiB SPI-NAND, 128 KiB blocks, 2 KiB pages. Offsets are vendor-logical: every
stage translates them through the vendor's bad-block tables (BBT shift, then BMT
substitution) exactly as stock U-Boot does, so all stages agree on where a
partition is.

| Partition | Offset | Size | Contents | Linux |
| --- | --- | --- | --- | --- |
| `bootloader` | 0x0000000 | 2 MiB | vendor BL2/BL31 and stock U-Boot | read-only |
| `uenv` | 0x0200000 | 2 MiB | stock U-Boot's saved environment | read-only |
| `calibration` | 0x0400000 | 2 MiB | identity and radio calibration | read-only |
| `loader` | 0x0600000 | 2 MiB | our U-Boot FIT | read-only |
| `system` | 0x0800000 | 48 MiB | UBI: `ubootenv`, `ubootenv2`, `fit`, `rootfs` | attached read-only |
| `durable` | 0x3800000 | 32 MiB | UBI: `durable`, an empty UBIFS for persistent files | detached; `durable-open` |
| `scratch` | 0x5800000 | 358 MiB | unused | read-only |
| `reserved-bmt` | 0x1be00000 | 66 MiB | vendor bad-block reserve pool | read-only |

The BMT layer replaces blocks that wear out (journalled, so an interrupted
replacement recovers on the next boot) and only ever writes inside
`system`..`scratch`.

## Checks in the build

The build stops when an output is wrong rather than leaving it for the unit to
find: requested kernel options that Kconfig dropped, a device tree whose
devices, 10 GbE wiring, reserved memory or partitions differ from the design,
OpenWrt's `mtk_bmt` linked into a kernel or loader (it rewrites vendor tables it
fails to parse), any NAND write path linked into the dumper, a U-Boot runtime
that would overlap its staging area, or an image that does not fit its
partition or volume. `make check` runs the unit tests, including the BMT layer
compiled for the host.
