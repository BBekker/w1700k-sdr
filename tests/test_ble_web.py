# SPDX-License-Identifier: MIT
import importlib.util
import json
from pathlib import Path
import threading
import unittest
from urllib.request import urlopen
from http.server import ThreadingHTTPServer

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("ble_server", ROOT / "host/ble/server.py")
web = importlib.util.module_from_spec(spec)
spec.loader.exec_module(web)


class BLEWebTest(unittest.TestCase):
    def test_name_address_and_malformed_data(self):
        data = bytes.fromhex("010203040506") + bytes([4, 9]) + b"<x>"
        pdu = (bytes([0x40, len(data)]) + data).hex()
        packet = web.advertisement(pdu)
        self.assertEqual(packet["address"], "06:05:04:03:02:01")
        self.assertEqual(packet["name"], "<x>")
        self.assertTrue(packet["random"])
        self.assertTrue(web.advertisement("40080102030405060a09")["malformed_ad"])
        with self.assertRaises(ValueError):
            web.advertisement("4009")

    def test_crc_gate_retention_and_stop(self):
        state = web.State()
        event = {
            "type": "packet",
            "channel": 38,
            "pdu": "4209a6a5a4a3a2c1010203",
            "crc_valid": False,
        }
        state.ingest(event)
        self.assertEqual(state.total, 0)
        event["crc_valid"] = True
        for _ in range(2005):
            state.ingest(event)
        snap = state.snapshot()
        self.assertEqual(snap["retained"], 2000)
        self.assertEqual(snap["total"], 2005)
        self.assertEqual(len(snap["packets"]), 500)
        state.ingest(
            {
                "type": "status",
                "channel": 38,
                "backlog_samples": 80000,
                "lost_samples": 80,
            }
        )
        self.assertTrue(state.snapshot()["channels"]["38"]["active"])
        self.assertEqual(state.snapshot()["channels"]["38"]["lag_ms"], 1)
        state.ingest({"type": "stopped"})
        self.assertFalse(state.snapshot()["channels"]["38"]["active"])

    def test_http_resources_and_export(self):
        state = web.State()
        server = ThreadingHTTPServer(
            ("127.0.0.1", 0), web.make_handler(state, ROOT / "host/ble/web")
        )
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            base = "http://127.0.0.1:" + str(server.server_port)
            with urlopen(base + "/api/state") as r:
                self.assertEqual(json.load(r)["phy"], "LE 1M")
                self.assertIn("script-src 'self'", r.headers["Content-Security-Policy"])
            for path in ("/", "/app.js", "/style.css", "/api/export"):
                with urlopen(base + path) as r:
                    self.assertEqual(r.status, 200)
        finally:
            server.shutdown()
            thread.join()
            server.server_close()


if __name__ == "__main__":
    unittest.main()
