#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build a router-local BLE viewer bundle from an already prepared toolchain."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--cc",
        default=str(
            ROOT / "platform/build/buildroot/host/bin/aarch64-buildroot-linux-musl-gcc"
        ),
    )
    p.add_argument("--out", type=Path, default=ROOT / "tools/ble/build/bundle")
    a = p.parse_args()
    a.out.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / "tools/dsp/build.py")], check=True)
    subprocess.run(
        [
            a.cc,
            "-O3",
            "-mcpu=cortex-a53",
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-pthread",
            "-static",
            str(ROOT / "host/ble/channel.c"),
            str(ROOT / "host/ble/receiver.c"),
            "-lm",
            "-o",
            str(a.out / "ble-receiver"),
        ],
        check=True,
    )
    for source, target in [
        ("tools/dsp/build/w1700k_dsp_test.bin", "dsp.bin"),
        ("tools/dsp/build/wm-ble.bin", "wm-ble.bin"),
        ("kernel/w1700k_sdr/build/w1700k_sdr.ko", "w1700k_sdr.ko"),
        ("tools/ble/receive.py", "receive.py"),
        ("tools/ble/service.py", "service.py"),
        ("host/ble/server.py", "server.py"),
    ]:
        shutil.copy2(ROOT / source, a.out / target)
    shutil.copytree(ROOT / "host/ble/web", a.out / "web", dirs_exist_ok=True)
    (a.out / "dsp").mkdir(exist_ok=True)
    for path in (ROOT / "tools/dsp").glob("*.py"):
        shutil.copy2(path, a.out / "dsp" / path.name)
    print(a.out)


if __name__ == "__main__":
    main()
