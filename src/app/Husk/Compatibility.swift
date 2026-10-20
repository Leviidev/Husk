// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Husk's compatibility list (antiweb.lol): how each app runs for everyone, next to how it ran on this iPhone (GameStatus).
///
/// The list is fetched now and then and kept on disk, so it shows offline and costs nothing at launch. Reports go to the
/// same site: how a game ran, sent from Husk with the device, iOS, build and JIT method filled in.
@MainActor
final class CompatibilityStore: ObservableObject {
    static let shared = CompatibilityStore()

    /// The site, then its Render address for when the domain does not answer. A different one can be put first for testing
    /// (defaults write com.husk.app husk.compatURL http://…).
    static var sites: [URL] {
        let custom = UserDefaults.standard.string(forKey: "husk.compatURL").flatMap(URL.init(string:))
        return [custom, URL(string: "https://antiweb.lol"), URL(string: "https://antiweb-website.onrender.com")].compactMap { $0 }
    }
    /// The one that answered last, for links.
    static var site: URL { shared.workingSite ?? sites[0] }
    @Published private(set) var workingSite: URL?

    /// The request on each site in turn until one answers with a status below 500 (a 4xx is the site's answer, not an outage).
    private func call(_ path: String, _ prepare: (inout URLRequest) -> Void = { _ in }) async throws -> (Data, HTTPURLResponse) {
        var last: Error = URLError(.cannotConnectToHost)
        for site in Self.sites {
            var request = URLRequest(url: site.appendingPathComponent(path))
            request.timeoutInterval = 15
            prepare(&request)
            do {
                let (data, response) = try await URLSession.shared.data(for: request)
                guard let http = response as? HTTPURLResponse, http.statusCode < 500 else { throw URLError(.badServerResponse) }
                if workingSite != site { workingSite = site; HuskLog.log("compat", "using \(site.host ?? site.absoluteString)") }
                return (data, http)
            } catch {
                HuskLog.log("compat", "\(site.host ?? "?") did not answer: \(error.localizedDescription)")
                last = error
            }
        }
        throw last
    }
    /// Whether Husk asks how a game ran after playing it (Settings).
    static let askKey = "husk.compatAsk"

    enum Status: String, Codable {
        case works, opens, broken, protected
        var label: String {
            switch self {
            case .works: return L10n.tr("Works")
            case .opens: return L10n.tr("Opens")
            case .broken: return L10n.tr("Doesn\u{2019}t Work")
            case .protected: return L10n.tr("Protected")
            }
        }
        var symbol: String {
            switch self {
            case .works: return "checkmark.circle.fill"
            case .opens: return "circle.lefthalf.filled"
            case .broken: return "xmark.circle.fill"
            case .protected: return "lock.fill"
            }
        }
        var color: Color {
            switch self {
            case .works: return .green
            case .opens: return .orange
            case .broken: return .red
            case .protected: return .secondary
            }
        }
        var explanation: String {
            switch self {
            case .works: return L10n.tr("Runs and plays or works normally.")
            case .opens: return L10n.tr("Starts and shows its screens, but has not been fully tested or has problems.")
            case .broken: return L10n.tr("Crashes or closes while starting.")
            case .protected: return L10n.tr("Refuses to run outside a real Android phone (anti-cheat, anti-tamper or DRM). Husk does not get around that.")
            }
        }
    }

    struct Entry: Codable {
        let package: String
        let name: String
        let icon: String?
        let official: Official?
        let community: Community
        struct Official: Codable { let status: Status; let note: String }
        struct Community: Codable { let reports: Int; let counts: [String: Int] }

        /// The official result when there is one, else what most people reported.
        var status: Status? {
            if let official { return official.status }
            return community.counts.filter { $0.value > 0 }.max { $0.value < $1.value }.flatMap { Status(rawValue: $0.key) }
        }
    }

    @Published private(set) var entries: [String: Entry] = [:]
    @Published private(set) var updated: Date?
    private var fetching = false

    private static var cacheFile: URL {
        FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("compatibility.json")
    }

    private init() {
        if let data = try? Data(contentsOf: Self.cacheFile), let list = try? JSONDecoder().decode([Entry].self, from: data) {
            entries = Dictionary(list.map { ($0.package, $0) }, uniquingKeysWith: { a, _ in a })
            updated = (try? FileManager.default.attributesOfItem(atPath: Self.cacheFile.path)[.modificationDate]) as? Date
        }
    }

    func entry(_ package: String?) -> Entry? { package.flatMap { entries[$0] } }

    /// The list again when the copy on disk is older than a few hours (or `force`).
    func refresh(force: Bool = false) async {
        guard !fetching, force || updated.map({ Date().timeIntervalSince($0) > 6 * 3600 }) ?? true else { return }
        fetching = true
        defer { fetching = false }
        struct Response: Decodable { let apps: [Entry] }
        do {
            let (data, response) = try await call("api/apps")
            guard response.statusCode == 200 else { throw URLError(.badServerResponse) }
            let list = try JSONDecoder().decode(Response.self, from: data).apps
            entries = Dictionary(list.map { ($0.package, $0) }, uniquingKeysWith: { a, _ in a })
            updated = Date()
            try? JSONEncoder().encode(list).write(to: Self.cacheFile, options: .atomic)
            HuskLog.log("compat", "list updated: \(list.count) apps")
        } catch {
            HuskLog.log("compat", "list not updated: \(error.localizedDescription)")
        }
    }

    // MARK: reports

    /// What Husk fills in on every report.
    static var device: String {
        var info = utsname()
        uname(&info)
        let machine = withUnsafeBytes(of: &info.machine) { String(decoding: $0.prefix { $0 != 0 }, as: UTF8.self) }
        return machine.isEmpty ? UIDevice.current.model : machine
    }
    static var jitMethod: String {
        switch JITCoordinator.shared.resolvedMethod {
        case .builtIn: return L10n.tr("StikJIT (built in)")
        case .stikDebug: return "StikDebug"
        case .trollStore: return "TrollStore"
        case .jailbreak: return L10n.tr("Jailbreak")
        case .automatic: return L10n.tr("Other")
        }
    }
    static var build: String { (Bundle.main.infoDictionary?["CFBundleVersion"] as? String) ?? "" }

    /// The app's launcher icon as a small PNG, for a first report on an app the list has no icon for.
    static func iconPNG(_ path: String?) -> String? {
        guard let path, let image = UIImage(contentsOfFile: path) else { return nil }
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        let size = CGSize(width: 128, height: 128)
        let png = UIGraphicsImageRenderer(size: size, format: format).pngData { _ in image.draw(in: CGRect(origin: .zero, size: size)) }
        return png.count <= 60_000 ? png.base64EncodedString() : nil
    }

    func send(package: String, name: String, isGame: Bool, status: Status, notes: String, author: String, iconPath: String? = nil,
              attachLog: Bool = false) async throws {
        var body: [String: String] = [
            "package": package, "name": name, "type": isGame ? "game" : "app", "status": status.rawValue,
            "device": Self.device, "ios": UIDevice.current.systemVersion, "build": Self.build, "jit": Self.jitMethod,
            "notes": notes, "author": author,
        ]
        if body["author"]?.isEmpty == true { body["author"] = nil }
        if entry(package)?.icon == nil, let icon = Self.iconPNG(iconPath) { body["icon"] = icon }
        if attachLog, let log = Self.logTail() { body["log"] = log }
        let payload = try JSONSerialization.data(withJSONObject: body)
        let (data, http) = try await call("api/reports") {
            $0.httpMethod = "POST"
            $0.setValue("application/json", forHTTPHeaderField: "Content-Type")
            $0.httpBody = payload
            $0.timeoutInterval = 20
        }
        guard (200..<300).contains(http.statusCode) else {
            let message = (try? JSONSerialization.jsonObject(with: data) as? [String: Any])?["error"] as? String
            throw NSError(domain: "Compatibility", code: 1, userInfo: [NSLocalizedDescriptionKey: message ?? L10n.tr("The compatibility list could not be reached.")])
        }
        HuskLog.log("compat", "reported \(package): \(status.rawValue)")
        markAsked(package)
        await refresh(force: true)
    }

    /// The end of Husk's log, as the log export shares it (husk.log): at most the last 400,000 bytes, cut on a character boundary.
    static func logTail(limit: Int = 400_000) -> String? {
        guard let handle = try? FileHandle(forReadingFrom: HuskLog.logFileURL) else { return nil }
        defer { try? handle.close() }
        let size = (try? handle.seekToEnd()) ?? 0
        try? handle.seek(toOffset: size > UInt64(limit) ? size - UInt64(limit) : 0)
        guard let data = try? handle.readToEnd(), !data.isEmpty else { return nil }
        var start = data.startIndex
        // Not inside a UTF-8 sequence: skip continuation bytes (10xxxxxx).
        while start < data.endIndex, data[start] & 0xC0 == 0x80 { start += 1 }
        return String(decoding: data[start...], as: UTF8.self)
    }

    // Ask once per game per Husk build: a new build can change how a game runs.
    private func askedKey(_ package: String) -> String { "husk.compatAsked.\(package)" }
    func shouldAsk(_ package: String?) -> Bool {
        guard let package, UserDefaults.standard.object(forKey: Self.askKey) as? Bool ?? true else { return false }
        return UserDefaults.standard.string(forKey: askedKey(package)) != Self.build
    }
    func markAsked(_ package: String) { UserDefaults.standard.set(Self.build, forKey: askedKey(package)) }

    func pageURL(_ package: String) -> URL { (workingSite ?? Self.sites[0]).appendingPathComponent("app").appendingPathComponent(package) }
}

/// What a report is about, for the sheet.
struct CompatSubject: Identifiable {
    let package: String
    let name: String
    let isGame: Bool
    /// The icon Husk shows for it, sent along when the list has none.
    var iconPath: String? = nil
    /// Filled in from a crash: it did not work, and why.
    var crash: String? = nil
    /// After playing, rather than from the page's button.
    var afterPlaying = false
    var id: String { package }
}

/// The game page's section: how it runs for everyone, and a way to add how it ran here.
struct CompatibilitySection: View {
    let subject: CompatSubject
    @Binding var reporting: CompatSubject?
    @ObservedObject private var compat = CompatibilityStore.shared

    var body: some View {
        let entry = compat.entry(subject.package)
        Section {
            if let entry, let status = entry.status {
                HStack(spacing: 12) {
                    Image(systemName: status.symbol).font(.title3).foregroundStyle(status.color)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(status.label).font(.body.weight(.semibold))
                        Text(entry.official?.note.isEmpty == false ? entry.official!.note : status.explanation)
                            .font(.footnote).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                    }
                }
                .padding(.vertical, 2)
                if entry.community.reports > 0 {
                    LabeledContent(L10n.tr("Player Reports"), value: "\(entry.community.reports)")
                }
            } else {
                Text(L10n.tr("Not on the list yet. Be the first to say how it runs."))
                    .font(.footnote).foregroundStyle(.secondary)
            }
            Button { reporting = subject } label: { Label(L10n.tr("Report How It Ran"), systemImage: "square.and.pencil") }
            Link(destination: compat.pageURL(subject.package)) { Label(L10n.tr("Open on antiweb.lol"), systemImage: "safari") }
        } header: {
            Text(L10n.tr("Compatibility"))
        } footer: {
            if entry?.official != nil { Text(L10n.tr("Tested by the Husk team on an iPhone.")) }
        }
    }
}

/// The report: works, opens or does not work, and a note. Everything else is filled in.
struct CompatReportSheet: View {
    let subject: CompatSubject
    @Environment(\.dismiss) private var dismiss
    @AppStorage("husk.compatAuthor") private var author = ""
    @State private var status: CompatibilityStore.Status?
    @State private var notes = ""
    @State private var sending = false
    @State private var error: String?
    @State private var sent = false
    @AppStorage("husk.compatAttachLog") private var attachLog = true

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    ForEach([CompatibilityStore.Status.works, .opens, .broken], id: \.self) { s in
                        Button { status = s } label: {
                            HStack {
                                Label(s == .opens ? L10n.tr("Opens, but Has Problems") : s.label, systemImage: s.symbol)
                                    .foregroundStyle(.primary)
                                Spacer()
                                if status == s { Image(systemName: "checkmark").foregroundStyle(.tint) }
                            }
                        }
                    }
                } header: {
                    Text(subject.afterPlaying ? L10n.tr("How did \(subject.name) run?") : L10n.tr("How does \(subject.name) run?"))
                }
                Section(L10n.tr("Notes")) {
                    if #available(iOS 16, *) {
                        TextField(L10n.tr("What worked, what didn\u{2019}t, how far you got"), text: $notes, axis: .vertical)
                            .lineLimit(3...8)
                    } else {
                        TextEditor(text: $notes).frame(minHeight: 80)
                    }
                }
                Section {
                    TextField(L10n.tr("Anonymous"), text: $author)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                } header: {
                    Text(L10n.tr("Your Name"))
                } footer: {
                    Text(L10n.tr("Sent with it: \(CompatibilityStore.device), iOS \(UIDevice.current.systemVersion), Husk build \(CompatibilityStore.build), JIT from \(CompatibilityStore.jitMethod). It shows on antiweb.lol for everyone."))
                }
                Section {
                    Toggle(L10n.tr("Attach my log"), isOn: $attachLog)
                } footer: {
                    Text(L10n.tr("Your Husk log helps the developer see what went wrong. Only moderators can read it."))
                }
                if let error {
                    Section { Text(error).foregroundStyle(.red) }
                }
            }
            .navigationTitle(sent ? L10n.tr("Thanks!") : L10n.tr("Report"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button(subject.afterPlaying ? L10n.tr("Not Now") : L10n.tr("Cancel")) {
                        if subject.afterPlaying { CompatibilityStore.shared.markAsked(subject.package) }
                        dismiss()
                    }
                }
                ToolbarItem(placement: .confirmationAction) {
                    if sending { ProgressView() } else {
                        Button(L10n.tr("Send")) { send() }.disabled(status == nil)
                    }
                }
            }
        }
        .onAppear {
            if let crash = subject.crash {
                status = .broken
                if notes.isEmpty { notes = crash }
            }
        }
        .huskDetents([.medium, .large])
    }

    private func send() {
        guard let status else { return }
        sending = true
        error = nil
        Task {
            do {
                try await CompatibilityStore.shared.send(package: subject.package, name: subject.name, isGame: subject.isGame,
                                                         status: status, notes: notes, author: author, iconPath: subject.iconPath,
                                                         attachLog: attachLog)
                sent = true
                try? await Task.sleep(nanoseconds: 600_000_000)
                dismiss()
            } catch {
                self.error = error.localizedDescription
            }
            sending = false
        }
    }
}

/// A small label for lists: the app's status on the compatibility list.
struct CompatBadge: View {
    let status: CompatibilityStore.Status
    var body: some View {
        Label(status.label, systemImage: status.symbol)
            .labelStyle(.titleAndIcon)
            .font(.caption.weight(.medium))
            .foregroundStyle(status.color)
    }
}
