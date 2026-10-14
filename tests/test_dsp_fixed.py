# SPDX-License-Identifier: MIT
"""Fixed arithmetic's error against a direct DFT, not the float FFT."""
import cmath
import ctypes
import math
from pathlib import Path
import random
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
class DspFixedTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.tmp.cleanup)
        path=Path(cls.tmp.name)/'fixed.so'
        subprocess.run(['cc','-O3','-shared','-fPIC',str(ROOT/'tools/dsp/fixed.c'),
                        str(ROOT/'tools/dsp/mac.c'),'-o',str(path)],check=True)
        cls.lib=ctypes.CDLL(str(path))
        cls.lib.dsp_fixed_spectrum.argtypes=[ctypes.POINTER(ctypes.c_uint32),ctypes.POINTER(ctypes.c_int32),
            ctypes.POINTER(ctypes.c_float),ctypes.c_uint,ctypes.POINTER(ctypes.c_float),ctypes.POINTER(ctypes.c_int32)]
    def transform(self,samples):
        n=len(samples)
        words=(ctypes.c_uint32*n)(*[((int(z.real)&0x3fff)<<16)|(int(z.imag)&0x3fff) for z in samples])
        x=(ctypes.c_int32*(2*n))()
        tw=[cmath.exp(-2j*math.pi*k/n) for k in range(n//2)]
        table=(ctypes.c_float*n)(*[v for z in tw for v in (z.real,z.imag)])
        power=(ctypes.c_float*n)()
        qtw=(ctypes.c_int32*n)()
        self.lib.dsp_fixed_spectrum(words,x,table,n,power,qtw)
        return list(power)
    def test_mac_reference_cases(self):
        self.assertEqual(self.lib.dsp_mac_check(),0)
    def test_dc(self):
        for z in (0j,8191-8192j,-8192+8191j):
            self.assertTrue(all(v==0 for v in self.transform([z]*2048)))
    def test_tones_and_noise_against_dft(self):
        n=2048; rng=random.Random(7996)
        signals=[[complex(round(amp*math.cos(2*math.pi*bin*t/n)),round(amp*math.sin(2*math.pi*bin*t/n)))
                  for t in range(n)] for amp,bin in ((4096,8),(4096,n-7),(16,103))]
        signals.append([complex(rng.randrange(-8192,8192),rng.randrange(-8192,8192)) for _ in range(n)])
        for samples in signals:
            power=self.transform(samples)
            mean=sum(samples)/n
            windowed=[(z-mean)*(.5-.5*math.cos(2*math.pi*t/n)) for t,z in enumerate(samples)]
            bins={0,1,7,8,9,102,103,104,n//2,n-8,n-7,n-6,n-1}
            for k in bins:
                ref=abs(sum(z*cmath.exp(-2j*math.pi*k*t/n) for t,z in enumerate(windowed))/(4096*n))**2
                # The normalized output has a 1/65536 amplitude quantum.
                # Bound bin error in amplitude units before squaring; a
                # relative-power-only bound is meaningless near zero.
                amplitude_error=4/65536
                delta=2*math.sqrt(ref)*amplitude_error+amplitude_error**2+ref*0.001
                self.assertAlmostEqual(power[k],ref,delta=delta,msg=f'bin {k}')
