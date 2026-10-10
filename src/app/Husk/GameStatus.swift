// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// How a game did the last time it was played here, noted by Husk itself: it played, it crashed, or it would not start.
///
/// Written to status.json in the game's folder as it happens, so the library can say at a glance which games work on this
/// iPhone without anyone keeping a list.
@MainActor
final class GameStatusStore: ObservableObject {
    static let shared = GameStatusStore()

    enum Result: String, Codable {
        case plays, crashed, failed

        var label: String {
            switch self {
            case .plays: return "Plays"
            case .crashed: return "Crashed last time"
            case .failed: return "Did not start"
            }
        }
        var symbol: String {
            switch self {
            case .plays: return "checkmark.circle.fill"
            case .crashed: return "exclamationmark.triangle.fill"
            case .failed: return "xmark.circle.fill"
            }
        }
        var color: Color {
            switch self {
            case .plays: return .green
            case .crashed: return .orange
            case .failed: return .red
            }
        }
    }

    struct Status: Codable {
        var result: Result
        var time: Date
        var detail: String?
        /// Why it stopped, when the run's log said (AppDiagnosis)
        var why: Why?
    }
    struct Why: Codable { var kind: String; var title: String; var detail: String; var evidence: String? }

    /// The reason the last run stopped, while it has not played since.
    func diagnosis(_ appID: String) -> AppDiagnosis? {
        guard let s = status(appID), s.result != .plays, let w = s.why, let kind = AppDiagnosis.Kind(rawValue: w.kind) else { return nil }
        return AppDiagnosis(kind: kind, title: w.title, detail: w.detail, evidence: w.evidence)
    }

    func record(_ appID: String, _ result: Result, diagnosis d: AppDiagnosis) {
        guard !appID.isEmpty else { return }
        let s = Status(result: result, time: Date(), detail: d.title, why: Why(kind: d.kind.rawValue, title: d.title, detail: d.detail, evidence: d.evidence))
        cache[appID] = s
        if let data = try? JSONEncoder().encode(s) { try? data.write(to: Self.file(appID), options: .atomic) }
        HuskLog.log("status", "\(appID): \(result.rawValue) (\(d.title))")
    }

    @Published private var cache: [String: Status] = [:]
    private var loaded: Set<String> = []

    private static func file(_ appID: String) -> URL {
        TranslationLayer.root.appendingPathComponent(appID, isDirectory: true).appendingPathComponent("status.json")
    }

    func status(_ appID: String) -> Status? {
        if !loaded.contains(appID) {
            loaded.insert(appID)
            if let data = try? Data(contentsOf: Self.file(appID)),
               let s = try? JSONDecoder().decode(Status.self, from: data) { cache[appID] = s }
        }
        return cache[appID]
    }

    func record(_ appID: String, _ result: Result, detail: String? = nil) {
        guard !appID.isEmpty, status(appID)?.result != result || result != .plays else { return }
        let s = Status(result: result, time: Date(), detail: detail, why: nil)
        cache[appID] = s
        if let data = try? JSONEncoder().encode(s) { try? data.write(to: Self.file(appID), options: .atomic) }
        HuskLog.log("status", "\(appID): \(result.rawValue)\(detail.map { " (\($0))" } ?? "")")
    }
}

/// The small mark on a game's icon in the library.
struct GameStatusBadge: View {
    let result: GameStatusStore.Result

    var body: some View {
        Image(systemName: result.symbol)
            .font(.system(size: 15, weight: .bold))
            .symbolRenderingMode(.palette)
            .foregroundStyle(.white, result.color)
            .background(Circle().fill(Color(uiColor: .systemBackground)).padding(1))
            .accessibilityLabel(result.label)
    }
}
