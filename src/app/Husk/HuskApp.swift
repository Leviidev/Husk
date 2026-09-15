// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

@main
struct HuskApp: App {
    init() {
        // Order matters. HuskLog redirects stderr, so anything that logs before
        // this point is lost -- and the JIT path is exactly what we cannot afford
        // to lose the first line of.
        HuskLog.start()
        HuskLog.logFootprint("app-launch")

        // Then the trap guard: without it, any brk we issue when StikDebug is
        // absent kills the process outright rather than returning an error.
        JITBootstrap.installTrapGuard()

        // The bars belong to UIKit, and it reads their appearance once when it
        // builds them. Set before the first view exists or the tab bar spends
        // the session in the system's default grey.
        Theme.applyBarAppearance()
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .statusBarHidden(true)
                .persistentSystemOverlays(.hidden)
        }
    }
}
