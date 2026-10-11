# SPDX-License-Identifier: MIT
"""Compile and run the C host tests of the w1700k_sdr driver (helpers only)."""
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDR = ROOT / 'kernel' / 'w1700k_sdr'
TESTS = SDR / 'tests'
UAPI = ROOT / 'include' / 'uapi'
INCLUDE = ROOT / 'include'
CC = os.environ.get('CC', 'cc')
CFLAGS = ['-std=gnu11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
          '-fsanitize=undefined', '-fno-sanitize-recover=all']
# Kernel headers the driver sources include; the test supplies their contents.
STUBS = ('linux/dma-mapping.h', 'linux/io.h', 'linux/mm.h', 'linux/slab.h',
         'linux/delay.h', 'linux/ktime.h', 'linux/mutex.h', 'linux/pci.h',
         'linux/sched/signal.h', 'linux/crc32.h')


def have_compiler():
    return shutil.which(CC) is not None


class CProgram:
    """One test source compiled into a private temporary directory."""

    def __init__(self, source, *extra, shared=False):
        self.tmp = tempfile.TemporaryDirectory(prefix='w1700k-sdr-test-')
        tmp = Path(self.tmp.name)
        for name in STUBS:
            stub = tmp / 'include' / name
            stub.parent.mkdir(parents=True, exist_ok=True)
            stub.write_text('/* supplied by the test harness */\n')
        self.path = tmp / (Path(source).stem + ('.so' if shared else ''))
        if shared:
            extra = ('-shared', '-fPIC', *extra)
        result = subprocess.run(
            [CC, *CFLAGS, *extra, '-I', str(tmp / 'include'), '-I', str(TESTS),
             '-I', str(SDR), '-I', str(UAPI), '-I', str(INCLUDE), str(TESTS / source),
             '-o', str(self.path)],
            capture_output=True, text=True)
        if result.returncode:
            self.tmp.cleanup()
            raise AssertionError(f'{source} does not compile:\n{result.stderr}')

    def run(self, *args, timeout=120):
        return subprocess.run([str(self.path), *map(str, args)], capture_output=True,
                              text=True, timeout=timeout)

    def close(self):
        self.tmp.cleanup()
