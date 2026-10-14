# SPDX-License-Identifier: MIT
"""Numerical tests of the portable DSP FFT, independent of radio hardware.

Uses direct DFT sums, not a second copy of the FFT algorithm. These tests
validate the arithmetic only; they do not validate the DSP startup or bus.
"""
import cmath
import ctypes
import math
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DspFftTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = shutil.which('cc')
        if not cc:
            raise unittest.SkipTest('native C compiler unavailable')
        cls.tmp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        output = Path(cls.tmp.name) / 'fft.so'
        subprocess.run([cc, '-O3', '-shared', '-fPIC', str(ROOT / 'tools/dsp/fft.c'),
                        '-o', str(output)], check=True, capture_output=True)
        cls.lib = ctypes.CDLL(str(output))
        cls.lib.dsp_fft.argtypes = [ctypes.POINTER(ctypes.c_float),
                                   ctypes.POINTER(ctypes.c_float), ctypes.c_uint]
        cls.lib.dsp_fft.restype = None

    def transform(self, x):
        n = len(x)
        data = (ctypes.c_float * (2*n))(*[v for z in x for v in (z.real, z.imag)])
        tw = [cmath.exp(-2j*math.pi*k/n) for k in range(n//2)]
        table = (ctypes.c_float * n)(*[v for z in tw for v in (z.real, z.imag)])
        self.lib.dsp_fft(data, table, n)
        return [complex(data[2*k], data[2*k+1]) for k in range(n)]

    def test_impulse_and_dc_all_sizes(self):
        for n in (2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096):
            with self.subTest(n=n):
                impulse = self.transform([1+0j] + [0j]*(n-1))
                self.assertTrue(all(z == 1 for z in impulse))
                dc = self.transform([1+0j]*n)
                self.assertEqual(dc[0], n)
                self.assertTrue(all(z == 0 for z in dc[1:]))

    def test_complex_tone_sign_order_and_scaling(self):
        for n in (16, 512, 4096):
            for bin_index in (3, n-3):
                with self.subTest(n=n, bin=bin_index):
                    result = self.transform([cmath.exp(2j*math.pi*bin_index*k/n) for k in range(n)])
                    for k, z in enumerate(result):
                        self.assertLess(abs(z-(n if k == bin_index else 0)), n*2e-6)

    def test_random_against_direct_dft(self):
        rng = random.Random(7996)
        for n in (2, 4, 8, 16, 32, 64, 128, 512, 2048, 4096):
            x = [complex(rng.uniform(-1, 1), rng.uniform(-1, 1)) for _ in range(n)]
            result = self.transform(x)
            bins = range(n) if n <= 128 else (0, 1, 3, 7, n//4, n//2, n//2+1, n-7, n-1)
            for k in bins:
                with self.subTest(n=n, bin=k):
                    reference = sum(z*cmath.exp(-2j*math.pi*k*t/n) for t, z in enumerate(x))
                    self.assertLess(abs(result[k]-reference), 3e-6*math.sqrt(n))
