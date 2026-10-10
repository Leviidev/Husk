// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// "That game closed Husk": noticed at the next launch, explained, and easy to send on.
///
/// A game that crashes takes Husk with it -- it runs in Husk's own process -- so nothing can be shown at the moment it
/// happens. Instead, a note is written when a game starts and removed when it is closed the normal way. A note still there
/// at the next launch means the game did not get to close: Husk shows what it knows from the previous run's log (kept as
/// husk-prev.log) and offers the whole report to share.
@MainActor
final class CrashReport: ObservableObject {
    static let shared = CrashReport()

    struct Report: Identifiable {
        let id = UUID()
        let game: String
        let appID: String
        let cause: Cause
        let when: Date?
        /// The previous run's last lines, for the sheet.
        let excerpt: [String]
    }

    enum Cause {
        case signal(Int)        // a fatal signal the crash handler recorded
        case startFailed        // the runtime said the game could not be started, and Husk closed after that
        case unknown            // no record of why: memory (iOS ends the app without a word), or Husk swiped away
        case diagnosed(AppDiagnosis)    // the run's log says why (AppDiagnosis)

        var title: String {
            switch self {
            case .signal: return "The game crashed"
            case .startFailed: return "The game could not start"
            case .unknown: return "Husk closed while the game was running"
            case .diagnosed(let d): return d.title
            }
        }
        var detail: String {
            switch self {
            case .signal(let s):
                let name = [11: "SIGSEGV, a bad memory access", 6: "SIGABRT, the game or a library gave up",
                            4: "SIGILL, an instruction this device does not run", 8: "SIGFPE, an arithmetic error",
                            12: "SIGSYS, a system call iOS does not allow"][s] ?? "signal \(s)"
                return "It stopped with \(name). The report has the full log of that run."
            case .startFailed:
                return "The runtime could not start it. The report says which part was missing."
            case .unknown:
                return "Nothing was recorded, which usually means iOS ended Husk for using too much memory, or Husk was closed from the app switcher."
            case .diagnosed(let d):
                return d.detail
            }
        }
    }

    @Published var pending: Report?

    private static var docs: URL { FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0] }
    private static var marker: URL { docs.appendingPathComponent("running-game.json") }
    static var previousLogURL: URL { docs.appendingPathComponent("husk-prev.log") }

    // MARK: the note

    private static var running: TLApp?
    private static var observers: [NSObjectProtocol] = []

    /// A game is starting: if Husk ends before `gameEnded`, the next launch will say so.
    ///
    /// Not while Husk is in the background, though: iOS ends background apps when it wants the memory, without a word,
    /// and that is not the game's doing. The note goes when Husk leaves the screen and comes back with it.
    static func gameStarted(_ app: TLApp) {
        running = app
        writeMarker()
        if observers.isEmpty {
            let nc = NotificationCenter.default
            observers.append(nc.addObserver(forName: UIApplication.didEnterBackgroundNotification, object: nil, queue: .main) { _ in
                try? FileManager.default.removeItem(at: marker)
            })
            observers.append(nc.addObserver(forName: UIApplication.willEnterForegroundNotification, object: nil, queue: .main) { _ in
                writeMarker()
            })
        }
    }

    private static func writeMarker() {
        guard let app = running else { return }
        let info: [String: Any] = ["id": app.id, "label": app.label, "time": Date().timeIntervalSince1970]
        if let data = try? JSONSerialization.data(withJSONObject: info) { try? data.write(to: marker, options: .atomic) }
    }

    /// The game was closed the normal way.
    static func gameEnded() {
        running = nil
        try? FileManager.default.removeItem(at: marker)
    }

    /// At launch: was a game running when Husk last ended?
    func checkPreviousRun() {
        guard let data = try? Data(contentsOf: Self.marker),
              let info = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
        try? FileManager.default.removeItem(at: Self.marker)
        let lines = ((try? String(contentsOf: Self.previousLogURL, encoding: .utf8)) ?? "")
            .split(separator: "\n", omittingEmptySubsequences: true).map(String.init)
        var cause = Cause.unknown
        if let fatal = lines.last(where: { $0.contains("*** FATAL SIGNAL") }),
           let n = fatal.split(separator: " ").compactMap({ Int($0) }).first {
            cause = .signal(n)
        } else if lines.contains(where: { $0.contains("the game could not be started") || $0.contains("native: launch refused") }) {
            cause = .startFailed
        }
        if let d = AppDiagnosis.fromLog(lines.joined(separator: "\n")) { cause = .diagnosed(d) }
        let time = (info["time"] as? Double).map { Date(timeIntervalSince1970: $0) }
        pending = Report(game: info["label"] as? String ?? "A game", appID: info["id"] as? String ?? "",
                         cause: cause, when: time, excerpt: Array(Self.interesting(lines).suffix(14)))
        HuskLog.log("crash", "\(pending!.game) did not close normally last run: \(cause.title)")
        switch cause {
        case .signal: GameStatusStore.shared.record(pending!.appID, .crashed)
        case .startFailed: GameStatusStore.shared.record(pending!.appID, .failed)
        case .unknown: break
        case .diagnosed(let d): GameStatusStore.shared.record(pending!.appID, .crashed, diagnosis: d)
        }
    }

    /// The lines of a log worth a glance: the runtime's and the crash handler's, not the JIT's housekeeping.
    private static func interesting(_ lines: [String]) -> [String] {
        lines.filter { l in
            !l.contains("csops status") && !l.contains("[husk-jit]") && !l.contains("[jit]") && !l.contains("[guest]")
                && !l.contains("[boot]") && !l.contains("[prev]")
        }
    }

    // MARK: the report

    /// Everything in one text file: what happened, this device, and both logs of the run that ended.
    static func writeReport(_ r: Report) -> URL? {
        let info = Bundle.main.infoDictionary ?? [:]
        var text = """
        Husk crash report
        =================
        Game     : \(r.game)
        What     : \(r.cause.title)
        Started  : \(r.when.map { ISO8601DateFormatter().string(from: $0) } ?? "?")
        Husk     : \(info["CFBundleShortVersionString"] as? String ?? "?") (\(info["CFBundleVersion"] as? String ?? "?")) \(info["HuskBuildCommit"] as? String ?? "")
        Device   : \(HuskLog.deviceModel), \(UIDevice.current.systemName) \(UIDevice.current.systemVersion)

        """
        if let app = TranslationLayerStore.shared.apps.first(where: { $0.id == r.appID }) {
            text += "Package  : \(app.packageName ?? "?")\n"
            text += "APKs     : \(app.apks.map { ($0 as NSString).lastPathComponent }.joined(separator: ", "))\n"
            text += "Engine   : \(app.report?.nativeEngine.map { "\($0)" } ?? "?")\n"
        }
        text += "\n---- husk.log of that run ----\n"
        text += (try? String(contentsOf: previousLogURL, encoding: .utf8)) ?? "(not kept)\n"
        text += "\n---- native output of that run ----\n"
        text += (try? String(contentsOf: HuskLog.previousNativeLogURL, encoding: .utf8)) ?? "(none)\n"
        let safe = r.game.replacingOccurrences(of: "/", with: "-")
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("Husk crash report - \(safe).txt")
        do { try text.write(to: url, atomically: true, encoding: .utf8) } catch { return nil }
        return url
    }
}

/// The sheet shown at launch after a game ended Husk.
struct CrashReportSheet: View {
    let report: CrashReport.Report
    @Environment(\.dismiss) private var dismiss
    @State private var file: URL?

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    HStack(spacing: 14) {
                        Image(systemName: "exclamationmark.triangle.fill")
                            .font(.system(size: 30)).foregroundStyle(.orange)
                        VStack(alignment: .leading, spacing: 3) {
                            Text(report.cause.title).font(.title3.weight(.semibold))
                            Text(report.game).foregroundStyle(.secondary)
                        }
                    }
                    Text(report.cause.detail).font(.callout)

                    if !report.excerpt.isEmpty {
                        VStack(alignment: .leading, spacing: 6) {
                            Text("Last lines").font(.footnote.weight(.semibold)).foregroundStyle(.secondary)
                            Text(report.excerpt.joined(separator: "\n"))
                                .font(.system(size: 10.5, design: .monospaced))
                                .textSelection(.enabled)
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .padding(10)
                                .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 10))
                        }
                    }

                    if let file {
                        ShareLink(item: file) {
                            Label("Share Report", systemImage: "square.and.arrow.up")
                                .frame(maxWidth: .infinity).padding(.vertical, 6)
                        }
                        .buttonStyle(.borderedProminent)
                    }
                    Text("The report holds the full log of that run and which device and build it was on. Nothing is sent anywhere unless you share it.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                .padding(20)
            }
            .navigationTitle("Crash Report")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar { ToolbarItem(placement: .confirmationAction) { Button("Done") { dismiss() } } }
        }
        .task { file = CrashReport.writeReport(report) }
    }
}
