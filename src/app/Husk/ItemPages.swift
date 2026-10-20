// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

// MARK: - the top of every page

/// An item's page opens on its artwork: the icon over a blurred, darkened copy of itself, the name, a line under it, and
/// the one thing to do -- Play, Open, Start -- as a big button.
private struct PageHero<Action: View>: View {
    let item: LibraryItem
    let subtitle: String
    @ViewBuilder var action: Action

    var body: some View {
        VStack(spacing: 0) {
            ZStack(alignment: .bottomLeading) {
                ItemBackdrop(item: item, fallback: Color(uiColor: .tertiarySystemFill))
                LinearGradient(colors: [.clear, .black.opacity(0.7)], startPoint: .top, endPoint: .bottom)
                HStack(alignment: .bottom, spacing: 14) {
                    ItemIcon(item: item, size: 84)
                        .shadow(color: .black.opacity(0.35), radius: 12, y: 5)
                    VStack(alignment: .leading, spacing: 4) {
                        Text(item.title)
                            .font(.system(.title2, design: .rounded).weight(.heavy))
                            .lineLimit(2)
                            .minimumScaleFactor(0.8)
                        Text(subtitle)
                            .font(.subheadline)
                            .foregroundStyle(.white.opacity(0.75))
                            .lineLimit(2)
                    }
                    .foregroundStyle(.white)
                    Spacer(minLength: 0)
                }
                .padding(16)
            }
            .frame(height: 210)
            action
                .padding(14)
        }
        .background(Color(uiColor: .secondarySystemGroupedBackground))
        .clipShape(RoundedRectangle(cornerRadius: 24, style: .continuous))
    }
}

/// The page's main button.
private struct BigButton: View {
    let title: String
    let systemImage: String
    var prominent = true
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            Label(title, systemImage: systemImage)
                .font(.system(.headline, design: .rounded))
                .frame(maxWidth: .infinity)
                .frame(height: 50)
                .foregroundStyle(prominent ? Color.white : Color.accentColor)
                .background(prominent ? Color.accentColor : Color.accentColor.opacity(0.14),
                            in: RoundedRectangle(cornerRadius: 15, style: .continuous))
        }
        .buttonStyle(CardButtonStyle())
    }
}

/// A hero that sits in a List as its first row, edge to edge within the list's margins.
private extension View {
    func heroRow() -> some View {
        listRowInsets(EdgeInsets()).listRowBackground(Color.clear)
    }
}

private func fileBytes(_ paths: [String]) -> String {
    let total = paths.reduce(Int64(0)) { sum, p in
        sum + (((try? FileManager.default.attributesOfItem(atPath: p)[.size]) as? NSNumber)?.int64Value ?? 0)
    }
    return ByteCountFormatter.string(fromByteCount: total, countStyle: .file)
}

@MainActor private func turnOnJIT() {
    let jit = JITCoordinator.shared
    if HuskBuiltInJIT.isAvailable { jit.method = .builtIn }
    jit.enable()
}

// MARK: - a game

/// A translation-layer game: Play, its settings, what it is, and removing it.
struct GamePage: View {
    let app: TLApp
    @ObservedObject private var showcase = ShowcaseStore.shared

    @ObservedObject private var store = TranslationLayerStore.shared
    @ObservedObject private var jit = JITCoordinator.shared
    @Environment(\.dismiss) private var dismiss
    @State private var playing = false
    @State private var confirmRemove = false
    @State private var backupFile: URL?
    @State private var savesBusy = false
    @State private var savesMessage: String?
    @State private var confirmRestore: URL?
    @State private var confirmQuit = false
    @State private var reporting: CompatSubject?

    private var runs: Bool { app.report?.canRun == true }

    private var compatSubject: CompatSubject? {
        app.packageName.map { CompatSubject(package: $0, name: app.label, isGame: LibraryItem.game(app).isGame, iconPath: app.iconPath) }
    }

    /// Another game already loaded in this run of Husk: engines cannot be unloaded, so this one cannot start until Husk is
    /// closed and opened again.
    private var blockedBy: String? {
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }), let apk = app.apks.first, loaded != apk
        else { return nil }
        return store.apps.first { $0.apks.first == loaded }?.label ?? L10n.tr("Another game")
    }

    private var preflight: AppDiagnosis? { AppDiagnosis.preflight(app) }

    private var subtitle: String {
        if let used = LibraryItem.game(app).usedText { return used }
        return runs ? L10n.tr("Ready to play") : L10n.tr("May not run on this iPhone")
    }

    var body: some View {
        List {
            Section {
                PageHero(item: .game(app), subtitle: subtitle) { primary }
            } footer: {
                if isGeodeLauncher {
                    Text(L10n.tr("This is Geode's Android launcher. On Husk, Geode runs inside Geometry Dash instead: turn it on here (or in Geometry Dash's settings, under Mods), then start Geometry Dash and use Geode's button on its main menu to get mods. This APK supplies a library Geode needs, so keep it."))
                } else if let other = blockedBy {
                    Text(L10n.tr("\(other) is still loaded, and a game cannot be unloaded once it has started. Close Husk with the button above, then open it again to play \(app.label)."))
                } else if !runs {
                    Text(L10n.tr("This APK has no 64-bit code Husk can run, so it will probably not start."))
                }
            }
            .heroRow()

            // what the files say before it starts: a damaged APK, the wrong CPU, a missing piece, protection that refuses Husk
            if let d = preflight {
                Section {
                    AppDiagnosisCard(diagnosis: d)
                        .listRowInsets(EdgeInsets(top: 4, leading: 4, bottom: 4, trailing: 4))
                }
            }
            // why it stopped last time, until it plays again
            if let d = GameStatusStore.shared.diagnosis(app.id), d != preflight {
                Section(L10n.tr("Last Run")) {
                    AppDiagnosisCard(diagnosis: d, showsEvidence: true)
                        .listRowInsets(EdgeInsets(top: 4, leading: 4, bottom: 4, trailing: 4))
                }
            }

            if let compatSubject, !isGeodeLauncher {
                CompatibilitySection(subject: compatSubject, reporting: $reporting)
            }

            if !ShowcaseStore.shared.pictures(for: app.packageName).isEmpty {
                Section(L10n.tr("Screenshots")) {
                    ShowcaseGallery(package: app.packageName)
                        .listRowInsets(EdgeInsets(top: 10, leading: 12, bottom: 10, trailing: 12))
                }
            }

            Section {
                HuskLink(value: LibraryRoute.gameSettings(app.id)) {
                    Label(L10n.tr("Game Settings"), systemImage: "slider.horizontal.3")
                }
                HuskLink(value: LibraryRoute.gameReport(app.id)) {
                    Label(L10n.tr("Technical Details"), systemImage: "cpu")
                }
            }

            if !isGeodeLauncher {
                Section {
                    Button { backUp() } label: { Label(L10n.tr("Back Up Saves"), systemImage: "square.and.arrow.up") }
                        .disabled(savesBusy || !SaveBackup.hasData(app))
                    Button { pickRestore() } label: { Label(L10n.tr("Restore Saves"), systemImage: "clock.arrow.circlepath") }
                        .disabled(savesBusy || loadedNow)
                } header: {
                    Text(L10n.tr("Saves"))
                } footer: {
                    if let savesMessage { Text(savesMessage) }
                    else if loadedNow { Text(L10n.tr("\(app.label) has been started in this run of Husk. Close Husk and open it again to restore saves.")) }
                    else { Text(L10n.tr("A backup is a .zip of everything \(app.label) has saved in Husk, to keep in Files or move to another device.")) }
                }
            }

            Section(L10n.tr("About")) {
                LabeledContent(L10n.tr("Runs With"), value: app.report?.runnerName ?? L10n.tr("Unknown"))
                if let st = GameStatusStore.shared.status(app.id) {
                    LabeledContent(L10n.tr("Last Result")) {
                        Label(st.result.label, systemImage: st.result.symbol).foregroundStyle(st.result.color)
                    }
                }
                LabeledContent(L10n.tr("Last Played"), value: app.lastPlayed.map { $0.formatted(date: .abbreviated, time: .shortened) } ?? L10n.tr("Never"))
                LabeledContent(L10n.tr("Size"), value: fileBytes(app.apks))
            }

            Section {
                Button(role: .destructive) { confirmRemove = true } label: {
                    Label(L10n.tr("Remove Game"), systemImage: "trash")
                }
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle(app.label)
        .navigationBarTitleDisplayMode(.inline)
        .fullScreenCover(isPresented: $playing, onDismiss: askHowItRan) { TLAttemptView(app: app) }
        .sheet(item: $reporting) { CompatReportSheet(subject: $0) }
        .sheet(item: Binding(get: { backupFile.map(IdentifiedURL.init) }, set: { backupFile = $0?.url })) { f in
            ShareSheet(items: [f.url])
        }
        .confirmationDialog(L10n.tr("Replace \(app.label)'s saves?"), isPresented: Binding(get: { confirmRestore != nil }, set: { if !$0 { confirmRestore = nil } }),
                            titleVisibility: .visible) {
            Button(L10n.tr("Replace"), role: .destructive) { if let url = confirmRestore { restore(url) } }
        } message: {
            Text(L10n.tr("What \(app.label) has saved now is replaced by the backup. Back up first if you want to keep it."))
        }
        .confirmationDialog(L10n.tr("Close Husk?"), isPresented: $confirmQuit, titleVisibility: .visible) {
            Button(L10n.tr("Close Husk")) {
                // The game that is loaded is closed with Husk, which is the normal way out for it, not a crash; and this
                // one starts by itself when Husk is opened again.
                Router.shared.switchTo(app.id)
                CrashReport.gameEnded()
                HuskLog.flushNow()
                exit(0)
            }
        } message: {
            Text(L10n.tr("\(blockedBy ?? L10n.tr("Another game")) is still loaded and only one game can run per session. Husk closes; open it again and \(app.label) starts by itself."))
        }
        .confirmationDialog(L10n.tr("Remove \(app.label)?"), isPresented: $confirmRemove, titleVisibility: .visible) {
            Button(L10n.tr("Remove"), role: .destructive) {
                dismiss()
                store.remove(app)
            }
        } message: {
            Text(L10n.tr("The game and everything it saved here are deleted from Husk."))
        }
        .id(jit.attachGeneration)
        .onAppear {
            // Kept until JIT is on: this page is rebuilt when it attaches (.id above), and plays then.
            guard Router.shared.autoPlay == app.id, Launcher.jitOn, blockedBy == nil else { return }
            Router.shared.autoPlay = nil
            play()
        }
    }

    /// Play: with Geode on, whatever it is missing (a newer release, its resources) is fetched first.
    private func play() {
        guard TLAppSettings.load(app.id).geode, case .ready = GeodeSupport.shared.current(app) else {
            if TLAppSettings.load(app.id).geode {
                Task { await GeodeSupport.shared.prepare(app); start() }
            } else {
                start()
            }
            return
        }
        start()
    }

    /// After a session that got going: how did it run? Once per game per build, and only when it played (a crash has its own sheet).
    private func askHowItRan() {
        guard var subject = compatSubject, CompatibilityStore.shared.shouldAsk(subject.package),
              GameStatusStore.shared.status(app.id)?.result == .plays else { return }
        subject.afterPlaying = true
        Task { try? await Task.sleep(nanoseconds: 500_000_000); reporting = subject }
    }

    /// Full screen, or in a window over Husk when Multitasking says so for this app.
    private func start() {
        if app.report?.runsOnNativeRuntime == true || app.report?.webKind != nil, AppWindows.opensInWindow(app) {
            AppWindows.shared.open(app)
        } else {
            AppWindows.shared.close(app)
            playing = true
        }
    }

    /// This game's engine is loaded in this run of Husk, so its files may be open: no restoring under it.
    private var loadedNow: Bool {
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }) else { return false }
        return loaded == app.apks.first
    }

    private func backUp() {
        savesBusy = true; savesMessage = nil
        let app = self.app
        Task.detached {
            let result = Result { try SaveBackup.export(app) }
            await MainActor.run {
                savesBusy = false
                switch result {
                case .success(let url): backupFile = url
                case .failure(let e): savesMessage = e.localizedDescription
                }
            }
        }
    }

    private func pickRestore() {
        HuskFilePicker.present(types: [.zip], multiple: false) { urls in
            if let url = urls.first { confirmRestore = url }
        } onFail: { savesMessage = $0 }
    }

    private func restore(_ url: URL) {
        savesBusy = true; savesMessage = nil
        let app = self.app
        Task.detached {
            let result = Result { try SaveBackup.restore(app, from: url) }
            await MainActor.run {
                savesBusy = false
                switch result {
                case .success(let n): savesMessage = L10n.tr("Restored \(n) file\(n == 1 ? "" : "s") from \(url.lastPathComponent).")
                case .failure(let e): savesMessage = e.localizedDescription
                }
            }
        }
    }

    /// Geode's Android launcher, added as if it were a game: it is a whole Android app, which Husk does not run. On Husk Geode
    /// is turned on from Geometry Dash's own settings, and this APK only lends it a library.
    private var isGeodeLauncher: Bool { app.packageName == GeodeSupport.launcherPackage }
    private var geometryDash: TLApp? { store.apps.first { $0.packageName == GeodeSupport.gamePackage } }

    @ViewBuilder
    private var primary: some View {
        if isGeodeLauncher {
            if let gd = geometryDash {
                BigButton(title: TLAppSettings.load(gd.id).geode ? L10n.tr("Geode Is On for Geometry Dash") : L10n.tr("Turn On Geode for Geometry Dash"),
                          systemImage: "puzzlepiece.extension.fill", prominent: !TLAppSettings.load(gd.id).geode) {
                    var s = TLAppSettings.load(gd.id)
                    s.geode = true
                    s.save(gd.id)
                    Task { await GeodeSupport.shared.prepare(gd) }
                }
            } else {
                BigButton(title: L10n.tr("Add Geometry Dash First"), systemImage: "plus", prominent: false) { }.disabled(true)
            }
        } else if blockedBy != nil {
            BigButton(title: L10n.tr("Close Husk to Play"), systemImage: "xmark.circle", prominent: false) { confirmQuit = true }
        } else if !Launcher.jitOn {
            if jit.busy {
                HStack(spacing: 10) { ProgressView(); Text(jit.status ?? L10n.tr("Turning on JIT…")).foregroundStyle(.secondary) }
                    .frame(maxWidth: .infinity).frame(height: 50)
            } else {
                BigButton(title: L10n.tr("Turn On JIT to Play"), systemImage: "bolt.fill", action: turnOnJIT)
            }
        } else {
            BigButton(title: runs ? L10n.tr("Play") : L10n.tr("Try to Run"), systemImage: "play.fill", prominent: runs) { play() }
        }
    }
}

// MARK: - a PS4 game

/// A PS4 game from Documents/PS4 Games: Play, and what it is. Run by the PS4 layer (libhuskps4.dylib) on Husk's JIT.
struct PS4GamePage: View {
    let game: PS4Game
    @ObservedObject private var jit = JITCoordinator.shared
    @State private var playing = false
    @State private var confirmQuit = false

    private var item: LibraryItem { .ps4(game) }

    /// Another game already loaded in this run of Husk (an Android game's engine, or another PS4 game).
    private var blockedBy: String? {
        if Router.shared.androidStarted { return "Android" }   // QEMU holds the JIT region the PS4 layer needs
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }), loaded != game.libraryID else { return nil }
        if loaded.hasPrefix("ps4:") { return PS4Store.shared.game(String(loaded.dropFirst(4)))?.title ?? L10n.tr("Another game") }
        return TranslationLayerStore.shared.apps.first { $0.apks.first == loaded }?.label ?? L10n.tr("Another game")
    }

    private var subtitle: String {
        if let used = item.usedText { return used }
        return "PlayStation 4" + (game.version.map { " · v\($0)" } ?? "")
    }

    var body: some View {
        List {
            PageHero(item: item, subtitle: subtitle) { primary }
                .heroRow()
            if !PS4Store.available {
                Section {
                    Text(PS4Store.moduleURL == nil
                         ? L10n.tr("This build of Husk does not include the PS4 layer.")
                         : L10n.tr("PS4 games need iOS 17.4 or later."))
                        .foregroundStyle(.secondary)
                }
            }
            Section(L10n.tr("About")) {
                LabeledContent(L10n.tr("Runs With"), value: L10n.tr("Husk's PS4 layer"))
                LabeledContent(L10n.tr("Title ID"), value: game.titleID)
                if let v = game.version { LabeledContent(L10n.tr("Version"), value: v) }
                LabeledContent(L10n.tr("Last Played"), value: game.lastPlayed.map { $0.formatted(date: .abbreviated, time: .shortened) } ?? L10n.tr("Never"))
                LabeledContent(L10n.tr("Folder"), value: "PS4 Games/" + game.folder.lastPathComponent)
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle(game.title)
        .navigationBarTitleDisplayMode(.inline)
        .fullScreenCover(isPresented: $playing) { PS4PlayView(game: game) }
        .confirmationDialog(L10n.tr("Close Husk?"), isPresented: $confirmQuit, titleVisibility: .visible) {
            Button(L10n.tr("Close Husk")) {
                Router.shared.switchTo(game.libraryID)
                CrashReport.gameEnded()
                HuskLog.flushNow()
                exit(0)
            }
        } message: {
            Text(L10n.tr("\(blockedBy ?? L10n.tr("Another game")) is still loaded and only one game can run per session. Husk closes; open it again and \(game.title) starts by itself."))
        }
        .id(jit.attachGeneration)
        .onAppear {
            guard Router.shared.autoPlay == game.libraryID, Launcher.jitOn, blockedBy == nil, PS4Store.available else { return }
            Router.shared.autoPlay = nil
            playing = true
        }
    }

    @ViewBuilder
    private var primary: some View {
        if !PS4Store.available {
            BigButton(title: L10n.tr("Can't Play Here"), systemImage: "xmark.octagon", prominent: false) { }.disabled(true)
        } else if blockedBy != nil {
            BigButton(title: L10n.tr("Close Husk to Play"), systemImage: "xmark.circle", prominent: false) { confirmQuit = true }
        } else if !Launcher.jitOn {
            if jit.busy {
                HStack(spacing: 10) { ProgressView(); Text(jit.status ?? L10n.tr("Turning on JIT…")).foregroundStyle(.secondary) }
                    .frame(maxWidth: .infinity).frame(height: 50)
            } else {
                BigButton(title: L10n.tr("Turn On JIT to Play"), systemImage: "bolt.fill", action: turnOnJIT)
            }
        } else {
            BigButton(title: L10n.tr("Play"), systemImage: "play.fill") { playing = true }
        }
    }
}

// MARK: - an app inside Android

/// An app installed in Android: Open (starting Android first when it is not running), what it is, and its files.
struct AndroidAppPage: View {
    let app: AndroidHost.Package
    @ObservedObject private var showcase = ShowcaseStore.shared

    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var guest = GuestImage.shared
    @ObservedObject private var runner = QemuRunner.shared
    @ObservedObject private var router = Router.shared
    @Environment(\.dismiss) private var dismiss
    @State private var confirmUninstall = false

    private var live: AndroidHost.Package { host.packages.first { $0.name == app.name } ?? app }
    private var canOpen: Bool { host.isReady && host.busy == nil }
    private var item: LibraryItem { .app(live) }

    var body: some View {
        List {
            Section {
                PageHero(item: item, subtitle: item.usedText ?? L10n.tr("Runs inside Android")) { primary }
            } footer: {
                if router.pendingAndroidLaunch == app.name, !host.isReady {
                    Text(L10n.tr("\(live.label) opens by itself once Android has started."))
                }
            }
            .heroRow()

            if !ShowcaseStore.shared.pictures(for: app.name).isEmpty {
                Section(L10n.tr("Screenshots")) {
                    ShowcaseGallery(package: app.name)
                        .listRowInsets(EdgeInsets(top: 10, leading: 12, bottom: 10, trailing: 12))
                }
            }

            Section(L10n.tr("About")) {
                LabeledContent(L10n.tr("Version"), value: live.version ?? "—")
                LabeledContent(L10n.tr("Size"), value: live.sizeBytes.map { ByteCountFormatter.string(fromByteCount: $0, countStyle: .file) } ?? "—")
                LabeledContent(L10n.tr("Package"), value: live.name)
                if let b = live.bitness { LabeledContent(L10n.tr("Architecture"), value: b) }
            }

            Section {
                Button { router.openFiles(at: "/sdcard/Android/data/\(app.name)") } label: {
                    Label(L10n.tr("Open in Files"), systemImage: "folder")
                }
                .disabled(!canOpen)
                Button { appInfo() } label: {
                    Label(L10n.tr("Android App Info"), systemImage: "info.circle")
                }
                .disabled(!canOpen)
                Button { UIPasteboard.general.string = app.name } label: {
                    Label(L10n.tr("Copy Package Name"), systemImage: "doc.on.doc")
                }
            }

            Section {
                Button(role: .destructive) { confirmUninstall = true } label: {
                    Label(L10n.tr("Uninstall"), systemImage: "trash")
                }
                .disabled(!canOpen)
            } footer: {
                if !canOpen { Text(L10n.tr("Start Android to uninstall or look inside the app.")) }
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle(live.label)
        .navigationBarTitleDisplayMode(.inline)
        .confirmationDialog(L10n.tr("Uninstall \(live.label)?"), isPresented: $confirmUninstall, titleVisibility: .visible) {
            Button(L10n.tr("Uninstall"), role: .destructive) {
                host.uninstall(app.name)
                dismiss()
            }
        } message: {
            Text(L10n.tr("Its data goes with it."))
        }
    }

    @ViewBuilder
    private var primary: some View {
        if canOpen {
            BigButton(title: L10n.tr("Open"), systemImage: "play.fill") {
                host.launch(app.name) { router.openGuest() }
            }
        } else if router.androidStarted {
            VStack(spacing: 10) {
                BigButton(title: L10n.tr("Starting Android…"), systemImage: "hourglass", prominent: false) { }
                    .disabled(true)
                if runner.bootProgress > 0 { ProgressView(value: Double(runner.bootProgress), total: 100) }
            }
        } else if guest.state == .ready {
            BigButton(title: Launcher.jitOn ? L10n.tr("Start Android and Open") : L10n.tr("Turn On JIT"), systemImage: Launcher.jitOn ? "play.fill" : "bolt.fill") {
                if Launcher.jitOn { router.pendingAndroidLaunch = app.name }
                router.startAndroid()
            }
        } else {
            HuskLink(value: LibraryRoute.androidSystem) {
                Label(L10n.tr("Get Android First"), systemImage: "arrow.down.circle")
                    .font(.system(.headline, design: .rounded))
                    .frame(maxWidth: .infinity).frame(height: 50)
                    .foregroundStyle(.white)
                    .background(Color.accentColor, in: RoundedRectangle(cornerRadius: 15, style: .continuous))
            }
            .buttonStyle(CardButtonStyle())
        }
    }

    /// Android's own page for the app: permissions, storage, force stop.
    private func appInfo() {
        let pkg = app.name
        DispatchQueue.global(qos: .userInitiated).async {
            _ = try? GuestBridge.shared.shell("am start -a android.settings.APPLICATION_DETAILS_SETTINGS -d package:\(pkg)", timeout: 30)
        }
        router.openGuest()
    }
}

// MARK: - Android itself

/// The whole Android system, as an app of its own: get it, start it, open it, and its settings.
struct AndroidSystemPage: View {
    @ObservedObject private var guest = GuestImage.shared
    @ObservedObject private var runner = QemuRunner.shared
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var router = Router.shared
    @ObservedObject private var jit = JITCoordinator.shared

    private var running: Bool { router.androidStarted }

    var body: some View {
        List {
            Section {
                PageHero(item: .android, subtitle: status) {
                    VStack(spacing: 10) {
                        primary
                        if let progress { ProgressView(value: progress) }
                    }
                }
            } footer: {
                if let note { Text(note) }
            }
            .heroRow()

            if host.isReady {
                Section {
                    Button { router.openFiles(at: FilesTab.root) } label: { Label(L10n.tr("Files"), systemImage: "folder") }
                    Button { runner.saveState(reason: "asked from the Android page") } label: {
                        Label(runner.isSavingState ? L10n.tr("Saving…") : L10n.tr("Save Android Now"), systemImage: "externaldrive.badge.checkmark")
                    }
                    .disabled(runner.isSavingState)
                } footer: {
                    Text(L10n.tr("Saving keeps Android exactly as it is, so the next start skips booting."))
                }
            }

            Section(L10n.tr("Settings")) {
                NavigationLink { PerformanceSettings() } label: { Label(L10n.tr("Performance"), systemImage: "speedometer") }
                NavigationLink { InputSettings() } label: { Label(L10n.tr("Input"), systemImage: "hand.tap") }
                NavigationLink { NetworkSettings() } label: { Label(L10n.tr("Network"), systemImage: "globe") }
                NavigationLink { SavedMachineSettings() } label: { Label(L10n.tr("Saved Machine"), systemImage: "externaldrive") }
            }

            Section {
                Text(L10n.tr("Android runs fully emulated, so it is slower than games on the translation layer. Use it for apps Husk cannot run directly, like Termux or app stores."))
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle("Android")
        .navigationBarTitleDisplayMode(.inline)
        .animation(.easeInOut(duration: 0.2), value: status)
    }

    private var status: String {
        if running {
            if host.isReady { return L10n.tr("Running") }
            return runner.setupMessage ?? (runner.bootProgress > 0 ? L10n.tr("Starting · \(runner.bootProgress)%") : L10n.tr("Starting…"))
        }
        switch guest.state {
        case .missing: return L10n.tr("Not downloaded yet")
        case .downloading(_, let received, let total):
            return L10n.tr("Downloading · \(bytes(received)) of \(total > 0 ? bytes(total) : "…")")
        case .installing: return L10n.tr("Installing…")
        case .failed: return L10n.tr("Something went wrong")
        case .ready: return Launcher.jitOn ? L10n.tr("Ready to start") : L10n.tr("Needs JIT to start")
        }
    }

    private var progress: Double? {
        if running, !host.isReady, runner.bootProgress > 0 { return Double(runner.bootProgress) / 100 }
        if !running, case .downloading(let p, _, _) = guest.state { return p }
        return nil
    }

    private var note: String? {
        guard !running else { return nil }
        switch guest.state {
        case .missing: return L10n.tr("Husk's Android runtime is about 760 MB. Android itself follows once it is installed.")
        case .failed(let message): return message
        default: return nil
        }
    }

    @ViewBuilder
    private var primary: some View {
        if running {
            if host.isReady {
                BigButton(title: L10n.tr("Open Android"), systemImage: "play.fill") { open() }
            } else {
                BigButton(title: L10n.tr("Watch It Start"), systemImage: "eye", prominent: false) { open() }
            }
        } else {
            switch guest.state {
            case .missing:
                BigButton(title: L10n.tr("Download Android"), systemImage: "arrow.down.circle") {
                    // Claim the JIT region before the download: it takes a while, and StikDebug lets go by the end of it.
                    JITBootstrap.prewarm()
                    guest.download()
                }
            case .downloading:
                BigButton(title: L10n.tr("Cancel Download"), systemImage: "xmark", prominent: false) { guest.cancel() }
            case .installing:
                ProgressView().frame(height: 50)
            case .failed:
                BigButton(title: L10n.tr("Try Again"), systemImage: "arrow.clockwise") { JITBootstrap.prewarm(); guest.download() }
            case .ready:
                if jit.busy {
                    HStack(spacing: 10) { ProgressView(); Text(jit.status ?? L10n.tr("Turning on JIT…")).foregroundStyle(.secondary) }
                        .frame(maxWidth: .infinity).frame(height: 50)
                } else {
                    BigButton(title: Launcher.jitOn ? L10n.tr("Start Android") : L10n.tr("Turn On JIT"), systemImage: Launcher.jitOn ? "power" : "bolt.fill") {
                        router.startAndroid()
                    }
                }
            }
        }
    }

    private func open() {
        UserDefaults.standard.set(Date(), forKey: "husk.android.lastOpened")
        router.openGuest()
    }

    private func bytes(_ n: Int64) -> String { ByteCountFormatter.string(fromByteCount: n, countStyle: .file) }
}

/// A URL that a sheet can be presented for.
struct IdentifiedURL: Identifiable {
    let url: URL
    var id: String { url.path }
}
