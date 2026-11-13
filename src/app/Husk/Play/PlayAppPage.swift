// SPDX-License-Identifier: GPL-2.0-or-later
// A Play app's page: what Play says about it (rich text, the numbers in short, screenshots to open full screen, what is new,
// the details) and how it runs on Husk.
import SwiftUI

struct PlayAppDetailSheet: View {
    @State private var app: PlayAPI.PlayApp
    @Environment(\.dismiss) private var dismiss
    @Environment(\.openURL) private var openURL
    @ObservedObject private var compat = CompatibilityStore.shared
    @ObservedObject private var manager = PlayStoreManager.shared
    @State private var viewing: Shot?

    init(app: PlayAPI.PlayApp) { _app = State(initialValue: app) }

    private struct Shot: Identifiable { let id: Int }
    private var playURL: URL { URL(string: "https://play.google.com/store/apps/details?id=\(app.id)")! }

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 26) {
                    header
                    installAction
                    stats
                    media
                    if !app.highlights.isEmpty { highlights }
                    about
                    if !app.whatsNew.isEmpty { whatsNew }
                    information
                }
                .padding(.horizontal, 20)
                .padding(.vertical, 16)
            }
            .background(Console.page.ignoresSafeArea())
            .navigationTitle(app.title)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button("Done") { dismiss() } }
                ToolbarItem(placement: .primaryAction) {
                    HuskShareLink(item: playURL, subject: Text(app.title), message: nil) { Image(systemName: "square.and.arrow.up") }
                }
            }
            .task { await refresh() }
            .fullScreenCover(item: $viewing) { shot in
                ScreenshotViewer(urls: app.screenshotURLs, start: shot.id)
            }
        }
    }

    /// Lists and the home shelf carry only part of a page: the whole of it from details.
    private func refresh() async {
        guard let s = manager.session, let full = try? await PlayAPI.details(packageName: app.id, session: s) else { return }
        app = full
    }

    // MARK: header

    private var header: some View {
        HStack(alignment: .top, spacing: 16) {
            AsyncImage(url: URL(string: app.iconURL ?? "")) { phase in
                if let image = phase.image { image.resizable().scaledToFill() } else { Console.raised }
            }
            .frame(width: 96, height: 96)
            .clipShape(RoundedRectangle(cornerRadius: 22, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 22, style: .continuous).stroke(Console.outline, lineWidth: 0.5))

            VStack(alignment: .leading, spacing: 5) {
                Text(app.title)
                    .font(.system(size: 22, weight: .bold))
                    .foregroundStyle(Console.text)
                    .lineLimit(3)
                    .fixedSize(horizontal: false, vertical: true)
                if !app.developer.isEmpty {
                    Text(app.developer).font(.system(size: 15, weight: .medium)).foregroundStyle(Theme.accent).lineLimit(1)
                }
                HStack(spacing: 8) {
                    if !app.award.isEmpty {
                        Label(app.award, systemImage: "rosette")
                            .font(.system(size: 12, weight: .semibold))
                            .foregroundStyle(Console.text)
                            .padding(.horizontal, 8).padding(.vertical, 4)
                            .background(Console.chip, in: Capsule())
                    }
                    if let status = compat.entry(app.id)?.status { CompatBadge(status: status) }
                }
                .padding(.top, 2)
            }
            Spacer(minLength: 0)
        }
    }

    // MARK: install

    private var installAction: some View {
        let progress = manager.downloadProgress[app.id]
        let downloading = manager.installingPackages.contains(app.id) || progress != nil
        return Group {
            if downloading {
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text(progress.map { "Downloading… \(Int($0 * 100))%" } ?? "Downloading…")
                            .font(.system(size: 14, weight: .semibold)).foregroundStyle(Console.text)
                        Spacer()
                        Button("Cancel", role: .destructive) { manager.cancelDownload(app.id) }
                            .font(.system(size: 14, weight: .semibold))
                    }
                    if let progress { ProgressView(value: progress).tint(Theme.accent) } else { ProgressView().frame(maxWidth: .infinity) }
                }
                .padding(14)
                .background(Console.card, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
            } else {
                Button {
                    manager.downloadAndInstall(app: app)
                    dismiss()
                } label: {
                    Text(app.isFree ? "Install" : app.formattedPrice)
                        .font(.system(size: 17, weight: .semibold))
                        .foregroundStyle(.white)
                        .frame(maxWidth: .infinity)
                        .frame(height: 48)
                        .background(Theme.accent, in: RoundedRectangle(cornerRadius: 14, style: .continuous))
                }
                .buttonStyle(CardButtonStyle())
            }
        }
    }

    // MARK: the numbers

    private var stats: some View {
        var items: [(value: String, label: LocalizedStringKey, icon: String?)] = []
        if app.rating > 0 {
            items.append((String(format: "%.1f", app.rating), app.reviews.isEmpty ? "Rating" : "\(app.reviews) reviews", "star.fill"))
        }
        if !app.installs.isEmpty { items.append((app.installs, "Downloads", nil)) }
        if !app.contentRating.isEmpty { items.append((app.contentRating, "Rated for", nil)) }
        if app.sizeBytes > 0 { items.append((ByteCountFormatter.string(fromByteCount: app.sizeBytes, countStyle: .file), "Size", nil)) }
        if !app.versionString.isEmpty { items.append((app.versionString, "Version", nil)) }
        // three to a row, the rest on the next one
        let columns = Array(repeating: GridItem(.flexible(), spacing: 8), count: min(3, max(1, items.count)))
        return LazyVGrid(columns: columns, spacing: 16) {
            ForEach(items.indices, id: \.self) { i in
                VStack(spacing: 4) {
                    HStack(spacing: 3) {
                        Text(items[i].value).font(.system(size: 16, weight: .bold)).foregroundStyle(Console.text)
                            .lineLimit(1).minimumScaleFactor(0.7)
                        if let icon = items[i].icon { Image(systemName: icon).font(.system(size: 11)).foregroundStyle(Console.text) }
                    }
                    Text(items[i].label).font(.system(size: 12)).foregroundStyle(Console.textDim)
                        .lineLimit(1).minimumScaleFactor(0.8)
                }
                .frame(maxWidth: .infinity)
            }
        }
        .padding(.vertical, 14)
        .padding(.horizontal, 8)
        .background(Console.card, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
        .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(Console.outline, lineWidth: 0.5))
    }

    // MARK: screenshots and the trailer

    @ViewBuilder
    private var media: some View {
        if !app.screenshotURLs.isEmpty || app.trailer != nil {
            // phones' portrait shots tall, landscape ones (most games) wide
            let landscape = (app.screenshotRatios.first ?? 0) > 1
            let height: CGFloat = landscape ? 190 : 300
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    if let trailer = app.trailer, let url = URL(string: trailer.url) {
                        Button { openURL(url) } label: {
                            ZStack {
                                AsyncImage(url: URL(string: trailer.thumbnail ?? "")) { phase in
                                    if let image = phase.image { image.resizable().scaledToFill() } else { Console.card }
                                }
                                Color.black.opacity(0.25)
                                Image(systemName: "play.circle.fill")
                                    .font(.system(size: 46))
                                    .symbolRenderingMode(.palette)
                                    .foregroundStyle(.white, .black.opacity(0.35))
                            }
                            .frame(width: height * 16 / 9, height: height)
                            .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
                        }
                        .buttonStyle(CardButtonStyle())
                    }
                    ForEach(app.screenshotURLs.indices, id: \.self) { i in
                        let ratio = i < app.screenshotRatios.count && app.screenshotRatios[i] > 0
                            ? app.screenshotRatios[i] : (landscape ? 16.0 / 9 : 9.0 / 16)
                        Button { viewing = Shot(id: i) } label: {
                            AsyncImage(url: PlayImages.sized(app.screenshotURLs[i], height: Int(height * 2))) { phase in
                                if let image = phase.image { image.resizable().scaledToFill() } else { Console.card }
                            }
                            .frame(width: height * CGFloat(ratio), height: height)
                            .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
                            .overlay(RoundedRectangle(cornerRadius: 14, style: .continuous).stroke(Console.outline, lineWidth: 0.5))
                        }
                        .buttonStyle(CardButtonStyle())
                    }
                }
                .padding(.horizontal, 20)
            }
            .padding(.horizontal, -20)
        }
    }

    // MARK: text

    private var highlights: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Highlights")
            VStack(alignment: .leading, spacing: 10) {
                ForEach(app.highlights, id: \.self) { line in
                    HStack(alignment: .top, spacing: 10) {
                        Image(systemName: "sparkles").font(.system(size: 13, weight: .semibold)).foregroundStyle(Theme.accent).padding(.top, 2)
                        Text(line).font(.system(size: 15)).foregroundStyle(Console.text.opacity(0.9))
                            .fixedSize(horizontal: false, vertical: true)
                    }
                }
            }
            .padding(14)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(Console.card, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
        }
    }

    private var about: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle(app.isGame ? "About this game" : "About this app")
            if !app.tagline.isEmpty {
                Text(app.tagline).font(.system(size: 15, weight: .semibold)).foregroundStyle(Console.text)
                    .fixedSize(horizontal: false, vertical: true)
            }
            if !app.summary.isEmpty {
                ExpandableText(text: PlayText.attributed(app.summary), lines: 8)
            }
            if !app.tags.isEmpty {
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(app.tags, id: \.self) { tag in
                            Text(tag).font(.system(size: 13, weight: .medium)).foregroundStyle(Console.text)
                                .padding(.horizontal, 12).frame(height: 30)
                                .overlay(Capsule().stroke(Console.outline, lineWidth: 1))
                        }
                    }
                    .padding(.horizontal, 20)
                }
                .padding(.horizontal, -20)
                .padding(.top, 4)
            }
        }
    }

    private var whatsNew: some View {
        VStack(alignment: .leading, spacing: 10) {
            HStack(alignment: .firstTextBaseline) {
                sectionTitle("What's new")
                Spacer()
                if !app.updated.isEmpty { Text(app.updated).font(.system(size: 13)).foregroundStyle(Console.textDim) }
            }
            ExpandableText(text: PlayText.attributed(app.whatsNew), lines: 5)
        }
    }

    // MARK: details

    private var information: some View {
        VStack(alignment: .leading, spacing: 10) {
            sectionTitle("Information")
            VStack(spacing: 0) {
                infoRow("Developer", app.developer)
                infoRow("Updated", app.updated)
                infoRow("Released", app.released)
                infoRow("Version", app.versionString)
                if app.sizeBytes > 0 { infoRow("Size", ByteCountFormatter.string(fromByteCount: app.sizeBytes, countStyle: .file)) }
                infoRow("Downloads", app.installs)
                infoRow("Rated for", app.contentRating)
                infoRow("Ads", app.ads)
                infoRow("In-app purchases", app.purchases)
                infoRow("Package", app.id, mono: true)
                linkRow("Website", "safari", URL(string: app.website))
                linkRow("Email", "envelope", app.email.isEmpty ? nil : URL(string: "mailto:\(app.email)"))
                linkRow("Privacy policy", "hand.raised", URL(string: app.privacy))
                linkRow("View on Google Play", "arrow.up.right.square", playURL, last: true)
            }
            .background(Console.card, in: RoundedRectangle(cornerRadius: 16, style: .continuous))
            .overlay(RoundedRectangle(cornerRadius: 16, style: .continuous).stroke(Console.outline, lineWidth: 0.5))
        }
    }

    @ViewBuilder
    private func infoRow(_ title: LocalizedStringKey, _ value: String, mono: Bool = false) -> some View {
        if !value.isEmpty {
            HStack(alignment: .firstTextBaseline, spacing: 12) {
                Text(title).font(.system(size: 14)).foregroundStyle(Console.textDim)
                Spacer(minLength: 12)
                Text(value)
                    .font(mono ? .system(size: 13, design: .monospaced) : .system(size: 14, weight: .medium))
                    .foregroundStyle(Console.text)
                    .multilineTextAlignment(.trailing)
                    .textSelection(.enabled)
            }
            .padding(.horizontal, 14).padding(.vertical, 12)
            .overlay(alignment: .bottom) { Rectangle().fill(Console.hairline).frame(height: 1).padding(.leading, 14) }
        }
    }

    @ViewBuilder
    private func linkRow(_ title: LocalizedStringKey, _ icon: String, _ url: URL?, last: Bool = false) -> some View {
        if let url, url.scheme != nil {
            Button { openURL(url) } label: {
                HStack(spacing: 12) {
                    Image(systemName: icon).font(.system(size: 15)).foregroundStyle(Theme.accent).frame(width: 22)
                    Text(title).font(.system(size: 14, weight: .medium)).foregroundStyle(Console.text)
                    Spacer()
                    Image(systemName: "chevron.right").font(.system(size: 12, weight: .bold)).foregroundStyle(Console.textDim)
                }
                .padding(.horizontal, 14).padding(.vertical, 13)
                .contentShape(Rectangle())
            }
            .buttonStyle(.plain)
            .overlay(alignment: .bottom) {
                if !last { Rectangle().fill(Console.hairline).frame(height: 1).padding(.leading, 48) }
            }
        }
    }

    private func sectionTitle(_ title: LocalizedStringKey) -> some View {
        Text(title).font(.system(size: 20, weight: .bold)).foregroundStyle(Console.text)
    }
}

/// Play's images come in any size: the same picture at the height asked for.
enum PlayImages {
    static func sized(_ url: String, height: Int) -> URL? {
        guard url.contains("googleusercontent.com"), !url.contains("=") else { return URL(string: url) }
        return URL(string: url + "=h\(height)")
    }
}

/// Text cut to a few lines, with More to read the rest.
private struct ExpandableText: View {
    let text: AttributedString
    let lines: Int
    @State private var expanded = false

    /// Roughly: more lines than shown, by its line breaks and length.
    private var long: Bool {
        let s = String(text.characters)
        let wrapped = s.split(separator: "\n", omittingEmptySubsequences: false).reduce(0) { $0 + max(1, ($1.count + 44) / 45) }
        return wrapped > lines
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(text)
                .font(.system(size: 15))
                .foregroundStyle(Console.text.opacity(0.85))
                .tint(Theme.accent)
                .lineSpacing(3)
                .lineLimit(expanded ? nil : lines)
                .fixedSize(horizontal: false, vertical: true)
            if long {
                Button(expanded ? "Less" : "More") { withAnimation(.easeInOut(duration: 0.2)) { expanded.toggle() } }
                    .font(.system(size: 15, weight: .semibold))
                    .foregroundStyle(Theme.accent)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

/// The screenshots full screen: swipe between them, pinch or double-tap to zoom.
private struct ScreenshotViewer: View {
    let urls: [String]
    @State var start: Int
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ZStack(alignment: .topTrailing) {
            Color.black.ignoresSafeArea()
            TabView(selection: $start) {
                ForEach(urls.indices, id: \.self) { i in
                    ZoomableShot(url: PlayImages.sized(urls[i], height: 1600)).tag(i)
                }
            }
            .tabViewStyle(.page(indexDisplayMode: urls.count > 1 ? .always : .never))
            .ignoresSafeArea()

            Button { dismiss() } label: {
                Image(systemName: "xmark")
                    .font(.system(size: 15, weight: .bold))
                    .foregroundStyle(.white)
                    .frame(width: 36, height: 36)
                    .background(.ultraThinMaterial, in: Circle())
            }
            .padding(16)
        }
        .appStatusBar(for: nil)
    }
}

private struct ZoomableShot: View {
    let url: URL?
    @State private var scale: CGFloat = 1
    @State private var settled: CGFloat = 1
    @State private var offset: CGSize = .zero
    @State private var dragged: CGSize = .zero

    var body: some View {
        AsyncImage(url: url) { phase in
            if let image = phase.image {
                image.resizable().scaledToFit()
            } else if phase.error != nil {
                Image(systemName: "photo").font(.system(size: 40)).foregroundStyle(.white.opacity(0.5))
            } else {
                ProgressView().tint(.white)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .scaleEffect(scale)
        .offset(offset)
        .gesture(MagnificationGesture()
            .onChanged { scale = max(1, settled * $0) }
            .onEnded { _ in
                settled = scale
                if scale <= 1 { withAnimation(.spring()) { offset = .zero; dragged = .zero } }
            })
        // panning only while zoomed in, so swiping still pages
        .simultaneousGesture(scale > 1 ? DragGesture()
            .onChanged { offset = CGSize(width: dragged.width + $0.translation.width, height: dragged.height + $0.translation.height) }
            .onEnded { _ in dragged = offset } : nil)
        .onTapGesture(count: 2) {
            withAnimation(.spring()) {
                if scale > 1 { scale = 1; offset = .zero; dragged = .zero } else { scale = 2.5 }
                settled = scale
            }
        }
    }
}
