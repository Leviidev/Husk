#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile the production iconv table and exercise it through symbol lookup.

macOS/Linux: python3 tests/translation-layer/run_iconv.py
Windows: python tests/translation-layer/run_iconv.py --zig build/test-tools/ziglang/zig.exe
Windows needs Git for Windows' GNU libiconv DLL; override with --iconv-dll.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cc", default=os.environ.get("CC", "cc"))
    ap.add_argument("--zig", type=Path)
    ap.add_argument("--iconv-dll", default=r"C:\Program Files\Git\mingw64\bin\libiconv-2.dll")
    ap.add_argument("--sanitize", action="store_true")
    args = ap.parse_args()
    out = ROOT / "build" / "iconv-tests"
    out.mkdir(parents=True, exist_ok=True)
    src = ROOT / "src" / "translation-layer-next"
    tests = Path(__file__).resolve().parent
    exe = out / ("iconv_test.exe" if os.name == "nt" else "iconv_test")
    cmd = [str(args.zig.resolve()), "cc"] if args.zig else [args.cc]
    cmd += ["-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-UNDEBUG", f"-I{src}",
            str(tests / "iconv_test.c"), str(src / "husk-tl-bionic-iconv.c"), "-o", str(exe)]
    env = os.environ.copy()
    if args.zig:
        env["ZIG_GLOBAL_CACHE_DIR"] = str(out / "zig-cache")
        env["ZIG_LOCAL_CACHE_DIR"] = str(out / "zig-local-cache")
    if os.name == "nt":
        if not Path(args.iconv_dll).is_file():
            ap.error("--iconv-dll must point to Git for Windows' mingw64 libiconv-2.dll")
        cmd += [f"-I{tests / 'windows-iconv'}", str(tests / "windows-iconv" / "backend.c")]
        env["TL_TEST_ICONV_DLL"] = args.iconv_dll
    else:
        cmd += ["-pthread"]
        if sys.platform == "darwin":
            cmd += ["-liconv"]
        if args.sanitize:
            cmd += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    print("Compiling production iconv bridge with warnings as errors", flush=True)
    subprocess.run(cmd, cwd=ROOT, env=env, check=True)
    subprocess.run([str(exe)], cwd=ROOT, env=env, check=True)


if __name__ == "__main__":
    main()
