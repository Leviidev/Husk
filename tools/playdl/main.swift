// SPDX-License-Identifier: GPL-2.0-or-later
// playdl: Husk's own Play Store client (src/app/Husk/Play) on the Mac, for compatibility testing.
//   playdl get <out-dir> <package>...   base APK + splits into <out-dir>/<package>/
//   playdl search <query>                package names and titles
//   playdl browse                        the store's front page
// An anonymous session from Aurora's token dispenser, cached in ~/.husk-playdl.json.
import Foundation

func session() async throws -> PlaySession {
    let cache = URL(fileURLWithPath: NSHomeDirectory()).appendingPathComponent(".husk-playdl.json")
    if let d = try? Data(contentsOf: cache), let s = try? JSONDecoder().decode(PlaySession.self, from: d),
       let a = try? FileManager.default.attributesOfItem(atPath: cache.path)[.modificationDate] as? Date, Date().timeIntervalSince(a) < 3600 {
        return s
    }
    let s = try await PlayAPI.fetchAuroraGuestSession()
    try? JSONEncoder().encode(s).write(to: cache)
    return s
}

func run() async -> Int32 {
    let args = Array(CommandLine.arguments.dropFirst())
    guard let cmd = args.first else { print("usage: playdl get <dir> <pkg>... | search <q> | browse"); return 2 }
    do {
        let s = try await session()
        switch cmd {
        case "search":
            for a in try await PlayAPI.search(query: args.dropFirst().joined(separator: " "), session: s) { print("\(a.id)\t\(a.title)") }
        case "browse":
            for a in try await PlayAPI.browse(session: s) { print("\(a.id)\t\(a.title)") }
        case "get":
            guard args.count >= 3 else { print("usage: playdl get <dir> <pkg>..."); return 2 }
            let out = URL(fileURLWithPath: args[1])
            var rc: Int32 = 0
            for pkg in args.dropFirst(2) {
                do {
                    guard let app = try await PlayAPI.details(packageName: pkg, session: s) else { print("\(pkg)\tFAIL no details"); rc = 1; continue }
                    let files = try await PlayAPI.downloadApp(packageName: pkg, versionCode: app.versionCode, session: s)
                    let dir = out.appendingPathComponent(pkg)
                    try? FileManager.default.removeItem(at: dir)
                    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
                    for f in files { try FileManager.default.moveItem(at: f, to: dir.appendingPathComponent(f.lastPathComponent)) }
                    print("\(pkg)\tOK vc \(app.versionCode)\t\(files.map { $0.lastPathComponent }.joined(separator: " "))")
                } catch { print("\(pkg)\tFAIL \(error.localizedDescription)"); rc = 1 }
            }
            return rc
        default: print("unknown command \(cmd)"); return 2
        }
    } catch { print("error: \(error.localizedDescription)"); return 1 }
    return 0
}

let sem = DispatchSemaphore(value: 0)
var code: Int32 = 0
Task { code = await run(); sem.signal() }
sem.wait()
exit(code)
