# SPDX-License-Identifier: MIT
"""The router controller must operate without SSH or an Ethernet export."""
import tempfile
import unittest
from pathlib import Path
from w1700k.config import parse, ConfigError
from w1700k.unit import Unit
from w1700k.stream import stream_argv

class LocalUnitTest(unittest.TestCase):
    def test_local_commands_and_binary_transfer(self):
        config = parse({'unit': {'transport': 'local'}, 'export': {'mode': 'local'}})
        unit = Unit(config.unit)
        with tempfile.TemporaryDirectory() as directory:
            path = str(Path(directory) / 'binary with spaces')
            data = bytes(range(256))
            unit.push(data, path, '600')
            self.assertEqual(unit.read(path), data)
            self.assertEqual(unit.check('printf local'), 'local')
        unit.close()

    def test_local_pump_has_no_network_export(self):
        config = parse({'unit': {'transport': 'local'}, 'export': {'mode': 'local'}})
        argv = stream_argv('/usr/sbin/mt7996-stream', config.export, None)
        self.assertNotIn('--export', argv)
        self.assertIn('--go', argv)
        self.assertIn('--stop', argv)

    def test_reject_ambiguous_local_setup(self):
        for data in ({'export': {'mode': 'local'}}, {'unit': {'transport': 'invalid'}},
                     {'export': {'mode': 'invalid'}}):
            with self.assertRaises(ConfigError): parse(data)
