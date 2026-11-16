# SPDX-License-Identifier: GPL-2.0-or-later
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from city_performance import summarize  # noqa: E402


class CityPerformanceStageAndClassTests(unittest.TestCase):
    def test_parses_render_tick_frame_stages_memory_trend_and_class_allocations(self):
        csv_text = (
            "time,frames,fps,frame_ms,frame_max_ms,tick_ms,render_ms,wait_ms,present_ms,draws,npcs,players\n"
            "09:47:35,25,25.0,40.0,62.0,10.0,20.0,2.0,0.4,1400,66,1\n"
            "09:47:45,26,26.0,38.5,58.0,9.5,19.0,3.0,0.3,1380,65,1\n"
        )
        log_text = "\n".join([
            "[     6.13ms][boot] build       : 3.3 (148) test-build built 2026-11-16 02:37 UTC",
            "I/TruTien: [EC.log] [09:47:35.100] <!> [ZX_TICKPROF] 50 fr, ms/fr: tick 10.0 render 20.0 | proto 0.00 input 0.08 host 1.50 msg 0.01 | players 0.02 (deliver 0.00 base 0.00 [gfx 0.00 sprite 0.00] mirror 0.00 work 0.00 model 0.00 rest 0.00 ranges 0.00 tail 0.00) npc 6.50 matter 0.40 otherman 0.20 worldrest 0.15 | ui 0.85 gfxcaster 0.09 gfxman 0.06 enganim 0.01 lua 0.00",
            "I/TruTien: [EC.log] [09:47:35.100] <!> [ZX_FRAMEPROF] frame 39.2 ms = wait-main 2.10 lock 0.05 pre 0.30 tick 10.0 render 20.0 post 0.00 rest 0.40 | per fr: files 0.0 (0 KB, 0.0 ms) model loads 0.00 unhide 0.00 | max frame 62",
            "I/TruTien: [EC.log] [09:47:35.100] <!> [ZX_RENDERPROF] ms/fr (draws/fr): shadow 0.00 (0) register 1.20 scene 0.00 (0) litmodel 0.70 (230) skin 0.55 (95) terrain 0.60 (5) grass 0.50 (80) water 5.10 (500) alpha 8.70 (390) post 0.02 ui 1.60 (48) present 0.50 | draws 1350 tris 580K",
            "memory-budget[game-periodic]: footprint=2500.0 MiB resident=1400.0 MiB available-before-jetsam=850.0 MiB",
            "jvm-class-allocations-partial[game-periodic]: epoch=1 sample=2 baseline=0 interval=30.000 s groups=40 top=8 rank=created-delta created-total=1000 created-delta=250 array-created-bytes-total=8000 array-created-bytes-delta=2000 other-created-delta=50 other-array-bytes-delta=400 (process cumulative creation)",
            "jvm-class-allocation[game-periodic]: epoch=1 sample=2 declaration=12 class=android/view/MotionEvent created-total=400 created-delta=200 header-created-bytes-delta=16000 array-created-bytes-delta=1600",
            "I/TruTien: [EC.log] [09:47:44.900] <!> [ZX_RENDERPROF] ms/fr (draws/fr): shadow 0.00 (0) register 1.00 scene 0.00 (0) litmodel 0.50 (220) skin 0.45 (90) terrain 0.40 (4) grass 0.40 (75) water 4.50 (480) alpha 7.50 (360) post 0.02 ui 1.40 (45) present 0.40 | draws 1280 tris 540K",
            "memory-budget[game-periodic]: footprint=2650.0 MiB resident=1350.0 MiB available-before-jetsam=700.0 MiB",
        ])
        out = summarize(csv_text, log_text, "09:47:35", "09:47:45", "test-build")
        self.assertEqual(out["renderProfSummary"]["alpha"]["max"], 8.7)
        self.assertEqual(out["renderProfSummary"]["water"]["median"], 4.8)
        self.assertEqual(out["renderProfDrawsSummary"]["draws"]["max"], 1350.0)
        self.assertEqual(out["tickProfSummary"]["npc"]["median"], 6.5)
        self.assertNotIn("deliver", out["tickProfSummary"])
        self.assertEqual(out["frameProfSummary"]["wait-main"]["max"], 2.1)
        self.assertEqual(out["frameProfSummary"]["maxFrame"]["max"], 62.0)
        self.assertEqual(out["memoryTrend"]["footprintDeltaMiB"], 150.0)
        self.assertEqual(out["memoryTrend"]["headroomDeltaMiB"], -150.0)
        self.assertEqual(len(out["javaClassAllocationSamples"]), 1)
        sample = out["javaClassAllocationSamples"][0]
        self.assertEqual(sample["createdDelta"], 250)
        self.assertEqual(sample["topClasses"][0]["className"], "android/view/MotionEvent")
        self.assertEqual(sample["topClasses"][0]["createdDelta"], 200)


if __name__ == "__main__":
    unittest.main()
