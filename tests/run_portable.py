#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Host QA: production ELF planner/analyzer, iconv bridge, APK/log/profile tools.

Windows: python tests/run_portable.py --zig build/test-tools/ziglang/zig.exe
macOS/Linux: python3 tests/run_portable.py --sanitize
Device-only checks are listed in docs/08-hung-delivery.md; never counted as PASS here.
"""
import argparse
import os
import json
import platform
import re
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--zig", type=Path)
    ap.add_argument("--cc", default=os.environ.get("CC", "cc"))
    ap.add_argument("--sanitize", action="store_true")
    args = ap.parse_args()
    compiler = [str(args.zig.resolve()), "cc"] if args.zig else [args.cc]
    out = ROOT / "build/portable-qa"; out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["ZIG_GLOBAL_CACHE_DIR"] = str(ROOT / "build/iconv-tests/zig-cache")
    env["ZIG_LOCAL_CACHE_DIR"] = str(out / "zig-local-cache")
    results = []
    def run(name, command):
        result = subprocess.run(command, cwd=ROOT, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(result.stdout, end="", flush=True)
        status = "FAIL" if result.returncode else "SKIP" if "Ran 0 tests" in result.stdout and "skipped=" in result.stdout else "PASS"
        results.append({"name": name, "status": status, "exitCode": result.returncode, "output": result.stdout})
        (ROOT / "build/qa-results.json").write_text(json.dumps({"host": platform.platform(), "checks": results,
            "deviceChecks": "NOT TESTED", "gameplay": "NOT TESTED"}, indent=2), encoding="utf-8")
        result.check_returncode()
    flags = ["-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-UNDEBUG", f"-I{ROOT / 'src/translation-layer'}"]
    if args.sanitize and os.name != "nt":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    for name in ("plan_test", "elf_import_test"):
        exe = out / (name + (".exe" if os.name == "nt" else ""))
        run(name + " compile", compiler + flags + [str(ROOT / f"tests/translation-layer/{name}.c"), str(ROOT / "src/translation-layer/husk-tl-elf.c"), "-o", str(exe)])
        run(name, [str(exe)])
    source = (ROOT / "src/translation-layer-next/husk-tl-bionic-str.c").read_text(encoding="utf-8")
    wrapper = re.search(r"static int b_getchar\(void\) \{[^\n]+\}", source)
    assert wrapper and 'TL_WRAP("getchar", b_getchar)' in source
    (out / "getchar_under_test.h").write_text(wrapper[0] + "\n", encoding="utf-8")
    (out / "stdin.bin").write_bytes(b"A\xff")
    exe = out / ("getchar_test.exe" if os.name == "nt" else "getchar_test")
    run("getchar compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}", str(ROOT / "tests/translation-layer/getchar_test.c"), "-o", str(exe)])
    run("getchar", [str(exe), str(out / "stdin.bin")])
    iconv = [sys.executable, "tests/translation-layer/run_iconv.py", "--cc", args.cc]
    if args.zig:
        iconv += ["--zig", str(args.zig)]
    if args.sanitize:
        iconv += ["--sanitize"]
    run("iconv", iconv)
    run("Python analyzer/diagnostics/profile/bootstrap", [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_hung_tools.py", "-v"])
    run("Swift diagnosis and settings", [sys.executable, "tests/test_hung_swift.py", "-v"])
    run("Swift APK ordering baseline", [sys.executable, "tests/translation-layer/test_apk_order.py"])
    print("Portable checks completed; iOS/JIT/graphics/boot/gameplay still require their own runs.")


if __name__ == "__main__":
    main()
