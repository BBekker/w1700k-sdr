# SPDX-License-Identifier: MIT
"""Driver DMA: WFDMA control against mt76, the MCU rings and the stream ring."""
import unittest

from kernel_build import CProgram, have_compiler


@unittest.skipUnless(have_compiler(), 'needs a C compiler')
class KernelDmaTests(unittest.TestCase):
    def check(self, source):
        program = CProgram(source)
        self.addCleanup(program.close)
        result = program.run()
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_wfdma_control_matches_mt76(self):
        self.check('wfdma-test.c')

    def test_rings_descriptors_and_stream_ring(self):
        self.check('dma-ring-test.c')

    def test_stream_pump_always_stops_the_producer(self):
        self.check('pump-test.c')


if __name__ == '__main__':
    unittest.main()
