#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""DSP0 snapshot spectrum producer; requires a fresh load.py and idle band 0.

Writes an atomic latest-frame file for w1700k-spectrum --dsp-input. Raw samples
never leave the radio except when --validate explicitly records a test capture.
Fixed 2048-point periodic Hann, DC removed, one block per frame, no continuous
coverage claim. This process owns capture and the WM scratch/output reservations.
"""
import argparse
import cmath
import array
import fcntl
import sys
import json
import math
import os
from pathlib import Path
import signal
import struct
import time
from run import read, write, packed, unpack
from registers import healthy
from w1700k import config
from w1700k.controller import Controller
from w1700k.unit import Unit
from w1700k.wm import Wm, STATUS, MAGIC, STATE_RUNNING, CALL

# Local Linux fast path: existing READ_MEM ioctl, 4096 bytes per call. Unlike
# the word-at-a-time CTL path it selects the PCIe window once per page, not
# once per word. No driver modification, raw IQ read, or /dev/mem mapping.
class PowerReader:
    def __init__(self, local):
        self.fd = os.open('/dev/w1700k-sdr', os.O_RDWR | os.O_CLOEXEC) if local else None
    def close(self):
        if self.fd is not None: os.close(self.fd)
    def memory(self, address, size):
        data=array.array('B',bytes(size))
        address_in_process=data.buffer_info()[0]
        for offset in range(0,size,4096):
            request=struct.pack('<IIQII',address+offset,min(4096,size-offset),
                                address_in_process+offset,0,0)
            fcntl.ioctl(self.fd,0x40185117,request)
        return bytes(data)
    def capture(self, w, traps):
        if self.fd is None:
            result=w.call(0x00912000,timeout=2)
            healthy(w)
            if result or w.traps()!=traps:
                raise RuntimeError(f'capture/DMA/DSP failed: {result}')
            return
        def status():
            st=struct.unpack('<23I',self.memory(STATUS,92))
            if st[0]!=MAGIC or st[3]!=STATE_RUNNING or st[15]!=traps:
                raise RuntimeError('WM stopped or trapped during capture')
            return st
        before=status()[17]
        buf=bytearray(800)
        struct.pack_into('<4I',buf,0,1,0,0,2000)
        struct.pack_into('<I',buf,16,CALL)
        struct.pack_into('<I',buf,272,0x00912000)
        fcntl.ioctl(self.fd,0xc3205120,buf,True)
        result,via_pump=struct.unpack_from('<iI',buf,784)
        if result or via_pump:
            raise RuntimeError('capture command failed or stream pump took ownership')
        deadline=time.monotonic()+2
        while True:
            st=status()
            if st[17]!=before:
                if st[22]: raise RuntimeError(f'capture/DMA/DSP failed: {st[22]}')
                return
            if time.monotonic()>deadline:
                raise RuntimeError('capture timed out')
    def metadata(self,w):
        values=(struct.unpack('<4I',self.memory(0x0090d000,16)) if self.fd is not None
                else w.session.ctl(read=(0x0090d000,4))[0])
        if values[0]!=0x31504644:
            raise RuntimeError('helper metadata missing; reload the matching wm-live.bin')
        return values[2],values[3]
    def read(self, w):
        if self.fd is None:
            return packed([v for offset in range(0,2048,64)
                           for v in w.session.ctl(read=(0x0090b000+offset*4,64))[0]])
        return self.memory(0x0090b000,8192)

HEADER = struct.Struct('<8sIIQddIIQ')
ROUTE = [(0x88009004, 0), (0x8800900c, 0x02249000),
         (0x88009018, 0x30001000), (0x8800901c, 0x70005000),
         (0x88009020, 0x1000), (0x88009024, 0x02000000),
         (0x88009028, 0x0003c001), (0x8800902c, 0x180),
         (0x80023098, 0), (0x8002309c, 0x7fc), (0x800230a4, 512),
         (0x800230b0, 0), (0x800230b8, 0x10000)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True)
    p.add_argument('--helper', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--rate', type=float, default=0, help='maximum captures/second; 0 runs without pacing')
    p.add_argument('--frames', type=int, default=0)
    p.add_argument('--validate', type=Path)
    args = p.parse_args()
    if not math.isfinite(args.rate) or not 0 <= args.rate <= 1000 or args.frames < 0:
        p.error('rate must be 0..1000 (0 = unpaced); frames must be nonnegative')
    cfg = config.load(args.config)
    controller = Controller(cfg)
    try:
        controller.attach()
        state = controller.state()
        if not state['radio']['loaded'] or state['radio']['band'] != 0:
            raise RuntimeError('load and configure band 0 first')
        center, rate = state['rf']['lo_hz'], state['radio']['sample_rate']
        if rate != 40_000_000:
            raise RuntimeError('initial DSP backend requires 40 Msps')
    finally:
        controller._close_ctl()
    code = args.helper.read_bytes()
    if not 0 < len(code) <= 0x4000:
        p.error('helper exceeds scratch reservation')
    code += bytes(-len(code)%4)
    stopping = False
    def stop(*unused):
        nonlocal stopping
        stopping = True
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix('.partial')
    count, start = 0, time.monotonic()
    with Wm(Unit(cfg.unit), sleep=lambda seconds: time.sleep(min(seconds, .001))) as w:
        healthy(w)
        if w.session.streaming():
            raise RuntimeError('stop viewer and stream pump first')
        if read(w, 0x81020010, 2) != [7, 0] or read(w, 0x86200000, 1) != [0x44535031]:
            raise RuntimeError('research DSP not loaded')
        if any(read(w, a, 1)[0]&1 for a in (0x80023090,0x80024090,0x80025090)):
            raise RuntimeError('capture already active')
        if read(w, 0x88009004, 1)[0]:
            raise RuntimeError('capture route already granted')
        write(w, 0x00912000, unpack(code))
        if packed(read(w, 0x00912000, len(code)//4)) != code:
            raise RuntimeError('helper readback mismatch')
        tw = [cmath.exp(-2j*math.pi*k/2048) for k in range(1024)]
        write(w, 0x86209000, unpack(struct.pack('<2048f', *[v for z in tw for v in (z.real,z.imag)])))
        saved = [(a,read(w,a,1)[0]) for a,_ in ROUTE]
        saved_cr0 = read(w,0x80023090,1)[0]
        traps = w.traps()
        reader=PowerReader(cfg.unit.transport == 'local' and sys.platform == 'linux')
        try:
            w.write(ROUTE)
            start=time.monotonic()
            while not stopping and (not args.frames or count < args.frames):
                began = time.monotonic()
                reader.capture(w,traps)
                dsp_cycles,dma_cycles=reader.metadata(w)
                power = reader.read(w)
                count+=1
                header=HEADER.pack(b'DSPFFT1\0',2048,1,count,center,rate,dsp_cycles,dma_cycles,time.time_ns())
                temporary.write_bytes(header+power)
                os.replace(temporary,args.output)
                if args.validate:
                    data={'raw_iq_words':read(w,0x8620d000,2048),
                          'power':list(struct.unpack('<2048f',power)), 'dsp_cycles':dsp_cycles,
                          'dma_cycles':dma_cycles, 'center_hz':center, 'sample_rate':rate}
                    args.validate.write_text(json.dumps(data)+'\n')
                    args.validate=None
                if count%100==0 or args.frames:
                    print(json.dumps({'frames':count,'fps':count/(time.monotonic()-start),
                                      'dsp_us':dsp_cycles/640,'dma_us':dma_cycles/720}),flush=True)
                if args.rate:
                    time.sleep(max(0,1/args.rate-(time.monotonic()-began)))
        finally:
            reader.close()
            healthy(w)
            w.write([(0x80023090,0x400),(0x88009004,0)]+saved+[(0x80023090,saved_cr0)])
            temporary.unlink(missing_ok=True)


if __name__=='__main__':
    main()
