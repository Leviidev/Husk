// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import WebKit

/// Sheet hosting WKWebView pointing to https://accounts.google.com/EmbeddedSetup to sign in.
/// Intercepts cookies to extract oauth_token and user email.
struct GoogleSignInSheet: View {
    @Environment(\.dismiss) private var dismiss
    @ObservedObject var manager = PlayStoreManager.shared

    @State private var webLoading = true
    @State private var intercepted = false
    @State private var promptError: String?

    var body: some View {
        NavigationStack {
            ZStack {
                Theme.backdrop.ignoresSafeArea()

                GoogleLoginWebView(
                    isLoading: $webLoading,
                    onAuthCaptured: { email, token in
                        guard !intercepted else { return }
                        intercepted = true
                        Task {
                            await manager.signIn(email: email, oauthToken: token)
                            dismiss()
                        }
                    }
                )
                .opacity(manager.isLoading ? 0.3 : 1.0)
                .disabled(manager.isLoading)

                if manager.isLoading {
                    VStack(spacing: 16) {
                        ProgressView()
                            .scaleEffect(1.2)
                        Text(manager.statusMessage ?? L10n.tr("Signing in to Google…"))
                            .font(.subheadline.weight(.medium))
                            .foregroundStyle(Theme.text)
                    }
                    .padding(24)
                    .background(Theme.surfaceHigh, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
                    .shadow(radius: 20)
                } else if webLoading {
                    ProgressView(L10n.tr("Loading Google Sign-In…"))
                        .font(.footnote)
                }
            }
            .navigationTitle(L10n.tr("Sign in with Google"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(L10n.tr("Cancel")) {
                        dismiss()
                    }
                    .disabled(manager.isLoading)
                }
            }
        }
    }
}

/// UIViewRepresentable wrapping WKWebView for Google EmbeddedSetup
struct GoogleLoginWebView: UIViewRepresentable {
    @Binding var isLoading: Bool
    let onAuthCaptured: (String, String) -> Void

    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }

    func makeUIView(context: Context) -> WKWebView {
        let config = WKWebViewConfiguration()
        // Ensure non-persistent or isolated cookies if needed, or let standard store handle it
        let webView = WKWebView(frame: .zero, configuration: config)
        webView.navigationDelegate = context.coordinator
        webView.customUserAgent = PlayDevice.authUserAgent

        // Clear any existing cookies for accounts.google.com so user can enter credentials cleanly
        WKWebsiteDataStore.default().removeData(
            ofTypes: [WKWebsiteDataTypeCookies],
            modifiedSince: Date.distantPast
        ) {
            if let url = URL(string: "https://accounts.google.com/EmbeddedSetup") {
                let req = URLRequest(url: url)
                webView.load(req)
            }
        }

        return webView
    }

    func updateUIView(_ uiView: WKWebView, context: Context) {}

    class Coordinator: NSObject, WKNavigationDelegate {
        var parent: GoogleLoginWebView
        var captured = false
        private var checkTimer: Timer?

        init(_ parent: GoogleLoginWebView) {
            self.parent = parent
            super.init()
        }

        deinit {
            checkTimer?.invalidate()
        }

        func webView(_ webView: WKWebView, didStartProvisionalNavigation navigation: WKNavigation!) {
            DispatchQueue.main.async {
                self.parent.isLoading = true
            }
        }

        func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
            DispatchQueue.main.async {
                self.parent.isLoading = false
            }
            checkCookies(in: webView)

            // Also poll cookies briefly since EmbeddedSetup sets oauth_token after redirect
            if checkTimer == nil {
                checkTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self, weak webView] _ in
                    guard let self = self, let wv = webView else { return }
                    self.checkCookies(in: wv)
                }
            }
        }

        func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
            decisionHandler(.allow)
            checkCookies(in: webView)
        }

        private func checkCookies(in webView: WKWebView) {
            guard !captured else { return }

            webView.configuration.websiteDataStore.httpCookieStore.getAllCookies { [weak self] cookies in
                guard let self = self, !self.captured else { return }

                var oauthToken: String?
                var accountEmail: String?

                for cookie in cookies {
                    if cookie.name == "oauth_token", !cookie.value.isEmpty {
                        oauthToken = cookie.value
                    }
                    if cookie.name == "ACCOUNT_CHOOSER" || cookie.name == "LSID" || cookie.name == "user" {
                        // Sometimes cookies store email in values or user id
                    }
                }

                if let token = oauthToken {
                    self.captured = true
                    self.checkTimer?.invalidate()
                    self.checkTimer = nil

                    // Try to inspect webView URL or page content for email
                    webView.evaluateJavaScript("document.querySelector('[data-email]')?.getAttribute('data-email') || document.body.innerText") { [weak self] result, _ in
                        var email = "account@gmail.com"
                        if let text = result as? String {
                            // Match email regex
                            if let range = text.range(of: "[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}", options: .regularExpression) {
                                email = String(text[range])
                            }
                        }

                        DispatchQueue.main.async {
                            self?.parent.onAuthCaptured(email, token)
                        }
                    }
                }
            }
        }
    }
}
