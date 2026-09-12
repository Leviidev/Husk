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
        .onChange(of: scenePhase) { _, phase in
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

        guard !started else { return }
        guard runner.profile == .phase0Alpine || guest.state == .ready else { return }
        guard JITBootstrap.isDebuggerAttached else {
            HuskLog.log("ui", "no debugger attached; waiting for StikDebug")
            return
        }
        start()
    }

    private func start() {
        guard !started else { return }
        guard JITBootstrap.isDebuggerAttached else { return }
        HuskLog.log("ui", "CS_DEBUGGED set; starting QEMU")
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
            HuskGLScreen().ignoresSafeArea()

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

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()
            VStack(spacing: 22) {
                Text("Husk")
                    .font(.system(size: 44, weight: .semibold, design: .rounded))

                content

                Picker("Guest", selection: $profile) {
                    ForEach(QemuRunner.Profile.allCases, id: \.self) { Text($0.rawValue).tag($0) }
                }
                .pickerStyle(.segmented)
                .padding(.horizontal, 30)
                .onChange(of: profile) { _, p in
                    QemuRunner.shared.profile = p
                    HuskLog.log("ui", "guest profile set to \(p.rawValue)")
                }

                Button { showLogs = true } label: {
                    Label("View logs", systemImage: "doc.text.magnifyingglass")
                }
                .font(.footnote)
            }
            .foregroundStyle(.white)
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
                    Button("Try again") { guest.download() }.buttonStyle(.borderedProminent)
                }
            case .missing:
                VStack(spacing: 12) {
                    Text("Husk needs its Android runtime — about 760 MB. Android itself is downloaded afterwards by the runtime.")
                        .font(.callout).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center).padding(.horizontal, 36)
                    Button("Download Android runtime") { guest.download() }
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
