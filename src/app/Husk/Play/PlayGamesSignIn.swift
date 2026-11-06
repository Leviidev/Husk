// SPDX-License-Identifier: GPL-2.0-or-later
import Compression
import CryptoKit
import Foundation

/// Google Play Games for the games Husk runs, signed in with the person's own Google account.
///
/// On a phone, Play services signs a game in. It asks Google, with the account on the phone, for a token in the game's name: the
/// game's package and the digest of the certificate its APK is signed with, which is how Google knows which game's console
/// project the token is for. With that token it reads the player that account is in that game from the Play Games API
/// (games/v1/players/me), and a game that keeps its saves on its own server under that player id (Idling to Rule the Gods) finds
/// them. Husk does the same with the account signed in to its Store (PlayStoreManager): the long-lived account token from that
/// sign-in, the game's own package and certificate, and the same two requests. Nothing is invented: no account, no player id.
/// With no Google account in Husk (or Aurora's anonymous one), the game is told the player is signed out, as before.
///
/// The translation layer asks through `husk_pgs_set_host` (husk-tl-playgames.h), from a game's thread; the answer is ready by
/// then most times, because the sign-in starts when the game's screen opens (`gameStarting`).
enum PlayGamesSignIn {
    /// A Play Games player, as the Play Games API answers for the signed-in account in one game.
    struct Player: Codable, Equatable {
        var id: String
        var name: String
        var avatar: String
        var title: String
    }

    enum Failure: LocalizedError {
        case noAccount, anonymousAccount, noCertificate(String), google(String), noProfile, network(String)
        var errorDescription: String? {
            switch self {
            case .noAccount: return "No Google account is signed in to Husk. Sign in with Google in the Store tab (or on the game's page) for Play Games and the game's online saves."
            case .anonymousAccount: return "The Store is using Aurora's anonymous account, which has no Play Games player. Sign in with your own Google account."
            case .noCertificate(let why): return "Husk could not read the game's signing certificate (\(why)), which Google needs to know the game."
            case .google(let why): return "Google did not sign the game in: \(why)"
            case .noProfile: return "This Google account has no Play Games profile yet. Open Google Play Games once on any Android device or at play.google.com/games, then try again."
            case .network(let why): return "Google could not be reached (\(why))."
            }
        }
    }

    // MARK: the translation layer's side

    /// Once, at launch: the runtime asks here from then on.
    static func install() {
        husk_pgs_set_host { op, package, apk, arg, arg2, flag in
            guard let op = op.map({ String(cString: $0) }) else { return nil }
            let game = PlayGamesSignIn.Game(package: package.map { String(cString: $0) }, apk: apk.map { String(cString: $0) })
            let answer: String
            switch op {
            case "signin":
                switch PlayGamesSignIn.player(game, interactive: flag != 0) {
                case .success(let p): answer = [p.id, p.name, p.avatar, p.avatar, p.title].map { $0.replacingOccurrences(of: "\n", with: " ") }.joined(separator: "\n")
                case .failure(let e): answer = "!" + (e.errorDescription ?? "signed out")
                }
            case "servercode":
                let client = arg.map { String(cString: $0) } ?? ""
                let scopes = arg2.map { String(cString: $0) } ?? ""
                switch PlayGamesSignIn.serverAuthCode(game, serverClientID: client, extraScopes: scopes) {
                case .success(let code): answer = code
                case .failure(let e): answer = "!" + (e.errorDescription ?? "no code")
                }
            default:
                return nil
            }
            return strdup(answer)
        }
    }

    /// Which game: what the runtime says (its package and base APK), else the game whose screen opened last.
    struct Game { var package: String?; var apk: String? }

    /// The game whose screen is opening: its package and APK, and the sign-in started now so it is ready when the game asks.
    static func gameStarting(_ app: TLApp) {
        guard let pkg = app.packageName, let apk = app.apks.first else { return }
        state.lock.lock()
        state.current = (pkg, apk)
        state.lock.unlock()
        guard let s = account(), !s.isAnonymous, !s.aasToken.isEmpty else { return }
        DispatchQueue.global(qos: .userInitiated).async { _ = player(Game(package: pkg, apk: apk), interactive: false) }
    }

    /// Whether the Store is signed in with a person's own Google account (not Aurora's anonymous one).
    static var hasGoogleAccount: Bool {
        guard let s = account() else { return false }
        return !s.isAnonymous && !s.aasToken.isEmpty
    }

    /// The player this account last signed in as in this game, on this iPhone (kept for offline starts and for the game's page).
    static func lastPlayer(package: String?) -> Player? {
        guard let s = account(), let package,
              let data = UserDefaults.standard.data(forKey: cacheKey(s.email, package)) else { return nil }
        return try? JSONDecoder().decode(Player.self, from: data)
    }

    /// Why the last sign-in for this game failed, if it did (cleared by one that works).
    static func lastFailure(package: String?) -> String? {
        package.flatMap { UserDefaults.standard.string(forKey: "husk.pgs.failure.\($0)") }
    }

    /// What to send when the game's online save does not find its player: the account, the game and the certificate digest
    /// Google was asked with, and the last answer. No tokens.
    static func details(_ app: TLApp) -> String {
        let s = account()
        let digest: String
        switch certificateDigest(apk: app.apks.first) { case .success(let d): digest = d; case .failure(let e): digest = e.errorDescription ?? "?" }
        let player = lastPlayer(package: app.packageName)
        let info = Bundle.main.infoDictionary
        return [
            "Husk \(info?["CFBundleShortVersionString"] as? String ?? "?") (\(info?["CFBundleVersion"] as? String ?? "?"), \(info?["HuskBuildCommit"] as? String ?? "?"))",
            "Game: \(app.label) \(app.packageName ?? "?")",
            "APK certificate SHA-1: \(digest)",
            "Google account: \(s.map { $0.isAnonymous ? "Aurora guest (cannot sign in to Play Games)" : $0.email } ?? "none")",
            "Play Games player: \(player.map { "\($0.name) (\($0.id))" } ?? "none yet")",
            "Last sign-in error: \(lastFailure(package: app.packageName) ?? "none")",
        ].joined(separator: "\n")
    }

    /// Games known to keep their progress on their own server under the Play Games player: playing them signed out starts over.
    static let onlineSaveGames: Set<String> = ["de.shugasu.itrtg"]
    static func savesOnline(_ app: TLApp) -> Bool { app.packageName.map(onlineSaveGames.contains) ?? false }

    // MARK: answers

    private final class State {
        let lock = NSLock()
        var current: (package: String, apk: String)?
        /// Per account and package: the player of this run (Play services keeps it once signed in).
        var players: [String: Player] = [:]
        /// Per account, package and scope: a token and when it stops working.
        var tokens: [String: (String, Date)] = [:]
        /// Sign-ins under way, so a second ask waits for the first rather than asking Google again.
        var pending: [String: DispatchGroup] = [:]
        /// Certificate digests by APK path.
        var digests: [String: String] = [:]
    }
    private static let state = State()

    private static func cacheKey(_ email: String, _ pkg: String) -> String { "husk.pgs.player.\(email).\(pkg)" }

    /// The account signed in to the Store, if it is a person's own.
    static func account() -> PlaySession? {
        guard let data = UserDefaults.standard.data(forKey: "husk_play_session"),
              let s = try? JSONDecoder().decode(PlaySession.self, from: data) else { return nil }
        return s
    }

    private static func target(_ game: Game) -> (package: String, apk: String?)? {
        state.lock.lock(); defer { state.lock.unlock() }
        let cur = state.current
        guard let pkg = game.package.flatMap({ $0.isEmpty ? nil : $0 }) ?? cur?.package else { return nil }
        let apk = game.apk.flatMap({ $0.isEmpty ? nil : $0 }) ?? (cur?.package == pkg ? cur?.apk : nil)
        return (pkg, apk)
    }

    /// The player the Store's account is in this game. Blocks until Google answers (or the last answer, offline).
    static func player(_ game: Game, interactive: Bool) -> Result<Player, Failure> {
        guard let s = account() else { return .failure(.noAccount) }
        guard !s.isAnonymous, !s.aasToken.isEmpty else { return .failure(.anonymousAccount) }
        guard let t = target(game) else { return .failure(.google("Husk is not running a game")) }
        let key = cacheKey(s.email, t.package)

        // One sign-in at a time per game: a second ask waits for the first one's answer.
        state.lock.lock()
        if let p = state.players[key] { state.lock.unlock(); return .success(p) }
        if let g = state.pending[key] {
            state.lock.unlock()
            _ = g.wait(timeout: .now() + 45)
            state.lock.lock()
            let p = state.players[key]
            state.lock.unlock()
            if let p { return .success(p) }
            if !interactive { return .failure(.google("the sign-in did not finish")) }
            state.lock.lock()
        }
        let group = DispatchGroup()
        group.enter()
        state.pending[key] = group
        state.lock.unlock()
        defer {
            state.lock.lock(); state.pending[key] = nil; state.lock.unlock()
            group.leave()
        }

        let result = signIn(session: s, package: t.package, apk: t.apk)
        switch result {
        case .success(let p):
            state.lock.lock(); state.players[key] = p; state.lock.unlock()
            if let data = try? JSONEncoder().encode(p) { UserDefaults.standard.set(data, forKey: key) }
            UserDefaults.standard.removeObject(forKey: "husk.pgs.failure.\(t.package)")
            HuskLog.log("pgs", "\(t.package): signed in to Play Games as \(p.name) (player \(p.id))")
            return .success(p)
        case .failure(let e):
            // Offline, Play services still knows who was signed in: so does Husk, for a player it has seen before.
            if case .network = e, let data = UserDefaults.standard.data(forKey: key), let p = try? JSONDecoder().decode(Player.self, from: data) {
                HuskLog.log("pgs", "\(t.package): offline; signed in as the last player seen (\(p.id))")
                state.lock.lock(); state.players[key] = p; state.lock.unlock()
                return .success(p)
            }
            HuskLog.log("pgs", "\(t.package): Play Games sign-in failed: \(e.errorDescription ?? "?")")
            UserDefaults.standard.set("\(Date().formatted(date: .abbreviated, time: .standard)): \(e.errorDescription ?? "?")", forKey: "husk.pgs.failure.\(t.package)")
            return .failure(e)
        }
    }

    /// A server auth code for the game's server (GamesSignInClient.requestServerSideAccess). Never cached: each is used once.
    static func serverAuthCode(_ game: Game, serverClientID: String, extraScopes: String) -> Result<String, Failure> {
        guard let s = account() else { return .failure(.noAccount) }
        guard !s.isAnonymous, !s.aasToken.isEmpty else { return .failure(.anonymousAccount) }
        guard let t = target(game) else { return .failure(.google("Husk is not running a game")) }
        guard !serverClientID.isEmpty else { return .failure(.google("the game named no server client id")) }
        let sig: String
        switch certificateDigest(apk: t.apk) { case .success(let d): sig = d; case .failure(let e): return .failure(e) }
        var scopes = ["https://www.googleapis.com/auth/games_lite"]
        scopes += extraScopes.split(separator: " ").map(String.init).filter { !$0.isEmpty }
        let service = "oauth2:server:client_id:\(serverClientID):api_scope:\(scopes.joined(separator: " "))"
        return blocking { try await token(session: s, package: t.package, sig: sig, service: service).0 }
    }

    /// The sign-in itself: a games token in the game's name, then the player.
    private static func signIn(session s: PlaySession, package: String, apk: String?) -> Result<Player, Failure> {
        let sig: String
        switch certificateDigest(apk: apk) { case .success(let d): sig = d; case .failure(let e): return .failure(e) }
        let service = "oauth2:https://www.googleapis.com/auth/games_lite"
        let tokenKey = "\(s.email)|\(package)|\(service)"
        return blocking {
            let bearer: String
            if let cached = cachedToken(tokenKey) { bearer = cached }
            else {
                let fresh = try await token(session: s, package: package, sig: sig, service: service)
                storeToken(tokenKey, fresh)
                bearer = fresh.0
            }
            return try await currentPlayer(bearer: bearer)
        }
    }

    /// A games token still good for two minutes, or none.
    private static func cachedToken(_ key: String) -> String? {
        state.lock.lock(); defer { state.lock.unlock() }
        guard let entry = state.tokens[key], entry.1.timeIntervalSinceNow > 120 else { return nil }
        return entry.0
    }
    private static func storeToken(_ key: String, _ token: (String, Date)) {
        state.lock.lock(); state.tokens[key] = token; state.lock.unlock()
    }

    // MARK: Google

    private static let urlSession: URLSession = {
        let c = URLSessionConfiguration.ephemeral
        c.timeoutIntervalForRequest = 20
        c.httpCookieStorage = nil
        c.urlCache = nil
        return URLSession(configuration: c)
    }()

    private static func form(_ params: [(String, String)]) -> String {
        var allowed = CharacterSet.alphanumerics
        allowed.insert(charactersIn: "-._~")
        return params.map { "\($0.0)=\($0.1.addingPercentEncoding(withAllowedCharacters: allowed) ?? $0.1)" }.joined(separator: "&")
    }

    /// A token from android.clients.google.com/auth in the game's name, as Play services asks for one: the game's package and
    /// certificate digest, Play services as the caller, the account's long-lived token.
    private static func token(session s: PlaySession, package: String, sig: String, service: String) async throws -> (String, Date) {
        let params: [(String, String)] = [
            ("androidId", s.gsfID),
            ("lang", PlayDevice.locale.replacingOccurrences(of: "_", with: "-")),
            ("google_play_services_version", "\(PlayDevice.playServicesVersion)"),
            ("sdk_version", "\(PlayDevice.sdk)"),
            ("device_country", PlayDevice.country),
            ("Email", s.email),
            ("accountType", "HOSTED_OR_GOOGLE"),
            ("source", "android"),
            ("service", service),
            ("app", package),
            ("client_sig", sig),
            ("callerPkg", "com.google.android.gms"),
            ("callerSig", PlayAPI.callerSig),
            ("Token", s.aasToken),
            ("has_permission", "1"),
            ("system_partition", "1"),
            ("it_caveat_types", "2"),
            ("oauth2_foreground", "1"),
            ("check_email", "1"),
        ]
        var req = URLRequest(url: PlayAPI.authURL)
        req.httpMethod = "POST"
        req.setValue(package, forHTTPHeaderField: "app")
        req.setValue(s.gsfID, forHTTPHeaderField: "device")
        req.setValue(PlayDevice.authUserAgent, forHTTPHeaderField: "User-Agent")
        req.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        req.httpBody = Data(form(params).utf8)
        let data: Data, response: URLResponse
        do { (data, response) = try await urlSession.data(for: req) } catch { throw Failure.network(error.localizedDescription) }
        var kv: [String: String] = [:]
        for line in String(decoding: data, as: UTF8.self).split(separator: "\n") {
            guard let eq = line.firstIndex(of: "=") else { continue }
            kv[String(line[..<eq])] = String(line[line.index(after: eq)...])
        }
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        guard status == 200, let auth = kv["Auth"], !auth.isEmpty else {
            let err = kv["Error"] ?? "HTTP \(status)"
            if err == "BadAuthentication" { throw Failure.google("the Google account's sign-in in Husk has expired; sign in again in the Store tab") }
            if err == "NeedPermission" { throw Failure.google("Google wants the account's consent for this game (NeedPermission); sign in to the game once on an Android device") }
            throw Failure.google(err)
        }
        let expiry: Date
        if let secs = kv["ExpiresInDurationSec"].flatMap(Double.init) { expiry = Date().addingTimeInterval(secs) }
        else if let at = kv["Expiry"].flatMap(Double.init) { expiry = Date(timeIntervalSince1970: at) }
        else { expiry = Date().addingTimeInterval(1800) }
        return (auth, expiry)
    }

    /// The signed-in player in this game, from the Play Games API; a first player profile when the account has none.
    private static func currentPlayer(bearer: String) async throws -> Player {
        func fetch() async throws -> (Int, [String: Any]) {
            var req = URLRequest(url: URL(string: "https://www.googleapis.com/games/v1/players/me")!)
            req.setValue("OAuth \(bearer)", forHTTPHeaderField: "Authorization")
            req.setValue("application/json", forHTTPHeaderField: "Accept")
            let data: Data, response: URLResponse
            do { (data, response) = try await urlSession.data(for: req) } catch { throw Failure.network(error.localizedDescription) }
            let json = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any] ?? [:]
            return ((response as? HTTPURLResponse)?.statusCode ?? 0, json)
        }
        var (status, json) = try await fetch()
        if status == 404 {
            // No Play Games profile: what Play Games makes on a phone the first time, a private one with a generated gamer tag.
            var req = URLRequest(url: URL(string: "https://www.googleapis.com/games/v1whitelisted/players/me/profilesettings?requestRandomGamerTag=true&language=\(Locale.current.languageCode ?? "en")")!)
            req.httpMethod = "PUT"
            req.setValue("OAuth \(bearer)", forHTTPHeaderField: "Authorization")
            req.setValue("application/json", forHTTPHeaderField: "Content-Type")
            req.httpBody = Data(#"{"profileVisible":false,"gamerTagIsDefault":true}"#.utf8)
            _ = try? await urlSession.data(for: req)
            (status, json) = try await fetch()
            if status == 404 { throw Failure.noProfile }
        }
        guard status == 200, let id = json["playerId"] as? String, !id.isEmpty else {
            let message = ((json["error"] as? [String: Any])?["message"] as? String) ?? "HTTP \(status)"
            throw Failure.google("Play Games answered: \(message)")
        }
        return Player(id: id,
                      name: json["displayName"] as? String ?? "",
                      avatar: json["avatarImageUrl"] as? String ?? "",
                      title: json["title"] as? String ?? "")
    }

    /// Runs async work to the end on this thread's behalf (the runtime asks from a game thread and waits for the answer).
    private final class ResultBox<T>: @unchecked Sendable { var result: Result<T, Failure> = .failure(.network("timed out")) }
    private static func blocking<T>(_ work: @escaping () async throws -> T) -> Result<T, Failure> {
        let box = ResultBox<T>()
        let done = DispatchSemaphore(value: 0)
        Task.detached {
            do { box.result = .success(try await work()) }
            catch let f as Failure { box.result = .failure(f) }
            catch { box.result = .failure(.network(error.localizedDescription)) }
            done.signal()
        }
        if done.wait(timeout: .now() + 60) == .timedOut { return .failure(.network("timed out")) }
        return box.result
    }

    // MARK: the game's certificate

    /// SHA-1 of the certificate the APK is signed with, lowercase hex: what Play services sends Google as the game's signature.
    static func certificateDigest(apk: String?) -> Result<String, Failure> {
        guard let apk else { return .failure(.noCertificate("no APK")) }
        state.lock.lock()
        if let d = state.digests[apk] { state.lock.unlock(); return .success(d) }
        state.lock.unlock()
        guard let cert = APKCertificate.first(apkPath: apk) else { return .failure(.noCertificate("no APK signature found")) }
        let digest = Insecure.SHA1.hash(data: cert).map { String(format: "%02x", $0) }.joined()
        state.lock.lock(); state.digests[apk] = digest; state.lock.unlock()
        return .success(digest)
    }
}

/// The signing certificate of an APK, as PackageManager reports it: from the APK Signing Block (v3, then v2), else from the v1
/// PKCS#7 signature under META-INF.
enum APKCertificate {
    static func first(apkPath: String) -> Data? {
        guard let fh = FileHandle(forReadingAtPath: apkPath) else { return nil }
        defer { try? fh.close() }
        let size = (try? fh.seekToEnd()) ?? 0
        guard size > 22 else { return nil }
        let tailLen = min(size, 65_557)
        try? fh.seek(toOffset: size - tailLen)
        guard let tail = try? fh.read(upToCount: Int(tailLen)) else { return nil }
        let t = [UInt8](tail)
        var eocd = -1
        var i = t.count - 22
        while i >= 0 { if t[i] == 0x50, t[i + 1] == 0x4b, t[i + 2] == 0x05, t[i + 3] == 0x06 { eocd = i; break }; i -= 1 }
        guard eocd >= 0 else { return nil }
        let cdOffset = UInt64(le32(t, eocd + 16)), cdSize = UInt64(le32(t, eocd + 12)), cdCount = Int(le16(t, eocd + 10))
        if let c = fromSigningBlock(fh, cdOffset: cdOffset) { return c }
        return fromV1(fh, cdOffset: cdOffset, cdSize: cdSize, count: cdCount)
    }

    private static func le16(_ b: [UInt8], _ o: Int) -> UInt16 { UInt16(b[o]) | UInt16(b[o + 1]) << 8 }
    private static func le32(_ b: [UInt8], _ o: Int) -> UInt32 { UInt32(b[o]) | UInt32(b[o + 1]) << 8 | UInt32(b[o + 2]) << 16 | UInt32(b[o + 3]) << 24 }
    private static func le64(_ b: [UInt8], _ o: Int) -> UInt64 { UInt64(le32(b, o)) | UInt64(le32(b, o + 4)) << 32 }

    private static func fromSigningBlock(_ fh: FileHandle, cdOffset: UInt64) -> Data? {
        guard cdOffset >= 32 else { return nil }
        try? fh.seek(toOffset: cdOffset - 24)
        guard let footData = try? fh.read(upToCount: 24), footData.count == 24 else { return nil }
        let foot = [UInt8](footData)
        guard String(decoding: foot[8..<24], as: UTF8.self) == "APK Sig Block 42" else { return nil }
        let blockSize = le64(foot, 0)
        guard blockSize >= 24, blockSize < cdOffset, blockSize < 64 << 20 else { return nil }
        try? fh.seek(toOffset: cdOffset - blockSize)
        guard let blkData = try? fh.read(upToCount: Int(blockSize - 24)) else { return nil }
        let b = [UInt8](blkData)
        var p = 0
        var v2: ArraySlice<UInt8>?, v3: ArraySlice<UInt8>?
        while p + 12 <= b.count {
            let len = Int(le64(b, p)); let id = le32(b, p + 8)
            guard len >= 4, p + 8 + len <= b.count else { break }
            let value = b[(p + 12)..<(p + 8 + len)]
            if id == 0x7109871a { v2 = value } else if id == 0xf05368c0 { v3 = value }
            p += 8 + len
        }
        for scheme in [v3, v2] {
            guard let s = scheme else { continue }
            // signers (length-prefixed) > first signer > signed data > digests, then certificates > first certificate
            let a = Array(s)
            guard a.count >= 4 else { continue }
            var q = 4                                         // the signers sequence's length
            guard q + 4 <= a.count else { continue }
            let signerLen = Int(le32(a, q)); q += 4
            guard q + signerLen <= a.count, signerLen >= 4 else { continue }
            let signedLen = Int(le32(a, q)); q += 4
            guard q + signedLen <= a.count, signedLen >= 8 else { continue }
            let digestsLen = Int(le32(a, q)); q += 4 + digestsLen
            guard q + 4 <= a.count else { continue }
            q += 4                                            // the certificates sequence's length
            guard q + 4 <= a.count else { continue }
            let certLen = Int(le32(a, q)); q += 4
            guard certLen > 0, q + certLen <= a.count else { continue }
            return Data(a[q..<(q + certLen)])
        }
        return nil
    }

    private static func fromV1(_ fh: FileHandle, cdOffset: UInt64, cdSize: UInt64, count: Int) -> Data? {
        guard cdSize > 0, cdSize < 64 << 20 else { return nil }
        try? fh.seek(toOffset: cdOffset)
        guard let cdData = try? fh.read(upToCount: Int(cdSize)) else { return nil }
        let cd = [UInt8](cdData)
        var p = 0
        for _ in 0..<count {
            guard p + 46 <= cd.count, le32(cd, p) == 0x02014b50 else { break }
            let method = le16(cd, p + 10)
            let csize = Int(le32(cd, p + 20)), usize = Int(le32(cd, p + 24))
            let nlen = Int(le16(cd, p + 28)), xlen = Int(le16(cd, p + 30)), clen = Int(le16(cd, p + 32))
            let local = UInt64(le32(cd, p + 42))
            guard p + 46 + nlen <= cd.count else { break }
            let name = String(decoding: cd[(p + 46)..<(p + 46 + nlen)], as: UTF8.self).uppercased()
            p += 46 + nlen + xlen + clen
            guard name.hasPrefix("META-INF/"), name.hasSuffix(".RSA") || name.hasSuffix(".DSA") || name.hasSuffix(".EC") else { continue }
            try? fh.seek(toOffset: local)
            guard let lh = try? fh.read(upToCount: 30), lh.count == 30 else { continue }
            let l = [UInt8](lh)
            let skip = UInt64(le16(l, 26)) + UInt64(le16(l, 28))
            try? fh.seek(toOffset: local + 30 + skip)
            guard let raw = try? fh.read(upToCount: csize) else { continue }
            let pkcs7: Data?
            if method == 0 { pkcs7 = raw } else if method == 8 { pkcs7 = inflate(raw, size: usize) } else { pkcs7 = nil }
            if let p7 = pkcs7, let cert = firstCertificate(pkcs7: [UInt8](p7)) { return cert }
        }
        return nil
    }

    private static func inflate(_ data: Data, size: Int) -> Data? {
        guard size > 0, size < 16 << 20 else { return nil }
        var out = [UInt8](repeating: 0, count: size)
        let n = data.withUnsafeBytes { src in
            compression_decode_buffer(&out, size, src.bindMemory(to: UInt8.self).baseAddress!, data.count, nil, COMPRESSION_ZLIB)
        }
        return n == size ? Data(out) : nil
    }

    /// DER: ContentInfo { oid, [0] SignedData { version, digestAlgorithms, contentInfo, [0] certificates, ... } }: the first.
    private static func firstCertificate(pkcs7 b: [UInt8]) -> Data? {
        func header(_ o: Int) -> (tag: UInt8, start: Int, len: Int)? {
            guard o + 2 <= b.count else { return nil }
            let tag = b[o]; var l = Int(b[o + 1]); var s = o + 2
            if l & 0x80 != 0 {
                let n = l & 0x7f
                guard n >= 1, n <= 4, s + n <= b.count else { return nil }
                l = 0
                for k in 0..<n { l = l << 8 | Int(b[s + k]) }
                s += n
            }
            guard s + l <= b.count else { return nil }
            return (tag, s, l)
        }
        guard let ci = header(0), ci.tag == 0x30,
              let oid = header(ci.start), oid.tag == 0x06,
              let explicit = header(oid.start + oid.len), explicit.tag == 0xa0,
              let sd = header(explicit.start), sd.tag == 0x30 else { return nil }
        var o = sd.start
        let end = sd.start + sd.len
        while o < end, let h = header(o) {
            if h.tag == 0xa0, let cert = header(h.start), cert.tag == 0x30 { return Data(b[h.start..<(cert.start + cert.len)]) }
            o = h.start + h.len
        }
        return nil
    }
}
