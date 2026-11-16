# SPDX-License-Identifier: GPL-2.0-or-later
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("iphone_perf_suite", ROOT / "tools/iphone_perf_suite.py")
suite = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = suite
SPEC.loader.exec_module(suite)


class IPhonePerfSuiteTests(unittest.TestCase):
    def test_catalog_covers_iphone_8_through_iphone_18_and_marks_unverified_variants(self):
        ids = {m.id for m in suite.IPHONE_CATALOG}
        for req in ("iphone-8", "iphone-8-plus", "iphone-x", "iphone-15-pro-max", "iphone-17-pro-max", "iphone-18-pro-max", "iphone-18"):
            self.assertIn(req, ids)
        i18 = suite.select_models("iphone-18")[0]
        self.assertEqual(i18.spec_status, "missing_verified_specs")
        selected = suite.select_models("iphone-8,iPhone16,2")
        self.assertEqual([m.id for m in selected], ["iphone-8", "iphone-15-pro-max"])

    def test_simulation_never_masquerades_as_real_device_pass(self):
        report = suite.evaluate_models(suite.select_models("iphone-8,iphone-16-pro-max,iphone-18"), [], include_simulation=True)
        for row in report["rows"]:
            self.assertEqual(row["conclusion"], "chua du du lieu")
            if row["id"] == "iphone-18":
                self.assertIn("thieu cau hinh xac minh", row["data_source"])
                self.assertIsNone(row["simulation"])
            else:
                self.assertTrue(row["simulation"]["simulated_only"])
                self.assertIn("mo phong", row["data_source"])

    def test_real_device_log_produces_pass_or_lag_strictly_from_observed_metrics(self):
        with tempfile.TemporaryDirectory() as tmp:
            d = Path(tmp)
            (d / "husk.log").write_text(
                "\n".join([
                    "[     1.00ms][boot] device      : iPhone16,2",
                    "[     1.10ms][boot] system      : iOS 26.6.1",
                    "[     1.20ms][boot] physical RAM: 7653 MiB",
                    "[     1.30ms][boot] build       : 3.3 (149) abc1234 built 2026-11-16 12:00 UTC",
                    "[  5000.00ms][switch] [2026-11-16T12:00:05Z] switch requested: from=vn.trutien.client (preState=2) -> to=com.tech3cgame.vhpt1 (retry=false)",
                    "[  5200.00ms][switch] [2026-11-16T12:00:08Z] switch committed: active=com.tech3cgame.vhpt1 postState=1",
                ]),
                encoding="utf-8",
            )
            (d / "husk-native.log").write_text(
                "\n".join([
                    "javaapp: com.tech3cgame.vhpt1, launcher org.cocos2dx.cpp.AppActivity, application (none)",
                    "unity: java-perf: pkg=com.tech3cgame.vhpt1 fps=59.8 mean_ms=16.72 max_ms=24.10 hitch50=0 hitch100=0",
                    "unity: java-perf: pkg=com.tech3cgame.vhpt1 fps=58.4 mean_ms=17.12 max_ms=31.00 hitch50=0 hitch100=0 scenario=heavy_crowd_effects",
                ]),
                encoding="utf-8",
            )
            parsed = suite.parse_device_logs(d)
            report = suite.evaluate_models(suite.select_models("iphone-15-pro-max,iphone-14-pro-max"), [parsed])
            pm = report["rows"][0]
            other = report["rows"][1]
            self.assertEqual(pm["conclusion"], "dat")
            self.assertEqual(pm["data_source"], "real_device")
            self.assertEqual(pm["tested_ios"], "iOS 26.6.1")
            self.assertEqual(other["conclusion"], "chua du du lieu")

            # Introduce a severe >100 ms hitch and verify conclusion switches to co giat/lag
            (d / "husk-native.log").write_text(
                "\n".join([
                    "javaapp: com.tech3cgame.vhpt1, launcher org.cocos2dx.cpp.AppActivity, application (none)",
                    "unity: java-perf: pkg=com.tech3cgame.vhpt1 fps=59.2 mean_ms=16.90 max_ms=22.00 hitch50=0 hitch100=0",
                    "unity: java-perf: pkg=com.tech3cgame.vhpt1 fps=42.0 mean_ms=23.80 max_ms=135.00 hitch50=3 hitch100=1 scenario=heavy_crowd_effects",
                ]),
                encoding="utf-8",
            )
            lag_report = suite.evaluate_models(suite.select_models("iphone-15-pro-max"), [suite.parse_device_logs(d)])
            self.assertEqual(lag_report["rows"][0]["conclusion"], "co giat/lag")


if __name__ == "__main__":
    unittest.main()
