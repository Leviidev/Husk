# SPDX-License-Identifier: GPL-2.0-or-later
import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location('city_performance', Path(__file__).resolve().parents[1] / 'tools/city_performance.py')
city = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(city)
CSV = 'time,frames,fps,frame_max_ms,tick_ms,render_ms,wait_ms\n09:00:00,60,60,20,4,5,7\n09:00:01,30,30,50,5,9,19\n09:00:02,25,25,90,12,27,1\n'


class CityPerformanceTests(unittest.TestCase):
    def test_only_confirmed_window_counts_and_no_instantaneous_minimum(self):
        result = city.summarize(CSV, '', '09:00:01', '09:00:02')
        self.assertEqual(result['window']['frames'], 55)
        self.assertEqual(result['window']['rows'], 2)
        self.assertEqual(result['weightedFPS'], 27.5)
        self.assertEqual(result['intervalsBelow30'], 1)
        self.assertEqual(result['maxFrameMsFromCSV'], 90)
        self.assertNotIn('sessionP95', result)
        self.assertEqual(result['gpuTiming'], 'unavailable')

    def test_frames_not_rows_weight_elapsed_time(self):
        text = 'time,frames,fps,frame_max_ms\n09:00:01,60,30,50\n09:00:02,30,60,25\n'
        self.assertEqual(city.summarize(text, '', '09:00:01', '09:00:02')['weightedFPS'], 36)

    def test_old_session_rows_and_outside_markers_do_not_supply_stage_or_memory(self):
        log = '[0ms][prev] I: [EC.log] [09:00:01.100] [ZX_WORSTFRAME] tick 999 + render 999 ms: rp.alpha 999\n'
        log += 'I: [EC.log] [09:00:01.200] [ZX_WORSTFRAME] tick 12 + render 27 ms: host 6.2 rp.alpha 12.5 rp.water 4.0\n'
        log += 'memory-budget[game-periodic]: footprint=2000.0 MiB resident=900.0 MiB available-before-jetsam=700.0\n'
        log += 'jvm-retained-partial[game-periodic]: objects=100 headers=1.0 MiB array-payload=2.0 MiB\n'
        log += 'I: [EC.log] [09:00:02.200] [FRAMECAP] test\n'
        log += 'jvm-retained-partial[game-periodic]: objects=130 headers=1.1 MiB array-payload=2.1 MiB\n'
        log += 'I: [EC.log] [09:00:05.000] [ZX_WORSTFRAME] tick 999 + render 999 ms: rp.alpha 999\n'
        result = city.summarize(CSV, log, '09:00:01', '09:00:03')
        self.assertEqual(len(result['worstFrameStages']), 1)
        self.assertEqual(result['worstFrameStages'][0]['worstFrameStageMs']['rp.alpha'], 12.5)
        self.assertEqual(result['memorySamples'][0]['headroomMiB'], 700)
        self.assertEqual(result['javaCounterTrend']['approxObjectsPerSecond'], 30)

    def test_missing_wrong_build_empty_and_invalid_data_fail_explicitly(self):
        for log in ('', '[0ms][boot] build : 3.3 old-dirty\n'):
            with self.assertRaises(ValueError):
                city.summarize(CSV, log, '09:00:01', '09:00:02', 'new-dirty')
        for text, start, end in ((CSV, '09:01:00', '09:02:00'), (CSV, '10:00:00', '09:00:00'),
                                  ('time,frames,fps,frame_max_ms\n09:00:01,1,nan,2\n', '09:00:01', '09:00:02')):
            with self.assertRaises(ValueError):
                city.summarize(text, '', start, end)


if __name__ == '__main__':
    unittest.main()
