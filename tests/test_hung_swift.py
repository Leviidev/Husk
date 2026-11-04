# SPDX-License-Identifier: GPL-2.0-or-later
"""Compile the production diagnosis/settings models with Foundation-only host stubs.

UI rendering and iOS integration still require Xcode/device checks.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SwiftModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = shutil.which("swiftc")
        if not cls.compiler:
            raise unittest.SkipTest("swiftc unavailable: Swift model tests require a Swift toolchain")

    def compile_run(self, source, main):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp)
            (p / "Model.swift").write_text(source, encoding="utf-8")
            (p / "main.swift").write_text("import Foundation\n" + main, encoding="utf-8")
            exe = p / "models"
            subprocess.run([self.compiler, str(p / "Model.swift"), str(p / "main.swift"), "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_production_diagnosis_classification(self):
        source = (ROOT / "src/app/Husk/AppDiagnosis.swift").read_text(encoding="utf-8")
        source = source.split("/// A diagnosis, as a card:")[0].replace("import SwiftUI", "import Foundation")
        source += '''
struct TLApp { var report: TLReport?; var apks: [String] = [] }
struct TLReport {
    var ok = true; var verdict = "native"; var error: String?; var summary = ""
    var abis: [String] = []; var libraries: [TLLibrary] = []
}
struct TLLibrary { var name: String }
func husk_apk_split_required(_ path: String) -> Int { 0 }
'''
        self.compile_run(source, '''
let warning = "ld: libmain.so: unresolved import iconv_open"
precondition(AppDiagnosis.fromLog(warning) == nil)
let called = "ld: CALLED an unresolved import: iconv_open (from libmain.so ?+0)"
precondition(AppDiagnosis.fromLog(called) == nil)
let d = AppDiagnosis.fromLog(called + "\\nthe app finished")!
precondition(d.errorCode == "NATIVE_SYMBOL_CALLED" && d.verificationStatus == "UNVERIFIED")
precondition(d.detail.contains("only cause"))
precondition(AppDiagnosis.fromLog("no StikDebug JIT region")?.kind == .noJIT)
precondition(AppDiagnosis.fromLog("ANGLE initialization failed") == nil)
precondition(AppDiagnosis.fromLog("ANGLE initialization failed\\nthe app finished")?.kind == .graphicsFailure)
precondition(AppDiagnosis.fromLog("SocketTimeoutException\\nthe app finished")?.kind == .networkTimeout)
precondition(AppDiagnosis.fromLog("file: open(cache) -> -1 ENOENT") == nil)
precondition(AppDiagnosis.fromLog("file: open(data) -> -1 ENOENT\\nthe app finished")?.kind == .fileAccess)
precondition(AppDiagnosis.fromLog("SecurityException Permission denied\\nthe app finished")?.kind == .permissionFailure)
precondition(AppDiagnosis.fromLog("QEMU boot failed")?.kind == .emulatorBoot)
precondition(AppDiagnosis.fromLog("startup: heartbeat frames=1000 delta=900") == nil)
print("PASS: production Swift diagnostics preserve uncertainty and ignore recoverable warnings")
''')

    def test_production_settings_legacy_decode_and_roundtrip(self):
        source = (ROOT / "src/app/Husk/TLAppSettings.swift").read_text(encoding="utf-8")
        source = source.split("// MARK: - The settings page")[0]
        source = source.replace("import SwiftUI", "import Foundation").replace("import UIKit", "")
        source += '''
enum TranslationLayer { static var root: URL { URL(fileURLWithPath: NSTemporaryDirectory()) } }
@MainActor final class UIScreen { static let main = UIScreen(); var scale: CGFloat { 3 } }
'''
        self.compile_run(source, '''
let old = try JSONDecoder().decode(TLAppSettings.self, from: Data("{}".utf8))
precondition(old.startupDiagnostics == false && old.resolution == .high)
var settings = old
settings.startupDiagnostics = true
settings.orientation = .portrait
let encoded = try JSONEncoder().encode(settings)
let decoded = try JSONDecoder().decode(TLAppSettings.self, from: encoded)
precondition(decoded == settings)
precondition(decoded.startupDiagnostics && decoded.orientation == .portrait)
print("PASS: per-app diagnostic profile survives Codable roundtrip and legacy settings")
''')


if __name__ == "__main__":
    unittest.main()
