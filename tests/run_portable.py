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
    net_source = (ROOT / "src/translation-layer-next/husk-tl-bionic-net.c").read_text(encoding="utf-8")
    net_functions = [re.search(r"static int " + name + r"\(int f\)\n\{.*?\n\}", net_source, re.S)[0]
                     for name in ("ai_flags_to_darwin", "ni_flags_to_darwin")]
    (out / "net_flags_under_test.h").write_text("\n".join(net_functions), encoding="utf-8")
    android_source = (ROOT / "src/translation-layer-next/husk-tl-dvm-android.c").read_text(encoding="utf-8")
    publication = [re.search(pattern, android_source, re.S)[0] for pattern in (
        r"uint64_t tl_javaapp_app_tex_publish_with_opacity\([^)]*\)\n\{.*?\n\}",
        r"uint64_t tl_javaapp_app_tex_publish\([^)]*\)\n\{.*?\n\}",
        r"void tl_javaapp_app_frame_pace\([^)]*\)\n\{.*?\n\}",
        r"bool tl_javaapp_app_pixels_rect_publish_with_opacity\([^)]*\)\n\{.*?\n\}",
        r"bool tl_javaapp_app_pixels_rect_publish\([^)]*\)\n\{.*?\n\}",
        r"void tl_javaapp_app_pixels_rect\([^)]*\)\n\{.*?\n\}",
        r"static void app_frame_presented\([^\n]*\)\n\{.*?\n\}")]
    (out / "frame_publication_under_test.h").write_text("\n\n".join(publication), encoding="utf-8")
    graphics_source = (ROOT / "src/translation-layer-next/husk-tl-dvm-graphics.c").read_text(encoding="utf-8")
    graphics_text = [re.search(pattern, graphics_source, re.S)[0] for pattern in (
        r"static CFStringRef cfstr_mapped\([^)]*\)\n\{.*?\n\}",
        r"static CFStringRef cfstr\([^)]*\)\n\{.*?\n\}",
        r"NAT\(G_txMeasure\)\n\{.*?\n\}",
        r"NAT\(G_txBounds\)\n\{.*?\n\}",
        r"NAT\(G_txBreak\)\n\{.*?\n\}",
        r"NAT\(G_txWidths\)\n\{.*?\n\}")]
    (out / "graphics_text_under_test.h").write_text("\n\n".join(graphics_text), encoding="utf-8")
    ui_publication = graphics_source.split("/* ================================================================== the frame on screen */", 1)[1].split(
        "static const struct { const char *name, *sig; dvm_native_fn fn; } k_gfx[]", 1)[0]
    ui_session_begin = re.search(r"static bool javaapp_begin_session\([^)]*\)\n\{.*?\n\}", android_source, re.S)[0]
    (out / "ui_frame_publication_under_test.h").write_text(ui_publication + "\n" + ui_session_begin, encoding="utf-8")
    egl_source = (ROOT / "src/translation-layer-next/husk-tl-egl.c").read_text(encoding="utf-8")
    context_caps = egl_source.split("#define MAX_COMP_CONTEXTS", 1)[1].split("static EGLBoolean w_eglDestroyContext", 1)[0]
    context_caps = "#define MAX_COMP_CONTEXTS" + context_caps
    context_caps += "\n" + re.search(r"static EGLBoolean w_eglMakeCurrent\([^)]*\)\n\{.*?\n\}", egl_source, re.S)[0]
    context_caps += "\n" + re.search(r"void tl_egl_compositor_context\([^)]*\)\n\{.*?\n\}", egl_source, re.S)[0]
    (out / "egl_context_caps_under_test.h").write_text(context_caps, encoding="utf-8")
    surface_lifecycle = [re.search(pattern, egl_source, re.S)[0] for pattern in (
        r"static EGLBoolean w_eglDestroySurface\([^)]*\)\n\{.*?\n\}",
        r"static bool comp_swap\([^)]*\)\n\{.*?\n\}")]
    (out / "surface_lifecycle_under_test.h").write_text("\n\n".join(surface_lifecycle), encoding="utf-8")
    pthread_source = (ROOT / "src/translation-layer-next/husk-tl-bionic-pthread.c").read_text(encoding="utf-8")
    once = re.search(r"static int b_once\([^\n]*\)\n\{.*?\n\}", pthread_source, re.S)[0]
    (out / "pthread_once_under_test.h").write_text(once, encoding="utf-8")
    ndk_source = (ROOT / "src/translation-layer-next/husk-tl-bionic-ndk.c").read_text(encoding="utf-8")
    opacity = [re.search(r"bool tl_nwindow_is_opaque\([^\n]*\)\n\{.*?\n\}", ndk_source, re.S)[0]]
    opacity += [re.search(r"static (?:void|uint64_t) " + name + r"\([^\n]*\)\n\{.*?\n\}", android_source, re.S)[0]
                for name in ("app_frame_draw", "ov_draw")]
    (out / "window_opacity_under_test.h").write_text("\n".join(opacity), encoding="utf-8")
    bionic_source = (ROOT / "src/translation-layer-next/husk-tl-bionic.c").read_text(encoding="utf-8")
    host_facts = [re.search(pattern, bionic_source, re.S)[0] for pattern in (
        r"static const struct \{ const char \*k, \*v; \} k_props\[\] = \{.*?\n\};",
        r"const char \*tl_sysprop\([^\n]*\)\n\{.*?\n\}",
        r"static const void \*bionic___system_property_find\([^\n]*\)\n\{.*?\n\}",
        r"static int bionic___system_property_read\([^\n]*\)\n\{.*?\n\}",
        r"typedef void \(\*guest_prop_cb\)\([^\n]*;",
        r"static void bionic___system_property_read_callback\([^\n]*\)\n\{.*?\n\}",
        r"static int bionic___system_property_get\([^\n]*\)\n\{.*?\n\}",
        r"typedef struct \{[^\n]+\} guest_sysinfo;",
        r"static int bionic_sysinfo\([^\n]*\)\n\{.*?\n\}",
        r"static long bionic_sysconf\([^\n]*\)\n\{.*?\n\}")]
    (out / "bionic_host_facts_under_test.h").write_text("\n".join(host_facts), encoding="utf-8")
    host_device_source = (ROOT / "src/translation-layer-next/husk-tl-host-device.c").read_text(encoding="utf-8")
    density_functions = [re.search(pattern, host_device_source, re.S)[0] for pattern in (
        r"static atomic_int display_density;",
        r"void tl_host_set_display_density\([^\n]*\)\n\{.*?\n\}",
        r"static const char \*display_density_property\([^\n]*\)\n\{.*?\n\}")]
    (out / "host_density_under_test.h").write_text("\n".join(density_functions), encoding="utf-8")
    unity_source = (ROOT / "src/translation-layer-next/husk-tl-unity-app.c").read_text(encoding="utf-8")
    display_facts = re.search(r"void husk_java_set_display_max_refresh\([^)]*\)\n\{.*?\n\}", unity_source, re.S)[0]
    display_facts += "\n" + re.search(r"NAT\(N_refreshRate\) [^\n]+", android_source)[0]
    display_facts += "\n" + re.search(r"NAT\(N_maxRefreshRate\) [^\n]+", android_source)[0]
    display_facts += "\n" + re.search(r"static tl_aconfig \*b_AConfiguration_new\([^)]*\)\n\{.*?\n\}", ndk_source, re.S)[0]
    display_facts += "\n" + re.search(r"static void b_AConfiguration_fromAssetManager\([^)]*\)\n\{.*?\n\}", ndk_source, re.S)[0]
    (out / "display_facts_under_test.h").write_text(display_facts, encoding="utf-8")
    loader_source = (ROOT / "src/translation-layer-next/husk-tl-ld.c").read_text(encoding="utf-8")
    entry_install = re.search(r"static bool patch_trutien_text_entries\([^\n]*\)\n\{.*?\n\}", loader_source, re.S)[0]
    (out / "entry_filter_install_under_test.h").write_text(entry_install, encoding="utf-8")
    entry_bridges = [re.search(r"__attribute__\(\(naked, used\)\) void " + name + r"\([^\n]*\)\n\{.*?\n\}", loader_source, re.S)[0]
                     for name in ("tl_entry_filter_common", "tl_entry_filter_return")]
    (out / "entry_filter_bridges_under_test.h").write_text("\n".join(entry_bridges), encoding="utf-8")
    jni_source = (ROOT / "src/translation-layer-next/husk-tl-jni.c").read_text(encoding="utf-8")
    jni_allocations = [re.search(pattern, jni_source, re.S)[0] for pattern in (
        r"static jobj \*obj_alloc\([^\n]*\)\n\{.*?\n\}",
        r"jobj \*tl_jni_ref\([^\n]*\) \{[^\n]+\}",
        r"void tl_jni_unref\([^\n]*\)\n\{.*?\n\}",
        r"static uint8_t esz_of\([^\n]*\)\n\{.*?\n\}",
        r"jobj \*tl_jni_new_prim_array\([^\n]*\)\n\{.*?\n\}",
        r"jobj \*tl_jni_new_obj_array\([^\n]*\)\n\{.*?\n\}")]
    (out / "jni_allocation_under_test.h").write_text("\n".join(jni_allocations), encoding="utf-8")
    for name in ("session_test", "guest_pool_test", "net_flags_test", "memory_info_test", "aot_data_test", "frame_sync_test", "frame_perf_test", "frame_publication_test", "ui_frame_publication_test", "surface_lifecycle_test", "egl_context_caps_test", "pthread_once_test", "window_opacity_test"):
        exe = out / (name + (".exe" if os.name == "nt" else ""))
        thread_flags = ["-pthread"] if os.name != "nt" and name == "session_test" else []
        run(name + " compile", compiler + flags + thread_flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
            str(ROOT / f"tests/translation-layer/{name}.c"), "-o", str(exe)])
        run(name, [str(exe)])
    for name in ("host_device_test", "bionic_host_facts_test", "display_facts_test", "text_color_test", "graphics_text_test", "trutien_text_filter_test", "entry_filter_test"):
        exe = out / (name + (".exe" if os.name == "nt" else ""))
        thread_flags = ["-pthread"] if os.name != "nt" and name == "trutien_text_filter_test" else []
        math_flags = ["-lm"] if os.name != "nt" and name == "graphics_text_test" else []
        run(name + " compile", compiler + flags + thread_flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
            str(ROOT / f"tests/translation-layer/{name}.c"), "-o", str(exe)] + math_flags)
        run(name, [str(exe)])
    jni_exe = out / ("jni_allocation_test.exe" if os.name == "nt" else "jni_allocation_test")
    jni_threads = ["-pthread"] if os.name != "nt" else []
    run("JNI allocation accounting compile", compiler + flags + jni_threads + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
        str(ROOT / "tests/translation-layer/jni_allocation_test.c"), "-o", str(jni_exe)])
    run("JNI allocation accounting", [str(jni_exe)])
    probe_exe = out / ("trutien_text_probe_test" + (".exe" if os.name == "nt" else ""))
    run("trutien text probe compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}",
        str(ROOT / "tests/translation-layer/trutien_text_probe_test.c"),
        str(ROOT / "src/translation-layer-next/husk-tl-trutien-text.c"), "-o", str(probe_exe)])
    run("trutien text probe", [str(probe_exe)])
    if platform.system() == "Darwin" and platform.machine() in ("arm64", "aarch64"):
        exe = out / "entry_filter_apple_arm64"
        run("Apple ARM64 entry filter ABI compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
            str(ROOT / "tests/translation-layer/entry_filter_apple_arm64.c"),
            str(ROOT / "src/translation-layer-next/husk-tl-trutien-text.c"), "-pthread", "-o", str(exe)])
        run("actual Apple ARM64 text entry/return ABI", [str(exe)])
    for name in ("plan_test", "elf_import_test"):
        exe = out / (name + (".exe" if os.name == "nt" else ""))
        run(name + " compile", compiler + flags + [str(ROOT / f"tests/translation-layer/{name}.c"), str(ROOT / "src/translation-layer/husk-tl-elf.c"), "-o", str(exe)])
        run(name, [str(exe)])
    io_source = (ROOT / "src/translation-layer-next/husk-tl-bionic-io.c").read_text(encoding="utf-8")
    madvise = [re.search(pattern, io_source, re.S)[0] for pattern in (
        r"static void anon_zap\([^\n]*\)\n\{.*?\n\}",
        r"static int b_madvise\([^\n]*\)\n\{.*?\n\}")]
    madvise_case = "    case 233:" + io_source.split("    case 233:", 1)[1].split("\n    case ", 1)[0]
    madvise += ["static intptr_t syscall_madvise_under_test(intptr_t a0, intptr_t a1, intptr_t a2, intptr_t nr)\n"
        "{ (void)a0; (void)a1; (void)a2; switch (nr) {\n" + madvise_case + "\n    default: return -38;\n} }\n"]
    (out / "madvise_under_test.h").write_text("\n\n".join(madvise), encoding="utf-8")
    exe = out / ("madvise_dispatch_test.exe" if os.name == "nt" else "madvise_dispatch_test")
    run("raw madvise compile", compiler + flags + [f"-I{out}", str(ROOT / "tests/translation-layer/madvise_dispatch_test.c"), "-o", str(exe)])
    run("raw madvise reclaim and Linux range validation", [str(exe)])
    run("raw madvise 4 KiB host compile", compiler + flags + ["-DPAGE=4096", f"-I{out}", str(ROOT / "tests/translation-layer/madvise_dispatch_test.c"), "-o", str(exe)])
    run("raw madvise 4 KiB host range boundaries", [str(exe)])
    source = (ROOT / "src/translation-layer-next/husk-tl-bionic-str.c").read_text(encoding="utf-8")
    wrapper = re.search(r"static int b_getchar\(void\) \{[^\n]+\}", source)
    assert wrapper and 'TL_WRAP("getchar", b_getchar)' in source
    (out / "getchar_under_test.h").write_text(wrapper[0] + "\n", encoding="utf-8")
    (out / "stdin.bin").write_bytes(b"A\xff")
    exe = out / ("getchar_test.exe" if os.name == "nt" else "getchar_test")
    run("getchar compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}", str(ROOT / "tests/translation-layer/getchar_test.c"), "-o", str(exe)])
    run("getchar", [str(exe), str(out / "stdin.bin")])
    io_source = (ROOT / "src/translation-layer-next/husk-tl-bionic-io.c").read_text(encoding="utf-8")
    log_paths = [re.search(pattern, io_source, re.S)[0] for pattern in (
        r"static bool trutien_log_leaf\([^)]*\)\n\{.*?\n\}",
        r"const char \*tl_path_resolve\([^)]*\)\n\{.*?\n\}")]
    (out / "trutien_log_resolver_under_test.h").write_text("\n".join(log_paths), encoding="utf-8")
    log_fixture = out / "trutien-log-fixture"
    (log_fixture / "sdcard/Android/data/vn.trutien.client/files/element_astc_v2").mkdir(parents=True, exist_ok=True)
    exe = out / ("trutien_log_path_test.exe" if os.name == "nt" else "trutien_log_path_test")
    run("Tru Tien log path compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
        str(ROOT / "tests/translation-layer/trutien_log_path_test.c"), str(ROOT / "src/translation-layer-next/husk-tl-path.c"), "-o", str(exe)])
    run("Tru Tien deleted log recreation and lookup isolation", [str(exe), log_fixture.resolve().as_posix()])
    directory_parts = [re.search(pattern, io_source, re.S)[0] for pattern in (
        r"typedef struct \{ uint64_t d_ino;[^\n]+guest_dirent;",
        r"typedef struct \{ DIR \*dir;[^\n]+guest_dir;",
        r"static void \*b_opendir\([^)]*\)\n\{.*?\n\}",
        r"static int b_closedir\([^\n]+")]
    if os.name != "nt":
        directory_parts.append(re.search(r"static void \*b_readdir\([^)]*\)\n\{.*?\n\}", io_source, re.S)[0])
    directory_parts.insert(2, "static guest_dir *jar_opendir(const char *p) { (void)p; return NULL; }")
    (out / "relative_directory_under_test.h").write_text("\n".join(directory_parts), encoding="utf-8")
    cwd_fixture = out / "relative-cwd-fixture"
    cwd_fixture.mkdir(exist_ok=True)
    exe = out / ("relative_cwd_test.exe" if os.name == "nt" else "relative_cwd_test")
    run("Relative cwd directory compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
        str(ROOT / "tests/translation-layer/relative_cwd_test.c"), str(ROOT / "src/translation-layer-next/husk-tl-path.c"), "-o", str(exe)])
    run("VHTG current-directory script recursion", [str(exe), cwd_fixture.resolve().as_posix()])
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
        pthread_source = (ROOT / "src/translation-layer-next/husk-tl-bionic-pthread.c").read_text(encoding="utf-8")
        registry = "typedef struct session_thread" + pthread_source.split("typedef struct session_thread", 1)[1].split("const char *tl_ld_symbol_at", 1)[0]
        join = re.search(r"int tl_guest_pthread_join\([^)]*\)\n\{.*?\n\}", pthread_source, re.S)[0]
        (out / "thread_registry_under_test.h").write_text(registry + "\n" + join, encoding="utf-8")
        exe = out / "thread_registry_test"
        run("thread registry compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
            str(ROOT / "tests/translation-layer/thread_registry_test.c"), "-pthread", "-o", str(exe)])
        run("thread registry including blocked TLS teardown", [str(exe)])
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
        re.search(r"int tl_count_scan_args\([^)]*\)\n\{.*?\n\}", va_source, re.S)[0],
        re.search(r"int tl_scan_string\([^)]*\)\n\{.*?\n\}", va_source, re.S)[0],
        re.search(r"int tl_scan_file\([^)]*\)\n\{.*?\n\}", va_source, re.S)[0],
        re.search(r"int tl_count_wide_scan_args\([^)]*\)\n\{.*?\n\}", va_source, re.S)[0],
        re.search(r"int tl_scan_wide\([^)]*\)\n\{.*?\n\}", va_source, re.S)[0],
        re.search(r"static char \*b_tmpnam\([^)]*\)\n\{.*?\n\}", source, re.S)[0],
    ]
    (out / "trutien_under_test.h").write_text("\n\n".join(trutien_snippets) + "\n", encoding="utf-8")
    exe = out / ("trutien_va_heap_test.exe" if os.name == "nt" else "trutien_va_heap_test")
    run("Tru Tien VA/heap compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}", f"-I{out}",
        str(ROOT / "tests/translation-layer/trutien_va_heap_test.c"), "-o", str(exe)])
    run("Tru Tien VA/heap/vswscanf/tmpnam contracts", [str(exe)])
    exe = out / ("opensles_player_test.exe" if os.name == "nt" else "opensles_player_test")
    sl_libs = (["-pthread"] if os.name != "nt" else []) + (["-lm"] if os.name != "nt" else [])
    run("OpenSL ES player compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}",
        str(ROOT / "tests/translation-layer/opensles_player_test.c"), "-o", str(exe)] + sl_libs)
    run("OpenSL ES URI/FD/PCM player and seek/loop contracts", [str(exe)])
    dvm_audio_source = (ROOT / "src/translation-layer-next/husk-tl-dvm-audio.c").read_text(encoding="utf-8")
    read_fd_snippet = re.search(r"static uint8_t \*read_fd\([^)]*\)\n\{.*?\n\}", dvm_audio_source, re.S)
    assert read_fd_snippet
    (out / "audio_fd_under_test.h").write_text(read_fd_snippet[0] + "\n", encoding="utf-8")
    exe = out / ("audio_fd_test.exe" if os.name == "nt" else "audio_fd_test")
    run("Audio FD read compile", compiler + flags + [f"-I{out}",
        str(ROOT / "tests/translation-layer/audio_fd_test.c"), "-o", str(exe)])
    run("Audio FD clamping and closed-FD contracts", [str(exe)])
    path_fixture = out / "path-cache-fixture"
    (path_fixture / "UserData").mkdir(parents=True, exist_ok=True)
    (path_fixture / "UserData/SystemSettings.ini").write_text("[Video]\n", encoding="utf-8")
    exe = out / ("path_cache_test.exe" if os.name == "nt" else "path_cache_test")
    run("path cache compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}",
        str(ROOT / "tests/translation-layer/path_cache_test.c"),
        str(ROOT / "src/translation-layer-next/husk-tl-path.c"), "-o", str(exe)])
    run("path case cache and invalidation contracts", [str(exe), path_fixture.resolve().as_posix()])
    exe = out / ("thread_clock_test.exe" if os.name == "nt" else "thread_clock_test")
    run("thread clock compile", compiler + flags + [f"-I{ROOT / 'src/translation-layer-next'}",
        str(ROOT / "tests/translation-layer/thread_clock_test.c"), "-o", str(exe)])
    run("per-thread CPU clock and schedstat contracts", [str(exe)])
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
