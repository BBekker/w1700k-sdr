#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Run the RAM-only DSP experiment on an idle own-firmware radio.

Requires viewer/capture stopped and this image already loaded and running.
Does not attempt a direct TCM load or core restart. No flash writes.
Hardware failures can still require a physical power cycle.
"""
import argparse
import cmath
import json
import math
from pathlib import Path
import struct
import time
from w1700k import config, rf
from w1700k.unit import Unit
from w1700k.wm import Wm
from registers import healthy, emit

ENABLE, RESET = 0x81020010, 0x81020014
FIELDS = ('magic request ack op n cycles cause epc tval result state address '
          'map_window windows base5 limit5 misa window_after').split()


def read(w, address, count):
    out = []
    for offset in range(0, count, 128):
        healthy(w)
        out += w.run([rf.r32(address+4*i) for i in range(offset, min(count, offset+128))])
    return out


def write(w, address, words):
    for offset in range(0, len(words), 128):
        healthy(w)
        w.write([(address+4*i, words[i]) for i in range(offset, min(len(words), offset+128))])


def unpack(data):
    return list(struct.unpack('<'+'I'*(len(data)//4), data))


def packed(words):
    return struct.pack('<'+'I'*len(words), *words)


def request(w, dtcm, op, **kw):
    old = read(w, dtcm, len(FIELDS))
    req = (old[1]+1) & 0xffffffff or 1
    updates = {'op': op, **kw}
    for field, value in updates.items():
        write(w, dtcm+4*FIELDS.index(field), [value])
    write(w, dtcm+4, [req])
    deadline = time.monotonic()+2
    while True:
        box = dict(zip(FIELDS, read(w, dtcm, len(FIELDS))))
        if box['ack'] == req:
            return box
        if box['state'] == 0xbad00001 or time.monotonic() > deadline:
            raise RuntimeError(f'DSP did not complete: {box}')
        time.sleep(.01)


def fft_test(w, dtcm, n):
    # Two complex tones; exact expected DFT is independent of our FFT code.
    data = [cmath.exp(2j*math.pi*7*k/n) + .25*cmath.exp(-2j*math.pi*13*k/n) for k in range(n)]
    tw = [cmath.exp(-2j*math.pi*k/n) for k in range(n//2)]
    def floats(z):
        return struct.pack('<'+'f'*(2*len(z)), *[v for x in z for v in (x.real, x.imag)])
    write(w, dtcm+0x1000, unpack(floats(data)))
    write(w, dtcm+0x9000, unpack(floats(tw)))
    box = request(w, dtcm, 1, n=n)
    if box['cause'] or box['result']:
        raise RuntimeError(f'FFT exception: {box}')
    raw = packed(read(w, dtcm+0x1000, 2*n))
    values = struct.unpack('<'+'f'*(2*n), raw)
    if not all(math.isfinite(v) for v in values):
        raise RuntimeError('FFT contains NaN or infinity')
    error = max(abs(complex(values[2*k], values[2*k+1]) -
                    (n if k == 7 else n*.25 if k == n-13 else 0)) for k in range(n))
    if not math.isfinite(error) or error > n*2e-6:
        raise RuntimeError(f'FFT mismatch: n={n}, max error={error}')
    return {'event': 'fft', 'n': n, 'cycles': box['cycles'], 'max_error': error,
            'compute_us_at_640mhz': box['cycles']/640, 'transfer_included': False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True)
    p.add_argument('--image', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--band', type=int, choices=range(3), default=0)
    p.add_argument('--probe-ram', action='store_true')
    args = p.parse_args()
    code = args.image.read_bytes()
    if not 0 < len(code) <= 65536:
        p.error('image must fit 64 KiB ITCM')
    code += bytes(-len(code) % 4)
    args.out.mkdir(parents=True, exist_ok=True)
    itcm = 0x86000000 + (args.band << 22)
    dtcm = itcm + 0x200000
    bit = 1 << args.band
    results = []
    with Wm(Unit(config.load(args.config).unit)) as w:
        first = healthy(w)
        time.sleep(.02)
        if healthy(w)['beats'] == first['beats'] or w.session.streaming():
            raise RuntimeError('stop capture and viewer before running the experiment')
        enabled, reset = read(w, ENABLE, 2)
        if not enabled & bit or reset & bit:
            raise RuntimeError('DSP must already be loaded and running; TCM access while reset was held timed out')
        try:
            emit(event='verify_running_image', band=args.band)
            if packed(read(w, itcm, len(code)//4)) != code:
                raise RuntimeError('running code differs from the supplied image')
            deadline = time.monotonic()+2
            while True:
                box = dict(zip(FIELDS, read(w, dtcm, len(FIELDS))))
                if box['magic'] == 0x44535031 and box['state'] == 1:
                    break
                if time.monotonic() > deadline:
                    raise RuntimeError(f'DSP startup failed: {box}')
                time.sleep(.01)
            emit(event='dsp_ready', mailbox=box)
            for n in (512, 1024, 2048, 4096):
                result = fft_test(w, dtcm, n)
                results.append(result); emit(**result)
            if args.probe_ram:
                # This documented free ULM1 scratch area is unused with capture stopped.
                address = 0x00910000
                saved = read(w, address, 2)
                try:
                    for pattern in ((0x13579bdf, 0x2468ace0), (0xcafebabe, 0x31415926)):
                        write(w, address, list(pattern))
                        if read(w, address, 2) != list(pattern):
                            raise RuntimeError('WM pattern readback differs')
                        for mapped in (0, 1):
                            for i in range(2):
                                box = request(w, dtcm, 2, address=address+4*i, map_window=mapped)
                                result = {'event': 'ram_probe', 'address': hex(address+4*i),
                                          'mapped': mapped, 'expected': pattern[i], 'mailbox': box}
                                results.append(result); emit(**result)
                finally:
                    healthy(w)
                    write(w, address, saved)
        finally:
            (args.out/'results.json').write_text(json.dumps(results, indent=2)+'\n')
            # Core stays running for further research. Restore production firmware
            # through the normal loader before restarting the viewer.
            emit(event='experiment_finished', core_left_running=True)



if __name__ == '__main__':
    main()
