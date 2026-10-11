# SPDX-License-Identifier: MIT
"""sdr-recv against a fake export over loopback: what it keeps, and what it counts."""
import json
import os
import shutil
import socket
import struct
import subprocess
import tempfile
import time
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RECV = os.path.join(ROOT, "host", "recv")
BINARY = os.path.join(RECV, "build", "sdr-recv")
MAGIC = 0x31524453


def free_udp_port():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def slice_words(seq, packed=False):
    """A slice whose words name it: raw words keep the spare bits clear, packed ones may not."""
    n = 1792 if packed else 2048
    mask = 0xFFFFFFFF if packed else 0x3FFF3FFF
    return struct.pack("<%dI" % n, *(((seq * 7919 + i) * 2654435761) & mask for i in range(n)))


@unittest.skipUnless(shutil.which("make") and shutil.which(os.environ.get("CC", "cc")), "no make or C compiler")
class SdrRecvTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        result = subprocess.run(["make", "-C", RECV], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError("build failed:\n" + result.stdout + result.stderr)

    def receive(self, datagrams, *options):
        port = free_udp_port()
        process = subprocess.Popen([BINARY, "--bind", "127.0.0.1", "--port", str(port), "--threads", "2",
                                    "--idle-ms", "400", "--wait", "10", *options],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        ready = process.stderr.readline()  # printed once the sockets are bound
        self.assertIn("listening on", ready)
        sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        for d in datagrams:
            sender.sendto(d, ("127.0.0.1", port))
            time.sleep(0.0005)
        sender.close()
        out, err = process.communicate(timeout=20)
        self.assertEqual(process.returncode, 0, err)
        return json.loads(out.strip().splitlines()[-1])

    def test_keeps_every_completion_in_order(self):
        directory = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, directory)
        path = os.path.join(directory, "stream.bin")
        payloads = {seq: slice_words(seq, packed=seq % 3 == 0) for seq in range(40)}
        datagrams = []
        for seq, payload in payloads.items():
            if seq == 17:
                continue  # lost on the way
            if seq == 5:  # sent in two pieces
                datagrams.append(struct.pack("<IHHQ", MAGIC, 0, len(payload), seq) + payload[:4096])
                datagrams.append(struct.pack("<IHHQ", MAGIC, 4096, len(payload), seq) + payload[4096:])
                continue
            datagrams.append(struct.pack("<IHHQ", MAGIC, 0, len(payload), seq) + payload)
        datagrams.append(b"not a datagram of the export")
        report = self.receive(datagrams, "--out", path, "--capacity-mib", "1", "--seconds", "5")
        self.assertEqual((report["seq_first"], report["seq_last"]), (0, 39))
        self.assertEqual((report["whole"], report["missing"], report["bad"]), (39, 1, 1))
        self.assertEqual(report["missing_examples"], [17])
        self.assertEqual(report["spare_words"], 0)
        with open(path + ".lengths", "rb") as f:
            lengths = struct.unpack("<40I", f.read())
        self.assertEqual(lengths, tuple(0 if seq == 17 else len(p) for seq, p in payloads.items()))
        with open(path, "rb") as f:
            self.assertEqual(f.read(), b"".join(p for seq, p in payloads.items() if seq != 17))

    def test_validates_without_keeping(self):
        datagrams = [struct.pack("<IHHQ", MAGIC, 0, 8192, seq) + slice_words(seq) for seq in range(30)]
        report = self.receive(datagrams, "--horizon", "8", "--seconds", "5")
        self.assertEqual((report["whole"], report["missing"], report["stale"], report["clashes"]), (30, 0, 0, 0))
        self.assertEqual(report["out"], "")


if __name__ == "__main__":
    unittest.main()
