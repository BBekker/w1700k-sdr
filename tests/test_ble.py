# SPDX-License-Identifier: MIT
import ctypes as C
import math
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
# Bluetooth Core 5.4, Vol 6 Part C, 4.2.1. Published bits are in air order.
SPEC_BITS = "01010101 01101011 01111101 10010001 01110001 00101001 00110011 01000111 10100001 10111111 10111110 11000010 01110010 01011000 11100101 00110101 11110111 11110011 10100101"
SPEC_PDU = bytes.fromhex("4209a6a5a4a3a2c1010203")


class Packet(C.Structure):
    _fields_ = [
        ("sample", C.c_uint32),
        ("length", C.c_uint16),
        ("channel", C.c_uint8),
        ("reserved", C.c_uint8),
        ("pdu", C.c_uint8 * 260),
    ]


class Output(C.Structure):
    _fields_ = [
        (n, C.c_uint32) for n in ("count", "dropped", "samples", "crc_fail", "valid")
    ] + [("packets", Packet * 12)]


def iq(bits, cfo=0, phase=0, noise=0):
    rng = random.Random(4)
    out = []
    for b in bits:
        for _ in range(2):
            phase += 2 * math.pi * ((250000 if b else -250000) + cfo) / 2000000
            out.extend(
                (
                    round(1000 * math.cos(phase) + rng.gauss(0, noise)),
                    round(1000 * math.sin(phase) + rng.gauss(0, noise)),
                )
            )
    return out


class DecoderHarness:
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        lib = Path(cls.tmp.name) / "decode.so"
        subprocess.run(
            [
                "cc",
                "-O2",
                "-shared",
                "-fPIC",
                "-Wall",
                "-Wextra",
                "-Werror",
                str(ROOT / "host/ble/decode.c"),
                "-o",
                str(lib),
            ],
            check=True,
        )
        cls.lib = C.CDLL(str(lib))
        cls.lib.ble_decode8.argtypes = [
            C.c_void_p,
            C.POINTER(C.c_int8),
            C.c_uint,
            C.POINTER(Output),
        ]
        cls.lib.ble_reset.argtypes = [C.c_void_p, C.c_uint]
        cls.lib.ble_decode.argtypes = [
            C.c_void_p,
            C.POINTER(C.c_int16),
            C.c_uint,
            C.POINTER(Output),
        ]

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def decode(self, data, ch=38, chunk=37, packed=False):
        state = C.create_string_buffer(4096)
        self.lib.ble_reset(state, ch)
        events = []
        out = Output()
        for off in range(0, len(data), chunk * 2):
            block = data[off : off + chunk * 2]
            if packed:
                shift = max(0, max(abs(v) for v in block).bit_length() - 7)
                block = [v >> shift for v in block]
                a = (C.c_int8 * len(block))(*block)
                self.lib.ble_decode8(state, a, len(block) // 2, C.byref(out))
            else:
                a = (C.c_int16 * len(block))(*block)
                self.lib.ble_decode(state, a, len(block) // 2, C.byref(out))
            events.extend(bytes(p.pdu[: p.length]) for p in out.packets[: out.count])
        return events, out


class BLETest(DecoderHarness, unittest.TestCase):
    def test_packed_iq_across_scaling_boundaries(self):
        bits = [0] * 30 + [int(b) for b in SPEC_BITS if b in "01"] + [0] * 10
        data = iq(bits, cfo=100000, noise=30)
        # Positive amplitude changes do not change the phase discriminator.
        data = [v // (1 + (k // 74) % 4) for k, v in enumerate(data)]
        got, out = self.decode(data, packed=True)
        self.assertEqual(got, [SPEC_PDU])
        self.assertEqual(out.valid, 1)

    def test_published_air_vector_across_blocks(self):
        for cfo in (-100000, 0, 100000):
            bits = [0] * 30 + [int(b) for b in SPEC_BITS if b in "01"] + [0] * 10
            got, out = self.decode(iq(bits, cfo=cfo, noise=30))
            self.assertEqual(got, [SPEC_PDU])
            self.assertEqual(out.valid, 1)

    def test_crc_corruption_and_wrong_channel(self):
        bits = [int(b) for b in SPEC_BITS if b in "01"]
        bits[100] ^= 1
        self.assertEqual(self.decode(iq(bits))[0], [])
        bits = [int(b) for b in SPEC_BITS if b in "01"]
        self.assertEqual(self.decode(iq(bits), ch=37)[0], [])


class ChannelTest(DecoderHarness, unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        lib = Path(cls.tmp.name) / "channel.so"
        subprocess.run(
            [
                "cc",
                "-O3",
                "-shared",
                "-fPIC",
                "-Wall",
                "-Wextra",
                "-Werror",
                str(ROOT / "host/ble/channel.c"),
                "-lm",
                "-o",
                str(lib),
            ],
            check=True,
        )
        cls.channel = C.CDLL(str(lib))
        cls.channel.ble_channel_init.argtypes = [C.c_void_p, C.c_uint]
        cls.channel.ble_channel_run.argtypes = [
            C.c_void_p,
            C.POINTER(C.c_uint32),
            C.c_uint,
            C.POINTER(C.c_int16),
        ]

    def test_simultaneous_wideband_advertisements(self):
        try:
            import numpy as np
        except ImportError:
            self.skipTest("numpy required for wideband test")
        published = [int(b) for b in SPEC_BITS if b in "01"]
        body = published[40:]

        def whitening(channel, n):
            state = channel | 64
            bits = []
            for _ in range(n):
                bits.append(state & 1)
                state = (state >> 1) ^ (0x44 if state & 1 else 0)
            return bits

        plain = [a ^ b for a, b in zip(body, whitening(38, len(body)))]
        count = 65536
        wide = np.zeros(count, dtype=np.complex128)
        t = np.arange(count) / 80e6
        for ch, offset in ((37, -39e6), (38, -15e6), (39, 39e6)):
            bits = published[:40] + [
                a ^ b for a, b in zip(plain, whitening(ch, len(plain)))
            ]
            symbols = np.repeat(np.array(bits) * 2 - 1, 80)
            axis = np.arange(-160, 161) / 80
            gaussian = np.exp(-2 * (np.pi * 0.5 * axis) ** 2 / np.log(2))
            gaussian /= gaussian.sum()
            frequency = np.convolve(symbols, gaussian, mode="same") * 250000 + 50000
            phase = np.cumsum(frequency) * 2 * np.pi / 80e6
            lead = 8192
            wide[lead : lead + len(phase)] += 1000 * np.exp(
                1j * (phase + 2 * np.pi * offset * t[lead : lead + len(phase)])
            )
        rng = np.random.default_rng(3)
        wide += rng.normal(0, 10, count) + 1j * rng.normal(0, 10, count)
        i = np.rint(wide.real).astype(np.int32)
        q = np.rint(wide.imag).astype(np.int32)
        raw = (((i & 0x3FFF) << 16) | (q & 0x3FFF)).astype(np.uint32)
        for ch in (37, 38, 39):
            state = C.create_string_buffer(4096)
            self.channel.ble_channel_init(state, ch)
            narrow = []
            for start in range(0, count, 2048):
                out = (C.c_int16 * 256)()
                a = (C.c_uint32 * 2048)(*raw[start : start + 2048])
                n = self.channel.ble_channel_run(state, a, 2048, out)
                narrow.extend(out[: 2 * n])
            for packed in (False, True):
                got, _ = self.decode(narrow, ch=ch, chunk=2048, packed=packed)
                self.assertEqual(got, [SPEC_PDU], f"channel {ch}, packed={packed}")


if __name__ == "__main__":
    unittest.main()
