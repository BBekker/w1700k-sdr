#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare guarded DSP reads of known patterns in the stopped capture window.

Requires load.py's running DSP image and all capture engines disabled. Only
0x00464000 and +4 are touched; original contents are restored. Passing the
experiment means collecting responses, not establishing RAM connectivity.
"""
import argparse
import json
from pathlib import Path
from run import read, write, request
from registers import healthy, emit
from w1700k import config
from w1700k.unit import Unit
from w1700k.wm import Wm


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    results = []
    with Wm(Unit(config.load(args.config).unit)) as w:
        healthy(w)
        if w.session.streaming():
            raise RuntimeError('stream active')
        for a in (0x80023090, 0x80024090, 0x80025090):
            v = read(w, a, 1)[0]
            emit(event='capture_control', address=hex(a), value=hex(v))
            if v & 1:
                raise RuntimeError('capture engine enabled')
        if read(w, 0x88009004, 1)[0] & 0xff000000:
            raise RuntimeError('window banks still granted')
        if read(w, 0x81020010, 2) != [7, 0]:
            raise RuntimeError('DSP cores not enabled or held in reset')
        if read(w, 0x86200000, 1)[0] != 0x44535031:
            raise RuntimeError('research DSP not loaded')
        address = 0x00464000
        saved = read(w, address, 2)
        try:
            for pattern in ((0x13579bdf, 0x2468ace0), (0xcafebabe, 0x31415926)):
                write(w, address, list(pattern))
                if read(w, address, 2) != list(pattern):
                    raise RuntimeError('WM pattern readback differs')
                for mapped in (0, 1):
                    for i in range(2):
                        a = address+4*i
                        emit(event='before_probe', address=hex(a), mapped=mapped)
                        box = request(w, 0x86200000, 2, address=a, map_window=mapped)
                        result = {'event': 'capture_ram_probe', 'address': hex(a),
                                  'mapped': mapped, 'expected': pattern[i], 'mailbox': box}
                        results.append(result)
                        emit(**result)
        finally:
            args.out.write_text(json.dumps(results, indent=2)+'\n')
            healthy(w)
            write(w, address, saved)


if __name__ == '__main__':
    main()
