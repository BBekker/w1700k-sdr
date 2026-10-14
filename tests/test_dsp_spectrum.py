# SPDX-License-Identifier: MIT
"""Validate the actual DSP window/unpack/power code against independent DFT sums."""
import cmath
import ctypes
import math
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
class DspSpectrumTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        lib=Path(cls.tmp.name)/'spectrum.so'
        subprocess.run(['cc','-O3','-shared','-fPIC',str(ROOT/'tools/dsp/fft.c'),
                        str(ROOT/'tools/dsp/spectrum.c'),'-o',str(lib)],check=True)
        cls.lib=ctypes.CDLL(str(lib))
        cls.lib.dsp_spectrum.argtypes=[ctypes.POINTER(ctypes.c_uint32),ctypes.POINTER(ctypes.c_float),
            ctypes.POINTER(ctypes.c_float),ctypes.c_uint,ctypes.POINTER(ctypes.c_float)]
    def check_signal(self, x):
        n=len(x)
        x=[complex(round(z.real),round(z.imag)) for z in x]
        raw=(ctypes.c_uint32*n)(*[((int(z.real)&0x3fff)<<16)|(int(z.imag)&0x3fff) for z in x])
        data=(ctypes.c_float*(2*n))()
        tw=[cmath.exp(-2j*math.pi*k/n) for k in range(n//2)]
        table=(ctypes.c_float*n)(*[v for z in tw for v in (z.real,z.imag)])
        power=(ctypes.c_float*n)()
        self.lib.dsp_spectrum(raw,data,table,n,power)
        mean=sum(x)/n
        samples=[(z-mean)*(.5-.5*math.cos(2*math.pi*t/n)) for t,z in enumerate(x)]
        for k in (0,1,2,3,7,8,9,17,n//2,n-7,n-1):
            ref=abs(sum(z*cmath.exp(-2j*math.pi*k*t/n) for t,z in enumerate(samples))/(8192*n/2))**2
            self.assertAlmostEqual(power[k],ref,delta=max(1e-12,ref*1e-4),msg=f'bin {k}')
        return power
    def test_dc_removed(self):
        self.assertTrue(all(p==0 for p in self.check_signal([1200-300j]*2048)))
    def test_hann_tone_scale_and_order(self):
        p=self.check_signal([4096*cmath.exp(2j*math.pi*8*t/2048)+100-200j for t in range(2048)])
        self.assertAlmostEqual(p[8],.25,delta=1e-5)
        self.assertAlmostEqual(p[7],.0625,delta=1e-5)
    def test_received_format_random(self):
        r=random.Random(7996)
        self.check_signal([complex(r.randrange(-8192,8192),r.randrange(-8192,8192)) for _ in range(2048)])
