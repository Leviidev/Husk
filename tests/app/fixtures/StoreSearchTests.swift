import Foundation

enum FixtureError: Error { case unexpectedRequest, missingRequest }
enum HuskLog { static func log(_ category: String, _ text: String) {} }
@MainActor final class IncomingFiles {
    static let shared = IncomingFiles()
    func receive(_ files: [URL]) {}
}

final class StubProtocol: URLProtocol {
    private static let lock = NSLock()
    private static var handler: (URLRequest) throws -> (Int, Data) = { _ in throw FixtureError.unexpectedRequest }
    private static var urls: [URL] = []
    static func configure(_ action: @escaping (URLRequest) throws -> (Int, Data)) {
        lock.lock(); defer { lock.unlock() }
        handler = action; urls = []
    }
    static func requests() -> [URL] { lock.lock(); defer { lock.unlock() }; return urls }
    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
    override func startLoading() {
        Self.lock.lock(); let respond = Self.handler; Self.urls.append(request.url!); Self.lock.unlock()
        do {
            let (status, data) = try respond(request)
            client?.urlProtocol(self, didReceive: HTTPURLResponse(url: request.url!, statusCode: status, httpVersion: nil, headerFields: nil)!, cacheStoragePolicy: .notAllowed)
            client?.urlProtocol(self, didLoad: data)
            client?.urlProtocolDidFinishLoading(self)
        } catch { client?.urlProtocol(self, didFailWithError: error) }
    }
    override func stopLoading() {}
}

func document(_ id: String, type: Int = 1) -> Data {
    var d = ProtoWriter()
    d.string(1, id); d.int(3, type); d.int(4, 3)
    d.string(5, id == "org.example.target" ? "Народная карта" : id)
    d.message(13) { $0.message(1) { $0.int(3, 42); $0.string(4, "1.0") } }
    return d.data
}
func response(_ payload: ProtoWriter) -> Data { var w = ProtoWriter(); w.bytes(1, payload.data); return w.data }
func details(_ id: String) -> Data {
    var p = ProtoWriter(); p.message(2) { $0.bytes(4, document(id)) }; return response(p)
}
func page(_ ids: [String], cursor: String?) -> Data {
    var p = ProtoWriter()
    p.message(1) { list in list.message(2) { root in
        root.string(1, "apps_cluster.org.example"); root.int(3, 45); root.string(5, "A cluster is not an app")
        for id in ids { root.bytes(11, document(id)) }
        // Dependency references and split metadata must never become search results.
        root.message(20) { $0.string(1, "com.google.android.gms"); $0.int(3, 3); $0.int(4, 1) }
        root.message(21) { $0.string(1, "config.arm64_v8a"); $0.string(2, "split") }
        if let cursor { root.message(12) { $0.string(2, cursor) } }
    } }
    return response(p)
}
func transport() -> URLSession {
    let c = URLSessionConfiguration.ephemeral; c.protocolClasses = [StubProtocol.self]; return URLSession(configuration: c)
}

@MainActor final class SearchGate {
    var pending: [String: CheckedContinuation<[PlayAPI.PlayApp], Error>] = [:]
    func request(_ query: String) async throws -> [PlayAPI.PlayApp] {
        try await withCheckedThrowingContinuation { pending[query] = $0 }
    }
    func wait(_ query: String) async throws {
        for _ in 0..<200 {
            if pending[query] != nil { return }
            try await Task.sleep(nanoseconds: 1_000_000)
        }
        throw FixtureError.missingRequest
    }
    func finish(_ query: String, id: String) {
        pending.removeValue(forKey: query)!.resume(returning: [PlayAPI.PlayApp.from(doc: ProtoMessage(document(id)))!])
    }
}

@main struct StoreSearchTests {
    static let session = PlaySession(email: "fixture@example.invalid", gsfID: "fixture", bearer: "fixture-token")

    static func api() async throws {
        let first = "searchList?q=%D0%BA%D0%B0%D1%80%D1%82%D0%B0%20%26%3F&o=0&c=3&ksm=1&sb=0"
        let next = first + "&ctntkn=cursor%26part%3D2"
        let ids = ["org.example.target", "org.example.second"] + (0..<10).map { "org.example.app\($0)" }
        StubProtocol.configure { request in
            let url = request.url!, c = URLComponents(url: url, resolvingAgainstBaseURL: false)!
            let q = c.queryItems ?? []
            switch url.lastPathComponent {
            case "search":
                precondition(request.value(forHTTPHeaderField: "Authorization") == "Bearer fixture-token")
                var p = ProtoWriter(); p.message(5) { $0.string(10, first) }; return (200, response(p))
            case "searchList":
                precondition(q.first { $0.name == "q" }?.value == "карта &?")
                precondition(q.first { $0.name == "c" }?.value == "3")
                precondition(q.first { $0.name == "ksm" }?.value == "1")
                if let cursor = q.first(where: { $0.name == "ctntkn" }) {
                    precondition(cursor.value == "cursor&part=2")
                    return (200, page(Array(ids.dropFirst(2)), cursor: first))
                }
                return (200, page(Array(ids.prefix(2)), cursor: next))
            case "details":
                let id = q.first { $0.name == "doc" }!.value!
                precondition(ids.contains(id), "False protobuf candidate reached details: \(id)")
                return (200, details(id))
            default: throw FixtureError.unexpectedRequest
            }
        }
        let t = transport(); defer { t.invalidateAndCancel() }
        let apps = try await PlayAPI.search(query: "карта &?", session: session, transport: t)
        precondition(apps.map(\.id) == ids, "Search order, cursor or app-document filtering failed")
        precondition(StubProtocol.requests().filter { $0.lastPathComponent == "searchList" }.count == 2)
        precondition(StubProtocol.requests().allSatisfy { $0.host == "android.clients.google.com" })

        var cursors = ProtoWriter()
        cursors.string(10, first); cursors.string(10, first)
        cursors.string(11, "https://attacker.invalid/fdfe/" + next)
        cursors.string(12, "/fdfe/" + next)
        precondition(PlayAPI.searchContinuationPaths(in: ProtoMessage(cursors.data)) == [first, next])

        StubProtocol.configure { _ in (401, Data()) }
        do { _ = try await PlayAPI.search(query: "test", session: session, transport: t); preconditionFailure("401 was hidden") }
        catch let error as PlayError { precondition(error.statusCode == 401) }
        precondition(StubProtocol.requests().count == 1, "Expired auth triggered a misleading fallback")

        StubProtocol.configure { _ in throw URLError(.cancelled) }
        do { _ = try await PlayAPI.search(query: "test", session: session, transport: t); preconditionFailure("Cancellation was hidden") }
        catch is CancellationError {}
        precondition(StubProtocol.requests().count == 1)

        StubProtocol.configure { _ in (200, details("org.example.other")) }
        let mismatch = try await PlayAPI.details(packageName: "org.example.requested", session: session, transport: t)
        precondition(mismatch == nil, "A different app was returned as the requested package")

        StubProtocol.configure { request in
            if request.url!.host == "play.google.com" {
                let query = URLComponents(url: request.url!, resolvingAgainstBaseURL: false)!.queryItems!
                precondition(query.first { $0.name == "hl" }?.value == PlayDevice.locale.replacingOccurrences(of: "_", with: "-"))
                return (200, Data(#"<a href="/store/apps/details?id=org.example.web">App</a> tracking.example.invalid config.arm64_v8a"#.utf8))
            }
            if request.url!.lastPathComponent == "details" { return (200, details("org.example.web")) }
            return (503, Data())
        }
        let web = try await PlayAPI.search(query: "fallback", session: session, transport: t)
        precondition(web.map(\.id) == ["org.example.web"])
        print("PASS: complete cursors, bounded paging, ranking, document filtering, trusted URLs, auth, cancellation and web fallback")
    }

    @MainActor static func manager() async throws {
        let suite = "HuskStoreSearchTests." + UUID().uuidString
        let defaults = UserDefaults(suiteName: suite)!
        defer { defaults.removePersistentDomain(forName: suite) }
        let gate = SearchGate()
        let manager = PlayStoreManager(defaults: defaults, searchRequest: { query, _ in try await gate.request(query) })
        precondition(!manager.canSearch)
        await manager.search(query: "signed-out")
        precondition(gate.pending.isEmpty && !manager.isSearching && manager.searchResults.isEmpty)
        manager.saveSession(PlaySession(email: "fixture@example.invalid"))
        precondition(!manager.canSearch)
        await manager.search(query: "no-token")
        precondition(gate.pending.isEmpty)
        manager.saveSession(session)
        precondition(manager.canSearch)
        let a = Task { await manager.search(query: "old") }; try await gate.wait("old")
        let b = Task { await manager.search(query: "new") }; try await gate.wait("new")
        gate.finish("new", id: "org.example.new"); await b.value
        gate.finish("old", id: "org.example.old"); await a.value
        precondition(manager.searchResults.map(\.id) == ["org.example.new"] && !manager.isSearching)
        let clearing = Task { await manager.search(query: "clearing") }; try await gate.wait("clearing")
        await manager.search(query: "   ")
        gate.finish("clearing", id: "org.example.old"); await clearing.value
        precondition(manager.searchResults.isEmpty && !manager.isSearching)
        let logout = Task { await manager.search(query: "logout") }; try await gate.wait("logout")
        manager.signOut(); gate.finish("logout", id: "org.example.old"); await logout.value
        precondition(!manager.canSearch && manager.searchResults.isEmpty && !manager.isSearching)
        manager.saveSession(session)
        let cancelled = Task { await manager.search(query: "cancelled") }; try await gate.wait("cancelled")
        cancelled.cancel(); gate.finish("cancelled", id: "org.example.old"); await cancelled.value
        precondition(manager.searchResults.isEmpty && !manager.isSearching && manager.errorMessage == nil)
        let replaced = Task { await manager.search(query: "replaced") }; try await gate.wait("replaced")
        var other = session; other.email = "second@example.invalid"; manager.saveSession(other)
        gate.finish("replaced", id: "org.example.old"); await replaced.value
        precondition(manager.searchResults.isEmpty && !manager.isSearching)
        let failed = Task { await manager.search(query: "failed") }; try await gate.wait("failed")
        gate.pending.removeValue(forKey: "failed")!.resume(throwing: PlayError("Fixture failure")); await failed.value
        precondition(manager.errorMessage == "Fixture failure" && !manager.isSearching)
        print("PASS: signed-out/tokenless gating, latest-query wins, clearing, sign-out, cancellation, account replacement and errors")
    }

    static func main() async throws { try await api(); try await manager() }
}
