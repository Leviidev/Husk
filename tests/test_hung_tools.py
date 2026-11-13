# SPDX-License-Identifier: GPL-2.0-or-later
import hashlib
import importlib.util
import json
from pathlib import Path
import plistlib
import struct
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


compat = module("hung_compat", "tools/hung_compat.py")
bootstrap = module("hung_ci", "scripts/prepare_hung_ci.py")
native_stage = module("hung_native", "scripts/stage_hung_dependencies.py")
framework_stage = module("hung_framework", "scripts/stage_hung_framework.py")


def binary_manifest():
    values = ["manifest", "package", "test.game", "uses-sdk", "targetSdkVersion", "uses-permission", "name", "android.permission.INTERNET", "uses-feature", "glEsVersion"]
    payload, offsets = bytearray(), []
    for value in values:
        b = value.encode()
        offsets.append(len(payload)); payload.extend(bytes([len(value), len(b)]) + b + b"\0")
    payload.extend(b"\0" * (-len(payload) % 4))
    start = 28 + 4 * len(values)
    pool = struct.pack("<HH6I", 1, 28, start + len(payload), len(values), 0, 256, start, 0)
    pool += struct.pack(f"<{len(values)}I", *offsets) + payload
    def node(name, key, value, typ):
        attr = struct.pack("<IIIHBBI", 0xFFFFFFFF, key, 0xFFFFFFFF, 8, 0, typ, value)
        head = struct.pack("<HHIII", 0x102, 16, 56, 1, 0xFFFFFFFF)
        return head + struct.pack("<II6H", 0xFFFFFFFF, name, 20, 20, 1, 0, 0, 0) + attr
    body = pool + node(0, 1, 2, 3) + node(3, 4, 34, 0x10) + node(5, 6, 7, 3) + node(8, 9, 0x30000, 0x11)
    return struct.pack("<HHI", 3, 8, 8 + len(body)) + body


def elf_fixture(machine=183, imports=3):
    b = bytearray(32768)
    b[:7] = b"\x7fELF\x02\x01\x01"
    struct.pack_into("<HHI", b, 16, 3, machine, 1)
    struct.pack_into("<Q", b, 32, 64)
    struct.pack_into("<HHH", b, 52, 64, 56, 2)
    struct.pack_into("<II6Q", b, 64, 1, 4, 0, 0, 0, len(b), len(b), 16384)
    struct.pack_into("<II6Q", b, 120, 2, 4, 0x200, 0x200, 0, 7*16, 7*16, 8)
    for i, (tag, val) in enumerate(((5, 0x4000), (10, 0x2000), (6, 0x1000), (11, 24), (4, 0x400), (1, 1), (0, 0))):
        struct.pack_into("<QQ", b, 0x200+i*16, tag, val)
    struct.pack_into("<II", b, 0x400, 1, imports+2)
    b[0x4001:0x4009] = b"libc.so\0"
    off = 9
    for i in range(1, imports+2):
        name = ("SDL_main" if i == imports+1 else ("iconv_open", "iconv", "iconv_close")[(i-1) % 3]).encode() + b"\0"
        struct.pack_into("<IBBHQQ", b, 0x1000+i*24, off, 0x22 if i == 2 else 0x12, 0, 1 if i > imports else 0, 0, 0)
        b[0x4000+off:0x4000+off+len(name)] = name; off += len(name)
    return bytes(b)


class AnalyzerTests(unittest.TestCase):
    def test_binary_manifest_typed_attributes(self):
        m = compat.manifest(binary_manifest())
        self.assertEqual(m["package"], "test.game")
        self.assertEqual(m["targetSdk"], 34)
        self.assertEqual(m["permissions"], ["android.permission.INTERNET"])
        self.assertEqual(m["glesVersion"], 0x30000)

    def test_elf_without_section_headers(self):
        r = compat.elf_report(elf_fixture())
        self.assertTrue(r["analysisComplete"])
        self.assertEqual(r["needed"], ["libc.so"])
        self.assertEqual([i["name"] for i in r["imports"]], ["iconv_open", "iconv", "iconv_close"])
        self.assertTrue(r["imports"][1]["weak"])
        self.assertIn("SDL_main", r["exports"])

    def test_elf_corruption_is_reported(self):
        for b in (b"not ELF", elf_fixture()[:90]):
            with self.assertRaises(ValueError):
                compat.elf_report(b)

    def test_elf_gnu_hash_without_sysv_hash(self):
        b = bytearray(elf_fixture())
        struct.pack_into("<Q", b, 0x200 + 4 * 16, 0x6FFFFEF5)
        struct.pack_into("<4I", b, 0x400, 1, 1, 1, 5)
        struct.pack_into("<Q", b, 0x410, 0)
        struct.pack_into("<I", b, 0x418, 1)
        struct.pack_into("<4I", b, 0x41C, 2, 2, 2, 3)
        r = compat.elf_report(bytes(b))
        self.assertTrue(r["analysisComplete"])
        self.assertEqual(len(r["imports"]), 3)
        self.assertIn("SDL_main", r["exports"])

    def test_elf32_arm_dynamic_symbols(self):
        b = bytearray(8192)
        b[:7] = b"\x7fELF\x01\x01\x01"
        struct.pack_into("<HHI", b, 16, 3, 40, 1)
        struct.pack_into("<I", b, 28, 52)
        struct.pack_into("<HHH", b, 40, 52, 32, 2)
        struct.pack_into("<8I", b, 52, 1, 0, 0, 0, len(b), len(b), 4, 4096)
        struct.pack_into("<8I", b, 84, 2, 0x200, 0x200, 0, 56, 56, 4, 4)
        for i, pair in enumerate(((5, 0x1000), (10, 64), (6, 0x400), (11, 16), (4, 0x300), (1, 1), (0, 0))):
            struct.pack_into("<iI", b, 0x200 + i * 8, *pair)
        struct.pack_into("<II", b, 0x300, 1, 2)
        b[0x1001:0x1009] = b"libc.so\0"
        b[0x1009:0x100F] = b"iconv\0"
        struct.pack_into("<IIIBBH", b, 0x410, 9, 0, 0, 0x12, 0, 0)
        r = compat.elf_report(bytes(b))
        self.assertEqual((r["bits"], r["machine"]), (32, 40))
        self.assertEqual(r["needed"], ["libc.so"])
        self.assertEqual(r["imports"], [{"name": "iconv", "weak": False}])

    def test_declared_abi_machine_mismatch(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / "mismatch.apk"
            with zipfile.ZipFile(p, "w") as z:
                z.writestr("AndroidManifest.xml", binary_manifest())
                z.writestr("lib/arm64-v8a/libmain.so", elf_fixture(62))
            r = compat.analyze([p])
            self.assertFalse(r["libraries"][0]["abiMatchesELF"])
            self.assertIn("does not match", r["warnings"][0])

    def test_apk_engine_hash_and_no_compatibility_claim(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / "game.apk"
            with zipfile.ZipFile(p, "w") as z:
                z.writestr("AndroidManifest.xml", binary_manifest())
                z.writestr("lib/arm64-v8a/libSDL3.so", elf_fixture())
                z.writestr("lib/x86_64/libSDL3.so", elf_fixture(62))
            r = compat.analyze([p])
            self.assertEqual(r["assessment"], "NOT TESTED")
            self.assertEqual(r["engines"][0]["name"], "SDL3")
            self.assertEqual(r["abis"], ["arm64-v8a", "x86_64"])
            self.assertEqual(r["apkFingerprint"], hashlib.sha256(p.read_bytes()).hexdigest())
            self.assertEqual(r["execution"]["translation"], "candidate")
            self.assertFalse(r["execution"]["automaticFallback"])
            self.assertTrue(r["internetDeclared"])

    def test_x86_is_not_solved_by_arm_guest(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / "x86.apk"
            with zipfile.ZipFile(p, "w") as z:
                z.writestr("AndroidManifest.xml", binary_manifest())
                z.writestr("lib/x86_64/libmain.so", elf_fixture(62))
            r = compat.analyze([p])
            self.assertEqual(r["execution"]["translation"], "unsupported ABI")
            self.assertTrue(r["warnings"])

    def test_dex_method_references(self):
        b = bytearray(256)
        b[:8] = b"dex\n035\0"
        struct.pack_into("<4I", b, 56, 2, 112, 1, 120)
        struct.pack_into("<2I", b, 88, 1, 124)
        struct.pack_into("<2I", b, 112, 144, 180)
        struct.pack_into("<I", b, 120, 0)
        struct.pack_into("<HHI", b, 124, 0, 0, 1)
        a = b"Landroid/app/Activity;"
        b[144:144+len(a)+2] = bytes([len(a)]) + a + b"\0"
        b[180:190] = b"\x08onCreate\0"
        self.assertEqual(compat.dex_api_refs(bytes(b)), ["Landroid/app/Activity;->onCreate"])

    def test_malformed_manifest_and_split_package_mismatch(self):
        with self.assertRaises((ValueError, IndexError)):
            compat.manifest(binary_manifest()[:-1])
        with tempfile.TemporaryDirectory() as temp:
            paths = []
            for name in ("one", "two"):
                p = Path(temp) / (name + ".apk"); paths.append(p)
                with zipfile.ZipFile(p, "w") as z:
                    z.writestr("AndroidManifest.xml", f'<manifest package="{name}"/>')
            with self.assertRaises(ValueError):
                compat.analyze(paths)


class DiagnosticTests(unittest.TestCase):
    def test_trutien_supplied_log_distinguishes_called_from_loaded(self):
        text = """[husk-jit] selftest: PASS
ld: libc++.so: unresolved import _Unwind_RaiseException
ld: libmain.so: unresolved import iconv_open
ld: libmain.so: unresolved import iconv
ld: CALLED an unresolved import: iconv_open (from libmain.so ?+0)
ld: CALLED an unresolved import: iconv (from libmain.so ?+0)
I/TruTien: update: release 000067 complete"""
        r = compat.diagnose(text)
        rows = {i["symbol"]: i for i in r["issues"]}
        self.assertTrue(r["jitSelftestPassed"])
        self.assertEqual(rows["iconv"]["code"], "NATIVE_SYMBOL_CALLED")
        self.assertEqual(rows["_Unwind_RaiseException"]["severity"], "info")
        self.assertEqual(r["gameplay"], "NOT TESTED")
        self.assertIn("unverified", r["rootCause"])

    def test_frames_and_update_complete_are_not_gameplay_pass(self):
        r = compat.diagnose("startup: heartbeat nativeRunMain=1 frames=1000 delta=900\nupdate: complete", True)
        self.assertEqual(r["gameplay"], "NOT TESTED")
        self.assertEqual(r["issues"][0]["code"], "STARTUP_WINDOW_EXPIRED")

    def test_diagnostic_categories_have_evidence_and_remediation(self):
        text = "\n".join(["no StikDebug JIT region", "jni: pending exception java/lang/Exception", "ANGLE init failed", "expansion file (OBB) is missing", "file: open(data) -> -1 ENOENT", "SocketTimeoutException", "SecurityException permission EACCES", "=== CRASH: signal 11", "OutOfMemoryError", "QEMU boot failed"])
        r = compat.diagnose(text)
        self.assertGreaterEqual(len(r["issues"]), 10)
        for issue in r["issues"]:
            for key in ("code", "severity", "status", "evidence", "remediation"):
                self.assertTrue(issue[key])


class ProfileTests(unittest.TestCase):
    def test_splash_frames_never_pass(self):
        self.assertEqual(compat.result_status({"launch": "PASS", "rendering": "PASS"}), "PARTIAL")
        self.assertEqual(compat.result_status({}), "NOT TESTED")
        self.assertEqual(compat.result_status({s: "PASS" for s in compat.STAGES}), "PASS")
        self.assertEqual(compat.result_status({"gameplay": "FAIL"}), "FAIL")
        self.assertEqual(compat.result_status({"launch": "BLOCKED"}), "BLOCKED")

    def test_atomic_profile_roundtrip_and_no_accidental_overwrite(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / "profile.json"
            compat.write_json(p, {"result": "NOT TESTED"}, exclusive=True)
            with self.assertRaises(FileExistsError):
                compat.write_json(p, {}, exclusive=True)
            compat.write_json(p, {"result": "PARTIAL"})
            self.assertEqual(json.loads(p.read_text())["result"], "PARTIAL")


class BootstrapTests(unittest.TestCase):
    def test_rebuilt_java_framework_staging_checks_both_dex_before_copying(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "java-artifact"
            source.mkdir()
            target = root / "build/java-runtime"
            target.mkdir(parents=True)
            provenance = root / "build/support-provenance.json"
            provenance.write_text(json.dumps({"upstreamVersion": "3.3"}))
            files = {}
            for name in ("husk-framework.dex", "husk-framework-core.dex"):
                data = b"dex\n035\0" + name.encode()
                (source / name).write_bytes(data)
                (target / name).write_bytes(b"old")
                files[name] = {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
            metadata = {"files": files, "r8SHA256": "test compiler hash"}
            (source / "java-framework-build.json").write_text(json.dumps(metadata))
            (source / "husk-framework-core.dex").write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError, "mismatch"):
                framework_stage.stage(source, root)
            self.assertEqual((target / "husk-framework.dex").read_bytes(), b"old")
            core = b"dex\n035\0husk-framework-core.dex"
            (source / "husk-framework-core.dex").write_bytes(core)
            framework_stage.stage(source, root)
            self.assertEqual((target / "husk-framework-core.dex").read_bytes(), core)
            record = json.loads(provenance.read_text())
            self.assertEqual(record["upstreamVersion"], "3.3")
            self.assertEqual(record["javaFramework"]["files"], files)
            metadata["files"]["../unexpected.dex"] = {}
            (source / "java-framework-build.json").write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError, "exactly"):
                framework_stage.stage(source, root)

    def prepare_fixture(self, temp, extra=None):
        root = Path(temp)
        (root / "src/app").mkdir(parents=True)
        (root / "src/app/project.yml").write_text("targets:\n" + bootstrap.PS4_DEPENDENCY)
        p = root / "support.ipa"
        with zipfile.ZipFile(p, "w") as z:
            z.writestr("Payload/Husk.app/Info.plist", plistlib.dumps({"CFBundleShortVersionString": "3.3", "CFBundleVersion": "148"}))
            for rel in bootstrap.REQUIRED:
                z.writestr("Payload/Husk.app/" + rel, b"fixture")
            z.writestr("Payload/Husk.app/Husk", b"must not be copied")
            z.writestr("Payload/Husk.app/Frameworks/libhuskps4.dylib", b"must not be copied")
            if extra:
                z.writestr(extra, b"invalid")
        return root, p, compat.digest(p)

    def test_verified_resource_only_bootstrap_and_optional_ps4(self):
        with tempfile.TemporaryDirectory() as temp:
            root, p, sha = self.prepare_fixture(temp)
            r = bootstrap.prepare(p, sha, root)
            self.assertFalse(r["ps4Included"])
            self.assertTrue((root / "build/java-runtime/husk-framework.dex").is_file())
            self.assertFalse((root / "build/Husk").exists())
            self.assertNotIn("libhuskps4", (root / "src/app/project.hung.generated.yml").read_text())

    def test_sha_and_archive_path_rejection(self):
        with tempfile.TemporaryDirectory() as temp:
            root, p, sha = self.prepare_fixture(temp, "Payload/Husk.app/java-runtime/../../escape")
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                bootstrap.prepare(p, "0" * 64, root)
            with self.assertRaisesRegex(ValueError, "unsafe"):
                bootstrap.prepare(p, sha, root)

    def test_pinned_native_dependencies_without_app_executable(self):
        with tempfile.TemporaryDirectory() as temp:
            root, p, _ = self.prepare_fixture(temp)
            with zipfile.ZipFile(p, "a") as z:
                for name in native_stage.NATIVE:
                    z.writestr("Payload/Husk.app/" + name, struct.pack("<II", 0xFEEDFACF, 0x0100000C) + b"\0" * 24)
                for name in native_stage.OTHER:
                    z.writestr("Payload/Husk.app/" + name, b"fixture")
                z.writestr("Payload/Husk.app/edk2-aarch64-code.fd", b"firmware")
            rows = native_stage.stage(p, compat.digest(p), root)
            self.assertGreater(len(rows), 6)
            self.assertTrue((root / "build/ios-arm64/lib/libqemu-aarch64-softmmu.dylib").exists())
            self.assertFalse((root / "build/Husk").exists())
            wrapper = plistlib.loads((root / "build/ios-arm64/lib/MoltenVK.xcframework/Info.plist").read_bytes())
            self.assertEqual(wrapper["AvailableLibraries"][0]["SupportedPlatform"], "ios")
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                native_stage.stage(p, "0" * 64, root)


class SourceRegressionTests(unittest.TestCase):
    def test_hung_goals_and_regression_guards(self):
        bionic_c = (ROOT / "src/translation-layer-next/husk-tl-bionic.c").read_text(encoding="utf-8")
        self.assertIn('TL_WRAP("sched_yield", tl_bionic_sched_yield)', bionic_c)
        self.assertNotIn("replaced below", bionic_c)

        settings_swift = (ROOT / "src/app/Husk/SettingsTab.swift").read_text(encoding="utf-8")
        self.assertIn("IncomingFiles.folder", settings_swift)
        self.assertNotIn("IncomingFiles.directory", settings_swift)

        item_pages = (ROOT / "src/app/Husk/ItemPages.swift").read_text(encoding="utf-8")
        launcher = (ROOT / "src/app/Husk/Launcher.swift").read_text(encoding="utf-8")
        self.assertNotIn(".id(jit.attachGeneration)", item_pages)
        self.assertNotIn(".id(jit.attachGeneration)", launcher)
        self.assertNotIn(".fileImporter", item_pages)
        self.assertIn("HuskFilePicker.present", item_pages)
        self.assertIn("pickUpdateAPK", item_pages)

        unity_view = (ROOT / "src/app/Husk/TLUnityView.swift").read_text(encoding="utf-8")
        tl_swift = (ROOT / "src/app/Husk/TranslationLayer.swift").read_text(encoding="utf-8")
        self.assertIn("TLFloatingReopenHandle", unity_view)
        self.assertIn("TLGameSwitcherSheet", unity_view)
        self.assertIn("TLFloatingReopenHandle", tl_swift)
        self.assertIn("TLGameSwitcherSheet", tl_swift)
        self.assertIn("StorageCleaner.removeGameCaches(in: existing)", tl_swift)


if __name__ == "__main__":
    unittest.main()
