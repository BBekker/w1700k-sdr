# Native macOS setup

This directory adapts this checkout for native macOS builds. It uses Homebrew
GNU tools and a case-sensitive APFS build volume. No VM or container is used.
The router still runs the project's Linux system.

The complete platform build succeeds natively: loader, Linux, SDR driver,
root filesystem, both durable images, backup/recovery images, radio firmware,
and the Mac receiver/viewer. Installation has now been verified on the connected
W25N04LV unit: stock and raw NAND backups, image readback checks, an installed
Linux boot, SSH, and a two-second 40 Msps radio capture all passed. A later
2.5 GbE live-stream check also passed; measured results are recorded below.

## Dependencies

Install Apple's Command Line Tools and Homebrew, then:

```sh
brew install make coreutils gnu-sed gnu-tar gawk gpatch findutils diffutils \
  gnu-getopt wget rsync cpio lld llvm squashfs libelf bison flex bash flock \
  gcc fftw pkgconf cmake
python3 -m venv tools/macos/.venv
tools/macos/.venv/bin/python -m pip install pyserial
```

`run` currently selects Homebrew GCC 14 as the native Buildroot compiler.
Set `HOSTCC`, `HOSTCXX`, and `HOSTCPP` to a matching installed GNU compiler,
C++ compiler, and preprocessor if needed. The native libuuid build uses Apple
Clang for its Mach-O symbol aliases.
The environment applies only to the invoked command.

## Build

Run commands from the repository root:

```sh
tools/macos/mount-build
tools/macos/run make
```

The mount helper creates an expandable 40 GiB case-sensitive disk image at
`tools/macos/build/platform.sparseimage` and mounts it at `platform/build`.
It refuses to cover an existing nonempty build directory. Run it again after
a reboot to remount the existing image. Keep free space on the Mac for the
disk image to grow.

Buildroot keeps its pinned upstream revision. `buildroot.patch` adapts host
tools and cross-build platform detection. The MTD tool uses portability patches and headers from
the project's pinned OpenWrt checkout. Source download hashes and the
generated Ethernet firmware checksum remain enforced.

These smaller targets are available separately:

```sh
tools/macos/run make firmware
tools/macos/run make host
```

## Mac applications

The runner places the built applications and `host/bin/w1700k` on PATH:

```sh
tools/macos/run w1700k --help
tools/macos/run w1700k-stream --help
tools/macos/run w1700k-spectrum --help
```

The macOS receiver uses one UDP receive socket, a bounded socket buffer,
POSIX shared memory, and a poll-based web server. The receiver reports the
actual socket buffer size at startup. Kernel socket-drop accounting is not
available on macOS; sample sequence loss is still counted. Sustained physical
radio throughput must be measured on the actual Ethernet adapter.

A local simulated stream sustained 40 Msps with correction enabled: 100,000
datagrams, no lost packets or processing overruns. This checks the Mac software
path, not the Ethernet adapter or the radio.

The receiver and spectrum viewer can be checked without router hardware:

```sh
tools/macos/run python3 -m unittest discover -s tests -p 'test_stream.py' -v
tools/macos/run python3 -m unittest discover -s tests -p 'test_spectrum_server.py' -v
```

The full `make check` suite also includes Linux driver/client harnesses that
require Linux host headers. Those harnesses are not yet ported to macOS.

## Router connection

For installation, connect Ethernet to the router's LAN3 port and use the
3.3 V serial console at 115200 baud. Only one program can own the serial port.
The native installer discovers Mac network interfaces using `ifconfig`.

The Linux-specific helpers in `init/host` are not used on macOS. A Mac setup
must assign the installation address to its Ethernet adapter and serve stock
U-Boot on UDP port 69. Run the backup-only installation first and preserve
the NAND backup and the unit's calibration before flashing. See
`init/README.md` for the image checks and installation sequence.

After installation, the sample stream uses the router's 10 GbE port and
jumbo frames. A 2.5 GbE adapter can be tested at 40 Msps; higher sample rates
need more link bandwidth. LAN3 is the installation/management connection.

The connected unit is installed and reachable at `10.77.0.1` through WAN.
Its private connection settings are in `build/receiver-config.json`, including
the SSH identity, pinned host key, calibration path, and `"sample_rate": 40000000`.
That optional setting initializes every band at 40 Msps, including after a router
restart. Raw 40 Msps IQ uses 1.28 Gbit/s before network overhead; raw 80 Msps
exceeds a 2.5 GbE link.

```sh
tools/macos/run w1700k --config tools/macos/build/receiver-config.json status
tools/macos/run w1700k --config tools/macos/build/receiver-config.json load --band 0
```

Backups are private under `build/unit-data/units/`: `nand.bin` is the 446 MiB
ECC-corrected stock restore image, `nand-raw.bin` is the 544 MiB physical snapshot
including spare areas, and `eeprom.bin` is radio calibration. Both transfers
passed device/host SHA-256 checks; the stock bootloader independently checked the
logical backup against its own flash read. Never publish these files: they also
contain the router's original credentials.

The RTL8156 adapter's built-in Apple driver reported a 1504-byte maximum MTU and
negotiated only 1 GbE. Realtek's signed and notarized 1.1.0 DriverKit package is
installed, activated, and bound to en8. MTU 9000 is configured and 8200-byte
ICMP payloads pass between router and Mac. After a router/adapter reboot and
restoring automatic media selection on the Mac, both ends negotiated 2.5 Gb/s.
A live 40 Msps raw IQ stream ran for over 86 seconds with 1,687,242 datagrams
received and zero reported lost, late, malformed, duplicate, or socket-dropped
packets. The spectrum viewer delivered 750 frames during a 30-second check.
Evidence is in `build/validation/stream-2500-check.json` and
`build/validation/receiver-2500-live.log`. Earlier training failures are historical;
their underlying cause was not isolated, and no alternate PHY firmware was used.

After a Mac reboot or adapter reconnection, restore the private stream address
if it is missing (these settings do not change Wi-Fi):

```sh
sudo ifconfig en8 media autoselect
sudo ifconfig en8 inet 10.77.0.10 netmask 255.255.255.0 alias
sudo ifconfig en8 mtu 9000 up
tools/macos/.venv/bin/python tools/macos/start-receiver
```

Open http://127.0.0.1:8073/ and press Play. Keep the rate at 40 Msps for this
2.5 GbE adapter. Adapter interface names may change when the driver takes over;
`en8` is the verified interface for this unit's current Mac setup.
