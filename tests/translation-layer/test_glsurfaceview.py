#!/usr/bin/env python3
"""Pixel integration test of GLSurfaceView placement/resizing on the actual native compositor.
Set HUSK_JAVAAPP_TEST, HUSK_FRAMEWORK, TL_ART, TL_ANGLE_EGL/TL_ANGLE_GLES and TL_FRAMEWORK_RES.
Runtime AOT settings can be supplied as TL_NO_XMEM=1 and TL_AOT_DIR. No personal APK or location is used.
"""
import os
import pathlib
import re
import struct
import subprocess
import tempfile
import unittest
import zipfile
import zlib

from apk_test_harness import pixels

ROOT = pathlib.Path(__file__).resolve().parents[2]


def manifest():
    strings = ["manifest", "package", "test.surface", "application", "activity", "name",
               "http://schemas.android.com/apk/res/android", "probe.SurfaceRectProbe", "intent-filter", "action",
               "android.intent.action.MAIN", "category", "android.intent.category.LAUNCHER"]
    offsets, data = [], bytearray()
    for value in strings:
        offsets.append(len(data)); b = value.encode(); data += bytes([len(value), len(b)]) + b + b"\0"
    data += b"\0" * (-len(data) % 4)
    start = 28 + 4 * len(strings)
    pool = struct.pack("<HH6I", 1, 28, start + len(data), len(strings), 0, 0x100, start, 0) + struct.pack("<%dI" % len(offsets), *offsets) + data
    def begin(name, attributes=()):
        out = struct.pack("<HHIII", 0x102, 16, 36 + 20 * len(attributes), 1, 0xffffffff)
        out += struct.pack("<IIHHHHHH", 0xffffffff, name, 20, 20, len(attributes), 0, 0, 0)
        for ns, key, value in attributes:
            out += struct.pack("<IIIHBBI", ns, key, value, 8, 0, 3, value)
        return out
    def end(name):
        return struct.pack("<HHIIIII", 0x103, 16, 24, 1, 0xffffffff, 0xffffffff, name)
    body = pool + begin(0, [(0xffffffff, 1, 2)]) + begin(3) + begin(4, [(6, 5, 7)]) + begin(8)
    body += begin(9, [(6, 5, 10)]) + end(9) + begin(11, [(6, 5, 12)]) + end(11) + end(8) + end(4) + end(3) + end(0)
    return struct.pack("<HHI", 3, 8, len(body) + 8) + body


class SurfaceRectTests(unittest.TestCase):
    @unittest.skipUnless(os.environ.get("HUSK_JAVAAPP_TEST") and os.environ.get("HUSK_FRAMEWORK"), "Set HUSK_JAVAAPP_TEST and HUSK_FRAMEWORK")
    def test_partial_resized_and_fullscreen_surface_pixels(self):
        jdk = pathlib.Path(os.environ.get("HUSK_JDK", "/opt/homebrew/opt/openjdk@17")) / "bin"
        d8 = os.environ.get("HUSK_D8", "/opt/homebrew/share/android-commandlinetools/cmdline-tools/latest/lib/r8.jar")
        with tempfile.TemporaryDirectory() as directory:
            w = pathlib.Path(directory); framework = ROOT / "src/java-framework"
            def run(args, **kwargs):
                result = subprocess.run(list(map(str, args)), capture_output=True, text=True, timeout=90, **kwargs)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr); return result
            for n in ("stubs", "framework", "classes", "dex"): (w/n).mkdir()
            run([jdk/"javac", "--release", "8", "-nowarn", "-d", w/"stubs", *sorted((framework/"stubs").rglob("*.java"))])
            run([jdk/"javac", "--release", "8", "-nowarn", "-cp", w/"stubs", "-d", w/"framework", *sorted((framework/"src").rglob("*.java")), *sorted((framework/"gen-src").rglob("*.java"))])
            run([jdk/"javac", "--release", "8", "-cp", str(w/"framework")+os.pathsep+str(w/"stubs"), "-d", w/"classes", pathlib.Path(__file__).parent/"fixtures/SurfaceRectProbe.java"])
            run([jdk/"jar", "cf", w/"fixture.jar", "-C", w/"classes", "."])
            run([jdk/"java", "-cp", d8, "com.android.tools.r8.D8", "--min-api", "26", "--output", w/"dex", w/"fixture.jar"])
            apk = w/"probe.apk"
            with zipfile.ZipFile(apk, "w") as archive:
                archive.writestr("AndroidManifest.xml", manifest()); archive.write(w/"dex/classes.dex", "classes.dex")
            for mode in ("partial", "full"):
                with self.subTest(mode=mode):
                    ctl = w/(mode+".ctl"); first = w/(mode+"-first.png"); second = w/(mode+"-second.png")
                    ctl.write_text(f"wait 3000\nshot {first}\nwait 3500\nshot {second}\nquit\n")
                    env = os.environ.copy()
                    for key in ("TL_ACTIVITY", "TL_APPLICATION", "TL_EXTRA_DEX", "TL_PACKAGE"): env.pop(key, None)
                    env.update(TL_FRAMEWORK=env["HUSK_FRAMEWORK"], TL_TMP=str(w), TL_DATA=str(w/(mode+"-data")), TL_CTL=str(ctl), SURFACE_PROBE_MODE=mode, TL_AUDIO_MUTE="1")
                    result = run([env["HUSK_JAVAAPP_TEST"], apk, "12"], env=env)
                    self.assertNotIn("the app's main thread ended", result.stderr)
                    rects = [tuple(map(int, m)) for m in re.findall(r"SurfaceRectProbe: RECT (-?\d+) (-?\d+) (\d+) (\d+)", result.stderr)]
                    self.assertGreaterEqual(len(rects), 1 if mode == "full" else 2, result.stderr)
                    for image, rect in ((first, rects[0]), (second, rects[-1])):
                        width, height, pixel = pixels(image); x, y, sw, sh = rect
                        for px, py in [(x+10,y+10), (x+sw-10,y+10), (x+10,y+sh-10), (x+sw-10,y+sh-10)]:
                            r, g, b = pixel(px, py)
                            self.assertTrue(r > 230 and g < 25 and b < 25, f"{mode} surface {rect}: pixel {px,py} is {r,g,b}")
                        if mode == "partial": self.assertTrue(all(v > 230 for v in pixel(x-10,y+10)), "Surface escaped its view bounds")


if __name__ == "__main__":
    unittest.main()
