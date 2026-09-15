// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Routes between the four things Husk can be doing.
///
/// The guest runs continuously once started; these are presentation states, not
/// lifecycle states. In particular `library` and `running` are the same VM --
/// the difference is only whether its surface is on screen.
struct ContentView: View {
    @StateObject private var guest = GuestImage.shared
    @StateObject private var runner = QemuRunner.shared
    @StateObject private var bridge = HuskBridgeFS.shared
    @ObservedObject private var host = AndroidHost.shared

    /// How Android was started, which decides what the app shows while it runs.
    enum StartMode { case fullScreen, library }
    @State private var mode: StartMode = .fullScreen
    @State private var started = false
    @State private var showLogs = false
    /// True while the guest's own screen is being shown instead of the library.
    /// Starts true because first boot always needs the Android wizard.
    @State private var showGuestScreen = false
    @State private var tab: HuskTab = .library
    @State private var showOnboarding = Onboarding.needed
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        ZStack {
            // Two tabs, and the guest on top of them.
            //
            // The guest is not a tab: HuskGLView owns the CAMetalLayer QEMU
            // renders into, and it has to stay in the hierarchy for the whole
            // session -- a torn-down layer is a black picture with a frame
            // counter that keeps climbing. Keeping it mounted and covering it
            // with the tabs is the arrangement that survives that. Showing
            // Android is then a matter of hiding what is over it, which is also
            // why it appears instantly rather than reloading.
            TabView(selection: $tab) {
                LibraryTab(onOpenGuest: { showGuestScreen = true },
                           onStartAndroid: startFromLibrary,
                           started: started && runner.isRunning)
                    .tabItem { Label("Library", systemImage: "square.grid.2x2.fill") }
                    .tag(HuskTab.library)

                SettingsTab()
                    .tabItem { Label("Settings", systemImage: "gearshape.fill") }
                    .tag(HuskTab.settings)
            }
            .opacity(showGuestScreen && started && runner.isRunning ? 0 : 1)

            if started && runner.isRunning {
                GuestScreenView(showLogs: $showLogs,
                                chromeHidden: false,
                                onBack: { showGuestScreen = false })
                    .opacity(showGuestScreen ? 1 : 0)
                    .allowsHitTesting(showGuestScreen)
            }

            if showSetup {
                // Before the guest exists there is nothing to cover, so the
                // start screen sits above the tabs rather than inside one.
                SetupView(showLogs: $showLogs) { chosen in
                    mode = chosen
                    HuskLog.log("ui", "start mode: "
                              + (chosen == .fullScreen ? "full screen" : "library"))
                    showGuestScreen = (chosen == .fullScreen)
                    start()
                }
                .transition(.opacity)
            }
        }
        .tint(Theme.accent)
        .animation(.snappy(duration: 0.22), value: showGuestScreen)
        .fullScreenCover(isPresented: $showOnboarding) {
            OnboardingView {
                showOnboarding = false
                if Onboarding.autoStart, JITBootstrap.isDebuggerAttached { start() }
            }
        }
        .sheet(isPresented: $showLogs) { LogView() }
        // Asking rather than downloading. Two gigabytes over someone's cellular
        // connection is not a decision to make on their behalf.
        .alert(guest.update.title, isPresented: Binding(
                get: { guest.update.isSomething },
                set: { if !$0 { guest.dismissUpdate() } })) {
            Button("Download") { guest.applyUpdate() }
            Button("Not now", role: .cancel) { guest.dismissUpdate() }
        } message: {
            Text(guest.update.detail)
        }
        .onAppear { evaluate() }
        // The two-parameter onChange is iOS 17; this single-parameter form is
        // deprecated there but still works, and is the only one that compiles
        // against the 16.4 deployment target.
        .onChange(of: scenePhase) { phase in
            // StikDebug relaunches Husk after attaching, so returning to the
            // foreground is the moment worth re-checking, not first launch.
            if phase == .active { evaluate() }
        }
    }

    private func evaluate() {
        try? guest.prepareFirmware()
        guest.refresh()
        HuskBridgeFS.shared.prepare()

        // What is installed is compared against the release by digest, not by
        // version name -- see GuestManifest. Deliberately not awaited: it is a
        // network round trip, and nothing on this screen should wait for it.
        Task { await guest.checkForUpdates() }

        // Deliberately does NOT start the guest.
        //
        // It used to: once the image was present and a debugger was attached,
        // Husk went straight into Android with no way to reach any setting
        // first. Booting takes minutes and its cost depends on choices made
        // before it starts -- which display, whether to fetch a pre-booted
        // snapshot -- so it is a decision, not a side effect of launching.
        if !JITBootstrap.isDebuggerAttached {
            HuskLog.log("ui", "no debugger attached; waiting for StikDebug")
        }
    }

    /// Whether the start screen has anything to offer that the library does not.
    ///
    /// On a first run it has everything: the runtime has to be downloaded and
    /// there is no catalogue, so there is literally nothing else to draw. Once
    /// apps have been seen once they are on disk, and covering them with a
    /// black screen holding two buttons throws away the whole point of caching
    /// them -- the library says Android is not running and offers to start it,
    /// which is all the start screen was saying.
    private var showSetup: Bool {
        !started && (guest.state != .ready || host.packages.isEmpty)
    }

    /// Start the guest from the library, without leaving it.
    ///
    /// `start()` returns silently when there is no debugger attached, which as
    /// the action behind a button reads as a button that does nothing, so the
    /// JIT prompt is raised here instead.
    private func startFromLibrary() {
        guard JITBootstrap.isDebuggerAttached else {
            HuskLog.log("ui", "start asked for without JIT; opening StikDebug")
            _ = JITBootstrap.requestAttach()
            return
        }
        start()
    }

    private func start() {
        guard !started else { return }
        guard JITBootstrap.isDebuggerAttached else { return }
        HuskLog.log("ui", "CS_DEBUGGED set; starting QEMU")
        // Take the JIT region at the last moment before QEMU, as well as before
        // the download. Whichever comes first wins; the second call is a no-op.
        //
        // And refuse to continue without it. qemu_init() allocates its
        // translation buffer inside itself and has nowhere to get executable
        // memory from if this failed, so starting anyway is not optimism, it is
        // a guaranteed SIGSEGV a few milliseconds later -- with the log showing
        // gigabytes free, which sends everyone looking at memory.
        // Refuse only when there is genuinely nothing left to try.
        //
        // On a device with TXM the trap-servicing route is the only one, so a
        // failed prewarm means qemu_init() has nowhere to get executable memory
        // and starting it is a guaranteed crash. Without TXM, CS_DEBUGGED alone
        // still buys a MAP_JIT mapping, and QEMU now falls back to it -- so
        // stopping here would refuse to start a guest that would have run.
        if !JITBootstrap.prewarm(), !JITBootstrap.isLive {
            if JITBootstrap.needsTrapServicer {
                HuskLog.log("jit", "refusing to start QEMU: this device needs a "
                                 + "trap servicer and none is answering")
                return
            }
            HuskLog.log("jit", "no dual mapping, but this device has no TXM -- "
                             + "letting QEMU try MAP_JIT instead")
        }
        started = true
        QemuRunner.shared.start()
        // Start probing the bridge now, not when the library happens to be
        // opened. isReady is only ever set here, and under the old screen-based
        // navigation this was called when someone chose library mode -- so with
        // tabs, opening Library after starting in full screen left it waiting
        // forever on a guest that was plainly up. It is idempotent and cheap.
        AndroidHost.shared.waitForReady()
        bridge.startWatching()
        GuestBridge.shared.startHealthWatch()
        if QemuRunner.soundEnabled { HuskAudio.shared.start() }
    }
}

/// The guest's screen, with touch, plus a small status pill.
///
/// This is what the user needs during Android's first-run setup, and it doubles as
/// the honest answer to "is it stuck or is it working?" -- if the guest is drawing,
/// you can see it.
struct GuestScreenView: View {
    @ObservedObject private var runner = QemuRunner.shared
    @Binding var showLogs: Bool
    /// True while another tab is covering this one, so the guest's own controls
    /// are not drawn on top of a settings list.
    var chromeHidden = false
    let onBack: () -> Void
    @State private var keyboard = false
    @State private var rotated = HuskGLView.rotated

    var body: some View {
        ZStack(alignment: .top) {
            Color.black.ignoresSafeArea()
            // The GL surface, not HuskDisplay. ANGLE renders into this layer
            // directly; HuskDisplay uploaded a CPU framebuffer itself, which is
            // the work the GPU path exists to remove. QemuRunner falls back to
            // the software display if GL cannot start, and that path draws
            // nothing here -- a black screen with "GL display FAILED" in the log
            // is the signal, rather than a silent wrong-looking picture.
            // HuskGLView always exists, because it is what publishes the
            // CAMetalLayer that QEMU needs before it can bring GL up. If GL
            // then fails, nothing ever draws into that layer -- so the software
            // display goes on top and takes over. Without this the fallback is
            // invisible: the log says it fell back and the screen stays black.
            HuskGLScreen().ignoresSafeArea()
            // Only once GL is known to have FAILED. While the answer is
            // still undecided this must draw nothing: HuskDisplay is opaque,
            // and putting it over the GL layer on the chance that GL might not
            // work is how the guest ends up hidden behind a view that has
            // nothing to show.
            if runner.displayKind == .software {
                HuskDisplay().ignoresSafeArea()
            }

            // Boot progress, over the guest's own screen.
            //
            // Full screen shows the guest and nothing else, so a cold boot here
            // was several minutes of a mostly black screen with no indication
            // anything was happening -- the two other waiting screens had a bar
            // and this one, the one people actually watch a boot on, did not.
            //
            // Gone the moment the guest is up, and never shown on a restore.
            if runner.bootProgress > 0 && runner.bootProgress < 100 {
                VStack(spacing: 10) {
                    ProgressView(value: Double(runner.bootProgress), total: 100)
                        .progressViewStyle(.linear)
                        .frame(width: 200)
                    Text(runner.setupMessage ?? "Starting Android…")
                        .font(.caption2).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                }
                .padding(.horizontal, 18).padding(.vertical, 14)
                .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 14))
                .frame(maxWidth: 300)
                // Below the Back/keyboard/console pill rather than centred over
                // the guest: the ZStack is top-aligned, so without this the card
                // lands on top of the chrome and hides the way out.
                .padding(.top, 58)
                .transition(.opacity)
            }

            // Zero-sized: it exists only to hold first-responder status, which is
            // what both the on-screen keyboard and hardware key events depend on.
            KeyCapture(active: $keyboard).frame(width: 0, height: 0)

            if keyboard {
                VStack {
                    Spacer()
                    SpecialKeysBar()
                        .padding(.horizontal, 10).padding(.vertical, 8)
                        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
                        .padding(.bottom, 6)
                }
            }

            // Always available, and deliberately only three things.
            //
            // This used to show a spinner and "Android is starting -- complete
            // its setup on screen" until the guest reported ready, and it never
            // did: readiness came from the 9p agent, which no longer exists. So
            // the one control that leaves this screen was hidden behind a
            // condition that is now permanently false, and opening an app was a
            // one-way trip.
            if !chromeHidden {
            // Circular glyph buttons on a dark ground, not a capsule of bare
            // symbols. Over a guest that can be any colour, each control needs
            // its own edge to aim at -- and the rotate button needs a state, so
            // it can show whether the picture is turned rather than only
            // offering to turn it.
            HStack(spacing: 10) {
                // Out of the guest, back to the library.
                //
                // This went missing when Android stopped being a tab: the
                // callback was still wired up and no control called it, so
                // showing the guest while it was still booting was a one-way
                // trip with nothing on screen to leave by.
                GuestControl(systemImage: "chevron.left", action: onBack)

                GuestControl(systemImage: keyboard
                             ? "keyboard.chevron.compact.down" : "keyboard",
                             active: keyboard) {
                    keyboard.toggle()
                    HuskLog.log("kbd", "keyboard \(keyboard ? "shown" : "hidden")")
                }

                // Turn the picture, on purpose.
                //
                // Android will not reshape its panel, so when an app asks for
                // landscape it turns its own composition inside a portrait
                // frame. This turns it back. A button rather than something
                // inferred from the accelerometer, because every attempt to
                // infer it raced either the boot sequence or the phone moving.
                GuestControl(systemImage: "rotate.right", active: rotated) {
                    HuskGLView.rotated.toggle()
                    rotated = HuskGLView.rotated
                }

                // Android's own Home key, over the bridge. Three-button
                // navigation is not drawn in this guest, so without it there is
                // no way out of an app from inside Android.
                GuestControl(systemImage: "house") {
                    DispatchQueue.global(qos: .userInitiated).async {
                        _ = try? GuestBridge.shared.shell(
                            "input keyevent KEYCODE_HOME", timeout: 20)
                        HuskLog.log("ui", "sent HOME to Android")
                    }
                }

                // Saving from here, not only from the library: this is where a
                // session actually happens, and the machine worth keeping is the
                // one you have just been using. The spinner matters -- the save
                // freezes the picture for about fifteen seconds, and without it
                // that reads as a hang.
                GuestControl(systemImage: "externaldrive.badge.checkmark",
                             busy: runner.isSavingState) {
                    QemuRunner.shared.saveState(reason: "asked from full screen")
                }
            }
            .padding(.horizontal, 10).padding(.vertical, 7)
            .background(.ultraThinMaterial, in: Capsule())
            .padding(.top, 6)
            }
        }
        // On the screen rather than the button: the chrome can hide while the
        // document picker is up, and an importer attached to a view that goes
        // away goes away with it.
        .statusBarHidden(true)
    }
}

/// Everything before the library: download the runtime, attach the debugger, wait
/// for Android to come up.
struct SetupView: View {
    @ObservedObject private var guest = GuestImage.shared
    @ObservedObject private var runner = QemuRunner.shared
    @Environment(\.colorScheme) private var scheme
    @Binding var showLogs: Bool
    let onStart: (ContentView.StartMode) -> Void

    @State private var profile: QemuRunner.Profile = .phase1Android
    @State private var showSettings = false

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 22) {
                // The icon the user is actually using, following the system
                // appearance when that choice is Automatic. This used to be a
                // fixed copy of the default artwork, which quietly disagreed
                // with the home screen.
                if let logo = HuskAppIcon.current.preview(dark: scheme == .dark) {
                    Image(uiImage: logo)
                        .resizable().scaledToFit()
                        .frame(width: 96, height: 96)
                        .clipShape(RoundedRectangle(cornerRadius: 22, style: .continuous))
                }
                Text("Husk")
                    .font(.system(size: 44, weight: .semibold, design: .rounded))
                Text("Android app launcher")
                    .font(.footnote).foregroundStyle(.secondary)

                content
            }
            .foregroundStyle(.white)

            // Settings, top right, out of the way of the one thing most people
            // open this screen to press.
            VStack {
                HStack {
                    Spacer()
                    Button { showSettings = true } label: {
                        Image(systemName: "gearshape")
                            .font(.title2)
                            .foregroundStyle(.white.opacity(0.75))
                            .padding(14)
                    }
                }
                Spacer()
            }
        }
        .sheet(isPresented: $showSettings) {
            SettingsTab()
        }
    }

    @ViewBuilder
    private var content: some View {
        if runner.isRunning {
            // Android's first boot is slow under TCG and its one-time setup is
            // slower still, so say what is happening rather than showing a black
            // screen for minutes.
            VStack(spacing: 12) {
                // A determinate bar once the guest has said anything at all.
                // Before that there is nothing to be determinate about, and a
                // bar sitting at zero reads as stuck rather than starting.
                if runner.bootProgress > 0 {
                    ProgressView(value: Double(runner.bootProgress), total: 100)
                        .progressViewStyle(.linear)
                        .frame(maxWidth: 240)
                } else {
                    ProgressView()
                }
                Text(runner.setupMessage.map { "Android: \($0)" } ?? "Starting Android…")
                    .font(.callout).foregroundStyle(.secondary)
                    .multilineTextAlignment(.center).padding(.horizontal, 36)
                Text("First run downloads Android and can take several minutes.")
                    .font(.caption2).foregroundStyle(.tertiary)
                    .multilineTextAlignment(.center).padding(.horizontal, 40)
            }
        } else {
            switch guest.state {
            case .downloading(let p, let received, let total):
                VStack(spacing: 10) {
                    Text(guest.hasShippedSnapshot || GuestImage.shared.isFetchingSnapshot
                         ? "Downloading pre-booted Android"
                         : "Downloading Android runtime").font(.headline)
                    ProgressView(value: p).padding(.horizontal, 50)
                    Text("\(fmt(received)) of \(total > 0 ? fmt(total) : "…")")
                        .font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                    Button("Cancel") { guest.cancel() }.font(.footnote)
                }
            case .installing:
                VStack(spacing: 10) { ProgressView(); Text("Installing…").font(.callout) }
            case .failed(let message):
                VStack(spacing: 10) {
                    Text("Something went wrong").font(.headline).foregroundStyle(.red)
                    Text(message).font(.caption).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center).padding(.horizontal, 34)
                    Button("Try again") { JITBootstrap.prewarm(); guest.download() }.buttonStyle(.borderedProminent)
                }
            case .missing:
                VStack(spacing: 12) {
                    Text("Husk needs its Android runtime — about 760 MB. Android itself is downloaded afterwards by the runtime.")
                        .font(.callout).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center).padding(.horizontal, 36)
                    Button("Download Android runtime") {
                        // Claim the JIT region before the download, not after:
                        // it takes about a minute, and StikDebug will have let
                        // go by the end of it.
                        JITBootstrap.prewarm()
                        guest.download()
                    }
                        .buttonStyle(.borderedProminent)
                }
            case .ready:
                VStack(spacing: 12) {
                    if JITBootstrap.isDebuggerAttached {
                        // The library first: it is the thing Husk is for. Full
                        // screen is the escape hatch for everything the library
                        // cannot express -- settings, the launcher, a wizard.
                        VStack(spacing: 10) {
                            ModeCard(icon: "square.grid.2x2.fill",
                                     title: "App library",
                                     subtitle: "Install APKs and open them straight, "
                                             + "without the Android desktop.",
                                     tint: .blue) { onStart(.library) }

                            ModeCard(icon: "rectangle.inset.filled",
                                     title: "Full screen Android",
                                     subtitle: "The whole desktop, as if it were a "
                                             + "second phone.",
                                     tint: .orange) { onStart(.fullScreen) }
                        }
                        .padding(.horizontal, 24)
                        .padding(.top, 4)
                    } else {
                        Text("Husk needs executable memory, which on iOS only an attached debugger can grant.")
                            .font(.callout).foregroundStyle(.secondary)
                            .multilineTextAlignment(.center).padding(.horizontal, 36)
                        Button("Enable JIT with StikDebug") {
                            _ = JITBootstrap.requestAttach()
                        }
                        .buttonStyle(.borderedProminent)
                    }
                    // Attached and still unable to claim memory is a different
                    // problem from not being attached, and it used to present as
                    // a crash rather than as anything readable.
                    if let why = JITBootstrap.lastFailure {
                        Text(why)
                            .font(.caption).foregroundStyle(.orange)
                            .multilineTextAlignment(.center).padding(.horizontal, 30)
                            .padding(.top, 6)
                    }
                }
            }
        }
    }

    private func fmt(_ bytes: Int64) -> String {
        ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file)
    }
}

/// An app's icon, or the placeholder while it is being fetched.
///
/// Loaded from the file rather than held in memory: icons arrive one at a time
/// over the guest bridge, and a list that redraws when each lands should not
/// also be carrying every decoded bitmap around with it.
struct AppIcon: View {
    let path: String?

    var body: some View {
        Group {
            if let path, let image = UIImage(contentsOfFile: path) {
                Image(uiImage: image)
                    .resizable()
                    .interpolation(.medium)
                    .aspectRatio(contentMode: .fit)
                    // Rounded like a launcher would draw it. Android icons are
                    // square PNGs; nothing else gives them an app-like shape.
                    .clipShape(RoundedRectangle(cornerRadius: 9, style: .continuous))
            } else {
                Image(systemName: "app.dashed")
                    .font(.title3)
                    .foregroundStyle(.secondary)
            }
        }
        .frame(width: 40, height: 40)
    }
}

/// One of the two ways to start Android.
///
/// A card rather than a button because the choice needs a sentence to explain
/// it, and a sentence crammed into a bordered button is what the previous
/// version looked like.
private struct ModeCard: View {
    let icon: String
    let title: String
    let subtitle: String
    let tint: Color
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 14) {
                Image(systemName: icon)
                    .font(.system(size: 19, weight: .semibold))
                    .foregroundStyle(tint)
                    .frame(width: 44, height: 44)
                    .background(tint.opacity(0.16),
                                in: RoundedRectangle(cornerRadius: 12, style: .continuous))

                VStack(alignment: .leading, spacing: 3) {
                    Text(title)
                        .font(.callout.weight(.semibold))
                        .foregroundStyle(.white)
                    Text(subtitle)
                        .font(.caption2)
                        .foregroundStyle(.white.opacity(0.55))
                        .multilineTextAlignment(.leading)
                        // Without this the subtitle is truncated to one line
                        // inside an HStack rather than wrapping.
                        .fixedSize(horizontal: false, vertical: true)
                }

                Spacer(minLength: 4)

                Image(systemName: "chevron.right")
                    .font(.caption.weight(.semibold))
                    .foregroundStyle(.white.opacity(0.28))
            }
            .padding(14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Color.white.opacity(0.07),
                        in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(
                RoundedRectangle(cornerRadius: 16, style: .continuous)
                    .strokeBorder(Color.white.opacity(0.09), lineWidth: 1)
            )
        }
        .buttonStyle(.plain)
    }
}

/// Live log tail with a share button. The share sheet is the practical way to get
/// husk.log and the guest's serial console off the device.
/// Settings, reached from the gear on the start screen.
///
/// These are the choices that have to be made before Android boots, because
/// booting is expensive and each of them changes what that boot costs.
struct LogView: View {
    var isSheet = true
    @Environment(\.dismiss) private var dismiss
    @State private var lines: [String] = []
    @State private var showShare = false
    private let tick = Timer.publish(every: 0.5, on: .main, in: .common).autoconnect()

    var body: some View {
        NavigationStack {
            ScrollViewReader { proxy in
                ScrollView {
                    LazyVStack(alignment: .leading, spacing: 1) {
                        ForEach(Array(lines.enumerated()), id: \.offset) { i, line in
                            Text(line)
                                .font(.system(size: 9, design: .monospaced))
                                .textSelection(.enabled)
                                .foregroundStyle(color(for: line))
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .id(i)
                        }
                    }
                    .padding(.horizontal, 8)
                }
                .onReceive(tick) { _ in
                    lines = HuskLog.recentLines(800)
                    if let last = lines.indices.last {
                        proxy.scrollTo(last, anchor: .bottom)
                    }
                }
            }
            .navigationTitle("Logs")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    if isSheet {
                        Button("Done") { dismiss() }
                    }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button { showShare = true } label: { Image(systemName: "square.and.arrow.up") }
                }
            }
            .sheet(isPresented: $showShare) {
                ShareSheet(items: [
                    HuskLog.logFileURL,
                    URL(fileURLWithPath: QemuRunner.shared.guestSerialLogPath),
                    // QEMU's own output. Missing from this list until now, which
                    // is precisely why a crash could not be diagnosed from a
                    // shared log.
                    HuskLog.nativeLogURL,
                    HuskLog.previousNativeLogURL,
                ])
            }
        }
    }

    /// Colour by source so the JIT path stands out from QEMU's own chatter.
    private func color(for line: String) -> Color {
        if line.contains("FAIL") || line.contains("FATAL") || line.contains("error") {
            return .red
        }
        if line.contains("[husk-jit]") { return .green }
        if line.contains("[husk-dpy]") { return .cyan }
        if line.contains("[guest]")    { return .yellow }
        return .primary
    }
}

struct ShareSheet: UIViewControllerRepresentable {
    let items: [Any]
    func makeUIViewController(context: Context) -> UIActivityViewController {
        UIActivityViewController(activityItems: items, applicationActivities: nil)
    }
    func updateUIViewController(_ vc: UIActivityViewController, context: Context) {}
}
