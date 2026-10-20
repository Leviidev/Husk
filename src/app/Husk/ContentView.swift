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

    @State private var started = false
    /// Asking before full Android starts: it is the experimental part of Husk
    @State private var confirmAndroid = false
    @State private var showLogs = false
    /// True while the guest's own screen is being shown instead of the library.
    /// Starts false: the tab UI is the home screen, and the guest -- including
    /// first boot's Android wizard, which runs underneath it -- is shown on
    /// demand from the library.
    @State private var showGuestScreen = false
    @ObservedObject private var router = Router.shared
    @ObservedObject private var jit = JITCoordinator.shared
    @State private var showOnboarding = Onboarding.needed
    /// True while the launch boot screen is up, rather than the library.
    @State private var booting = false
    @Environment(\.scenePhase) private var scenePhase
    @AppStorage(Theme.Appearance.key) private var appearance = Theme.Appearance.system
    @ObservedObject private var theme = AppTheme.shared
    @ObservedObject private var incoming = IncomingFiles.shared
    @ObservedObject private var showcase = ShowcaseStore.shared
    @ObservedObject private var tlStore = TranslationLayerStore.shared
    @ObservedObject private var crash = CrashReport.shared
    @ObservedObject private var massTest = MassTest.shared
    @ObservedObject private var updates = AppUpdates.shared
    @State private var showWhatsNew = false
    @AppStorage(Holiday.key) private var holiday = true

    var body: some View {
        ZStack {
            // The tabs, and the guest on top of them.
            //
            // The guest is not a tab: HuskGLView owns the CAMetalLayer QEMU
            // renders into, and it has to stay in the hierarchy for the whole
            // session -- a torn-down layer is a black picture with a frame
            // counter that keeps climbing. Keeping it mounted and covering it
            // with the tabs is the arrangement that survives that. Showing
            // Android is then a matter of hiding what is over it, which is also
            // why it appears instantly rather than reloading.
            TabView(selection: tabSelection) {
                HomeView()
                    .tabItem { Label(L10n.tr("Home"), systemImage: "house.fill") }
                    .tag(HuskTab.home)

                LibraryScreen()
                    .tabItem { Label(L10n.tr("Library"), systemImage: "square.grid.2x2.fill") }
                    .tag(HuskTab.library)

                StoreView()
                    .tabItem { Label(L10n.tr("Store"), systemImage: "cart.fill") }
                    .tag(HuskTab.store)

                SettingsTab()
                    .tabItem { Label(L10n.tr("Settings"), systemImage: "gearshape.fill") }
                    .tag(HuskTab.settings)
            }
            .opacity(showGuestScreen && started && runner.isRunning ? 0 : 1)

            // Christmas: snow over the tabs (never over Android or an app's window, which come after it)
            if holiday && Holiday.inSeason && !(showGuestScreen && started && runner.isRunning) {
                Snowfall().ignoresSafeArea()
            }

            // Multitasking: an app in a window over everything else in Husk (AppWindows.swift).
            AppWindowLayer()
                .ignoresSafeArea(.keyboard)

            // Outcomes, over whichever tab is showing. Above the tab bar rather
            // than over it: a message that covers the way out of the screen it
            // appears on is a message in the way.
            if let toast = host.toast {
                VStack {
                    Spacer()
                    ToastView(toast: toast) { host.toast = nil }
                        .padding(.horizontal, 16)
                        .padding(.bottom, 8)
                        .transition(.move(edge: .bottom).combined(with: .opacity))
                }
                .task(id: toast.id) {
                    try? await Task.sleep(nanoseconds: 4_500_000_000)
                    withAnimation(.snappy) {
                        if host.toast?.id == toast.id { host.toast = nil }
                    }
                }
            }

            if started && runner.isRunning {
                GuestScreenView(showLogs: $showLogs,
                                chromeHidden: false,
                                onBack: { showGuestScreen = false })
                    .opacity(showGuestScreen ? 1 : 0)
                    .allowsHitTesting(showGuestScreen)
            }

            // Over the tabs and over the start screen: when Husk is starting
            // Android for you, that is the whole screen until it is done.
            if booting {
                BootScreen { withAnimation(.easeInOut(duration: 0.3)) { booting = false } }
                    .transition(.opacity)
                    .zIndex(3)
                    .onChange(of: host.isReady) { ready in
                        if ready { withAnimation(.easeInOut(duration: 0.45)) { booting = false } }
                    }
            }

        }
        .tint(theme.accentColor)
        // The user's appearance: the system's, unless they pinned one. Set on the
        // window rather than with .preferredColorScheme — see Theme.apply.
        .onAppear { Theme.apply(appearance) }
        .onChange(of: appearance) { Theme.apply($0) }
        .animation(.snappy(duration: 0.22), value: showGuestScreen)
        .animation(.snappy(duration: 0.25), value: host.toast)
        .fullScreenCover(isPresented: $showOnboarding) {
            OnboardingView {
                showOnboarding = false
                if Onboarding.autoStart, JITBootstrap.ready {
                    booting = true
                    start()
                }
            }
        }
        .sheet(isPresented: $showLogs) { LogView() }
        .alert(L10n.tr("Start Android?"), isPresented: $confirmAndroid) {
            Button(L10n.tr("Start")) { startFromLibrary() }
            Button(L10n.tr("Cancel"), role: .cancel) {}
        } message: {
            Text(L10n.tr("Full Android emulation is experimental. It is slow, takes minutes to start, and can freeze or crash Husk. Games and apps run much better in the Translation Layer."))
        }
        // A game that ended Husk last time: say so, with its report.
        .sheet(item: Binding(get: { showOnboarding ? nil : crash.pending }, set: { crash.pending = $0 }),
               onDismiss: { if WhatsNew.due { showWhatsNew = true } }) { r in
            CrashReportSheet(report: r)
        }
        // After an update: what it brought, once.
        .fullScreenCover(isPresented: $showWhatsNew) {
            WhatsNewSheet { WhatsNew.markSeen(); showWhatsNew = false }
        }
        // A newer Husk is on GitHub.
        .alert(L10n.tr("Husk \(updates.available?.version ?? "") Is Available"), isPresented: Binding(
                get: { updates.available != nil && !showOnboarding && !showWhatsNew && crash.pending == nil },
                set: { if !$0 { updates.dismiss() } })) {
            Button(L10n.tr("View Release")) {
                if let page = updates.available?.page { UIApplication.shared.open(page) }
                updates.dismiss()
            }
            Button(L10n.tr("Not Now"), role: .cancel) { updates.dismiss() }
        } message: {
            Text(L10n.tr("You have \(AppUpdates.current). The new version's IPA is on its release page."))
        }
        .task { await updates.check() }
        .task { await CompatibilityStore.shared.refresh() }
        .sheet(isPresented: $router.showFiles) { FilesTab() }
        .sheet(isPresented: $router.showDownloads) { DownloadsTab() }
        // Pictures for the apps here: read the index on launch and whenever the set of apps changes.
        .task(id: showcasePackages) { showcase.refresh(for: showcasePackages) }
        .alert(L10n.tr("Metadata Updated"), isPresented: Binding(get: { showcase.shouldAsk && !showOnboarding },
                                                         set: { if !$0 { showcase.asked() } })) {
            Button(L10n.tr("Download")) { showcase.downloadUpdates() }
            Button(L10n.tr("Not Now"), role: .cancel) { showcase.asked() }
        } message: {
            let n = showcase.updates.count
            Text(L10n.tr("Metadata for \(n) of your app\(n == 1 ? "" : "s") has been updated. Would you like to download the updates?"))
        }
        // An APK shared to Husk: where does it go?
        .sheet(isPresented: Binding(get: { !incoming.waiting.isEmpty },
                                    set: { if !$0 { incoming.discard() } })) {
            IncomingChooser()
                .huskDetents([.medium], dragIndicator: true)
        }
        // APKs meant for Android wait until it can take them.
        .onChange(of: host.isReady) { ready in
            incoming.installForAndroidIfReady()
            // An app asked for while Android was off: open it now.
            if ready, let pkg = router.pendingAndroidLaunch {
                router.pendingAndroidLaunch = nil
                host.launch(pkg) { showGuestScreen = true }
            }
        }
        .onChange(of: host.busy) { _ in incoming.installForAndroidIfReady() }
        .sheet(isPresented: $jit.showSetup) { JITSetupFlow() }
        // The built-in helper attaches while Husk stays in the foreground, so
        // there is no relaunch to trigger the region claim below; this is it.
        .onChange(of: jit.attachGeneration) { _ in evaluate() }
        // Asking rather than downloading. Two gigabytes over someone's cellular
        // connection is not a decision to make on their behalf.
        .alert(guest.update.title, isPresented: Binding(
                get: { guest.update.isSomething },
                set: { if !$0 { guest.dismissUpdate() } })) {
            Button(L10n.tr("Download")) { guest.applyUpdate() }
            Button(L10n.tr("Not now"), role: .cancel) { guest.dismissUpdate() }
        } message: {
            Text(guest.update.detail)
        }
        .sheet(isPresented: $massTest.shareResults) { ShareSheet(items: [MassTest.resultsURL]) }
        .onAppear {
            crash.checkPreviousRun()
            // the developer mass test: the last app's fate, then the next app (it clears a crash report it has written down)
            massTest.resume(crash: crash)
            if crash.pending == nil, !massTest.active { router.resumeSwitch() }
            if crash.pending == nil, WhatsNew.due { showWhatsNew = true }
            router.openGuest = { showGuestScreen = true }
            router.startAndroid = { confirmAndroid = true }
            evaluate()
        }
        // The two-parameter onChange is iOS 17; this single-parameter form is
        // deprecated there but still works, and is the only one that compiles
        // against the 16.0 deployment target.
        .onChange(of: scenePhase) { phase in
            // StikDebug relaunches Husk after attaching, so returning to the
            // foreground is the moment worth re-checking, not first launch.
            if phase == .active { evaluate() }
        }
    }

    /// The selected tab. Choosing the tab already shown takes Home or the Library back to its top, as a tab bar does.
    private var tabSelection: Binding<HuskTab> {
        Binding(get: { router.tab }, set: { tab in
            if tab == router.tab {
                switch tab {
                case .home: router.home = []
                case .library: router.library = []
                default: break
                }
            }
            router.tab = tab
        })
    }

    /// Every package Husk holds, from both sides: what the picture index is read for.
    private var showcasePackages: Set<String> {
        Set(tlStore.apps.compactMap(\.packageName) + host.packages.map(\.name))
    }

    private func evaluate() {
        // Before anything else wants JIT: the setting that turns it on as Husk opens.
        jit.enableAtLaunchIfAsked()
        try? guest.prepareFirmware()
        guest.refresh()
        if AutoStart.readyAtLaunch == nil { AutoStart.readyAtLaunch = guest.state == .ready }
        HuskBridgeFS.shared.prepare()

        // What is installed is compared against the release by digest, not by
        // version name -- see GuestManifest. Deliberately not awaited: it is a
        // network round trip, and nothing on this screen should wait for it.
        Task { await guest.checkForUpdates() }

        if !JITBootstrap.ready {
            HuskLog.log("ui", "no JIT yet; waiting for a debugger")
            return
        }

        // Claim the JIT region now, on the first foreground pass after the debugger
        // attaches, while it is still running. Waiting for the Start button means
        // iOS has often suspended the debugger, causing unserviced brk freezes.
        // After the first call this is a no-op, and on success it also detaches the debugger.
        JITBootstrap.prewarm()

        // Start on launch, when that is what was asked for.
        //
        // This deliberately did nothing for a long time, and the reason was
        // sound: booting takes minutes and its cost depends on choices made
        // before it starts, so it should not be a side effect of opening the
        // app. But the setup flow now asks the question outright, and someone
        // who answered yes has made the decision -- continuing to ignore it
        // just means every launch begins by pressing Start.
        //
        // Only when Android was already installed as Husk opened (#75, #128): evaluate() runs on every return to the foreground,
        // so an image that finished downloading in this run would otherwise start the moment Husk came back -- with a debugger
        // that iOS may have suspended hours into the download, whose unanswered trap ends Husk. And not again after a start
        // that Husk did not survive, or every launch would end the same way.
        guard Onboarding.autoStart, !started, guest.state == .ready,
              !showOnboarding else { return }
        guard AutoStart.readyAtLaunch == true else {
            HuskLog.log("ui", "Android was installed in this run; not starting it on its own (press Start)")
            return
        }
        guard !AutoStart.lastOneDied() else {
            HuskLog.log("ui", "Husk closed while starting Android on launch last time; not starting it on its own this time")
            return
        }
        HuskLog.log("ui", "starting Android on launch")
        booting = true
        AutoStart.begin()
        start()
        if !started { AutoStart.end() }
    }

    /// Start the guest from the library, without leaving it.
    ///
    /// `start()` returns silently when there is no debugger attached, which as
    /// the action behind a button reads as a button that does nothing, so the
    /// JIT prompt is raised here instead.
    private func startFromLibrary() {
        guard JITBootstrap.ready else {
            HuskLog.log("ui", "start asked for without JIT; enabling with \(jit.resolvedMethod.title)")
            jit.enable()
            return
        }
        start()
    }

    private func start() {
        guard !started else { return }
        guard JITBootstrap.ready else { return }
        HuskLog.log("ui", "JIT is available; starting QEMU")
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
        // Two routes, and the second one is not a consolation prize: with
        // CS_DEBUGGED set the kernel honours a plain MAP_JIT mapping, which is
        // what QEMU falls back to on its own and what every other iOS emulator
        // runs on. Stopping here is only right when neither route exists.
        //
        // Which route is available is now measured. It used to be predicted
        // from the device model and the iOS version, and the prediction was
        // wrong for iOS 26: StikDebug attaches there without servicing traps,
        // because on that OS it does not need to, and Husk read that as "no
        // executable memory" and refused to start a guest that would have run.
        if !JITBootstrap.prewarm(), !JITBootstrap.isLive {
            guard JITBootstrap.mapJITWorks else {
                HuskLog.log("jit", "refusing to start QEMU: no trap servicer is "
                                 + "answering and MAP_JIT does not execute here")
                // Said on screen too: a Start button that silently does nothing reads as a hang (#122).
                host.say(L10n.tr("Android can't start without JIT"),
                         JITBootstrap.lastFailure ?? L10n.tr("Husk could not get executable memory. Turn JIT on again (Settings > JIT) and retry."),
                         good: false)
                booting = false
                return
            }
            HuskLog.log("jit", "no dual mapping, but MAP_JIT executes -- letting "
                             + "QEMU map its own buffer")
        }
        // A PS4 game started in this run holds the same JIT region QEMU would take.
        if let console = husk_console_loaded_game() {
            HuskLog.log("jit", "refusing to start QEMU: \(String(cString: console)) is loaded in this run; close Husk first")
            return
        }
        started = true
        router.androidStarted = true
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
    @State private var showControls = false

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
                    Text(runner.setupMessage ?? L10n.tr("Starting Android…"))
                        .font(.caption2).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                }
                .padding(.horizontal, 18).padding(.vertical, 14)
                .huskPanel(RoundedRectangle(cornerRadius: 14, style: .continuous))
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

            // The controls, as one pill at the bottom.
            //
            // They used to be a row of circles pinned to the top left, which is
            // where Android draws its own status bar and where a game puts its
            // score. At the bottom they are where a thumb already rests, and
            // there are three of them: everything rarer lives behind the last
            // one rather than adding another circle over someone's game.
            if !chromeHidden {
                VStack(spacing: 8) {
                    Spacer()

                    if keyboard {
                        SpecialKeysBar()
                            .padding(.horizontal, 10).padding(.vertical, 8)
                            .huskPanel(RoundedRectangle(cornerRadius: 14,
                                                        style: .continuous))
                    }

                    HStack(spacing: 2) {
                        GuestControl(systemImage: "gamecontroller",
                                     active: showControls) { showControls = true }

                        GuestControl(systemImage: keyboard
                                     ? "keyboard.chevron.compact.down" : "keyboard",
                                     active: keyboard) {
                            keyboard.toggle()
                            HuskLog.log("kbd", "keyboard \(keyboard ? "shown" : "hidden")")
                        }

                        Menu {
                            Button { onBack() } label: {
                                Label(L10n.tr("Back to Husk"), systemImage: "chevron.left")
                            }
                            // Android's own Home key, over the bridge. Three-button
                            // navigation is not drawn in this guest, so without it
                            // there is no way out of an app from inside Android.
                            Button {
                                DispatchQueue.global(qos: .userInitiated).async {
                                    _ = try? GuestBridge.shared.shell(
                                        "input keyevent KEYCODE_HOME", timeout: 20)
                                    HuskLog.log("ui", "sent HOME to Android")
                                }
                            } label: { Label(L10n.tr("Home"), systemImage: "house") }
                            // Android will not reshape its panel, so when an app
                            // asks for landscape it turns its own composition
                            // inside a portrait frame. This turns it back.
                            Button {
                                HuskGLView.rotated.toggle()
                                rotated = HuskGLView.rotated
                            } label: {
                                Label(rotated ? L10n.tr("Unrotate picture") : L10n.tr("Rotate picture"),
                                      systemImage: "rotate.right")
                            }
                            Divider()
                            Button {
                                QemuRunner.shared.saveState(reason: "asked from full screen")
                            } label: {
                                Label(runner.isSavingState ? L10n.tr("Saving…") : L10n.tr("Save Android"),
                                      systemImage: "externaldrive.badge.checkmark")
                            }
                            .disabled(runner.isSavingState)
                            Button { showLogs = true } label: {
                                Label(L10n.tr("Console"), systemImage: "terminal")
                            }
                        } label: {
                            Image(systemName: "ellipsis")
                                .font(.system(size: 17, weight: .medium))
                                .foregroundStyle(.white)
                                .frame(width: 46, height: 42)
                                .contentShape(Rectangle())
                        }
                    }
                    .padding(.horizontal, 6).padding(.vertical, 3)
                    .huskPanel(Capsule())
                    .padding(.bottom, 8)
                }
            }
        }
        // The guest's screen is dark in every appearance: it is a picture of
        // another phone, mostly black, and its chrome is drawn to sit on that.
        .environment(\.colorScheme, .dark)
        // On the screen rather than the button: the chrome can hide while the
        // document picker is up, and an importer attached to a view that goes
        // away goes away with it.
        .statusBarHidden(true)
        .huskHideSystemOverlays()
        .sheet(isPresented: $showControls) {
            ControlsSheet(keyboard: $keyboard)
                .huskDetents([.height(300)])
        }
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
            .navigationTitle(L10n.tr("Logs"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    if isSheet {
                        Button(L10n.tr("Done")) { dismiss() }
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

/// What is driving the guest, and what could be.
///
/// Touch is the only input Husk actually delivers today; the keyboard is real
/// but optional, and a gamepad and a pointer are not built yet. They are listed
/// anyway, dimmed: a control surface that hides what it cannot do leaves you
/// wondering whether you simply cannot find it.
struct ControlsSheet: View {
    @Binding var keyboard: Bool
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ZStack {
            Theme.backdrop
            VStack(spacing: 16) {
                HStack {
                    Text(L10n.tr("Controls"))
                        .font(.system(size: 20, weight: .semibold))
                        .foregroundStyle(Theme.text)
                    Spacer()
                    Button { dismiss() } label: {
                        Image(systemName: "xmark")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(Theme.textDim)
                            .frame(width: 30, height: 30)
                            .background(Theme.surfaceHigh, in: Circle())
                    }
                    .buttonStyle(.plain)
                }

                RowGroup {
                    row("hand.tap.fill", L10n.tr("Touch"), on: true, available: true)
                    RowDivider()
                    Button {
                        keyboard.toggle()
                        HuskLog.log("kbd", "keyboard \(keyboard ? "shown" : "hidden")")
                        dismiss()
                    } label: {
                        row("keyboard", L10n.tr("Keyboard"), on: keyboard, available: true)
                    }
                    .buttonStyle(.plain)
                    RowDivider()
                    row("gamecontroller", L10n.tr("Gamepad"), on: false, available: false)
                    RowDivider()
                    row("computermouse", L10n.tr("Mouse"), on: false, available: false)
                }

                Text(L10n.tr("Touch always works. A gamepad and a pointer are not wired through to Android yet."))
                    .font(.system(size: 12))
                    .foregroundStyle(Theme.textDim)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 12)

                Spacer(minLength: 0)
            }
            .padding(.horizontal, 20).padding(.top, 18)
        }
    }

    private func row(_ icon: String, _ title: String,
                     on: Bool, available: Bool) -> some View {
        HStack(spacing: 14) {
            Image(systemName: icon)
                .font(.system(size: 16, weight: .medium))
                .foregroundStyle(available ? Theme.text : Theme.textDim.opacity(0.6))
                .frame(width: 34, height: 34)
                .background(Theme.surfaceHigh,
                            in: RoundedRectangle(cornerRadius: 10, style: .continuous))
            Text(title)
                .font(.system(size: 15, weight: .medium))
                .foregroundStyle(available ? Theme.text : Theme.textDim.opacity(0.6))
            Spacer()
            if on {
                Image(systemName: "checkmark")
                    .font(.system(size: 14, weight: .semibold))
                    .foregroundStyle(Theme.accent)
            } else if !available {
                Text(L10n.tr("Not yet")).font(.system(size: 12)).foregroundStyle(Theme.textDim)
            }
        }
        .padding(.horizontal, 14).padding(.vertical, 12)
        .contentShape(Rectangle())
    }
}

/// Starting Android on launch, made safe to repeat: whether the image was there as Husk opened, and a marker that is only on
/// disk while an automatic start is under way, so a start Husk did not survive is not tried again on the next launch.
enum AutoStart {
    /// Whether the guest was ready on the first look in this process; nil before it.
    nonisolated(unsafe) static var readyAtLaunch: Bool?

    private static var marker: String {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("husk-autostart-pending").path
    }

    /// True once per crash: the marker of an automatic start that never finished is read and removed.
    static func lastOneDied() -> Bool {
        guard FileManager.default.fileExists(atPath: marker) else { return false }
        try? FileManager.default.removeItem(atPath: marker)
        return true
    }

    /// Marks an automatic start; the mark goes once QEMU has been running for a minute.
    static func begin() {
        FileManager.default.createFile(atPath: marker, contents: Data())
        DispatchQueue.main.asyncAfter(deadline: .now() + 60) { end() }
    }

    static func end() { try? FileManager.default.removeItem(atPath: marker) }
}
