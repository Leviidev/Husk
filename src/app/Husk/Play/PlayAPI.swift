// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// Talking to Google Play, as the Play Store app on a phone does: sign-in tokens from android.clients.google.com/auth, device
/// registration through /checkin and /fdfe/uploadDeviceConfig, then the store itself under /fdfe -- details, search, the free
/// "purchase" that every download starts with, and delivery, which answers with the files' addresses.
///
/// Written for Husk from how those requests look on the wire; it shares no code with any other client.
struct PlayError: LocalizedError {
    let message: String
    init(_ message: String) { self.message = message }
    var errorDescription: String? { message }
}

/// Everything a signed-in session needs, kept between launches.
struct PlaySession: Codable, Equatable {
    var email: String
    /// The long-lived account token (from signing in once). Never shown, kept in the Keychain.
    var aasToken: String = ""
    /// This device's id with Google (hex), from checkin.
    var gsfID: String = ""
    var consistencyToken: String = ""
    var deviceConfigToken: String = ""
    /// The Play Store's own short-lived token, made from the account token.
    var bearer: String = ""
    /// True if using Aurora Store's anonymous token dispenser server.
    var isAnonymous: Bool = false
}

enum PlayAPI {
    static let authURL = URL(string: "https://android.clients.google.com/auth")!
    static let checkinURL = URL(string: "https://android.clients.google.com/checkin")!
    static let fdfe = "https://android.clients.google.com/fdfe/"
    static let defaultDispenserURL = "https://auroraoss.com/api/auth"
    /// The signature of the Google app the requests speak for (Play Services' certificate digest).
    static let callerSig = "38918a453d07199354f8b19af05ec6562ced5788"

    private static let urlSession: URLSession = {
        let c = URLSessionConfiguration.ephemeral
        c.timeoutIntervalForRequest = 30
        c.httpCookieStorage = nil
        c.urlCache = nil
        return URLSession(configuration: c)
    }()

    // MARK: Aurora guest session

    /// Obtains an anonymous guest token from Aurora's token dispenser server.
    static func fetchAuroraGuestSession(dispenserURL: String = defaultDispenserURL) async throws -> PlaySession {
        guard let url = URL(string: dispenserURL) else {
            throw PlayError(L10n.tr("Invalid dispenser URL: \(dispenserURL)"))
        }
        var req = URLRequest(url: url)
        req.httpMethod = "POST"
        req.setValue("com.aurora.store-4.6.1-70", forHTTPHeaderField: "User-Agent")
        req.setValue("application/json", forHTTPHeaderField: "Content-Type")

        let deviceProps: [String: String] = [
            "Build.HARDWARE": PlayDevice.hardware,
            "Build.RADIO": PlayDevice.radio,
            "Build.FINGERPRINT": PlayDevice.fingerprint,
            "Build.BRAND": PlayDevice.brand,
            "Build.DEVICE": PlayDevice.device,
            "Build.VERSION.SDK_INT": "\(PlayDevice.sdk)",
            "Build.VERSION.RELEASE": PlayDevice.release,
            "Build.MODEL": PlayDevice.model,
            "Build.MANUFACTURER": PlayDevice.manufacturer,
            "Build.PRODUCT": PlayDevice.product,
            "Build.ID": PlayDevice.buildID,
            "Build.BOOTLOADER": PlayDevice.bootloader,
            "Platforms": PlayDevice.platforms.joined(separator: ","),
            "Locales": PlayDevice.locale,
            "Features": PlayDevice.features.joined(separator: ","),
            "SharedLibraries": PlayDevice.sharedLibraries.joined(separator: ","),
            "GL.Extensions": PlayDevice.glExtensions.joined(separator: ","),
            "GL.Version": "\(PlayDevice.glEsVersion)",
            "Screen.Density": "\(PlayDevice.screenDensity)",
            "Screen.Width": "\(PlayDevice.screenWidth)",
            "Screen.Height": "\(PlayDevice.screenHeight)",
            "Client": "android-google",
            "GSF.version": "\(PlayDevice.playServicesVersion)",
            "Vending.version": "\(PlayDevice.vendingVersion)",
            "Vending.versionString": PlayDevice.vendingVersionString,
            "SimOperator": PlayDevice.simOperator,
            "CellOperator": PlayDevice.cellOperator
        ]

        req.httpBody = try? JSONSerialization.data(withJSONObject: deviceProps)

        // The dispenser is a free service of the Aurora Store project; when Google refuses its shared accounts it answers 5xx for
        // hours, so the message says what is down and that signing in still works
        let unavailable = L10n.tr("The anonymous login service (Aurora's token dispenser) is not working right now")
        let advice = L10n.tr("Sign in with a Google account instead, or try again later.")
        let data: Data, response: URLResponse
        do { (data, response) = try await urlSession.data(for: req) } catch { throw PlayError("\(unavailable) (\(error.localizedDescription)). \(advice)") }
        guard (response as? HTTPURLResponse)?.statusCode == 200 else {
            throw PlayError(L10n.tr("\(unavailable) (HTTP \((response as? HTTPURLResponse)?.statusCode ?? 0)). \(advice)"))
        }

        struct DispenserResponse: Decodable {
            let email: String?
            let auth: String?
            let authToken: String?
            let gsfId: String?
            let deviceConfigToken: String?
            let deviceCheckInConsistencyToken: String?
        }

        let decoded = try JSONDecoder().decode(DispenserResponse.self, from: data)
        guard let bearer = decoded.authToken ?? decoded.auth, !bearer.isEmpty else {
            throw PlayError(L10n.tr("Aurora token dispenser returned an empty auth token."))
        }

        let email = decoded.email ?? "anonymous@auroraoss.com"
        let gsf = decoded.gsfId ?? "38918a453d071993"
        let consistency = decoded.deviceCheckInConsistencyToken ?? ""
        let configTok = decoded.deviceConfigToken ?? ""

        return PlaySession(
            email: email,
            aasToken: "",
            gsfID: gsf,
            consistencyToken: consistency,
            deviceConfigToken: configTok,
            bearer: bearer,
            isAnonymous: true
        )
    }

    // MARK: sign-in tokens

    /// The `key=value` lines /auth answers with.
    private static func parseKeyValues(_ data: Data) -> [String: String] {
        var out: [String: String] = [:]
        for line in String(decoding: data, as: UTF8.self).split(separator: "\n") {
            guard let eq = line.firstIndex(of: "=") else { continue }
            out[String(line[..<eq])] = String(line[line.index(after: eq)...])
        }
        return out
    }

    private static func form(_ params: [(String, String)]) -> String {
        var allowed = CharacterSet.alphanumerics
        allowed.insert(charactersIn: "-._~")
        return params.map { "\($0.0)=\($0.1.addingPercentEncoding(withAllowedCharacters: allowed) ?? $0.1)" }.joined(separator: "&")
    }

    /// The web sign-in leaves an `oauth_token` cookie; this trades it for the account token that lasts.
    static func accountToken(email: String, oauthToken: String) async throws -> String {
        let params: [(String, String)] = [
            ("lang", PlayDevice.locale.replacingOccurrences(of: "_", with: "-")),
            ("google_play_services_version", "\(PlayDevice.playServicesVersion)"),
            ("sdk_version", "\(PlayDevice.sdk)"),
            ("device_country", PlayDevice.country),
            ("Email", email),
            ("service", "ac2dm"),
            ("get_accountid", "1"),
            ("ACCESS_TOKEN", "1"),
            ("callerPkg", "com.google.android.gms"),
            ("add_account", "1"),
            ("Token", oauthToken),
            ("callerSig", callerSig),
            ("droidguard_results", "null"),
        ]
        var req = URLRequest(url: authURL)
        req.httpMethod = "POST"
        req.setValue("com.google.android.gms", forHTTPHeaderField: "app")
        req.setValue(PlayDevice.authUserAgent, forHTTPHeaderField: "User-Agent")
        req.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        req.httpBody = Data(form(params).utf8)
        let (data, response) = try await urlSession.data(for: req)
        let kv = parseKeyValues(data)
        guard (response as? HTTPURLResponse)?.statusCode == 200, let token = kv["Token"], !token.isEmpty else {
            throw PlayError(L10n.tr("Google did not accept the sign-in (\(kv["Error"] ?? "HTTP \((response as? HTTPURLResponse)?.statusCode ?? 0)"))."))
        }
        return token
    }

    /// The Play Store's own token, made from the account token for this device.
    static func playToken(_ s: PlaySession) async throws -> String {
        let params: [(String, String)] = [
            ("androidId", s.gsfID),
            ("sdk_version", "\(PlayDevice.sdk)"),
            ("Email", s.email),
            ("google_play_services_version", "\(PlayDevice.playServicesVersion)"),
            ("device_country", PlayDevice.country),
            ("lang", PlayDevice.locale.replacingOccurrences(of: "_", with: "-")),
            ("callerSig", callerSig),
            ("app", "com.android.vending"),
            ("client_sig", callerSig),
            ("callerPkg", "com.google.android.gms"),
            ("Token", s.aasToken),
            ("oauth2_foreground", "1"),
            ("token_request_options", "CAA4AVAB"),
            ("check_email", "1"),
            ("system_partition", "1"),
            ("service", "oauth2:https://www.googleapis.com/auth/googleplay"),
            ("droidguard_results", "null"),
        ]
        var req = URLRequest(url: authURL)
        req.httpMethod = "POST"
        req.setValue("com.google.android.gms", forHTTPHeaderField: "app")
        req.setValue(PlayDevice.authUserAgent, forHTTPHeaderField: "User-Agent")
        req.setValue(s.gsfID, forHTTPHeaderField: "device")
        req.setValue("application/x-www-form-urlencoded", forHTTPHeaderField: "Content-Type")
        req.httpBody = Data(form(params).utf8)
        let (data, response) = try await urlSession.data(for: req)
        let kv = parseKeyValues(data)
        guard (response as? HTTPURLResponse)?.statusCode == 200, let token = kv["Auth"], !token.isEmpty else {
            throw PlayError(L10n.tr("Google Play would not open a session (\(kv["Error"] ?? "HTTP \((response as? HTTPURLResponse)?.statusCode ?? 0)"))."))
        }
        return token
    }

    // MARK: registering the device

    /// Checkin: Google gives this device an id (and a consistency token).
    static func checkin() async throws -> (gsfID: String, consistency: String) {
        var req = URLRequest(url: checkinURL)
        req.httpMethod = "POST"
        req.setValue("com.google.android.gms", forHTTPHeaderField: "app")
        req.setValue(PlayDevice.authUserAgent, forHTTPHeaderField: "User-Agent")
        req.setValue("application/x-protobuffer", forHTTPHeaderField: "Content-Type")
        req.httpBody = PlayDevice.checkinRequest()
        let (data, response) = try await urlSession.data(for: req)
        guard (response as? HTTPURLResponse)?.statusCode == 200 else {
            throw PlayError(L10n.tr("Google did not register the device (HTTP \((response as? HTTPURLResponse)?.statusCode ?? 0))."))
        }
        let reply = ProtoMessage(data)
        guard let id = reply.uint64(7), id != 0 else { throw PlayError(L10n.tr("Google registered the device but gave it no id.")) }
        return (String(id, radix: 16), reply.string(12) ?? "")
    }

    /// Tell Play what this device is, so it serves the right builds.
    static func uploadDeviceConfig(_ s: PlaySession) async throws -> String {
        var w = ProtoWriter()
        w.message(1, PlayDevice.configuration)
        let reply = try await fdfeCall("uploadDeviceConfig", session: s, body: w.data)
        return reply.message(28)?.string(1) ?? ""
    }

    // MARK: the store

    private static func headers(_ s: PlaySession) -> [String: String] {
        var h = [
            "Authorization": "Bearer \(s.bearer)",
            "User-Agent": PlayDevice.finskyUserAgent,
            "X-DFE-Device-Id": s.gsfID,
            "Accept-Language": PlayDevice.locale.replacingOccurrences(of: "_", with: "-"),
            "X-DFE-Client-Id": "am-android-google",
            "X-DFE-Network-Type": "4",
            "X-DFE-Content-Filters": "",
            "X-Limit-Ad-Tracking-Enabled": "false",
            "X-Ad-Id": "",
            "X-DFE-UserLanguages": PlayDevice.locale,
            "X-DFE-Request-Params": "timeoutMs=4000",
            "X-DFE-MCCMNC": PlayDevice.simOperator,
        ]
        if !s.consistencyToken.isEmpty { h["X-DFE-Device-Checkin-Consistency-Token"] = s.consistencyToken }
        if !s.deviceConfigToken.isEmpty { h["X-DFE-Device-Config-Token"] = s.deviceConfigToken }
        return h
    }

    /// One /fdfe call; the reply's payload (ResponseWrapper.payload, or the prefetched one when that is where the answer is).
    @discardableResult
    static func fdfeCall(_ path: String, session s: PlaySession, query: [(String, String)] = [], body: Data? = nil,
                         post: Bool = false) async throws -> ProtoMessage {
        var comps = URLComponents(string: fdfe + path)!
        if !query.isEmpty { comps.queryItems = (comps.queryItems ?? []) + query.map { URLQueryItem(name: $0.0, value: $0.1) } }
        var req = URLRequest(url: comps.url!)
        req.httpMethod = (body != nil || post) ? "POST" : "GET"
        for (k, v) in headers(s) { req.setValue(v, forHTTPHeaderField: k) }
        if let body {
            req.httpBody = body
            req.setValue("application/x-protobuf", forHTTPHeaderField: "Content-Type")
        }
        let (data, response) = try await urlSession.data(for: req)
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        let wrapper = ProtoMessage(data)
        if status == 401 { throw PlayError(L10n.tr("Google Play's session has ended; signing in again.")) }
        guard status == 200 else {
            let message = wrapper.message(5)?.string(1) ?? "HTTP \(status)"    // ServerCommands.displayErrorMessage
            throw PlayError(L10n.tr("Google Play said no: \(message)"))
        }
        if let payload = wrapper.message(1), !payload.fields.isEmpty { return payload }
        return wrapper.message(3)?.message(2)?.message(1) ?? ProtoMessage(Data())
    }

    // MARK: - Store APIs & Models

    public struct PlayApp: Identifiable, Hashable {
        public let id: String // packageName
        public let title: String
        public let developer: String
        public let summary: String
        public let iconURL: String?
        public let screenshotURLs: [String]
        public let versionCode: Int
        public let versionString: String
        public let sizeBytes: Int64
        public let downloads: String
        public let rating: Double
        public let formattedPrice: String
        public let isFree: Bool

        public static func isValidPackageName(_ id: String) -> Bool {
            id.range(of: "^[a-zA-Z][a-zA-Z0-9_]*(\\.[a-zA-Z][a-zA-Z0-9_]+)+$", options: .regularExpression) != nil
        }

        public static func from(doc: ProtoMessage) -> PlayApp? {
            guard let id = doc.string(1), isValidPackageName(id) else { return nil }

            let titleCandidate = doc.string(5) ?? id
            // Reject entries where title is a URL, tag, or has non-printable/empty junk
            if titleCandidate.hasPrefix("http://") || titleCandidate.hasPrefix("https://") || titleCandidate.hasPrefix("fbtag;") {
                return nil
            }
            let title = titleCandidate
            let dev = doc.string(6) ?? ""
            let desc = doc.string(7) ?? ""

            // Images: field 10 (imageType 4 = icon, 1 or 2 = screenshot)
            var icon: String? = nil
            var screenshots: [String] = []
            for img in doc.messages(10) {
                let imgType = img.int(1) ?? 0
                if let url = img.string(5), url.hasPrefix("http") {
                    if imgType == 4 && icon == nil {
                        icon = url
                    } else if imgType == 1 || imgType == 2 {
                        screenshots.append(url)
                    } else if icon == nil {
                        icon = url
                    }
                }
            }

            // Offer / Price: field 8
            var price = L10n.tr("Free")
            var isFree = true
            if let offer = doc.messages(8).first {
                if let p = offer.string(7), !p.isEmpty {
                    price = p
                }
                let mic = offer.int(1) ?? 0
                isFree = (mic == 0)
            }

            // App details: field 13 message 1
            let appDetails = doc.message(13)?.message(1)
            let vc = Int(appDetails?.int(3) ?? 0)
            let vs = appDetails?.string(4) ?? ""
            let size = appDetails?.int(9) ?? 0
            let downloads = appDetails?.string(13) ?? ""

            // Aggregate rating: field 14 message 1 (stars / 5.0)
            var ratingVal = 0.0
            if let agg = doc.message(14) {
                if let f = agg.float(2) {
                    ratingVal = Double(round(f * 10) / 10)
                } else if let r = agg.string(1), let d = Double(r) {
                    ratingVal = d
                }
            }

            return PlayApp(
                id: id,
                title: title,
                developer: dev,
                summary: desc,
                iconURL: icon,
                screenshotURLs: screenshots,
                versionCode: vc,
                versionString: vs,
                sizeBytes: size,
                downloads: downloads,
                rating: ratingVal,
                formattedPrice: price,
                isFree: isFree
            )
        }

        public static func parseApps(from message: ProtoMessage) -> [PlayApp] {
            var apps: [PlayApp] = []
            var visited = Set<String>()

            func traverse(_ msg: ProtoMessage) {
                // If this is a DocV2
                if let docid = msg.string(1), isValidPackageName(docid), !visited.contains(docid) {
                    if let app = PlayApp.from(doc: msg) {
                        visited.insert(docid)
                        apps.append(app)
                    }
                }

                // Recurse into child messages across all fields
                for (_, vals) in msg.fields {
                    for val in vals {
                        if case .bytes(let d) = val {
                            let child = ProtoMessage(d)
                            if !child.fields.isEmpty {
                                traverse(child)
                            }
                        }
                    }
                }
            }

            traverse(message)
            return apps
        }
    }

    /// Popular apps featured on the Store home screen
    static let featuredPackageNames: [String] = [
        "com.kiloo.subwaysurf",
        "com.robtopx.geometryjumplite",
        "com.supercell.clashofclans",
        "com.mojang.minecraftpe",
        "org.videolan.vlc",
        "com.halfbrick.fruitninjafree",
        "com.imangi.templerun2",
        "com.vectorunit.cobalt.googleplay",
        "com.ea.game.pvzfree_row",
        "com.fingersoft.hillclimb",
        "org.libretro.retroarch"
    ]

    /// Browse top / featured apps
    static func browse(session: PlaySession) async throws -> [PlayApp] {
        // Fetch details concurrently for the featured apps
        var results: [PlayApp] = []
        await withTaskGroup(of: PlayApp?.self) { group in
            for pkg in featuredPackageNames {
                group.addTask {
                    try? await details(packageName: pkg, session: session)
                }
            }
            for await app in group {
                if let app {
                    results.append(app)
                }
            }
        }

        if !results.isEmpty {
            // Sort by popularity index matching the list order
            results.sort { a, b in
                let ia = featuredPackageNames.firstIndex(of: a.id) ?? 999
                let ib = featuredPackageNames.firstIndex(of: b.id) ?? 999
                return ia < ib
            }
            return results
        }

        // Fallback to native browse if individual details failed
        let q = [("c", "3")]
        let payload = try await fdfeCall("browse", session: session, query: q)
        return PlayApp.parseApps(from: payload)
    }

    /// Search Google Play
    static func search(query: String, session: PlaySession) async throws -> [PlayApp] {
        let trimmed = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmed.isEmpty else { return [] }

        // Helper to extract valid Android package names from any string / HTML
        func extractPackageNames(from text: String) -> [String] {
            var found: [String] = []
            let patterns = [
                "/store/apps/details\\?id=([a-zA-Z][a-zA-Z0-9_]*(?:\\.[a-zA-Z][a-zA-Z0-9_]+)+)",
                "id=([a-zA-Z][a-zA-Z0-9_]*(?:\\.[a-zA-Z][a-zA-Z0-9_]+)+)",
                "\\b([a-zA-Z][a-zA-Z0-9_]*(?:\\.[a-zA-Z][a-zA-Z0-9_]+)+)\\b"
            ]
            for pattern in patterns {
                guard let regex = try? NSRegularExpression(pattern: pattern) else { continue }
                let matches = regex.matches(in: text, range: NSRange(text.startIndex..., in: text))
                for match in matches {
                    let rangeIdx = match.numberOfRanges > 1 ? 1 : 0
                    if let range = Range(match.range(at: rangeIdx), in: text) {
                        let candidate = String(text[range])
                        if PlayApp.isValidPackageName(candidate) && !found.contains(candidate) {
                            found.append(candidate)
                        }
                    }
                }
            }
            return found
        }

        var candidatePackages: [String] = []

        // 1. If an exact package name was searched, prioritize it
        if PlayApp.isValidPackageName(trimmed) {
            candidatePackages.append(trimmed)
        }

        // 2. Direct fdfe/search call with ksm=1
        let q = [
            ("c", "3"),
            ("q", trimmed),
            ("ksm", "1")
        ]
        if let payload = try? await fdfeCall("search", session: session, query: q) {
            let immediateApps = PlayApp.parseApps(from: payload)
            for app in immediateApps {
                if !candidatePackages.contains(app.id) {
                    candidatePackages.append(app.id)
                }
            }

            // Follow searchList stream continuation token if present
            for (_, vals) in payload.fields {
                for val in vals {
                    if case .bytes(let d) = val {
                        let text = String(decoding: d, as: UTF8.self)
                        let pattern = "searchList\\?q=[^\"\\s&]+"
                        if let range = text.range(of: pattern, options: .regularExpression) {
                            let streamPath = String(text[range])
                            if let streamPayload = try? await fdfeCall(streamPath, session: session) {
                                for app in PlayApp.parseApps(from: streamPayload) {
                                    if !candidatePackages.contains(app.id) {
                                        candidatePackages.append(app.id)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // 3. fdfe/searchSuggest call
        if candidatePackages.count < 10 {
            let suggestQuery = [
                ("q", trimmed),
                ("sb", "5"),
                ("sst", "2"),
                ("sst", "3")
            ]
            if let suggestMsg = try? await fdfeCall("searchSuggest", session: session, query: suggestQuery) {
                for app in PlayApp.parseApps(from: suggestMsg) {
                    if !candidatePackages.contains(app.id) {
                        candidatePackages.append(app.id)
                    }
                }
            }
        }

        // 4. Web search fallback: query Google Play's public search catalog to obtain top ranked package IDs
        if candidatePackages.count < 10 {
            var comps = URLComponents(string: "https://play.google.com/store/search")!
            comps.queryItems = [
                URLQueryItem(name: "q", value: trimmed),
                URLQueryItem(name: "c", value: "apps"),
                URLQueryItem(name: "hl", value: "en"),
                URLQueryItem(name: "gl", value: PlayDevice.country)
            ]
            if let webURL = comps.url {
                var webReq = URLRequest(url: webURL)
                webReq.setValue("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36", forHTTPHeaderField: "User-Agent")
                if let (data, resp) = try? await urlSession.data(for: webReq),
                   (resp as? HTTPURLResponse)?.statusCode == 200,
                   let html = String(data: data, encoding: .utf8) {
                    for pkg in extractPackageNames(from: html) {
                        if !candidatePackages.contains(pkg) {
                            candidatePackages.append(pkg)
                            if candidatePackages.count >= 20 { break }
                        }
                    }
                }
            }
        }

        // 5. Featured apps substring matching fallback
        for pkg in featuredPackageNames {
            if (pkg.localizedCaseInsensitiveContains(trimmed) ||
                pkg.replacingOccurrences(of: ".", with: " ").localizedCaseInsensitiveContains(trimmed)) &&
                !candidatePackages.contains(pkg) {
                candidatePackages.append(pkg)
            }
        }

        // If no candidate packages were identified, return empty
        if candidatePackages.isEmpty {
            return []
        }

        // Limit candidate list to top 20 to avoid excessive queries
        let topCandidates = Array(candidatePackages.prefix(20))

        // Resolve rich details concurrently for each package
        var resolvedApps: [PlayApp] = []
        await withTaskGroup(of: PlayApp?.self) { group in
            for pkg in topCandidates {
                group.addTask {
                    try? await details(packageName: pkg, session: session)
                }
            }
            for await app in group {
                if let app, PlayApp.isValidPackageName(app.id) {
                    resolvedApps.append(app)
                }
            }
        }

        // Sort resolved apps to preserve the candidate package ranking order
        resolvedApps.sort { a, b in
            let ia = topCandidates.firstIndex(of: a.id) ?? 999
            let ib = topCandidates.firstIndex(of: b.id) ?? 999
            return ia < ib
        }

        return resolvedApps
    }

    /// Get full details for a single app
    static func details(packageName: String, session: PlaySession) async throws -> PlayApp? {
        let q = [
            ("doc", packageName)
        ]
        let payload = try await fdfeCall("details", session: session, query: q)

        // Payload -> detailsResponse (field 2) -> docV2 (field 4)
        let detResp = payload.message(2)
        if let doc = detResp?.message(4), var app = PlayApp.from(doc: doc) {
            // Extract latest versionCode from prefetch/cluster info (field 6.2.11.2) if not in appDetails
            if app.versionCode <= 0 {
                let f6 = detResp?.message(6)
                let f2 = f6?.message(2)
                let f11 = f2?.message(11)
                if let vc = f11?.int(2), vc > 0 {
                    app = PlayApp(
                        id: app.id,
                        title: app.title,
                        developer: app.developer,
                        summary: app.summary,
                        iconURL: app.iconURL,
                        screenshotURLs: app.screenshotURLs,
                        versionCode: Int(vc),
                        versionString: app.versionString,
                        sizeBytes: app.sizeBytes,
                        downloads: app.downloads,
                        rating: app.rating,
                        formattedPrice: app.formattedPrice,
                        isFree: app.isFree
                    )
                }
            }
            return app
        }

        let apps = PlayApp.parseApps(from: payload)
        return apps.first(where: { $0.id == packageName }) ?? apps.first
    }

    public struct DeliveryData {
        public struct SplitFile {
            public let name: String
            public let size: Int64
            public let downloadUrl: String
        }
        public let downloadUrl: String?
        public let size: Int64
        public let splits: [SplitFile]
        public let downloadAuthCookie: (name: String, value: String)?
        /// Expansion files (OBB): a game's data kept beside the APK, "main" and sometimes "patch"
        public struct ExpansionFile {
            public let isPatch: Bool
            public let versionCode: Int64
            public let size: Int64
            public let downloadUrl: String
        }
        public var expansions: [ExpansionFile] = []
    }

    /// "Purchase" (license) a free app so Google Play allows delivery. Returns encoded delivery token if provided.
    @discardableResult
    static func purchase(packageName: String, versionCode: Int, session: PlaySession) async throws -> String? {
        let q = [
            ("doc", packageName),
            ("ot", "1"),
            ("vc", "\(versionCode)")
        ]
        // POST to fdfe/purchase
        let payload = try await fdfeCall("purchase", session: session, query: q, post: true)
        // buyResponse is field 4, encodedDeliveryToken is field 55
        let buyResponse = payload.message(4)
        return buyResponse?.string(55)
    }

    /// Request delivery URL(s) for an app
    static func delivery(packageName: String, versionCode: Int, deliveryToken: String? = nil, session: PlaySession) async throws -> DeliveryData {
        var q = [
            ("doc", packageName),
            ("ot", "1"),
            ("vc", "\(versionCode)")
        ]
        if let deliveryToken, !deliveryToken.isEmpty {
            q.append(("dtok", deliveryToken))
        }
        let payload = try await fdfeCall("delivery", session: session, query: q)

        // Delivery response: field 21 message 2 (AndroidAppDeliveryData)
        let delivResp = payload.message(21)
        let status = delivResp?.int(1) ?? 1
        if status == 2 {
            throw PlayError(L10n.tr("Google Play reported app is incompatible with this device profile (status 2)."))
        } else if status == 3 {
            throw PlayError(L10n.tr("App requires purchase or license before download."))
        }

        guard let deliveryDataMsg = delivResp?.message(2) else {
            throw PlayError(L10n.tr("Google Play did not return download information."))
        }

        if ProcessInfo.processInfo.environment["HUSK_PLAY_DUMP"] != nil {
            // what Play sent, field by field (asset packs and expansion files are in fields Husk did not read)
            func dump(_ m: ProtoMessage, _ indent: String, _ depth: Int) {
                for k in m.fields.keys.sorted() {
                    let subs = m.messages(k)
                    print("\(indent)\(k): \(subs.isEmpty || depth > 2 ? (m.string(k).map { String($0.prefix(80)) } ?? m.int(k).map(String.init) ?? "?") : "\(subs.count) message(s)")")
                    if depth <= 2 { for sub in subs.prefix(6) { dump(sub, indent + "    ", depth + 1) } }
                }
            }
            print("delivery response:"); if let r = delivResp { dump(r, "  ", 0) }
        }
        let downloadUrl = deliveryDataMsg.string(3)
        let downloadSize = deliveryDataMsg.int(1) ?? 0

        // Splits: field 15
        var splits: [DeliveryData.SplitFile] = []
        for splitMsg in deliveryDataMsg.messages(15) {
            let sName = splitMsg.string(1) ?? "split"
            let sSize = splitMsg.int(2) ?? 0
            if let sUrl = splitMsg.string(5) {
                splits.append(DeliveryData.SplitFile(name: sName, size: sSize, downloadUrl: sUrl))
            }
        }

        // Cookie: field 5
        var cookie: (name: String, value: String)? = nil
        if let cookieMsg = deliveryDataMsg.messages(5).first,
           let cName = cookieMsg.string(1),
           let cVal = cookieMsg.string(2) {
            cookie = (cName, cVal)
        }

        // Expansion files: field 4 (AppFileMetadata: 1 file type, 0 main and 1 patch; 2 version code; 3 size; 4 URL)
        var expansions: [DeliveryData.ExpansionFile] = []
        for f in deliveryDataMsg.messages(4) {
            if let url = f.string(4) {
                expansions.append(DeliveryData.ExpansionFile(isPatch: f.int(1) == 1, versionCode: f.int(2) ?? Int64(versionCode),
                                                             size: f.int(3) ?? 0, downloadUrl: url))
            }
        }

        var data = DeliveryData(
            downloadUrl: downloadUrl,
            size: downloadSize,
            splits: splits,
            downloadAuthCookie: cookie
        )
        data.expansions = expansions
        return data
    }

    /// Downloads the APK (and splits if present) into a temporary directory and returns the list of file URLs.
    static func downloadApp(
        packageName: String,
        versionCode: Int,
        session: PlaySession,
        stagingDirectory: URL? = nil,
        progress: (@MainActor (Double) -> Void)? = nil
    ) async throws -> [URL] {
        // Purchase first to obtain delivery token, then request delivery
        var dtok: String? = nil
        do {
            dtok = try await purchase(packageName: packageName, versionCode: versionCode, session: session)
        } catch {
            // Ignore if purchase fails or already owned
        }
        let deliveryData = try await delivery(packageName: packageName, versionCode: versionCode, deliveryToken: dtok, session: session)

        var downloadsToMake: [(name: String, url: String, size: Int64)] = []
        if let base = deliveryData.downloadUrl {
            downloadsToMake.append(("\(packageName).apk", base, deliveryData.size))
        }
        for split in deliveryData.splits {
            downloadsToMake.append(("\(packageName).\(split.name).apk", split.downloadUrl, split.size))
        }
        // Expansion files keep the name Android gives them; the store puts them where the game looks (obb/ beside the APKs)
        for x in deliveryData.expansions {
            downloadsToMake.append(("\(x.isPatch ? "patch" : "main").\(x.versionCode).\(packageName).obb", x.downloadUrl, x.size))
        }

        guard !downloadsToMake.isEmpty else {
            throw PlayError(L10n.tr("No download URLs available for \(packageName)."))
        }

        let totalExpectedBytes = downloadsToMake.reduce(Int64(0)) { $0 + max(0, $1.size) }
        // stagingDirectory: a folder on the destination's disk (the Mac downloader's games go to an external one; the temporary
        // folder is always on the internal disk)
        let tempDir = (stagingDirectory ?? FileManager.default.temporaryDirectory).appendingPathComponent("PlayDownload_\(UUID().uuidString)", isDirectory: true)
        try FileManager.default.createDirectory(at: tempDir, withIntermediateDirectories: true)

        var downloadedFiles: [URL] = []
        var totalBytesWritten: Int64 = 0

        for item in downloadsToMake {
            guard let dlURL = URL(string: item.url) else { continue }
            var req = URLRequest(url: dlURL)
            req.setValue(PlayDevice.finskyUserAgent, forHTTPHeaderField: "User-Agent")
            if let c = deliveryData.downloadAuthCookie {
                req.setValue("\(c.name)=\(c.value)", forHTTPHeaderField: "Cookie")
            }

            let fileDest = tempDir.appendingPathComponent(item.name)
            let status = try await FileStreamer.download(req, to: fileDest)
            guard status == 200 else {
                try? FileManager.default.removeItem(at: fileDest)
                throw PlayError(L10n.tr("Failed to download \(item.name) (HTTP \(status))"))
            }
            downloadedFiles.append(fileDest)

            totalBytesWritten += item.size
            if totalExpectedBytes > 0 {
                let frac = min(1.0, Double(totalBytesWritten) / Double(totalExpectedBytes))
                await progress?(frac)
            }
        }

        await progress?(1.0)
        return downloadedFiles
    }
}



/// Writes a download straight into its file as it arrives. A download task would first put the whole file in the system's temporary
/// folder and then move it: twice the space at the peak, and on the internal disk whatever the destination (the Mac downloader's
/// games go to an external one).
private final class FileStreamer: NSObject, URLSessionDataDelegate {
    private let handle: FileHandle
    private var status = 0
    private var failure: Error?
    private var continuation: CheckedContinuation<Int, Error>?

    private init(handle: FileHandle) { self.handle = handle }

    static func download(_ request: URLRequest, to destination: URL) async throws -> Int {
        FileManager.default.createFile(atPath: destination.path, contents: nil)
        let handle = try FileHandle(forWritingTo: destination)
        let streamer = FileStreamer(handle: handle)
        let session = URLSession(configuration: .default, delegate: streamer, delegateQueue: nil)
        defer { session.finishTasksAndInvalidate(); try? handle.close() }
        do {
            // Cancelling the download's Task (the Store's stop button) cancels the transfer
            return try await withTaskCancellationHandler {
                try await withCheckedThrowingContinuation { continuation in
                    streamer.continuation = continuation
                    session.dataTask(with: request).resume()
                }
            } onCancel: {
                session.invalidateAndCancel()
            }
        } catch {
            try? FileManager.default.removeItem(at: destination)
            throw error
        }
    }

    func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive response: URLResponse,
                    completionHandler: @escaping (URLSession.ResponseDisposition) -> Void) {
        status = (response as? HTTPURLResponse)?.statusCode ?? 0
        completionHandler(status == 200 ? .allow : .cancel)
    }

    func urlSession(_ session: URLSession, dataTask: URLSessionDataTask, didReceive data: Data) {
        guard failure == nil else { return }
        do { try handle.write(contentsOf: data) } catch { failure = error; dataTask.cancel() }
    }

    func urlSession(_ session: URLSession, task: URLSessionTask, didCompleteWithError error: Error?) {
        let c = continuation
        continuation = nil
        if let failure { c?.resume(throwing: failure) }
        else if status != 200 { c?.resume(returning: status) }
        else if let error { c?.resume(throwing: error) }
        else { c?.resume(returning: status) }
    }
}
