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
precondition(old.questOnlyCutout)
let low = try JSONDecoder().decode(TLAppSettings.self, from: Data(#"{"resolution":"low"}"#.utf8))
precondition(low.resolution == .low && low.avoidUnsafeArea)
var settings = old
settings.startupDiagnostics = true
settings.orientation = .portrait
settings.avoidUnsafeArea = false
settings.questOnlyCutout = false
let encoded = try JSONEncoder().encode(settings)
let decoded = try JSONDecoder().decode(TLAppSettings.self, from: encoded)
precondition(decoded == settings)
precondition(decoded.startupDiagnostics && decoded.orientation == .portrait)
precondition(!decoded.avoidUnsafeArea)
precondition(!decoded.questOnlyCutout)
print("PASS: per-app diagnostic profile survives Codable roundtrip and legacy settings")
''')

    def test_production_host_layout_converges_without_redundant_writes(self):
        source = (ROOT / "src/app/Husk/TLGameViewport.swift").read_text(encoding="utf-8")
        host = (ROOT / "src/app/Husk/AppWindows.swift").read_text(encoding="utf-8")
        host = "final class TLWindowHost:" + host.split("final class TLWindowHost:", 1)[1]
        # Execute the actual host class, with counting geometry properties standing in
        # for UIKit. This detects the identity/reset feedback pattern, not UIKit itself.
        host = host.replace("CGAffineTransform(", "TestTransform(")
        source += '''
struct TestTransform: Equatable {
    var scaleX: CGFloat; var y: CGFloat
    static let identity = TestTransform(scaleX: 1, y: 1)
}
enum TestColor { case black }
class UIView {
    var bounds = CGRect.zero { didSet { geometryWrites += 1 } }
    var center = CGPoint.zero { didSet { geometryWrites += 1 } }
    var transform = TestTransform.identity { didSet { geometryWrites += 1 } }
    var geometryWrites = 0
    var layoutRequests = 0
    var backgroundColor: TestColor?; var clipsToBounds = false
    weak var superview: UIView?
    init(frame: CGRect) { bounds = CGRect(origin: .zero, size: frame.size); center = CGPoint(x: frame.midX, y: frame.midY) }
    required init?(coder: NSCoder) { fatalError("unused") }
    func addSubview(_ child: UIView) { child.superview = self }
    func safeAreaInsetsDidChange() {}
    func layoutSubviews() {}
    func setNeedsLayout() { layoutRequests += 1 }
}
class TLUnityUIView: UIView {
    enum Engine { case java, flutter }
    var engine = Engine.java
    var launchedSize: CGSize? = CGSize(width: 932, height: 430)
    var contentScaleFactor: CGFloat = 3
    var windowed = false
}
enum HuskLog { static func log(_ scope: String, _ text: String) {} }
''' + host
        self.compile_run(source, '''
let game = TLUnityUIView(frame: .zero)
let host = TLWindowHost(game: game, windowed: false)
host.bounds = CGRect(x: 0, y: 0, width: 932, height: 410)
host.layoutSubviews()
let fitted = TLGameViewport.placement(game.launchedSize!, in: host.bounds)!
precondition(game.bounds == fitted.bounds && game.center == fitted.center)
precondition(game.transform == TestTransform(scaleX: fitted.scale, y: fitted.scale))
let writes = game.geometryWrites, requests = game.layoutRequests
for _ in 0..<10000 { host.layoutSubviews() }
precondition(game.geometryWrites == writes && game.layoutRequests == requests)
// Camera metadata refreshes even if it needs no geometry change.
host.safeAreaInsetsDidChange()
precondition(game.layoutRequests == requests + 1)
host.layoutSubviews(); precondition(game.geometryWrites == writes)
// A real resize converges, and never resizes the cached guest bounds.
host.bounds = CGRect(x: 0, y: 0, width: 932, height: 430)
host.layoutSubviews()
precondition(game.transform == .identity && game.bounds.size == game.launchedSize!)
let resized = game.geometryWrites
host.layoutSubviews();precondition(game.geometryWrites == resized)
// Flutter receives the new host size; a detached cached game is left alone.
game.engine = .flutter
host.bounds = CGRect(x: 0, y: 0, width: 600, height: 400)
host.layoutSubviews();precondition(game.bounds.size == host.bounds.size && game.transform == .identity)
game.superview = nil
let detached = game.geometryWrites
host.layoutSubviews();precondition(game.geometryWrites == detached)
print("PASS: production host converges without geometry/layout writes over 10000 passes, handles metadata, resize, Flutter and detach; UIKit integration NOT TESTED")
''')

    def test_production_viewport_cutouts_and_cached_surface(self):
        source = (ROOT / "src/app/Husk/TLGameViewport.swift").read_text(encoding="utf-8")
        self.compile_run(source, '''
func near(_ a: CGFloat, _ b: CGFloat) { precondition(abs(a - b) < 0.000001) }
// Representative landscape cutout/home-indicator geometry; runtime reads UIWindow's actual values.
let screen = CGRect(x: 0, y: 0, width: 932, height: 430)
let safe = CGRect(x: 59, y: 0, width: 814, height: 409)
// The full-screen surface keeps its complete launch dimensions. Insets affect HUD metadata only.
precondition(TLGameViewport.fit(screen.size, in: screen) == screen)
let inset = TLGameViewport.insets(view: screen, safeFrame: safe)
precondition(inset.left == 59 && inset.right == 59 && inset.top == 0 && inset.bottom == 21)
let fittedInsets = TLGameViewport.insets(view: safe, safeFrame: safe)
precondition(fittedInsets.top == 0 && fittedInsets.left == 0 && fittedInsets.bottom == 0 && fittedInsets.right == 0)
// A surface that already fills a smaller window must not be inset twice.
let small = CGRect(x: 200, y: 50, width: 400, height: 250)
let left = TLGameViewport.cutoutInsets(view: screen, safeFrame: safe, edge: .left)
precondition(left.left == 59 && left.right == 0 && left.top == 0 && left.bottom == 0)
let right = TLGameViewport.cutoutInsets(view: screen, safeFrame: safe, edge: .right)
precondition(right.right == 59 && right.left == 0 && right.top == 0 && right.bottom == 0)
let phone = CGRect(x: 0, y: 0, width: 430, height: 932)
let portraitSafe = CGRect(x: 0, y: 59, width: 430, height: 839)
let top = TLGameViewport.cutoutInsets(view: phone, safeFrame: portraitSafe, edge: .top)
precondition(top.top == 59 && top.left == 0 && top.right == 0 && top.bottom == 0)
let upside = TLGameViewport.cutoutInsets(view: phone, safeFrame: CGRect(x: 0, y: 34, width: 430, height: 839), edge: .bottom)
precondition(upside.bottom == 59 && upside.top == 0)
let none = TLGameViewport.cutoutInsets(view: screen, safeFrame: safe, edge: .none)
precondition(none.top == 0 && none.left == 0 && none.right == 0 && none.bottom == 0)
let window = TLGameViewport.cutoutInsets(view: small, safeFrame: safe, edge: .left)
precondition(window.left == 0 && window.right == 0 && window.top == 0 && window.bottom == 0)
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
precondition(TLGameViewport.fit(.zero, in: safe) == .zero)
// Layout must converge without an intermediate identity scale, including the exact
// 932x430 -> 932x410 cached-surface change recorded in the watchdog run.
let reduced = CGRect(x: 0, y: 0, width: 932, height: 410)
let placed = TLGameViewport.placement(screen.size, in: reduced)!
precondition(placed.bounds == screen)
near(placed.scale, 410.0/430.0)
near(placed.center.x, 466); near(placed.center.y, 205)
for _ in 0..<10000 { precondition(TLGameViewport.placement(screen.size, in: reduced) == placed) }
precondition(TLGameViewport.placement(.zero, in: reduced) == nil)
precondition(TLGameViewport.placement(screen.size, in: .zero) == nil)
precondition(TLGameViewport.placement(screen.size, in: screen)?.scale == 1)
let zero = TLGameViewport.cutoutInsets(view: .zero, safeFrame: safe, edge: .top)
precondition(zero.top == 0 && zero.left == 0 && zero.bottom == 0 && zero.right == 0)
print("PASS: full-screen geometry stays unchanged; only the physical camera edge gets HUD insets; cached aspect/touches stay consistent")
''')


if __name__ == "__main__":
    unittest.main()
