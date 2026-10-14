#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""One received 2048-sample FFT on DSP0, using WM copy or MEM_DMA.

Requires load.py's running image, band 0 configured, viewer and stream stopped.
Uses a stopped ICAP window or pool; no transmission, flash writes or continuous-stream
claim. MEM_DMA moves payload without either CPU copying the samples.
Raw input is also read back afterwards solely for independent validation.
"""
import argparse
import cmath
import json
import math
from pathlib import Path
import struct
import time
from run import read, write, request, unpack, packed
from registers import healthy, emit
from w1700k import config
from w1700k.controller import Controller
from w1700k.unit import Unit
from w1700k.wm import Wm


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True)
    transport = p.add_mutually_exclusive_group(required=True)
    transport.add_argument('--copy-image', type=Path)
    transport.add_argument('--dma-helper', type=Path)
    p.add_argument('--pool', action='store_true', help='capture into the normal streaming pool (requires DMA)')
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    if args.pool and not args.dma_helper:
        p.error('--pool requires --dma-helper')
    code = (args.copy_image or args.dma_helper).read_bytes()
    if not 0 < len(code) <= 2048:
        p.error('WM helper must fit before the DMA descriptor reservation')
    code += bytes(-len(code) % 4)
    n, dtcm, scratch = 2048, 0x86200000, 0x00910000
    cr0, grant = 0x80023090, 0x88009004
    # Only documented route / capture configuration words.
    changes = [(0x88009004, 0xff000000), (0x8800900c, 0),
               (0x88009024, 0x02000000), (0x88009028, 0x400), (0x8800902c, 0),
               (0x80023098, 0), (0x8002309c, 0xffc), (0x800230a4, 1024),
               (0x800230b0, 0), (0x800230b8, 0)]
    if args.pool:
        changes = [(0x88009004, 0xff000000), (0x8800900c, 0x02249000),
                   (0x88009018, 0x30001000), (0x8800901c, 0x70005000),
                   (0x88009020, 0x1000), (0x88009024, 0x02000000),
                   (0x88009028, 0x0003c001), (0x8800902c, 0x180),
                   (0x80023098, 0), (0x8002309c, 0x7fc), (0x800230a4, 512),
                   (0x800230b0, 0), (0x800230b8, 0x10000)]
    cfg = config.load(args.config)
    controller = Controller(cfg)
    try:
        controller.attach()
        state = controller.state()
        if not state['radio']['loaded'] or state['radio']['band'] != 0:
            raise RuntimeError('load and configure band 0 first')
        sample_rate = state['radio']['sample_rate']
        center_hz = state['rf']['lo_hz']
    finally:
        controller._close_ctl()
    with Wm(Unit(cfg.unit)) as w:
        healthy(w)
        if w.session.streaming():
            raise RuntimeError('stream active')
        if read(w, dtcm, 1)[0] != 0x44535031 or read(w, dtcm+40, 1)[0] != 1:
            raise RuntimeError('research DSP not ready')
        for a in (cr0, 0x80024090, 0x80025090):
            if read(w, a, 1)[0] & 1:
                raise RuntimeError('capture engine enabled')
        if read(w, grant, 1)[0] != 0:
            raise RuntimeError('window banks already granted')
        saved = [(a, read(w, a, 1)[0]) for a, _ in changes]
        saved_cr0 = read(w, cr0, 1)[0]
        old_code = read(w, scratch, len(code)//4)
        try:
            # ULM1 is non-cacheable; use fresh scratch after load.py, before
            # this address has ever executed. Do not patch a live code image.
            write(w, scratch, unpack(code))
            if packed(read(w, scratch, len(code)//4)) != code:
                raise RuntimeError('WM helper readback mismatch')
            tw = [cmath.exp(-2j*math.pi*k/n) for k in range(n//2)]
            write(w, dtcm+0x9000, unpack(struct.pack('<'+'f'*n,
                  *[v for z in tw for v in (z.real, z.imag)])))
            w.write([(cr0, 0), (cr0, 0x400)] + changes)
            emit(event='receive_capture_start', n=n)
            w.write([(cr0, 0x401), (cr0, 0x403)])
            deadline = time.monotonic()+1
            while read(w, cr0, 1)[0] & 2:
                if time.monotonic() > deadline:
                    raise RuntimeError('receive capture did not complete')
            w.write([(cr0, 0x400), (grant, 0)])
            dma_stats = []
            if args.dma_helper:
                from dma_bench import helper
                sources = [(0xe0400000, dtcm+0xd000, 8192)] if args.pool else [
                    (0x00464000, dtcm+0xd000, 4096), (0x00466000, dtcm+0xe000, 4096)]
                for src, dst, size in sources:
                    dma_stats.append(helper(w, src, dst, size, 1))
                cycles = sum(s['sum'] for s in dma_stats)
                emit(event='capture_dma', bytes=n*4, cycles=cycles, us_at_720mhz=cycles/720)
            else:
                before = w.traps()
                cycles = w.call(scratch)
                healthy(w)
                if w.traps() != before:
                    raise RuntimeError('WM copy trapped')
                emit(event='wm_copy', bytes=n*4, cycles=cycles, us_at_720mhz=cycles/720)
            box = request(w, dtcm, 3, n=n)
            if box['cause'] or box['result']:
                raise RuntimeError(f'DSP raw FFT failed: {box}')
            # Validation-only reads happen AFTER the DSP has completed.
            raw = read(w, 0xe0400000, n) if args.pool else (
                read(w, 0x00464000, 1024) + read(w, 0x00466000, 1024))
            copied = read(w, dtcm+0xd000, n)
            if raw != copied:
                raise RuntimeError('DSP input differs from the captured window')
            if any(v & 0xc000c000 for v in raw) or len(set(raw)) < 8:
                raise RuntimeError('capture is not varying 14-bit IQ')
            values = struct.unpack('<'+'f'*(2*n), packed(read(w, dtcm+0x1000, 2*n)))
            if not all(math.isfinite(v) for v in values):
                raise RuntimeError('non-finite FFT output')
            result = {'n': n, 'sample_rate': sample_rate, 'center_hz': center_hz, 'transfer_cycles': cycles,
                      'transfer_clock_assumed_hz': 720000000, 'dsp_mailbox': box,
                      'dsp_clock_assumed_hz': 640000000,
                      'continuous': False, 'transfer': 'MEM_DMA' if args.dma_helper else 'WM scalar copy',
                      'capture_source': 'pool' if args.pool else 'window',
                      'dma_stats': dma_stats,
                      'raw_iq_words': raw, 'fft_interleaved': list(values)}
            args.out.write_text(json.dumps(result)+'\n')
            emit(event='received_fft_saved', output=str(args.out), fft_cycles=box['cycles'],
                 independent_validation_pending=True)
        finally:
            healthy(w)
            w.write([(cr0, 0x400), (grant, 0)])
            w.write(saved + [(cr0, saved_cr0)])
            write(w, scratch, old_code)


if __name__ == '__main__':
    main()
