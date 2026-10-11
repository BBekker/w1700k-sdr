# SPDX-License-Identifier: MIT
"""Compile the stream firmware into position-fixed images: the WM producer and the WA packer,
which own.py places in our WM's and WA's containers at their link addresses.

An image has no loader: our WM enters the producer as a function, our WA the packer. So
everything must land in .text (code, then read-only data); any data, bss or small-data section
is refused, as is a call into a runtime library. ld.lld resolves every relocation at link time
and llvm-objcopy extracts the raw .text.

Two production configurations, by packing schedule over each PERIOD chunks:
- raw: no packing; every 8 KiB chunk is published as captured (up to 160 Msps of 14-bit IQ).
- half: the WM packs 4 chunks and the WA 4 of every 16 to 28 bits (7168 bytes): 50% packed,
  so 320 Msps fits one 10 GbE link. Raising the WA leaves the firmware command path dead until
  the next firmware load.
"""
import shutil
import struct
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path

HERE = Path(__file__).resolve().parent

WA_CODE, WA_LIMIT = 0x1001A400, 0x1001B000      # 1 KiB below WA_STACK_TOP stays the WA's stack

CFLAGS = ['--target=riscv32', '-march=rv32ima_zicsr', '-mabi=ilp32', '-O2', '-ffreestanding', '-fno-builtin',
          '-nostdlib', '-mno-relax', '-msmall-data-limit=0', '-mcmodel=medany', '-fno-pic', '-fno-jump-tables',
          '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-Wall', '-Werror']
LINKER_SCRIPT = ('SECTIONS {{\n . = {base:#x};\n .text : {{ KEEP(*(.text.entry)) *(.text .text.*) '
                 '*(.rodata .rodata.* .srodata .srodata.*) }}\n /DISCARD/ : {{ *(.comment) *(.riscv.attributes) '
                 '*(.note .note.*) }}\n}}\n')


@dataclass(frozen=True)
class Packing:
    name: str
    wa: int                       # chunks of each period the WA packs
    wm: int                       # chunks of each period the WM packs
    period: int = 16


RAW = Packing('raw', 0, 0)
HALF = Packing('half', 4, 4)
PACKINGS = (RAW, HALF)


def schedule(packing):
    """(WA mask, WM mask, slot table): each packer's positions spread evenly over the period."""
    period, wa, wm = packing.period, packing.wa, packing.wm
    n = wa + wm
    if period & (period - 1) or not 1 <= period <= 32 or n > period:
        raise ValueError('period: a power of two up to 32, at least wa + wm')
    at = [i * period // n for i in range(n)]
    who = ['wa' if (j + 1) * wa // n > j * wa // n else 'wm' for j in range(n)]
    masks, slots, count = dict(wa=0, wm=0), [0] * period, dict(wa=0, wm=0)
    for position, packer in zip(at, who):
        masks[packer] |= 1 << position
        slots[position] = count[packer]
        count[packer] += 1
    return masks['wa'], masks['wm'], slots


def _schedule_defines(packing):
    wa_mask, wm_mask, slots = schedule(packing)
    return dict(PERIOD=f'{packing.period}u', WA_MASK=f'{wa_mask:#x}u', WM_MASK=f'{wm_mask:#x}u',
                WA_N=f'{packing.wa}u', WM_N=f'{packing.wm}u', SLOTS=','.join(map(str, slots)))


def _alloc_sections(elf):
    """Names of an ELF32's non-empty allocated sections."""
    shoff, = struct.unpack_from('<I', elf, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from('<HHH', elf, 0x2e)
    headers = [struct.unpack_from('<IIIIIIIIII', elf, shoff + i * shentsize) for i in range(shnum)]
    strtab = headers[shstrndx][4]
    name = lambda off: elf[strtab + off:elf.index(b'\0', strtab + off)].decode()
    return [name(h[0]) for h in headers if h[2] & 2 and h[5]]


def compile_blob(sources, defines, base, limit):
    """Compile and link sources into one blob at `base`, its entry (section .text.entry) first."""
    tools = {t: shutil.which(t) for t in ('clang', 'ld.lld', 'llvm-objcopy')}
    missing = [t for t, path in tools.items() if not path]
    if missing:
        raise SystemExit('the stream firmware build needs ' + ', '.join(missing))
    flags = CFLAGS + [f'-I{HERE.parents[1] / "include"}'] + [f'-D{k}={v}' for k, v in defines.items()]
    with tempfile.TemporaryDirectory() as tmp:
        objects = []
        for i, source in enumerate(sources):
            obj = Path(tmp, f'{i}.o')
            subprocess.run([tools['clang'], *flags, '-c', str(HERE / source), '-o', str(obj)], check=True)
            objects.append(str(obj))
        script, elf, raw = Path(tmp, 'blob.ld'), Path(tmp, 'blob.elf'), Path(tmp, 'blob.bin')
        script.write_text(LINKER_SCRIPT.format(base=base))
        subprocess.run([tools['ld.lld'], '-T', str(script), '--no-relax', '-e', f'{base:#x}', '-o', str(elf),
                        *objects], check=True)
        sections = _alloc_sections(elf.read_bytes())
        if sections != ['.text']:
            raise SystemExit(f'blob sections other than .text: {sections}')
        subprocess.run([tools['llvm-objcopy'], '-O', 'binary', '-j', '.text', str(elf), str(raw)], check=True)
        code = raw.read_bytes()
    code += bytes(-len(code) % 4)
    if len(code) > limit - base:
        raise SystemExit(f'blob too large: {len(code)} bytes, {limit - base} allowed')
    return code


def wa_packer(packing):
    return compile_blob(['wa_lead.S', 'packer.c', 'pack.S'], _schedule_defines(packing), WA_CODE, WA_LIMIT)
