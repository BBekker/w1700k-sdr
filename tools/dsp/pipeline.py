#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Own-firmware ping-pong DSP snapshot producer with DMA on both sides.

Requires a fresh load.py, idle capture, and no stream pump. Raw IQ and powers
are never read through the host control mailbox. The native reader consumes
DMA packets, checks every sequence, and publishes display snapshots at 20 Hz.
This pipelines all three DSPs but does not promise continuous RF coverage.
"""
import argparse
import cmath
import json
import math
from pathlib import Path
import signal
import struct
import subprocess
import time
from live import ROUTE
from registers import healthy
from run import read,write,packed,unpack
from w1700k import config
from w1700k.controller import Controller
from w1700k.unit import Unit
from w1700k.wm import Wm

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',required=True)
    p.add_argument('--helper',type=Path,required=True)
    p.add_argument('--reader',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--seconds',type=float,default=86400)
    args=p.parse_args()
    if not math.isfinite(args.seconds) or not 0<args.seconds<=86400:
        p.error('seconds must be in (0, 86400]')
    cfg=config.load(args.config)
    if cfg.unit.transport!='local':p.error('native DMA reader requires router-local transport')
    c=Controller(cfg)
    try:
        c.attach();state=c.state()
        if not state['radio']['loaded'] or state['radio']['band']!=0 or state['radio']['sample_rate']!=40000000:
            raise RuntimeError('load research DSP firmware and configure idle band 0 at 40 Msps')
        center=state['rf']['lo_hz']
    finally:c._close_ctl()
    code=args.helper.read_bytes()
    if not 0<len(code)<=0x4000:p.error('helper exceeds scratch reservation')
    code+=bytes(-len(code)%4)
    tw=[cmath.exp(-2j*math.pi*k/2048) for k in range(1024)]
    tw=unpack(struct.pack('<2048f',*[v for z in tw for v in (z.real,z.imag)]))
    child=None;stopping=False
    def stop(*unused):
        nonlocal stopping
        stopping=True
        if child is not None:child.terminate()
    signal.signal(signal.SIGTERM,stop);signal.signal(signal.SIGINT,stop)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with Wm(Unit(cfg.unit)) as w:
        healthy(w)
        if w.session.streaming() or any(read(w,a,1)[0]&1 for a in (0x80023090,0x80024090,0x80025090)):
            raise RuntimeError('capture or stream already active')
        if read(w,0x81020010,2)!=[7,0] or read(w,0x88009004,1)[0]:
            raise RuntimeError('DSPs disabled/reset or pool already granted')
        for b in range(3):
            box=read(w,0x86200000+(b<<22),11)
            if box[0]!=0x44535031 or box[1]!=box[2] or box[10]!=1:
                raise RuntimeError(f'DSP{b} not ready')
            write(w,0x86209000+(b<<22),tw)
        write(w,0x00912000,unpack(code))
        if packed(read(w,0x00912000,len(code)//4))!=code:raise RuntimeError('helper verification failed')
        saved=[(a,read(w,a,1)[0]) for a,_ in ROUTE];cr0=read(w,0x80023090,1)[0]
        traps=w.traps()
        if stopping:return
        w.write(ROUTE);w.session.ctl([(0x0090f008,0)])
        before=w.start(0x00912000,0,7)
        began=time.monotonic()
        try:
            child=subprocess.Popen([str(args.reader),'/dev/w1700k-sdr',str(args.output),str(args.seconds),str(center),'40000000'],stdout=subprocess.PIPE,text=True)
            if stopping:child.terminate()
            stdout,_=child.communicate()
        finally:
            # This is a direct SRAM write, safe while WM is in the helper.
            w.session.ctl([(0x0090f008,0x53544f50)])
            deadline=time.monotonic()+3
            while w.status()['calls']==before:
                if time.monotonic()>deadline:raise RuntimeError('pipeline failed to retire; firmware reload required')
                time.sleep(.01)
            status=healthy(w)
            stats=w.session.ctl(read=(0x0090d000,32))[0]
            w.write([(0x80023090,0x400),(0x88009004,0)]+saved+[(0x80023090,cr0)])
        result=json.loads(stdout)
        result.update(wm_status=stats[1],captured=stats[3],computed=stats[4],delivered=stats[5],
                      elapsed_seconds=time.monotonic()-began,continuous_rf=False)
        print(json.dumps(result),flush=True)
        if child.returncode or stats[1] or status['traps']!=traps or status['ret'] or not stats[3]==stats[4]==stats[5]==result['fft_frames']:
            raise RuntimeError('pipeline stopped with an error or missing result')

if __name__=='__main__':main()
