# SPDX-License-Identifier: MIT
"""The DSP viewer consumes powers without starting CPU FFT workers or a raw stream."""
import math
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import unittest
from test_spectrum_server import BINARY, SPECTRUM, WebSocketClient, build_problem

@unittest.skipIf(build_problem(), build_problem() or '')
class DspViewerTest(unittest.TestCase):
    def test_power_frame_validation_and_stale_status(self):
        subprocess.run(['make','-C',SPECTRUM,'-j4'],check=True,stdout=subprocess.DEVNULL)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'frame'
            proc=subprocess.Popen([BINARY,'--listen','127.0.0.1:0','--dsp-input',str(path)],
                                  stderr=subprocess.PIPE,text=True)
            try:
                line=proc.stderr.readline()
                self.assertIn('serving on http://',line)
                host,port=line.split('http://',1)[1].split('/',1)[0].split(':')
                ws=WebSocketClient(host,int(port))
                try:
                    ws.wait(lambda: ws.statuses>0)
                    self.assertEqual(ws.status['fft']['backend'],'dsp')
                    self.assertEqual(ws.status['spectrum']['workers'],0)
                    self.assertIsNone(ws.status['stream'])
                    self.assertFalse(ws.status['running'])
                    ws.send({'t':'view','start':0,'stop':0,'bins':2048,'auto':False})
                    def publish(power, timestamp=None, suffix=b'', version=1):
                        data=struct.pack('<8sIIQddIIQ',b'DSPFFT1\0' if version==1 else b'DSPFFT2\0',2048,1 if version==1 else 7,1,2440e6,40e6,390000,6300,
                                         timestamp or time.time_ns())+struct.pack('<2048f',*power)+suffix
                        temp=path.with_suffix('.part');temp.write_bytes(data);os.replace(temp,path)
                    power=[1e-10]*2048;power[8]=.25
                    publish(power)
                    ws.wait(lambda: ws.frames)
                    ws.send({'t':'fft','rate':20})
                    ws.wait(lambda: ws.status['running'])
                    frame=ws.frames[-1]
                    self.assertAlmostEqual(frame['peak_db'],10*math.log10(.25),delta=.6)
                    self.assertLess(abs(frame['peak_hz']-(2440e6+8*40e6/2048)),40e6/2048)
                    ws.send({'t':'fft','size':32768})
                    ws.wait_notice('DSP0 preview')
                    # A stale file, nonfinite bin, or excess payload must never refresh liveness.
                    publish(power,timestamp=time.time_ns()-10_000_000_000)
                    ws.wait(lambda: not ws.status['running'],timeout=5)
                    count=len(ws.frames)
                    power[0]=float('nan');publish(power)
                    time.sleep(.15)
                    self.assertEqual(len(ws.frames),count)
                    power[0]=1e-10;publish(power,suffix=b'extra')
                    time.sleep(.15)
                    self.assertEqual(len(ws.frames),count)
                    publish(power,version=2)
                    ws.wait(lambda: len(ws.frames)>count)
                    ws.wait(lambda: ws.status['dsp']['core_mask']==7)
                finally: ws.close()
            finally:
                proc.terminate();proc.wait(timeout=5);proc.stderr.close()
