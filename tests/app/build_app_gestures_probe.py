"""Generate a Simulator probe of UIKit edge-gesture and Home-indicator preferences.

Compile the generated Swift with swiftc, then sign/install
GestureProbe.app. Documents/result.json records ordinary app/game/shell transitions.
This checks UIKit policy; an actual first-swipe Home gesture needs device testing.
"""
import pathlib
import plistlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
work = pathlib.Path(sys.argv[1])
app = work / "GestureProbe.app"
app.mkdir(parents=True, exist_ok=True)
compat = (ROOT / "src/app/Husk/Compat15.swift").read_text()
wrappers = ""
for name in ("huskHideSystemOverlays", "huskDefersSystemGestures"):
    start = compat.index("    @ViewBuilder\n    func " + name)
    end = compat.index("\n    }", start) + 6
    wrappers += compat[start:end] + "\n"
source = r'''
import SwiftUI
import UIKit
@MainActor final class Model: ObservableObject {
    @Published var presented = false
    @Published var game = false
}
@MainActor enum Probe {
    static let model = Model()
    static var window: UIWindow!
    static var checks: [[String:Any]] = []
    static func check(_ name: String, game: Bool, running: Bool) async {
        try? await Task.sleep(nanoseconds: 1_200_000_000)
        var presented = window.rootViewController!
        while let next = presented.presentedViewController { presented = next }
        var edge = presented
        while let next = edge.childForScreenEdgesDeferringSystemGestures { edge = next }
        var home = presented
        while let next = home.childForHomeIndicatorAutoHidden { home = next }
        let edges = edge.preferredScreenEdgesDeferringSystemGestures.rawValue
        let hideHome = home.prefersHomeIndicatorAutoHidden
        let hideStatus = window.windowScene!.statusBarManager!.isStatusBarHidden
        // The shell may defer side edges; its Home edge must remain available.
        let correctEdges = game ? edges == UIRectEdge.all.rawValue
            : running ? edges == 0 : edges & UIRectEdge.bottom.rawValue == 0
        checks.append(["name":name,"deferred_edges":edges,"home_hidden":hideHome,"status_hidden":hideStatus,
                       "success": correctEdges && hideHome == game])
    }
    static func run() async {
        await check("shell",game:false,running:false)
        model.presented = true
        await check("ordinary-app",game:false,running:true)
        model.game = true
        await check("game",game:true,running:true)
        model.game = false
        await check("return-to-app",game:false,running:true)
        model.presented = false
        await check("return-to-shell",game:false,running:false)
        let result: [String:Any] = ["checks":checks,"success":checks.allSatisfy { $0["success"] as? Bool == true }]
        let dest = FileManager.default.urls(for:.documentDirectory,in:.userDomainMask)[0].appendingPathComponent("result.json")
        try! JSONSerialization.data(withJSONObject:result,options:[.prettyPrinted,.sortedKeys]).write(to:dest)
    }
}
struct Running: View {
    @ObservedObject var model = Probe.model
    var body: some View {
        Color.black
            .huskDefersSystemGestures(on:model.game ? .all : [])
            .statusBarHidden(true)
            .huskHideSystemOverlays(model.game)
    }
}
struct Root: View {
    @ObservedObject var model = Probe.model
    var body: some View {
        Color.white.huskHideSystemOverlays(false).fullScreenCover(isPresented:$model.presented) { Running() }
    }
}
@main struct Main: App {
    var body: some Scene {
        WindowGroup { Root().task {
            while Probe.window == nil {
                Probe.window = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }.flatMap { $0.windows }.first { $0.isKeyWindow }
                if Probe.window == nil { try? await Task.sleep(nanoseconds:50_000_000) }
            }
            await Probe.run()
        } }
    }
}
'''
(work / "GestureProbe.swift").write_text(source + "\nextension View {\n" + wrappers + "\n}\n")
info = dict(CFBundleIdentifier="com.husk.gesture-probe", CFBundleExecutable="GestureProbe", CFBundleName="GestureProbe",
            CFBundlePackageType="APPL", CFBundleVersion="1", CFBundleShortVersionString="1", MinimumOSVersion="17.0",
            UIDeviceFamily=[1], UILaunchScreen={}, UIApplicationSceneManifest={"UIApplicationSupportsMultipleScenes": False, "UISceneConfigurations": {}})
(app / "Info.plist").write_bytes(plistlib.dumps(info))
