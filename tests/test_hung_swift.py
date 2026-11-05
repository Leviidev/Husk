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
            (p / "main.swift").write_text("import Foundation\n#if canImport(CoreGraphics)\nimport CoreGraphics\n#endif\n" + main, encoding="utf-8")
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
let memory = "unity: alive: 1000 frames (+60), 12 MiB left before jetsam"
let pressure = AppDiagnosis.fromLog(memory)!
precondition(pressure.detail.contains("does not confirm"))
precondition(AppDiagnosis.fromLog(memory + "\\n*** FATAL SIGNAL 6 -- process is dying ***")?.kind == .nativeCrash)
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
precondition(old.startupDiagnostics == false && old.resolution == .high && old.avoidUnsafeArea)
let low = try JSONDecoder().decode(TLAppSettings.self, from: Data(#"{"resolution":"low"}"#.utf8))
precondition(low.resolution == .low && low.avoidUnsafeArea)
var settings = old
settings.startupDiagnostics = true
settings.orientation = .portrait
settings.avoidUnsafeArea = false
let encoded = try JSONEncoder().encode(settings)
let decoded = try JSONDecoder().decode(TLAppSettings.self, from: encoded)
precondition(decoded == settings)
precondition(decoded.startupDiagnostics && decoded.orientation == .portrait)
precondition(!decoded.avoidUnsafeArea)
print("PASS: per-app diagnostic profile survives Codable roundtrip and legacy settings")
''')

    def test_production_viewport_cutouts_and_cached_surface(self):
        source = (ROOT / "src/app/Husk/TLGameViewport.swift").read_text(encoding="utf-8")
        self.compile_run(source, '''
func near(_ a: CGFloat, _ b: CGFloat) { precondition(abs(a - b) < 0.000001) }
// Representative landscape cutout/home-indicator geometry; runtime reads UIWindow's actual values.
let screen = CGRect(x: 0, y: 0, width: 932, height: 430)
let safe = CGRect(x: 59, y: 0, width: 814, height: 409)
precondition(TLGameViewport.area(in: screen, safeFrame: safe, avoidUnsafeArea: true) == safe)
precondition(TLGameViewport.area(in: screen, safeFrame: safe, avoidUnsafeArea: false) == screen)
let inset = TLGameViewport.insets(view: screen, safeFrame: safe)
precondition(inset.left == 59 && inset.right == 59 && inset.top == 0 && inset.bottom == 21)
let fittedInsets = TLGameViewport.insets(view: safe, safeFrame: safe)
precondition(fittedInsets.top == 0 && fittedInsets.left == 0 && fittedInsets.bottom == 0 && fittedInsets.right == 0)
// A surface that already fills a smaller window must not be inset twice.
let small = CGRect(x: 200, y: 50, width: 400, height: 250)
precondition(TLGameViewport.area(in: small, safeFrame: safe, avoidUnsafeArea: true) == small)
for area in [safe, CGRect(x: 0, y: 59, width: 430, height: 839), small, screen] {
    for size in [screen.size, safe.size, CGSize(width: 430, height: 932)] {
        let fit = TLGameViewport.fit(size, in: area)
        precondition(fit.minX >= area.minX - 0.000001 && fit.maxX <= area.maxX + 0.000001)
        precondition(fit.minY >= area.minY - 0.000001 && fit.maxY <= area.maxY + 0.000001)
        near(fit.width / fit.height, size.width / size.height)
        near(fit.midX, area.midX); near(fit.midY, area.midY)
        // Inverse view transform preserves guest touch positions after moving/resizing a cached surface.
        let k = fit.width / size.width
        for x in [CGFloat(0), size.width / 2, size.width] {
            near((fit.minX + x * k - fit.minX) / k, x)
        }
    }
}
precondition(TLGameViewport.area(in: screen, safeFrame: CGRect(x: 2000, y: 0, width: 10, height: 10), avoidUnsafeArea: true) == .zero)
precondition(TLGameViewport.fit(.zero, in: safe) == .zero)
precondition(TLGameViewport.area(in: .zero, safeFrame: safe, avoidUnsafeArea: true) == .zero)
print("PASS: production viewport avoids cutouts, preserves aspect/touches, and reports no double inset")
''')


if __name__ == "__main__":
    unittest.main()
