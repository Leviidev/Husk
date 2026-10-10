// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit
import WebKit

extension WebAppRuntime {
    /// A web app closed itself; the object is the app's id.
    static let closed = Notification.Name("husk.web.closed")
}

/// The web app's page, wherever it is shown: full screen or in a window. The same web view each time, so the app keeps its state.
struct WebAppSurface: UIViewRepresentable {
    let app: TLApp

    func makeUIView(context: Context) -> WebAppHostView { WebAppHostView(app: app) }
    func updateUIView(_ view: WebAppHostView, context: Context) {}
}

final class WebAppHostView: UIView {
    private let runtime: WebAppRuntime?

    init(app: TLApp) {
        runtime = WebAppRuntime.runtime(for: app)
        super.init(frame: .zero)
        backgroundColor = .systemBackground
        guard let runtime else { return }
        let id = app.id
        runtime.onClose = { NotificationCenter.default.post(name: WebAppRuntime.closed, object: id) }
        runtime.webView.removeFromSuperview()
        addSubview(runtime.webView)
        // Android's back gesture: a swipe in from the left edge goes to the app's back handler, then the page history.
        let edge = UIScreenEdgePanGestureRecognizer(target: self, action: #selector(edgeSwiped(_:)))
        edge.edges = .left
        addGestureRecognizer(edge)
    }

    required init?(coder: NSCoder) { fatalError("not used") }

    @objc private func edgeSwiped(_ g: UIScreenEdgePanGestureRecognizer) {
        if g.state == .ended, g.translation(in: self).x > 60 { runtime?.back() }
    }

    override func layoutSubviews() {
        super.layoutSubviews()
        if runtime?.webView.superview === self { runtime?.webView.frame = bounds }
    }

    override func didMoveToWindow() {
        super.didMoveToWindow()
        runtime?.setActive(window != nil)
        if window != nil, let rt = runtime, rt.webView.superview !== self {
            addSubview(rt.webView)
            setNeedsLayout()
        }
    }
}

/// A web app full screen: a thin bar to leave it by, and the page below the status bar, in the colour the app asked for.
struct WebAppScreenView: View {
    let app: TLApp
    @Environment(\.dismiss) private var dismiss
    @State private var color: Color = Color(uiColor: .systemBackground)

    private var portrait: Bool { app.apks.first.map { husk_apk_orientation($0) != 0 } ?? true }

    var body: some View {
        VStack(spacing: 0) {
            HStack(spacing: 12) {
                Button { dismiss() } label: { Label("Close", systemImage: "xmark").font(.system(size: 13, weight: .semibold)) }
                Button { WebAppRuntime.runtime(for: app)?.back() } label: {
                    Label("Back", systemImage: "chevron.backward").font(.system(size: 13, weight: .semibold))
                }
                Spacer()
                Text(app.label).font(.system(size: 12, weight: .medium)).foregroundStyle(.secondary).lineLimit(1)
            }
            .padding(.horizontal, 14)
            .frame(height: 30)
            .background(Color(uiColor: .systemBackground))
            WebAppSurface(app: app)
        }
        .background(color.ignoresSafeArea())
        .onAppear {
            HuskOrientation.set(portrait ? .portrait : .landscape)
            refreshColor()
        }
        .onDisappear { HuskOrientation.set(HuskOrientation.standard) }
        .onReceive(NotificationCenter.default.publisher(for: WebAppRuntime.closed)) { n in
            if (n.object as? String) == app.id { WebAppRuntime.close(app); dismiss() }
        }
        .onReceive(NotificationCenter.default.publisher(for: WebAppRuntime.chromeChanged)) { _ in refreshColor() }
    }

    private func refreshColor() {
        if let hex = WebAppRuntime.runtime(for: app)?.chromeColor, let c = Color(hexString: hex) { color = c }
    }
}

private extension Color {
    /// "#07070f" or "#07070fff".
    init?(hexString: String) {
        var s = hexString.trimmingCharacters(in: .whitespaces)
        if s.hasPrefix("#") { s.removeFirst() }
        guard s.count == 6 || s.count == 8, let v = UInt64(s, radix: 16) else { return nil }
        let rgb = s.count == 8 ? v >> 8 : v
        self.init(red: Double((rgb >> 16) & 0xFF) / 255, green: Double((rgb >> 8) & 0xFF) / 255, blue: Double(rgb & 0xFF) / 255)
    }
}
