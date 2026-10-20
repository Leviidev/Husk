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
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack(path: $router.files) {
            DirectoryView(path: Self.root, title: L10n.tr("Files"))
                .huskDestination(for: String.self) { path in
                    DirectoryView(path: path,
                                  title: (path as NSString).lastPathComponent)
                }
                .toolbar {
                    ToolbarItem(placement: .cancellationAction) { Button(L10n.tr("Done")) { dismiss() } }
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
        List {
            if let space {
                Section { storage(space) }
            }
            if !entries.isEmpty {
                Section {
                    ForEach(entries, id: \.id) { e in row(e) }
                } footer: {
                    if path != FilesTab.root {
                        Text(path).font(.footnote).lineLimit(1).truncationMode(.head)
                    }
                }
            }
        }
        .listStyle(.insetGrouped)
        .overlay {
            if loading && entries.isEmpty {
                ProgressView()
            } else if let failure {
                EmptyState(title: L10n.tr("Cannot Read This Folder"), message: failure, systemImage: "lock")
            } else if entries.isEmpty {
                EmptyState(title: L10n.tr("Empty"), message: L10n.tr("Nothing is in this folder yet."),
                           systemImage: "folder", actionTitle: L10n.tr("Import Files"),
                           action: { showImportSheet = true })
            }
        }
        .navigationTitle(title)
        .navigationBarTitleDisplayMode(path == FilesTab.root ? .large : .inline)
        .toolbar {
            ToolbarItemGroup(placement: .topBarTrailing) {
                Button { load() } label: { Label(L10n.tr("Refresh"), systemImage: "arrow.clockwise") }
                Button { showImportSheet = true } label: { Label(L10n.tr("Import"), systemImage: "plus") }
            }
        }
        .sheet(isPresented: $showImportSheet) {
            ImportSheet(destination: path) { showImportSheet = false; importing = true }
                .huskDetents([.height(340)])
        }
        .huskFilePicker(isPresented: $importing) { urls in
            host.sendFiles(urls, to: path)
        }
        .confirmationDialog(L10n.tr("Install \(installing?.name ?? "")?"),
                            isPresented: Binding(get: { installing != nil },
                                                 set: { if !$0 { installing = nil } }),
                            titleVisibility: .visible) {
            Button(L10n.tr("Install")) {
                if let apk = installing { host.installFromGuest(apk.path, name: apk.name) }
                installing = nil
            }
            Button(L10n.tr("Cancel"), role: .cancel) { installing = nil }
        } message: {
            Text(L10n.tr("Android installs it from where it already is — nothing is copied."))
        }
        .task(id: path) { load() }
        .refreshable { load() }
    }

    @ViewBuilder private func row(_ e: AndroidHost.GuestEntry) -> some View {
        if e.isDirectory {
            HuskLink(value: e.path) {
                fileLabel(icon: "folder.fill", tint: .accentColor, title: e.name,
                          subtitle: e.modified.map(Self.when))
            }
        } else {
            Button {
                if e.name.lowercased().hasSuffix(".apk") { installing = e }
            } label: {
                HStack {
                    fileLabel(icon: icon(for: e.name), tint: .secondary, title: e.name, subtitle: subtitle(e))
                    if e.name.lowercased().hasSuffix(".apk") {
                        Spacer()
                        Image(systemName: "arrow.down.circle").foregroundStyle(Color.accentColor)
                    }
                }
            }
            .foregroundStyle(.primary)
        }
    }

    private func fileLabel(icon: String, tint: Color, title: String, subtitle: String?) -> some View {
        HStack(spacing: 12) {
            Image(systemName: icon)
                .font(.title3)
                .foregroundStyle(tint)
                .frame(width: 28)
            VStack(alignment: .leading, spacing: 2) {
                Text(title).lineLimit(1).truncationMode(.middle)
                if let subtitle {
                    Text(subtitle).font(.caption).foregroundStyle(.secondary).lineLimit(1)
                }
            }
        }
    }

    private func storage(_ s: (free: Int64, total: Int64)) -> some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(L10n.tr("Storage"))
                Spacer()
                Text(L10n.tr("\(AppDetailView.bytes(s.total - s.free)) of \(AppDetailView.bytes(s.total))"))
                    .font(.subheadline)
                    .foregroundStyle(.secondary)
            }
            ProgressView(value: Double(used(s)))
        }
        .padding(.vertical, 4)
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

/// The import screen: one target, one button.
struct ImportSheet: View {
    let destination: String
    let onBrowse: () -> Void

    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            VStack(spacing: 18) {
                VStack(spacing: 10) {
                    Image(systemName: "doc.badge.plus")
                        .font(.system(size: 34, weight: .light))
                        .foregroundStyle(Color.accentColor)
                    Text(L10n.tr("Import Files")).font(.headline)
                    Text(L10n.tr("They go to \((destination as NSString).lastPathComponent). Unmodified APKs install from here too."))
                        .font(.subheadline)
                        .foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 24)
                .huskCard()

                Button(L10n.tr("Browse Files"), action: onBrowse)
                    .buttonStyle(.borderedProminent)
                    .controlSize(.large)
                Spacer(minLength: 0)
            }
            .padding(16)
            .background(Theme.backdrop)
            .navigationTitle(L10n.tr("Import"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) { Button(L10n.tr("Done")) { dismiss() } }
            }
        }
    }
}
