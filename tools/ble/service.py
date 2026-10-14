#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Start/stop the RAM-installed BLE viewer; run on the router."""

import argparse
import fcntl
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
PID = HERE / "service.pid"
LOCK = HERE / "service.lock"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("start", "stop", "run", "status"))
    parser.add_argument("--decoder", choices=("cpu", "dsp"), default="cpu")
    parser.add_argument("--seconds", type=int, default=86400)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 86400:
        parser.error("seconds must be 1..86400")
    lock = LOCK.open("a")
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        running = False
    except BlockingIOError:
        running = True
    if args.action == "status":
        print("running" if running else "stopped")
        return
    if args.action == "stop":
        if running:
            os.kill(int(PID.read_text()), signal.SIGTERM)
            for _ in range(600):
                try:
                    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    break
                except BlockingIOError:
                    time.sleep(0.1)
            else:
                raise RuntimeError("receiver has not stopped; inspect service.log")
        return
    if running:
        if args.action == "run":
            raise RuntimeError("BLE service already running")
        print("already running")
        return
    if args.action == "start":
        fcntl.flock(lock, fcntl.LOCK_UN)
        with (HERE / "service.log").open("w") as log:
            subprocess.Popen(
                [
                    sys.executable,
                    str(__file__),
                    "run",
                    "--decoder",
                    args.decoder,
                    "--seconds",
                    str(args.seconds),
                ],
                stdin=subprocess.DEVNULL,
                stdout=log,
                stderr=log,
                start_new_session=True,
                close_fds=True,
            )
        print("Starting BLE receiver; firmware initialization takes about a minute.")
        return
    PID.write_text(str(os.getpid()))
    child = None
    stopped = False

    def stop(*_):
        nonlocal stopped
        stopped = True
        # Never interrupt firmware loading; stop the capture gracefully once ready.
        if child and child.poll() is None and capture_active:
            child.terminate()

    capture_active = False
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    env = {
        **os.environ,
        "PYTHONHOME": "/opt/w1700k",
        "PYTHONPATH": "/opt/w1700k/host",
        "PYTHONDONTWRITEBYTECODE": "1",
    }
    py = "/opt/w1700k/bin/python3"
    cfg = str(HERE / "config.json")
    web = subprocess.Popen(
        [py, str(HERE / "server.py"), "--events", str(HERE / "events.jsonl")], env=env
    )
    try:
        time.sleep(0.1)
        if web.poll() is not None:
            raise RuntimeError(
                "web server failed to start; inspect the port and service.log"
            )
        commands = [
            [
                py,
                str(HERE / "dsp/load.py"),
                "--config",
                cfg,
                "--container",
                str(HERE / "dsp.bin"),
            ],
            [
                "/opt/w1700k/bin/w1700k",
                "--config",
                cfg,
                "band",
                "0",
                "--rate",
                "80",
                "--lo",
                "2441M",
            ],
            ["/opt/w1700k/bin/w1700k", "--config", cfg, "filter", "80"],
        ]
        if args.decoder == "cpu":
            commands[0] = ["/opt/w1700k/bin/w1700k", "--config", cfg, "load", "--band", "0"]
        for command in commands:
            if stopped:
                return
            subprocess.run(command, env=env, check=True)
        if stopped:
            return
        capture_active = True
        child = subprocess.Popen(
            [
                py,
                str(HERE / "receive.py"),
                "--config",
                cfg,
                "--helper",
                str(HERE / "wm-ble.bin"),
                "--receiver",
                str(HERE / "ble-receiver"),
                "--events",
                str(HERE / "events.jsonl"),
                "--seconds",
                str(args.seconds),
                "--decoder",
                args.decoder,
            ],
            env=env,
        )
        if stopped:
            child.terminate()
        if child.wait():
            raise RuntimeError("receiver failed; see preceding diagnostics")
        # Leave the last capture visible until explicitly stopped.
        while not stopped and web.poll() is None:
            time.sleep(0.5)
    finally:
        web.terminate()
        web.wait()
        PID.unlink(missing_ok=True)


if __name__ == "__main__":
    main()
