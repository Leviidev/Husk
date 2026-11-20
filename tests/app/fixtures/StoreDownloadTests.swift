import Foundation

final class DownloadProtocol: URLProtocol {
    enum Mode { case known, responseSize, unknown, failure, cancellation }
    static var mode = Mode.known
    private let lock = NSLock()
    private var stopped = false
    override class func canInit(with request: URLRequest) -> Bool { true }
    override class func canonicalRequest(for request: URLRequest) -> URLRequest { request }
    override func stopLoading() { lock.lock(); stopped = true; lock.unlock() }
    private var isStopped: Bool { lock.lock(); defer { lock.unlock() }; return stopped }

    override func startLoading() {
        let mode = Self.mode
        let path = request.url!.lastPathComponent
        if path == "purchase" || path == "delivery" {
            var wrapper = ProtoWriter()
            wrapper.message(1) { p in
                if path == "delivery" {
                    p.message(21) { response in response.int(1, 1); response.message(2) { data in
                        data.string(3, "https://files.example.invalid/base")
                        if mode == .known { data.int(1, 100_000) }
                        if mode == .known || mode == .failure {
                            data.message(15) { $0.string(1, "config.arm64_v8a"); $0.int(2, 40_000); $0.string(5, "https://files.example.invalid/split") }
                            data.message(4) { $0.int(1, 0); $0.int(2, 42); $0.int(3, 60_000); $0.string(4, "https://files.example.invalid/obb") }
                        }
                    } }
                }
            }
            client?.urlProtocol(self, didReceive: HTTPURLResponse(url: request.url!, statusCode: 200, httpVersion: nil, headerFields: nil)!, cacheStoragePolicy: .notAllowed)
            client?.urlProtocol(self, didLoad: wrapper.data)
            client?.urlProtocolDidFinishLoading(self)
            return
        }
        let size = path == "base" ? 100_000 : path == "split" ? 40_000 : 60_000
        let headers = mode == .unknown ? [:] : ["Content-Length": String(size)]
        let status = mode == .failure && path == "split" ? 503 : 200
        client?.urlProtocol(self, didReceive: HTTPURLResponse(url: request.url!, statusCode: status, httpVersion: nil, headerFields: headers)!, cacheStoragePolicy: .notAllowed)
        guard status == 200 else { client?.urlProtocolDidFinishLoading(self); return }
        DispatchQueue.global().async {
            for _ in 0..<5 {
                Thread.sleep(forTimeInterval: 0.03)
                guard !self.isStopped else { return }
                self.client?.urlProtocol(self, didLoad: Data(repeating: UInt8(size / 1000), count: size / 5))
            }
            if !self.isStopped { self.client?.urlProtocolDidFinishLoading(self) }
        }
    }
}

@main struct StoreDownloadTests {
    static let session = PlaySession(email: "fixture@example.invalid", gsfID: "fixture", bearer: "fixture-token")
    @MainActor static func main() async throws {
        let root = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
        try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: root) }
        let config = URLSessionConfiguration.ephemeral
        config.protocolClasses = [DownloadProtocol.self]
        let transport = URLSession(configuration: config)
        defer { transport.invalidateAndCancel() }
        var progress: [PlayAPI.DownloadProgress] = []
        func download() async throws -> [URL] {
            try await PlayAPI.downloadApp(packageName: "org.example.test", versionCode: 42, session: session,
                                          stagingDirectory: root, transport: transport) { progress.append($0) }
        }
        DownloadProtocol.mode = .known
        let files = try await download()
        precondition(files.count == 3 && files.last!.pathExtension == "obb")
        let sizes = try files.map { try Data(contentsOf: $0).count }
        precondition(sizes == [100_000, 40_000, 60_000])
        precondition(progress.contains { $0.received > 0 && $0.received < 100_000 && ($0.fraction ?? 1) < 0.5 }, "No progress while the base APK was downloading")
        precondition(progress.contains { $0.received > 100_000 && $0.received < 140_000 }, "Split progress missing")
        precondition(progress.contains { $0.received > 140_000 && $0.received < 200_000 }, "OBB progress missing")
        precondition(progress.last?.received == 200_000 && progress.last?.fraction == 1)
        precondition(zip(progress, progress.dropFirst()).allSatisfy { $0.received <= $1.received })
        try FileManager.default.removeItem(at: files[0].deletingLastPathComponent())

        progress = []; DownloadProtocol.mode = .responseSize
        let responseSized = try await download()
        precondition(progress.contains { $0.received > 0 && $0.received < 100_000 && $0.total == 100_000 }, "Content-Length was not used")
        try FileManager.default.removeItem(at: responseSized[0].deletingLastPathComponent())
        progress = []; DownloadProtocol.mode = .unknown
        let unknown = try await download()
        precondition(progress.contains { $0.received > 0 && $0.received < 100_000 && $0.fraction == nil }, "Unknown length invented a percentage")
        precondition(progress.last?.fraction == 1)
        try FileManager.default.removeItem(at: unknown[0].deletingLastPathComponent())

        progress = []; DownloadProtocol.mode = .failure
        do { _ = try await download(); preconditionFailure("HTTP failure was ignored") } catch is PlayError {}
        let afterFailure = try FileManager.default.contentsOfDirectory(atPath: root.path)
        precondition(afterFailure.isEmpty, "Failed partial set left behind")
        precondition(progress.last?.fraction != 1)

        progress = []; DownloadProtocol.mode = .cancellation
        let task = Task { try await download() }
        try await Task.sleep(nanoseconds: 70_000_000)
        task.cancel()
        do { _ = try await task.value; preconditionFailure("Cancelled download completed") } catch {}
        let afterCancel = try FileManager.default.contentsOfDirectory(atPath: root.path)
        precondition(afterCancel.isEmpty, "Cancelled partial set left behind")
        precondition(progress.last?.fraction != 1)
        print("PASS: live byte progress for base/splits/OBB, response lengths, unknown totals, cancellation and partial-file cleanup")
    }
}
