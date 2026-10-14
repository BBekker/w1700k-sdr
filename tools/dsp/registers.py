#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Read individually selected DSP/DFE/DMA registers through the own-WM proxy.

The viewer and stream must be stopped first. JSON lines are flushed BEFORE
each first-touch read, so a bus fault has an attributable address. No retry,
no address sweep, no register writes (apart from the proxy's RAM mailbox).
Example from the repository root on the host:
 PYTHONPATH=host python3 tools/dsp/registers.py --config CONFIG.json 0x81020010
"""
import argparse
import json
import time
from w1700k import config, rf
from w1700k.unit import Unit
from w1700k.wm import Wm, MAGIC, STATE_RUNNING

POISON = (0xdead2117, 0xffffffff)
ALLOWED = {0x81020010, 0x81020014}
for band in range(3):
    ALLOWED.update(0x86c00000 + (band << 20) + x for x in (0x10, 0x14))
    ALLOWED.add(0x830a02b8 + (band << 20))
    ALLOWED.update(0x830ad000 + (band << 20) + x for x in
                   (0x594, 0x5cc, 0x5d0, 0x5d4, 0xc2c, 0xeb0, 0xeb4, 0xe00, 0xbd0))
ALLOWED.update(0x58000000+x for x in (0x100, 0x110, 0x114, 0x208, 0x220,
               0x230, 0x284, 0x2b0, 0x300, 0x304, 0x308, 0x30c, 0x310,
               0x314, 0x318, 0x31c, 0x500, 0x504, 0x508, 0x50c,
               0x510, 0x514, 0x518, 0x51c, 0x600, 0x604, 0x680, 0x684))


def emit(**data):
    print(json.dumps(data), flush=True)


def healthy(wm):
    status = wm.status()
    if status['magic'] != MAGIC or status['state'] != STATE_RUNNING:
        raise RuntimeError(f'WM unavailable: magic={status["magic"]:#x}, state={status["state"]:#x}')
    return status


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config', required=True)
    p.add_argument('address', nargs='+', type=lambda s: int(s, 0))
    args = p.parse_args()
    if any(a not in ALLOWED for a in args.address):
        p.error('address outside the individually documented read allowlist')
    with Wm(Unit(config.load(args.config).unit)) as wm:
        first = healthy(wm)
        time.sleep(.02)
        second = healthy(wm)
        if first['beats'] == second['beats']:
            raise RuntimeError('WM heartbeat stalled: stop capture and viewer before probing')
        if wm.session.streaming():
            raise RuntimeError('stream pump still active')
        for address in args.address:
            healthy(wm)
            emit(event='before_read', address=hex(address))
            value = wm.run([rf.r32(address)])[0]
            healthy(wm)
            if value in POISON:
                raise RuntimeError(f'invalid bus response {value:#x}; stopping without retry')
            emit(event='read', address=hex(address), value=hex(value))


if __name__ == '__main__':
    main()
