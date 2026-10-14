#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Router-local, read-only BLE advertisement viewer. No scanning transmissions."""

import argparse
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import threading
import time
from urllib.parse import urlsplit

TYPES = (
    "ADV_IND",
    "ADV_DIRECT_IND",
    "ADV_NONCONN_IND",
    "SCAN_REQ",
    "SCAN_RSP",
    "CONNECT_IND",
    "ADV_SCAN_IND",
    "ADV_EXT_IND",
)


def advertisement(hex_pdu):
    p = bytes.fromhex(hex_pdu)
    if len(p) < 2 or len(p) != p[1] + 2:
        raise ValueError("PDU length mismatch")
    kind = p[0] & 15
    if kind > 7:
        raise ValueError("reserved PDU")
    body = p[2:]
    out = {
        "kind": TYPES[kind],
        "address": None,
        "name": "",
        "fields": [],
        "random": bool(p[0] & 64),
        "raw": hex_pdu,
        "extended": kind == 7,
    }
    if kind in (0, 1, 2, 4, 6) and len(body) >= 6:
        out["address"] = ":".join(f"{b:02X}" for b in body[:6][::-1])
    elif kind in (3, 5) and len(body) >= 12:
        out["address"] = ":".join(f"{b:02X}" for b in body[6:12][::-1])
        out["scanner"] = ":".join(f"{b:02X}" for b in body[:6][::-1])
        out["random"] = bool(p[0] & 128)
    if kind == 7:
        out["note"] = (
            "Primary extended-advertising header; secondary-channel payload is not followed."
        )
    if kind in (0, 2, 4, 6):
        ad = body[6:]
        at = 0
        while at < len(ad):
            n = ad[at]
            at += 1
            if not n:
                break
            if at + n > len(ad):
                out["malformed_ad"] = True
                break
            typ = ad[at]
            value = ad[at + 1 : at + n]
            at += n
            field = {"type": f"0x{typ:02X}", "hex": value.hex()}
            if typ in (8, 9):
                field["label"] = "Local name"
                field["value"] = value.decode("utf-8", errors="replace")
                out["name"] = field["value"]
            elif typ == 1:
                field.update(label="Flags", value=value.hex())
            elif typ == 10 and len(value) == 1:
                field.update(
                    label="TX power",
                    value=f'{int.from_bytes(value,"little",signed=True)} dBm',
                )
            elif typ == 255 and len(value) >= 2:
                field.update(
                    label="Manufacturer data",
                    value=f'Company 0x{int.from_bytes(value[:2],"little"):04X} · {value[2:].hex()}',
                )
            elif typ in (2, 3) and len(value) % 2 == 0:
                field.update(
                    label="16-bit service UUIDs",
                    value=", ".join(
                        f'{int.from_bytes(value[k:k+2],"little"):04X}'
                        for k in range(0, len(value), 2)
                    ),
                )
            elif typ == 22 and len(value) >= 2:
                field.update(
                    label="Service data",
                    value=f'UUID {int.from_bytes(value[:2],"little"):04X} · {value[2:].hex()}',
                )
            else:
                field.update(label=f"AD type 0x{typ:02X}", value=value.hex())
            out["fields"].append(field)
    return out


class State:
    def __init__(self):
        self.lock = threading.Lock()
        self.packets = deque(maxlen=2000)
        self.channels = {str(c): {"packets": 0, "last_status": 0} for c in (37, 38, 39)}
        self.total = 0
        self.errors = 0
        self.stopped = False
        self.started = time.time()

    def ingest(self, event):
        with self.lock:
            typ = event.get("type")
            now = time.time()
            if typ == "start":
                self.packets.clear()
                self.total = self.errors = 0
                self.channels = {
                    str(c): {"packets": 0, "last_status": 0} for c in (37, 38, 39)
                }
                self.stopped = False
            elif typ == "packet":
                if event.get("crc_valid") is not True or event.get("channel") not in (
                    37,
                    38,
                    39,
                ):
                    return
                try:
                    packet = advertisement(event["pdu"])
                except (ValueError, KeyError):
                    self.errors += 1
                    return
                self.total += 1
                packet.update(
                    id=self.total,
                    time=event.get("time", now),
                    channel=event["channel"],
                    crc=event.get("crc", ""),
                )
                self.packets.append(packet)
                self.channels[str(event["channel"])]["packets"] += 1
            elif typ == "status" and event.get("channel") in (37, 38, 39):
                ch = self.channels[str(event["channel"])]
                ch.update(event, last_status=now)
                self.stopped = False
            elif typ == "stopped":
                self.stopped = True
            elif typ == "error":
                self.errors += 1

    def snapshot(self):
        with self.lock:
            now = time.time()
            channels = {}
            for channel, status in self.channels.items():
                active = not self.stopped and now - status["last_status"] < 3
                channels[channel] = {
                    **status,
                    "active": active,
                    "lag_ms": status.get("backlog_samples", 0) / 80000,
                    "dropped": status.get("lost_samples", 0),
                }
            return {
                "total": self.total,
                "channels": channels,
                "packets": list(self.packets)[-500:][::-1],
                "retained": len(self.packets),
                "history_limit": 2000,
                "errors": self.errors,
                "stopped": self.stopped,
                "phy": "LE 1M",
                "backend": "Three radio DSPs",
                "sample_rate": 80000000,
                "time": now,
            }


def watch(path, state):
    position = 0
    identity = None
    while True:
        try:
            with path.open() as f:
                stat = path.stat()
                current = (stat.st_dev, stat.st_ino)
                if current != identity or stat.st_size < position:
                    position = 0
                identity = current
                f.seek(position)
                while True:
                    line = f.readline()
                    if not line or not line.endswith("\n"):
                        break
                    position = f.tell()
                    try:
                        state.ingest(json.loads(line))
                    except (ValueError, TypeError):
                        pass
        except FileNotFoundError:
            pass
        time.sleep(0.1)


def make_handler(state, web):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            path = urlsplit(self.path).path
            if path == "/api/state":
                data = json.dumps(state.snapshot()).encode()
                mime = "application/json"
            elif path == "/api/export":
                with state.lock:
                    data = (
                        "\n".join(json.dumps(p) for p in state.packets) + "\n"
                    ).encode()
                mime = "application/x-ndjson"
            elif path in ("/", "/app.js", "/style.css"):
                name = {
                    "/": "index.html",
                    "/app.js": "app.js",
                    "/style.css": "style.css",
                }[path]
                data = (web / name).read_bytes()
                mime = {
                    "/": "text/html",
                    "/app.js": "text/javascript",
                    "/style.css": "text/css",
                }[path]
            else:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", mime + "; charset=utf-8")
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header(
                "Content-Security-Policy",
                "default-src 'self'; style-src 'self'; script-src 'self'; connect-src 'self'; frame-ancestors 'none'",
            )
            if path == "/api/export":
                self.send_header(
                    "Content-Disposition",
                    'attachment; filename="ble-advertisements.jsonl"',
                )
            self.end_headers()
            self.wfile.write(data)

        def log_message(self, *args):
            pass

    return Handler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--events", type=Path, required=True)
    p.add_argument("--port", type=int, default=8074)
    p.add_argument("--listen", default="0.0.0.0")
    p.add_argument("--web", type=Path, default=Path(__file__).with_name("web"))
    a = p.parse_args()
    state = State()
    threading.Thread(target=watch, args=(a.events, state), daemon=True).start()
    ThreadingHTTPServer((a.listen, a.port), make_handler(state, a.web)).serve_forever()


if __name__ == "__main__":
    main()
