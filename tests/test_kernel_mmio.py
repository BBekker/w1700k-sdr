# SPDX-License-Identifier: MIT
"""Driver register access: BAR translation, READ_MEM windows, ownership handshake."""
import unittest

from kernel_build import CProgram, have_compiler


@unittest.skipUnless(have_compiler(), 'needs a C compiler')
class KernelRegisterAccessTests(unittest.TestCase):
    def check(self, source):
        program = CProgram(source)
        self.addCleanup(program.close)
        result = program.run()
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)

    def test_translation_reads_and_read_windows(self):
        self.check('mmio-test.c')

    def test_host_ownership_handshake(self):
        self.check('ownership-test.c')


if __name__ == '__main__':
    unittest.main()
