#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate a capture.py result against direct DFT sums, without NumPy."""
import argparse
import cmath
import hashlib
import json
import math
from pathlib import Path


def verify(data):
    n = data['n']
    words, values = data['raw_iq_words'], data['fft_interleaved']
    if n != 2048 or len(words) != n or len(values) != 2*n:
        raise ValueError('expected 2048 captured samples and FFT outputs')
    if any(w & 0xc000c000 for w in words) or len(set(words)) < 8:
        raise ValueError('invalid or constant raw IQ')
    if not all(math.isfinite(v) for v in values):
        raise ValueError('non-finite FFT output')
    def signed(v):
        return ((v & 0x3fff) ^ 0x2000) - 0x2000
    x = [complex(signed(w >> 16), signed(w)) for w in words]
    y = [complex(values[2*k], values[2*k+1]) for k in range(n)]
    errors, ref_energy = [], 0.0
    for k in range(n):
        ref = sum(z*cmath.exp(-2j*math.pi*k*t/n) for t, z in enumerate(x))
        errors.append(abs(y[k]-ref))
        ref_energy += abs(ref)**2
    rel_l2 = math.sqrt(sum(e*e for e in errors)/ref_energy)
    peak_scaled = max(errors)/math.sqrt(ref_energy/n)
    if rel_l2 > 2e-6 or peak_scaled > 2e-5:
        raise ValueError(f'FFT differs from direct DFT: relative L2 {rel_l2}, peak scaled {peak_scaled}')
    cycles = data['transfer_cycles'] if 'transfer_cycles' in data else data['copy_cycles']
    clock = data.get('transfer_clock_assumed_hz', data.get('copy_clock_assumed_hz'))
    transfer_us = cycles/clock*1e6
    dsp_us = data['dsp_mailbox']['cycles']/data['dsp_clock_assumed_hz']*1e6
    return {'n': n, 'reference': 'all bins, direct DFT sums (complex128)',
            'relative_l2_error': rel_l2, 'max_absolute_error': max(errors),
            'peak_error_over_rms_bin': peak_scaled, 'unique_raw_words': len(set(words)),
            'transfer': data['transfer'], 'transfer_bytes': n*4,
            'transfer_cycles': cycles, 'transfer_us': transfer_us,
            'transfer_MBps': n*4/transfer_us,
            'dsp_unpack_and_fft_cycles': data['dsp_mailbox']['cycles'],
            'dsp_unpack_and_fft_us': dsp_us, 'clock_rates_assumed': True,
            'continuous': False, 'passed': True}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', type=Path)
    p.add_argument('--out', type=Path)
    args = p.parse_args()
    raw = args.input.read_bytes()
    result = verify(json.loads(raw))
    result['capture_file_sha256'] = hashlib.sha256(raw).hexdigest()
    text = json.dumps(result, indent=2)+'\n'
    if args.out:
        args.out.write_text(text)
    print(text, end='')


if __name__ == '__main__':
    main()
