import Foundation
#if canImport(Metal)
import Metal
#endif

/// Known Apple machine identifier -> marketing name mapping (display convenience only).
/// Any device not in this map falls back cleanly to its raw `utsname().machine` ID.
enum DeviceModelLookup {
    static let names: [String: String] = [
        "iPhone10,1": "iPhone 8", "iPhone10,4": "iPhone 8",
        "iPhone10,2": "iPhone 8 Plus", "iPhone10,5": "iPhone 8 Plus",
        "iPhone10,3": "iPhone X", "iPhone10,6": "iPhone X",
        "iPhone11,8": "iPhone XR", "iPhone11,2": "iPhone XS",
        "iPhone11,4": "iPhone XS Max", "iPhone11,6": "iPhone XS Max",
        "iPhone12,1": "iPhone 11", "iPhone12,3": "iPhone 11 Pro",
        "iPhone12,5": "iPhone 11 Pro Max", "iPhone12,8": "iPhone SE (2nd gen)",
        "iPhone13,1": "iPhone 12 mini", "iPhone13,2": "iPhone 12",
        "iPhone13,3": "iPhone 12 Pro", "iPhone13,4": "iPhone 12 Pro Max",
        "iPhone14,4": "iPhone 13 mini", "iPhone14,5": "iPhone 13",
        "iPhone14,2": "iPhone 13 Pro", "iPhone14,3": "iPhone 13 Pro Max",
        "iPhone14,6": "iPhone SE (3rd gen)",
        "iPhone14,7": "iPhone 14", "iPhone14,8": "iPhone 14 Plus",
        "iPhone15,2": "iPhone 14 Pro", "iPhone15,3": "iPhone 14 Pro Max",
        "iPhone15,4": "iPhone 15", "iPhone15,5": "iPhone 15 Plus",
        "iPhone16,1": "iPhone 15 Pro", "iPhone16,2": "iPhone 15 Pro Max",
        "iPhone17,3": "iPhone 16", "iPhone17,4": "iPhone 16 Plus",
        "iPhone17,1": "iPhone 16 Pro", "iPhone17,2": "iPhone 16 Pro Max",
        "iPhone17,5": "iPhone 16e",
        "iPhone18,3": "iPhone 17", "iPhone18,1": "iPhone 17 Pro",
        "iPhone18,2": "iPhone 17 Pro Max", "iPhone18,4": "iPhone Air",
        "iPhone18,5": "iPhone 17e",
        "iPhone19,2": "iPhone 18 Pro",
        "iPhone19,3": "iPhone 18 Pro Max", "iPhone19,7": "iPhone 18 Pro Max",
        "iPhone19,4": "iPhone Duo",
    ]

    static func friendlyName(for machineID: String) -> String {
        names[machineID] ?? machineID
    }
}

/// Real hardware and OS specifications automatically collected from the running iPhone/iPad.
struct CurrentDeviceSpecs: Equatable {
    var machineID: String
    var modelName: String
    var chipName: String
    var osVersion: String
    var ramMiB: Int
    var cpuCount: Int
    var activeCPUCount: Int
    var nativePixelWidth: Int
    var nativePixelHeight: Int
    var pointWidth: Int
    var pointHeight: Int
    var nativeScale: Double
    var maxRefreshHz: Int
    var safeLeftPt: Double
    var safeRightPt: Double
    var safeTopPt: Double
    var safeBottomPt: Double
    var colorGamut: String
    var thermalStateLabel: String
    var lowPowerMode: Bool
    var buildStamp: String

    var landscapePixelsText: String {
        let w = max(nativePixelWidth, nativePixelHeight)
        let h = min(nativePixelWidth, nativePixelHeight)
        return (w > 0 && h > 0) ? "\(w)×\(h) px" : "—"
    }

    var landscapePointsText: String {
        let w = max(pointWidth, pointHeight)
        let h = min(pointWidth, pointHeight)
        return (w > 0 && h > 0) ? "\(w)×\(h) pt (@\(String(format: "%.0fx", max(nativeScale, 1.0))))" : "—"
    }

    var ramText: String {
        guard ramMiB > 0 else { return "—" }
        return String(format: "%d MiB (~%.1f GB)", ramMiB, Double(ramMiB) / 1024.0)
    }

    var cutoutText: String {
        let edge = max(safeLeftPt, safeRightPt, safeTopPt, safeBottomPt)
        if edge <= 20.5 { return "Viền phẳng / Home (\(String(format: "%.0f", edge)) pt)" }
        if edge >= 52.0 { return "Dynamic Island (\(String(format: "%.0f", edge)) pt)" }
        return "Tai thỏ / Notch (\(String(format: "%.0f", edge)) pt)"
    }
}

enum IPhonePerfThresholds {
    static let targetFPS: Double = 60.0
    static let frameBudgetMs: Double = 16.67
    static let minAvgFPS: Double = 54.0
    static let maxMeanFrameMs: Double = 18.5
    static let maxP95FrameMs: Double = 22.0
    static let maxSingleFrameMs: Double = 50.0
    static let maxHitch50PerMin: Double = 2.0
    static let maxHitch100Count: Int = 0
}

struct IPhonePerfScenarioMetrics: Equatable {
    var samples: Int = 0
    var fpsValues: [Double] = []
    var meanValues: [Double] = []
    var maxValues: [Double] = []
    var hitch50: Int = 0
    var hitch100: Int = 0

    var avgFPS: Double { fpsValues.isEmpty ? 0 : fpsValues.reduce(0, +) / Double(fpsValues.count) }
    var meanMs: Double { meanValues.isEmpty ? 0 : meanValues.reduce(0, +) / Double(meanValues.count) }
    var p95Ms: Double {
        guard !meanValues.isEmpty else { return 0 }
        let sorted = meanValues.sorted()
        let idx = max(0, min(sorted.count - 1, Int(ceil(0.95 * Double(sorted.count))) - 1))
        return sorted[idx]
    }
    var maxMs: Double { maxValues.max() ?? 0 }

    var verdict: String {
        guard samples > 0 else { return "chưa đủ dữ liệu" }
        let durMin = max(Double(samples) * 9.0 / 60.0, 1e-6)
        let h50PerMin = Double(hitch50) / durMin
        let pass = avgFPS >= IPhonePerfThresholds.minAvgFPS
            && meanMs <= IPhonePerfThresholds.maxMeanFrameMs
            && p95Ms <= IPhonePerfThresholds.maxP95FrameMs
            && maxMs <= IPhonePerfThresholds.maxSingleFrameMs
            && hitch100 <= IPhonePerfThresholds.maxHitch100Count
            && h50PerMin <= IPhonePerfThresholds.maxHitch50PerMin
        return pass ? "đạt" : "có giật/lag"
    }
}

struct DeviceGamePerfSummary: Equatable, Identifiable {
    var packageName: String
    var normalPlay = IPhonePerfScenarioMetrics()
    var heavyCrowd = IPhonePerfScenarioMetrics()
    var switchAttempts: Int = 0
    var switchPassed: Int = 0
    var minFreeMiB: Int?
    var lastFreeMiB: Int?

    var id: String { packageName }

    var displayName: String {
        switch packageName {
        case "com.tech3cgame.vhpt1": return "Võ Hiệp Phong Thần"
        case "com.game3c.vhtgmobile", "review.com.game3c.vhtgmobile": return "Võ Hiệp Truyền Kỳ"
        case "vn.trutien.client": return "Tru Tiên"
        default: return packageName
        }
    }

    var combinedMetrics: IPhonePerfScenarioMetrics {
        var merged = IPhonePerfScenarioMetrics()
        merged.samples = normalPlay.samples + heavyCrowd.samples
        merged.fpsValues = normalPlay.fpsValues + heavyCrowd.fpsValues
        merged.meanValues = normalPlay.meanValues + heavyCrowd.meanValues
        merged.maxValues = normalPlay.maxValues + heavyCrowd.maxValues
        merged.hitch50 = normalPlay.hitch50 + heavyCrowd.hitch50
        merged.hitch100 = normalPlay.hitch100 + heavyCrowd.hitch100
        return merged
    }

    var switchVerdict: String {
        guard switchAttempts > 0 else { return "chưa đủ dữ liệu" }
        return switchPassed == switchAttempts ? "đạt" : "có lỗi chuyển game"
    }

    var overallVerdict: String {
        let total = combinedMetrics
        if total.samples == 0 && switchAttempts == 0 { return "chưa đủ dữ liệu" }
        if switchAttempts > 0 && switchPassed < switchAttempts { return "có lỗi chuyển game" }
        if heavyCrowd.samples > 0 && heavyCrowd.verdict == "có giật/lag" { return "có giật/lag" }
        if normalPlay.samples > 0 && normalPlay.verdict == "có giật/lag" { return "có giật/lag" }
        return total.samples > 0 ? total.verdict : switchVerdict
    }
}

struct IPhoneParsedLogs: Equatable {
    var deviceMachineID: String = ""
    var iosVersion: String = ""
    var ramMiB: Int = 0
    var buildStamp: String = ""
    var games: [String: DeviceGamePerfSummary] = [:]
    var minFreeMiB: Int?
    var lastFreeMiB: Int?
    var activePackage: String = ""

    var orderedGames: [DeviceGamePerfSummary] {
        games.values.sorted { lhs, rhs in
            let lCount = lhs.combinedMetrics.samples + lhs.switchAttempts
            let rCount = rhs.combinedMetrics.samples + rhs.switchAttempts
            if lCount != rCount { return lCount > rCount }
            return lhs.packageName < rhs.packageName
        }
    }

    func summary(for packageName: String) -> DeviceGamePerfSummary {
        games[packageName] ?? DeviceGamePerfSummary(packageName: packageName)
    }
}

enum IPhoneLogEvaluator {
    static let targetPackage = "com.tech3cgame.vhpt1"

    static func parse(lines: [String]) -> IPhoneParsedLogs {
        var out = IPhoneParsedLogs()
        var pendingTarget: String?
        var currentPackage = ""

        for line in lines {
            if line.contains("[prev] ") { continue }
            if let r = line.range(of: "[boot] device      : ") {
                out.deviceMachineID = String(line[r.upperBound...]).trimmingCharacters(in: .whitespacesAndNewlines)
            } else if let r = line.range(of: "[boot] system      : ") {
                out.iosVersion = String(line[r.upperBound...]).trimmingCharacters(in: .whitespacesAndNewlines)
            } else if let r = line.range(of: "[boot] physical RAM: ") {
                let s = String(line[r.upperBound...]).components(separatedBy: " ").first ?? ""
                out.ramMiB = Int(s) ?? out.ramMiB
            } else if let r = line.range(of: "[boot] build       : ") {
                out.buildStamp = String(line[r.upperBound...]).trimmingCharacters(in: .whitespacesAndNewlines)
            } else if let r = line.range(of: "javaapp: ") {
                let pkg = String(line[r.upperBound...]).components(separatedBy: ",").first?.trimmingCharacters(in: .whitespacesAndNewlines) ?? ""
                if !pkg.isEmpty, pkg.contains(".") {
                    currentPackage = pkg
                    out.activePackage = pkg
                    if out.games[pkg] == nil { out.games[pkg] = DeviceGamePerfSummary(packageName: pkg) }
                }
            }

            if line.contains("[switch] ["), line.contains("switch requested:") {
                pendingTarget = extractSwitchTarget(line)
            } else if line.contains("[switch] ["), line.contains("switch committed:") {
                let active = extractToken(line, prefix: "active=") ?? pendingTarget ?? ""
                if !active.isEmpty {
                    var g = out.games[active] ?? DeviceGamePerfSummary(packageName: active)
                    g.switchAttempts += 1
                    g.switchPassed += 1
                    out.games[active] = g
                    currentPackage = active
                    out.activePackage = active
                }
                pendingTarget = nil
            } else if line.contains("[switch] ["), (line.contains("blocked") || line.contains("failed")) {
                let target = pendingTarget ?? extractToken(line, prefix: "to=") ?? currentPackage
                if !target.isEmpty {
                    var g = out.games[target] ?? DeviceGamePerfSummary(packageName: target)
                    g.switchAttempts += 1
                    out.games[target] = g
                }
                pendingTarget = nil
            }

            if line.contains("java-perf:"), let pkg = extractToken(line, prefix: "pkg=") {
                let isCrowd = line.contains("scenario=heavy_crowd_effects") || line.lowercased().contains("crowd")
                let fps = extractDouble(line, prefix: "fps=")
                let meanMs = extractDouble(line, prefix: "mean_ms=")
                let maxMs = extractDouble(line, prefix: "max_ms=")
                let h50 = Int(extractDouble(line, prefix: "hitch50=") ?? 0)
                let h100 = Int(extractDouble(line, prefix: "hitch100=") ?? 0)
                if let fps, let meanMs, let maxMs {
                    currentPackage = pkg
                    out.activePackage = pkg
                    var g = out.games[pkg] ?? DeviceGamePerfSummary(packageName: pkg)
                    if isCrowd {
                        g.heavyCrowd.samples += 1
                        g.heavyCrowd.fpsValues.append(fps)
                        g.heavyCrowd.meanValues.append(meanMs)
                        g.heavyCrowd.maxValues.append(maxMs)
                        g.heavyCrowd.hitch50 += h50
                        g.heavyCrowd.hitch100 += h100
                    } else {
                        g.normalPlay.samples += 1
                        g.normalPlay.fpsValues.append(fps)
                        g.normalPlay.meanValues.append(meanMs)
                        g.normalPlay.maxValues.append(maxMs)
                        g.normalPlay.hitch50 += h50
                        g.normalPlay.hitch100 += h100
                    }
                    out.games[pkg] = g
                }
            }

            if line.contains("MiB left before jetsam"), let mib = extractJetsamMiB(line) {
                out.lastFreeMiB = mib
                out.minFreeMiB = out.minFreeMiB.map { min($0, mib) } ?? mib
                if !currentPackage.isEmpty {
                    var g = out.games[currentPackage] ?? DeviceGamePerfSummary(packageName: currentPackage)
                    g.lastFreeMiB = mib
                    g.minFreeMiB = g.minFreeMiB.map { min($0, mib) } ?? mib
                    out.games[currentPackage] = g
                }
            }
        }
        return out
    }

    static func reportText(specs: CurrentDeviceSpecs, logs: IPhoneParsedLogs, focusedPackage: String?) -> String {
        var lines: [String] = []
        lines.append("=== Chỉ số thiết bị & hiệu năng Husk ===")
        lines.append("Thiết bị : \(specs.modelName) (\(specs.machineID))")
        lines.append("Chip/GPU : \(specs.chipName)")
        lines.append("RAM      : \(specs.ramText)")
        lines.append("CPU      : \(specs.cpuCount) nhân (\(specs.activeCPUCount) active)")
        lines.append("Màn hình : \(specs.landscapePixelsText) · \(specs.landscapePointsText)")
        lines.append("Tần số   : \(specs.maxRefreshHz) Hz · \(specs.colorGamut)")
        lines.append("Tai thỏ  : \(specs.cutoutText)")
        lines.append("Hệ thống : \(specs.osVersion)")
        lines.append("Nhiệt độ : \(specs.thermalStateLabel)\(specs.lowPowerMode ? " · Tiết kiệm pin" : "")")
        lines.append("Build    : \(logs.buildStamp.isEmpty ? specs.buildStamp : logs.buildStamp)")
        if let free = logs.lastFreeMiB, let minFree = logs.minFreeMiB {
            lines.append("RAM trống trước Jetsam: \(free) MiB (thấp nhất \(minFree) MiB)")
        }
        lines.append("")
        let gameList: [DeviceGamePerfSummary]
        if let pkg = focusedPackage, !pkg.isEmpty {
            let matching = logs.summary(for: pkg)
            let others = logs.orderedGames.filter { $0.packageName != pkg }
            gameList = [matching] + others
        } else {
            gameList = logs.orderedGames
        }
        if gameList.isEmpty {
            lines.append("Chưa có dữ liệu chơi game trong phiên này (hãy mở game để tự động ghi FPS / frame time).")
        } else {
            for g in gameList {
                let c = g.combinedMetrics
                lines.append("[\(g.displayName)] (\(g.packageName)) -> \(g.overallVerdict)")
                if c.samples > 0 {
                    lines.append(String(format: "  Chơi thực tế: %.1f FPS · mean %.2f ms · p95 %.2f ms · max %.2f ms · h50=%d h100=%d (%d mẫu)",
                                        c.avgFPS, c.meanMs, c.p95Ms, c.maxMs, c.hitch50, c.hitch100, c.samples))
                } else {
                    lines.append("  Chơi thực tế: chưa có mẫu java-perf")
                }
                if g.switchAttempts > 0 {
                    lines.append("  Chuyển game : \(g.switchPassed)/\(g.switchAttempts) lượt (\(g.switchVerdict))")
                }
            }
        }
        return lines.joined(separator: "\n")
    }

    private static func extractSwitchTarget(_ line: String) -> String? {
        if let to = extractToken(line, prefix: "to=") { return to }
        return nil
    }

    private static func extractToken(_ line: String, prefix: String) -> String? {
        guard let r = line.range(of: prefix) else { return nil }
        let tail = line[r.upperBound...]
        let raw = tail.prefix { !$0.isWhitespace && $0 != "," && $0 != ")" }
        return raw.isEmpty ? nil : String(raw)
    }

    private static func extractDouble(_ line: String, prefix: String) -> Double? {
        guard let tok = extractToken(line, prefix: prefix) else { return nil }
        return Double(tok)
    }

    private static func extractJetsamMiB(_ line: String) -> Int? {
        guard let r = line.range(of: " MiB left before jetsam") else { return nil }
        let head = line[..<r.lowerBound]
        let digits = head.reversed().prefix { $0.isNumber }.reversed()
        return digits.isEmpty ? nil : Int(String(digits))
    }
}

#if canImport(SwiftUI) && canImport(UIKit)
#if canImport(Combine)
import Combine
#endif
import SwiftUI
import UIKit

enum CurrentDeviceProbe {
    static func collect() -> CurrentDeviceSpecs {
        let machine = HuskLog.deviceModel
        let modelName = DeviceModelLookup.friendlyName(for: machine)
        let pi = ProcessInfo.processInfo
        let d = UIDevice.current
        let screen = UIScreen.main
        let nb = screen.nativeBounds.size
        let pb = screen.bounds.size
        let scale = Double(screen.nativeScale)
        let insets = activeSafeAreaInsets()
        let info = Bundle.main.infoDictionary ?? [:]
        let version = info["CFBundleShortVersionString"] as? String ?? "?"
        let build = info["CFBundleVersion"] as? String ?? "?"
        let commit = info["HuskBuildCommit"] as? String ?? ""
        let stamp = commit.isEmpty ? "\(version) (\(build))" : "\(version) (\(build)) \(commit)"

        let chip: String = {
            #if canImport(Metal)
            if let gpu = MTLCreateSystemDefaultDevice()?.name, !gpu.isEmpty {
                return gpu
            }
            #endif
            return "\(pi.processorCount)-core ARM64"
        }()

        let gamut = UITraitCollection.current.displayGamut == .P3 ? "Display P3" : "sRGB"
        let thermal: String
        switch pi.thermalState {
        case .nominal: thermal = "Bình thường"
        case .fair: thermal = "Hơi ấm"
        case .serious: thermal = "Nóng (thermal)"
        case .critical: thermal = "Quá nhiệt (throttle)"
        @unknown default: thermal = "Không rõ"
        }

        return CurrentDeviceSpecs(
            machineID: machine,
            modelName: modelName,
            chipName: chip,
            osVersion: "\(d.systemName) \(d.systemVersion)",
            ramMiB: Int(pi.physicalMemory / (1024 * 1024)),
            cpuCount: pi.processorCount,
            activeCPUCount: pi.activeProcessorCount,
            nativePixelWidth: Int(nb.width.rounded()),
            nativePixelHeight: Int(nb.height.rounded()),
            pointWidth: Int(pb.width.rounded()),
            pointHeight: Int(pb.height.rounded()),
            nativeScale: scale,
            maxRefreshHz: screen.maximumFramesPerSecond,
            safeLeftPt: Double(insets.left),
            safeRightPt: Double(insets.right),
            safeTopPt: Double(insets.top),
            safeBottomPt: Double(insets.bottom),
            colorGamut: gamut,
            thermalStateLabel: thermal,
            lowPowerMode: pi.isLowPowerModeEnabled,
            buildStamp: stamp
        )
    }

    static func readCurrentLogs() -> IPhoneParsedLogs {
        if let data = try? Data(contentsOf: HuskLog.logFileURL),
           let text = String(data: data, encoding: .utf8), !text.isEmpty {
            return IPhoneLogEvaluator.parse(lines: text.components(separatedBy: .newlines))
        }
        return IPhoneLogEvaluator.parse(lines: HuskLog.recentLines(4000))
    }

    private static func activeSafeAreaInsets() -> UIEdgeInsets {
        let scenes = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
        for scene in scenes {
            if let win = scene.windows.first(where: { $0.isKeyWindow }) ?? scene.windows.first {
                return win.safeAreaInsets
            }
        }
        return .zero
    }
}

/// Automatically detects hardware specifications and live gameplay performance of this iPhone/iPad.
struct IPhonePerfSuiteView: View {
    var defaultPackage: String? = nil

    @State private var specs = CurrentDeviceProbe.collect()
    @State private var parsedLogs = CurrentDeviceProbe.readCurrentLogs()
    @State private var copiedBanner = false

    private let refreshTimer = Timer.publish(every: 2.0, on: .main, in: .common).autoconnect()

    private var activeSummary: DeviceGamePerfSummary? {
        if let pkg = defaultPackage, !pkg.isEmpty {
            return parsedLogs.summary(for: pkg)
        }
        if !parsedLogs.activePackage.isEmpty {
            return parsedLogs.summary(for: parsedLogs.activePackage)
        }
        return parsedLogs.orderedGames.first
    }

    var body: some View {
        Form {
            Section {
                DetailRow(label: "Mẫu máy", value: "\(specs.modelName) (\(specs.machineID))")
                DetailRow(label: "Chip / GPU", value: specs.chipName)
                DetailRow(label: "RAM vật lý", value: specs.ramText)
                DetailRow(label: "CPU", value: "\(specs.cpuCount) nhân (\(specs.activeCPUCount) active)")
                DetailRow(label: "Độ phân giải thật", value: specs.landscapePixelsText)
                DetailRow(label: "Khung hình (pt)", value: specs.landscapePointsText)
                DetailRow(label: "Tần số quét", value: "\(specs.maxRefreshHz) Hz · \(specs.colorGamut)")
                DetailRow(label: "Vùng tai thỏ / mép", value: specs.cutoutText)
                DetailRow(label: "Hệ điều hành", value: specs.osVersion)
                DetailRow(label: "Nhiệt độ máy", value: specs.thermalStateLabel + (specs.lowPowerMode ? " · Pin yếu" : ""))
                DetailRow(label: "Bản build", value: parsedLogs.buildStamp.isEmpty ? specs.buildStamp : parsedLogs.buildStamp)
                if let free = parsedLogs.lastFreeMiB, let minFree = parsedLogs.minFreeMiB {
                    DetailRow(label: "RAM trống trước Jetsam", value: "\(free) MiB (thấp nhất \(minFree) MiB)")
                }
            } header: {
                Text("Chỉ số phần cứng máy này (tự động)")
            }

            if let focused = activeSummary {
                gamePerfSection(focused, title: "Hiệu năng game đang chọn / gần nhất")
                let others = parsedLogs.orderedGames.filter { $0.packageName != focused.packageName }
                if !others.isEmpty {
                    ForEach(others) { g in
                        gamePerfSection(g, title: g.displayName)
                    }
                }
            } else {
                Section {
                    Text("Chưa có dữ liệu FPS/frame time trong phiên hiện tại. Hãy vào chơi game để Husk tự động đo chỉ số của máy này.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                } header: {
                    Text("Hiệu năng đo thực tế")
                }
            }

            Section {
                DetailRow(label: "FPS đạt chuẩn", value: String(format: ">= %.0f / %.0f FPS", IPhonePerfThresholds.minAvgFPS, IPhonePerfThresholds.targetFPS))
                DetailRow(label: "Frame time TB / P95", value: String(format: "<= %.1f ms / <= %.1f ms", IPhonePerfThresholds.maxMeanFrameMs, IPhonePerfThresholds.maxP95FrameMs))
                DetailRow(label: "Frame đỉnh / giật nặng", value: String(format: "<= %.0f ms · hitch100 = 0", IPhonePerfThresholds.maxSingleFrameMs))
            } header: {
                Text("Ngưỡng đánh giá ổn định")
            }

            Section {
                Button {
                    refreshNow()
                } label: {
                    Label("Đo & cập nhật lại ngay", systemImage: "arrow.clockwise")
                }
                Button {
                    UIPasteboard.general.string = IPhoneLogEvaluator.reportText(
                        specs: specs, logs: parsedLogs, focusedPackage: defaultPackage
                    )
                    copiedBanner = true
                } label: {
                    Label(copiedBanner ? "Đã chép báo cáo thiết bị" : "Sao chép báo cáo máy này", systemImage: "doc.on.doc")
                }
            }
        }
        .navigationTitle("Thiết bị & Hiệu năng")
        .onAppear { refreshNow() }
        .onReceive(refreshTimer) { _ in refreshNow() }
    }

    @ViewBuilder
    private func gamePerfSection(_ g: DeviceGamePerfSummary, title: String) -> some View {
        let total = g.combinedMetrics
        Section {
            DetailRow(label: "Game", value: "\(g.displayName)")
            if total.samples > 0 {
                DetailRow(label: "FPS trung bình", value: String(format: "%.1f FPS", total.avgFPS))
                DetailRow(label: "Frame time (TB / P95 / Max)",
                          value: String(format: "%.2f / %.2f / %.2f ms", total.meanMs, total.p95Ms, total.maxMs))
                DetailRow(label: "Đợt giật (>50ms / >100ms)",
                          value: "\(total.hitch50) / \(total.hitch100) (\(total.samples) mẫu)")
            } else {
                DetailRow(label: "FPS & Frame time", value: "Chưa có dữ liệu phiên này")
            }
            if g.switchAttempts > 0 {
                DetailRow(label: "Chuyển game", value: "\(g.switchPassed)/\(g.switchAttempts) lượt (\(g.switchVerdict))")
            }
            DetailRow(label: "Kết luận", value: g.overallVerdict)
        } header: {
            Text(title)
        }
    }

    private func refreshNow() {
        specs = CurrentDeviceProbe.collect()
        parsedLogs = CurrentDeviceProbe.readCurrentLogs()
    }
}
#endif
