#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compare float and custom-MAC spectra on identical stopped-capture samples.

Requires this DSP image loaded through load.py, with the viewer/producer stopped.
No flash writes. The input is a live.py --validate JSON capture. Save results
under build/: output powers may contain private received data.
"""
import argparse
import cmath
import hashlib
import json
import math
from pathlib import Path
import statistics
import struct
from w1700k import config
from w1700k.unit import Unit
from w1700k.wm import Wm
from registers import healthy
from run import packed, read, request, unpack, write

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',required=True)
    p.add_argument('--capture',type=Path,required=True)
    p.add_argument('--out',type=Path,required=True)
    args=p.parse_args()
    data=args.capture.read_bytes()
    words=json.loads(data)['raw_iq_words']
    if len(words)!=2048 or any(not isinstance(v,int) or not 0<=v<2**32 or v&0xc000c000 for v in words):
        p.error('expected 2048 packed 14-bit IQ words')
    result={'capture_sha256':hashlib.sha256(data).hexdigest(),'dsp_clock_assumed_hz':640000000,'results':{}}
    tw=[cmath.exp(-2j*math.pi*k/2048) for k in range(1024)]
    with Wm(Unit(config.load(args.config).unit)) as w:
        healthy(w)
        if w.session.streaming() or any(read(w,a,1)[0]&1 for a in (0x80023090,0x80024090,0x80025090)):
            raise RuntimeError('stop viewer, capture and stream before benchmarking')
        if read(w,0x81020010,2)!=[7,0] or read(w,0x86200000,1)!=[0x44535031]:
            raise RuntimeError('research DSP not running')
        check=request(w,0x86200000,5)
        if check['cause'] or check['result']:
            raise RuntimeError(f'custom MAC check failed: {check}')
        result['mac_check']=check
        write(w,0x8620d000,words)
        write(w,0x86209000,unpack(struct.pack('<2048f',*[v for z in tw for v in (z.real,z.imag)])))
        for name,op in (('float',4),('custom_mac_fixed',6)):
            cycles=[]
            for _ in range(10):
                box=request(w,0x86200000,op,n=2048)
                if box['cause'] or box['result']:
                    raise RuntimeError(f'{name} failed: {box}')
                cycles.append(box['cycles'])
            power=list(struct.unpack('<2048f',packed(read(w,0x86205000,2048))))
            if any(not math.isfinite(v) or v<0 for v in power):
                raise RuntimeError('invalid power output')
            result['results'][name]={'cycles':cycles,'median_us':statistics.median(cycles)/640,'power':power}
            print(json.dumps({'implementation':name,'median_us':statistics.median(cycles)/640}),flush=True)
    args.out.write_text(json.dumps(result)+'\n')

if __name__=='__main__': main()
