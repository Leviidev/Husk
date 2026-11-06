// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Keep the library alive while the Java runtime drains and drops an APK's session.
@MainActor final class APKSession: ObservableObject {
    static let shared = APKSession()
    @Published private(set) var closing = false
    @Published private(set) var stoppingAPK: String?
    @Published private(set) var revision = 0
    private var retry: Task<Void, Never>?

    func close(_ app: TLApp) async -> Bool {
        guard !closing, let apk = app.apks.first,
              husk_native_loaded_apk().map({ String(cString: $0) }) == apk else { return false }
        closing = true
        defer { closing = false }
        stoppingAPK = apk
        let stopped = await Task.detached(priority: .userInitiated) { husk_native_close() }.value
        guard stopped else {
            if retry == nil {
                retry = Task {
                    let ready = await Task.detached(priority: .utility) { husk_native_wait_for_close() }.value
                    retry = nil
                    if ready && stoppingAPK == apk {
                        _ = await close(app)
                    }
                }
            }
            return false
        }
        stoppingAPK = nil
        TLUnityScreen.discard(apk: apk)
        AppWindows.shared.close(app)
        NotificationCenter.default.post(name: TLUnityUIView.huskStopPlaying, object: nil)
        CrashReport.gameEnded()
        revision += 1
        HuskLog.log("ui", "APK fully closed; next launch creates a new session: \(app.label)")
        return true
    }
}

struct CloseAPKButton: View {
    let app: TLApp
    var compact = false
    @ObservedObject private var session = APKSession.shared
    @State private var confirm = false
    @State private var failed = false

    var body: some View {
        Button(role: .destructive) { confirm = true } label: {
            if session.closing { ProgressView() }
            else { Label("Fully Close APK", systemImage: "stop.circle.fill").labelStyle(.titleAndIcon) }
        }
        .font(compact ? .system(size: 12, weight: .semibold) : .body)
        .disabled(session.closing)
        .confirmationDialog("Fully close this APK?", isPresented: $confirm, titleVisibility: .visible) {
            Button("Fully Close APK", role: .destructive) {
                Task { if !(await session.close(app)) { failed = true } }
            }
        } message: {
            Text("Husk stays open. The next launch starts the app from the beginning. Saved data is kept; unsaved changes may be lost.")
        }
        .alert("The APK is still stopping", isPresented: $failed) {
            Button("OK", role: .cancel) { }
        } message: {
            Text("Husk is still releasing this APK's resources and will finish closing it automatically. The app cannot be resumed while it is stopping.")
        }
    }
}

/// A stop request cannot be undone: do not present the retained GPU frame as a live app.
struct APKStoppingOverlay: View {
    let app: TLApp
    let onReturn: () -> Void
    @ObservedObject private var session = APKSession.shared

    var body: some View {
        if let apk = app.apks.first, session.stoppingAPK == apk {
            VStack(spacing: 16) {
                ProgressView()
                Text("Closing APK…").font(.headline)
                Text("Husk is releasing the app's resources. You can return to the library while it finishes.")
                    .font(.subheadline).multilineTextAlignment(.center)
                Button("Return to Library", action: onReturn)
            }
            .padding(24)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background(Color(.systemBackground))
        }
    }
}
