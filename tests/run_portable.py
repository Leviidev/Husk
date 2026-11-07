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
    io_source = (ROOT / "src/translation-layer-next/husk-tl-bionic-io.c").read_text(encoding="utf-8")
    snippets = [re.search(pattern, io_source, re.S)[0] for pattern in (
        r"void tl_set_guest_package\([^)]*\)\n\{.*?\n\}",
        r"static char \*b_getcwd\([^)]*\)\n\{.*?\n\}")]
    (out / "getcwd_under_test.h").write_text("\n".join(snippets) + "\n", encoding="utf-8")
    # Deliberately long physical container path, distinct from Android's alias.
    fixture = out / ("physical-container-" + "x" * 112)
    fixture.mkdir(exist_ok=True)
    (fixture / "files/quest").mkdir(parents=True, exist_ok=True)
    (fixture / "files/quest/keep.txt").write_text("KEEP", encoding="utf-8")
    fixture.with_name(fixture.name + "-sibling").mkdir(exist_ok=True)
    if os.name != "nt":
        alias = fixture.with_name(fixture.name + "-alias")
        if not alias.exists():
            alias.symlink_to(fixture, target_is_directory=True)
    exe = out / ("getcwd_test.exe" if os.name == "nt" else "getcwd_test")
    run("getcwd compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
        str(ROOT / "tests/translation-layer/getcwd_test.c"), str(ROOT / "src/translation-layer-next/husk-tl-path.c"), "-o", str(exe)])
    run("getcwd", [str(exe), fixture.resolve().as_posix()])
    if os.name != "nt":   # needs mmap/pthread; the registry test is POSIX-only
        exe = out / "fallback_alloc_test"
        run("fallback allocator compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}",
            str(ROOT / "tests/translation-layer/fallback_alloc_test.c"),
            str(ROOT / "src/translation-layer-next/husk-tl-fallback-alloc.c"), "-pthread", "-o", str(exe)])
        run("fallback allocator never probes foreign pointers", [str(exe)])
    android_source = (ROOT / "src/translation-layer-next/husk-tl-dvm-android.c").read_text(encoding="utf-8")
    frame_copy = re.search(r"void tl_javaapp_app_pixels\([^)]*\)\n\{.*?\n\}", android_source, re.S)
    assert frame_copy
    (out / "frame_copy_under_test.h").write_text(frame_copy[0] + "\n", encoding="utf-8")
    exe = out / ("frame_buffer_test.exe" if os.name == "nt" else "frame_buffer_test")
    run("frame buffer compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
        str(ROOT / "tests/translation-layer/frame_buffer_test.c"), "-o", str(exe)])
    run("frame buffer allocation failure and reuse", [str(exe)])
    inset_fn = re.search(r"void tl_javaapp_set_insets\([^)]*\)\n\{.*?\n\}", android_source, re.S)
    assert inset_fn
    (out / "quest_insets_under_test.h").write_text(inset_fn[0] + "\n", encoding="utf-8")
    exe = out / ("quest_layout_test.exe" if os.name == "nt" else "quest_layout_test")
    run("quest layout compile", compiler + flags + [f"-I{ROOT / 'tests/translation-layer/quest-host'}",
        f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
        str(ROOT / "tests/translation-layer/quest_layout_test.c"),
        str(ROOT / "src/translation-layer-next/husk-tl-vhtg-quest.c"), "-lm", "-o", str(exe)])
    run("VHTG selected HUD contracts", [str(exe)])
    exe = out / ("autorelease_pool_test.exe" if os.name == "nt" else "autorelease_pool_test")
    run("autorelease scope compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}",
        str(ROOT / "tests/translation-layer/autorelease_pool_test.c"), "-o", str(exe)])
    run("autorelease scope lifecycle", [str(exe)])
    if platform.system() == "Darwin":
        exe = out / "autorelease_pool_apple"
        run("Apple autorelease runtime compile", compiler + flags + ["-fno-objc-arc", "-framework", "Foundation",
            f"-I{ROOT / 'src/translation-layer-next'}", str(ROOT / "tests/translation-layer/autorelease_pool_apple.m"), "-o", str(exe)])
        run("actual Apple autorelease lifetime", [str(exe)])
    loader_source = (ROOT / "src/translation-layer-next/husk-tl-ld.c").read_text(encoding="utf-8")
    slot = re.search(r"case R_ABS64: case R_GLOB_DAT: case R_JUMP_SLOT: \{.*?return true;\n    \}", loader_source, re.S)
    assert slot
    (out / "plt_slot_under_test.h").write_text(slot[0] + "\n", encoding="utf-8")
    exe = out / ("plt_filter_test.exe" if os.name == "nt" else "plt_filter_test")
    run("PLT filter compile", compiler + flags + [f"-I{out}", str(ROOT / "tests/translation-layer/plt_filter_test.c"), "-o", str(exe)])
    run("PLT filter relocation contracts", [str(exe)])
    unity_app_source = (ROOT / "src/translation-layer-next/husk-tl-unity-app.c").read_text(encoding="utf-8")
    va_source = (ROOT / "src/translation-layer-next/husk-tl-va.c").read_text(encoding="utf-8")
    trutien_snippets = [
        re.search(r"static bool lib_uses_compressed_heap\([^)]*\)\n\{.*?\n\}", unity_app_source, re.S)[0],
        re.search(r"int tl_count_wide_scan_args\([^)]*\)\n\{.*?\n\}", va_source, re.S)[0],
        re.search(r"int tl_scan_wide\([^)]*\)\n\{.*?\n\}", va_source, re.S)[0],
        re.search(r"static char \*b_tmpnam\([^)]*\)\n\{.*?\n\}", source, re.S)[0],
    ]
    (out / "trutien_under_test.h").write_text("\n\n".join(trutien_snippets) + "\n", encoding="utf-8")
    exe = out / ("trutien_va_heap_test.exe" if os.name == "nt" else "trutien_va_heap_test")
    run("Tru Tien VA/heap compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
        str(ROOT / "tests/translation-layer/trutien_va_heap_test.c"), "-o", str(exe)])
    run("Tru Tien VA/heap/vswscanf/tmpnam contracts", [str(exe)])
    iconv = [sys.executable, "tests/translation-layer/run_iconv.py", "--cc", args.cc]
    if args.zig:
        iconv += ["--zig", str(args.zig)]
    if args.sanitize:
        iconv += ["--sanitize"]
    run("iconv", iconv)
    run("Python analyzer/diagnostics/profile/bootstrap", [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_hung_tools.py", "-v"])
    run("USB log mirror contracts", [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-p", "test_ios_logs.py", "-v"])
    run("Swift diagnosis and settings", [sys.executable, "tests/test_hung_swift.py", "-v"])
    run("Swift APK ordering baseline", [sys.executable, "tests/translation-layer/test_apk_order.py"])
    print("Portable checks completed; iOS/JIT/graphics/boot/gameplay still require their own runs.")


if __name__ == "__main__":
    main()
