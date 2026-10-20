// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UniformTypeIdentifiers

/// APKs handed to Husk from another app: the share sheet's "Husk" (or "Copy to Husk"), or "Open in Husk" from Files.
///
/// Each arrives as a URL that may only be readable for a moment (a security-scoped file in another app's container, or a copy
/// in Documents/Inbox that iOS made), so it is copied straight away to a folder of Husk's own, and then the person is asked
/// where it goes: the Translation Layer, which runs a game on the iPhone itself, or Android. Several shared at once -- a base
/// APK and its splits -- arrive one URL at a time and are gathered into one question.
@MainActor
final class IncomingFiles: ObservableObject {
    static let shared = IncomingFiles()

    /// Files waiting for the person to say where they go.
    @Published var waiting: [URL] = []
    /// Files meant for Android, held until Android is up to take them.
    @Published private(set) var forAndroid: [URL] = []

    private var gathering: [URL] = []
    private var gatherTask: Task<Void, Never>?

    /// Where copies wait. Emptied when Husk starts, so nothing left over takes up space.
    nonisolated static var folder: URL {
        FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("Incoming", isDirectory: true)
    }

    nonisolated static func clearLeftovers() {
        try? FileManager.default.removeItem(at: folder)
    }

    static let extensions: Set<String> = ["apk", "xapk", "apkm", "apks", "obb"]

    /// Copies under way, said in one line while they last.
    @Published private(set) var preparing: String?

    /// A URL iOS opened Husk with. Anything that is not an APK or a bundle of them is left alone (husk:// links are not files).
    func receive(_ url: URL) {
        if url.isFileURL, url.pathExtension.lowercased() == "pkg" { installPS4Packages([url]); return }
        guard url.isFileURL, Self.extensions.contains(url.pathExtension.lowercased()) else { return }
        take([url]) { copies in
            self.gathering += copies
            // The pieces of a split set arrive one after another: ask once they have all come.
            self.gatherTask?.cancel()
            self.gatherTask = Task {
                try? await Task.sleep(nanoseconds: 600_000_000)
                guard !Task.isCancelled else { return }
                self.waiting += self.gathering
                self.gathering = []
            }
        }
    }

    /// Files picked inside Husk: all at once, so they are one question.
    func receive(_ urls: [URL]) {
        // A PS4 game is a folder (eboot.bin and sce_sys/): it goes to the PS4 library, not to Android.
        let ps4 = urls.filter(Self.isPS4GameFolder)
        if !ps4.isEmpty { importPS4(ps4) }
        // PS4 packages: picked themselves, or in a picked folder that is not a game folder.
        let packages = urls.filter { $0.pathExtension.lowercased() == "pkg" }
            + urls.filter { !Self.isPS4GameFolder($0) }.flatMap(Self.packages(in:))
        if !packages.isEmpty { installPS4Packages(packages) }
        let wanted = urls.filter { Self.extensions.contains($0.pathExtension.lowercased()) }
        guard !wanted.isEmpty else { return }
        take(wanted) { self.waiting += $0 }
    }

    /// An extracted PS4 game: a folder with eboot.bin, or with sce_sys/param.sfo.
    nonisolated static func isPS4GameFolder(_ url: URL) -> Bool {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        var dir: ObjCBool = false
        guard FileManager.default.fileExists(atPath: url.path, isDirectory: &dir), dir.boolValue else { return false }
        return FileManager.default.fileExists(atPath: url.appendingPathComponent("eboot.bin").path)
            || FileManager.default.fileExists(atPath: url.appendingPathComponent("sce_sys/param.sfo").path)
    }

    /// The .pkg files directly inside a folder (none when url is a file).
    nonisolated static func packages(in url: URL) -> [URL] {
        let scoped = url.startAccessingSecurityScopedResource()
        defer { if scoped { url.stopAccessingSecurityScopedResource() } }
        let items = (try? FileManager.default.contentsOfDirectory(at: url, includingPropertiesForKeys: nil, options: [.skipsHiddenFiles])) ?? []
        return items.filter { $0.pathExtension.lowercased() == "pkg" }.sorted { $0.lastPathComponent < $1.lastPathComponent }
    }

    /// Installs PS4 packages (.pkg) into Documents/PS4 Games through the PS4 module, off the main thread (they are gigabytes):
    /// a game as its title ID's folder, an update beside the game it updates as <folder>-UPDATE, where the PS4 layer looks for
    /// it. Only fake packages (fPKG) and homebrew open; a retail package is refused with a message saying so.
    private func installPS4Packages(_ files: [URL]) {
        guard PS4Store.available, let module = PS4Store.moduleURL else {
            AndroidHost.shared.say(L10n.tr("Can't install PS4 games here"), PS4Store.moduleURL == nil ? L10n.tr("This copy of Husk doesn't have the PS4 layer.") : L10n.tr("PS4 games need iOS 17.4 or later."), good: false)
            return
        }
        let progress = PackageProgress { [weak self] text in self?.preparing = text }
        preparing = L10n.tr("Installing \(files[0].lastPathComponent)…")
        Task.detached(priority: .userInitiated) {
            let fm = FileManager.default
            try? fm.createDirectory(at: PS4Store.root, withIntermediateDirectories: true)
            var installed: [String] = [], failed: [(String, String)] = []
            for (i, url) in files.enumerated() {
                let scoped = url.startAccessingSecurityScopedResource()
                defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                let name = url.deletingPathExtension().lastPathComponent
                progress.begin(files.count == 1 ? L10n.tr("Installing \(name)") : L10n.tr("Installing \(name) (\(i + 1) of \(files.count))"))
                // Unpacked, a game takes at least its package's size.
                let size = (try? url.resourceValues(forKeys: [.fileSizeKey]).fileSize).map(Int64.init) ?? 0
                if let free = try? PS4Store.root.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey]).volumeAvailableCapacityForImportantUsage,
                   free < size {
                    failed.append((name, L10n.tr("needs \(ByteCountFormatter.string(fromByteCount: size, countStyle: .file)) free")))
                    continue
                }
                // Into a hidden folder on the same volume, so the library never lists a half-installed game.
                let temp = PS4Store.root.appendingPathComponent(".installing-\(UUID().uuidString)", isDirectory: true)
                var err = [CChar](repeating: 0, count: 512)
                let box = Unmanaged.passRetained(progress)
                let ok = husk_console_install_package(module.path, url.path, temp.path, { ctx, done, total in
                    Unmanaged<PackageProgress>.fromOpaque(ctx!).takeUnretainedValue().update(done, total)
                }, box.toOpaque(), &err, err.count)
                box.release()
                guard ok else {
                    try? fm.removeItem(at: temp)
                    failed.append((name, String(cString: err)))
                    HuskLog.log("ps4", "could not install \(url.lastPathComponent): \(String(cString: err))")
                    continue
                }
                let sfo = ParamSfo.parse(from: temp.appendingPathComponent("sce_sys/param.sfo"))
                let titleID = sfo?.titleID ?? name
                let base = PS4Store.folder(of: titleID) ?? PS4Store.root.appendingPathComponent(titleID, isDirectory: true)
                let isUpdate = sfo?.category == "gp"
                let dest = isUpdate ? base.deletingLastPathComponent().appendingPathComponent(base.lastPathComponent + "-UPDATE", isDirectory: true) : base
                if isUpdate { try? fm.removeItem(at: dest) }
                guard !fm.fileExists(atPath: dest.path) else {
                    try? fm.removeItem(at: temp)
                    failed.append((sfo?.title ?? name, L10n.tr("is already installed")))
                    continue
                }
                do {
                    try fm.moveItem(at: temp, to: dest)
                    installed.append(isUpdate ? L10n.tr("\(sfo?.title ?? titleID) update") : sfo?.title ?? titleID)
                    HuskLog.log("ps4", "installed \(url.lastPathComponent) as \(dest.lastPathComponent)")
                } catch {
                    try? fm.removeItem(at: temp)
                    failed.append((name, error.localizedDescription))
                }
            }
            await MainActor.run {
                self.preparing = nil
                PS4Store.shared.reload()
                if let first = failed.first {
                    let (what, why) = first
                    AndroidHost.shared.say(L10n.tr("Could not install \(what)"), why.prefix(1).uppercased() + why.dropFirst() + (failed.count > 1 ? L10n.tr(" (and \(failed.count - 1) more)") : ""), good: false)
                } else if !installed.isEmpty {
                    AndroidHost.shared.say(installed.count == 1 ? L10n.tr("Installed \(installed[0])") : L10n.tr("Installed \(installed.count) PS4 packages"), L10n.tr("It's in your library."))
                }
            }
        }
    }

    /// Copies PS4 game folders into Documents/PS4 Games (off the main thread: they are gigabytes) and shows them in the library.
    private func importPS4(_ folders: [URL]) {
        preparing = folders.count == 1 ? L10n.tr("Importing \(folders[0].lastPathComponent)…") : L10n.tr("Importing \(folders.count) PS4 games…")
        Task.detached(priority: .userInitiated) {
            let fm = FileManager.default
            try? fm.createDirectory(at: PS4Store.root, withIntermediateDirectories: true)
            for url in folders {
                let scoped = url.startAccessingSecurityScopedResource()
                defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                let dest = PS4Store.root.appendingPathComponent(url.lastPathComponent, isDirectory: true)
                guard !fm.fileExists(atPath: dest.path) else {
                    HuskLog.log("ps4", "\(url.lastPathComponent) is already in PS4 Games")
                    continue
                }
                do {
                    try fm.copyItem(at: url, to: dest)
                    HuskLog.log("ps4", "imported \(url.lastPathComponent)")
                } catch {
                    try? fm.removeItem(at: dest)
                    HuskLog.log("ps4", "could not import \(url.lastPathComponent): \(error.localizedDescription)")
                }
            }
            await MainActor.run {
                self.preparing = nil
                PS4Store.shared.reload()
            }
        }
    }

    /// Copy files to Husk's own folder, off the main thread -- a game can be gigabytes -- then hand back the copies.
    private func take(_ urls: [URL], then: @escaping @MainActor ([URL]) -> Void) {
        preparing = urls.count == 1 ? L10n.tr("Preparing \(urls[0].lastPathComponent)…") : L10n.tr("Preparing \(urls.count) files…")
        Task.detached(priority: .userInitiated) {
            var copies: [URL] = []
            for url in urls {
                let scoped = url.startAccessingSecurityScopedResource()
                defer { if scoped { url.stopAccessingSecurityScopedResource() } }
                let dir = Self.folder.appendingPathComponent(UUID().uuidString, isDirectory: true)
                let copy = dir.appendingPathComponent(url.lastPathComponent)
                do {
                    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
                    try FileManager.default.copyItem(at: url, to: copy)
                    copies.append(copy)
                    // iOS's own copy (Documents/Inbox) is no use once Husk has one.
                    if url.path.contains("/Documents/Inbox/") { try? FileManager.default.removeItem(at: url) }
                    HuskLog.log("ui", "took in \(url.lastPathComponent)")
                } catch {
                    HuskLog.log("ui", "could not take in \(url.lastPathComponent): \(error.localizedDescription)")
                }
            }
            await MainActor.run {
                self.preparing = nil
                if !copies.isEmpty { then(copies) }
            }
        }
    }

    /// Run on the iPhone itself.
    func sendToTranslationLayer() {
        let urls = waiting
        waiting = []
        guard !urls.isEmpty else { return }
        // Moved, not copied: the copy here is Husk's own, and a game can be gigabytes.
        TranslationLayerStore.shared.add(urls, move: true)
    }

    /// Installed into Android -- now if it is up, otherwise as soon as it is.
    func sendToAndroid() {
        let urls = waiting
        waiting = []
        guard !urls.isEmpty else { return }
        forAndroid += urls
        installForAndroidIfReady()
    }

    func installForAndroidIfReady() {
        guard AndroidHost.shared.isReady, AndroidHost.shared.busy == nil, !forAndroid.isEmpty else { return }
        let urls = forAndroid
        forAndroid = []
        // An expansion file is not a package: Android's installer would refuse the whole set over it
        let obbs = urls.filter { $0.pathExtension.lowercased() == "obb" }
        if !obbs.isEmpty { HuskLog.log("ui", "not installing \(obbs.map(\.lastPathComponent)) into Android: expansion files only go with the translation layer") }
        AndroidHost.shared.install(urls.filter { $0.pathExtension.lowercased() != "obb" })
    }

    func discard() {
        for url in waiting { try? FileManager.default.removeItem(at: url.deletingLastPathComponent()) }
        waiting = []
    }
}

/// Where a shared APK goes. Two choices, each saying what it means, and a way out.
struct IncomingChooser: View {
    @ObservedObject private var incoming = IncomingFiles.shared
    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var guest = GuestImage.shared

    private var title: String {
        guard let first = incoming.waiting.first else { return "" }
        return incoming.waiting.count == 1 ? first.lastPathComponent : L10n.tr("\(incoming.waiting.count) files")
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 18) {
            VStack(alignment: .leading, spacing: 4) {
                Text(L10n.tr("Add to Husk")).font(.title2.weight(.bold))
                Text(title).font(.subheadline).foregroundStyle(.secondary).lineLimit(2)
            }

            choice(icon: "bolt.fill", title: L10n.tr("Translation Layer"),
                   detail: L10n.tr("Runs the game directly on your iPhone, without starting Android. Best for games.")) {
                incoming.sendToTranslationLayer()
            }
            choice(icon: "apps.iphone", title: "Android",
                   detail: androidDetail) {
                incoming.sendToAndroid()
            }

            Button(L10n.tr("Cancel"), role: .cancel) { incoming.discard() }
                .frame(maxWidth: .infinity)
                .padding(.top, 2)
        }
        .padding(22)
    }

    private var androidDetail: String {
        if host.isReady { return L10n.tr("Installs it into Android, where any app can run.") }
        if guest.state == .ready { return L10n.tr("Installs it into Android the next time Android starts.") }
        return L10n.tr("Installs it into Android once Android is downloaded and started.")
    }

    private func choice(icon: String, title: String, detail: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 14) {
                Image(systemName: icon)
                    .font(.system(size: 18, weight: .semibold))
                    .foregroundStyle(Color.accentColor)
                    .frame(width: 44, height: 44)
                    .background(Color.accentColor.opacity(0.14), in: RoundedRectangle(cornerRadius: 12, style: .continuous))
                VStack(alignment: .leading, spacing: 2) {
                    Text(title).font(.headline).foregroundStyle(.primary)
                    Text(detail).font(.subheadline).foregroundStyle(.secondary)
                        .fixedSize(horizontal: false, vertical: true)
                }
                Spacer(minLength: 0)
                Image(systemName: "chevron.right").font(.footnote.weight(.semibold)).foregroundStyle(.tertiary)
            }
            .padding(14)
            .background(Color(uiColor: .secondarySystemBackground), in: RoundedRectangle(cornerRadius: 16, style: .continuous))
        }
        .buttonStyle(CardButtonStyle())
    }
}

/// A package install's progress, from the extractor's worker threads to the busy strip: whole percents only, on the main actor.
private final class PackageProgress: @unchecked Sendable {
    private let lock = NSLock()
    private var label = ""
    private var shown = -1
    private let show: @MainActor (String) -> Void

    init(show: @escaping @MainActor (String) -> Void) { self.show = show }

    func begin(_ text: String) {
        lock.lock(); label = text; shown = -1; lock.unlock()
        let show = self.show
        Task { @MainActor in show(text + "…") }
    }

    func update(_ done: UInt64, _ total: UInt64) {
        let pct = total > 0 ? Int(min(done, total) * 100 / total) : 0
        lock.lock()
        guard pct > shown else { lock.unlock(); return }
        shown = pct
        let text = "\(label)… \(pct)%"
        lock.unlock()
        let show = self.show
        Task { @MainActor in show(text) }
    }
}
