// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation
import WebKit
import UniformTypeIdentifiers
#if canImport(UIKit)
import UIKit
#endif

/// Web apps: Capacitor, Cordova and Ionic (which is one of the two underneath). The app is HTML, CSS and JavaScript in a WebView,
/// plus a bridge to native plugins; Android's WebView is Chromium, here it is WebKit, and Husk answers the bridge itself.
///
/// - The APK's web folder (assets/public for Capacitor, assets/www for Cordova) is copied out once and served from a scheme of its
///   own, one origin per app (capacitor://<package>/), so each app keeps its own storage, cookies and service workers.
/// - Capacitor: the same native-bridge.js the app ships for Android is injected, with `window.androidBridge` posting to Husk, so
///   the app believes it is on Android. Husk declares the plugins it answers (PluginHeaders); every other plugin falls back to its
///   web implementation, as it would in a browser.
/// - Cordova: cordova-android's prompt() bridge (gap:, gap_init:), answered in WKUIDelegate's prompt panel, and results returned
///   through cordova.callbackFromNative.
enum WebAppKind: String {
    case capacitor, cordova

    var assetFolder: String { self == .capacitor ? "public" : "www" }
    var scheme: String { self == .capacitor ? "capacitor" : "app" }
    var name: String { self == .capacitor ? "Capacitor" : "Cordova" }
}

extension TLReport {
    var webKind: WebAppKind? {
        switch engine {
        case "Capacitor": return .capacitor
        case "Cordova": return .cordova
        default: return nil
        }
    }
}

/// One running web app. Kept for the life of the process (or until it is closed), so minimising a window or leaving the screen does
/// not reload it.
@MainActor
final class WebAppRuntime: NSObject {
    let kind: WebAppKind
    let package: String
    let label: String
    let version: String
    /// The copy of the APK's assets folder.
    let assets: URL
    var root: URL { assets.appendingPathComponent(kind.assetFolder, isDirectory: true) }
    let webView: WKWebView
    /// The app asked to close itself (App.exitApp, navigator.app.exitApp, back on the first page).
    var onClose: (() -> Void)?
    var origin: String { "\(kind.scheme)://\(package.lowercased())" }

    private var capacitorListeners: [String: [String]] = [:]       // "Plugin.event" -> callback ids
    private var cordovaChannel: String?                              // CoreAndroid's messageChannel callback
    private var cordovaOverridesBack = false
    private var bridgeSecret = Int.random(in: 1000...999_999)
    private var statusBarColor: String?

    private static var running: [String: WebAppRuntime] = [:]

    /// The app's runtime, made (and its files copied out) the first time.
    static func runtime(for app: TLApp) -> WebAppRuntime? {
        if let r = running[app.id] { return r }
        guard let kind = app.report?.webKind, let apk = app.apks.first else { return nil }
        let dir = TranslationLayer.root.appendingPathComponent(app.id, isDirectory: true)
            .appendingPathComponent("web", isDirectory: true)
        guard prepare(apk: apk, into: dir) else { return nil }
        let r = WebAppRuntime(kind: kind, package: app.packageName ?? app.id, label: app.label,
                              version: "1.0", assets: dir.appendingPathComponent("assets", isDirectory: true),
                              dataDir: TranslationLayer.root.appendingPathComponent(app.id, isDirectory: true))
        running[app.id] = r
        return r
    }

    static func close(_ app: TLApp) {
        running[app.id]?.webView.stopLoading()
        running[app.id] = nil
    }

    /// Copy the APK's assets out, unless this APK's are already there.
    private static func prepare(apk: String, into dir: URL) -> Bool {
        let attrs = try? FileManager.default.attributesOfItem(atPath: apk)
        let stamp = "\((attrs?[.size] as? NSNumber)?.int64Value ?? 0)-\(((attrs?[.modificationDate] as? Date)?.timeIntervalSince1970 ?? 0))"
        let stampFile = dir.appendingPathComponent(".stamp")
        if (try? String(contentsOf: stampFile, encoding: .utf8)) == stamp { return true }
        try? FileManager.default.removeItem(at: dir)
        try? FileManager.default.createDirectory(at: dir.appendingPathComponent("assets"), withIntermediateDirectories: true)
        let n = husk_tl_extract(apk, "assets/", dir.appendingPathComponent("assets").path)
        HuskLog.log("web", "copied \(n) files out of \((apk as NSString).lastPathComponent)")
        guard n > 0 else { return false }
        try? stamp.write(to: stampFile, atomically: true, encoding: .utf8)
        return true
    }

    init(kind: WebAppKind, package: String, label: String, version: String, assets: URL, dataDir: URL) {
        self.kind = kind
        self.package = package
        self.label = label
        self.version = version
        self.assets = assets
        let config = WKWebViewConfiguration()
        let controller = WKUserContentController()
        config.userContentController = controller
        config.preferences.javaScriptCanOpenWindowsAutomatically = true
        #if canImport(UIKit)
        config.allowsInlineMediaPlayback = true
        config.mediaTypesRequiringUserActionForPlayback = []
        #endif
        let handler = SchemeHandler()
        config.setURLSchemeHandler(handler, forURLScheme: kind.scheme)
        webView = WKWebView(frame: .zero, configuration: config)
        super.init()
        handler.runtime = self
        controller.add(WeakMessageHandler(self), name: "huskBridge")
        controller.addUserScript(WKUserScript(source: bootScript(), injectionTime: .atDocumentStart, forMainFrameOnly: true))
        webView.uiDelegate = self
        webView.navigationDelegate = self
        if #available(iOS 16.4, macOS 13.3, *) { webView.isInspectable = true }
        #if canImport(UIKit)
        webView.scrollView.contentInsetAdjustmentBehavior = .never
        webView.scrollView.bounces = false
        webView.isOpaque = false
        #endif
        load()
    }

    private var config: [String: Any] {
        guard let data = try? Data(contentsOf: assets.appendingPathComponent("capacitor.config.json")),
              let obj = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return [:] }
        return obj
    }

    /// The first page: the app's own server when its config names one (a live-reload or remote app), its index.html otherwise.
    private func load() {
        if kind == .capacitor, let server = config["server"] as? [String: Any], let s = server["url"] as? String, let url = URL(string: s) {
            webView.load(URLRequest(url: url))
            return
        }
        webView.load(URLRequest(url: URL(string: "\(origin)/index.html")!))
    }

    // MARK: the bridge script

    private func bootScript() -> String {
        switch kind {
        case .capacitor:
            let bridge = (try? String(contentsOf: assets.appendingPathComponent("native-bridge.js"), encoding: .utf8)) ?? ""
            let headers = Self.capacitorHeaders()
            return """
            window.androidBridge = { postMessage: function (s) { window.webkit.messageHandlers.huskBridge.postMessage(s); } };
            window.Capacitor = { DEBUG: false, isLoggingEnabled: false, Plugins: {}, PluginHeaders: \(headers) };
            window.WEBVIEW_SERVER_URL = '\(origin)';
            \(bridge)
            window.Capacitor.PluginHeaders = \(headers);
            """
        case .cordova:
            // cordova-android talks through prompt(); nothing to add but a way to say the page is Cordova's.
            return "window.__huskCordova = true;"
        }
    }

    private static let capacitorPlugins: [String: [String]] = [
        "App": ["getInfo", "getState", "getLaunchUrl", "exitApp", "minimizeApp", "toggleBackButtonHandler", "removeListener", "removeAllListeners"],
        "Device": ["getId", "getInfo", "getBatteryInfo", "getLanguageCode", "getLanguageTag"],
        "StatusBar": ["setStyle", "setBackgroundColor", "show", "hide", "getInfo", "setOverlaysWebView"],
        "SplashScreen": ["show", "hide"],
        "Keyboard": ["show", "hide", "setAccessoryBarVisible", "setScroll", "setStyle", "setResizeMode", "getResizeMode", "removeAllListeners"],
        "Haptics": ["impact", "notification", "vibrate", "selectionStart", "selectionChanged", "selectionEnd"],
    ]

    private static func capacitorHeaders() -> String {
        let list: [[String: Any]] = capacitorPlugins.keys.sorted().map { name in
            var methods = capacitorPlugins[name]!.map { ["name": $0, "rtype": "promise"] }
            if name == "App" || name == "Keyboard" { methods.append(["name": "addListener", "rtype": "callback"]) }
            return ["name": name, "methods": methods]
        }
        let data = (try? JSONSerialization.data(withJSONObject: list)) ?? Data("[]".utf8)
        return String(decoding: data, as: UTF8.self)
    }

    // MARK: Capacitor calls

    fileprivate func capacitorCall(_ json: String) {
        guard let data = json.data(using: .utf8), let call = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let plugin = call["pluginId"] as? String, let method = call["methodName"] as? String else { return }
        let id = call["callbackId"] as? String ?? "-1"
        let options = call["options"] as? [String: Any] ?? [:]
        func ok(_ data: [String: Any] = [:]) { capacitorResult(id: id, plugin: plugin, method: method, data: data) }
        func fail(_ message: String) { capacitorResult(id: id, plugin: plugin, method: method, error: message) }

        switch (plugin, method) {
        case (_, "addListener"):
            if let event = options["eventName"] as? String { capacitorListeners["\(plugin).\(event)", default: []].append(id) }
        case (_, "removeListener"):
            if let cid = options["callbackId"] as? String {
                for k in capacitorListeners.keys { capacitorListeners[k]?.removeAll { $0 == cid } }
            }
            ok()
        case (_, "removeAllListeners"):
            capacitorListeners = capacitorListeners.filter { !$0.key.hasPrefix(plugin + ".") }
            ok()
        case ("App", "getInfo"):
            ok(["name": label, "id": package, "build": version, "version": version])
        case ("App", "getState"):
            ok(["isActive": true])
        case ("App", "getLaunchUrl"):
            ok([:])
        case ("App", "exitApp"), ("App", "minimizeApp"):
            ok(); onClose?()
        case ("App", "toggleBackButtonHandler"):
            ok()
        case ("Device", "getId"):
            ok(["identifier": Self.deviceID, "uuid": Self.deviceID])
        case ("Device", "getInfo"):
            ok(Self.deviceInfo)
        case ("Device", "getBatteryInfo"):
            ok(["batteryLevel": Self.batteryLevel, "isCharging": false])
        case ("Device", "getLanguageCode"):
            ok(["value": Locale.current.language.languageCode?.identifier ?? "en"])
        case ("Device", "getLanguageTag"):
            ok(["value": Locale.preferredLanguages.first ?? "en-US"])
        case ("StatusBar", "setBackgroundColor"):
            statusBarColor = options["color"] as? String
            NotificationCenter.default.post(name: Self.chromeChanged, object: self)
            ok()
        case ("StatusBar", "getInfo"):
            ok(["visible": true, "style": "DEFAULT", "color": statusBarColor ?? "#000000", "overlays": false])
        case ("Haptics", _):
            haptic(method, options)
            ok()
        case ("StatusBar", _), ("SplashScreen", _), ("Keyboard", _):
            ok()
        default:
            fail("\(plugin).\(method) is not implemented in Husk")
        }
    }

    private func capacitorResult(id: String, plugin: String, method: String, data: [String: Any] = [:], error: String? = nil, keep: Bool = false) {
        var result: [String: Any] = ["callbackId": id, "pluginId": plugin, "methodName": method, "success": error == nil]
        if let error { result["error"] = ["message": error] } else { result["data"] = data }
        if keep { result["save"] = true }
        guard let json = try? JSONSerialization.data(withJSONObject: result) else { return }
        webView.evaluateJavaScript("window.Capacitor && window.Capacitor.fromNative(\(String(decoding: json, as: UTF8.self)))")
    }

    /// Tell every listener for an event (App.backButton, App.appStateChange). Returns whether there was one.
    @discardableResult
    private func capacitorEvent(_ plugin: String, _ event: String, _ data: [String: Any]) -> Bool {
        let ids = capacitorListeners["\(plugin).\(event)"] ?? []
        for id in ids { capacitorResult(id: id, plugin: plugin, method: "addListener", data: data, keep: true) }
        return !ids.isEmpty
    }

    // MARK: Cordova calls

    /// One prompt() from cordova-android's bridge. The return value is what prompt() gives back to the page.
    fileprivate func cordovaPrompt(_ message: String, _ tag: String) -> String? {
        if tag.hasPrefix("gap_init:") { return String(bridgeSecret) }
        if tag.hasPrefix("gap_bridge_mode:") || tag.hasPrefix("gap_poll:") { return "" }
        guard tag.hasPrefix("gap:"), let data = tag.dropFirst(4).data(using: .utf8),
              let head = try? JSONSerialization.jsonObject(with: data) as? [Any], head.count >= 4,
              let service = head[1] as? String, let action = head[2] as? String, let callback = head[3] as? String else { return nil }
        let args = (try? JSONSerialization.jsonObject(with: Data(message.utf8))) as? [Any] ?? []
        cordovaExec(service, action, callback, args)
        return ""
    }

    private func cordovaExec(_ service: String, _ action: String, _ id: String, _ args: [Any]) {
        func ok(_ value: Any? = nil, keep: Bool = false) { cordovaResult(id, success: true, value, keep: keep) }
        func fail(_ message: String) { cordovaResult(id, success: false, message) }
        switch (service, action) {
        case ("CoreAndroid", "messageChannel"):
            cordovaChannel = id
        case ("CoreAndroid", "overrideBackbutton"):
            cordovaOverridesBack = args.first as? Bool ?? false
            ok()
        case ("CoreAndroid", "exitApp"):
            ok(); onClose?()
        case ("CoreAndroid", "backHistory"):
            webView.goBack(); ok()
        case ("CoreAndroid", _):
            ok()
        case ("Device", "getDeviceInfo"):
            ok(["uuid": Self.deviceID, "version": "14", "platform": "Android", "model": Self.model, "manufacturer": "Apple",
                "isVirtual": false, "serial": "unknown", "sdkVersion": "34"])
        case ("NetworkStatus", "getConnectionInfo"):
            ok("wifi", keep: true)
        case ("StatusBar", "_ready"):
            ok(true)
        case ("StatusBar", "backgroundColorByHexString"):
            statusBarColor = args.first as? String
            NotificationCenter.default.post(name: Self.chromeChanged, object: self)
            ok()
        case ("StatusBar", _), ("SplashScreen", _), ("Keyboard", _):
            ok()
        case ("WebIntent", "onNewIntent"):
            break                                   // no intents arrive here
        case ("WebIntent", _):
            ok("")
        case ("Vibration", _):
            haptic("vibrate", [:]); ok()
        case ("InAppBrowser", "open"):
            if let s = args.first as? String, let url = URL(string: s, relativeTo: webView.url) { openExternal(url) }
            ok(["type": "loadstop"], keep: true)
        case ("Notification", "alert"):
            dialog(title: args[safe: 1] as? String, message: args[safe: 0] as? String ?? "",
                   buttons: [(args[safe: 2] as? String) ?? "OK"]) { [weak self] _, _ in self?.cordovaResult(id, success: true, nil) }
        case ("Notification", "confirm"):
            let labels = (args[safe: 2] as? [String]) ?? ["OK", "Cancel"]
            dialog(title: args[safe: 1] as? String, message: args[safe: 0] as? String ?? "", buttons: labels) { [weak self] i, _ in
                self?.cordovaResult(id, success: true, i + 1)
            }
        case ("Notification", "prompt"):
            let labels = (args[safe: 2] as? [String]) ?? ["OK", "Cancel"]
            dialog(title: args[safe: 1] as? String, message: args[safe: 0] as? String ?? "", buttons: labels,
                   field: args[safe: 3] as? String ?? "") { [weak self] i, text in
                self?.cordovaResult(id, success: true, ["buttonIndex": i + 1, "input1": text ?? ""])
            }
        case ("Notification", "beep"):
            haptic("vibrate", [:]); ok()
        default:
            HuskLog.log("web", "cordova: no plugin for \(service).\(action)")
            cordovaResult(id, success: false, "Class not found", status: 2)
        }
    }

    private func cordovaResult(_ id: String, success: Bool, _ value: Any?, keep: Bool = false, status: Int? = nil) {
        let status = status ?? (success ? 1 : 9)
        let arg: String
        if let value, JSONSerialization.isValidJSONObject([value]), let data = try? JSONSerialization.data(withJSONObject: [value]) {
            arg = String(decoding: data, as: UTF8.self)
        } else {
            arg = "[]"
        }
        webView.evaluateJavaScript("window.cordova && cordova.callbackFromNative('\(id)', \(success), \(status), \(arg), \(keep))")
    }

    private func cordovaEvent(_ action: String) -> Bool {
        guard let channel = cordovaChannel else { return false }
        cordovaResult(channel, success: true, ["action": action], keep: true)
        return true
    }

    // MARK: from Husk

    /// Android's back button: the app's own handler when it has one, the page history otherwise, and closing on the first page.
    func back() {
        switch kind {
        case .capacitor:
            if capacitorEvent("App", "backButton", ["canGoBack": webView.canGoBack]) { return }
        case .cordova:
            if cordovaOverridesBack, cordovaEvent("backbutton") { return }
        }
        if webView.canGoBack { webView.goBack() } else { onClose?() }
    }

    /// The app leaving or coming back to the screen.
    func setActive(_ active: Bool) {
        switch kind {
        case .capacitor:
            capacitorEvent("App", "appStateChange", ["isActive": active])
            capacitorEvent("App", active ? "resume" : "pause", [:])
        case .cordova:
            _ = cordovaEvent(active ? "resume" : "pause")
        }
    }

    /// The colour the app asked for behind the status bar, if any.
    var chromeColor: String? { statusBarColor ?? ((config["plugins"] as? [String: Any])?["StatusBar"] as? [String: Any])?["backgroundColor"] as? String }
    static let chromeChanged = Notification.Name("husk.web.chromeChanged")

    // MARK: device facts

    static var deviceID: String {
        let key = "husk.web.deviceID"
        if let id = UserDefaults.standard.string(forKey: key) { return id }
        let id = UUID().uuidString.replacingOccurrences(of: "-", with: "").lowercased().prefix(16)
        UserDefaults.standard.set(String(id), forKey: key)
        return String(id)
    }

    static var model: String {
        #if canImport(UIKit)
        return UIDevice.current.model
        #else
        return "Mac"
        #endif
    }

    static var batteryLevel: Double {
        #if canImport(UIKit)
        UIDevice.current.isBatteryMonitoringEnabled = true
        let l = UIDevice.current.batteryLevel
        return l < 0 ? 1 : Double(l)
        #else
        return 1
        #endif
    }

    static var deviceInfo: [String: Any] {
        ["name": model, "model": model, "platform": "android", "operatingSystem": "android", "osVersion": "14",
         "androidSDKVersion": 34, "manufacturer": "Apple", "isVirtual": false, "webViewVersion": "120.0.0.0",
         "memUsed": 0, "realDiskFree": 0, "realDiskTotal": 0]
    }

    // MARK: native bits

    private func haptic(_ method: String, _ options: [String: Any]) {
        #if canImport(UIKit)
        switch method {
        case "impact":
            let style: UIImpactFeedbackGenerator.FeedbackStyle = (options["style"] as? String) == "HEAVY" ? .heavy
                : (options["style"] as? String) == "MEDIUM" ? .medium : .light
            UIImpactFeedbackGenerator(style: style).impactOccurred()
        case "notification":
            let type: UINotificationFeedbackGenerator.FeedbackType = (options["type"] as? String) == "ERROR" ? .error
                : (options["type"] as? String) == "WARNING" ? .warning : .success
            UINotificationFeedbackGenerator().notificationOccurred(type)
        case "selectionChanged":
            UISelectionFeedbackGenerator().selectionChanged()
        default:
            UIImpactFeedbackGenerator(style: .medium).impactOccurred()
        }
        #endif
    }

    private func openExternal(_ url: URL) {
        #if canImport(UIKit)
        UIApplication.shared.open(url)
        #else
        NSWorkspace.shared.open(url)
        #endif
    }

    /// A dialog the page asked for: the buttons in order, and the text typed when there is a field.
    private func dialog(title: String?, message: String, buttons: [String], field: String? = nil,
                        done: @escaping (Int, String?) -> Void) {
        #if canImport(UIKit)
        let alert = UIAlertController(title: title, message: message, preferredStyle: .alert)
        if let field { alert.addTextField { $0.text = field } }
        for (i, b) in buttons.enumerated() {
            alert.addAction(UIAlertAction(title: b, style: .default) { _ in done(i, alert.textFields?.first?.text) })
        }
        guard let top = Self.topController(from: webView) else { done(0, field); return }
        top.present(alert, animated: true)
        #else
        done(0, field)
        #endif
    }

    #if canImport(UIKit)
    static func topController(from view: UIView) -> UIViewController? {
        var vc = view.window?.rootViewController
        while let presented = vc?.presentedViewController { vc = presented }
        return vc
    }
    #endif
}

// MARK: - WebKit plumbing

extension WebAppRuntime: WKScriptMessageHandler {
    func userContentController(_ controller: WKUserContentController, didReceive message: WKScriptMessage) {
        MainActor.assumeIsolated {
            if let s = message.body as? String { capacitorCall(s) }
            else if let obj = message.body as? [String: Any], let data = try? JSONSerialization.data(withJSONObject: obj) {
                capacitorCall(String(decoding: data, as: UTF8.self))
            }
        }
    }
}

extension WebAppRuntime: WKUIDelegate, WKNavigationDelegate {
    func webView(_ webView: WKWebView, runJavaScriptTextInputPanelWithPrompt prompt: String, defaultText: String?,
                             initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (String?) -> Void) {
        MainActor.assumeIsolated {
            if kind == .cordova, let tag = defaultText, let answer = cordovaPrompt(prompt, tag) {
                completionHandler(answer)
                return
            }
            dialog(title: nil, message: prompt, buttons: ["OK", "Cancel"], field: defaultText ?? "") { i, text in
                completionHandler(i == 0 ? text : nil)
            }
        }
    }

    func webView(_ webView: WKWebView, runJavaScriptAlertPanelWithMessage message: String,
                             initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping () -> Void) {
        MainActor.assumeIsolated { dialog(title: nil, message: message, buttons: ["OK"]) { _, _ in completionHandler() } }
    }

    func webView(_ webView: WKWebView, runJavaScriptConfirmPanelWithMessage message: String,
                             initiatedByFrame frame: WKFrameInfo, completionHandler: @escaping (Bool) -> Void) {
        MainActor.assumeIsolated { dialog(title: nil, message: message, buttons: ["OK", "Cancel"]) { i, _ in completionHandler(i == 0) } }
    }

    /// target=_blank and window.open: the link opens outside, as Android's WebView hands it to the browser.
    func webView(_ webView: WKWebView, createWebViewWith configuration: WKWebViewConfiguration,
                             for navigationAction: WKNavigationAction, windowFeatures: WKWindowFeatures) -> WKWebView? {
        MainActor.assumeIsolated { if let url = navigationAction.request.url { openExternal(url) } }
        return nil
    }

    /// The app's own pages stay in; anything else opens outside (the system browser, the phone, mail).
    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction,
                             decisionHandler: @escaping @MainActor @Sendable (WKNavigationActionPolicy) -> Void) {
        MainActor.assumeIsolated {
            guard let url = navigationAction.request.url, let scheme = url.scheme?.lowercased() else { decisionHandler(.allow); return }
            let ours = scheme == kind.scheme || scheme == "about" || scheme == "blob" || scheme == "data"
            let web = scheme == "http" || scheme == "https"
            if ours || (web && navigationAction.targetFrame?.isMainFrame == false) || (web && navigationAction.navigationType == .other) {
                decisionHandler(.allow)
            } else if web, navigationAction.targetFrame?.isMainFrame == true, navigationAction.navigationType == .linkActivated {
                openExternal(url); decisionHandler(.cancel)
            } else if web {
                decisionHandler(.allow)
            } else {
                openExternal(url); decisionHandler(.cancel)
            }
        }
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        MainActor.assumeIsolated { HuskLog.log("web", "loaded \(webView.url?.absoluteString ?? "?")") }
    }

    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) {
        MainActor.assumeIsolated { HuskLog.log("web", "load failed: \(error.localizedDescription)") }
    }

    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) {
        MainActor.assumeIsolated { HuskLog.log("web", "load failed: \(error.localizedDescription)") }
    }

    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        MainActor.assumeIsolated { HuskLog.log("web", "the page's process ended; reloading"); webView.reload() }
    }
}

/// The app's files, from the copy of its assets, at <scheme>://<package>/<path>. Capacitor's /_capacitor_file_/ paths are files on
/// the device.
private final class SchemeHandler: NSObject, WKURLSchemeHandler {
    weak var runtime: WebAppRuntime?

    func webView(_ webView: WKWebView, start task: WKURLSchemeTask) {
        guard let url = task.request.url else { return }
        let file: URL? = MainActor.assumeIsolated {
            guard let runtime else { return nil }
            var path = url.path.removingPercentEncoding ?? url.path
            if path.hasPrefix("/_capacitor_file_") { return URL(fileURLWithPath: String(path.dropFirst("/_capacitor_file_".count))) }
            if path.isEmpty || path == "/" { path = "/index.html" }
            let candidate = runtime.root.appendingPathComponent(String(path.dropFirst())).standardizedFileURL
            guard candidate.path.hasPrefix(runtime.root.standardizedFileURL.path) else { return nil }
            var isDir: ObjCBool = false
            if FileManager.default.fileExists(atPath: candidate.path, isDirectory: &isDir), isDir.boolValue {
                return candidate.appendingPathComponent("index.html")
            }
            return candidate
        }
        guard let file, let data = try? Data(contentsOf: file) else {
            let r = HTTPURLResponse(url: url, statusCode: 404, httpVersion: "HTTP/1.1", headerFields: ["Content-Type": "text/plain"])!
            task.didReceive(r)
            task.didReceive(Data("Not found".utf8))
            task.didFinish()
            return
        }
        let mime = UTType(filenameExtension: file.pathExtension)?.preferredMIMEType ?? "application/octet-stream"
        let headers = ["Content-Type": mime, "Content-Length": "\(data.count)", "Cache-Control": "no-cache",
                       "Access-Control-Allow-Origin": "*"]
        task.didReceive(HTTPURLResponse(url: url, statusCode: 200, httpVersion: "HTTP/1.1", headerFields: headers)!)
        task.didReceive(data)
        task.didFinish()
    }

    func webView(_ webView: WKWebView, stop task: WKURLSchemeTask) {}
}

/// WKUserContentController keeps its handlers strongly; this breaks the cycle with the runtime that owns the web view.
private final class WeakMessageHandler: NSObject, WKScriptMessageHandler {
    weak var target: WKScriptMessageHandler?
    init(_ target: WKScriptMessageHandler) { self.target = target }
    func userContentController(_ c: WKUserContentController, didReceive m: WKScriptMessage) { target?.userContentController(c, didReceive: m) }
}

private extension Array {
    subscript(safe i: Int) -> Element? { indices.contains(i) ? self[i] : nil }
}
