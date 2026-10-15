#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the production Swift binary-manifest parser and APK ordering.

Run: python3 tests/translation-layer/test_apk_order.py
Needs swiftc; Foundation-only, so no Xcode/iOS SDK or APK downloads are needed.
Fixtures use APK filenames but contain only the extracted AndroidManifest.xml bytes.
"""
import pathlib
import shutil
import struct
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]


def manifest(split=None, typed_only=False, package="ru.easyanatomy"):
    """Build binary Android XML, including real string-pool and attribute layouts."""
    strings = ["manifest", "package", package, "split"]
    if split is not None:
        strings.append(split)
    offsets, payload = [], bytearray()
    for value in strings:
        encoded = value.encode("utf-8")
        offsets.append(len(payload))
        payload.extend(bytes([len(value), len(encoded)]) + encoded + b"\0")
    payload.extend(b"\0" * (-len(payload) % 4))
    start = 28 + 4 * len(strings)
    pool = struct.pack("<HH6I", 1, 28, start + len(payload), len(strings), 0, 0x100, start, 0)
    pool += struct.pack(f"<{len(offsets)}I", *offsets) + payload

    def attr(name, value):
        return struct.pack("<IIIHBBI", 0xFFFFFFFF, name,
                           0xFFFFFFFF if typed_only else value, 8, 0, 3, value)

    attrs = attr(1, 2)
    if split is not None:
        attrs += attr(3, 4)
    element = struct.pack("<HHIII", 0x102, 16, 36 + len(attrs), 1, 0xFFFFFFFF)
    element += struct.pack("<II6H", 0xFFFFFFFF, 0, 20, 20, len(attrs) // 20, 0, 0, 0) + attrs
    body = pool + element
    return struct.pack("<HHI", 3, 8, 8 + len(body)) + body


class APKOrderTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which("swiftc")
        if not compiler:
            raise unittest.SkipTest("swiftc is unavailable; production Swift tests require a Swift toolchain")
        cls.temp = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = pathlib.Path(cls.temp.name)
        main = cls.work / "main.swift"
        main.write_text('''import Foundation
let paths = Array(CommandLine.arguments.dropFirst())
var reads = 0
let ordered = BundleUnpacker.orderedAPKs(paths) { path in
    reads += 1
    return try? Data(contentsOf: URL(fileURLWithPath: path))
}
precondition(reads == paths.count, "read each manifest exactly once")
for path in ordered { print((path as NSString).lastPathComponent) }
''', encoding="utf-8")
        cls.exe = cls.work / "apk-order"
        subprocess.run([compiler, str(ROOT / "src/app/Husk/ApkMetadata.swift"),
                        str(ROOT / "src/app/Husk/BundleUnpacker.swift"), str(main),
                        "-o", str(cls.exe)], check=True)

    def order(self, fixtures):
        with tempfile.TemporaryDirectory(dir=self.work) as tmp:
            paths = []
            for name, data in fixtures:
                path = pathlib.Path(tmp) / name
                path.write_bytes(data)
                paths.append(str(path))
            result = subprocess.run([str(self.exe), *paths], check=True, capture_output=True, text=True)
            return result.stdout.splitlines()

    def test_easy_anatomy_asset_packs_are_not_base(self):
        files = [("models.apk", manifest("models")), ("textures.apk", manifest("textures")),
                 ("config.arm64_v8a.apk", manifest("config.arm64_v8a")),
                 ("ru.easyanatomy.apk", manifest()), ("config.xxhdpi.apk", manifest("config.xxhdpi"))]
        expected = ["ru.easyanatomy.apk", "config.arm64_v8a.apk", "models.apk", "textures.apk", "config.xxhdpi.apk"]
        actual = self.order(files)
        self.assertEqual(actual, expected)
        self.assertEqual(self.order(list(reversed(files))), actual)

    def test_manifest_overrides_misleading_base_filename(self):
        self.assertEqual(self.order([("base.apk", manifest("models")),
                                     ("z.apk", manifest())])[0], "z.apk")

    def test_base_overrides_split_filename(self):
        self.assertEqual(self.order([("aaa.apk", manifest("textures")),
                                     ("config.base.apk", manifest())])[0], "config.base.apk")

    def test_typed_strings_without_raw_values(self):
        self.assertEqual(self.order([("models.apk", manifest("models", typed_only=True)),
                                     ("ru.easyanatomy.apk", manifest(typed_only=True))])[0], "ru.easyanatomy.apk")

    def test_empty_split_is_base(self):
        self.assertEqual(self.order([("aaa.apk", manifest("models")),
                                     ("z.apk", manifest(""))])[0], "z.apk")

    def test_unreadable_manifest_uses_legacy_filename_fallback(self):
        self.assertEqual(self.order([("config.arm64_v8a.apk", b"bad"),
                                     ("game.apk", b"bad")]), ["game.apk", "config.arm64_v8a.apk"])

    def test_confirmed_base_precedes_unreadable_manifest(self):
        self.assertEqual(self.order([("aaa.apk", b"bad"), ("z.apk", manifest())])[0], "z.apk")

    def test_single_apk(self):
        self.assertEqual(self.order([("game.apk", manifest())]), ["game.apk"])


if __name__ == "__main__":
    unittest.main()
