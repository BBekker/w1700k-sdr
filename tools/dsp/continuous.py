#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Bounded continuous capture experiment, not a viewer service.

Stop the viewer first. Always reload through load.py before the first use of
this helper (WM instruction cache); never replace a different running helper.
The DMA-only mode measures transport, NOT FFT coverage. Spectrum mode processes
every complete block in order, stopping before unread data can be overwritten.
Only the final per-core power arrays remain; this does not export every FFT.
"""
import argparse
import cmath
import hashlib
import json
import math
from pathlib import Path
import struct
import time
from live import ROUTE
from registers import healthy
from run import read, write, packed, unpack
from w1700k import config
from w1700k.unit import Unit
from w1700k.wm import Wm

STATUS = {1: 'duration_complete', 2: 'capacity_guard', 3: 'dma_timeout',
          4: 'dsp_error', 5: 'drain_timeout', 6: 'pointer_poll_gap',
          7: 'dma_length_error', 10: 'bad_args_or_capture_active',
          11: 'dsp_not_ready', 12: 'dma_busy'}

def summarize(s):
    written = s[9] + (s[10] << 32)
    capture_s, elapsed_s = s[12]/720e6, s[13]/720e6
    return dict(status=STATUS.get(s[3], str(s[3])), mode=s[4], mask=s[5],
                requested_ms=s[6], dispatched_blocks=s[7], completed_blocks=s[8],
                captured_bytes=written, complete_captured_blocks=written//8192,
                capture_ms=capture_s*1000, elapsed_ms=elapsed_s*1000,
                observed_sample_rate=written/4/capture_s if capture_s else None,
                completed_samples_per_second=s[8]*2048/elapsed_s if elapsed_s else None,
                max_lag_bytes=s[11], mean_dma_us=s[14]/720/s[7] if s[7] else None,
                max_poll_gap_us=s[16]/720, core_completed=s[20:23],
                last_dsp_us=[v/640 for v in s[24:27]], pending=s[28:31],
                all_complete_blocks_processed=(s[3] in (1,2) and s[7]==s[8]==written//8192),
                capture_completed_requested_duration=(s[3]==1),
                registers=s)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True)
    p.add_argument('--helper', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    p.add_argument('--ms', type=int, default=100)
    p.add_argument('--mask', type=int, default=7)
    p.add_argument('--mode', choices=('dma', 'spectrum'), default='spectrum')
    p.add_argument('--validate', type=Path, help='save final raw/power arrays for independent validation; fresh firmware load required')
    args=p.parse_args()
    if not 1<=args.ms<=3000 or not 1<=args.mask<=7:
        p.error('ms must be 1..3000 and mask 1..7')
    code=args.helper.read_bytes()
    if not 0<len(code)<=0x4000:
        p.error('helper exceeds reservation')
    code+=bytes(-len(code)%4)
    tw=[cmath.exp(-2j*math.pi*k/2048) for k in range(1024)]
    tw=unpack(struct.pack('<2048f',*[v for z in tw for v in (z.real,z.imag)]))
    with Wm(Unit(config.load(args.config).unit)) as w:
        healthy(w)
        if w.session.streaming() or any(read(w,a,1)[0]&1 for a in (0x80023090,0x80024090,0x80025090)):
            raise RuntimeError('stop viewer, capture and stream pump first')
        if read(w,0x81020010,2)!=[7,0] or read(w,0x88009004,1)[0]:
            raise RuntimeError('DSP cores disabled/reset or pool already granted')
        for b in range(3):
            if args.mask&(1<<b):
                box=read(w,0x86200000+(b<<22),11)
                if box[0]!=0x44535031 or box[1]!=box[2] or box[10]!=1:
                    raise RuntimeError(f'DSP{b} not ready')
                write(w,0x86209000+(b<<22),tw)
        write(w,0x00912000,unpack(code))
        if packed(read(w,0x00912000,len(code)//4))!=code:
            raise RuntimeError('helper verification failed')
        saved=[(a,read(w,a,1)[0]) for a,_ in ROUTE]
        cr0=read(w,0x80023090,1)[0]
        traps=w.traps()
        try:
            w.write([(a,0x3fffc if a==0x8002309c else v) for a,v in ROUTE])
            rc=w.call(0x00912000,args.ms,args.mask,int(args.mode=='spectrum'),timeout=args.ms/1000+3)
            healthy(w)
            if w.traps()!=traps:
                raise RuntimeError('WM trapped')
            # The direct host ioctl window ends below this scratch page.
            # Reading through the interpreter is safe only AFTER call returns.
            s=read(w,0x00910b00,48)
            if s[0]!=0x44534331 or s[3]!=rc:
                raise RuntimeError('invalid result')
            result=summarize(s)
            result['helper_sha256']=hashlib.sha256(code).hexdigest()
            # Pending FFTs may finish after a capacity guard; do not reuse
            # their input buffers while they are still owned by a DSP.
            time.sleep(.01)
            result['final_mailboxes']=[read(w,0x86200000+(b<<22),11) for b in range(3)]
            if args.validate and s[3] in (1,2):
                cores=[b for b in range(3) if args.mask&(1<<b)]
                checks=[]
                for i,b in enumerate(cores):
                    seq=s[7]-1-((s[7]-1-i)%len(cores))
                    if seq<0:
                        continue
                    raw=read(w,0x8620d000+(b<<22),2048)
                    source=read(w,0xe0400000+((seq*8192)&0xfffff),2048)
                    if source!=raw or any(v&0xc000c000 for v in raw) or len(set(raw))<8:
                        raise RuntimeError(f'core {b} block {seq}: DMA input integrity failure')
                    item=dict(core=b,block=seq,raw_iq_words=raw)
                    if args.mode=='spectrum':
                        item['power']=list(struct.unpack('<2048f',packed(read(w,0x86205000+(b<<22),2048))))
                    checks.append(item)
                args.validate.write_text(json.dumps(checks)+'\n')
                result['final_buffer_checks']=[{'core':v['core'],'block':v['block'],'words_matched':2048} for v in checks]
            args.out.write_text(json.dumps(result,indent=2)+'\n')
            print(json.dumps({k:v for k,v in result.items() if k not in ('registers','final_mailboxes')}),flush=True)
        finally:
            healthy(w)
            w.write([(0x80023090,0x400),(0x88009004,0)]+saved+[(0x80023090,cr0)])

if __name__=='__main__':
    main()
