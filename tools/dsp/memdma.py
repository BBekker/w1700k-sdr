#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded MEM_DMA TX0 -> RX0 known-pattern experiment, idle own firmware.

All changes are volatile. Uses documented free ULM1 for descriptors/buffers.
Does not infer success from TX completion: RX completion, length, contents,
and guard words must also agree. On failure, leave allocated scratch intact
and restore normal firmware through RESET_MT before starting the viewer.
"""
import argparse
import json
import time
from pathlib import Path
from run import read, write
from registers import healthy, emit
from w1700k import config
from w1700k.unit import Unit
from w1700k.wm import Wm

BASE = 0x58000000
TX, RX = 0x00910800, 0x00910900
SOURCE = 0x00912000
SCRATCH_DEST, DSP_DEST = 0x00914000, 0x8620d000
N = 8


def snap(w):
    offsets = (0x110, 0x208, 0x308, 0x30c, 0x508, 0x50c)
    return {hex(o): read(w, BASE+o, 1)[0] for o in offsets}


def stop(w):
    cfg = read(w, BASE+0x208, 1)[0]
    w.write([(BASE+0x208, cfg & ~5)])
    deadline = time.monotonic()+.2
    while read(w, BASE+0x208, 1)[0] & 10:
        if time.monotonic() > deadline:
            raise RuntimeError('MEM_DMA did not become idle; keep scratch intact and RESET_MT')
    return cfg & ~10


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True)
    p.add_argument('--destination', choices=('scratch', 'dsp'), default='scratch')
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    dest = DSP_DEST if args.destination == 'dsp' else SCRATCH_DEST
    results = []
    with Wm(Unit(config.load(args.config).unit)) as w:
        healthy(w)
        if w.session.streaming():
            raise RuntimeError('stop viewer and stream first')
        if args.destination == 'dsp':
            if read(w, 0x81020010, 2) != [7, 0] or read(w, 0x86200000, 1)[0] != 0x44535031:
                raise RuntimeError('research DSP not ready')
        initial = snap(w)
        emit(event='memdma_initial', registers=initial)
        if initial['0x308'] != initial['0x30c'] or initial['0x208'] & 10:
            raise RuntimeError('MEM_DMA already busy')
        original_cfg = stop(w)
        try:
            # Existing ROM prefetch assignment stays untouched (TX0 base 0x80,
            # RX0 base 0x220, depth 2). No allocator or vendor reclaim calls.
            for trial, seed in enumerate((0x13579bdf, 0xcafebabe)):
                stop(w)
                pattern = [(seed ^ (i*0x9e3779b9)) & 0xffffffff for i in range(16)]
                write(w, SOURCE, pattern)
                write(w, dest-4, [0x5a5a5a5a] + [0xa5a5a5a5]*32 + [0x96969696])
                tx, rx = [], []
                for i in range(N):
                    tx += [SOURCE, (0x40000000 | (64 << 16)) if i == 0 else 0x80000000, 0, 0]
                    rx += [dest, 128 << 16, 0, 0]
                write(w, TX, tx); write(w, RX, rx)
                w.write([(BASE+0x300, TX), (BASE+0x304, N), (BASE+0x308, 0),
                         (BASE+0x500, RX), (BASE+0x504, N), (BASE+0x508, 0),
                         (BASE+0x20c, 1), (BASE+0x280, 1)])
                emit(event='memdma_bound', registers=snap(w))
                if read(w, BASE+0x30c, 1)[0] or read(w, BASE+0x50c, 1)[0]:
                    raise RuntimeError('DMA indices did not reset')
                # Publish receive capacity before offering one TX descriptor.
                w.write([(BASE+0x508, N-1), (BASE+0x208, original_cfg | 5)])
                emit(event='memdma_publish', trial=trial, source=hex(SOURCE), destination=hex(dest), bytes=64)
                w.write([(BASE+0x308, 1)])
                deadline = time.monotonic()+.2
                state = snap(w)
                while (state['0x30c'] != 1 or state['0x50c'] != 1 or
                       not read(w, RX+4, 1)[0] & 0x80000000) and time.monotonic() < deadline:
                    state = snap(w)
                stopped_cfg = stop(w)
                received = read(w, dest, 32)
                guards = [read(w, dest-4, 1)[0], read(w, dest+128, 1)[0]]
                desc = read(w, RX, 4)
                success = (state['0x30c'] == state['0x50c'] == 1 and
                           (desc[1] & 0x80000000) and ((desc[1] >> 16) & 0x3fff) == 64 and
                           received[:16] == pattern and received[16:] == [0xa5a5a5a5]*16 and
                           guards == [0x5a5a5a5a, 0x96969696])
                record = dict(event='memdma_result', trial=trial, success=bool(success),
                              registers=state, stopped_cfg=stopped_cfg, rx_desc=desc,
                              expected=pattern, received=received, guards=guards)
                results.append(record); emit(**record)
                if not success:
                    raise RuntimeError('DMA transfer not verified; no further descriptor submitted')
        finally:
            args.out.write_text(json.dumps(results, indent=2)+'\n')
            # Leave test queues stopped and memory reserved. A full normal
            # reload restores the ROM ring topology, including on failure.
            healthy(w)
            stop(w)


if __name__ == '__main__':
    main()
