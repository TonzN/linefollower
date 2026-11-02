import argparse
import csv
import json
from pathlib import Path
import tempfile
import unittest

import hardware_testbed as testbed


def options(**changes):
    result=dict(test='sensors',seconds=0.1,interval_ms=20,confirm_motion=False,
                label='test',left=0,right=0,start=0,end=12,step=5,dwell_ms=100,side='left')
    result.update(changes)
    return argparse.Namespace(**result)


def sample(**changes):
    result=dict(kind='sample',sample=1,time_us=1000,sensor_us=200,cycle_us=500,
                mode=2,fault=0,flags=1025,sensor_count=2,raw=[0,950],calibrated=[None,None],
                voltage_mv=None,current_ma=None,left_rpm_x10=None,right_rpm_x10=None)
    result.update(changes)
    return result


class FakeBridge:
    def __init__(self,fail=False):
        self.commands=[];self.fail=fail

    def command(self,command):
        self.commands.append(command)
        return sample()

    def poll(self):
        if self.fail:
            raise testbed.BoardError('lost link')
        return sample(sample=20,time_us=153000,flags=1281,mode=0)


class FakeSerial:
    def __init__(self,replies):
        self.replies=iter(replies);self.writes=[]
    def write(self,data): self.writes.append(data)
    def flush(self): pass
    def readline(self): return next(self.replies,b'')


class Tests(unittest.TestCase):
    def test_side_values_and_physical_indices_in_csv(self):
        s=sample(side_count=2,side_raw=[0,None],side_indices=[0,24],main_indices=[11,12],side_fresh=0)
        row=testbed.flatten(s,'side',0,0,0)
        self.assertEqual(row['side_raw_0'],0)
        self.assertIsNone(row['side_raw_1'])
        self.assertEqual(row['side_indices_1'],24)
        self.assertNotIn('side_raw',row)
        serial=FakeSerial([json.dumps(dict(s,side_count=3)).encode()])
        with self.assertRaises(testbed.BoardError): testbed.SerialBridge(serial).poll()

    def test_duration_and_sweep_endpoints(self):
        self.assertEqual(testbed.test_command(options(test='sweep')),('SWEEP 1 0 12 5 100',0.4))
        self.assertEqual(testbed.test_command(options(test='sweep',end=-100,step=-5))[1],2.1)
        for args in [options(seconds=0),options(seconds=float('nan')),options(test='sweep',step=0),options(test='sweep',end=-10,step=5)]:
            with self.assertRaises(ValueError): testbed.test_command(args)

    def test_motor_requires_explicit_motion_flag(self):
        bridge=FakeBridge()
        with self.assertRaises(ValueError): testbed.run_test(bridge,options(test='motor'),'unused.csv')
        self.assertEqual(bridge.commands,[])

    def test_existing_raw_file_is_never_overwritten(self):
        bridge=FakeBridge()
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'keep.csv'
            path.write_text('original',encoding='utf-8')
            with self.assertRaises(ValueError): testbed.run_test(bridge,options(),path)
            self.assertEqual(path.read_text(encoding='utf-8'),'original')
            self.assertEqual(bridge.commands,[])

    def test_missing_measurement_is_blank_real_zero_preserved(self):
        bridge=FakeBridge()
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'test.csv'
            result=testbed.run_test(bridge,options(),path,sleep=lambda _:None)
            with path.open(encoding='utf-8') as file:
                rows=list(csv.DictReader(file))
            self.assertEqual(rows[0]['raw_0'],'0')
            self.assertEqual(rows[0]['voltage_v'],'')
            self.assertTrue(result['complete'])
            self.assertEqual(rows[1]['skipped_between_polls'],'18')
            self.assertEqual(bridge.commands[-1],'STOP')

    def test_stop_on_exception_and_incomplete_metadata(self):
        bridge=FakeBridge(fail=True)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'test.csv'
            with self.assertRaises(testbed.BoardError):
                testbed.run_test(bridge,options(),path,sleep=lambda _:None)
            metadata=json.loads(path.with_suffix('.json').read_text(encoding='utf-8'))
            self.assertFalse(metadata['complete']);self.assertTrue(metadata['stop_acknowledged'])
            self.assertEqual(bridge.commands[-1],'STOP')

    def test_serial_reports_board_error(self):
        serial=FakeSerial([b'{"kind":"error","message":"not configured"}\n'])
        with self.assertRaisesRegex(testbed.BoardError,'not configured'):
            testbed.SerialBridge(serial).poll()

    def test_nano_acknowledgement_is_checked(self):
        reply=sample(ack_sequence=42,ack_result=3)
        serial=FakeSerial([b'{"kind":"sent","sequence":42}\n',json.dumps(reply).encode()+b'\n'])
        with self.assertRaisesRegex(testbed.BoardError,'ikke konfigurert'):
            testbed.SerialBridge(serial).command('SENSORS 5000')

    def test_sensor_count_validation(self):
        reply=sample(raw=[1])
        serial=FakeSerial([json.dumps(reply).encode()+b'\n'])
        with self.assertRaises(testbed.BoardError): testbed.SerialBridge(serial).poll()


if __name__=='__main__':
    unittest.main()
