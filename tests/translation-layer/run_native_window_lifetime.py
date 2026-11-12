#!/usr/bin/env python3
"""ASan-test the real native-window adapter with a deterministic mock EGL driver.

No APK is executed. DVM is deliberately absent so JNI's reference-counted HLE
mode can verify ownership: Java DVM currently defers object collection separately.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--out", type=Path, required=True, help="Directory for build output and logs")
args = parser.parse_args()
OUT = args.out.resolve()
OUT.mkdir(parents=True, exist_ok=True)
TASK_TEMP = OUT / "tmp"
TASK_TEMP.mkdir(parents=True, exist_ok=True)
CHILD_ENV = dict(os.environ, TMPDIR=str(TASK_TEMP), TMP=str(TASK_TEMP), TEMP=str(TASK_TEMP), TL_TMP=str(TASK_TEMP))
native, old = ROOT / "src/translation-layer-next", ROOT / "src/translation-layer"
sources = [ROOT / "tests/translation-layer/native_window_lifetime.c"]
sources += sorted(p for p in native.iterdir() if p.suffix in (".c", ".m") and p.name not in ("husk-tl-unity-app.c", "husk-tl-egl.c"))
sources += [old / name for name in ("husk-tl-elf.c", "husk-tl-zip.c", "husk-tl-sound.c")]
binary = OUT / "native-window-lifetime-test"
command = ["xcrun", "--sdk", "macosx", "clang", "-g", "-O1", "-fobjc-arc", "-Wall", "-Wno-unused-function",
           "-Wno-deprecated-declarations", "-fsanitize=address", "-fno-omit-frame-pointer", "-I" + str(old), "-I" + str(native)]
command += [str(p) for p in sources]
command += ["-Wl,-U,_husk_native_loaded_apk", "-lz", "-lm", "-lresolv", "-lsqlite3"]
for framework in ("AudioToolbox", "Foundation", "CoreFoundation", "CoreText", "CoreGraphics", "Security", "QuartzCore", "Metal",
                  "IOSurface", "ImageIO", "WebKit", "AppKit", "VideoToolbox", "CoreMedia", "CoreVideo", "Accelerate"):
    command += ["-framework", framework]
command += ["-o", str(binary)]
with (OUT / "build.log").open("w") as log:
    subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, env=CHILD_ENV)
subprocess.run(["codesign", "--force", "--sign", "-", str(binary)], check=True, capture_output=True, env=CHILD_ENV)
with (OUT / "results.log").open("w") as log:
    result = subprocess.run([str(binary)], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60, env=CHILD_ENV)
    log.write(result.stdout)
    print(result.stdout, end="")
    result.check_returncode()
print(OUT / "results.log")
