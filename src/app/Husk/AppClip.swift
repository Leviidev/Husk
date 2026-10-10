// SPDX-License-Identifier: GPL-2.0-or-later
import Network
import SafariServices
import SwiftUI
import UIKit

/// "Generate App Clip": a home-screen icon for one game that opens it straight from the home screen.
///
/// iOS puts an icon on the home screen from a configuration profile's Web Clip payload. Its URL here is
/// husk://open_app?unique_identifier=<id>, which brings Husk up on that game and starts it. The profile is made here, served from
/// a small HTTP server on this iPhone (127.0.0.1) and opened in Safari's in-app view, which hands it to Settings to install.
@MainActor
enum AppClip {
    static let scheme = "husk"
    static let host = "open_app"

    /// The link the clip opens.
    static func link(for id: String) -> URL {
        var c = URLComponents()
        c.scheme = scheme
        c.host = host
        c.queryItems = [URLQueryItem(name: "unique_identifier", value: id)]
        return c.url!
    }

    /// husk://open_app?unique_identifier=<id>: open that game's page and start it. True when the URL was one.
    @discardableResult
    static func handle(_ url: URL) -> Bool {
        guard url.scheme?.lowercased() == scheme, url.host?.lowercased() == host,
              let id = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?.first(where: { $0.name == "unique_identifier" })?.value,
              !id.isEmpty else { return false }
        HuskLog.log("ui", "app clip: opening \(id)")
        let r = Router.shared
        r.tab = .library
        r.library = [.game(id)]
        r.autoPlay = id
        return true
    }

    /// The profile: one Web Clip with the game's name and icon.
    static func profile(for app: TLApp) -> Data {
        let clip = UUID().uuidString, outer = UUID().uuidString
        var payload: [String: Any] = [
            "PayloadType": "com.apple.webClip.managed",
            "PayloadVersion": 1,
            "PayloadIdentifier": "app.husk.clip.\(safe(app.id)).webclip",
            "PayloadUUID": clip,
            "PayloadDisplayName": app.label,
            "Label": app.label,
            "URL": link(for: app.id).absoluteString,
            "IsRemovable": true,
            "FullScreen": false,
            "Precomposed": true,
            "IgnoreManifestScope": true,
        ]
        if let icon = iconPNG(app) { payload["Icon"] = icon }
        let plist: [String: Any] = [
            "PayloadType": "Configuration",
            "PayloadVersion": 1,
            "PayloadIdentifier": "app.husk.clip.\(safe(app.id))",
            "PayloadUUID": outer,
            "PayloadDisplayName": "\(app.label) (Husk)",
            "PayloadDescription": "Puts \(app.label) on your Home Screen. It opens in Husk.",
            "PayloadOrganization": "Husk",
            "PayloadRemovalDisallowed": false,
            "PayloadContent": [payload],
        ]
        return (try? PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)) ?? Data()
    }

    private static func safe(_ s: String) -> String { String(s.map { $0.isLetter || $0.isNumber || $0 == "." ? $0 : "-" }) }

    /// The game's icon as a 180-point square PNG (the size iOS draws home-screen icons from).
    private static func iconPNG(_ app: TLApp) -> Data? {
        guard let path = app.iconPath, let image = UIImage(contentsOfFile: path) else { return nil }
        let side: CGFloat = 180
        let format = UIGraphicsImageRendererFormat()
        format.scale = 1
        format.opaque = true
        return UIGraphicsImageRenderer(size: CGSize(width: side, height: side), format: format).pngData { ctx in
            UIColor.black.setFill()
            ctx.fill(CGRect(x: 0, y: 0, width: side, height: side))
            let s = image.size, k = max(side / max(s.width, 1), side / max(s.height, 1))
            let w = s.width * k, h = s.height * k
            image.draw(in: CGRect(x: (side - w) / 2, y: (side - h) / 2, width: w, height: h))
        }
    }

    /// Serve the profile and open it. The server stays up until the sheet closes.
    static func generate(for app: TLApp) {
        let data = profile(for: app)
        let name = "\(safe(app.label)).mobileconfig"
        let server = ProfileServer(body: data, name: name)
        server.start { url in
            guard let url else {
                HuskLog.log("ui", "app clip: could not start the local server")
                return
            }
            HuskLog.log("ui", "app clip for \(app.id): serving \(url.absoluteString)")
            let safari = SFSafariViewController(url: url)
            safari.dismissButtonStyle = .done
            safari.presentationController?.delegate = server
            server.retainUntilDismissed(safari)
            topController()?.present(safari, animated: true)
        }
    }

    private static func topController() -> UIViewController? {
        var top = UIApplication.shared.connectedScenes.compactMap { ($0 as? UIWindowScene)?.keyWindow }.first?.rootViewController
        while let next = top?.presentedViewController { top = next }
        return top
    }
}

/// A one-file HTTP server on 127.0.0.1: GET anything answers the profile, as iOS wants it served to install it.
final class ProfileServer: NSObject, UIAdaptivePresentationControllerDelegate, SFSafariViewControllerDelegate {
    private let body: Data
    private let name: String
    private var listener: NWListener?
    private static var live: [ProfileServer] = []
    private weak var safari: SFSafariViewController?

    init(body: Data, name: String) { self.body = body; self.name = name }

    func start(_ ready: @escaping (URL?) -> Void) {
        let params = NWParameters.tcp
        params.requiredLocalEndpoint = NWEndpoint.hostPort(host: "127.0.0.1", port: .any)
        guard let l = try? NWListener(using: params) else { ready(nil); return }
        listener = l
        Self.live.append(self)
        l.newConnectionHandler = { [weak self] c in self?.serve(c) }
        l.stateUpdateHandler = { [weak self] state in
            switch state {
            case .ready:
                let port = l.port?.rawValue ?? 0
                let encoded = self?.name.addingPercentEncoding(withAllowedCharacters: .urlPathAllowed) ?? "profile.mobileconfig"
                DispatchQueue.main.async { ready(URL(string: "http://127.0.0.1:\(port)/\(encoded)")) }
            case .failed:
                DispatchQueue.main.async { ready(nil) }
                self?.stop()
            default: break
            }
        }
        l.start(queue: .global(qos: .userInitiated))
        // nobody installs a profile after ten minutes: stop anyway
        DispatchQueue.main.asyncAfter(deadline: .now() + 600) { [weak self] in self?.stop() }
    }

    private func serve(_ c: NWConnection) {
        c.start(queue: .global(qos: .userInitiated))
        c.receive(minimumIncompleteLength: 1, maximumLength: 16384) { [weak self] _, _, _, _ in
            guard let self else { c.cancel(); return }
            var head = "HTTP/1.1 200 OK\r\n"
            head += "Content-Type: application/x-apple-aspen-config\r\n"
            head += "Content-Disposition: attachment; filename=\"\(self.name)\"\r\n"
            head += "Content-Length: \(self.body.count)\r\n"
            head += "Cache-Control: no-store\r\nConnection: close\r\n\r\n"
            var out = Data(head.utf8)
            out.append(self.body)
            c.send(content: out, completion: .contentProcessed { _ in c.cancel() })
        }
    }

    func retainUntilDismissed(_ s: SFSafariViewController) { safari = s; s.delegate = self }
    func safariViewControllerDidFinish(_ controller: SFSafariViewController) { stop() }
    func presentationControllerDidDismiss(_ presentationController: UIPresentationController) { stop() }

    func stop() {
        listener?.cancel()
        listener = nil
        DispatchQueue.main.async { Self.live.removeAll { $0 === self } }
    }
}
