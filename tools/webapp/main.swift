// Mac harness for WebApp.swift: runs a Capacitor or Cordova APK's web side in a WKWebView, the way the app does.
// Build: tools/webapp/build.sh; run: webapp-test <apk> <engine: Capacitor|Cordova> <package> [seconds] [js-to-run-after]
import AppKit
import WebKit

struct TLReport { let engine: String? }
struct TLApp { let id: String; let label: String; let apks: [String]; let packageName: String?; let report: TLReport? }
enum TranslationLayer { static var root = URL(fileURLWithPath: NSTemporaryDirectory()).appendingPathComponent("husk-webapp") }
enum HuskLog { static func log(_ tag: String, _ s: String) { print("[\(tag)] \(s)") } }

func husk_tl_extract(_ apk: String, _ prefix: String, _ out: String) -> Int32 {
    let p = Process()
    p.executableURL = URL(fileURLWithPath: "/usr/bin/unzip")
    let tmp = out + ".unzip"
    try? FileManager.default.removeItem(atPath: tmp)
    p.arguments = ["-q", "-o", apk, prefix + "*", "-d", tmp]
    try? p.run(); p.waitUntilExit()
    let src = URL(fileURLWithPath: tmp).appendingPathComponent(prefix)
    try? FileManager.default.removeItem(atPath: out)
    try? FileManager.default.moveItem(at: src, to: URL(fileURLWithPath: out))
    let n = (FileManager.default.enumerator(atPath: out)?.allObjects.count) ?? 0
    return Int32(n)
}

let args = CommandLine.arguments
let app = TLApp(id: args[3], label: args[3], apks: [args[1]], packageName: args[3], report: TLReport(engine: args[2]))
let secs = args.count > 4 ? Double(args[4])! : 8
let js = args.count > 5 ? args[5] : ""

NSApplication.shared.setActivationPolicy(.regular)
let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 390, height: 844), styleMask: [.titled], backing: .buffered, defer: false)
MainActor.assumeIsolated {
    guard let rt = WebAppRuntime.runtime(for: app) else { print("no runtime"); exit(1) }
    rt.onClose = { print("app closed itself") }
    rt.webView.frame = window.contentView!.bounds
    window.contentView!.addSubview(rt.webView)
    window.orderFrontRegardless()
    DispatchQueue.main.asyncAfter(deadline: .now() + secs) {
        let probe = """
        JSON.stringify({ title: document.title, url: location.href, platform: window.Capacitor && Capacitor.getPlatform(),
          plugins: window.Capacitor && Object.keys(Capacitor.Plugins||{}), cordova: !!window.cordova,
          body: (document.body && document.body.innerText || '').slice(0, 300) })
        """
        rt.webView.evaluateJavaScript(js.isEmpty ? probe : js) { r, e in
            print("probe:", r ?? "nil", e.map { "\($0)" } ?? "")
            let after = ProcessInfo.processInfo.environment["AFTER"] ?? probe
            if ProcessInfo.processInfo.environment["BACK"] != nil { print("harness: back"); rt.back() }
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { rt.webView.evaluateJavaScript(after) { r2, _ in
                if !js.isEmpty { print("after:", r2 ?? "nil") }
                DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) {
                    rt.webView.takeSnapshot(with: nil) { img, _ in
                        if let img, let tiff = img.tiffRepresentation, let rep = NSBitmapImageRep(data: tiff),
                           let png = rep.representation(using: .png, properties: [:]) {
                            let out = (ProcessInfo.processInfo.environment["SHOT"] ?? "/tmp/webapp.png")
                            try? png.write(to: URL(fileURLWithPath: out)); print("shot:", out)
                        }
                        exit(0)
                    }
                }
            } }
        }
    }
}
NSApplication.shared.run()
