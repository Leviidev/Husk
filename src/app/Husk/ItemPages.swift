// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

// MARK: - the top of every page

/// An item's page opens on its artwork, large: the name and its platform and status as chips over the bottom of it, then the
/// one thing to do -- Play, Open, Start -- as a big button, and a row of facts (how it runs, when it was last played).
private struct PageHero<Action: View>: View {
    let item: LibraryItem
    let subtitle: String
    var facts: [(symbol: String, text: String)] = []
    @ViewBuilder var action: Action

    /// playstation.logo is iOS 16+; a missing symbol on iOS 15 can blank the chip row.
    private static var psSymbol: String {
        UIImage(systemName: "playstation.logo") != nil ? "playstation.logo" : "gamecontroller.fill"
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            ZStack(alignment: .bottomLeading) {
                ItemBackdrop(item: item, pixels: 1400, fallback: Console.card)
                LinearGradient(colors: [.clear, .black.opacity(0.8)], startPoint: .center, endPoint: .bottom)
                VStack(alignment: .leading, spacing: 10) {
                    HStack(alignment: .bottom, spacing: 12) {
                        if item.artworkPath == nil {
                            ItemIcon(item: item, size: 68).shadow(color: .black.opacity(0.35), radius: 10, y: 4)
                        }
                        Text(item.title)
                            .font(.system(size: 30, weight: .bold))
                            .foregroundStyle(.white)
                            .lineLimit(2)
                            .minimumScaleFactor(0.7)
                    }
                    HStack(spacing: 8) {
                        PageChip(text: item.platformShort, symbol: item.isPS4 ? Self.psSymbol : "iphone", tint: .white)
                        if let st = item.status {
                            PageChip(text: st == .plays ? "Works Great" : st.label, symbol: "checkmark.seal.fill", tint: st.color)
                        }
                    }
                }
                .padding(16)
            }
            .frame(height: 260)
            .clipShape(RoundedRectangle(cornerRadius: 22, style: .continuous))
            action
            if !facts.isEmpty {
                HStack(alignment: .top) {
                    ForEach(Array(facts.enumerated()), id: \.offset) { _, f in
                        VStack(spacing: 6) {
                            Image(systemName: f.symbol).font(.system(size: 17, weight: .semibold)).foregroundStyle(Console.textDim)
                            Text(f.text).font(.system(size: 12, weight: .medium)).foregroundStyle(Console.text.opacity(0.85))
                                .multilineTextAlignment(.center).lineLimit(2)
                        }
                        .frame(maxWidth: .infinity)
                    }
                }
                .padding(.vertical, 4)
            }
            if !subtitle.isEmpty {
                Text(subtitle).font(.subheadline).foregroundStyle(Console.textDim)
            }
        }
        .padding(.vertical, 4)
    }
}

/// A chip on an item's artwork: its platform, how it runs.
private struct PageChip: View {
    let text: String
    let symbol: String
    let tint: Color
    var body: some View {
        Label(text, systemImage: symbol)
            .font(.system(size: 12, weight: .semibold))
            .foregroundStyle(tint)
            .padding(.horizontal, 9).frame(height: 24)
            .background(.black.opacity(0.45), in: Capsule())
            .overlay(Capsule().stroke(tint.opacity(0.35), lineWidth: 0.5))
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
    @State private var reporting: CompatSubject?
    /// JIT-less mode converting and signing this game's libraries before it starts
    @State private var preparing: String?
    @State private var prepareFailed: String?
    @State private var confirmQuit = false

    private var runs: Bool { app.report?.canRun == true }

    /// Another session is still in this process: Play would have to unload it, which is what made games slow and crash.
    private var blockedBy: String? {
        if Router.shared.androidStarted { return "Android" }
        let apk = app.apks.first
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }), loaded != apk else { return nil }
        return Self.name(ofLoaded: loaded)
    }

    private var compatSubject: CompatSubject? {
        app.packageName.map { CompatSubject(package: $0, name: app.label, isGame: LibraryItem.game(app).isGame, iconPath: app.iconPath) }
    }

    private var preflight: AppDiagnosis? { AppDiagnosis.preflight(app) }

    /// Under Play: what runs it, how long it has been played, and when last.
    private var gameFacts: [(symbol: String, text: String)] {
        var f: [(symbol: String, text: String)] = [("cpu", app.report?.runnerName ?? "Translation Layer")]
        let t = Playtime.seconds(LibraryItem.game(app).id)
        f.append(("gamecontroller", t > 0 ? Playtime.text(t) + " played" : "Not played yet"))
        if let d = app.lastPlayed {
            let r = RelativeDateTimeFormatter(); r.unitsStyle = .short
            f.append(("clock", "Last played\n" + r.localizedString(for: d, relativeTo: Date())))
        }
        return f
    }

    /// When it was last played is in the facts already: only a warning here.
    private var subtitle: String { runs ? "" : "May not run on this iPhone" }

    var body: some View {
        HuskPageList {
            Section {
                PageHero(item: .game(app), subtitle: subtitle, facts: gameFacts) { primary }
                    .heroRow()
            } footer: {
                if isGeodeLauncher {
                    Text("This is Geode's Android launcher. On Husk, Geode runs inside Geometry Dash instead: turn it on here "
                       + "(or in Geometry Dash's settings, under Mods), then start Geometry Dash and use Geode's button on its "
                       + "main menu to get mods. This APK supplies a library Geode needs, so keep it.")
                } else if !runs {
                    Text("This APK has no ARM code Husk can run, so it will probably not start.")
                }
            }

            // what the files say before it starts: a damaged APK, the wrong CPU, a missing piece, protection that refuses Husk
            if let d = preflight {
                Section {
                    AppDiagnosisCard(diagnosis: d)
                        .listRowInsets(EdgeInsets(top: 4, leading: 4, bottom: 4, trailing: 4))
                }
            }
            // why it stopped last time, until it plays again
            if let d = GameStatusStore.shared.diagnosis(app.id), d != preflight {
                Section("Last Run") {
                    AppDiagnosisCard(diagnosis: d, showsEvidence: true)
                        .listRowInsets(EdgeInsets(top: 4, leading: 4, bottom: 4, trailing: 4))
                }
            }

            if let compatSubject, !isGeodeLauncher {
                CompatibilitySection(subject: compatSubject, reporting: $reporting)
            }

            if !ShowcaseStore.shared.pictures(for: app.packageName).isEmpty {
                Section("Screenshots") {
                    ShowcaseGallery(package: app.packageName)
                        .listRowInsets(EdgeInsets(top: 10, leading: 12, bottom: 10, trailing: 12))
                }
            }

            Section {
                HuskLink(value: LibraryRoute.gameSettings(app.id)) {
                    Label("Game Settings", systemImage: "slider.horizontal.3")
                }
                HuskLink(value: LibraryRoute.gameReport(app.id)) {
                    Label("Technical Details", systemImage: "cpu")
                }
            }

            if !isGeodeLauncher {
                Section {
                    Button { backUp() } label: { Label("Back Up Saves", systemImage: "square.and.arrow.up") }
                        .disabled(savesBusy || !SaveBackup.hasData(app))
                    Button { pickRestore() } label: { Label("Restore Saves", systemImage: "clock.arrow.circlepath") }
                        .disabled(savesBusy || loadedNow)
                } header: {
                    Text("Saves")
                } footer: {
                    if let savesMessage { Text(savesMessage) }
                    else if loadedNow { Text("\(app.label) has been started in this run of Husk. Close Husk and open it again to restore saves.") }
                    else { Text("A backup is a .zip of everything \(app.label) has saved in Husk, to keep in Files or move to another device.") }
                }
            }

            Section("About") {
                LabeledContent("Runs With", value: app.report?.runnerName ?? "Unknown")
                if let st = GameStatusStore.shared.status(app.id) {
                    LabeledContent("Last Result") {
                        Label(st.result.label, systemImage: st.result.symbol).foregroundStyle(st.result.color)
                    }
                }
                LabeledContent("Last Played", value: app.lastPlayed.map { $0.formatted(date: .abbreviated, time: .shortened) } ?? "Never")
                LabeledContent("Size", value: fileBytes(app.apks))
            }

            Section {
                Button(role: .destructive) { confirmRemove = true } label: {
                    Label("Remove Game", systemImage: "trash")
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
        .confirmationDialog("Replace \(app.label)'s saves?", isPresented: Binding(get: { confirmRestore != nil }, set: { if !$0 { confirmRestore = nil } }),
                            titleVisibility: .visible) {
            Button("Replace", role: .destructive) { if let url = confirmRestore { restore(url) } }
        } message: {
            Text("What \(app.label) has saved now is replaced by the backup. Back up first if you want to keep it.")
        }
        .onReceive(NotificationCenter.default.publisher(for: TLUnityUIView.huskStopPlaying)) { _ in playing = false }
        .alert("JIT-less mode could not prepare this game", isPresented: Binding(get: { prepareFailed != nil }, set: { if !$0 { prepareFailed = nil } })) {
            Button("OK", role: .cancel) { }
        } message: {
            Text(prepareFailed ?? "")
        }
        .confirmationDialog("Close Husk?", isPresented: $confirmQuit, titleVisibility: .visible) {
            Button("Close Husk") {
                Router.shared.switchTo(app.id)
                CrashReport.gameEnded()
                HuskLog.flushNow()
                exit(0)
            }
        } message: {
            Text("\(blockedBy ?? "The other game") is still loaded and only one game can run per session. Husk closes; open it again and \(app.label) starts by itself.")
        }
        .confirmationDialog("Remove \(app.label)?", isPresented: $confirmRemove, titleVisibility: .visible) {
            Button("Remove", role: .destructive) {
                dismiss()
                store.remove(app)
            }
        } message: {
            Text("The game and everything it saved here are deleted from Husk.")
        }
        .id(jit.attachGeneration)
        .onAppear {
            // Kept until JIT is on: this page is rebuilt when it attaches (.id above), and plays then.
            guard Router.shared.autoPlay == app.id, blockedBy == nil, Launcher.jitOn || JITLess.enabled else { return }
            Router.shared.autoPlay = nil
            play()
        }
    }

    /// Play: in JIT-less mode the game's libraries are converted and signed first (once); with Geode on, whatever it is missing
    /// (a newer release, its resources) is fetched first.
    private func play() {
        if JITLess.enabled, SigningCertificate.credentials() != nil, preparing == nil {
            preparing = "Preparing for JIT-less mode…"
            Task {
                do {
                    try await JITLess.prepare(apks: app.apks) { text in Task { @MainActor in preparing = text } }
                    preparing = nil
                    playPrepared()
                } catch {
                    preparing = nil
                    prepareFailed = error.localizedDescription
                }
            }
            return
        }
        playPrepared()
    }

    private func playPrepared() {
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
            AppWindows.shared.close()
            playing = true
        }
    }

    /// This game's engine is loaded in this run of Husk, so its files may be open: no restoring under it.
    private var loadedNow: Bool {
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }) else { return false }
        return loaded == app.apks.first
    }

    fileprivate static func name(ofLoaded loaded: String) -> String {
        if loaded.hasPrefix("ps4:") {
            return PS4Store.shared.game(String(loaded.dropFirst(4)))?.title ?? "Another game"
        }
        if let other = TranslationLayerStore.shared.apps.first(where: { $0.apks.first == loaded }) {
            return other.label
        }
        return "Another game"
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
                case .success(let n): savesMessage = "Restored \(n) file\(n == 1 ? "" : "s") from \(url.lastPathComponent)."
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
                BigButton(title: TLAppSettings.load(gd.id).geode ? "Geode Is On for Geometry Dash" : "Turn On Geode for Geometry Dash",
                          systemImage: "puzzlepiece.extension.fill", prominent: !TLAppSettings.load(gd.id).geode) {
                    var s = TLAppSettings.load(gd.id)
                    s.geode = true
                    s.save(gd.id)
                    Task { await GeodeSupport.shared.prepare(gd) }
                }
            } else {
                BigButton(title: "Add Geometry Dash First", systemImage: "plus", prominent: false) { }.disabled(true)
            }
        } else if let preparing {
            HStack(spacing: 10) { ProgressView(); Text(preparing).foregroundStyle(.secondary).lineLimit(1) }
                .frame(maxWidth: .infinity).frame(height: 50)
        } else if blockedBy != nil {
            BigButton(title: "Close Husk to Play", systemImage: "xmark.circle", prominent: false) { confirmQuit = true }
        } else if !Launcher.jitOn && !JITLess.enabled {
            if jit.busy {
                HStack(spacing: 10) { ProgressView(); Text(jit.status ?? "Turning on JIT…").foregroundStyle(.secondary) }
                    .frame(maxWidth: .infinity).frame(height: 50)
            } else {
                BigButton(title: "Turn On JIT to Play", systemImage: "bolt.fill", action: turnOnJIT)
            }
        } else {
            BigButton(title: runs ? "Play" : "Try to Run", systemImage: "play.fill", prominent: runs) { play() }
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

    /// QEMU Android, or another game, still holds this process.
    private var blockedBy: String? {
        if Router.shared.androidStarted { return "Android" }
        guard let loaded = husk_native_loaded_apk().map({ String(cString: $0) }), loaded != game.libraryID else { return nil }
        return GamePage.name(ofLoaded: loaded)
    }

    private func playPS4() {
        playing = true
    }

    /// The same facts as an Android game's: version, how long it has been played, and when last.
    private var facts: [(symbol: String, text: String)] {
        var f: [(symbol: String, text: String)] = [("cpu", "PS4 layer" + (game.version.map { "\nv\($0)" } ?? ""))]
        let t = Playtime.seconds(item.id)
        f.append(("gamecontroller", t > 0 ? Playtime.text(t) + " played" : "Not played yet"))
        if let d = game.lastPlayed {
            let r = RelativeDateTimeFormatter(); r.unitsStyle = .short
            f.append(("clock", "Last played\n" + r.localizedString(for: d, relativeTo: Date())))
        }
        return f
    }

    var body: some View {
        HuskPageList {
            PageHero(item: item, subtitle: "", facts: facts) { primary }
                .heroRow()
            if !PS4Store.available {
                Section {
                    Text(PS4Store.moduleURL == nil
                         ? "This build of Husk does not include the PS4 layer."
                         : "PS4 games need iOS 17.4 or later.")
                        .foregroundStyle(.secondary)
                }
            }
            Section("About") {
                LabeledContent("Runs With", value: "Husk's PS4 layer")
                LabeledContent("Title ID", value: game.titleID)
                if let v = game.version { LabeledContent("Version", value: v) }
                LabeledContent("Last Played", value: game.lastPlayed.map { $0.formatted(date: .abbreviated, time: .shortened) } ?? "Never")
                LabeledContent("Folder", value: "PS4 Games/" + game.folder.lastPathComponent)
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle(game.title)
        .navigationBarTitleDisplayMode(.inline)
        .fullScreenCover(isPresented: $playing) { PS4PlayView(game: game) }
        .confirmationDialog("Close Husk?", isPresented: $confirmQuit, titleVisibility: .visible) {
            Button("Close Husk") {
                Router.shared.switchTo(game.libraryID)
                CrashReport.gameEnded()
                HuskLog.flushNow()
                exit(0)
            }
        } message: {
            Text("\(blockedBy ?? "The other game") is still loaded and only one game can run per session. Husk closes; open it again and \(game.title) starts by itself.")
        }
        .id(jit.attachGeneration)
        .onAppear {
            guard Router.shared.autoPlay == game.libraryID, Launcher.jitOn, blockedBy == nil, PS4Store.available else { return }
            Router.shared.autoPlay = nil
            playPS4()
        }
    }

    @ViewBuilder
    private var primary: some View {
        if !PS4Store.available {
            BigButton(title: "Can't Play Here", systemImage: "xmark.octagon", prominent: false) { }.disabled(true)
        } else if blockedBy != nil {
            BigButton(title: "Close Husk to Play", systemImage: "xmark.circle", prominent: false) { confirmQuit = true }
        } else if !Launcher.jitOn {
            if jit.busy {
                HStack(spacing: 10) { ProgressView(); Text(jit.status ?? "Turning on JIT…").foregroundStyle(.secondary) }
                    .frame(maxWidth: .infinity).frame(height: 50)
            } else {
                BigButton(title: "Turn On JIT to Play", systemImage: "bolt.fill", action: turnOnJIT)
            }
        } else {
            BigButton(title: "Play", systemImage: "play.fill") { playPS4() }
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
        HuskPageList {
            Section {
                PageHero(item: item, subtitle: item.usedText ?? "Runs inside Android") { primary }
                    .heroRow()
            } footer: {
                if router.pendingAndroidLaunch == app.name, !host.isReady {
                    Text("\(live.label) opens by itself once Android has started.")
                }
            }

            if !ShowcaseStore.shared.pictures(for: app.name).isEmpty {
                Section("Screenshots") {
                    ShowcaseGallery(package: app.name)
                        .listRowInsets(EdgeInsets(top: 10, leading: 12, bottom: 10, trailing: 12))
                }
            }

            Section("About") {
                LabeledContent("Version", value: live.version ?? "—")
                LabeledContent("Size", value: live.sizeBytes.map { ByteCountFormatter.string(fromByteCount: $0, countStyle: .file) } ?? "—")
                LabeledContent("Package", value: live.name)
                if let b = live.bitness { LabeledContent("Architecture", value: b) }
            }

            Section {
                Button { router.openFiles(at: "/sdcard/Android/data/\(app.name)") } label: {
                    Label("Open in Files", systemImage: "folder")
                }
                .disabled(!canOpen)
                Button { appInfo() } label: {
                    Label("Android App Info", systemImage: "info.circle")
                }
                .disabled(!canOpen)
                Button { UIPasteboard.general.string = app.name } label: {
                    Label("Copy Package Name", systemImage: "doc.on.doc")
                }
            }

            Section {
                Button(role: .destructive) { confirmUninstall = true } label: {
                    Label("Uninstall", systemImage: "trash")
                }
                .disabled(!canOpen)
            } footer: {
                if !canOpen { Text("Start Android to uninstall or look inside the app.") }
            }
        }
        .listStyle(.insetGrouped)
        .navigationTitle(live.label)
        .navigationBarTitleDisplayMode(.inline)
        .confirmationDialog("Uninstall \(live.label)?", isPresented: $confirmUninstall, titleVisibility: .visible) {
            Button("Uninstall", role: .destructive) {
                host.uninstall(app.name)
                dismiss()
            }
        } message: {
            Text("Its data goes with it.")
        }
    }

    @ViewBuilder
    private var primary: some View {
        if canOpen {
            BigButton(title: "Open", systemImage: "play.fill") {
                host.launch(app.name) { router.openGuest() }
            }
        } else if router.androidStarted {
            VStack(spacing: 10) {
                BigButton(title: "Starting Android…", systemImage: "hourglass", prominent: false) { }
                    .disabled(true)
                if runner.bootProgress > 0 { ProgressView(value: Double(runner.bootProgress), total: 100) }
            }
        } else if guest.state == .ready {
            BigButton(title: Launcher.jitOn ? "Start Android and Open" : "Turn On JIT", systemImage: Launcher.jitOn ? "play.fill" : "bolt.fill") {
                if Launcher.jitOn { router.pendingAndroidLaunch = app.name }
                router.startAndroid()
            }
        } else {
            HuskLink(value: LibraryRoute.androidSystem) {
                Label("Get Android First", systemImage: "arrow.down.circle")
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
        HuskPageList {
            Section {
                PageHero(item: .android, subtitle: status) {
                    VStack(spacing: 10) {
                        primary
                        if let progress { ProgressView(value: progress) }
                    }
                }
                .heroRow()
            } footer: {
                if let note { Text(note) }
            }

            if host.isReady {
                Section {
                    Button { router.openFiles(at: FilesTab.root) } label: { Label("Files", systemImage: "folder") }
                    Button { runner.saveState(reason: "asked from the Android page") } label: {
                        Label(runner.isSavingState ? "Saving…" : "Save Android Now", systemImage: "externaldrive.badge.checkmark")
                    }
                    .disabled(runner.isSavingState)
                } footer: {
                    Text("Saving keeps Android exactly as it is, so the next start skips booting.")
                }
            }

            Section("Settings") {
                NavigationLink { PerformanceSettings() } label: { Label("Performance", systemImage: "speedometer") }
                NavigationLink { InputSettings() } label: { Label("Input", systemImage: "hand.tap") }
                NavigationLink { NetworkSettings() } label: { Label("Network", systemImage: "globe") }
                NavigationLink { SavedMachineSettings() } label: { Label("Saved Machine", systemImage: "externaldrive") }
            }

        }
        .listStyle(.insetGrouped)
        .navigationTitle("Android")
        .navigationBarTitleDisplayMode(.inline)
        .animation(.easeInOut(duration: 0.2), value: status)
    }

    private var status: String {
        if running {
            if host.isReady { return "Running" }
            return runner.setupMessage ?? (runner.bootProgress > 0 ? "Starting · \(runner.bootProgress)%" : "Starting…")
        }
        switch guest.state {
        case .missing: return "Not downloaded yet"
        case .downloading(_, let received, let total):
            return "Downloading · \(bytes(received)) of \(total > 0 ? bytes(total) : "…")"
        case .installing: return "Installing…"
        case .failed: return "Something went wrong"
        case .ready: return Launcher.jitOn ? "Ready to start" : "Needs JIT to start"
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
        case .missing: return "Husk's Android runtime is about 760 MB. Android itself follows once it is installed."
        case .failed(let message): return message
        default: return nil
        }
    }

    @ViewBuilder
    private var primary: some View {
        if running {
            if host.isReady {
                BigButton(title: "Open Android", systemImage: "play.fill") { open() }
            } else {
                BigButton(title: "Watch It Start", systemImage: "eye", prominent: false) { open() }
            }
        } else {
            switch guest.state {
            case .missing:
                BigButton(title: "Download Android", systemImage: "arrow.down.circle") {
                    // Claim the JIT region before the download: it takes a while, and StikDebug lets go by the end of it.
                    JITBootstrap.prewarm()
                    guest.download()
                }
            case .downloading:
                BigButton(title: "Cancel Download", systemImage: "xmark", prominent: false) { guest.cancel() }
            case .installing:
                ProgressView().frame(height: 50)
            case .failed:
                BigButton(title: "Try Again", systemImage: "arrow.clockwise") { JITBootstrap.prewarm(); guest.download() }
            case .ready:
                if jit.busy {
                    HStack(spacing: 10) { ProgressView(); Text(jit.status ?? "Turning on JIT…").foregroundStyle(.secondary) }
                        .frame(maxWidth: .infinity).frame(height: 50)
                } else {
                    BigButton(title: Launcher.jitOn ? "Start Android" : "Turn On JIT", systemImage: Launcher.jitOn ? "power" : "bolt.fill") {
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
