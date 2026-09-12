// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Phase 0 UI: enough to drive the JIT handoff, watch what happens, and get the
/// logs off the device. This is scaffolding, not the product — Phase 3 replaces
/// it with the library grid and full-screen per-app presentation.
struct ContentView: View {
    @StateObject private var guest = GuestImage.shared
    @StateObject private var runner = QemuRunner.shared
    @State private var booted = false
    @State private var status = "Checking…"
    @State private var showLogs = false
    @State private var profile: QemuRunner.Profile = .phase1Waydroid
    @Environment(\.scenePhase) private var scenePhase

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()

            if booted {
                HuskDisplay().ignoresSafeArea()
                if let msg = runner.setupMessage, !msg.contains("complete") {
                    // First-run Android setup happens inside the guest and takes
                    // minutes; without this the screen is just black and idle.
                    VStack {
                        Spacer()
                        HStack(spacing: 10) {
                            ProgressView().controlSize(.small)
                            Text(msg).font(.caption)
                        }
                        .padding(.horizontal, 16).padding(.vertical, 10)
                        .background(.ultraThinMaterial, in: Capsule())
                        .padding(.bottom, 40)
                    }
                }
                // Always reachable: if the guest never draws, this button is the
                // only way to find out why without plugging into a Mac.
                VStack {
                    HStack {
                        Spacer()
                        Button { showLogs = true } label: {
                            Image(systemName: "doc.text.magnifyingglass")
                                .font(.title3)
                                .padding(10)
                                .background(.ultraThinMaterial, in: Circle())
                        }
                        .padding(.trailing, 16)
                        .padding(.top, 8)
                    }
                    Spacer()
                }
            } else {
                setupView
            }
        }
        .sheet(isPresented: $showLogs) { LogView() }
        .onAppear { evaluate() }
        .onChange(of: scenePhase) { _, phase in
            // StikDebug relaunches us after attaching, so the interesting moment is
            // coming back to the foreground, not first launch.
            if phase == .active { evaluate() }
        }
    }

    @ViewBuilder
    private var setupView: some View {
        VStack(spacing: 22) {
            Text("Husk")
                .font(.system(size: 44, weight: .semibold, design: .rounded))

            switch guest.state {
            case .downloading(let p, let received, let total):
                VStack(spacing: 10) {
                    Text("Downloading Android runtime").font(.headline)
                    ProgressView(value: p).padding(.horizontal, 50)
                    Text("\(fmt(received)) of \(total > 0 ? fmt(total) : "…")")
                        .font(.caption.monospacedDigit())
                        .foregroundStyle(.secondary)
                    Button("Cancel") { guest.cancel() }.font(.footnote)
                }

            case .installing:
                VStack(spacing: 10) {
                    ProgressView()
                    Text("Installing…").font(.callout).foregroundStyle(.secondary)
                }

            case .failed(let message):
                VStack(spacing: 10) {
                    Text("Download failed").font(.headline).foregroundStyle(.red)
                    Text(message)
                        .font(.caption).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center).padding(.horizontal, 36)
                    Button("Try again") { guest.download() }.buttonStyle(.borderedProminent)
                }

            case .missing:
                VStack(spacing: 10) {
                    Text(status)
                        .font(.callout).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center).padding(.horizontal, 36)
                    // The Android runtime is a ~1.2 GB download rather than part of
                    // the app, and Android itself is fetched by the guest after
                    // that, from Waydroid's own servers.
                    Button("Download Android runtime") { guest.download() }
                        .buttonStyle(.borderedProminent)
                }

            case .ready:
                VStack(spacing: 10) {
                    Text(status)
                        .font(.callout).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center).padding(.horizontal, 36)
                    if !JITBootstrap.isDebuggerAttached {
                        Button("Enable JIT with StikDebug") {
                            if !JITBootstrap.requestAttach() {
                                status = "StikDebug is not installed, or refused the request."
                            }
                        }
                        .buttonStyle(.borderedProminent)
                    }
                }
            }

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

    private func fmt(_ bytes: Int64) -> String {
        ByteCountFormatter.string(fromByteCount: bytes, countStyle: .file)
    }

    private func evaluate() {
        guard !booted else { return }

        try? guest.prepareFirmware()
        guest.refresh()

        // Phase 1 cannot start without the downloaded guest; Phase 0 boots from
        // the app bundle and needs nothing.
        if profile == .phase1Waydroid, guest.state != .ready {
            if case .missing = guest.state {
                status = """
                Husk needs its Android runtime — a Debian guest with Waydroid, \
                about 1.2 GB. Android itself is downloaded afterwards by the guest.
                """
            }
            return
        }

        // The gate is "a debugger is attached", not "JIT is live". A JIT region is
        // only allocated inside qemu_init, so isLive cannot be true until after the
        // guest has started -- gating on it would mean never starting at all.
        if JITBootstrap.isDebuggerAttached {
            status = "Debugger attached — booting guest"
            HuskLog.log("ui", "CS_DEBUGGED set; starting QEMU")
            QemuRunner.shared.start()
            booted = true
            verifyJITCameUp()
            return
        }

        HuskLog.log("ui", "no debugger attached; showing StikDebug handoff")
        status = """
        Husk needs executable memory, which on iOS 27 only an attached debugger can grant.

        Tap below to hand off to StikDebug. It will attach, set up the JIT region, and \
        relaunch Husk automatically.
        """
    }
}

extension ContentView {
    /// The allocation and its self-test happen inside qemu_init, so give it a few
    /// seconds and then say plainly whether JIT actually came up. Without this the
    /// only symptom of a failed self-test is a black screen.
    func verifyJITCameUp() {
        DispatchQueue.main.asyncAfter(deadline: .now() + 8) {
            if JITBootstrap.isLive {
                HuskLog.log("ui", "confirmed: JIT region allocated and self-test passed")
            } else {
                HuskLog.log("ui", "WARNING: 8s after start, no JIT region has passed "
                                + "the self-test. The guest is probably not running. "
                                + "Check the [husk-jit] lines above.")
            }
        }
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
