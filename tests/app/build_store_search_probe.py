"""Generate a Simulator probe using StoreView's actual scrolling search field.

Compile StoreSearchProbe.swift with swiftc, sign/install StoreSearchProbe.app,
then read Documents/result.json. No Google account or network is used.
"""
import pathlib
import plistlib
import sys

root = pathlib.Path(__file__).resolve().parents[2]
output = pathlib.Path(sys.argv[1])
app = output / "StoreSearchProbe.app"
app.mkdir(parents=True, exist_ok=True)
bridge = (root / "src/app/Husk/Play/StoreView.swift").read_text().split("// MARK: - Search field\n", 1)[1]
source = r'''import SwiftUI
import UIKit

@MainActor final class Model: ObservableObject {
    @Published var signedIn = false
    @Published var loading = false
    @Published var query = ""
}
enum Console { static let raised = Color(uiColor: .secondarySystemFill) }
struct Root: View {
    @ObservedObject var model = Probe.model
    var body: some View {
        NavigationStack {
            Group {
                if !model.signedIn { Text("Sign in") }
                else {
                    StoreSearchContent(text: $model.query, enabled: model.signedIn) {
                        if model.loading { ProgressView("Connecting") }
                        else {
                            LazyVStack { ForEach(0..<80) { Text("App \($0)").frame(maxWidth: .infinity).padding() } }
                        }
                    }
                }
            }
            .navigationTitle("Store")
            .navigationBarTitleDisplayMode(.large)
        }
    }
}
@MainActor enum Probe {
    static let model = Model()
    static var window: UIWindow!
    static var checks: [[String: Any]] = []
    static func pause() async { try? await Task.sleep(nanoseconds: 800_000_000) }
    static func find<T: UIViewController>(_ type: T.Type, in vc: UIViewController?) -> T? {
        guard let vc else { return nil }
        if let match = vc as? T { return match }
        for child in vc.children { if let match = find(type, in: child) { return match } }
        return nil
    }
    static var nav: UINavigationController? { find(UINavigationController.self, in: window.rootViewController) }
    static func field(in view: UIView?) -> UITextField? {
        guard let view else { return nil }
        if let field = view as? UITextField { return field }
        for child in view.subviews { if let field = field(in: child) { return field } }
        return nil
    }
    static var search: UITextField? { field(in: nav?.topViewController?.view) }
    static func scroll(in view: UIView?) -> UIScrollView? {
        guard let view else { return nil }
        if let match = view as? UIScrollView { return match }
        for child in view.subviews { if let match = scroll(in: child) { return match } }
        return nil
    }
    static func visibleHeight(_ view: UIView?) -> CGFloat {
        guard let view, view.window != nil else { return 0 }
        var rect = view.convert(view.bounds, to: window)
        var ancestor: UIView? = view
        while let current = ancestor {
            if current.isHidden || current.alpha < 0.01 { return 0 }
            if current.clipsToBounds { rect = rect.intersection(current.convert(current.bounds, to: window)) }
            ancestor = current.superview
        }
        return rect.isNull ? 0 : rect.height
    }
    static func check(_ name: String, _ success: Bool, _ extra: [String: Any] = [:]) {
        checks.append(extra.merging(["name": name, "success": success]) { _, new in new })
    }
    static func screenshot(_ name: String) {
        let renderer = UIGraphicsImageRenderer(bounds: window.bounds)
        let data = renderer.image { _ in window.drawHierarchy(in: window.bounds, afterScreenUpdates: true) }.pngData()!
        let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        try! data.write(to: dir.appendingPathComponent(name + ".png"))
    }
    static func run() async {
        await pause()
        check("signed-out-no-search", nav != nil && search == nil)
        model.loading = true; model.signedIn = true
        await pause()
        check("visible-while-loading", visibleHeight(search) > 15)
        model.loading = false
        await pause()
        let initialHeight = visibleHeight(search)
        check("initially-visible-without-focus", initialHeight > 15 && search?.isFirstResponder == false,
              ["height": initialHeight, "offset": scroll(in: nav?.topViewController?.view)?.contentOffset.y ?? 0,
               "inset": scroll(in: nav?.topViewController?.view)?.adjustedContentInset.top ?? 0,
               "bar": search?.frame.debugDescription ?? "nil"])
        screenshot("initial")
        let content = scroll(in: nav?.topViewController?.view)
        content?.setContentOffset(CGPoint(x: 0, y: 450), animated: true)
        await pause()
        let hiddenHeight = visibleHeight(search)
        check("collapses-on-scroll", content != nil && hiddenHeight < 5, ["height": hiddenHeight])
        screenshot("scrolled")
        if let content { content.setContentOffset(CGPoint(x: 0, y: -content.adjustedContentInset.top), animated: true) }
        await pause()
        check("visible-again-at-top", visibleHeight(search) > 15)
        search?.text = "Народная карта"
        search?.sendActions(for: .editingChanged)
        await pause()
        check("query-binding", model.query == "Народная карта")
        search?.becomeFirstResponder()
        await pause()
        model.loading = true
        await pause()
        check("focus-preserved-during-results-update", search?.isFirstResponder == true)
        model.loading = false
        await pause()
        model.query = ""
        await pause()
        check("binding-clears-field", search?.text?.isEmpty == true)
        model.signedIn = false
        await pause()
        check("sign-out-removes-active-search", search == nil)
        model.signedIn = true
        await pause()
        check("sign-in-reveals-search", visibleHeight(search) > 15 && search?.isFirstResponder == false)
        let result: [String: Any] = ["checks": checks, "success": checks.allSatisfy { $0["success"] as? Bool == true }]
        let dest = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("result.json")
        try! JSONSerialization.data(withJSONObject: result, options: [.prettyPrinted, .sortedKeys]).write(to: dest)
    }
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
(output / "StoreSearchProbe.swift").write_text(source + bridge)
info = dict(CFBundleIdentifier="com.husk.store-search-probe", CFBundleExecutable="StoreSearchProbe",
            CFBundleName="StoreSearchProbe", CFBundlePackageType="APPL", CFBundleVersion="1",
            CFBundleShortVersionString="1", MinimumOSVersion="17.0", UIDeviceFamily=[1],
            UISupportedInterfaceOrientations=["UIInterfaceOrientationPortrait"],
            UILaunchScreen={}, UIApplicationSceneManifest={"UIApplicationSupportsMultipleScenes": False, "UISceneConfigurations": {}})
(app / "Info.plist").write_bytes(plistlib.dumps(info))
