#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the three-channel BLE receiver after a fresh firmware load."""

import argparse
import json
from pathlib import Path
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent / "dsp"))
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "dsp"))
from live import ROUTE
from registers import healthy
from run import read, write, packed, unpack
from w1700k import config
from w1700k.controller import Controller
from w1700k.unit import Unit
from w1700k.wm import Wm


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--config", required=True)
    p.add_argument("--helper", type=Path, required=True)
    p.add_argument("--receiver", type=Path, required=True)
    p.add_argument("--events", type=Path, required=True)
    p.add_argument("--seconds", type=int, default=86400)
    p.add_argument("--decoder", choices=("cpu", "dsp"), default="cpu")
    a = p.parse_args()
    if not 1 <= a.seconds <= 86400:
        p.error("seconds must be 1..86400")
    cfg = config.load(a.config)
    if cfg.unit.transport != "local":
        p.error("receiver requires router-local transport")
    c = Controller(cfg)
    try:
        c.attach()
        state = c.state()
        radio, rf = state["radio"], state["rf"]
        if (
            not radio["loaded"]
            or radio["band"] != 0
            or radio["capture_rate"] != 80000000
            or radio["sample_rate"] != 80000000
            or abs(rf["lo_hz"] - 2441000000) > 100
            or rf["filter_mhz"] != 80
        ):
            raise RuntimeError(
                "configure band 0 at 80 Msps, LO 2441 MHz, filter 80 MHz first"
            )
    finally:
        c._close_ctl()
    code = a.helper.read_bytes()
    code += bytes(-len(code) % 4)
    if not 0 < len(code) <= 0x4000:
        p.error("helper too large")
    child = None
    stopping = False

    def stop(*_):
        nonlocal stopping
        stopping = True
        if child and child.poll() is None:
            child.terminate()

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    with Wm(Unit(cfg.unit)) as w:
        healthy(w)
        if w.session.streaming() or (
            a.decoder == "dsp" and read(w, 0x81020010, 2) != [7, 0]
        ):
            raise RuntimeError("radio not idle / DSP not running")
        write(w, 0x00912000, unpack(code))
        if packed(read(w, 0x00912000, len(code) // 4)) != code:
            raise RuntimeError("helper readback mismatch")
        saved = [(address, read(w, address, 1)[0]) for address, _ in ROUTE]
        cr0 = read(w, 0x80023090, 1)[0]
        w.write(ROUTE)
        w.session.ctl([(0x0090F008, 0)])
        before = w.start(0x00912000, int(a.decoder == "cpu"))
        try:
            child = subprocess.Popen(
                [str(a.receiver), str(a.events), str(a.seconds), a.decoder]
            )
            if stopping:
                child.terminate()
            rc = child.wait()
        finally:
            w.session.ctl([(0x0090F008, 0x53544F50)])
            until = time.monotonic() + 3
            while w.status()["calls"] == before:
                if time.monotonic() > until:
                    raise RuntimeError("BLE helper failed to retire; reload required")
                time.sleep(0.01)
            status = healthy(w)
            stats = w.session.ctl(read=(0x0090EC00, 16))[0]
            w.write(
                [(0x80023090, 0x400), (0x88009004, 0)] + saved + [(0x80023090, cr0)]
            )
            print(
                json.dumps(
                    {
                        "receiver_exit": child.returncode if child else None,
                        "wm": stats,
                        "wm_return": status["ret"],
                    }
                ),
                flush=True,
            )
        if rc or stats[2] or status["ret"]:
            raise RuntimeError("BLE receiver failed; see log")


if __name__ == "__main__":
    main()
