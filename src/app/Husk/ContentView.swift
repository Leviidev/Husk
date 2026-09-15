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

    /// How Android was started, which decides what the app shows while it runs.
    enum StartMode { case fullScreen, library }
    @State private var mode: StartMode = .fullScreen
    @State private var started = false
    @State private var showLogs = false
    /// True while the guest's own screen is being shown instead of the library.
    /// Starts true because first boot always needs the Android wizard.
    @State private var showGuestScreen = true
    @State private var tab: HuskTab = .android
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        // SwiftUI's own TabView.
        //
        // The earlier hand-rolled bar existed because HuskGLView owns the
        // CAMetalLayer QEMU renders into, and a torn-down layer is a black guest
        // with a frame counter that keeps climbing -- a day was lost to that
        // once. It is safe here for a specific reason: HuskGLScreen hands back
        // HuskGLView.shared, a single instance for the process. UIKit moves a
        // view between parents without recreating it, so whatever SwiftUI does
        // with the tab, the layer QEMU holds stays the layer being composited.
        //
        // Android is first and the default, which also matters: probeGL needs
        // the layer published before QEMU builds its command line, and the first
        // tab is the one that gets laid out at launch.
        TabView(selection: $tab) {
            androidTab
                .tabItem { Label(HuskTab.android.title,
                                 systemImage: HuskTab.android.icon) }
                .tag(HuskTab.android)

            libraryTab
                .tabItem { Label(HuskTab.library.title,
                                 systemImage: HuskTab.library.icon) }
                .tag(HuskTab.library)

            LogView(isSheet: false)
                .tabItem { Label(HuskTab.console.title,
                                 systemImage: HuskTab.console.icon) }
                .tag(HuskTab.console)

            SettingsView(isSheet: false, showLogs: $showLogs)
                .tabItem { Label(HuskTab.settings.title,
                                 systemImage: HuskTab.settings.icon) }
                .tag(HuskTab.settings)
        }
        .tint(Theme.accent)
        .onChange(of: tab) { HuskLog.log("ui", "tab: \($0.rawValue)") }
        .ignoresSafeArea(.keyboard)
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

    /// The guest, or the start screen before it is running.
    ///
    /// Deliberately one tab rather than two: before Android exists there is
    /// nothing to show, and a separate "main" tab would be an empty room for the
    /// rest of the session.
    @ViewBuilder private var androidTab: some View {
        if started && runner.isRunning {
            GuestScreenView(showLogs: $showLogs,
                            chromeHidden: false,
                            onBack: { tab = .library })
        } else {
            SetupView(showLogs: $showLogs) { chosen in
                mode = chosen
                HuskLog.log("ui", "start mode: "
                          + (chosen == .fullScreen ? "full screen" : "library"))
                showGuestScreen = (chosen == .fullScreen)
                start()
                if chosen == .library { tab = .library }
            }
        }
    }

    @ViewBuilder private var libraryTab: some View {
        if started {
            AdbLibraryView(onOpened: { tab = .android }, showLogs: $showLogs)
                .onAppear { AndroidHost.shared.waitForReady() }
        } else {
            notStartedYet("The library talks to Android over the bridge, "
                        + "so it needs Android running.")
        }
    }

    /// Shown by a tab that cannot do anything useful until Android is up.
    private func notStartedYet(_ why: String) -> some View {
        VStack(spacing: 12) {
            Image(systemName: "power").font(.largeTitle).foregroundStyle(.secondary)
            Text("Android is not running").font(.headline)
            Text(why)
                .font(.caption).foregroundStyle(.secondary)
                .multilineTextAlignment(.center).padding(.horizontal, 40)
            Button("Go to Android") { tab = .android }
                .buttonStyle(.borderedProminent).padding(.top, 4)
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
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
            SettingsView(showLogs: $showLogs)
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
private struct AppIcon: View {
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
struct SettingsView: View {
    private func forget(_ mode: String, _ name: String) {
        if QemuRunner.shared.forgetSnapshot(mode: mode) {
            deleteResult = "Deleted the \(name) machine. The next launch boots from cold."
        } else {
            deleteResult = "No \(name) machine is saved, so nothing was deleted."
        }
    }

    /// False when this is a tab rather than a sheet: there is nothing to
    /// dismiss, and a Done button that does nothing is worse than no button.
    var isSheet = true
    @Environment(\.presentationMode) private var presentation
    @Binding var showLogs: Bool

    @ObservedObject private var guest = GuestImage.shared
    @State private var gpuMode =
        // Absent means GPU: bool(forKey:) answers false for a key nobody
        // has set, which quietly made the slow renderer the default.
        UserDefaults.standard.object(forKey: "husk.gpuMode") as? Bool ?? true
    @State private var useSnapshot =
        UserDefaults.standard.object(forKey: "husk.downloadSnapshot") as? Bool ?? true
    @State private var keepNetwork =
        UserDefaults.standard.object(forKey: "husk.keepNetwork") as? Bool ?? true
    @State private var sound =
        UserDefaults.standard.bool(forKey: "husk.sound")
    @State private var soundDevice =
        UserDefaults.standard.object(forKey: "husk.soundDevice") as? Bool ?? true
    @State private var autoSave =
        UserDefaults.standard.object(forKey: "husk.autoSave") as? Bool ?? true
    @State private var appIcon = HuskAppIcon.current
    @State private var landscapeGuest =
        UserDefaults.standard.bool(forKey: "husk.landscapeGuest")
    @Environment(\.colorScheme) private var scheme
    @State private var askWhichToDelete = false
    @State private var deleteResult: String?

    var body: some View {
        NavigationView {
            Form {
                Section {
                    Button(role: .destructive) { askWhichToDelete = true } label: {
                        Label("Delete saved machine", systemImage: "trash")
                    }
                    .disabled(!QemuRunner.shared.hasSnapshot)
                    if let deleteResult {
                        Text(deleteResult).font(.caption2).foregroundColor(.secondary)
                    }
                    Toggle(isOn: $autoSave) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Save automatically")
                            Text(autoSave
                                 ? "Husk saves once Android settles, so later launches "
                                 + "restore in seconds. The picture freezes while it writes."
                                 : "Nothing is saved on its own. Every launch boots from "
                                 + "cold; the Save button in the library still works.")
                                .font(.caption2).foregroundColor(.secondary)
                        }
                    }
                    .onChange(of: autoSave) { v in
                        UserDefaults.standard.set(v, forKey: "husk.autoSave")
                        HuskLog.log("ui", v ? "automatic saving on"
                                            : "automatic saving off")
                    }
                } header: {
                    Text("Saved machine")
                } footer: {
                    Text(QemuRunner.shared.hasSnapshot
                         ? "Husk restores this instead of booting, which takes seconds "
                         + "rather than minutes. Deleting it forces one cold boot, after "
                         + "which a fresh one is saved. Currently saved: "
                         + ((QemuRunner.shared.snapshotDisplay ?? "sw") == "gl"
                            ? "GPU." : "software.")
                         : "Nothing is saved, so Android boots from cold.")
                        .font(.caption2)
                }
                .confirmationDialog("Which saved machine?",
                                    isPresented: $askWhichToDelete,
                                    titleVisibility: .visible) {
                    // Both offered, because which one is on disk is not something
                    // anyone should have to remember -- and deleting the mode you
                    // are not in should never quietly throw away the one you are.
                    Button("GPU machine", role: .destructive) { forget("gl", "GPU") }
                    Button("Software machine", role: .destructive) { forget("sw", "software") }
                    Button("Cancel", role: .cancel) { }
                } message: {
                    Text("Android will boot from cold once, then save a new one.")
                }

                Section {
                    Toggle(isOn: $useSnapshot) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Download pre-booted snapshot")
                            Text(useSnapshot
                                 ? "Adds about 2 GB to the download, and skips the first boot."
                                 : "Smaller download. Android boots from cold the first time.")
                                .font(.caption2).foregroundColor(.secondary)
                            // The shipped snapshot was captured on the software
                            // renderer, and a machine saved under one renderer
                            // cannot restore into the other -- the device model
                            // differs, so QEMU refuses the restore. On GPU it is
                            // two gigabytes that will never be loaded.
                            Label("This snapshot is for CPU only and will not be "
                                + "used on GPU, which cold-boots once and then "
                                + "saves its own.", systemImage: "exclamationmark.triangle.fill")
                                .font(.caption2)
                                .foregroundColor(.orange)
                        }
                    }
                    .onChange(of: useSnapshot) { v in
                        UserDefaults.standard.set(v, forKey: "husk.downloadSnapshot")
                        HuskLog.log("ui", v ? "will fetch the pre-booted snapshot"
                                            : "will boot Android from cold")
                    }
                    if guest.hasShippedSnapshot {
                        Label("Snapshot installed", systemImage: "checkmark.circle.fill")
                            .foregroundColor(.green)
                            .font(.footnote)
                    } else if useSnapshot {
                        Button {
                            // Close settings first: the download reports its
                            // progress on the main screen, where the guest
                            // image download already does.
                            presentation.wrappedValue.dismiss()
                            GuestImage.shared.downloadSnapshotNow()
                        } label: {
                            Label("Download snapshot now (2 GB)",
                                  systemImage: "arrow.down.circle")
                        }
                        .disabled(guest.state != .ready)
                    }
                } header: {
                    Text("First launch")
                } footer: {
                    Text(guest.hasShippedSnapshot
                         ? "Android is already booted. Starting it restores that machine in seconds."
                         : "A snapshot is a machine that has already finished booting. Restoring one takes seconds; booting takes minutes.")
                }

                Section {
                    Toggle(isOn: $keepNetwork) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Keep the network across saves")
                            Text(keepNetwork
                                 ? "Saving closes apps but leaves Android's framework "
                                 + "running, so the network still works after a restore."
                                 : "Saving stops Android's framework too. Clears every "
                                 + "GPU resource, which is steadier — but the network "
                                 + "may not come back until a cold boot.")
                                .font(.caption2).foregroundColor(.secondary)
                        }
                    }
                    .onChange(of: keepNetwork) { v in
                        UserDefaults.standard.set(v, forKey: "husk.keepNetwork")
                        HuskLog.log("ui", v ? "saves will keep the framework running"
                                            : "saves will stop the framework")
                    }
                } header: {
                    Text("Network")
                } footer: {
                    Text("Takes effect on the next save. An existing saved machine "
                       + "keeps whichever behaviour it was saved with.")
                        .font(.caption2)
                }

                Section {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack(spacing: 14) {
                            ForEach(HuskAppIcon.allCases) { icon in
                                Button {
                                    appIcon = icon
                                    HuskAppIcon.apply(icon)
                                } label: {
                                    VStack(spacing: 6) {
                                        if let art = icon.preview(dark: scheme == .dark) {
                                            Image(uiImage: art)
                                                .resizable().scaledToFit()
                                                .frame(width: 58, height: 58)
                                                .clipShape(RoundedRectangle(cornerRadius: 13,
                                                                            style: .continuous))
                                        } else {
                                            RoundedRectangle(cornerRadius: 13, style: .continuous)
                                                .fill(.quaternary)
                                                .frame(width: 58, height: 58)
                                        }
                                        Text(icon.title).font(.caption2)
                                            .lineLimit(1)
                                    }
                                    .overlay(alignment: .topTrailing) {
                                        if appIcon == icon {
                                            Image(systemName: "checkmark.circle.fill")
                                                .font(.caption)
                                                .foregroundStyle(.tint)
                                                .offset(x: 4, y: -4)
                                        }
                                    }
                                }
                                .buttonStyle(.plain)
                            }
                        }
                        .padding(.vertical, 4)
                    }
                } header: {
                    Text("App icon")
                } footer: {
                    Text("Automatic follows the system appearance — light, dark "
                       + "and tinted — and is the only option that changes with it. "
                       + "The others pin one look. iOS shows its own confirmation "
                       + "after a change; that alert cannot be turned off.")
                        .font(.caption2)
                }

                Section {
                    Toggle(isOn: $sound) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Sound (experimental)")
                            Text("Adds a sound device to the machine. While this is "
                               + "on, Android cannot be saved — QEMU refuses to "
                               + "snapshot a machine with a sound device, so every "
                               + "launch boots from cold. Turning it on or off also "
                               + "changes the hardware, costing one cold boot.")
                                .font(.caption2).foregroundColor(.secondary)
                        }
                    }
                    .onChange(of: sound) { v in
                        UserDefaults.standard.set(v, forKey: "husk.sound")
                        HuskLog.log("ui", v ? "sound on; the machine gains a device "
                                            + "and will cold-boot once"
                                            : "sound off; the machine loses a device "
                                            + "and will cold-boot once")
                    }
                    if sound {
                        Toggle(isOn: $soundDevice) {
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Attach the sound device")
                                Text("On is normal. Off runs the audio backend "
                                   + "with no hardware behind it — silent, and "
                                   + "only useful for isolating a fault.")
                                    .font(.caption2).foregroundColor(.secondary)
                            }
                        }
                        .onChange(of: soundDevice) { v in
                            UserDefaults.standard.set(v, forKey: "husk.soundDevice")
                            HuskLog.log("ui", v ? "sound device attached"
                                                : "sound backend only, no device")
                        }
                    }
                } header: {
                    Text("Sound")
                }

                Section {
                    Picker("Guest screen", selection: $landscapeGuest) {
                        Text("Portrait").tag(false)
                        Text("Landscape").tag(true)
                    }
                    .pickerStyle(.segmented)
                    .onChange(of: landscapeGuest) { v in
                        UserDefaults.standard.set(v, forKey: "husk.landscapeGuest")
                        HuskLog.log("ui", v ? "guest panel will be landscape"
                                            : "guest panel will be portrait")
                    }
                } header: {
                    Text("Guest screen")
                } footer: {
                    Text("Android cannot reshape a running screen, so a landscape "
                       + "game on a portrait one gets letterboxed into a band and "
                       + "looks tiny. Creating the screen landscape instead is the "
                       + "only way it can fill it. Changing this costs one cold "
                       + "boot, and portrait apps are letterboxed instead.")
                        .font(.caption2)
                }

                Section {
                    Picker("Renderer", selection: $gpuMode) {
                        Text("GPU").tag(true)
                        Text("CPU").tag(false)
                    }
                    .pickerStyle(.segmented)
                    .onChange(of: gpuMode) { v in
                        UserDefaults.standard.set(v, forKey: "husk.gpuMode")
                        HuskLog.log("ui", v ? "GPU renderer selected"
                                            : "CPU renderer selected")
                    }
                } header: {
                    Text("Display")
                } footer: {
                    // The old text warned that GPU mode could not be snapshotted
                    // and cold-booted every launch. Both stopped being true once
                    // the virgl save worked, and a warning that has gone stale is
                    // worse than none -- it argues for the slower option.
                    Text(gpuMode
                         ? "Android draws on the real GPU through Metal, about four "
                         + "times the frame rate. This is the default."
                         : "Every pixel is drawn by the emulated CPU. Much slower, "
                         + "and only worth choosing if the GPU misbehaves.")
                        .font(.caption2)
                }

                Section {
                    Button {
                        presentation.wrappedValue.dismiss()
                        showLogs = true
                    } label: {
                        Label("View logs", systemImage: "doc.text.magnifyingglass")
                    }
                } header: {
                    Text("Diagnostics")
                }
            }
            .navigationTitle("Settings")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    if isSheet {
                        Button("Done") { presentation.wrappedValue.dismiss() }
                    }
                }
            }
        }
    }
}

/// The app library: Android running out of sight, reached over ADB.
///
/// This is the point of the project. The Android desktop is a means; what a
/// person wants is their APK, installed and opened, with none of the system UI
/// around it. Nothing here is visible until the guest answers on ADB, because
/// until then there is nothing to install into and saying otherwise would be a
/// lie the user pays for in confusion.
struct AdbLibraryView: View {
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var runner = QemuRunner.shared
    let onOpened: () -> Void
    @Binding var showLogs: Bool

    @State private var importing = false
    @State private var sendingFiles = false
    /// Not persisted on purpose: it lives in the guest, and the guest is
    /// restored from a snapshot that may or may not have had it applied.

    var body: some View {
        NavigationView {
            Group {
                if host.isReady { ready } else { waiting }
            }
            .navigationTitle("Library")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .navigationBarLeading) {
                    Button { showLogs = true } label: {
                        Image(systemName: "doc.text.magnifyingglass")
                    }
                }
                ToolbarItem(placement: .navigationBarTrailing) {
                    Menu {
                        Button { importing = true } label: {
                            Label("Install APK(s)", systemImage: "square.and.arrow.down")
                        }
                        Button { sendingFiles = true } label: {
                            Label("Send files to Android", systemImage: "doc.badge.plus")
                        }
                    } label: { Image(systemName: "plus") }
                        .disabled(!host.isReady || host.busy != nil)
                }
            }
            // Multiple selection, for split APK sets. The base APK of a modern
            // game carries no native code; picking it alone fails with "Failed
            // to extract native libraries", which reads like a bad download.
            .fileImporter(isPresented: $importing,
                          allowedContentTypes: [.item],
                          allowsMultipleSelection: true) { result in
                if case .success(let urls) = result, !urls.isEmpty {
                    HuskLog.log("ui", "importing \(urls.count) file(s): "
                              + urls.map(\.lastPathComponent).joined(separator: ", "))
                    host.install(urls)
                }
            }
            .fileImporter(isPresented: $sendingFiles,
                          allowedContentTypes: [.item],
                          allowsMultipleSelection: true) { result in
                if case .success(let urls) = result {
                    HuskLog.log("ui", "sending \(urls.count) file(s) to Android")
                    host.sendFiles(urls)
                }
            }
        }
        .navigationViewStyle(.stack)
    }

    private var waiting: some View {
        VStack(spacing: 14) {
            if QemuRunner.shared.bootProgress > 0 {
                ProgressView(value: Double(QemuRunner.shared.bootProgress), total: 100)
                    .progressViewStyle(.linear)
                    .frame(maxWidth: 240)
            } else {
                ProgressView()
            }
            Text(host.status).font(.callout)
            Text(runner.setupMessage ?? "Android is running in the background.")
                .font(.caption2).foregroundStyle(.secondary)
                .multilineTextAlignment(.center).padding(.horizontal, 40)
            Button("Show Android") { onOpened() }
                .font(.footnote).padding(.top, 6)
        }
    }

    private var ready: some View {
        List {
            if let busy = host.busy {
                Section { HStack { ProgressView(); Text(busy).font(.footnote) } }
            }
            if host.packages.isEmpty {
                Section {
                    VStack(alignment: .leading, spacing: 6) {
                        Text("No apps yet").font(.headline)
                        Text("Add an APK with + and Husk installs it into Android over ADB.")
                            .font(.caption).foregroundStyle(.secondary)
                    }
                    .padding(.vertical, 6)
                }
            } else {
                Section("Installed") {
                    ForEach(host.packages) { pkg in
                        Button {
                            // Open the app first, then show the screen -- so what
                            // appears is the app, not the launcher behind it.
                            host.launch(pkg.name) { onOpened() }
                        } label: {
                            HStack(spacing: 12) {
                                AppIcon(path: pkg.iconPath)
                                VStack(alignment: .leading, spacing: 1) {
                                    Text(pkg.label)
                                    Text(pkg.name).font(.caption2).foregroundStyle(.secondary)
                                }
                            }
                        }
                        .disabled(host.busy != nil)
                    }
                }
            }
            Section {
                Button("Show the Android desktop") { onOpened() }
                    .font(.footnote)
                Button {
                    QemuRunner.shared.saveState(reason: "asked from the library")
                } label: {
                    Label(runner.isSavingState ? "Saving…" : "Save Android state",
                          systemImage: "externaldrive.badge.checkmark")
                        .font(.footnote)
                }
                .disabled(runner.isSavingState || host.busy != nil)
            } footer: {
                Text("Husk restores a saved machine instead of booting it, so anything "
                   + "changed since the last save is dropped. Installing an app saves "
                   + "automatically; sign-ins and Android settings need this button.")
                    .font(.caption2)
            }
        }
    }
}

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
