# SPDX-License-Identifier: MIT
"""The local fast path must retain error checks and use bounded bulk reads."""
import ctypes
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/dsp'))
from live import PowerReader, MAGIC, STATE_RUNNING, CALL

class DspLiveIoTest(unittest.TestCase):
    def reader(self):
        with patch('live.os.open',return_value=42):
            return PowerReader(True)

    def test_two_page_reads_preserve_every_power_word(self):
        reader=self.reader()
        expected=bytes((i*17)%256 for i in range(8192))
        seen=[]
        def ioctl(fd,command,request):
            self.assertEqual((fd,command),(42,0x40185117))
            address,length,pointer,r0,r1=struct.unpack('<IIQII',request)
            self.assertEqual((r0,r1),(0,0))
            offset=address-0x0090b000
            self.assertEqual(length,4096)
            seen.append(address)
            ctypes.memmove(pointer,expected[offset:offset+length],length)
        with patch('live.fcntl.ioctl',side_effect=ioctl):
            self.assertEqual(reader.read(None),expected)
        self.assertEqual(seen,[0x0090b000,0x0090c000])

    def test_completion_checks_traps_and_return_value(self):
        def state(calls,traps=0,result=0):
            values=[0]*23
            values[0]=MAGIC;values[3]=STATE_RUNNING;values[15]=traps
            values[17]=calls;values[22]=result
            return struct.pack('<23I',*values)
        for after,error in ((state(9),None),(state(9,traps=1),'trapped'),
                            (state(9,result=32),'failed: 32')):
            reader=self.reader()
            def ioctl(fd,command,buf,mutable):
                self.assertEqual(command,0xc3205120)
                self.assertEqual(struct.unpack_from('<4I',buf),(1,0,0,2000))
                self.assertEqual(struct.unpack_from('<I',buf,16)[0],CALL)
                self.assertEqual(struct.unpack_from('<I',buf,272)[0],0x00912000)
            with patch.object(reader,'memory',side_effect=[state(8),after]),patch('live.fcntl.ioctl',side_effect=ioctl):
                if error:
                    with self.assertRaisesRegex(RuntimeError,error): reader.capture(None,0)
                else: reader.capture(None,0)

    def test_missing_metadata_rejected(self):
        reader=self.reader()
        with patch.object(reader,'memory',return_value=bytes(16)):
            with self.assertRaisesRegex(RuntimeError,'metadata missing'):
                reader.metadata(None)

    def test_stream_pump_takeover_rejected(self):
        reader=self.reader()
        values=[0]*23;values[0]=MAGIC;values[3]=STATE_RUNNING
        def ioctl(fd,command,buf,mutable): struct.pack_into('<I',buf,788,1)
        with patch.object(reader,'memory',return_value=struct.pack('<23I',*values)),patch('live.fcntl.ioctl',side_effect=ioctl):
            with self.assertRaisesRegex(RuntimeError,'stream pump'):
                reader.capture(None,0)
