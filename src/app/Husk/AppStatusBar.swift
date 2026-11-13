// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

private struct SystemStatusBarVisibleKey: EnvironmentKey {
    static let defaultValue = true
}

private struct StatusBarHiddenPreference: PreferenceKey {
    static var defaultValue = false
    static func reduce(value: inout Bool, nextValue: () -> Bool) { value = nextValue() }
}

extension EnvironmentValues {
    var systemStatusBarVisible: Bool {
        get { self[SystemStatusBarVisibleKey.self] }
        set { self[SystemStatusBarVisibleKey.self] = newValue }
    }
}

/// Fullscreen visibility belongs to the library item. The Husk shell always shows the system status bar.
enum AppStatusBar {
    static let androidSystem = "android"

    static func key(for itemID: String) -> String { "husk.statusBar.\(itemID)" }

    fileprivate static func updateAppearance() {
        // Re-read after SwiftUI commits the hosting controller's preference, including when the AppStorage key changes.
        DispatchQueue.main.async {
            for case let scene as UIWindowScene in UIApplication.shared.connectedScenes {
                for window in scene.windows where window.isKeyWindow {
                    var controller = window.rootViewController
                    while let current = controller {
                        if current.presentingViewController != nil { current.modalPresentationCapturesStatusBarAppearance = true }
                        current.childForStatusBarHidden?.setNeedsStatusBarAppearanceUpdate()
                        current.setNeedsStatusBarAppearanceUpdate()
                        if #available(iOS 16, *) { current.setNeedsUpdateOfSupportedInterfaceOrientations() }
                        controller = current.presentedViewController
                    }
                    window.setNeedsLayout()
                    window.layoutIfNeeded()
                }
            }
        }
    }

    /// A failed or incomplete shell response leaves the last known app's preference in effect.
    static func androidItem(from activityDump: String) -> String? {
        let lines = activityDump.split(separator: "\n")
        for marker in ["topResumedActivity=", "mResumedActivity:"] {
            for line in lines where line.contains(marker) {
                if line.trimmingCharacters(in: .whitespaces).hasSuffix("null") { return androidSystem }
                guard let range = line.range(of: #"\s([A-Za-z0-9_.]+)/[^\s}]+"#, options: .regularExpression) else { continue }
                let component = line[range].trimmingCharacters(in: .whitespaces)
                return "android:" + component.split(separator: "/")[0]
            }
        }
        return nil
    }
}

struct AppStatusBarToggle: View {
    @AppStorage private var showStatusBar: Bool

    init(itemID: String) {
        _showStatusBar = AppStorage(wrappedValue: false, AppStatusBar.key(for: itemID))
    }

    var body: some View { Toggle("System status bar", isOn: $showStatusBar) }
}

private struct AppStatusBarVisibility: ViewModifier {
    let itemID: String?
    @AppStorage private var showStatusBar: Bool

    init(itemID: String?) {
        self.itemID = itemID
        _showStatusBar = AppStorage(wrappedValue: false, AppStatusBar.key(for: itemID ?? "shell"))
    }

    func body(content: Content) -> some View {
        let hidden = itemID != nil && !showStatusBar
        content
            .environment(\.systemStatusBarVisible, !hidden)
            .background(Color(uiColor: .systemBackground).ignoresSafeArea())
            .systemStatusBarHidden(hidden)
            .huskHideSystemOverlays(hidden)
            .preference(key: StatusBarHiddenPreference.self, value: hidden)
    }
}

extension View {
    @ViewBuilder
    fileprivate func systemStatusBarHidden(_ hidden: Bool) -> some View {
        if #available(iOS 27, *) {
            toolbarVisibility(hidden ? .hidden : .visible, for: .statusBar)
        } else {
            statusBarHidden(hidden)
        }
    }

    /// nil means Husk is visible, even when a guest surface remains mounted underneath it.
    func appStatusBar(for itemID: String?) -> some View {
        modifier(AppStatusBarVisibility(itemID: itemID))
            .onPreferenceChange(StatusBarHiddenPreference.self) { _ in AppStatusBar.updateAppearance() }
    }
}
