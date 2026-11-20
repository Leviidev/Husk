"""Offline regression checks of the production Play client and search manager; no real accounts or network."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class StoreSearchTests(unittest.TestCase):
    def test_play_cursors_and_search_session_lifecycle(self):
        with tempfile.TemporaryDirectory(prefix="husk-store-search-") as directory:
            work = pathlib.Path(directory)
            sources = [ROOT / "src/app/Husk/Play" / name for name in
                       ("Protobuf.swift", "PlayDevice.swift", "PlayAPI.swift", "PlayStoreManager.swift")]
            result = subprocess.run(["xcrun", "swiftc", "-swift-version", "5", "-module-cache-path", str(work / "modules"),
                                     *map(str, sources), str(ROOT / "tests/app/fixtures/StoreSearchTests.swift"),
                                     "-o", str(work / "probe")], capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(work / "probe")], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(result.stdout.count("PASS:"), 2, result.stdout)


if __name__ == "__main__":
    unittest.main()
