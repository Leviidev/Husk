// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UniformTypeIdentifiers

/// The guest's storage, browsable.
///
/// Husk could already push a file into Android's Download folder and never show
/// you what happened to it, which is a one-way street: you cannot check a game
/// found its data pack, or get a screenshot back, or see why an install failed
/// on a file that is not where you think it is. Everything here is `stat` and
/// `find` over the same bridge the rest of the app uses — no agent, no adb.
struct FilesTab: View {
    static let root = "/sdcard"

    @ObservedObject private var host = AndroidHost.shared
    @ObservedObject private var router = Router.shared

    var body: some View {
        NavigationStack(path: $router.files) {
            DirectoryView(path: Self.root, title: "Files")
                .navigationDestination(for: String.self) { path in
                    DirectoryView(path: path,
                                  title: (path as NSString).lastPathComponent)
                }
        }
    }
}

/// One directory.
struct DirectoryView: View {
    let path: String
    let title: String

    @ObservedObject private var host = AndroidHost.shared
    @State private var entries: [AndroidHost.GuestEntry] = []
    @State private var space: (free: Int64, total: Int64)?
    @State private var loading = true
    @State private var failure: String?
    @State private var importing = false
    @State private var showImportSheet = false
    @State private var installing: AndroidHost.GuestEntry?

    var body: some View {
        ZStack {
            Theme.backdrop
            content
        }
        .navigationTitle(title)
        .toolbar {
            ToolbarItem(placement: .navigationBarTrailing) {
                HStack(spacing: 10) {
                    CircleButton(systemImage: "arrow.clockwise") { load() }
                    CircleButton(systemImage: "plus") { showImportSheet = true }
                }
            }
        }
        .sheet(isPresented: $showImportSheet) {
            ImportSheet(destination: path) { showImportSheet = false; importing = true }
                .presentationDetents([.height(320)])
        }
        .fileImporter(isPresented: $importing, allowedContentTypes: [.item],
                      allowsMultipleSelection: true) { result in
            if case .success(let urls) = result, !urls.isEmpty {
                host.sendFiles(urls, to: path)
            }
        }
        .confirmationDialog("Install \(installing?.name ?? "")?",
                            isPresented: Binding(get: { installing != nil },
                                                 set: { if !$0 { installing = nil } }),
                            titleVisibility: .visible) {
            Button("Install") {
                if let apk = installing { host.installFromGuest(apk.path, name: apk.name) }
                installing = nil
            }
            Button("Cancel", role: .cancel) { installing = nil }
        } message: {
            Text("Android installs it from where it already is — nothing is copied.")
        }
        .task(id: path) { load() }
        .refreshable { load() }
    }

    @ViewBuilder private var content: some View {
        ScrollView {
            VStack(spacing: 14) {
                if let space { storage(space) }

                if loading && entries.isEmpty {
                    ProgressView().tint(Theme.accent).padding(.top, 60)
                } else if let failure {
                    EmptyState(title: "Cannot read this folder",
                               message: failure, systemImage: "lock")
                } else if entries.isEmpty {
                    EmptyState(title: "Empty",
                               message: "Nothing is in this folder yet.",
                               systemImage: "folder",
                               actionTitle: "Import files",
                               action: { showImportSheet = true })
                } else {
                    RowGroup {
                        ForEach(Array(entries.enumerated()), id: \.element.id) { i, e in
                            row(e)
                            if i < entries.count - 1 { RowDivider() }
                        }
                    }
                }
            }
            .padding(.horizontal, 18)
            .padding(.top, 6)
            .padding(.bottom, 28)
        }
    }

    @ViewBuilder private func row(_ e: AndroidHost.GuestEntry) -> some View {
        if e.isDirectory {
            NavigationLink(value: e.path) {
                HuskRow(systemImage: "folder.fill", title: e.name,
                        subtitle: e.modified.map(Self.when))
            }
            .buttonStyle(.plain)
        } else {
            Button {
                if e.name.lowercased().hasSuffix(".apk") { installing = e }
            } label: {
                HuskRow(systemImage: icon(for: e.name), title: e.name,
                        subtitle: subtitle(e),
                        showsChevron: e.name.lowercased().hasSuffix(".apk"))
            }
            .buttonStyle(.plain)
        }
    }

    private func storage(_ s: (free: Int64, total: Int64)) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("Storage")
                    .font(.system(size: 13, weight: .medium))
                    .foregroundStyle(Theme.text)
                Spacer()
                Text("\(AppDetailView.bytes(s.total - s.free)) of "
                   + "\(AppDetailView.bytes(s.total))")
                    .font(.system(size: 12))
                    .foregroundStyle(Theme.textDim)
            }
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(Theme.surfaceHigh)
                    Capsule().fill(Theme.accent)
                        .frame(width: geo.size.width * used(s))
                }
            }
            .frame(height: 5)
        }
        .padding(.horizontal, 14).padding(.vertical, 13)
        .huskCard(RoundedRectangle(cornerRadius: Theme.rowCorner, style: .continuous))
    }

    private func used(_ s: (free: Int64, total: Int64)) -> CGFloat {
        guard s.total > 0 else { return 0 }
        return min(max(CGFloat(s.total - s.free) / CGFloat(s.total), 0.02), 1)
    }

    private func subtitle(_ e: AndroidHost.GuestEntry) -> String {
        let size = AppDetailView.bytes(e.size)
        guard let m = e.modified else { return size }
        return "\(size) · \(Self.when(m))"
    }

    private func icon(for name: String) -> String {
        let n = name.lowercased()
        if n.hasSuffix(".apk") { return "shippingbox.fill" }
        if n.hasSuffix(".png") || n.hasSuffix(".jpg") || n.hasSuffix(".jpeg")
            || n.hasSuffix(".webp") { return "photo" }
        if n.hasSuffix(".mp4") || n.hasSuffix(".mkv") { return "film" }
        if n.hasSuffix(".mp3") || n.hasSuffix(".ogg") || n.hasSuffix(".wav") {
            return "music.note"
        }
        if n.hasSuffix(".zip") || n.hasSuffix(".obb") { return "archivebox" }
        if n.hasSuffix(".txt") || n.hasSuffix(".log") || n.hasSuffix(".json") {
            return "doc.text"
        }
        return "doc"
    }

    static func when(_ d: Date) -> String {
        let f = DateFormatter()
        f.dateStyle = .short
        f.timeStyle = Calendar.current.isDateInToday(d) ? .short : .none
        return f.string(from: d)
    }

    private func load() {
        loading = true
        let where_ = path
        Task.detached {
            var rows: [AndroidHost.GuestEntry] = []
            var why: String?
            do { rows = try AndroidHost.shared.list(where_) }
            catch { why = error.localizedDescription }
            let s = AndroidHost.shared.freeSpace(at: where_)
            await MainActor.run {
                entries = rows
                space = s
                failure = rows.isEmpty ? why : nil
                loading = false
            }
        }
    }
}

/// The import screen from the concept: one target, one button.
struct ImportSheet: View {
    let destination: String
    let onBrowse: () -> Void

    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ZStack {
            Theme.backdrop
            VStack(spacing: 18) {
                HStack {
                    Spacer()
                    Button { dismiss() } label: {
                        Image(systemName: "xmark")
                            .font(.system(size: 13, weight: .semibold))
                            .foregroundStyle(Theme.textDim)
                            .frame(width: 30, height: 30)
                            .background(Theme.surfaceHigh, in: Circle())
                    }
                    .buttonStyle(.plain)
                }

                VStack(spacing: 10) {
                    Image(systemName: "doc.badge.plus")
                        .font(.system(size: 30, weight: .light))
                        .foregroundStyle(Theme.textDim)
                    Text("Tap to import")
                        .font(.system(size: 15, weight: .medium))
                        .foregroundStyle(Theme.text)
                    Text("Goes to \((destination as NSString).lastPathComponent). "
                       + "Unmodified APKs install from here too.")
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.textDim)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 20)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 30)
                .background(
                    RoundedRectangle(cornerRadius: Theme.cardCorner, style: .continuous)
                        .strokeBorder(style: StrokeStyle(lineWidth: 1, dash: [6, 5]))
                        .foregroundStyle(Theme.hairline))
                .contentShape(Rectangle())
                .onTapGesture { onBrowse() }

                Button("Browse files", action: onBrowse)
                    .buttonStyle(PrimaryButtonStyle())
                Spacer(minLength: 0)
            }
            .padding(.horizontal, 20)
            .padding(.top, 14)
        }
    }
}
