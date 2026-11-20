"""Exercise the production Play downloader with delayed chunks and no real account or network."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


class StoreDownloadTests(unittest.TestCase):
    def test_download_bytes_and_cancellation(self):
        with tempfile.TemporaryDirectory(prefix="husk-store-download-") as directory:
            work = pathlib.Path(directory)
            sources = [ROOT / "src/app/Husk/Play" / name for name in
                       ("Protobuf.swift", "PlayDevice.swift", "PlayAPI.swift")]
            result = subprocess.run(["xcrun", "swiftc", "-swift-version", "5", "-module-cache-path", str(work / "modules"),
                                     *map(str, sources), str(ROOT / "tests/app/fixtures/StoreDownloadTests.swift"),
                                     "-o", str(work / "probe")], capture_output=True, text=True, timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(work / "probe")], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS:", result.stdout)


if __name__ == "__main__":
    unittest.main()
