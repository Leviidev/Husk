// SPDX-License-Identifier: GPL-2.0-or-later
import AVFoundation
import BackgroundTasks
import Foundation
import UIKit
#if canImport(ActivityKit)
import ActivityKit
#endif

/// Downloads from a link: one file, or a whole folder served from a computer (tools/serve-folder.py).
///
/// Every file is written into a `.part` beside where it goes and continued with a Range request from what is already on disk, so
/// nothing is fetched twice -- after a crash, an update, a server restart or a dropped connection. A file already in place at its
/// full size is not fetched at all. A finished APK is added to the library; anything else goes into Shared Storage at the path it had
/// in the served folder, which is where games that keep their data on /sdcard look for it.
///
/// Two ways to move the bytes:
///   - while Husk runs (in front, or kept running in the background by an iOS 26 continued processing task) it downloads itself,
///     several files at once, which is much faster than iOS's download service gives a background session;
///   - when Husk leaves the screen without that task, what is left of each file is handed to a background URLSession, which iOS
///     carries on with while Husk is suspended or closed. Those tasks are made while Husk is still active, or iOS would treat them as
///     work to do whenever it suits it.
@MainActor
final class Downloads: ObservableObject {
    static let shared = Downloads()

    struct File: Codable, Hashable {
        var path: String            // where it goes, relative to Shared Storage (a folder) or its file name (a single file)
        var url: String
        var size: Int64             // 0 when not known
        var received: Int64 = 0
        var done = false
        var error: String?
    }

    struct Job: Codable, Identifiable, Hashable {
        enum Kind: String, Codable { case file, folder, apk }
        var id = UUID()
        var title: String
        var source: String
        var kind: Kind
        var files: [File]
        var added = Date()
        var paused = false
        var finished: Date?

        var total: Int64 { files.reduce(0) { $0 + max($1.size, $1.received) } }
        var received: Int64 { files.reduce(0) { $0 + ($1.done ? max($1.size, $1.received) : $1.received) } }
        var filesDone: Int { files.filter(\.done).count }
        var failed: Bool { files.contains { $0.error != nil } }
        var complete: Bool { files.allSatisfy(\.done) }
    }

    @Published private(set) var jobs: [Job] = []
    /// Bytes per second across everything running, smoothed.
    @Published private(set) var speed: Double = 0
    @Published var problem: String?
    /// Live Activities are turned off for Husk in Settings, so a download cannot show on the Lock Screen or in the Dynamic Island.
    @Published var liveActivitiesOff = false

    nonisolated static let sessionID = "com.husk.downloads"
    /// How many files at once.
    private static let parallel = 4

    private let background: URLSession
    private let backgroundDelegate = BackgroundDownloadDelegate()
    private let direct: URLSession
    private let directDelegate = DirectDownloadDelegate()
    /// Files being fetched by Husk itself, and files handed to the background session ("job|path").
    private var directRunning: [String: URLSessionDataTask] = [:]
    private var handedOff: Set<String> = []
    /// Husk can download itself: it is on screen, or a continued processing task keeps it running.
    private var awake = true
    private var grace: UIBackgroundTaskIdentifier = .invalid
    private var retries: [String: Int] = [:]
    /// From the app delegate, when iOS woke Husk for the background session's events: call it once they are handled.
    var backgroundCompletion: (() -> Void)?

    private init() {
        let bg = URLSessionConfiguration.background(withIdentifier: Self.sessionID)
        bg.isDiscretionary = false
        bg.sessionSendsLaunchEvents = true
        bg.allowsCellularAccess = true
        bg.httpMaximumConnectionsPerHost = Self.parallel
        bg.timeoutIntervalForRequest = 120
        bg.timeoutIntervalForResource = 7 * 24 * 3600
        background = URLSession(configuration: bg, delegate: backgroundDelegate, delegateQueue: nil)

        let fg = URLSessionConfiguration.default
        fg.httpMaximumConnectionsPerHost = Self.parallel
        fg.timeoutIntervalForRequest = 60
        fg.requestCachePolicy = .reloadIgnoringLocalCacheData
        fg.urlCache = nil
        let q = OperationQueue()
        q.maxConcurrentOperationCount = 1
        q.name = "husk.downloads.direct"
        direct = URLSession(configuration: fg, delegate: directDelegate, delegateQueue: q)

        jobs = Self.loadJobs()
        awake = UIApplication.shared.applicationState != .background
        let nc = NotificationCenter.default
        nc.addObserver(forName: UIApplication.willResignActiveNotification, object: nil, queue: .main) { _ in
            MainActor.assumeIsolated { Downloads.shared.leaving() }
        }
        nc.addObserver(forName: UIApplication.didBecomeActiveNotification, object: nil, queue: .main) { _ in
            MainActor.assumeIsolated { Downloads.shared.returned() }
        }
        settleFromDisk()
        adoptBackgroundTasks()
    }

    // MARK: adding

    /// Work out what a link is -- a folder's manifest, an APK, or any other file -- and start on it.
    func add(link raw: String) async {
        problem = nil
        let text = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        guard var url = URL(string: text.contains("://") ? text : "http://" + text), url.scheme?.hasPrefix("http") == true else {
            problem = L10n.tr("That is not a web address.")
            return
        }
        HuskLog.log("downloads", "adding \(url.absoluteString)")
        // A served folder answers its root with a manifest; so may any link that returns JSON listing files.
        if let manifest = await Self.fetchManifest(url) {
            if !url.absoluteString.hasSuffix("/") { url = url.appendingPathComponent("") }
            let files = manifest.files.map { f -> File in
                let path = f.path.split(separator: "/").map { $0.addingPercentEncoding(withAllowedCharacters: .urlPathAllowed) ?? String($0) }.joined(separator: "/")
                return File(path: f.path, url: URL(string: "f/" + path, relativeTo: url)!.absoluteString, size: f.size)
            }
            guard !files.isEmpty else { problem = L10n.tr("That folder is empty."); return }
            start(Job(title: manifest.name.isEmpty ? (url.host ?? L10n.tr("Folder")) : manifest.name, source: url.absoluteString, kind: .folder, files: files))
            return
        }
        let (name, size) = await Self.probe(url)
        let kind: Job.Kind = name.lowercased().hasSuffix(".apk") ? .apk : .file
        start(Job(title: name, source: url.absoluteString, kind: kind, files: [File(path: name, url: url.absoluteString, size: size)]))
    }

    private func start(_ new: Job) {
        // The same link again: carry on with the one already there, rather than fetch anything twice.
        if let existing = jobs.first(where: { $0.source == new.source }) {
            HuskLog.log("downloads", "\(new.title) is already in the list; continuing it")
            if let i = jobs.firstIndex(where: { $0.id == existing.id }) {
                // The folder may have changed on the computer: take the new list, keeping what is known about each file.
                var merged = new
                merged.id = existing.id
                merged.added = existing.added
                for k in merged.files.indices {
                    if let old = existing.files.first(where: { $0.path == merged.files[k].path }), old.size == merged.files[k].size {
                        merged.files[k].done = old.done
                    }
                }
                jobs[i] = merged
            }
            resume(existing.id)
            return
        }
        var job = new
        let need = job.files.filter { !$0.done }.reduce(Int64(0)) { $0 + max(0, $1.size - Self.onDisk(job.kind, $1.path)) }
        if let free = Self.freeSpace(), need > free {
            problem = L10n.tr("This needs \(Self.bytes(need)) more and the device has \(Self.bytes(free)) free.")
            return
        }
        Self.settle(&job)
        jobs.insert(job, at: 0)
        save()
        HuskLog.log("downloads", "started \(job.title): \(job.files.count) file(s), \(Self.bytes(job.total)), \(job.filesDone) already here")
        keepRunning()
        pump()
        LiveDownload.update(jobs: jobs, speed: speed)
    }

    // MARK: controls

    func pause(_ id: UUID) {
        guard let i = jobs.firstIndex(where: { $0.id == id }) else { return }
        jobs[i].paused = true
        save()
        DownloadKeepAlive.shared.set(jobs.contains { !$0.complete && !$0.paused })
        for (key, task) in directRunning where key.hasPrefix(id.uuidString) { task.cancel() }
        background.getAllTasks { tasks in for t in tasks where TaskTag(t.taskDescription)?.job == id { t.cancel() } }
        handedOff = handedOff.filter { !$0.hasPrefix(id.uuidString) }
        LiveDownload.update(jobs: jobs, speed: speed)
    }

    func resume(_ id: UUID) {
        guard let i = jobs.firstIndex(where: { $0.id == id }) else { return }
        jobs[i].paused = false
        for k in jobs[i].files.indices { jobs[i].files[k].error = nil; retries[key(id, jobs[i].files[k].path)] = nil }
        Self.settle(&jobs[i])
        save()
        keepRunning()
        pump()
        LiveDownload.update(jobs: jobs, speed: speed)
    }

    /// The keep-alive stopped while Husk was in the background (a call took the audio): hand off, as if leaving now.
    fileprivate func keepAliveLost() {
        if UIApplication.shared.applicationState != .active { leaving() }
    }

    func remove(_ id: UUID) {
        for (key, task) in directRunning where key.hasPrefix(id.uuidString) { task.cancel() }
        background.getAllTasks { tasks in for t in tasks where TaskTag(t.taskDescription)?.job == id { t.cancel() } }
        jobs.removeAll { $0.id == id }
        save()
        DownloadKeepAlive.shared.set(jobs.contains { !$0.complete && !$0.paused })
        LiveDownload.update(jobs: jobs, speed: speed)
    }

    func clearFinished() {
        jobs.removeAll { $0.complete }
        save()
    }

    // MARK: moving bytes

    private func key(_ job: UUID, _ path: String) -> String { "\(job.uuidString)|\(path)" }

    /// Start files until `parallel` are running: Husk's own requests while it is awake, nothing new while it is not (what was handed
    /// to the background session carries on there).
    private func pump() {
        DownloadKeepAlive.shared.set(jobs.contains { !$0.complete && !$0.paused })
        guard awake else { return }
        for job in jobs where !job.paused {
            for f in job.files where !f.done && f.error == nil {
                guard directRunning.count < Self.parallel else { return }
                let k = key(job.id, f.path)
                if directRunning[k] != nil || handedOff.contains(k) { continue }
                fetch(job, f)
            }
        }
    }

    private func fetch(_ job: Job, _ f: File) {
        guard let url = URL(string: f.url), let dest = Self.destination(job.kind, f.path) else { return }
        let part = Self.partURL(dest)
        let offset = Self.size(part)
        var req = URLRequest(url: url)
        if offset > 0 { req.setValue("bytes=\(offset)-", forHTTPHeaderField: "Range") }
        let task = direct.dataTask(with: req)
        let tag = TaskTag(job: job.id, path: f.path, kind: job.kind, offset: offset)
        task.taskDescription = tag.encoded
        directDelegate.begin(task: task, tag: tag, part: part, offset: offset)
        directRunning[key(job.id, f.path)] = task
        task.resume()
    }

    /// Husk is leaving the screen. With a continued processing task running it carries on as it is; without one, each file's
    /// remaining bytes go to the background session now, while Husk is still active.
    private func leaving() {
        if #available(iOS 26.0, *), ContinuedDownload.active { return }
        guard jobs.contains(where: { !$0.complete && !$0.paused }) else { return }
        // Kept running by its silent audio: carry on downloading as if on screen.
        if DownloadKeepAlive.shared.running { HuskLog.log("downloads", "leaving the screen; kept running, downloads carry on"); return }
        awake = false
        if grace == .invalid {
            grace = UIApplication.shared.beginBackgroundTask(withName: "Husk downloads") {
                MainActor.assumeIsolated { Downloads.shared.endGrace() }
            }
        }
        // Stop Husk's own requests: what they wrote is on disk, and the background session continues from there.
        for task in directRunning.values { task.cancel() }
        directRunning.removeAll()
        var n = 0
        for job in jobs where !job.paused {
            for f in job.files where !f.done {
                let k = key(job.id, f.path)
                guard !handedOff.contains(k), let url = URL(string: f.url), let dest = Self.destination(job.kind, f.path) else { continue }
                let offset = Self.size(Self.partURL(dest))
                var req = URLRequest(url: url)
                if offset > 0 { req.setValue("bytes=\(offset)-", forHTTPHeaderField: "Range") }
                let task = background.downloadTask(with: req)
                task.taskDescription = TaskTag(job: job.id, path: f.path, kind: job.kind, offset: offset).encoded
                if f.size > 0 { task.countOfBytesClientExpectsToReceive = f.size - offset }
                task.priority = URLSessionTask.highPriority
                task.resume()
                handedOff.insert(k)
                n += 1
            }
        }
        HuskLog.log("downloads", "Husk is leaving the screen: \(n) file(s) handed to iOS's background downloads")
        save()
    }

    /// Back on screen: files the background session has not started are taken back and fetched by Husk again; ones it is part-way
    /// through are left to finish there.
    private func returned() {
        endGrace()
        awake = true
        keepRunning()
        background.getAllTasks { tasks in
            // Ones it has barely started on as well: Husk refetches 64 MB in seconds, where a file left to the background service
            // stays slow until it is done.
            let idle = tasks.filter { $0.countOfBytesReceived < 64 << 20 && $0.state == .running }
            for t in idle { t.cancel() }
            let busy = Set(tasks.filter { $0.countOfBytesReceived >= 64 << 20 }.compactMap { TaskTag($0.taskDescription) }.map { "\($0.job.uuidString)|\($0.path)" })
            Task { @MainActor in
                self.handedOff = busy
                if !idle.isEmpty || !busy.isEmpty { HuskLog.log("downloads", "back on screen: \(idle.count) file(s) taken back, \(busy.count) finishing in the background") }
                self.pump()
            }
        }
    }

    private func endGrace() {
        if grace != .invalid { UIApplication.shared.endBackgroundTask(grace); grace = .invalid }
    }

    /// After a relaunch: background tasks still running are noted, so they are not fetched twice.
    private func adoptBackgroundTasks() {
        background.getAllTasks { tasks in
            // Tasks this build cannot place (no tag it understands) would download for nothing: stop them.
            for t in tasks where TaskTag(t.taskDescription) == nil { t.cancel() }
            // Tasks from a build that wrote the whole file to iOS's own temporary file and moved it into place (offset unknown, no
            // .part) still finish correctly: their response starts at byte 0.
            let running = Set(tasks.compactMap { TaskTag($0.taskDescription) }.map { "\($0.job.uuidString)|\($0.path)" })
            Task { @MainActor in
                self.handedOff = running
                if !running.isEmpty { HuskLog.log("downloads", "\(running.count) file(s) still downloading in the background") }
                self.pump()
                LiveDownload.update(jobs: self.jobs, speed: self.speed)
            }
        }
    }

    // MARK: progress (main actor)

    private var lastSample = (time: Date(), bytes: Int64(0))
    private var lastLive = Date.distantPast
    private var lastSave = Date.distantPast

    fileprivate func progressed(_ tag: TaskTag, received: Int64, expected: Int64) {
        guard let j = jobs.firstIndex(where: { $0.id == tag.job }), let k = jobs[j].files.firstIndex(where: { $0.path == tag.path }) else { return }
        jobs[j].files[k].received = received
        if expected > 0 { jobs[j].files[k].size = expected }
        let now = Date(), all = jobs.reduce(0) { $0 + $1.received }
        let dt = now.timeIntervalSince(lastSample.time)
        if dt >= 1 {
            let rate = Double(all - lastSample.bytes) / dt
            if rate >= 0 { speed = speed == 0 ? rate : speed * 0.6 + rate * 0.4 }
            lastSample = (now, all)
        }
        if now.timeIntervalSince(lastLive) > 2 { lastLive = now; LiveDownload.update(jobs: jobs, speed: speed) }
        if now.timeIntervalSince(lastSave) > 10 { lastSave = now; save() }
    }

    /// One file's transfer ended: complete, or stopped part-way (its bytes stay in the .part for next time).
    fileprivate func ended(_ tag: TaskTag, error: String?, cancelled: Bool = false, background: Bool = false) {
        let k = key(tag.job, tag.path)
        if background { handedOff.remove(k) } else { directRunning[k] = nil }
        guard let j = jobs.firstIndex(where: { $0.id == tag.job }), let i = jobs[j].files.firstIndex(where: { $0.path == tag.path }) else { pump(); return }
        let f = jobs[j].files[i]
        if f.done { pump(); return }      // a second transfer of a file that is already in place
        guard let dest = Self.destination(tag.kind, f.path) else { return }
        let part = Self.partURL(dest), have = Self.size(part)
        jobs[j].files[i].received = have
        if f.size > 0, have > f.size {
            // Longer than the file: something wrote twice. Start that file again rather than keep it.
            HuskLog.log("downloads", "\(f.path): \(Self.bytes(have)) on disk for a \(Self.bytes(f.size)) file; fetching it again")
            try? FileManager.default.removeItem(at: part)
            jobs[j].files[i].received = 0
            save()
            pump()
            return
        }
        if error == nil, f.size == 0 || have == f.size {
            // All of it: into place.
            let fm = FileManager.default
            try? fm.removeItem(at: dest)
            do {
                try fm.moveItem(at: part, to: dest)
                jobs[j].files[i].done = true
                jobs[j].files[i].error = nil
                jobs[j].files[i].received = Self.size(dest)
                if f.size == 0 { jobs[j].files[i].size = jobs[j].files[i].received }
                retries[k] = nil
                HuskLog.log("downloads", "\(f.path) done (\(jobs[j].filesDone)/\(jobs[j].files.count))")
                if tag.kind == .apk { TranslationLayerStore.shared.add([dest], move: true) }
            } catch {
                jobs[j].files[i].error = error.localizedDescription
            }
        } else if !cancelled, !jobs[j].paused {
            // Stopped early (a dropped connection, the server went away, the background service gave up): again from where it is,
            // after a pause that grows, a few times; then it waits for Retry.
            let n = retries[k, default: 0] + 1
            retries[k] = n
            if n <= 5 {
                let wait: UInt64 = [2, 5, 15, 30, 60][n - 1]
                HuskLog.log("downloads", "\(f.path): \(error ?? "incomplete") at \(Self.bytes(have)); again in \(wait) s")
                Task { @MainActor in
                    try? await Task.sleep(nanoseconds: wait * 1_000_000_000)
                    self.pump()
                }
            } else {
                jobs[j].files[i].error = error ?? L10n.tr("the transfer kept stopping")
                HuskLog.log("downloads", "\(f.path): giving up after \(n - 1) tries")
            }
        }
        if jobs[j].complete, jobs[j].finished == nil {
            jobs[j].finished = Date()
            HuskLog.log("downloads", "\(jobs[j].title) finished")
        }
        save()
        pump()
        LiveDownload.update(jobs: jobs, speed: speed)
    }

    func sessionEventsDone() {
        save()
        LiveDownload.update(jobs: jobs, speed: speed)
        backgroundCompletion?()
        backgroundCompletion = nil
    }

    // MARK: on disk

    /// Files already in place at their full size are done; partly fetched ones count what their .part holds.
    private static func settle(_ job: inout Job) {
        for i in job.files.indices where !job.files[i].done {
            guard let dest = destination(job.kind, job.files[i].path) else { continue }
            let full = size(dest)
            if full > 0, job.files[i].size > 0, full == job.files[i].size, !FileManager.default.fileExists(atPath: partURL(dest).path) {
                job.files[i].done = true
                job.files[i].received = full
            } else {
                job.files[i].received = size(partURL(dest))
            }
        }
    }

    private func settleFromDisk() {
        for j in jobs.indices { Self.settle(&jobs[j]) }
        save()
    }

    nonisolated static func size(_ url: URL) -> Int64 {
        ((try? FileManager.default.attributesOfItem(atPath: url.path))?[.size] as? NSNumber)?.int64Value ?? 0
    }
    nonisolated static func partURL(_ dest: URL) -> URL { dest.deletingLastPathComponent().appendingPathComponent(dest.lastPathComponent + ".part") }
    private static func onDisk(_ kind: Job.Kind, _ path: String) -> Int64 {
        guard let dest = destination(kind, path) else { return 0 }
        return max(size(dest), size(partURL(dest)))
    }

    nonisolated static var stateDir: URL {
        let dir = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("Downloads", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir
    }
    private static var jobsFile: URL { stateDir.appendingPathComponent("jobs.json") }
    private static func loadJobs() -> [Job] { (try? JSONDecoder().decode([Job].self, from: Data(contentsOf: jobsFile))) ?? [] }
    private func save() { try? JSONEncoder().encode(jobs).write(to: Self.jobsFile, options: .atomic) }

    /// Where a finished file goes. A folder's files keep their paths under Shared Storage; a single file goes to its top; an APK waits
    /// in Husk's own space until the library takes it.
    nonisolated static func destination(_ kind: Job.Kind, _ path: String) -> URL? {
        let parts = path.split(separator: "/").map(String.init)
        guard !parts.isEmpty, !parts.contains(".."), !parts.contains(".") else { return nil }
        if kind == .apk { return stateDir.appendingPathComponent("incoming", isDirectory: true).appendingPathComponent(parts.last!) }
        return parts.reduce(TranslationLayer.sharedStorage) { $0.appendingPathComponent($1) }
    }

    // MARK: probing a link

    private struct Manifest: Decodable {
        struct Entry: Decodable { let path: String; let size: Int64 }
        let name: String
        let files: [Entry]
        enum CodingKeys: String, CodingKey { case name, files }
        init(from d: Decoder) throws {
            let c = try d.container(keyedBy: CodingKeys.self)
            name = (try? c.decode(String.self, forKey: .name)) ?? ""
            files = try c.decode([Entry].self, forKey: .files)
        }
    }

    private static func fetchManifest(_ url: URL) async -> Manifest? {
        var req = URLRequest(url: url)
        req.httpMethod = "HEAD"
        req.timeoutInterval = 15
        guard let (_, head) = try? await URLSession.shared.data(for: req), let http = head as? HTTPURLResponse,
              (http.value(forHTTPHeaderField: "Content-Type") ?? "").contains("json"), http.expectedContentLength < 64 << 20 else { return nil }
        req.httpMethod = "GET"
        guard let (data, _) = try? await URLSession.shared.data(for: req) else { return nil }
        return try? JSONDecoder().decode(Manifest.self, from: data)
    }

    private static func probe(_ url: URL) async -> (String, Int64) {
        var req = URLRequest(url: url)
        req.httpMethod = "HEAD"
        req.timeoutInterval = 15
        let fallback = url.lastPathComponent.isEmpty ? (url.host ?? "download") : url.lastPathComponent
        guard let (_, r) = try? await URLSession.shared.data(for: req) else { return (fallback, 0) }
        let name = r.suggestedFilename.flatMap { $0.isEmpty || $0 == "Unknown" ? nil : $0 } ?? fallback
        return (name, max(0, r.expectedContentLength))
    }

    static func freeSpace() -> Int64? {
        let values = try? FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
            .resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey])
        return values?.volumeAvailableCapacityForImportantUsage
    }

    static func bytes(_ n: Int64) -> String { ByteCountFormatter.string(fromByteCount: n, countStyle: .file) }

    // MARK: iOS 26

    /// Ask to keep running after Husk leaves the screen (a continued processing task). The system shows its progress in the Dynamic
    /// Island and on the Lock Screen, and Husk keeps downloading itself. Only from something the person did -- adding or resuming a
    /// download -- which is what the system allows.
    private func keepRunning() {
        guard let active = jobs.first(where: { !$0.complete && !$0.paused }) else { return }
        if #available(iOS 26.0, *) { ContinuedDownload.begin(title: active.title) }
    }

    /// The continued processing task ended (finished, or stopped by the system): if Husk is not on screen, hand off now.
    fileprivate func continuedEnded() {
        if UIApplication.shared.applicationState != .active { leaving() }
    }
}

/// Which job and file a task is for, kept in its taskDescription so it survives a relaunch, with the byte the request started at.
struct TaskTag: Codable {
    let job: UUID
    let path: String
    let kind: Downloads.Job.Kind
    var offset: Int64 = 0

    init(job: UUID, path: String, kind: Downloads.Job.Kind, offset: Int64 = 0) { self.job = job; self.path = path; self.kind = kind; self.offset = offset }
    enum CodingKeys: String, CodingKey { case job, path, kind, offset }
    init(from d: Decoder) throws {
        let c = try d.container(keyedBy: CodingKeys.self)
        job = try c.decode(UUID.self, forKey: .job)
        path = try c.decode(String.self, forKey: .path)
        kind = try c.decode(Downloads.Job.Kind.self, forKey: .kind)
        offset = (try? c.decodeIfPresent(Int64.self, forKey: .offset)) ?? 0     // tasks from before offsets were kept: from the start
    }
    init?(_ s: String?) {
        guard let s, let d = s.data(using: .utf8), let t = try? JSONDecoder().decode(TaskTag.self, from: d) else { return nil }
        self = t
    }
    var encoded: String { (try? JSONEncoder().encode(self)).flatMap { String(data: $0, encoding: .utf8) } ?? "" }
}

/// Husk's own transfers: each response is written straight into its file's .part as it arrives. Runs on one serial queue.
final class DirectDownloadDelegate: NSObject, URLSessionDataDelegate {
    private final class Transfer {
        let tag: TaskTag
        let part: URL
        var handle: FileHandle?
        var written: Int64
        var expected: Int64 = 0
        var lastReport = Date.distantPast
        var error: String?
        init(tag: TaskTag, part: URL, offset: Int64) { self.tag = tag; self.part = part; written = offset }
    }
    private var transfers: [Int: Transfer] = [:]
    private let lock = NSLock()

    func begin(task: URLSessionDataTask, tag: TaskTag, part: URL, offset: Int64) {
        lock.lock(); transfers[task.taskIdentifier] = Transfer(tag: tag, part: part, offset: offset); lock.unlock()
    }
    private func transfer(_ task: URLSessionTask) -> Transfer? { lock.lock(); defer { lock.unlock() }; return transfers[task.taskIdentifier] }

    func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive response: URLResponse, completionHandler: @escaping (URLSession.ResponseDisposition) -> Void) {
        guard let t = transfer(dataTask), let http = response as? HTTPURLResponse else { completionHandler(.cancel); return }
        let fm = FileManager.default
        try? fm.createDirectory(at: t.part.deletingLastPathComponent(), withIntermediateDirectories: true)
        if !fm.fileExists(atPath: t.part.path) { fm.createFile(atPath: t.part.path, contents: nil) }
        guard let h = try? FileHandle(forWritingTo: t.part) else { t.error = L10n.tr("cannot write the file"); completionHandler(.cancel); return }
        switch http.statusCode {
        case 206:
            // The rest, from where the .part ends.
            try? h.truncate(atOffset: UInt64(t.tag.offset))
            t.written = t.tag.offset
            t.expected = t.tag.offset + max(0, http.expectedContentLength)
        case 200:
            // The whole file (no Range asked, or the server ignores it): from the start.
            try? h.truncate(atOffset: 0)
            t.written = 0
            t.expected = max(0, http.expectedContentLength)
        case 416:
            // Nothing left to send: the .part is already whole.
            t.expected = t.written
            try? h.close()
            completionHandler(.cancel)
            return
        default:
            t.error = L10n.tr("the server answered \(http.statusCode)")
            try? h.close()
            completionHandler(.cancel)
            return
        }
        try? h.seekToEnd()
        t.handle = h
        completionHandler(.allow)
    }

    func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive data: Data) {
        guard let t = transfer(dataTask), let h = t.handle else { return }
        do { try h.write(contentsOf: data) } catch { t.error = L10n.tr("cannot write: \(error.localizedDescription)"); dataTask.cancel(); return }
        t.written += Int64(data.count)
        let now = Date()
        if now.timeIntervalSince(t.lastReport) > 0.5 {
            t.lastReport = now
            let tag = t.tag, written = t.written, expected = t.expected
            Task { @MainActor in Downloads.shared.progressed(tag, received: written, expected: expected) }
        }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        lock.lock(); let t = transfers.removeValue(forKey: task.taskIdentifier); lock.unlock()
        guard let t else { return }
        try? t.handle?.synchronize()
        try? t.handle?.close()
        let ns = error as NSError?
        let cancelled = ns?.code == NSURLErrorCancelled && t.error == nil
        let message = t.error ?? (cancelled ? nil : error?.localizedDescription)
        let incomplete = t.expected > 0 && t.written < t.expected
        let tag = t.tag
        Task { @MainActor in Downloads.shared.ended(tag, error: message ?? (incomplete && !cancelled ? L10n.tr("the connection ended early") : nil), cancelled: cancelled) }
    }
}

/// The background session's delegate. A finished task's file has to be dealt with before this callback returns (iOS deletes it
/// after): it is the rest of the file from the tag's offset, so it is appended to the .part (or becomes it, from the start).
final class BackgroundDownloadDelegate: NSObject, URLSessionDownloadDelegate {
    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didWriteData _: Int64, totalBytesWritten: Int64, totalBytesExpectedToWrite: Int64) {
        guard let tag = TaskTag(downloadTask.taskDescription) else { return }
        let received = tag.offset + totalBytesWritten, expected = totalBytesExpectedToWrite > 0 ? tag.offset + totalBytesExpectedToWrite : 0
        Task { @MainActor in Downloads.shared.progressed(tag, received: received, expected: expected) }
    }

    func urlSession(_ session: URLSession, downloadTask: URLSessionDownloadTask, didFinishDownloadingTo location: URL) {
        guard let tag = TaskTag(downloadTask.taskDescription), let dest = Downloads.destination(tag.kind, tag.path) else { return }
        let part = Downloads.partURL(dest)
        let status = (downloadTask.response as? HTTPURLResponse)?.statusCode ?? 0
        let fm = FileManager.default
        do {
            try fm.createDirectory(at: part.deletingLastPathComponent(), withIntermediateDirectories: true)
            if status == 416 {
                // Nothing was left to send: the .part is already whole.
            } else if status == 206, tag.offset > 0, Downloads.size(part) >= tag.offset {
                // Append the rest to what was there. The .part can be a little longer than when the request was made (Husk's own
                // transfer was still writing as it was stopped); the response is exactly the bytes from the offset, so cut it back first.
                guard let out = try? FileHandle(forWritingTo: part), let inp = try? FileHandle(forReadingFrom: location) else { throw CocoaError(.fileWriteUnknown) }
                try out.truncate(atOffset: UInt64(tag.offset))
                try out.seekToEnd()
                while let chunk = try inp.read(upToCount: 8 << 20), !chunk.isEmpty { try out.write(contentsOf: chunk) }
                try out.close(); try inp.close()
            } else if status == 200 || (status == 206 && tag.offset == 0) {
                try? fm.removeItem(at: part)
                try fm.moveItem(at: location, to: part)
            } else {
                throw NSError(domain: "HuskDownloads", code: status, userInfo: [NSLocalizedDescriptionKey: L10n.tr("the server answered \(status)")])
            }
        } catch {
            let message = error.localizedDescription
            Task { @MainActor in Downloads.shared.ended(tag, error: message, background: true) }
            return
        }
        Task { @MainActor in Downloads.shared.ended(tag, error: nil, background: true) }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        guard let error, let tag = TaskTag(task.taskDescription) else { return }
        let ns = error as NSError
        let cancelled = ns.code == NSURLErrorCancelled
        // The bytes it had are lost with it (they are in iOS's own file, not the .part); the next try starts where the .part ends.
        let message = error.localizedDescription
        Task { @MainActor in Downloads.shared.ended(tag, error: message, cancelled: cancelled, background: true) }
    }

    func urlSessionDidFinishEvents(forBackgroundURLSession session: URLSession) {
        Task { @MainActor in Downloads.shared.sessionEventsDone() }
    }
}

/// The Live Activity: one for everything downloading, ended when nothing is.
@MainActor
enum LiveDownload {
    static func update(jobs: [Downloads.Job], speed: Double) {
        if #available(iOS 26.0, *), ContinuedDownload.update(jobs: jobs, speed: speed) {
            // The system's own progress is showing; Husk's Live Activity would only repeat it.
            #if canImport(ActivityKit)
            if #available(iOS 16.1, *) { Live.end() }
            #endif
            return
        }
        #if canImport(ActivityKit)
        if #available(iOS 16.1, *) { Live.update(jobs: jobs, speed: speed) }
        #endif
    }
}

#if canImport(ActivityKit)
@available(iOS 16.1, *)
@MainActor
private enum Live {
    static var activity: Activity<HuskDownloadAttributes>?
    static var started = Date()
    static var logged = false

    static func end() {
        if let a = activity ?? Activity<HuskDownloadAttributes>.activities.first {
            Task { await a.end(dismissalPolicy: .immediate) }
            activity = nil
        }
    }

    static func update(jobs: [Downloads.Job], speed: Double) {
        let active = jobs.filter { !$0.complete && !$0.paused }
        if activity == nil { activity = Activity<HuskDownloadAttributes>.activities.first }
        guard !active.isEmpty else {
            if let a = activity {
                let last = jobs.first
                let state = HuskDownloadAttributes.ContentState(title: last?.title ?? L10n.tr("Downloads"), received: last?.received ?? 0, total: last?.total ?? 0,
                                                                finishBy: nil, started: started, finished: !(last?.failed ?? false), failed: last?.failed ?? false)
                Task { await a.end(using: state, dismissalPolicy: .after(Date().addingTimeInterval(60 * 10))) }
                activity = nil
            }
            return
        }
        let received = active.reduce(0) { $0 + $1.received }, total = active.reduce(0) { $0 + $1.total }
        let finishBy = speed > 1 && total > received ? Date().addingTimeInterval(Double(total - received) / speed) : nil
        let title = active.count == 1 ? active[0].title : L10n.tr("\(active.count) downloads")
        var state = HuskDownloadAttributes.ContentState(title: title, received: received, total: total, finishBy: finishBy, started: started,
                                                        finished: false, failed: active.contains { $0.failed })
        state.speed = speed
        state.filesDone = active.reduce(0) { $0 + $1.filesDone }
        state.filesTotal = active.reduce(0) { $0 + $1.files.count }
        if let a = activity, a.activityState == .active {
            Task { await a.update(using: state) }
        } else {
            activity = nil
            started = Date()
            state.started = started
            let auth = ActivityAuthorizationInfo().areActivitiesEnabled
            let widget = Bundle.main.builtInPlugInsURL.map { FileManager.default.fileExists(atPath: $0.appendingPathComponent("HuskDownloadsWidget.appex").path) } ?? false
            if !logged { logged = true; HuskLog.log("downloads", "Live Activity: allowed \(auth), widget extension installed \(widget)") }
            Downloads.shared.liveActivitiesOff = !auth
            guard auth else { return }
            do {
                let a = try Activity.request(attributes: HuskDownloadAttributes(), contentState: state, pushType: nil)
                activity = a
                HuskLog.log("downloads", "Live Activity started (\(a.id))")
            } catch {
                HuskLog.log("downloads", "no Live Activity: \(error.localizedDescription)")
            }
        }
    }
}
#endif

/// The download as a continued processing task (iOS 26): the system keeps Husk running in the background for it and shows its
/// progress in the Dynamic Island and on the Lock Screen.
@available(iOS 26.0, *)
@MainActor
enum ContinuedDownload {
    private static var task: BGContinuedProcessingTask?
    private static var pending: String?
    private static var registered: String?
    private static var lastSubtitle = Date.distantPast

    static var running: Bool { task != nil || pending != nil }
    /// The system started it: Husk is being kept running.
    static var active: Bool { task != nil }

    private static var refused = false

    static func begin(title: String) {
        guard !running, !refused, let bundle = Bundle.main.bundleIdentifier else { return }
        // The launch handler is registered for a wildcard pattern from Info.plist, and the submitted identifier has to start with the
        // app's bundle ID. A sideloader renames the bundle (com.husk.app.<team>) but not Info.plist: then the broad com.husk.app.* is
        // the pattern, and the identifier is made from the bundle ID as it is now.
        let permitted = Bundle.main.object(forInfoDictionaryKey: "BGTaskSchedulerPermittedIdentifiers") as? [String] ?? []
        guard let downloads = permitted.first(where: { $0.hasSuffix(".downloads.*") }) else { return }
        let built = String(downloads.dropLast(".downloads.*".count))      // the bundle ID Husk was built with
        let pattern: String? = bundle == built ? downloads : (bundle.hasPrefix(built + ".") ? permitted.first { $0 == built + ".*" } : nil)
        guard let pattern else { refused = true; HuskLog.log("downloads", "background task: no permitted pattern"); return }
        if registered != pattern {
            let ok = BGTaskScheduler.shared.register(forTaskWithIdentifier: pattern, using: .main) { t in
                MainActor.assumeIsolated { started(t) }
            }
            guard ok else { refused = true; HuskLog.log("downloads", "background task: \(pattern) not permitted; Husk keeps itself running instead"); return }
            registered = pattern
        }
        let id = "\(bundle).downloads.\(UUID().uuidString.prefix(8))"
        let request = BGContinuedProcessingTaskRequest(identifier: id, title: L10n.tr("Downloading \(title)"), subtitle: L10n.tr("Starting…"))
        request.strategy = .fail
        do {
            try BGTaskScheduler.shared.submit(request)
            pending = id
            HuskLog.log("downloads", "background task submitted (\(id), handler \(pattern))")
        } catch {
            HuskLog.log("downloads", "background task \(id) refused: \(error.localizedDescription); using Husk's Live Activity instead")
        }
    }

    private static func started(_ t: BGTask) {
        guard let t = t as? BGContinuedProcessingTask else { t.setTaskCompleted(success: false); return }
        task = t
        pending = nil
        HuskLog.log("downloads", "background task running")
        t.expirationHandler = {
            MainActor.assumeIsolated {
                HuskLog.log("downloads", "background task ended by the system")
                task = nil
                Downloads.shared.continuedEnded()
            }
        }
        _ = update(jobs: Downloads.shared.jobs, speed: Downloads.shared.speed)
    }

    /// Progress into the task. True while the system's progress is the one showing.
    @discardableResult
    static func update(jobs: [Downloads.Job], speed: Double) -> Bool {
        guard let t = task else { return false }
        let active = jobs.filter { !$0.complete && !$0.paused }
        if active.isEmpty {
            let failed = jobs.contains { $0.failed }
            t.progress.completedUnitCount = t.progress.totalUnitCount
            t.updateTitle(failed ? L10n.tr("Download stopped") : L10n.tr("Download finished"), subtitle: failed ? L10n.tr("Open Husk to retry") : (jobs.first?.title ?? ""))
            t.setTaskCompleted(success: !failed)
            task = nil
            HuskLog.log("downloads", "background task completed")
            return false
        }
        let received = active.reduce(0) { $0 + $1.received }, total = active.reduce(0) { $0 + $1.total }
        // In megabytes: Progress counts in Int64 and the system shows a fraction, so the unit only has to be fine enough.
        t.progress.totalUnitCount = max(1, total / 1_000_000)
        t.progress.completedUnitCount = min(t.progress.totalUnitCount, received / 1_000_000)
        if Date().timeIntervalSince(lastSubtitle) > 2 {
            lastSubtitle = Date()
            var sub = L10n.tr("\(Downloads.bytes(received)) of \(Downloads.bytes(total))")
            if speed > 1 { sub += " · \(Downloads.bytes(Int64(speed)))/s" }
            let files = active.reduce(0) { $0 + $1.files.count }
            if files > 1 { sub += L10n.tr(" · \(active.reduce(0) { $0 + $1.filesDone })/\(files) files") }
            if speed > 1, total > received {
                let left = Double(total - received) / speed
                let f = DateComponentsFormatter(); f.allowedUnits = left > 3600 ? [.hour, .minute] : [.minute, .second]; f.unitsStyle = .abbreviated
                if let tl = f.string(from: left) { sub += L10n.tr(" · \(tl) left") }
            }
            t.updateTitle(active.count == 1 ? L10n.tr("Downloading \(active[0].title)") : L10n.tr("Downloading \(active.count) items"), subtitle: sub)
        }
        return true
    }
}

/// Keeps Husk running in the background while downloads are active, by playing silence: iOS lets an app that is playing audio keep
/// running, so Husk goes on downloading at full speed and its Live Activity stays current. It mixes with other audio (music keeps
/// playing) and stops as soon as nothing is downloading. The way an App Store app would do this is a continued processing task, which
/// iOS refuses a sideloaded copy (its bundle ID no longer matches the identifiers in Info.plist).
@MainActor
final class DownloadKeepAlive {
    static let shared = DownloadKeepAlive()
    private var engine: AVAudioEngine?
    private var player: AVAudioPlayerNode?
    private(set) var running = false

    private init() {
        NotificationCenter.default.addObserver(forName: AVAudioSession.interruptionNotification, object: nil, queue: .main) { note in
            let type = (note.userInfo?[AVAudioSessionInterruptionTypeKey] as? UInt).flatMap(AVAudioSession.InterruptionType.init)
            MainActor.assumeIsolated {
                let k = DownloadKeepAlive.shared
                guard k.running || k.wanted else { return }
                if type == .began {
                    HuskLog.log("downloads", "keep-alive interrupted (a call, or another app took the audio)")
                    k.stopEngine()
                    Downloads.shared.keepAliveLost()
                } else if type == .ended {
                    k.startEngine()
                }
            }
        }
    }

    private var wanted = false

    func set(_ on: Bool) {
        guard on != wanted else { return }
        wanted = on
        if on { startEngine() } else { stopEngine() }
    }

    private func startEngine() {
        guard wanted, !running else { return }
        // A game's screen sets up its own audio and is in front anyway; nothing to do while one is playing.
        do {
            let session = AVAudioSession.sharedInstance()
            try session.setCategory(.playback, mode: .default, options: [.mixWithOthers])
            try session.setActive(true)
            let engine = AVAudioEngine(), player = AVAudioPlayerNode()
            engine.attach(player)
            let format = AVAudioFormat(standardFormatWithSampleRate: 44100, channels: 1)!
            engine.connect(player, to: engine.mainMixerNode, format: format)
            guard let buffer = AVAudioPCMBuffer(pcmFormat: format, frameCapacity: 44100) else { return }
            buffer.frameLength = 44100                  // one second of zeros, looped
            try engine.start()
            player.scheduleBuffer(buffer, at: nil, options: .loops)
            player.play()
            self.engine = engine
            self.player = player
            running = true
            HuskLog.log("downloads", "keep-alive on: Husk keeps running in the background while downloading")
        } catch {
            HuskLog.log("downloads", "keep-alive failed: \(error.localizedDescription)")
        }
    }

    private func stopEngine() {
        guard running else { return }
        player?.stop()
        engine?.stop()
        player = nil
        engine = nil
        running = false
        HuskLog.log("downloads", "keep-alive off")
    }
}
