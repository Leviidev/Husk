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
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .statusBarHidden(true)
                .persistentSystemOverlays(.hidden)
        }
    }
}
