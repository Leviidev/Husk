"""Build inputs for a UIKit/SwiftUI Simulator check using the production status-bar policy.

Run with an output directory, compile the generated Swift together with
src/app/Husk/AppStatusBar.swift (module StatusBarProbe), then sign/install the
app with simctl. The probe writes Documents/result.json and uses only its own
UserDefaults suite. It does not launch a guest, camera or network connection.
"""
import pathlib
import plistlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
output = pathlib.Path(sys.argv[1])
app = output / "StatusBarProbe.app"
app.mkdir(parents=True, exist_ok=True)

# Use the same availability wrapper as production, without the unrelated iOS-15 shims.
compat = (ROOT / "src/app/Husk/Compat15.swift").read_text()
start = compat.index("    @ViewBuilder\n    func huskHideSystemOverlays")
end = compat.index("    @ViewBuilder", start + 20)
wrapper = "extension View {\n" + compat[start:end] + "}\n"

source = r'''import SwiftUI
import UIKit

@MainActor final class ProbeModel: ObservableObject {
    @Published var rootItem: String?
    @Published var presented = false
    @Published var gameItem = "tl:status-probe-first"
}
@MainActor enum Probe {
    static let model = ProbeModel()
    static var results: [[String: Any]] = []
    static weak var surface: UIView?
    static var window: UIWindow!
    static func check(_ name: String, hidden: Bool) async {
        try? await Task.sleep(nanoseconds: 1_200_000_000)
        let actual = window.windowScene!.statusBarManager!.isStatusBarHidden
        results.append(["name": name, "hidden": actual, "success": actual == hidden])
    }
    static func run() async {
        let first = AppStatusBar.key(for: "tl:status-probe-first")
        let second = AppStatusBar.key(for: "tl:status-probe-second")
        UserDefaults.standard.removeObject(forKey: first)
        UserDefaults.standard.removeObject(forKey: second)
        // The settings preserve clock/battery choices separately for different library item types.
        assert(first != AppStatusBar.key(for: "ps4:status-probe-first"))
        assert(AppStatusBar.androidItem(from: "mResumedActivity: ActivityRecord{123 u0 test.app/.Main t1}") == "android:test.app")
        assert(AppStatusBar.androidItem(from: "topResumedActivity=ActivityRecord{123 u0 test.second/.Main t2}\nmResumedActivity: ActivityRecord{123 u0 test.first/.Main t1}") == "android:test.second")
        assert(AppStatusBar.androidItem(from: "mResumedActivity: null") == AppStatusBar.androidSystem)
        assert(AppStatusBar.androidItem(from: "shell response timed out") == nil)
        await check("shell", hidden: false)
        model.presented = true
        await check("fullscreen-default", hidden: true)
        UserDefaults.standard.set(true, forKey: first)
        await check("fullscreen-visible", hidden: false)
        if let surface, let scene = window.windowScene {
            let rect = surface.convert(surface.bounds, to: window)
            let bar = scene.statusBarManager!.statusBarFrame
            results.append(["name": "surface-below-clock", "surface_top": rect.minY,
                            "status_bottom": bar.maxY, "success": bar.height > 0 && rect.minY >= bar.maxY - 1])
        }
        model.gameItem = "tl:status-probe-second"
        await check("other-app-independent", hidden: true)
        model.gameItem = "tl:status-probe-first"
        await check("first-app-restored", hidden: false)
        model.presented = false
        await check("dismiss-to-shell", hidden: false)
        model.rootItem = "tl:status-probe-second"
        await check("mounted-guest-visible", hidden: true)
        model.rootItem = nil
        await check("mounted-guest-covered-by-shell", hidden: false)
        let result: [String: Any] = ["checks": results, "success": results.allSatisfy { $0["success"] as? Bool == true }]
        let dest = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("result.json")
        try! JSONSerialization.data(withJSONObject: result, options: [.prettyPrinted, .sortedKeys]).write(to: dest)
        UserDefaults.standard.removeObject(forKey: first)
        UserDefaults.standard.removeObject(forKey: second)
    }
}
struct Surface: UIViewRepresentable {
    func makeUIView(context: Context) -> UIView {
        let view = UIView(); view.backgroundColor = .systemBlue; Probe.surface = view; return view
    }
    func updateUIView(_ view: UIView, context: Context) {}
}
struct Game: View {
    @Environment(\.systemStatusBarVisible) private var statusVisible
    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea(.container, edges: statusVisible ? [.horizontal, .bottom] : .all)
            Surface().ignoresSafeArea(.container, edges: statusVisible ? [.horizontal, .bottom] : .all)
        }
    }
}
struct Root: View {
    @ObservedObject var model = Probe.model
    var body: some View {
        ZStack {
            Color.blue
            Game().opacity(0).allowsHitTesting(false)
        }
        .appStatusBar(for: model.rootItem)
        .fullScreenCover(isPresented: $model.presented) {
            Modal(model: model)
        }
    }
}
struct Modal: View {
    @ObservedObject var model: ProbeModel
    var body: some View { Game().appStatusBar(for: model.gameItem) }
}
@main struct Main: App {
    var body: some Scene {
        WindowGroup {
            Root().task {
                while Probe.window == nil {
                    Probe.window = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
                        .flatMap { $0.windows }.first { $0.isKeyWindow }
                    if Probe.window == nil { try? await Task.sleep(nanoseconds: 50_000_000) }
                }
                await Probe.run()
            }
        }
    }
}
'''
(output / "StatusBarProbe.swift").write_text(source + wrapper)
info = plistlib.loads((ROOT / "src/app/Husk/Info.plist").read_bytes())
info = {k: info[k] for k in ("UIStatusBarHidden", "UIViewControllerBasedStatusBarAppearance") if k in info}
info.update(CFBundleIdentifier="com.husk.statusbar-probe", CFBundleExecutable="StatusBarProbe",
            CFBundleName="StatusBarProbe", CFBundlePackageType="APPL", CFBundleVersion="1",
            CFBundleShortVersionString="1", MinimumOSVersion="17.0", UIDeviceFamily=[1],
            UISupportedInterfaceOrientations=["UIInterfaceOrientationPortrait"],
            UIApplicationSceneManifest={"UIApplicationSupportsMultipleScenes": False, "UISceneConfigurations": {}})
(app / "Info.plist").write_bytes(plistlib.dumps(info))
