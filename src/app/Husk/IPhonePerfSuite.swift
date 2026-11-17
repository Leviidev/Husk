// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

// MARK: - Evaluation Model

enum IPhoneFamily: String, CaseIterable, Identifiable {
    case all = "Tất cả (8 → 18)"
    case s8X = "iPhone 8 / X"
    case sXSXR = "iPhone XR / XS"
    case s11SE = "iPhone 11 / SE"
    case s12 = "iPhone 12"
    case s13 = "iPhone 13"
    case s14 = "iPhone 14"
    case s15 = "iPhone 15"
    case s16 = "iPhone 16"
    case s17 = "iPhone 17 / Air"
    case s18 = "iPhone 18 / Duo"

    var id: String { rawValue }
}

struct IPhoneModelProfile: Identifiable, Hashable {
    let id: String
    let family: IPhoneFamily
    let variant: String
    let machineIDs: [String]
    let chip: String
    let ramGB: Double?
    let pixelWidth: Int?
    let pixelHeight: Int?
    let pointWidth: Int?
    let pointHeight: Int?
    let scale: Double?
    let refreshRateHz: Int?
    let cutoutType: String
    let safeLeftPt: Int?
    let compatibleIOS: String
    let specVerified: Bool
    let specNotes: String

    var resolutionLabel: String {
        guard let pw = pixelWidth, let ph = pixelHeight else { return specNotes.isEmpty ? "Chưa xác minh" : specNotes }
        if let lw = pointWidth, let lh = pointHeight {
            return "\(pw)×\(ph) px (\(lw)×\(lh) pt)"
        }
        return "\(pw)×\(ph) px"
    }

    var ramLabel: String {
        guard let r = ramGB else { return "Chưa xác minh" }
        return String(format: "%.0f GB", r)
    }

    var refreshLabel: String {
        guard let hz = refreshRateHz else { return "Chưa xác minh" }
        return "\(hz) Hz"
    }

    var safeLeftPx: Int {
        guard let s = scale, let pt = safeLeftPt else { return 0 }
        return Int((Double(pt) * s).rounded())
    }
}

enum IPhoneTestCatalog {
    static let targetPackage = "com.tech3cgame.vhpt1"

    static let all: [IPhoneModelProfile] = [
        IPhoneModelProfile(id: "iphone-8", family: .s8X, variant: "iPhone 8 (4.7\")", machineIDs: ["iPhone10,1", "iPhone10,4"], chip: "Apple A11 Bionic", ramGB: 2, pixelWidth: 1334, pixelHeight: 750, pointWidth: 667, pointHeight: 375, scale: 2, refreshRateHz: 60, cutoutType: "none", safeLeftPt: 0, compatibleIOS: "iOS 11.0 - 16.7.10", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-8-plus", family: .s8X, variant: "iPhone 8 Plus (5.5\")", machineIDs: ["iPhone10,2", "iPhone10,5"], chip: "Apple A11 Bionic", ramGB: 3, pixelWidth: 1920, pixelHeight: 1080, pointWidth: 736, pointHeight: 414, scale: 3, refreshRateHz: 60, cutoutType: "none", safeLeftPt: 0, compatibleIOS: "iOS 11.0 - 16.7.10", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-x", family: .s8X, variant: "iPhone X (5.8\")", machineIDs: ["iPhone10,3", "iPhone10,6"], chip: "Apple A11 Bionic", ramGB: 3, pixelWidth: 2436, pixelHeight: 1125, pointWidth: 812, pointHeight: 375, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 44, compatibleIOS: "iOS 11.0 - 16.7.10", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-xr", family: .sXSXR, variant: "iPhone XR (6.1\")", machineIDs: ["iPhone11,8"], chip: "Apple A12 Bionic", ramGB: 3, pixelWidth: 1792, pixelHeight: 828, pointWidth: 896, pointHeight: 414, scale: 2, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 48, compatibleIOS: "iOS 12.0 - 18.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-xs", family: .sXSXR, variant: "iPhone XS (5.8\")", machineIDs: ["iPhone11,2"], chip: "Apple A12 Bionic", ramGB: 4, pixelWidth: 2436, pixelHeight: 1125, pointWidth: 812, pointHeight: 375, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 44, compatibleIOS: "iOS 12.0 - 18.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-xs-max", family: .sXSXR, variant: "iPhone XS Max (6.5\")", machineIDs: ["iPhone11,4", "iPhone11,6"], chip: "Apple A12 Bionic", ramGB: 4, pixelWidth: 2688, pixelHeight: 1242, pointWidth: 896, pointHeight: 414, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 44, compatibleIOS: "iOS 12.0 - 18.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-11", family: .s11SE, variant: "iPhone 11 (6.1\")", machineIDs: ["iPhone12,1"], chip: "Apple A13 Bionic", ramGB: 4, pixelWidth: 1792, pixelHeight: 828, pointWidth: 896, pointHeight: 414, scale: 2, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 48, compatibleIOS: "iOS 13.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-11-pro", family: .s11SE, variant: "iPhone 11 Pro (5.8\")", machineIDs: ["iPhone12,3"], chip: "Apple A13 Bionic", ramGB: 4, pixelWidth: 2436, pixelHeight: 1125, pointWidth: 812, pointHeight: 375, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 44, compatibleIOS: "iOS 13.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-11-pro-max", family: .s11SE, variant: "iPhone 11 Pro Max (6.5\")", machineIDs: ["iPhone12,5"], chip: "Apple A13 Bionic", ramGB: 4, pixelWidth: 2688, pixelHeight: 1242, pointWidth: 896, pointHeight: 414, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 44, compatibleIOS: "iOS 13.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-se-2", family: .s11SE, variant: "iPhone SE (2nd gen)", machineIDs: ["iPhone12,8"], chip: "Apple A13 Bionic", ramGB: 3, pixelWidth: 1334, pixelHeight: 750, pointWidth: 667, pointHeight: 375, scale: 2, refreshRateHz: 60, cutoutType: "none", safeLeftPt: 0, compatibleIOS: "iOS 13.4 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-se-3", family: .s11SE, variant: "iPhone SE (3rd gen)", machineIDs: ["iPhone14,6"], chip: "Apple A15 Bionic (4-GPU)", ramGB: 4, pixelWidth: 1334, pixelHeight: 750, pointWidth: 667, pointHeight: 375, scale: 2, refreshRateHz: 60, cutoutType: "none", safeLeftPt: 0, compatibleIOS: "iOS 15.4 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-12-mini", family: .s12, variant: "iPhone 12 mini (5.4\")", machineIDs: ["iPhone13,1"], chip: "Apple A14 Bionic", ramGB: 4, pixelWidth: 2340, pixelHeight: 1080, pointWidth: 780, pointHeight: 360, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 50, compatibleIOS: "iOS 14.2 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-12", family: .s12, variant: "iPhone 12 (6.1\")", machineIDs: ["iPhone13,2"], chip: "Apple A14 Bionic", ramGB: 4, pixelWidth: 2532, pixelHeight: 1170, pointWidth: 844, pointHeight: 390, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 14.1 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-12-pro", family: .s12, variant: "iPhone 12 Pro (6.1\")", machineIDs: ["iPhone13,3"], chip: "Apple A14 Bionic", ramGB: 6, pixelWidth: 2532, pixelHeight: 1170, pointWidth: 844, pointHeight: 390, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 14.1 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-12-pro-max", family: .s12, variant: "iPhone 12 Pro Max (6.7\")", machineIDs: ["iPhone13,4"], chip: "Apple A14 Bionic", ramGB: 6, pixelWidth: 2778, pixelHeight: 1284, pointWidth: 926, pointHeight: 428, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 14.2 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-13-mini", family: .s13, variant: "iPhone 13 mini (5.4\")", machineIDs: ["iPhone14,4"], chip: "Apple A15 Bionic (4-GPU)", ramGB: 4, pixelWidth: 2340, pixelHeight: 1080, pointWidth: 780, pointHeight: 360, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 50, compatibleIOS: "iOS 15.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-13", family: .s13, variant: "iPhone 13 (6.1\")", machineIDs: ["iPhone14,5"], chip: "Apple A15 Bionic (4-GPU)", ramGB: 4, pixelWidth: 2532, pixelHeight: 1170, pointWidth: 844, pointHeight: 390, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 15.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-13-pro", family: .s13, variant: "iPhone 13 Pro (6.1\")", machineIDs: ["iPhone14,2"], chip: "Apple A15 Bionic (5-GPU)", ramGB: 6, pixelWidth: 2532, pixelHeight: 1170, pointWidth: 844, pointHeight: 390, scale: 3, refreshRateHz: 120, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 15.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-13-pro-max", family: .s13, variant: "iPhone 13 Pro Max (6.7\")", machineIDs: ["iPhone14,3"], chip: "Apple A15 Bionic (5-GPU)", ramGB: 6, pixelWidth: 2778, pixelHeight: 1284, pointWidth: 926, pointHeight: 428, scale: 3, refreshRateHz: 120, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 15.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-14", family: .s14, variant: "iPhone 14 (6.1\")", machineIDs: ["iPhone14,7"], chip: "Apple A15 Bionic (5-GPU)", ramGB: 6, pixelWidth: 2532, pixelHeight: 1170, pointWidth: 844, pointHeight: 390, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 16.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-14-plus", family: .s14, variant: "iPhone 14 Plus (6.7\")", machineIDs: ["iPhone14,8"], chip: "Apple A15 Bionic (5-GPU)", ramGB: 6, pixelWidth: 2778, pixelHeight: 1284, pointWidth: 926, pointHeight: 428, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 16.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-14-pro", family: .s14, variant: "iPhone 14 Pro (6.1\")", machineIDs: ["iPhone15,2"], chip: "Apple A16 Bionic", ramGB: 6, pixelWidth: 2556, pixelHeight: 1179, pointWidth: 852, pointHeight: 393, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 59, compatibleIOS: "iOS 16.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-14-pro-max", family: .s14, variant: "iPhone 14 Pro Max (6.7\")", machineIDs: ["iPhone15,3"], chip: "Apple A16 Bionic", ramGB: 6, pixelWidth: 2796, pixelHeight: 1290, pointWidth: 932, pointHeight: 430, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 59, compatibleIOS: "iOS 16.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-15", family: .s15, variant: "iPhone 15 (6.1\")", machineIDs: ["iPhone15,4"], chip: "Apple A16 Bionic", ramGB: 6, pixelWidth: 2556, pixelHeight: 1179, pointWidth: 852, pointHeight: 393, scale: 3, refreshRateHz: 60, cutoutType: "dynamic_island", safeLeftPt: 59, compatibleIOS: "iOS 17.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-15-plus", family: .s15, variant: "iPhone 15 Plus (6.7\")", machineIDs: ["iPhone15,5"], chip: "Apple A16 Bionic", ramGB: 6, pixelWidth: 2796, pixelHeight: 1290, pointWidth: 932, pointHeight: 430, scale: 3, refreshRateHz: 60, cutoutType: "dynamic_island", safeLeftPt: 59, compatibleIOS: "iOS 17.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-15-pro", family: .s15, variant: "iPhone 15 Pro (6.1\")", machineIDs: ["iPhone16,1"], chip: "Apple A17 Pro", ramGB: 8, pixelWidth: 2556, pixelHeight: 1179, pointWidth: 852, pointHeight: 393, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 59, compatibleIOS: "iOS 17.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-15-pro-max", family: .s15, variant: "iPhone 15 Pro Max (6.7\")", machineIDs: ["iPhone16,2"], chip: "Apple A17 Pro", ramGB: 8, pixelWidth: 2796, pixelHeight: 1290, pointWidth: 932, pointHeight: 430, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 59, compatibleIOS: "iOS 17.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-16", family: .s16, variant: "iPhone 16 (6.1\")", machineIDs: ["iPhone17,3"], chip: "Apple A18", ramGB: 8, pixelWidth: 2556, pixelHeight: 1179, pointWidth: 852, pointHeight: 393, scale: 3, refreshRateHz: 60, cutoutType: "dynamic_island", safeLeftPt: 59, compatibleIOS: "iOS 18.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-16-plus", family: .s16, variant: "iPhone 16 Plus (6.7\")", machineIDs: ["iPhone17,4"], chip: "Apple A18", ramGB: 8, pixelWidth: 2796, pixelHeight: 1290, pointWidth: 932, pointHeight: 430, scale: 3, refreshRateHz: 60, cutoutType: "dynamic_island", safeLeftPt: 59, compatibleIOS: "iOS 18.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-16-pro", family: .s16, variant: "iPhone 16 Pro (6.3\")", machineIDs: ["iPhone17,1"], chip: "Apple A18 Pro", ramGB: 8, pixelWidth: 2622, pixelHeight: 1206, pointWidth: 874, pointHeight: 402, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 62, compatibleIOS: "iOS 18.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-16-pro-max", family: .s16, variant: "iPhone 16 Pro Max (6.9\")", machineIDs: ["iPhone17,2"], chip: "Apple A18 Pro", ramGB: 8, pixelWidth: 2868, pixelHeight: 1320, pointWidth: 956, pointHeight: 440, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 62, compatibleIOS: "iOS 18.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-16e", family: .s16, variant: "iPhone 16e (6.1\")", machineIDs: ["iPhone17,5"], chip: "Apple A18 (4-GPU)", ramGB: 8, pixelWidth: 2532, pixelHeight: 1170, pointWidth: 844, pointHeight: 390, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 18.3 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-17", family: .s17, variant: "iPhone 17 (6.3\")", machineIDs: ["iPhone18,3"], chip: "Apple A19", ramGB: 8, pixelWidth: 2622, pixelHeight: 1206, pointWidth: 874, pointHeight: 402, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 62, compatibleIOS: "iOS 26.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-air", family: .s17, variant: "iPhone Air (6.5\")", machineIDs: ["iPhone18,4"], chip: "Apple A19 Pro (5-GPU)", ramGB: 12, pixelWidth: 2736, pixelHeight: 1260, pointWidth: 912, pointHeight: 420, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 62, compatibleIOS: "iOS 26.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-17-pro", family: .s17, variant: "iPhone 17 Pro (6.3\")", machineIDs: ["iPhone18,1"], chip: "Apple A19 Pro", ramGB: 12, pixelWidth: 2622, pixelHeight: 1206, pointWidth: 874, pointHeight: 402, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 62, compatibleIOS: "iOS 26.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-17-pro-max", family: .s17, variant: "iPhone 17 Pro Max (6.9\")", machineIDs: ["iPhone18,2"], chip: "Apple A19 Pro", ramGB: 12, pixelWidth: 2868, pixelHeight: 1320, pointWidth: 956, pointHeight: 440, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 62, compatibleIOS: "iOS 26.0 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-17e", family: .s17, variant: "iPhone 17e (6.1\")", machineIDs: ["iPhone18,5"], chip: "Apple A19", ramGB: 8, pixelWidth: 2532, pixelHeight: 1170, pointWidth: 844, pointHeight: 390, scale: 3, refreshRateHz: 60, cutoutType: "notch", safeLeftPt: 47, compatibleIOS: "iOS 26.3 - 27.x", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-18-pro", family: .s18, variant: "iPhone 18 Pro (6.3\")", machineIDs: ["iPhone19,1"], chip: "Apple A20 Pro", ramGB: 12, pixelWidth: 2622, pixelHeight: 1206, pointWidth: 874, pointHeight: 402, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 54, compatibleIOS: "iOS 27.0+", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-18-pro-max", family: .s18, variant: "iPhone 18 Pro Max (6.9\")", machineIDs: ["iPhone19,2"], chip: "Apple A20 Pro", ramGB: 12, pixelWidth: 2868, pixelHeight: 1320, pointWidth: 956, pointHeight: 440, scale: 3, refreshRateHz: 120, cutoutType: "dynamic_island", safeLeftPt: 54, compatibleIOS: "iOS 27.0+", specVerified: true, specNotes: ""),
        IPhoneModelProfile(id: "iphone-18", family: .s18, variant: "iPhone 18 (Standard)", machineIDs: [], chip: "Chưa công bố (dự kiến X/2027)", ramGB: nil, pixelWidth: nil, pixelHeight: nil, pointWidth: nil, pointHeight: nil, scale: nil, refreshRateHz: nil, cutoutType: "dynamic_island", safeLeftPt: nil, compatibleIOS: "iOS 27.x (dự kiến)", specVerified: false, specNotes: "Thiếu cấu hình phần cứng xác minh (dự kiến mùa xuân 2027)."),
        IPhoneModelProfile(id: "iphone-18e", family: .s18, variant: "iPhone 18e", machineIDs: [], chip: "Chưa công bố", ramGB: nil, pixelWidth: nil, pixelHeight: nil, pointWidth: nil, pointHeight: nil, scale: nil, refreshRateHz: nil, cutoutType: "notch", safeLeftPt: nil, compatibleIOS: "iOS 27.x (dự kiến)", specVerified: false, specNotes: "Thiếu cấu hình phần cứng xác minh chính thức."),
        IPhoneModelProfile(id: "iphone-duo", family: .s18, variant: "iPhone Duo / Fold", machineIDs: [], chip: "Apple A20 Pro", ramGB: 12, pixelWidth: nil, pixelHeight: nil, pointWidth: nil, pointHeight: nil, scale: nil, refreshRateHz: 120, cutoutType: "foldable", safeLeftPt: nil, compatibleIOS: "iOS 27.0+", specVerified: false, specNotes: "Màn hình gập (~7.8\"/5.4\"), thiếu độ phân giải & safe-area xác minh."),
    ]

    static func model(forID id: String) -> IPhoneModelProfile? {
        all.first { $0.id == id }
    }

    static func model(matchingMachine machine: String) -> IPhoneModelProfile? {
        all.first { $0.machineIDs.contains(machine) }
    }

    static func models(in family: IPhoneFamily) -> [IPhoneModelProfile] {
        family == .all ? all : all.filter { $0.family == family }
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

struct IPhoneParsedLogs: Equatable {
    var deviceMachineID: String = ""
    var iosVersion: String = ""
    var ramMiB: Int = 0
    var buildStamp: String = ""
    var normalPlay = IPhonePerfScenarioMetrics()
    var heavyCrowd = IPhonePerfScenarioMetrics()
    var switchAttempts: Int = 0
    var switchPassed: Int = 0

    var switchVerdict: String {
        guard switchAttempts > 0 else { return "chưa đủ dữ liệu" }
        return switchPassed == switchAttempts ? "đạt" : "có giật/lag"
    }
}

enum IPhoneLogEvaluator {
    static func parse(lines: [String]) -> IPhoneParsedLogs {
        var out = IPhoneParsedLogs()
        var pendingVHPT = false
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
            }

            if line.contains("[switch] ["), line.contains("switch requested:") {
                pendingVHPT = line.contains(IPhoneTestCatalog.targetPackage)
            } else if line.contains("[switch] ["), line.contains("switch committed:"), pendingVHPT {
                out.switchAttempts += 1
                out.switchPassed += 1
                pendingVHPT = false
            } else if line.contains("[switch] ["), (line.contains("blocked") || line.contains("failed")), pendingVHPT {
                out.switchAttempts += 1
                pendingVHPT = false
            }

            if line.contains("java-perf:"), line.contains("pkg=\(IPhoneTestCatalog.targetPackage)") {
                let isCrowd = line.contains("scenario=heavy_crowd_effects") || line.lowercased().contains("crowd")
                let fps = extractDouble(line, prefix: "fps=")
                let meanMs = extractDouble(line, prefix: "mean_ms=")
                let maxMs = extractDouble(line, prefix: "max_ms=")
                let h50 = Int(extractDouble(line, prefix: "hitch50=") ?? 0)
                let h100 = Int(extractDouble(line, prefix: "hitch100=") ?? 0)
                if let fps, let meanMs, let maxMs {
                    if isCrowd {
                        out.heavyCrowd.samples += 1
                        out.heavyCrowd.fpsValues.append(fps)
                        out.heavyCrowd.meanValues.append(meanMs)
                        out.heavyCrowd.maxValues.append(maxMs)
                        out.heavyCrowd.hitch50 += h50
                        out.heavyCrowd.hitch100 += h100
                    } else {
                        out.normalPlay.samples += 1
                        out.normalPlay.fpsValues.append(fps)
                        out.normalPlay.meanValues.append(meanMs)
                        out.normalPlay.maxValues.append(maxMs)
                        out.normalPlay.hitch50 += h50
                        out.normalPlay.hitch100 += h100
                    }
                }
            }
        }
        return out
    }

    private static func extractDouble(_ text: String, prefix: String) -> Double? {
        guard let r = text.range(of: prefix) else { return nil }
        let tail = text[r.upperBound...]
        let token = tail.prefix { $0.isNumber || $0 == "." }
        return Double(token)
    }

    static func conclusion(for profile: IPhoneModelProfile, logs: IPhoneParsedLogs, currentMachine: String) -> (verdict: String, reason: String) {
        if !profile.specVerified {
            return ("chưa đủ dữ liệu", "Thiếu cấu hình phần cứng xác minh (\(profile.specNotes))")
        }
        let hostID = logs.deviceMachineID.isEmpty ? currentMachine : logs.deviceMachineID
        guard !hostID.isEmpty, profile.machineIDs.contains(hostID) else {
            let cur = hostID.isEmpty ? "không rõ" : hostID
            return ("chưa đủ dữ liệu", "Chưa có thiết bị thật khớp mẫu này (máy hiện tại: \(cur))")
        }
        let v1 = logs.normalPlay.verdict
        let v2 = logs.heavyCrowd.verdict
        let v3 = logs.switchVerdict
        if v1 == "có giật/lag" || v2 == "có giật/lag" || v3 == "có giật/lag" {
            return ("có giật/lag", "Đo trực tiếp trên \(hostID) vượt ngưỡng 60 FPS / giật khung hình")
        }
        if v1 == "đạt" && v2 == "đạt" && v3 == "đạt" {
            return ("đạt", "Đủ 3 kịch bản trên máy thật \(hostID)")
        }
        var missing: [String] = []
        if v1 == "chưa đủ dữ liệu" { missing.append("Chơi bình thường") }
        if v2 == "chưa đủ dữ liệu" { missing.append("Cảnh đông hiệu ứng") }
        if v3 == "chưa đủ dữ liệu" { missing.append("Chuyển game qua lại") }
        return ("chưa đủ dữ liệu", "Trên \(hostID) còn thiếu log kịch bản: \(missing.joined(separator: ", "))")
    }
}

enum IPhoneTestProfile {
    static let selectedModelIDKey = "husk.testSuite.modelID"
    static let displayOverrideKey = "husk.testSuite.displayOverride"

    static func selectedModel(defaultMachine: String = "") -> IPhoneModelProfile {
        if let saved = UserDefaults.standard.string(forKey: selectedModelIDKey),
           let m = IPhoneTestCatalog.model(forID: saved) {
            return m
        }
        if !defaultMachine.isEmpty, let matched = IPhoneTestCatalog.model(matchingMachine: defaultMachine) {
            return matched
        }
        return IPhoneTestCatalog.model(forID: "iphone-15-pro-max") ?? IPhoneTestCatalog.all[0]
    }

    static var isDisplayOverrideEnabled: Bool {
        get { UserDefaults.standard.bool(forKey: displayOverrideKey) }
        set { UserDefaults.standard.set(newValue, forKey: displayOverrideKey) }
    }

    static func activeDisplayOverride(defaultMachine: String = "") -> IPhoneModelProfile? {
        guard isDisplayOverrideEnabled else { return nil }
        let m = selectedModel(defaultMachine: defaultMachine)
        guard m.specVerified, m.pixelWidth != nil, m.pixelHeight != nil, m.pointWidth != nil, m.pointHeight != nil, m.scale != nil else {
            return nil
        }
        return m
    }
}

#if canImport(SwiftUI) && canImport(UIKit)
import SwiftUI
import UIKit

struct IPhonePerfSuiteView: View {
    var defaultPackage: String? = nil

    @State private var selectedFamily: IPhoneFamily = .all
    @State private var selectedModelID: String = UserDefaults.standard.string(forKey: IPhoneTestProfile.selectedModelIDKey) ?? ""
    @State private var displayOverride: Bool = IPhoneTestProfile.isDisplayOverrideEnabled
    @State private var parsed = IPhoneParsedLogs()
    @State private var copiedNotice = false

    private var currentMachine: String { HuskLog.deviceModel }
    private var currentIOS: String { "\(UIDevice.current.systemName) \(UIDevice.current.systemVersion)" }
    private var currentBuild: String {
        let b = (Bundle.main.infoDictionary?["CFBundleVersion"] as? String) ?? "?"
        return "\(Bundle.main.version) (\(b)) \(Bundle.main.commit)"
    }

    private var filteredModels: [IPhoneModelProfile] {
        IPhoneTestCatalog.models(in: selectedFamily)
    }

    private var selectedModel: IPhoneModelProfile {
        if let m = IPhoneTestCatalog.model(forID: selectedModelID) { return m }
        return IPhoneTestProfile.selectedModel(defaultMachine: currentMachine)
    }

    var body: some View {
        HuskForm {
            Section {
                Picker("Dòng máy", selection: $selectedFamily) {
                    ForEach(IPhoneFamily.allCases) { f in Text(f.rawValue).tag(f) }
                }
                Picker("Mẫu & biến thể iPhone", selection: $selectedModelID) {
                    ForEach(filteredModels) { m in
                        let tag = m.specVerified ? "" : " [Thiếu xác minh]"
                        Text("\(m.variant)\(tag)").tag(m.id)
                    }
                }
                if let nativeMatch = IPhoneTestCatalog.model(matchingMachine: currentMachine) {
                    Button {
                        selectedFamily = .all
                        selectedModelID = nativeMatch.id
                        saveSelection()
                    } label: {
                        Label("Chọn đúng máy đang dùng (\(nativeMatch.variant))", systemImage: "iphone.gen3")
                    }
                }
            } header: {
                Text("Chọn iPhone kiểm thử (iPhone 8 → iPhone 18)")
            } footer: {
                Text("Cho phép chọn từng mẫu và biến thể từ iPhone 8 đến iPhone 18 Pro Max (cả các biến thể chưa có cấu hình xác minh như iPhone 18 chuẩn, 18e, Duo).")
            }

            Section {
                let m = selectedModel
                DetailRow(label: "Mẫu máy", value: m.variant, mono: false)
                DetailRow(label: "Machine ID", value: m.machineIDs.isEmpty ? "Chưa công bố" : m.machineIDs.joined(separator: ", "))
                DetailRow(label: "Chip", value: m.chip, mono: false)
                DetailRow(label: "RAM", value: m.ramLabel, mono: false)
                DetailRow(label: "Độ phân giải", value: m.resolutionLabel, mono: false)
                DetailRow(label: "Tần số quét", value: m.refreshLabel, mono: false)
                DetailRow(label: "Vùng cắt / Safe Left", value: m.safeLeftPt.map { "\(m.cutoutType) · \($0) pt (\($0 == 0 ? 0 : m.safeLeftPx) px)" } ?? "\(m.cutoutType) (Chưa xác minh)", mono: false)
                DetailRow(label: "iOS tương thích", value: m.compatibleIOS, mono: false)
                DetailRow(label: "Trạng thái cấu hình", value: m.specVerified ? "Đã xác minh" : "Thiếu cấu hình xác minh", mono: false)
                if m.specVerified && m.pixelWidth != nil {
                    Toggle("Áp dụng màn hình & Safe-Area của mẫu này khi mở game", isOn: $displayOverride)
                        .onChange(of: displayOverride) { v in
                            IPhoneTestProfile.isDisplayOverrideEnabled = v
                            HuskLog.log("perf-suite", "displayOverride=\(v) model=\(m.id)")
                        }
                }
            } header: {
                Text("Cấu hình đã chọn")
            } footer: {
                if displayOverride && selectedModel.specVerified {
                    Text("Chế độ test kích thước màn hình đang BẬT cho lần mở game tiếp theo: khung game & Safe-Area sẽ mô phỏng \(selectedModel.resolutionLabel), phù hợp để kiểm tra bố cục HUD trên các cỡ màn hình (kết luận FPS chỉ tính từ máy thật trùng Machine ID).")
                } else {
                    Text("Các mẫu thiếu cấu hình xác minh được đánh dấu rõ và không thể dùng để kết luận Đạt.")
                }
            }

            Section {
                let res = IPhoneLogEvaluator.conclusion(for: selectedModel, logs: parsed, currentMachine: currentMachine)
                DetailRow(label: "Thiết bị đang đo", value: "\(currentMachine) · \(currentIOS)", mono: false)
                DetailRow(label: "Bản build", value: parsed.buildStamp.isEmpty ? currentBuild : parsed.buildStamp)
                DetailRow(label: "Chơi bình thường", value: formatScenario(parsed.normalPlay))
                DetailRow(label: "Cảnh đông hiệu ứng", value: formatScenario(parsed.heavyCrowd))
                DetailRow(label: "Chuyển game qua lại", value: parsed.switchAttempts > 0 ? "\(parsed.switchPassed)/\(parsed.switchAttempts) lượt (\(parsed.switchVerdict))" : "chưa đủ dữ liệu")
                DetailRow(label: "Kết luận cho mẫu", value: res.verdict.uppercased(), mono: false)
                Text(res.reason).font(.caption).foregroundStyle(.secondary)
            } header: {
                Text("Số liệu đo trên máy thật (\(IPhoneTestCatalog.targetPackage))")
            } footer: {
                Text("Ngưỡng Đạt (mục tiêu 60 FPS): avg ≥ 54.0 fps, mean ≤ 18.5 ms, p95 ≤ 22.0 ms, max ≤ 50.0 ms, hitch ≥ 100 ms = 0, hitch ≥ 50 ms ≤ 2.0/phút, chuyển game 0 lỗi.")
            }

            Section {
                Button { reloadLogs() } label: {
                    Label("Làm mới số liệu từ Log", systemImage: "arrow.clockwise")
                }
                Button {
                    UIPasteboard.general.string = exportMarkdown()
                    copiedNotice = true
                } label: {
                    Label(copiedNotice ? "Đã sao chép bảng kết quả!" : "Copy Bảng Kết Quả (Markdown)", systemImage: "doc.on.doc")
                }
            }
        }
        .huskForm()
        .navigationTitle("iPhone 8–18 Test Suite")
        .onAppear {
            if selectedModelID.isEmpty {
                selectedModelID = IPhoneTestProfile.selectedModel(defaultMachine: currentMachine).id
            }
            reloadLogs()
        }
        .onChange(of: selectedModelID) { _ in saveSelection() }
        .onChange(of: selectedFamily) { _ in
            if !filteredModels.contains(where: { $0.id == selectedModelID }), let first = filteredModels.first {
                selectedModelID = first.id
                saveSelection()
            }
        }
    }

    private func saveSelection() {
        UserDefaults.standard.set(selectedModelID, forKey: IPhoneTestProfile.selectedModelIDKey)
        HuskLog.log("perf-suite", "selected model=\(selectedModelID) variant=\(selectedModel.variant)")
    }

    private func formatScenario(_ s: IPhonePerfScenarioMetrics) -> String {
        guard s.samples > 0 else { return "chưa đủ dữ liệu" }
        return String(format: "%.1f fps · mean %.2fms · p95 %.2fms · max %.2fms · h50=%d h100=%d (%@)",
                      s.avgFPS, s.meanMs, s.p95Ms, s.maxMs, s.hitch50, s.hitch100, s.verdict)
    }

    private func reloadLogs() {
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        var lines: [String] = []
        for name in ["husk.log", "husk-native.log"] {
            let u = docs.appendingPathComponent(name)
            if let text = try? String(contentsOf: u, encoding: .utf8) {
                lines.append(contentsOf: text.components(separatedBy: .newlines))
            }
        }
        parsed = IPhoneLogEvaluator.parse(lines: lines)
    }

    private func exportMarkdown() -> String {
        var rows = [
            "| Mẫu iPhone | Chip | RAM | Độ phân giải | Tần số | iOS | Build | Kết luận |",
            "|---|---|---:|---|---:|---|---|---|",
        ]
        for m in IPhoneTestCatalog.all {
            let r = IPhoneLogEvaluator.conclusion(for: m, logs: parsed, currentMachine: currentMachine)
            let bld = (m.machineIDs.contains(currentMachine) && !parsed.buildStamp.isEmpty) ? parsed.buildStamp : (m.machineIDs.contains(currentMachine) ? currentBuild : "Chưa đo")
            rows.append("| \(m.variant) | \(m.chip) | \(m.ramLabel) | \(m.resolutionLabel) | \(m.refreshLabel) | \(m.compatibleIOS) | \(bld) | \(r.verdict) |")
        }
        return rows.joined(separator: "\n")
    }
}
#endif
