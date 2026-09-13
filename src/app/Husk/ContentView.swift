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

    @State private var started = false
    @State private var runningApp: HuskBridgeFS.AndroidApp?
    @State private var showLogs = false
    /// True while the guest's own screen is being shown instead of the library.
    /// Starts true because first boot always needs the Android wizard.
    @State private var showGuestScreen = true
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        Group {
            if let app = runningApp {
                RunningAppView(app: app) {
                    HuskLog.log("ui", "returning to library from \(app.package)")
                    runningApp = nil
                }
            } else if started && androidReady && !showGuestScreen {
                LibraryView(running: $runningApp)
            } else if started && runner.isRunning {
                // Android's own first-run wizard has to be completed by hand, and
                // LineageOS will not finish booting until it is. Hiding the guest
                // behind a spinner makes that look like a hang: it is drawing
                // continuously, just waiting for a human who cannot see or touch it.
                GuestScreenView(showLogs: $showLogs,
                                canReturnToLibrary: androidReady,
                                onLibrary: { showGuestScreen = false })
            } else {
                SetupView(showLogs: $showLogs, onStart: start)
            }
        }
        .sheet(isPresented: $showLogs) { LogView() }
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

    /// The catalogue only appears once the guest agent is running AND Waydroid
    /// reports a live session, so its presence is a good proxy for "Android is up".
    private var androidReady: Bool {
        !bridge.apps.isEmpty || bridge.lastAgentMessage != nil
    }

    private func evaluate() {
        try? guest.prepareFirmware()
        guest.refresh()
        HuskBridgeFS.shared.prepare()

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

    private func start() {
        guard !started else { return }
        guard JITBootstrap.isDebuggerAttached else { return }
        HuskLog.log("ui", "CS_DEBUGGED set; starting QEMU")
        // Take the JIT region at the last moment before QEMU, as well as before
        // the download. Whichever comes first wins; the second call is a no-op.
        JITBootstrap.prewarm()
        started = true
        QemuRunner.shared.start()
        bridge.startWatching()
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
    let canReturnToLibrary: Bool
    let onLibrary: () -> Void
    @State private var keyboard = false

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
            if !runner.glDisplayActive {
                HuskDisplay().ignoresSafeArea()
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

            HStack(spacing: 10) {
                if canReturnToLibrary {
                    Button(action: onLibrary) {
                        Label("Library", systemImage: "square.grid.2x2")
                            .font(.caption.weight(.medium))
                    }
                } else {
                    ProgressView().controlSize(.small)
                    Text(runner.setupMessage ?? "Android is starting — complete its setup on screen")
                        .font(.caption2)
                        .lineLimit(2)
                }
                Button {
                    keyboard.toggle()
                    HuskLog.log("kbd", "keyboard \(keyboard ? "shown" : "hidden")")
                } label: {
                    Image(systemName: keyboard ? "keyboard.chevron.compact.down" : "keyboard")
                        .font(.caption)
                }
                Button { showLogs = true } label: {
                    Image(systemName: "doc.text.magnifyingglass").font(.caption)
                }
                Button {
                    HuskBridgeFS.shared.requestDiagnostics()
                    HuskLog.log("ui", "asked the guest for diagnostics")
                } label: {
                    Image(systemName: "stethoscope").font(.caption)
                }
            }
            .padding(.horizontal, 14).padding(.vertical, 8)
            .background(.ultraThinMaterial, in: Capsule())
            .padding(.top, 6)
        }
        .statusBarHidden(true)
    }
}

/// Everything before the library: download the runtime, attach the debugger, wait
/// for Android to come up.
struct SetupView: View {
    @ObservedObject private var guest = GuestImage.shared
    @ObservedObject private var runner = QemuRunner.shared
    @ObservedObject private var bridge = HuskBridgeFS.shared
    @Binding var showLogs: Bool
    let onStart: () -> Void

    @State private var profile: QemuRunner.Profile = .phase1Android
    @State private var showSettings = false

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 22) {
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
            SettingsView(profile: $profile, showLogs: $showLogs)
        }
    }

    @ViewBuilder
    private var content: some View {
        if runner.isRunning {
            // Android's first boot is slow under TCG and its one-time setup is
            // slower still, so say what is happening rather than showing a black
            // screen for minutes.
            VStack(spacing: 12) {
                ProgressView()
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
                    Text("Downloading Android runtime").font(.headline)
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
                        Button("Start Android", action: onStart).buttonStyle(.borderedProminent)
                    } else {
                        Text("Husk needs executable memory, which on iOS only an attached debugger can grant.")
                            .font(.callout).foregroundStyle(.secondary)
                            .multilineTextAlignment(.center).padding(.horizontal, 36)
                        Button("Enable JIT with StikDebug") {
                            _ = JITBootstrap.requestAttach()
                        }
                        .buttonStyle(.borderedProminent)
                    }
                }
            }
        }
    }

    private func fmt(_ bytes: Int64) -> String {
        ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file)
    }
}

/// Live log tail with a share button. The share sheet is the practical way to get
/// husk.log and the guest's serial console off the device.
/// Settings, reached from the gear on the start screen.
///
/// These are the choices that have to be made before Android boots, because
/// booting is expensive and each of them changes what that boot costs.
struct SettingsView: View {
    @Environment(\.presentationMode) private var presentation
    @Binding var profile: QemuRunner.Profile
    @Binding var showLogs: Bool

    @State private var forceSoftware =
        UserDefaults.standard.object(forKey: "husk.forceSoftwareDisplay") as? Bool ?? true
    @State private var useSnapshot =
        UserDefaults.standard.object(forKey: "husk.downloadSnapshot") as? Bool ?? true

    var body: some View {
        NavigationView {
            Form {
                Section {
                    Toggle(isOn: $useSnapshot) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text("Download pre-booted snapshot")
                            Text(useSnapshot
                                 ? "Adds about 2 GB to the download, and skips a first boot that takes five to twelve minutes."
                                 : "Smaller download. Android boots from cold the first time, which takes five to twelve minutes.")
                                .font(.caption2).foregroundColor(.secondary)
                        }
                    }
                    .onChange(of: useSnapshot) { v in
                        UserDefaults.standard.set(v, forKey: "husk.downloadSnapshot")
                        HuskLog.log("ui", v ? "will fetch the pre-booted snapshot"
                                            : "will boot Android from cold")
                    }
                } header: {
                    Text("First launch")
                } footer: {
                    Text("A snapshot is a machine that has already finished booting. Restoring one takes seconds; booting takes minutes.")
                }

                Section {
                    Toggle(isOn: $forceSoftware) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(forceSoftware ? "Software (CPU)" : "GPU (virtio-gpu-gl)")
                            Text(forceSoftware
                                 ? "Drawing is slower, but the machine can be snapshotted."
                                 : "QEMU cannot snapshot a machine using the GPU, so every launch boots from cold.")
                                .font(.caption2)
                                .foregroundColor(forceSoftware ? .secondary : .orange)
                        }
                    }
                    .onChange(of: forceSoftware) { v in
                        UserDefaults.standard.set(v, forKey: "husk.forceSoftwareDisplay")
                        HuskLog.log("ui", v ? "forcing the software display"
                                            : "allowing the GPU display")
                    }
                } header: {
                    Text("Display")
                }

                Section {
                    Picker("Guest", selection: $profile) {
                        ForEach(QemuRunner.Profile.allCases, id: \.self) {
                            Text($0.rawValue).tag($0)
                        }
                    }
                    .onChange(of: profile) { p in
                        QemuRunner.shared.profile = p
                        HuskLog.log("ui", "guest profile set to \(p.rawValue)")
                    }
                } header: {
                    Text("Guest")
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
                    Button("Done") { presentation.wrappedValue.dismiss() }
                }
            }
        }
    }
}

struct LogView: View {
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
                    Button("Done") { dismiss() }
                }
                ToolbarItem(placement: .primaryAction) {
                    Button { showShare = true } label: { Image(systemName: "square.and.arrow.up") }
                }
            }
            .sheet(isPresented: $showShare) {
                ShareSheet(items: [
                    HuskLog.logFileURL,
                    URL(fileURLWithPath: QemuRunner.shared.guestSerialLogPath),
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
