// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Why an app will not run, or why it stopped, in words: from what the APK is (before it starts) and from what its run logged.
///
/// Before a start it answers the questions the files can: is this an app at all, does it have code for this iPhone's CPU, is a
/// part of it missing, does it carry protection that refuses to run anywhere but a certified Android phone. After a run it reads
/// the log for the line that ended it: a crash in one of the app's libraries, the app closing itself, an exception from its Java
/// code, a feature of Android that Husk does not have yet, Google Play services it cannot do without.
struct AppDiagnosis: Identifiable, Equatable {
    enum Kind: String {
        case badAPK, unsupportedCPU, missingSplits, protected, needsGoogle, missingFeature, javaCrash, nativeCrash, closedItself,
             missingFunction, noProgress, noJIT, outOfAddressSpace
    }
    let id = UUID()
    let kind: Kind
    let title: String
    let detail: String
    /// The log line or report field it came from, for the curious and for bug reports.
    let evidence: String?
    /// A warning before launch that still lets the app be tried.
    var blocksLaunch: Bool { kind == .badAPK || kind == .unsupportedCPU }

    static func == (a: AppDiagnosis, b: AppDiagnosis) -> Bool { a.kind == b.kind && a.title == b.title && a.detail == b.detail }

    var symbol: String {
        switch kind {
        case .badAPK: return "doc.badge.ellipsis"
        case .unsupportedCPU: return "cpu"
        case .missingSplits: return "square.stack.3d.up.slash"
        case .protected, .closedItself: return "lock.shield"
        case .needsGoogle: return "person.crop.circle.badge.exclamationmark"
        case .missingFeature, .missingFunction: return "puzzlepiece"
        case .javaCrash, .nativeCrash: return "exclamationmark.triangle.fill"
        case .noProgress: return "hourglass"
        case .noJIT: return "bolt.slash"
        case .outOfAddressSpace: return "memorychip"
        }
    }

    // MARK: before a start

    /// Libraries of commercial app protectors: they check the device and the app's own files, and stop the app off a real phone.
    private static let protectorLibraries = ["libjiagu", "libDexHelper", "libsecexe", "libsecmain", "libprotectClass", "libdexprotector",
                                             "libapp_shield", "libNSaferOnly", "libshell-super", "libtup", "libAppGuard", "libxloader",
                                             "libarmour", "libnesec", "libSecShell", "libkwscmm", "libmobisec",
                                             // seen in Play apps: ByteDance's metasec (TikTok, CapCut), Netflix's, Akamai's bot manager
                                             // (eBay), and games whose own library refuses uncertified devices (Clash Royale, HCR2)
                                             "libmetasec", "libcf2a", "libakamaibap", "libakamaibmp", "libk30d", "libsupercell_", "libfingersoft_hcr2", "libfp"]

    static func preflight(_ app: TLApp) -> AppDiagnosis? {
        guard let r = app.report else { return nil }
        if !r.ok || r.verdict == "unreadable" {
            return AppDiagnosis(kind: .badAPK, title: "Bad APK",
                                detail: "This file is not an app Husk can read" + (r.error.map { ": \($0)." } ?? ".")
                                    + " It may be damaged, only partly downloaded, or not an Android app at all.",
                                evidence: r.error ?? r.summary)
        }
        if r.verdict == "noArm64" {
            return AppDiagnosis(kind: .unsupportedCPU, title: "Not supported: wrong kind of CPU code",
                                detail: r.summary + " iPhones run 64-bit ARM (arm64) code. Look for an arm64 or universal build of this app.",
                                evidence: "ABIs: " + r.abis.joined(separator: ", "))
        }
        if app.apks.count == 1, let base = app.apks.first, husk_apk_split_required(base) != 0 {
            return AppDiagnosis(kind: .missingSplits, title: "Incomplete app",
                                detail: "This app is published in several pieces (split APKs) and only its base was imported, so its libraries or "
                                    + "resources are missing. Import the whole bundle (.apks, .xapk or .apkm), or get it from Husk's store.",
                                evidence: "manifest: split APKs required")
        }
        if let lib = r.libraries.first(where: { l in protectorLibraries.contains(where: { l.name.hasPrefix($0) }) }) {
            return AppDiagnosis(kind: .protected, title: "Protected app",
                                detail: "This app carries anti-tamper protection (\(lib.name)). Such protection checks that it runs on a certified "
                                    + "Android phone and closes the app otherwise, so it may not start in Husk.",
                                evidence: lib.name)
        }
        return nil
    }

    // MARK: after a run

    /// The reason a run stopped, from its log; nil when nothing in it explains the end (the app closed the normal way).
    static func fromLog(_ text: String) -> AppDiagnosis? {
        let lines = text.split(separator: "\n", omittingEmptySubsequences: true).map(String.init)

        // No executable memory: on iOS with TXM only a debugger can grant it, and without it the loader cannot load Android's own
        // libraries, so everything after (java.lang.System failing to initialise, missing natives) follows from this
        if let l = lines.first(where: { $0.contains("no StikDebug JIT region") }) {
            return AppDiagnosis(kind: .noJIT, title: "JIT is not enabled",
                                detail: "This iPhone only lets an app run translated code while a debugger enables JIT for it. Open Husk "
                                    + "through StikDebug, or tap Turn On JIT in Husk, then start the app again.",
                                evidence: l)
        }

        // iOS closed Husk for memory: the runtime's last readings ("N MiB left before jetsam") had fallen to almost nothing. Checked
        // before what the log said earlier in the run, which may only be a warning (#175: an OBB note, then an out-of-memory end).
        let left = lines.suffix(40).compactMap { l -> Int? in
            guard let r = l.range(of: " MiB left before jetsam") else { return nil }
            return Int(l[..<r.lowerBound].split(separator: " ").last ?? "")
        }
        if let last = left.last, last < 80 {
            return AppDiagnosis(kind: .noProgress, title: "Ran out of memory",
                                detail: "iOS closed Husk because the game used all the memory it allows an app (\(last) MiB were left). "
                                    + "Close other apps and try again; lower graphics settings in the game help. On SideStore or AltStore, "
                                    + "GetMoreRam raises the limit.",
                                evidence: lines.last { $0.contains("left before jetsam") })
        }

        // The JIT region is shared: Android (QEMU) holds its top half while it runs, an app's libraries fill it from the bottom
        if let l = lines.first(where: { $0.contains("but QEMU holds its top") || $0.contains("of TCG") && $0.contains("no room for") }) {
            return AppDiagnosis(kind: .noJIT, title: "Not enough JIT memory",
                                detail: "Android and this app share the memory Husk runs translated code in, and together they do not fit. "
                                    + "Close Husk and open it again, then start only the app (or only Android).",
                                evidence: l)
        }

        // Out of address space, not memory: a reservation of address space was refused (an allocator's PROT_NONE mmap failing with
        // ENOMEM), and the engine gave up. Unity's own allocators reserve 256 MiB regions, as 512 MiB each, far ahead of use; an iOS
        // app without the extended-virtual-addressing entitlement has a few GiB of address space, and installs re-signed with their
        // user's own team often lose it. What follows the refusal (a crash, iOS closing Husk) follows from it.
        if let i = lines.firstIndex(where: { $0.contains("mmap FAILED") && $0.contains(" prot=0 ") && $0.contains("errno=12") }),
           let oom = lines[i...].first(where: { $0.contains("System out of memory!") }) {
            return AppDiagnosis(kind: .outOfAddressSpace, title: String(localized: "Out of address space"),
                                detail: String(localized: """
                                        The game ran out of address space, not memory: iOS refused to set aside more addresses for it. An app \
                                        without the extended virtual addressing entitlement gets only a few GB of addresses, and installs \
                                        re-signed with your own Apple account or team often lose that entitlement. Install Husk with it kept, \
                                        then start the game again.
                                        """),
                                evidence: lines[i] + "\n" + oom)
        }

        // A native crash: the runtime's crash handler names the library the bad instruction was in
        if let i = lines.lastIndex(where: { $0.contains("=== CRASH:") || $0.contains("*** FATAL SIGNAL") }) {
            let head = lines[i]
            let lib = lines[(i + 1)..<min(lines.count, i + 4)].compactMap { l -> String? in
                guard l.contains("pc ") else { return nil }
                return l.split(separator: " ").map(String.init).first { $0.hasSuffix(".so") || $0.contains(".so") }
            }.first
            let signal = head.range(of: "signal \\d+", options: .regularExpression).map { String(head[$0]) } ?? "a fatal signal"
            if let lib, !lib.hasPrefix("libhusk"), !lib.contains("Husk") {
                return AppDiagnosis(kind: .nativeCrash, title: "Crashed in \(lib)",
                                    detail: "The app's own native code (\(lib)) stopped with \(signal). It most likely needs something from Android "
                                        + "that Husk does not provide yet, or does something iOS does not allow.",
                                    evidence: head)
            }
            return AppDiagnosis(kind: .nativeCrash, title: "The app crashed",
                                detail: "It stopped with \(signal). The report has the full log of the run.", evidence: head)
        }

        // The app ended itself from native code: protection that found it is not on a certified phone, most of the time
        if let l = lines.last(where: { $0.contains("bionic: exit(") && $0.contains("called from") }),
           let code = l.range(of: "exit\\((\\d+)\\)", options: .regularExpression).map({ String(l[$0]) }), code != "exit(0)" {
            let lib = l.components(separatedBy: "called from ").last?.split(separator: " ").first.map(String.init) ?? "its code"
            return AppDiagnosis(kind: .closedItself, title: "The app refused to run here",
                                detail: "\(lib) closed the app on purpose (\(code)). Apps with anti-tamper or anti-cheat protection do this when "
                                    + "they find they are not on a certified Android phone, which Husk cannot pretend to be for them.",
                                evidence: l)
        }

        // Java: the exception that ended the app's main thread
        if let l = lines.last(where: { $0.contains("main thread ended with ") }) {
            let what = l.components(separatedBy: "main thread ended with ").last ?? l
            let name = String(what.prefix { $0 != ":" && $0 != " " })
            let short = name.split(separator: ".").last.map(String.init) ?? name
            let message = what.dropFirst(name.count).drop { $0 == ":" || $0 == " " }
            let firstPart = message.components(separatedBy: " | caused by").first ?? String(message)
            if ["NoSuchMethodError", "NoClassDefFoundError", "UnsatisfiedLinkError", "AbstractMethodError", "NoSuchFieldError",
                "IncompatibleClassChangeError"].contains(short) {
                return AppDiagnosis(kind: .missingFeature, title: "Uses something Husk does not have yet",
                                    detail: "The app needs a part of Android that Husk's runtime does not provide yet: \(apiName(firstPart)).",
                                    evidence: l)
            }
            if what.localizedCaseInsensitiveContains("google play services") || what.contains("GooglePlayServices") {
                return googleDiagnosis(l)
            }
            return AppDiagnosis(kind: .javaCrash, title: "The app crashed",
                                detail: "Its code stopped with \(short)" + (firstPart.isEmpty ? "." : ": \(firstPart.prefix(220))"),
                                evidence: l)
        }

        // Google Play services the app will not run without
        if let l = lines.last(where: { $0.contains("requires Google Play services") || $0.contains("Failed to fetch kids onboarding status")
                                        || $0.contains("GooglePlayServicesNotAvailableException") }),
           lines.contains(where: { $0.contains("the app finished") }) {
            return googleDiagnosis(l)
        }

        // An app whose work is done by Linux programs it starts as processes of their own (Winlator's and GameNative's Box64 and Wine,
        // a proot userland): its own screens run, the programs can not, as an iOS app may not start processes
        if let l = lines.first(where: { $0.contains("the app started a process:") }),
           ["box64", "box86", "wine", "proot", "/usr/bin/", "imagefs", "rootfs", "termux", "ld-linux"].contains(where: { l.localizedCaseInsensitiveContains($0) }) {
            let prog = l.components(separatedBy: "the app started a process: ").last?.components(separatedBy: " -- ").first ?? "a program"
            return AppDiagnosis(kind: .missingFeature, title: "Runs Linux programs as separate processes",
                                detail: "This app does its main work in Linux programs it starts as processes of their own (here \(prog.prefix(120))). "
                                    + "Its own screens run in Husk, but iOS does not let an app start other processes, so those programs, and the "
                                    + "games or tools they run, can not start yet.",
                                evidence: l)
        }

        // An app that only works signed in to a Google account: it asked Android's account picker, got none, and closed
        if let l = lines.last(where: { $0.contains("ChooseTypeAndAccountActivity") || $0.contains("AccountManager.newChooseAccountIntent") }),
           lines.contains(where: { $0.contains("the app finished") }) {
            return AppDiagnosis(kind: .needsGoogle, title: "Needs a Google account",
                                detail: "This app only works signed in to a Google account. It asked Android to pick one, and Husk has no Google "
                                    + "accounts to offer, so the app closed.",
                                evidence: l)
        }

        // A game whose data comes as an expansion file the store did not install
        if let l = lines.first(where: { $0.contains("expansion file (OBB) is missing") }) {
            return AppDiagnosis(kind: .missingSplits, title: "Missing game data",
                                detail: "This game keeps most of its data in an expansion file (OBB) that the Play Store downloads separately "
                                    + "from the app. Husk installed the app itself but not that file, so the game cannot load.",
                                evidence: l)
        }

        // Video that only plays with DRM keys
        if let l = lines.first(where: { $0.contains("Husk cannot decrypt protected media") }) {
            return AppDiagnosis(kind: .protected, title: "Protected video is not supported",
                                detail: "The app's video is DRM-protected (Widevine), which only certified Android devices can decrypt. "
                                    + "Unprotected video plays; protected video stays black.",
                                evidence: l)
        }

        // A function of Android's C library or system libraries that was never written for Husk
        if let l = lines.first(where: { $0.contains("CALLED an unresolved import: ") }), lines.contains(where: { $0.contains("the app finished") || $0.contains("CRASH") }) {
            let fn = l.components(separatedBy: "unresolved import: ").last?.split(separator: " ").first.map(String.init) ?? "?"
            return AppDiagnosis(kind: .missingFunction, title: "Uses something Husk does not have yet",
                                detail: "The app called \(fn), a system function Husk does not provide yet.", evidence: l)
        }
        return nil
    }

    private static func googleDiagnosis(_ line: String) -> AppDiagnosis {
        AppDiagnosis(kind: .needsGoogle, title: "Needs Google Play services",
                     detail: "This app relies on Google Play services (Google sign-in, Google's account and device checks) for more than Husk "
                        + "can answer for. Apps that only use Google's libraries run; this one stops without the real services.",
                     evidence: line)
    }

    /// "android/os/StrictMode$ThreadPolicy$Builder.penaltyListener(...)" as "StrictMode.ThreadPolicy.Builder.penaltyListener".
    private static func apiName(_ s: String) -> String {
        var t = s.components(separatedBy: "(").first ?? s
        t = t.replacingOccurrences(of: "No implementation found for ", with: "")
        t = t.replacingOccurrences(of: "$", with: ".")
        let parts = t.split(separator: "/").map(String.init)
        return (parts.count > 1 ? parts.suffix(2).joined(separator: ".") : t).trimmingCharacters(in: .whitespaces)
    }
}

/// A diagnosis, as a card: on an app's page before it starts, and over an app's screen when it stops.
struct AppDiagnosisCard: View {
    let diagnosis: AppDiagnosis
    var showsEvidence = false

    var body: some View {
        HStack(alignment: .top, spacing: 12) {
            Image(systemName: diagnosis.symbol)
                .font(.system(size: 20, weight: .semibold))
                .foregroundStyle(diagnosis.blocksLaunch ? .red : .orange)
                .frame(width: 26)
            VStack(alignment: .leading, spacing: 4) {
                Text(diagnosis.title).font(.subheadline.weight(.semibold))
                Text(diagnosis.detail).font(.footnote).foregroundStyle(.secondary).fixedSize(horizontal: false, vertical: true)
                if showsEvidence, let e = diagnosis.evidence {
                    Text(e).font(.system(size: 10.5, design: .monospaced)).foregroundStyle(.secondary)
                        .lineLimit(4).textSelection(.enabled).padding(.top, 2)
                }
            }
            Spacer(minLength: 0)
        }
        .padding(14)
        .background(Color(.secondarySystemBackground), in: RoundedRectangle(cornerRadius: 14, style: .continuous))
    }
}

/// Over an app's screen when its run ends with a reason: what happened, and the way out.
struct AppStoppedOverlay: View {
    let app: TLApp
    let diagnosis: AppDiagnosis
    let close: () -> Void
    @State private var report: URL?

    var body: some View {
        ZStack {
            Color.black.opacity(0.55).ignoresSafeArea()
            VStack(alignment: .leading, spacing: 14) {
                Text(app.label).font(.footnote.weight(.semibold)).foregroundStyle(.secondary)
                AppDiagnosisCard(diagnosis: diagnosis, showsEvidence: true)
                HStack(spacing: 10) {
                    if let report {
                        HuskShareLink(item: report) { Label("Share Report", systemImage: "square.and.arrow.up").frame(maxWidth: .infinity) }
                            .buttonStyle(.bordered)
                    }
                    Button(action: close) { Text("Close").frame(maxWidth: .infinity) }
                        .buttonStyle(.borderedProminent)
                }
            }
            .padding(18)
            .frame(maxWidth: 440)
            .background(Color(.systemBackground), in: RoundedRectangle(cornerRadius: 20, style: .continuous))
            .padding(24)
        }
        .task { report = AppDiagnosis.writeReport(app: app, diagnosis: diagnosis) }
    }
}

extension AppDiagnosis {
    /// The run that just ended, in one text file to share: the reason, the app, this device, and the runtime's log of the run.
    @MainActor static func writeReport(app: TLApp, diagnosis d: AppDiagnosis) -> URL? {
        let info = Bundle.main.infoDictionary ?? [:]
        var text = """
        Husk app report
        ===============
        App      : \(app.label) (\(app.packageName ?? "?"))
        What     : \(d.title)
        Why      : \(d.detail)
        Evidence : \(d.evidence ?? "-")
        Husk     : \(info["CFBundleShortVersionString"] as? String ?? "?") (\(info["CFBundleVersion"] as? String ?? "?"))
        Device   : \(HuskLog.deviceModel), \(UIDevice.current.systemName) \(UIDevice.current.systemVersion)
        APKs     : \(app.apks.map { ($0 as NSString).lastPathComponent }.joined(separator: ", "))
        Engine   : \(app.report?.runnerName ?? "?")

        ---- log of the run ----

        """
        if let c = husk_tl_attempt_log() { text += String(cString: c); free(c) }
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("Husk report - \(app.label.replacingOccurrences(of: "/", with: "-")).txt")
        do { try text.write(to: url, atomically: true, encoding: .utf8) } catch { return nil }
        return url
    }

    /// When a run has ended: why, from its log (nil when it closed the normal way).
    static func ofCurrentRun() -> AppDiagnosis? {
        guard let c = husk_tl_attempt_log() else { return nil }
        defer { free(c) }
        return fromLog(String(cString: c))
    }
}
