// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit
import QuartzCore
import AVFoundation
import UniformTypeIdentifiers

/// Engines the native runtime drives. Java-driven APKs close cooperatively
/// before a different APK starts; other engines still require a restart.
enum TLNativeEngine {
    case unity     // Subway Surfers and other Unity games: portrait, driven by UnityPlayer's own thread
    case cocos     // Geometry Dash and other cocos2d-x games: landscape, driven by a GL thread of our own
    case minecraft // Minecraft and other GameActivity games: landscape, multi-touch, the game runs its own threads
    case sdl       // Beach Buggy Racing 2 and other SDL3 games: landscape, multi-touch, the game runs its own threads
    case gta       // GTA San Andreas (Rockstar): landscape, touch and controllers, plain OpenGL ES
    case ue4       // Minecraft Dungeons and other Unreal Engine 4 games: landscape, touch and controllers, Vulkan on MoltenVK
    case godot     // A Godot 3 or 4 game: the manifest says which way up, OpenGL ES through ANGLE, multi-touch
    case nativeactivity // A game that is a NativeActivity library of its own (Open Golf): the manifest says which way up, OpenGL ES through ANGLE
    case gamemaker // A GameMaker game: YoYo's runner draws with OpenGL ES through ANGLE; options.ini says which way up
    case java      // A Java app (libGDX games): Husk's own Dalvik runtime runs its code, GLSurfaceView draws through ANGLE
    case flutter   // A Flutter app: the engine and the app's Dart run natively, Impeller draws through ANGLE; portrait unless the manifest says
}

/// A Unity game's screen: one CAMetalLayer that the game's own GL (ANGLE over Metal) presents into.
///
/// Nothing is copied or composed here. The runtime hands the layer to EGL as the game's window; the
/// game draws and presents on its own thread. This view's jobs are the layer's size, the pause that
/// goes with leaving the screen, and turning touches into the pixel coordinates Android reports.
final class TLUnityUIView: UIView, UIKeyInput {
    override class var layerClass: AnyClass { CAMetalLayer.self }

    /// A touch on a Java app's web page goes to the WKWebView under this view.
    override func hitTest(_ point: CGPoint, with event: UIEvent?) -> UIView? {
        if engine == .java, husk_java_web_hit(Float(point.x), Float(point.y)) != 0 { return nil }
        return super.hitTest(point, with: event)
    }

    /// The cocos2d-x or SDL game on screen, which the game's keyboard requests (they arrive on its own thread) are routed to.
    nonisolated(unsafe) static weak var cocosView: TLUnityUIView?

    private let apk: String
    /// The app's other APKs -- splits, an asset pack -- which an SDL game's libraries and data may be in.
    private let extraApks: [String]
    private let dataDir: String
    let engine: TLNativeEngine
    /// A portrait game is told its size when the screen is taller than wide, as a landscape one is when it is wider.
    private let portrait: Bool
    private(set) var launched = false
    /// The size, in points, the game was told it has as it started. Every engine but Flutter keeps it for good, so a window
    /// that is resized later scales the picture instead (TLWindowHost).
    private(set) var launchedSize: CGSize?
    /// In a floating window rather than full screen: no notch or home indicator to keep clear of.
    var windowed = false
    /// This flag supplies HUD insets; it never shrinks the game surface.
    var avoidUnsafeArea = true
    private var sentSafeInsets: UIEdgeInsets?
    private var keyboardInset: Int32 = 0
    private var gameSafeAreaInsets: UIEdgeInsets {
        guard avoidUnsafeArea, !windowed, UIDevice.current.userInterfaceIdiom == .phone, let window else { return .zero }
        let edge: TLGameViewport.CutoutEdge
        switch window.windowScene?.interfaceOrientation {
        case .portrait: edge = .top
        case .portraitUpsideDown: edge = .bottom
        // Interface orientation names describe the Home-button side; the camera is opposite.
        case .landscapeLeft: edge = .right
        case .landscapeRight: edge = .left
        default: edge = .none
        }
        // Use UIKit's resolved inset values; querying a layout guide here can require layout
        // while this view is already inside layoutSubviews.
        let safeFrame = convert(window.bounds.inset(by: window.safeAreaInsets), from: window)
        let inset = TLGameViewport.cutoutInsets(view: bounds, safeFrame: safeFrame, edge: edge)
        return UIEdgeInsets(top: inset.top, left: inset.left, bottom: inset.bottom, right: inset.right)
    }

    private func sendSafeInsetsIfNeeded() {
        let inset = gameSafeAreaInsets
        guard sentSafeInsets != inset else { return }
        sentSafeInsets = inset
        let k = contentScaleFactor
        let l = Int32((inset.left * k).rounded()), t = Int32((inset.top * k).rounded())
        let r = Int32((inset.right * k).rounded()), b = Int32((inset.bottom * k).rounded())
        HuskLog.log("tl", "display-cutout: pixels L=\(l) T=\(t) R=\(r) B=\(b); full-screen surface unchanged")
        switch engine {
        case .java: husk_java_set_insets(l, t, r, b, keyboardInset)
        case .sdl: husk_sdl_set_safe_insets(l, t, r, b)
        case .flutter: husk_flutter_set_insets(t, r, b, l)
        default: break
        }
    }
    /// Where the corner statistics go when this view does not draw them itself (a landscape game has its own bar).
    var onStats: ((String) -> Void)?
    /// Active touches by UITouch identity, each given a small stable id like Android's pointer ids.
    private var pointers: [ObjectIdentifier: Int32] = [:]

    /// "58 fps · 11.2 ms" in the corner, as the other screen has: frames the game finished per second, and the
    /// mean time one frame takes it. Refreshed once a second from the runtime's own counters.
    private let stats = UILabel()
    private var statsTimer: Timer?

    /// Called when three fingers tap at once: the way back to the interface while it is hidden.
    var onThreeFingerTap: (() -> Void)?

    private let nativeResolution: Bool

    init(apk: String, extraApks: [String] = [], dataDir: String, engine: TLNativeEngine, portrait: Bool = false, scale: CGFloat = 2,
         nativeResolution: Bool = false) {
        self.apk = apk
        self.portrait = portrait
        self.dataDir = dataDir
        self.engine = engine
        self.extraApks = extraApks
        self.nativeResolution = nativeResolution
        super.init(frame: .zero)
        backgroundColor = .black
        isMultipleTouchEnabled = true
        // Two pixels per point unless the game's settings say otherwise: sharp enough, and a third of the pixels a 3x phone would
        // ask the game for -- a 3D game is limited by fill rate, and ANGLE's translation costs on top.
        contentScaleFactor = scale
        if let metal = layer as? CAMetalLayer {
            metal.pixelFormat = .bgra8Unorm
            metal.framebufferOnly = true
            metal.contentsScale = scale
            metal.isOpaque = true
        }
        let threeFingers = UITapGestureRecognizer(target: self, action: #selector(threeFingerTapped))
        threeFingers.numberOfTouchesRequired = 3
        threeFingers.cancelsTouchesInView = false
        threeFingers.delaysTouchesBegan = false
        threeFingers.delaysTouchesEnded = false
        addGestureRecognizer(threeFingers)

        stats.font = .monospacedSystemFont(ofSize: 10, weight: .medium)
        stats.textColor = .white
        stats.backgroundColor = UIColor.black.withAlphaComponent(0.55)
        stats.layer.cornerRadius = 4
        stats.layer.masksToBounds = true
        stats.textAlignment = .center
        stats.isUserInteractionEnabled = false
        stats.text = " "
        TLUnityUIView.installOpenURLHandler()
        if engine == .unity { addSubview(stats) }
        if engine == .unity {
            TLUnityUIView.cocosView = self
            TLUnityUIView.installUnityKeyboardHandler()
        } else if engine == .cocos {
            TLUnityUIView.cocosView = self
            TLUnityUIView.installKeyboardHandler()
        } else if engine == .sdl {
            TLUnityUIView.cocosView = self
            TLUnityUIView.installSDLKeyboardHandler()
        } else if engine == .minecraft {
            TLUnityUIView.cocosView = self
            TLUnityUIView.installGameActivityKeyboardHandler()
        } else if engine == .java {
            TLUnityUIView.cocosView = self
            TLUnityUIView.installJavaHandlers()
            // Android's back gesture: a swipe in from the left edge.
            let back = UIScreenEdgePanGestureRecognizer(target: self, action: #selector(edgeSwiped(_:)))
            back.edges = .left
            back.cancelsTouchesInView = false
            back.delaysTouchesBegan = false
            back.delaysTouchesEnded = false
            addGestureRecognizer(back)
            // The keyboard covers the bottom of the app: its views are told (an adjustResize window shrinks above it).
            for name in [UIResponder.keyboardWillChangeFrameNotification, UIResponder.keyboardWillHideNotification] {
                NotificationCenter.default.addObserver(forName: name, object: nil, queue: .main) { [weak self] note in
                    self?.keyboardMoved(note)
                }
            }
        } else if engine == .flutter {
            TLUnityUIView.cocosView = self
            TLUnityUIView.installFlutterHandlers()
            // Android's back gesture: a swipe in from the left edge.
            let back = UIScreenEdgePanGestureRecognizer(target: self, action: #selector(edgeSwiped(_:)))
            back.edges = .left
            back.cancelsTouchesInView = false
            back.delaysTouchesBegan = false
            back.delaysTouchesEnded = false
            addGestureRecognizer(back)
            // The keyboard covers the bottom of the app: Flutter is told, and moves the focused field above it.
            for name in [UIResponder.keyboardWillChangeFrameNotification, UIResponder.keyboardWillHideNotification] {
                NotificationCenter.default.addObserver(forName: name, object: nil, queue: .main) { [weak self] note in
                    self?.keyboardMoved(note)
                }
            }
        }
        // The GPU is not the app's while it is in the background: stop drawing, and carry on when it returns.
        NotificationCenter.default.addObserver(forName: UIApplication.willResignActiveNotification, object: nil, queue: .main) { _ in
            husk_unity_set_paused(true)
        }
        NotificationCenter.default.addObserver(forName: UIApplication.didBecomeActiveNotification, object: nil, queue: .main) { [weak self] _ in
            if self?.window != nil { husk_unity_set_paused(false) }
        }
    }

    @objc private func threeFingerTapped() { onThreeFingerTap?() }

    /// A Java app's WebViews (WKWebViews) live in a view right under this one, where the app's window leaves a hole for them.
    /// Touches on a page go straight to it, so the back swipe and the three-finger tap are on that view too.
    private var webHost: UIView?
    private func attachWebHost(fresh: Bool) {
        guard let superview else { return }
        webHost?.removeFromSuperview()
        let host = UIView(frame: .zero)
        host.backgroundColor = .black
        superview.insertSubview(host, belowSubview: self)
        let back = UIScreenEdgePanGestureRecognizer(target: self, action: #selector(edgeSwiped(_:)))
        back.edges = .left
        back.cancelsTouchesInView = false
        back.delaysTouchesBegan = false
        back.delaysTouchesEnded = false
        host.addGestureRecognizer(back)
        let threeFingers = UITapGestureRecognizer(target: self, action: #selector(threeFingerTapped))
        threeFingers.numberOfTouchesRequired = 3
        threeFingers.cancelsTouchesInView = false
        threeFingers.delaysTouchesBegan = false
        threeFingers.delaysTouchesEnded = false
        host.addGestureRecognizer(threeFingers)
        webHost = host
        layoutWebHost()
        husk_java_web_attach(Unmanaged.passUnretained(self).toOpaque(), Unmanaged.passUnretained(host).toOpaque(), Float(contentScaleFactor), fresh ? 1 : 0)
    }

    private func layoutWebHost() {
        guard let host = webHost else { return }
        // Preserve the same local coordinates as the game when its cached surface is scaled to fit.
        if host.bounds != bounds { host.bounds = bounds }
        if host.center != center { host.center = center }
        if host.transform != transform { host.transform = transform }
    }

    override func didMoveToSuperview() {
        super.didMoveToSuperview()
        if engine == .java, launched, superview != nil { attachWebHost(fresh: false) }
    }

    override func willMove(toSuperview newSuperview: UIView?) {
        super.willMove(toSuperview: newSuperview)
        if newSuperview == nil { webHost?.removeFromSuperview() }
    }

    @objc private func edgeSwiped(_ g: UIScreenEdgePanGestureRecognizer) {
        if g.state == .ended, g.translation(in: self).x > 60 { if engine == .java { husk_java_back() } else { husk_flutter_back() } }
    }

    private func keyboardMoved(_ note: Notification) {
        guard let window, let end = note.userInfo?[UIResponder.keyboardFrameEndUserInfoKey] as? CGRect else { return }
        let hiding = note.name == UIResponder.keyboardWillHideNotification
        let keyboard = convert(window.convert(end, from: nil), from: window)
        let covered = hiding ? 0 : min(bounds.height, max(0, bounds.maxY - keyboard.minY))
        keyboardInset = Int32((covered * contentScaleFactor).rounded())
        if engine == .java {
            let k = contentScaleFactor, inset = gameSafeAreaInsets
            husk_java_set_insets(Int32(inset.left * k), Int32(inset.top * k), Int32(inset.right * k), Int32(inset.bottom * k), keyboardInset)
            if hiding, isFirstResponder == false { husk_java_keyboard_closed() }
            return
        }
        husk_flutter_set_keyboard_inset(keyboardInset)
    }

    deinit { statsTimer?.invalidate(); NotificationCenter.default.removeObserver(self) }

    private func updateStats() {
        var p = husk_unity_perf()
        husk_unity_perf_snapshot(&p)
        let text = p.fps > 0
            ? String(format: "%.0f fps · %.1f ms · max %.0f", p.fps, p.mean_ms, p.max_ms)
            : "starting"
        stats.text = text
        onStats?(text)
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    override func layoutSubviews() {
        super.layoutSubviews()
        let statsFrame = CGRect(x: bounds.width - 148, y: bounds.height - 22, width: 142, height: 16)
        if stats.frame != statsFrame { stats.frame = statsFrame }
        layoutWebHost()
        guard bounds.width > 0, bounds.height > 0 else { return }
        // Resolve the actual attached screen before launch (including external displays).
        // A running engine retains its launch dimensions until its session closes.
        if nativeResolution, !launched, let screen = window?.screen {
            let scale = screen.nativeScale
            if contentScaleFactor != scale { contentScaleFactor = scale }
            if let metal = layer as? CAMetalLayer, metal.contentsScale != scale { metal.contentsScale = scale }
        }
        let w = Int((bounds.width * contentScaleFactor).rounded())
        let h = Int((bounds.height * contentScaleFactor).rounded())
        // An Unreal game draws with MoltenVK, which sizes this layer itself to the swapchain it made; sizing it back here on every layout would leave the layer and the
        // swapchain disagreeing from then on.
        if !(engine == .ue4 && launched), let metal = layer as? CAMetalLayer {
            let pixels = CGSize(width: w, height: h)
            if metal.drawableSize != pixels { metal.drawableSize = pixels }
        }
        sendSafeInsetsIfNeeded()
        // Flutter lays itself out again at any size, as an Android view does when its window changes: a window being resized, the
        // screen turning.
        if engine == .flutter, launched { husk_flutter_resize(Int32(w), Int32(h)) }
        // A landscape game is told its size once, when it starts, so it must not start while the screen is still
        // turning: wait for a surface that matches the game's orientation.
        if firstLayout == nil, window != nil {
            firstLayout = Date()
        }
        if !launched, window != nil {
            let orientationMatch = portrait ? (h >= w) : (w >= h)
            let waited = firstLayout.map { Date().timeIntervalSince($0) > 3.5 } ?? false
            if orientationMatch || waited {
                launch(width: w, height: h)
            } else {
                HuskOrientation.set(portrait ? .portrait : .landscape)
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.2) { [weak self] in
                    self?.setNeedsLayout()
                }
            }
        }
    }

    private var firstLayout: Date?

    override func didMoveToWindow() {
        super.didMoveToWindow()
        statsTimer?.invalidate()
        statsTimer = nil
        if window != nil {
            husk_unity_set_paused(false)
            statsTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in self?.updateStats() }
        } else {
            husk_unity_set_paused(true)
            if isFirstResponder { resignFirstResponder() }
        }
        setNeedsLayout()
    }

    private func launch(width: Int, height: Int) {
        launched = true
        launchedSize = bounds.size
        let angle = (Bundle.main.privateFrameworksPath ?? "") + "/libANGLE-shared.dylib"
        let ca = Bundle.main.path(forResource: "cacert", ofType: "pem") ?? ""
        try? FileManager.default.createDirectory(atPath: dataDir, withIntermediateDirectories: true)
        let layerPtr = Unmanaged.passUnretained(layer).toOpaque()
        let loaded = husk_native_loaded_apk().map { String(cString: $0) }
        let st = husk_unity_state()
        if st != Int32(HUSK_UNITY_IDLE) {
            if loaded == apk, st == Int32(HUSK_UNITY_RUNNING) {
                HuskLog.log("tl", "unity: already started; resuming")
                if engine == .java { attachWebHost(fresh: false) }
                return
            }
            HuskLog.log("tl", "native: \(loaded ?? "another app") is still loaded; close Husk to start \(apk)")
            return
        }
        let appID = ((dataDir as NSString).deletingLastPathComponent as NSString).lastPathComponent
        if TLAppSettings.load(appID).startupDiagnostics {
            for name in ["TL_STARTUP_TRACE", "TL_ICONV_TRACE", "TL_FILE_TRACE", "TL_NET_TRACE"] {
                setenv(name, "1", 1)
            }
            HuskLog.log("tl", "startup: detailed diagnostics enabled for this launch")
        }
        // The game plays through the silent switch, like the guest's own audio, and mixes with other audio. Unity games too:
        // they used to be left out, from before they had sound, and played through a session that was never set up.
        do {
            let session = AVAudioSession.sharedInstance()
            try? session.setCategory(.playback, mode: .default, options: [.mixWithOthers])
            try? session.setActive(true)
        }
        HuskLog.log("tl", "native: launching \(apk) at \(width)x\(height) (\(engine == .cocos ? "cocos2d-x" : engine == .minecraft ? "gameactivity" : engine == .sdl ? "sdl" : engine == .ue4 ? "ue4" : engine == .gta ? "gta" : engine == .godot ? "godot" : engine == .nativeactivity ? "nativeactivity" : engine == .flutter ? "flutter" : engine == .gamemaker ? "gamemaker" : engine == .java ? "java" : "unity"))")
        // Splits and the asset pack are part of the app, whatever its engine; the game's libraries and data may be in any of
        // them (a Google Play install keeps a Unity game's libraries in one split and its data in an asset pack).
        JITLess.prepareLaunch()
        for extra in extraApks.prefix(15) { husk_native_add_package(extra) }
        // Android's shared storage, one folder for every game: a game that keeps its data in a folder of its own on /sdcard finds it
        // in Husk's "Shared Storage", which can be filled from Files or Finder.
        husk_native_set_shared_storage(TranslationLayer.sharedStorage.path)
        // The CA certificates and system fonts Android keeps under /system, as the app carries them.
        if let sys = Bundle.main.resourcePath.map({ $0 + "/android-system" }), FileManager.default.fileExists(atPath: sys) {
            husk_native_set_system_files(sys + "/cacerts", sys)
        }
        husk_native_set_extra_fonts(CJKFonts.directory.path)
        let started: Bool
        switch engine {
        case .sdl:
            // Vulkan for the SDL games that draw with it (directly, or through DXVK): MoltenVK, as for Unreal.
            if let fw = Bundle.main.privateFrameworksPath { husk_ue4_set_vulkan(fw + "/MoltenVK.framework/MoltenVK") }
            started = husk_sdl_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .gta: started = husk_gta_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .godot: started = husk_godot_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .nativeactivity: started = husk_ue4_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)       // the NativeActivity driver; with no Unreal in the APK it runs the plain game
        case .ue4:
            // Unreal draws with Vulkan, which on this device is MoltenVK, a framework of the app's own.
            if let fw = Bundle.main.privateFrameworksPath { husk_ue4_set_vulkan(fw + "/MoltenVK.framework/MoltenVK") }
            started = husk_ue4_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .cocos:
            // Geode, when it is on for this game and downloaded: loaded into the game after its own libraries.
            let geode = GeodeSupport.files(appDir: (dataDir as NSString).deletingLastPathComponent)
            husk_cocos_set_geode(geode?.zip, geode?.launcher)
            if geode != nil { HuskLog.log("geode", "loading Geode into the game") }
            started = husk_cocos_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .java:
            // Husk's Dalvik runtime: libcore, ICU and Husk's Java framework, carried in the app.
            husk_java_set_quest_only_cutout(TLAppSettings.load(appID).questOnlyCutout ? 1 : 0)
            husk_java_set_runtime((Bundle.main.resourcePath ?? "") + "/java-runtime", Float(contentScaleFactor))
            let screen = window?.screen ?? UIScreen.main
            husk_java_set_display_max_refresh(Float(screen.maximumFramesPerSecond))
            HuskLog.log("tl", "host display: nativePixels=\(screen.nativeBounds.size) nativeScale=\(screen.nativeScale) renderScale=\(contentScaleFactor) refreshCapability=\(screen.maximumFramesPerSecond) Hz")
            husk_java_set_night_mode(traitCollection.userInterfaceStyle == .dark ? 1 : 0)
            attachWebHost(fresh: true)
            started = husk_java_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .gamemaker: started = husk_gamemaker_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .flutter:
            // The screen's scale is the app's device pixel ratio; cutout insets were sent during layout.
            husk_flutter_set_pixel_ratio(Float(contentScaleFactor))
            started = husk_flutter_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .minecraft: started = husk_gameactivity_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        case .unity: started = husk_unity_launch(apk, dataDir, layerPtr, Int32(width), Int32(height), angle, ca)
        }
        if !started { HuskLog.log("tl", "native: launch refused") }
    }

    // MARK: keyboard (cocos2d-x and SDL games)

    /// A game asks for the keyboard when its text field is tapped. The keyboard belongs to this view; what it types goes
    /// to the game, and a strip above the keyboard shows the text, because in landscape the keyboard covers the game's field.
    override var canBecomeFirstResponder: Bool { engine == .cocos || engine == .sdl || engine == .minecraft || engine == .flutter || engine == .java || engine == .unity }
    var hasText: Bool { true }
    var autocorrectionType: UITextAutocorrectionType = .no
    var autocapitalizationType: UITextAutocapitalizationType = .none
    var spellCheckingType: UITextSpellCheckingType = .no
    var smartQuotesType: UITextSmartQuotesType = .no
    var smartDashesType: UITextSmartDashesType = .no
    var smartInsertDeleteType: UITextSmartInsertDeleteType = .no
    var keyboardType: UIKeyboardType = .default
    var keyboardAppearance: UIKeyboardAppearance = .dark
    var returnKeyType: UIReturnKeyType = .done
    var isSecureTextEntry = false

    private var typed = ""
    private lazy var typedLabel: UILabel = {
        let l = UILabel()
        l.font = .systemFont(ofSize: 17, weight: .medium)
        l.textColor = .white
        l.lineBreakMode = .byTruncatingHead
        return l
    }()
    private lazy var keyboardBar: UIView = {
        let bar = UIView(frame: CGRect(x: 0, y: 0, width: 100, height: 44))
        bar.backgroundColor = UIColor(white: 0.12, alpha: 1)
        bar.autoresizingMask = [.flexibleWidth]
        typedLabel.frame = CGRect(x: 16, y: 0, width: 100, height: 44)
        typedLabel.autoresizingMask = [.flexibleWidth]
        bar.addSubview(typedLabel)
        let done = UIButton(type: .system)
        done.setTitle("Done", for: .normal)
        done.titleLabel?.font = .systemFont(ofSize: 17, weight: .semibold)
        done.frame = CGRect(x: 100, y: 0, width: 80, height: 44)
        done.autoresizingMask = [.flexibleLeftMargin]
        done.addAction(UIAction { [weak self] _ in self?.finishTyping() }, for: .touchUpInside)
        bar.addSubview(done)
        return bar
    }()
    override var inputAccessoryView: UIView? { engine == .cocos || engine == .sdl || engine == .minecraft || engine == .unity ? keyboardBar : nil }

    /// The keyboard was opened from the screen's keyboard button, not by a field in the app: what is typed then is key presses for
    /// apps that read keys themselves (an SDL game's menus, a terminal), where the engine has keys to send.
    var openedByHand = false

    /// Whether the screen's keyboard button can do anything for this engine.
    var offersKeyboardButton: Bool { canBecomeFirstResponder }

    /// The keyboard button: opens the keyboard when it is away (for an app that never asks for it), puts it away when it is up.
    func toggleKeyboardByHand() {
        if isFirstResponder { _ = resignFirstResponder(); return }
        openedByHand = true
        keyboardType = .default
        isSecureTextEntry = false
        returnKeyType = .default
        setTyped("")
        if !becomeFirstResponder() { openedByHand = false }
    }

    /// Android key codes for what the keyboard types (US layout), with whether shift goes with it.
    private static func androidKey(_ ch: Character) -> (code: Int32, shift: Bool)? {
        if let a = ch.asciiValue {
            switch a {
            case 97...122: return (29 + Int32(a - 97), false)          // a-z
            case 65...90: return (29 + Int32(a - 65), true)           // A-Z
            case 48...57: return (7 + Int32(a - 48), false)           // 0-9
            default: break
            }
        }
        let shifted: [Character: Int32] = ["!": 8, "@": 9, "#": 10, "$": 11, "%": 12, "^": 13, "&": 14, "*": 15, "(": 16, ")": 7,
                                           "<": 55, ">": 56, "~": 68, "_": 69, "+": 70, "{": 71, "}": 72, "|": 73, ":": 74, "\"": 75, "?": 76]
        let plain: [Character: Int32] = [",": 55, ".": 56, "\t": 61, " ": 62, "\n": 66, "`": 68, "-": 69, "=": 70, "[": 71, "]": 72,
                                         "\\": 73, ";": 74, "'": 75, "/": 76]
        if let c = plain[ch] { return (c, false) }
        if let c = shifted[ch] { return (c, true) }
        return nil
    }
    private func pressSDLKeys(_ text: String) {
        for ch in text {
            guard let k = Self.androidKey(ch) else { continue }
            if k.shift { husk_sdl_key(59, 1) }                     // KEYCODE_SHIFT_LEFT
            husk_sdl_key(k.code, 1); husk_sdl_key(k.code, 0)
            if k.shift { husk_sdl_key(59, 0) }
        }
    }

    func insertText(_ text: String) {
        if openedByHand, engine == .sdl { pressSDLKeys(text); return }
        if engine == .java {
            // The app's own text field shows what is typed; Return is the field's action (a new line in a multi-line one).
            if text == "\n" { husk_java_text_action() } else { husk_java_insert_text(text) }
            return
        }
        if engine == .flutter {
            // Flutter's field draws the text itself: no strip over the keyboard, and Return is the field's action.
            if text == "\n", !husk_flutter_text_multiline() { husk_flutter_text_action() } else { husk_flutter_insert_text(text) }
            return
        }
        if text == "\n", !(engine == .unity && unityMultiline) { finishTyping(); return }
        if engine == .unity {
            // The game's field may refuse characters past its limit: the strip shows what the field holds.
            husk_unity_insert_text(text)
            var buf = [CChar](repeating: 0, count: 4096)
            husk_unity_text(&buf, UInt(buf.count))
            let field = String(cString: buf)
            setTyped(isSecureTextEntry ? String(repeating: "•", count: field.count) : field)
            return
        }
        typed += text
        typedLabel.text = typed
        if engine == .sdl { husk_sdl_commit_text(text) } else if engine == .minecraft { husk_ga_insert_text(text) } else { husk_cocos_insert_text(text) }
    }

    func deleteBackward() {
        if openedByHand, engine == .sdl { husk_sdl_key(67, 1); husk_sdl_key(67, 0); return }   // KEYCODE_DEL
        if engine == .java { husk_java_delete_backward(); return }
        if engine == .flutter { husk_flutter_delete_backward(); return }
        if engine == .unity { husk_unity_delete_backward() }
        if !typed.isEmpty { typed.removeLast() }
        typedLabel.text = typed
        if engine == .sdl { husk_sdl_key(67, 1); husk_sdl_key(67, 0) }   // KEYCODE_DEL
        else if engine == .minecraft { husk_ga_delete_backward() }
        else if engine != .unity { husk_cocos_delete_backward() }
    }

    private func setTyped(_ text: String) { typed = text; typedLabel.text = text }

    /// Return, or the Done button: what Android's "done" action does -- the game gets a newline, and the keyboard goes.
    private func finishTyping() {
        if engine == .unity { husk_unity_finish_text(1) }   // the dialog's OK: the field keeps the text, and the game hears it is done
        else if engine == .sdl { husk_sdl_key(66, 1); husk_sdl_key(66, 0) }   // KEYCODE_ENTER
        else if engine == .minecraft { husk_ga_editor_action() }        // the field's own action: send the chat, name the world
        else { husk_cocos_insert_text("\n") }
        resignFirstResponder()
    }

    /// A Flutter app's text fields ask for the keyboard (on its platform thread), and the app may close itself from its first screen.
    static func installFlutterHandlers() {
        husk_flutter_set_keyboard_handler { show, kind in
            DispatchQueue.main.async {
                guard let view = TLUnityUIView.cocosView, view.engine == .flutter else { return }
                if show != 0 {
                    switch kind & 0xF {
                    case 1: view.keyboardType = .numbersAndPunctuation
                    case 2: view.keyboardType = .emailAddress
                    case 3: view.keyboardType = .URL
                    default: view.keyboardType = .default
                    }
                    view.isSecureTextEntry = kind & 0x20 != 0
                    view.returnKeyType = kind & 0x10 != 0 ? .default : .done
                    view.autocapitalizationType = kind & 0xF == 0 && kind & 0x20 == 0 ? .sentences : .none
                    view.openedByHand = false; if view.isFirstResponder { view.reloadInputViews() } else { view.becomeFirstResponder() }
                } else {
                    view.resignFirstResponder()
                }
            }
        }
        husk_flutter_set_close_handler {
            DispatchQueue.main.async { NotificationCenter.default.post(name: TLUnityUIView.appClosedItself, object: nil) }
        }
    }

    /// A Java app's views (Husk's framework): a focused text field asks for the keyboard with Android's EditorInfo input type and
    /// IME options; the clipboard is the iPhone's; sharing opens the share sheet.
    static func installJavaHandlers() {
        husk_java_set_host({ show, inputType, imeOptions in
            DispatchQueue.main.async {
                guard let view = TLUnityUIView.cocosView, view.engine == .java else { return }
                guard show != 0 else { view.resignFirstResponder(); return }
                let cls = inputType & 0xF, variation = inputType & 0xFF0
                let newSecure = (cls == 1 && (variation == 0x80 || variation == 0xE0)) || (cls == 2 && variation == 0x10)
                let multiLine = cls == 1 && inputType & 0x20000 != 0
                let newReturnKey: UIReturnKeyType
                switch imeOptions & 0xFF {
                case 2: newReturnKey = .go
                case 3: newReturnKey = .search
                case 4: newReturnKey = .send
                case 5: newReturnKey = .next
                default: newReturnKey = multiLine ? .default : .done
                }
                let caps = inputType & 0x7000
                let newCaps: UITextAutocapitalizationType = cls != 1 || newSecure ? .none : caps & 0x1000 != 0 ? .allCharacters : caps & 0x2000 != 0 ? .words : caps & 0x4000 != 0 ? .sentences : .none
                let newCorrection: UITextAutocorrectionType = cls == 1 && inputType & 0x8000 != 0 ? .yes : .no
                let newKeyboardType: UIKeyboardType
                switch cls {
                case 2: newKeyboardType = variation == 0x10 ? .numberPad : .decimalPad           // number (password: digits only)
                case 3: newKeyboardType = .phonePad
                case 4: newKeyboardType = .numbersAndPunctuation
                default: newKeyboardType = variation == 0x20 || variation == 0xD0 ? .emailAddress : variation == 0x10 ? .URL : .default
                }

                let traitsChanged = view.keyboardType != newKeyboardType ||
                                    view.isSecureTextEntry != newSecure ||
                                    view.returnKeyType != newReturnKey ||
                                    view.autocapitalizationType != newCaps ||
                                    view.autocorrectionType != newCorrection

                view.keyboardType = newKeyboardType
                view.isSecureTextEntry = newSecure
                view.returnKeyType = newReturnKey
                view.autocapitalizationType = newCaps
                view.autocorrectionType = newCorrection
                view.openedByHand = false

                if view.isFirstResponder {
                    if traitsChanged { view.reloadInputViews() }
                } else {
                    view.becomeFirstResponder()
                }
            }
        }, { text in
            guard let text else { return }
            let s = String(cString: text)
            DispatchQueue.main.async { UIPasteboard.general.string = s }
        }, {
            guard let s = UIPasteboard.general.string else { return nil }
            return strdup(s)
        }, { text in
            guard let text else { return }
            let s = String(cString: text)
            DispatchQueue.main.async {
                guard let view = TLUnityUIView.cocosView, let root = view.window?.rootViewController else { return }
                var top = root
                while let p = top.presentedViewController { top = p }
                let sheet = UIActivityViewController(activityItems: [s], applicationActivities: nil)
                sheet.popoverPresentationController?.sourceView = view
                sheet.popoverPresentationController?.sourceRect = CGRect(x: view.bounds.midX, y: view.bounds.midY, width: 1, height: 1)
                top.present(sheet, animated: true)
            }
        }, { orientation in
            HuskLog.log("tl", "java: the app asks for orientation \(orientation)")
            DispatchQueue.main.async {
                if orientation == 0 || orientation == 6 || orientation == 8 || orientation == 11 {
                    HuskOrientation.set(.landscape)
                } else if orientation == 1 || orientation == 7 || orientation == 9 || orientation == 12 {
                    HuskOrientation.set(.portrait)
                }
            }
        })
        husk_java_set_document_picker { mimeTypes, multiple in
            let types = TLUnityUIView.contentTypes(mimeTypes.map { String(cString: $0) } ?? "*/*")
            DispatchQueue.main.async {
                HuskFilePicker.present(types: types, multiple: multiple != 0, asCopy: true, onPick: { urls in
                    // copies in a folder of their own: the app reads them by path, long after the picker's access ends
                    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("picked/\(UUID().uuidString)", isDirectory: true)
                    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
                    var paths: [String] = []
                    for url in urls {
                        let scoped = url.startAccessingSecurityScopedResource()
                        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                        let dest = dir.appendingPathComponent(url.lastPathComponent)
                        do { try FileManager.default.copyItem(at: url, to: dest); paths.append(dest.path) }
                        catch { HuskLog.log("tl", "java: could not copy the picked \(url.lastPathComponent): \(error.localizedDescription)") }
                    }
                    TLUnityUIView.answerDocumentPicker(paths)
                }, onCancel: { TLUnityUIView.answerDocumentPicker([]) })
            }
        }
    }

    /// The picker's answer to the Java app (no paths: it was cancelled).
    private static func answerDocumentPicker(_ paths: [String]) {
        let c = paths.map { strdup($0) }
        defer { c.forEach { free($0) } }
        var ptrs = c.map { UnsafePointer<CChar>($0) }
        ptrs.withUnsafeMutableBufferPointer { husk_java_documents_picked($0.baseAddress, Int32(paths.count)) }
    }

    /// The document types for an intent's MIME types ("image/*,application/pdf"); anything unknown opens the picker to all files.
    private static func contentTypes(_ mimeTypes: String) -> [UTType] {
        var types: [UTType] = []
        for m in mimeTypes.split(separator: ",").map({ $0.trimmingCharacters(in: .whitespaces).lowercased() }) {
            switch m {
            case "*/*", "": return [.item]
            case "image/*": types.append(.image)
            case "video/*": types.append(.movie)
            case "audio/*": types.append(.audio)
            case "text/*": types.append(.text)
            case "application/*": types.append(.data)
            default:
                guard let t = UTType(mimeType: m) else { return [.item] }
                types.append(t)
            }
        }
        return types.isEmpty ? [.item] : types
    }

    /// A Flutter app closed itself (back on its first screen): whatever shows it closes.
    static let appClosedItself = Notification.Name("husk.tl.appClosedItself")
    /// The current translation-layer session was stopped so another app can start.
    static let huskStopPlaying = Notification.Name("husk.stopPlaying")

    /// Shared handler for URLs and APK updates opened by any runtime engine (Java, Cocos, SDL, Godot, GameMaker).
    static func installOpenURLHandler() {
        husk_cocos_set_open_url_handler { url in
            guard let url else { return }
            let raw = String(cString: url)
            Task { @MainActor in
                TLUnityUIView.handleOpenURL(raw)
            }
        }
    }

    private static let apkUpdateExtensions: Set<String> = ["apk", "xapk", "apkm", "apks"]
    private static var activeUpdateTask: Task<Void, Never>?

    private static func isAPKUpdateURL(_ link: URL) -> Bool {
        if link.isFileURL {
            return apkUpdateExtensions.contains(link.pathExtension.lowercased())
        }
        guard let scheme = link.scheme?.lowercased(), scheme == "http" || scheme == "https" else {
            return false
        }
        if link.fragment == "husk.apk" { return true }
        return apkUpdateExtensions.contains(link.pathExtension.lowercased())
    }

    private static func topPresenter() -> UIViewController? {
        let scenes = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
        let window = TLUnityUIView.cocosView?.window
            ?? scenes.first(where: { $0.activationState == .foregroundActive })?.windows.first(where: \.isKeyWindow)
            ?? scenes.flatMap(\.windows).first(where: { !$0.isHidden })
        guard var top = window?.rootViewController else { return nil }
        while let presented = top.presentedViewController, !presented.isBeingDismissed {
            top = presented
        }
        return top
    }

    private static func dismissAlert(_ alert: UIAlertController) async {
        while alert.isBeingPresented || alert.isBeingDismissed {
            try? await Task.sleep(nanoseconds: 50_000_000)
        }
        guard alert.presentingViewController != nil else { return }
        await withCheckedContinuation { (cont: CheckedContinuation<Void, Never>) in
            alert.dismiss(animated: true) { cont.resume() }
        }
    }

    private static func handleOpenURL(_ raw: String) {
        let trimmed = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty,
              let link = URL(string: trimmed)
                  ?? trimmed.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed).flatMap(URL.init(string:)) else {
            HuskLog.log("tl", "open url ignored invalid link: \(raw)")
            return
        }
        HuskLog.log("tl", "open url: \(link.absoluteString)")
        if isAPKUpdateURL(link) {
            performInAppAPKUpdate(link)
            return
        }
        UIApplication.shared.open(link)
    }

    private static func performInAppAPKUpdate(_ link: URL) {
        guard activeUpdateTask == nil else {
            HuskLog.log("tl", "apk update already running; ignoring duplicate request")
            return
        }
        var cleanLink = link
        if cleanLink.fragment == "husk.apk",
           var comps = URLComponents(url: cleanLink, resolvingAgainstBaseURL: false) {
            comps.fragment = nil
            if let stripped = comps.url { cleanLink = stripped }
        }

        let progressAlert = UIAlertController(
            title: "Đang cập nhật APK…",
            message: cleanLink.isFileURL ? "Đang cài đặt bản cập nhật…" : "Đang kết nối máy chủ cập nhật…",
            preferredStyle: .alert
        )
        progressAlert.addAction(UIAlertAction(title: "Huỷ", style: .cancel) { _ in
            TLUnityUIView.activeUpdateTask?.cancel()
            TLUnityUIView.activeUpdateTask = nil
        })
        topPresenter()?.present(progressAlert, animated: true)

        activeUpdateTask = Task { @MainActor in
            defer { TLUnityUIView.activeUpdateTask = nil }
            let fm = FileManager.default
            var tempDir: URL?
            defer {
                if let tempDir { try? fm.removeItem(at: tempDir) }
            }

            do {
                let installURL: URL
                let moveIntoStore: Bool
                if cleanLink.isFileURL {
                    guard fm.fileExists(atPath: cleanLink.path) else {
                        throw APKUpdateError.message("Không tìm thấy tệp APK cập nhật cục bộ.")
                    }
                    installURL = cleanLink
                    moveIntoStore = cleanLink.lastPathComponent == "husk-update.apk"
                } else {
                    let dir = fm.temporaryDirectory.appendingPathComponent("apk-update-\(UUID().uuidString)", isDirectory: true)
                    try fm.createDirectory(at: dir, withIntermediateDirectories: true)
                    tempDir = dir

                    var fileName = cleanLink.lastPathComponent
                    if fileName.isEmpty || fileName == "/" {
                        fileName = "update.apk"
                    } else if !apkUpdateExtensions.contains((fileName as NSString).pathExtension.lowercased()) {
                        fileName += ".apk"
                    }
                    let destURL = dir.appendingPathComponent(fileName)

                    try await APKUpdateDownloader.download(from: cleanLink, to: destURL) { written, expected in
                        Task { @MainActor in
                            let doneMB = Double(written) / 1_048_576.0
                            if expected > 0 {
                                let totalMB = Double(expected) / 1_048_576.0
                                let pct = min(100, max(0, Int((Double(written) * 100.0 / Double(expected)).rounded())))
                                progressAlert.message = String(format: "Đang tải gói cập nhật: %.1f / %.1f MB (%d%%)", doneMB, totalMB, pct)
                            } else {
                                progressAlert.message = String(format: "Đang tải gói cập nhật: %.1f MB…", doneMB)
                            }
                        }
                    }

                    let fh = try FileHandle(forReadingFrom: destURL)
                    let magic = try fh.read(upToCount: 4)
                    try? fh.close()
                    guard magic == Data([0x50, 0x4B, 0x03, 0x04]) else {
                        throw APKUpdateError.message("Tệp tải về không phải gói APK hợp lệ.")
                    }
                    installURL = destURL
                    moveIntoStore = true
                }

                try Task.checkCancellation()
                progressAlert.message = "Đang cài đặt bản cập nhật\n(giữ nguyên dữ liệu game)…"

                let result = await Task.detached(priority: .userInitiated) {
                    TranslationLayerStore.installUpdate(installURL, move: moveIntoStore)
                }.value

                if let failure = result.error {
                    throw APKUpdateError.message(failure)
                }

                TranslationLayerStore.shared.reload()
                await dismissAlert(progressAlert)

                let gameTitle = result.label ?? "game"
                let doneAlert = UIAlertController(
                    title: "Cập nhật hoàn tất",
                    message: "Đã cập nhật \(gameTitle) thành công và giữ nguyên toàn bộ dữ liệu.\nĐóng Husk ngay bây giờ để mở lại phiên bản mới?",
                    preferredStyle: .alert
                )
                doneAlert.addAction(UIAlertAction(title: "Đóng để mở bản mới", style: .default) { _ in
                    if let appID = result.appID {
                        Router.shared.switchTo(appID)
                    }
                    CrashReport.gameEnded()
                    HuskLog.flushNow()
                    exit(0)
                })
                doneAlert.addAction(UIAlertAction(title: "Để sau", style: .cancel))
                topPresenter()?.present(doneAlert, animated: true)
            } catch {
                let nsErr = error as NSError
                let isCancelled = Task.isCancelled
                    || error is CancellationError
                    || (nsErr.domain == NSURLErrorDomain && nsErr.code == NSURLErrorCancelled)
                await dismissAlert(progressAlert)
                guard !isCancelled else {
                    HuskLog.log("tl", "apk update cancelled by user")
                    return
                }
                HuskLog.log("tl", "apk update failed: \(error.localizedDescription)")
                let failAlert = UIAlertController(
                    title: "Cập nhật thất bại",
                    message: error.localizedDescription,
                    preferredStyle: .alert
                )
                if !cleanLink.isFileURL {
                    failAlert.addAction(UIAlertAction(title: "Mở bằng Safari", style: .default) { _ in
                        UIApplication.shared.open(cleanLink)
                    })
                }
                failAlert.addAction(UIAlertAction(title: "Đóng", style: .cancel))
                topPresenter()?.present(failAlert, animated: true)
            }
        }
    }

    /// The game's own requests, from its GL thread: 0 toggles, 1 shows, 2 hides.
    static func installKeyboardHandler() {
        installOpenURLHandler()
        husk_cocos_set_keyboard_handler { action in
            DispatchQueue.main.async {
                guard let view = TLUnityUIView.cocosView else { return }
                let show = action == 1 || (action == 0 && !view.isFirstResponder)
                if show {
                    // Start the strip from what the game's field already holds, so editing a name shows the whole name.
                    husk_cocos_request_text { text in
                        let seed = text.map { String(cString: $0) } ?? ""
                        DispatchQueue.main.async { TLUnityUIView.cocosView?.setTyped(seed) }
                    }
                    view.becomeFirstResponder()
                } else {
                    view.resignFirstResponder()
                }
            }
        }
    }

    /// A GameActivity game's requests (Minecraft's text fields, through GameTextInput): 1 shows the keyboard, 2 hides it.
    static func installGameActivityKeyboardHandler() {
        husk_ga_set_keyboard_handler { action in
            DispatchQueue.main.async {
                guard let view = TLUnityUIView.cocosView else { return }
                if action == 1 {
                    var buf = [CChar](repeating: 0, count: 16384)
                    husk_ga_text(&buf, UInt(buf.count))
                    view.setTyped(String(cString: buf))
                    view.openedByHand = false
                    view.becomeFirstResponder()
                } else {
                    view.resignFirstResponder()
                }
            }
        }
    }

    /// Whether the Unity field that asked for the keyboard takes new lines (Return types one instead of finishing).
    private var unityMultiline = false

    /// A Unity game's requests (TouchScreenKeyboard.Open from an InputField, on its own thread): 1 shows the keyboard, 2 hides it.
    static func installUnityKeyboardHandler() {
        husk_unity_set_keyboard_handler { action, kind in
            DispatchQueue.main.async {
                guard let view = TLUnityUIView.cocosView, view.engine == .unity else { return }
                if action == 1 {
                    // TouchScreenKeyboardType
                    switch kind & 0xFF {
                    case 2: view.keyboardType = .numbersAndPunctuation
                    case 3: view.keyboardType = .URL
                    case 4: view.keyboardType = .numberPad
                    case 5: view.keyboardType = .phonePad
                    case 6: view.keyboardType = .namePhonePad
                    case 7: view.keyboardType = .emailAddress
                    case 8: view.keyboardType = .twitter
                    case 9: view.keyboardType = .webSearch
                    case 10: view.keyboardType = .decimalPad
                    case 1: view.keyboardType = .asciiCapable
                    default: view.keyboardType = .default
                    }
                    view.isSecureTextEntry = kind & 0x100 != 0
                    view.unityMultiline = kind & 0x200 != 0
                    var buf = [CChar](repeating: 0, count: 4096)
                    husk_unity_text(&buf, UInt(buf.count))
                    view.setTyped(view.isSecureTextEntry ? "" : String(cString: buf))
                    view.openedByHand = false; if view.isFirstResponder { view.reloadInputViews() } else { view.becomeFirstResponder() }
                } else {
                    view.resignFirstResponder()
                }
            }
        }
    }

    override func resignFirstResponder() -> Bool {
        openedByHand = false
        // Put away without Done (the game hid it, or the view went): the field is dismissed, as the dialog's back button does.
        if engine == .unity { husk_unity_finish_text(0) }
        return super.resignFirstResponder()
    }

    /// An SDL game's requests (SDL_StartTextInput / SDL_StopTextInput, from its main thread): 1 shows the keyboard, 2 hides it.
    static func installSDLKeyboardHandler() {
        husk_sdl_set_keyboard_handler { action in
            DispatchQueue.main.async {
                guard let view = TLUnityUIView.cocosView else { return }
                if action == 1 { view.openedByHand = false; view.setTyped(""); view.becomeFirstResponder() } else { view.resignFirstResponder() }
            }
        }
    }

    // MARK: touch

    private func id(for touch: UITouch) -> Int32 {
        let key = ObjectIdentifier(touch)
        if let existing = pointers[key] { return existing }
        var next: Int32 = 0
        while pointers.values.contains(next) { next += 1 }
        pointers[key] = next
        return next
    }

    private func send(_ touches: Set<UITouch>, phase: Int32) {
        for t in touches {
            let p = t.location(in: self)
            let pid = id(for: t)
            husk_unity_touch(phase, pid, Float(p.x * contentScaleFactor), Float(p.y * contentScaleFactor))
            if phase == 2 || phase == 3 { pointers[ObjectIdentifier(t)] = nil }
        }
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?)     { send(touches, phase: 0) }
    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?)     { send(touches, phase: 1) }
    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?)     { send(touches, phase: 2) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) {
        send(touches, phase: 2)
    }
}

private enum APKUpdateError: LocalizedError {
    case message(String)
    var errorDescription: String? {
        switch self {
        case .message(let s): return s
        }
    }
}

private final class APKUpdateDownloader: NSObject, URLSessionDownloadDelegate, @unchecked Sendable {
    private let destURL: URL
    private let onProgress: @Sendable (Int64, Int64) -> Void
    private let lock = NSLock()
    private var continuation: CheckedContinuation<Void, Error>?
    private var task: URLSessionDownloadTask?
    private var cancelled = false
    private var finishError: Error?
    private var movedOK = false
    private var lastReport: CFAbsoluteTime = 0

    private init(destURL: URL, onProgress: @escaping @Sendable (Int64, Int64) -> Void) {
        self.destURL = destURL
        self.onProgress = onProgress
    }

    static func download(
        from url: URL,
        to dest: URL,
        onProgress: @escaping @Sendable (Int64, Int64) -> Void
    ) async throws {
        let request = URLRequest(url: url, cachePolicy: .reloadIgnoringLocalCacheData, timeoutInterval: 300)
        let delegate = APKUpdateDownloader(destURL: dest, onProgress: onProgress)
        let queue = OperationQueue()
        queue.maxConcurrentOperationCount = 1
        let config = URLSessionConfiguration.ephemeral
        config.timeoutIntervalForRequest = 60
        config.timeoutIntervalForResource = 1800
        let session = URLSession(configuration: config, delegate: delegate, delegateQueue: queue)

        try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { (cont: CheckedContinuation<Void, Error>) in
                delegate.start(session: session, request: request, continuation: cont)
            }
        } onCancel: {
            delegate.cancel()
        }
    }

    private func start(
        session: URLSession,
        request: URLRequest,
        continuation: CheckedContinuation<Void, Error>
    ) {
        lock.lock()
        if cancelled {
            lock.unlock()
            session.invalidateAndCancel()
            continuation.resume(throwing: CancellationError())
            return
        }
        let downloadTask = session.downloadTask(with: request)
        self.continuation = continuation
        self.task = downloadTask
        lock.unlock()
        downloadTask.resume()
    }

    private func cancel() {
        lock.lock()
        cancelled = true
        let activeTask = task
        lock.unlock()
        activeTask?.cancel()
    }

    func urlSession(
        _ session: URLSession,
        downloadTask: URLSessionDownloadTask,
        didWriteData bytesWritten: Int64,
        totalBytesWritten: Int64,
        totalBytesExpectedToWrite: Int64
    ) {
        let now = CFAbsoluteTimeGetCurrent()
        if now - lastReport >= 0.2 || (totalBytesExpectedToWrite > 0 && totalBytesWritten >= totalBytesExpectedToWrite) {
            lastReport = now
            onProgress(totalBytesWritten, totalBytesExpectedToWrite)
        }
    }

    func urlSession(
        _ session: URLSession,
        downloadTask: URLSessionDownloadTask,
        didFinishDownloadingTo location: URL
    ) {
        if let http = downloadTask.response as? HTTPURLResponse, !(200...299).contains(http.statusCode) {
            finishError = APKUpdateError.message("Máy chủ trả về mã lỗi HTTP \(http.statusCode).")
            return
        }
        do {
            let fm = FileManager.default
            if fm.fileExists(atPath: destURL.path) {
                try fm.removeItem(at: destURL)
            }
            try fm.moveItem(at: location, to: destURL)
            movedOK = true
        } catch {
            finishError = error
        }
    }

    func urlSession(
        _ session: URLSession,
        task: URLSessionTask,
        didCompleteWithError error: Error?
    ) {
        session.finishTasksAndInvalidate()
        lock.lock()
        let cont = continuation
        continuation = nil
        lock.unlock()
        guard let cont else { return }
        if let error {
            cont.resume(throwing: error)
        } else if let finishError {
            cont.resume(throwing: finishError)
        } else if !movedOK {
            cont.resume(throwing: APKUpdateError.message("Không nhận được dữ liệu APK từ máy chủ."))
        } else {
            cont.resume()
        }
    }
}

struct TLUnityScreen: UIViewRepresentable {
    let apk: String
    var extraApks: [String] = []
    let dataDir: String
    var engine: TLNativeEngine = .unity
    var portrait = false
    var scale: CGFloat = 2
    var nativeResolution = false
    var avoidUnsafeArea = true
    var onStats: ((String) -> Void)? = nil
    var onThreeFingerTap: (() -> Void)? = nil
    /// Keep the surface while a game runs or is minimized. Cooperative close
    /// releases it after the native session has drained.
    private static var shared: [String: TLUnityUIView] = [:]
    static func cached(_ apk: String) -> TLUnityUIView? { shared[apk] }
    static func release(_ apk: String) {
        guard let view = shared.removeValue(forKey: apk) else { return }
        view.onStats = nil; view.onThreeFingerTap = nil
        view.removeFromSuperview()
    }

    /// The game's one view, made the first time it is asked for.
    static func view(apk: String, extraApks: [String], dataDir: String, engine: TLNativeEngine, portrait: Bool, scale: CGFloat,
                     nativeResolution: Bool = false) -> TLUnityUIView {
        if let view = shared[apk] { return view }
        let view = TLUnityUIView(apk: apk, extraApks: extraApks, dataDir: dataDir, engine: engine, portrait: portrait, scale: scale,
                                nativeResolution: nativeResolution)
        shared[apk] = view
        return view
    }

    func makeUIView(context: Context) -> TLWindowHost {
        let view = Self.view(apk: apk, extraApks: extraApks, dataDir: dataDir, engine: engine, portrait: portrait, scale: scale,
                             nativeResolution: nativeResolution)
        view.avoidUnsafeArea = avoidUnsafeArea
        view.onStats = onStats
        view.onThreeFingerTap = onThreeFingerTap
        return TLWindowHost(game: view, windowed: false)
    }
    func updateUIView(_ host: TLWindowHost, context: Context) {
        host.game.onStats = onStats; host.game.onThreeFingerTap = onThreeFingerTap
        host.game.avoidUnsafeArea = avoidUnsafeArea
        host.game.setNeedsLayout()
    }
}

/// Close APKs on a worker while the library stays responsive. Retain the cached
/// layer until native threads finish. A failed close prevents the next launch.
@MainActor
final class TLNativeSession: ObservableObject {
    static let shared = TLNativeSession()
    @Published private(set) var stopping = false
    @Published var lastError: String?
    private var pending: Task<Bool, Never>?

    func prepare(for apk: String) async -> Bool {
        if let pending, !(await pending.value) { return false }
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }) else { return true }
        if loaded == apk && husk_unity_state() == Int32(HUSK_UNITY_RUNNING) { return true }
        return await stop()
    }

    func close(_ apk: String) { Task { _ = await stop(only: apk) } }

    func stop(only apk: String? = nil) async -> Bool {
        if let pending { return await pending.value }
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }),
              apk == nil || apk == loaded else { return true }
        guard !loaded.hasPrefix("ps4:"), husk_native_can_stop() else {
            lastError = "This engine still needs Husk closed before another game can start."
            return false
        }
        stopping = true
        lastError = nil
        let task = Task { @MainActor in
            defer { self.pending = nil; self.stopping = false }
            let layerOwner = TLUnityScreen.cached(loaded)
            let ok = await Task.detached(priority: .userInitiated) { husk_native_stop(8000) }.value
            withExtendedLifetime(layerOwner) {}
            if ok {
                husk_java_web_attach(nil, nil, 1, 1)
                TLUnityScreen.release(loaded)
                HuskLog.log("tl", "closed previous APK; ready for another game without restarting Husk")
            } else {
                self.lastError = "The previous game has not finished closing. Wait a moment and try Play again. Its session has been kept to avoid a crash."
            }
            return ok
        }
        pending = task
        return await task.value
    }
}

/// Polls the runtime for the status line and its log, ten times a second at most.
@MainActor
final class TLUnityModel: ObservableObject {
    @Published var state: Int32 = 0
    @Published var frames: UInt = 0
    @Published var logText = ""
    private var timer: Timer?
    private var ticks = 0

    func start() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.poll() }
        }
    }

    func stop() { timer?.invalidate(); timer = nil }

    private func poll() {
        ticks += 1
        state = husk_unity_state()
        frames = husk_unity_frames()
        if ticks % 4 == 0, let c = husk_tl_attempt_log() {
            let text = String(cString: c)
            free(c)
            if text != logText { logText = text }
        }
    }

    var statusText: String {
        switch state {
        case Int32(HUSK_UNITY_STARTING): return "Loading the engine…"
        case Int32(HUSK_UNITY_RUNNING):  return "Running"
        case Int32(HUSK_UNITY_FAILED):   return "Could not start — see the log"
        case Int32(HUSK_UNITY_ENDED):    return "The game exited"
        case Int32(HUSK_UNITY_STOPPING): return "Closing the game…"
        default:                         return "Starting"
        }
    }

    var subStatusText: String {
        switch state {
        case Int32(HUSK_UNITY_RUNNING): return "\(frames) frame(s) drawn · native runtime"
        case Int32(HUSK_UNITY_STARTING): return "Loading libraries and starting the engine"
        default: return "Native runtime"
        }
    }

    var statusColor: Color {
        switch state {
        case Int32(HUSK_UNITY_RUNNING):  return Theme.good
        case Int32(HUSK_UNITY_FAILED):   return .red
        case Int32(HUSK_UNITY_ENDED):    return .orange
        default:                         return Theme.accent
        }
    }
}

/// A cocos2d-x game's screen (Geometry Dash). These are landscape games: the app turns to landscape while this is up and
/// back afterwards, the game takes the whole screen, and a thin bar above it carries what the other runners show -- the
/// status, the frames per second and the time a frame takes -- and, with developer info on, the log beside the game.
struct TLCocosAttemptView: View {
    let app: TLApp
    @Environment(\.dismiss) private var dismiss
    @StateObject private var model = TLUnityModel()
    /// Why the app stopped, when its log says: shown over the screen
    @State private var stoppedWhy: AppDiagnosis?
    @AppStorage("husk.tl.unity.showLog") private var showLogSetting = false
    @AppStorage(TranslationLayer.devInfoKey) private var devInfo = false
    @State private var stats = "starting"
    @ObservedObject private var pads = HuskGamepads.shared
    @StateObject private var virtualPad = VirtualPad()
    /// Where the player has put the pad's controls in this game, and whether they are moving them now.
    @State private var padLayout = PadLayout()
    @State private var editingPad = false
    @State private var padSelected: String?
    /// This game's own settings (TLAppSettings), read once as the screen opens.
    @State private var settings: TLAppSettings
    /// Nothing over the picture. Starts as the game's "Hide the interface" setting says, and three fingers tapped together flip it.
    @State private var uiHidden: Bool
    /// The line saying how to get the interface back, shown for a few seconds after it goes.
    @State private var hint = false
    private var showLog: Bool { get { showLogSetting && devInfo } nonmutating set { showLogSetting = newValue } }

    init(app: TLApp) {
        self.app = app
        let loaded = TLAppSettings.load(app.id, packageName: app.packageName)
        _settings = State(initialValue: loaded)
        _uiHidden = State(initialValue: loaded.cleanView)
    }

    private var engine: TLNativeEngine { app.screenEngine }
    private var dataDir: String { app.nativeDataDir }
    private var portrait: Bool { app.drawsPortrait(settings) }

    /// Whether an on-screen controller may be offered: not when the game's settings say never, not while a real one is connected, and
    /// without "Always" only for Unreal, whose menus answer nothing else.
    private var padOffered: Bool {
        settings.pad != .never && pads.names.isEmpty && (settings.pad == .always || engine == .ue4 || wantsController)
    }

    /// A game that has no touch controls of its own, found once it is running.
    private var wantsController: Bool { model.state == Int32(HUSK_UNITY_RUNNING) && husk_native_wants_controller() }

    private func toggleInterface() {
        withAnimation(.easeInOut(duration: 0.15)) { uiHidden.toggle() }
        if uiHidden { showHint() }
    }

    private func showHint() {
        hint = true
        DispatchQueue.main.asyncAfter(deadline: .now() + 3) { withAnimation(.easeOut(duration: 0.4)) { hint = false } }
    }

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            Group {
                if let apk = app.apks.first {
                    // Keep all render pixels, including behind the camera. The guest positions its HUD from cutout data.
                    TLUnityScreen(apk: apk, extraApks: Array(app.apks.dropFirst()), dataDir: dataDir, engine: engine, portrait: portrait,
                                  scale: settings.resolution.scale, nativeResolution: settings.resolution == .native,
                                  avoidUnsafeArea: settings.avoidUnsafeArea,
                                  onStats: { stats = $0 }, onThreeFingerTap: { toggleInterface() })
                        .background(Color.black)
                        .ignoresSafeArea(.container)
                }
            }
            // Husk's own controls remain in the safe layout independently of the full-screen game background.
            if padOffered, settings.padShown, model.state == Int32(HUSK_UNITY_RUNNING) {
                VirtualPadView(pad: virtualPad, opacity: settings.padOpacity, haptics: settings.haptics,
                               layout: padLayout, editing: editingPad, selected: $padSelected,
                               onChange: { padLayout = $0; padLayout.save(app.id) })
                    .overlay(alignment: .center) { if editingPad { padEditor } }
            }
            if !uiHidden {
                VStack(spacing: 0) {
                    bar
                    if showLog {
                        HStack(spacing: 0) {
                            Spacer(minLength: 0)
                            logPanel.frame(width: 320)
                        }
                        .transition(.move(edge: .trailing).combined(with: .opacity))
                    } else {
                        Spacer(minLength: 0)
                    }
                }
            }
            if hint {
                VStack {
                    Spacer()
                    Text("Tap with three fingers to show the interface")
                        .font(.system(size: 13, weight: .medium))
                        .foregroundStyle(.white)
                        .padding(.horizontal, 14).padding(.vertical, 8)
                        .background(.black.opacity(0.65), in: Capsule())
                        .padding(.bottom, 26)
                }
                .allowsHitTesting(false)
                .transition(.opacity)
            }
        }
        .ignoresSafeArea(.keyboard) // The guest receives IME insets; opening the keyboard must not resize its fixed surface.
        .statusBarHidden(true)
        .huskHideSystemOverlays()
        // Swipes near the edges are the game's.
        .huskDefersSystemGestures(on: .all)
        .onAppear {
            HuskOrientation.set(portrait ? .portrait : .landscape)
            UIApplication.shared.isIdleTimerDisabled = settings.keepAwake
            if uiHidden { showHint() }
            CrashReport.gameStarted(app)
            padLayout = PadLayout.load(app.id)
            model.start()
        }
        // What happened, for the library: ten seconds of frames is a game that plays; a refusal is one that did not start.
        .onChange(of: model.frames) { f in
            if f > 600, model.state == Int32(HUSK_UNITY_RUNNING) { GameStatusStore.shared.record(app.id, .plays) }
        }
        .onChange(of: model.state) { st in
            if st == Int32(HUSK_UNITY_FAILED) || st == Int32(HUSK_UNITY_ENDED), stoppedWhy == nil, let d = AppDiagnosis.ofCurrentRun() {
                stoppedWhy = d
                GameStatusStore.shared.record(app.id, st == Int32(HUSK_UNITY_FAILED) ? .failed : .crashed, diagnosis: d)
            } else if st == Int32(HUSK_UNITY_FAILED) { GameStatusStore.shared.record(app.id, .failed) }
        }
        .onDisappear {
            CrashReport.gameEnded()
            model.stop()
            UIApplication.shared.isIdleTimerDisabled = false
            HuskOrientation.set(HuskOrientation.standard)
            if let apk = app.apks.first { TLNativeSession.shared.close(apk) }
        }
        .onReceive(NotificationCenter.default.publisher(for: TLUnityUIView.appClosedItself)) { _ in dismiss() }
        .overlay { if let d = stoppedWhy { AppStoppedOverlay(app: app, diagnosis: d) { dismiss() } } }
    }

    /// While the pad is being edited: what to do with the control picked, in a small panel in the middle of the screen.
    private var padEditor: some View {
        VStack(spacing: 10) {
            if let id = padSelected {
                Text(PadLayout.name(id)).font(.system(size: 14, weight: .semibold)).foregroundStyle(.white)
                HStack(spacing: 10) {
                    Image(systemName: "minus.magnifyingglass").foregroundStyle(.white.opacity(0.7))
                    Slider(value: Binding(get: { Double(padLayout[id].scale) },
                                          set: { padLayout[id].scale = CGFloat($0) }),
                           in: 0.6...1.8, onEditingChanged: { if !$0 { padLayout.save(app.id) } })
                        .frame(width: 180)
                    Image(systemName: "plus.magnifyingglass").foregroundStyle(.white.opacity(0.7))
                }
                HStack(spacing: 10) {
                    Button(padLayout[id].hidden ? "Show" : "Hide") { padLayout[id].hidden.toggle(); padLayout.save(app.id) }
                    Button("Reset") { padLayout[id] = PadLayout.Adjust(); padLayout.save(app.id) }
                }
                .buttonStyle(.bordered).tint(.white)
            } else {
                Text("Drag a control to move it, or tap one to resize or hide it.")
                    .font(.system(size: 13, weight: .medium)).foregroundStyle(.white)
                    .multilineTextAlignment(.center).frame(maxWidth: 260)
            }
            HStack(spacing: 10) {
                Button("Reset All", role: .destructive) { padLayout = PadLayout(); padSelected = nil; padLayout.save(app.id) }
                Button("Done") { editingPad = false; padSelected = nil }.buttonStyle(.borderedProminent)
            }
            .buttonStyle(.bordered)
        }
        .padding(16)
        .background(.black.opacity(0.75), in: RoundedRectangle(cornerRadius: 16))
        .font(.system(size: 13, weight: .semibold))
    }

    private var bar: some View {
        HStack(spacing: 12) {
            Button { dismiss() } label: {
                Label("Close", systemImage: "xmark").font(.system(size: 13, weight: .semibold))
            }
            .tint(.white)
            if engine == .flutter {
                Button { husk_flutter_back() } label: {
                    Label("Back", systemImage: "chevron.backward").font(.system(size: 13, weight: .semibold))
                }
                .tint(.white)
            }
            Circle().fill(model.statusColor).frame(width: 7, height: 7)
            Text(model.state == Int32(HUSK_UNITY_RUNNING) ? app.label : model.statusText)
                .font(.system(size: 12, weight: .medium)).foregroundStyle(.white.opacity(0.85)).lineLimit(1)
            Spacer()
            if !pads.names.isEmpty {
                Label(pads.names.count == 1 ? pads.names[0] : "\(pads.names.count) controllers", systemImage: "gamecontroller.fill")
                    .font(.system(size: 11, weight: .medium)).foregroundStyle(.white.opacity(0.7)).lineLimit(1)
            }
            if padOffered {
                Button { settings.padShown.toggle(); settings.save(app.id); if !settings.padShown { editingPad = false } } label: {
                    Label(settings.padShown ? "Hide pad" : "Pad", systemImage: "gamecontroller").font(.system(size: 12, weight: .semibold))
                }
                .tint(.white)
                if settings.padShown {
                    Button { padSelected = nil; editingPad.toggle() } label: {
                        Label(editingPad ? "Done" : "Edit pad", systemImage: "slider.horizontal.below.square.and.square.filled")
                            .font(.system(size: 12, weight: .semibold))
                    }
                    .tint(editingPad ? .yellow : .white)
                }
            }
            if TLUnityUIView.cocosView?.offersKeyboardButton == true, model.state == Int32(HUSK_UNITY_RUNNING) {
                Button { TLUnityUIView.cocosView?.toggleKeyboardByHand() } label: {
                    Label("Keyboard", systemImage: "keyboard").font(.system(size: 12, weight: .semibold))
                }
                .tint(.white)
                .accessibilityHint("Shows or hides the keyboard for the app")
            }
            if settings.showStats, model.state == Int32(HUSK_UNITY_RUNNING) {
                Text(stats).font(.technical(11)).foregroundStyle(.white.opacity(0.7)).lineLimit(1)
            }
            if devInfo {
                Button { withAnimation(.snappy(duration: 0.25)) { showLog.toggle() } } label: {
                    Text(showLog ? "Hide log" : "Log").font(.system(size: 12, weight: .semibold))
                }
                .tint(.white)
            }
            Button { toggleInterface() } label: {
                Label("Hide", systemImage: "eye.slash").font(.system(size: 12, weight: .semibold))
            }
            .tint(.white)
        }
        .padding(.horizontal, 14)
        .frame(height: 30)
        .background(Color(white: 0.08))
    }

    private var logPanel: some View {
        VStack(spacing: 0) {
            HStack {
                Text("ATTEMPT LOG").font(.technical(10, weight: .bold)).foregroundStyle(Theme.textDim)
                Spacer()
                Button { UIPasteboard.general.string = model.logText } label: {
                    Label("Copy", systemImage: "doc.on.doc").font(.system(size: 11))
                }
            }
            .padding(.horizontal, 10).padding(.vertical, 6)
            ScrollViewReader { proxy in
                ScrollView {
                    Text(model.logText.isEmpty ? "Starting…" : model.logText)
                        .font(.technical(10))
                        .foregroundStyle(Theme.text)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(8)
                        .textSelection(.enabled)
                        .id("bottom")
                }
                .onChange(of: model.logText) { _ in proxy.scrollTo("bottom", anchor: .bottom) }
            }
        }
        .background(Theme.bg.opacity(0.85))
    }
}

/// How the native runtime shows an app: which engine draws it, where it keeps its data, and which way up it is.
extension TLApp {
    /// Geometry Dash and the like are cocos2d-x; Minecraft is built on GameActivity. Both are landscape.
    var screenEngine: TLNativeEngine {
        switch report?.nativeEngine {
        case .minecraft: return .minecraft
        case .sdl: return .sdl
        case .ue4: return .ue4
        case .gta: return .gta
        case .godot: return .godot
        case .nativeactivity: return .nativeactivity
        case .flutter: return .flutter
        case .gamemaker: return .gamemaker
        case .java: return .java
        case .cocos: return .cocos
        default: return .unity
        }
    }

    var nativeDataDir: String {
        TranslationLayer.root.appendingPathComponent(id, isDirectory: true)
            .appendingPathComponent(Self.dataFolder(screenEngine), isDirectory: true).path
    }

    private static func dataFolder(_ engine: TLNativeEngine) -> String {
        switch engine {
        case .unity: return "unity-data"
        case .minecraft: return "minecraft-data"
        case .sdl: return "sdl-data"
        case .ue4: return "ue4-data"
        case .gta: return "gta-data"
        case .godot: return "godot-data"
        case .nativeactivity: return "na-data"
        case .flutter: return "flutter-data"
        case .gamemaker: return "gm-data"
        case .java: return "java-data"
        case .cocos: return "cocos-data"
        }
    }

    /// Which way up: what the game's settings say, and otherwise what its manifest asks. Some SDL games are portrait; every other
    /// native game is landscape.
    func drawsPortrait(_ settings: TLAppSettings) -> Bool {
        switch settings.orientation {
        case .landscape: return false
        case .portrait: return true
        case .auto:
            // A Flutter app is a phone app: portrait, unless its manifest asks for landscape.
            if screenEngine == .flutter || screenEngine == .java, let apk = apks.first { return husk_apk_orientation(apk) != 0 }
            if screenEngine == .gamemaker, let apk = apks.first {
                let o = husk_gamemaker_orientation(apk)
                return o == 1 || (o < 0 && husk_sdl_apk_is_portrait(apk) != 0)
            }
            guard screenEngine == .sdl || screenEngine == .nativeactivity || screenEngine == .unity || screenEngine == .godot, let apk = apks.first else { return false }
            return husk_sdl_apk_is_portrait(apk) != 0
        }
    }
}
