// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Downloads from a link, and what has an update.
struct DownloadsTab: View {
    @ObservedObject private var downloads = Downloads.shared
    @ObservedObject private var store = PlayStoreManager.shared
    @ObservedObject private var updates = AppUpdates.shared
    @ObservedObject private var guest = GuestImage.shared
    @State private var adding = false
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                updatesSection
                Section {
                    if downloads.jobs.isEmpty {
                        VStack(alignment: .leading, spacing: 6) {
                            Text(L10n.tr("No downloads")).font(.headline)
                            Text(L10n.tr("Add a link to an APK or a file, or the address a computer gives you when it shares a folder."))
                                .font(.subheadline).foregroundStyle(.secondary)
                        }
                        .padding(.vertical, 6)
                    }
                    ForEach(store.downloadProgress.keys.sorted(), id: \.self) { pkg in
                        HStack(spacing: 10) {
                            Image(systemName: "bag.fill").foregroundStyle(Color.accentColor)
                            VStack(alignment: .leading, spacing: 6) {
                                Text(store.downloadTitles[pkg] ?? pkg).font(.headline).lineLimit(1)
                                ProgressView(value: store.downloadProgress[pkg] ?? 0)
                                Text(L10n.tr("From the Store")).font(.caption).foregroundStyle(.secondary)
                            }
                            Button(role: .destructive) { store.cancelDownload(pkg) } label: {
                                Image(systemName: "xmark.circle.fill").font(.title2)
                            }
                            .buttonStyle(.borderless)
                            .accessibilityLabel(L10n.tr("Cancel Download"))
                        }
                        .padding(.vertical, 4)
                    }
                    ForEach(downloads.jobs) { job in
                        DownloadRow(job: job, speed: downloads.speed)
                            .swipeActions {
                                Button(role: .destructive) { downloads.remove(job.id) } label: { Label(L10n.tr("Remove"), systemImage: "trash") }
                            }
                    }
                } header: {
                    HStack {
                        Text(L10n.tr("Downloads"))
                        Spacer()
                        if downloads.jobs.contains(where: \.complete) {
                            Button(L10n.tr("Clear Finished")) { downloads.clearFinished() }.font(.caption).textCase(nil)
                        }
                    }
                } footer: {
                    VStack(alignment: .leading, spacing: 6) {
                        if downloads.liveActivitiesOff {
                            Text(L10n.tr("Live Activities are off for Husk, so downloads don't show on the Lock Screen or in the Dynamic Island. Turn them on in Settings › Husk › Live Activities."))
                                .foregroundStyle(.orange)
                        }
                        if let free = Downloads.freeSpace() { Text(L10n.tr("\(Downloads.bytes(free)) free on this device.")) }
                    }
                }
            }
            .navigationTitle(L10n.tr("Downloads"))
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button(L10n.tr("Done")) { dismiss() } }
                ToolbarItem(placement: .primaryAction) {
                    Button { adding = true } label: { Label(L10n.tr("Add"), systemImage: "plus") }
                }
            }
            .sheet(isPresented: $adding) { AddDownloadSheet() }
        }
    }

    @ViewBuilder private var updatesSection: some View {
        Section(L10n.tr("Updates")) {
            if let release = updates.available {
                HStack {
                    Label("Husk \(release.version)", systemImage: "arrow.down.app.fill")
                    Spacer()
                    Button(L10n.tr("View")) { UIApplication.shared.open(release.page) }.buttonStyle(.bordered)
                }
            }
            if guest.update.isSomething {
                HStack {
                    Label(guest.update.title, systemImage: "shippingbox.fill")
                    Spacer()
                    Button(L10n.tr("Download")) { guest.applyUpdate() }.buttonStyle(.bordered)
                }
            }
            if updates.available == nil && !guest.update.isSomething {
                Label(L10n.tr("Everything is up to date"), systemImage: "checkmark.circle").foregroundStyle(.secondary)
            }
        }
    }
}

private struct DownloadRow: View {
    let job: Downloads.Job
    let speed: Double

    private var fraction: Double { job.total > 0 ? min(1, Double(job.received) / Double(job.total)) : 0 }

    private var status: String {
        if job.complete { return job.kind == .apk ? L10n.tr("Added to the library") : job.kind == .folder ? L10n.tr("In Shared Storage") : L10n.tr("In Shared Storage") }
        if job.paused { return L10n.tr("Paused · \(Downloads.bytes(job.received)) of \(Downloads.bytes(job.total))") }
        if let err = job.files.first(where: { $0.error != nil })?.error { return L10n.tr("Stopped: \(err)") }
        var s = L10n.tr("\(Downloads.bytes(job.received)) of \(Downloads.bytes(job.total))")
        if job.files.count > 1 { s += L10n.tr(" · \(job.filesDone)/\(job.files.count) files") }
        if speed > 1 {
            s += " · \(Downloads.bytes(Int64(speed)))/s"
            let left = Double(job.total - job.received) / speed
            if left.isFinite, left > 0 {
                let f = DateComponentsFormatter(); f.allowedUnits = left > 3600 ? [.hour, .minute] : [.minute, .second]; f.unitsStyle = .abbreviated
                if let t = f.string(from: left) { s += L10n.tr(" · \(t) left") }
            }
        }
        return s
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 10) {
                Image(systemName: job.kind == .folder ? "folder.fill" : job.kind == .apk ? "app.badge.fill" : "doc.fill")
                    .foregroundStyle(job.complete ? Color.green : Color.accentColor)
                Text(job.title).font(.headline).lineLimit(1)
                Spacer()
                if !job.complete {
                    Button {
                        if job.paused || job.failed { Downloads.shared.resume(job.id) } else { Downloads.shared.pause(job.id) }
                    } label: {
                        Image(systemName: job.paused || job.failed ? "arrow.clockwise.circle.fill" : "pause.circle.fill").font(.title2)
                    }
                    .buttonStyle(.borderless)
                    Button(role: .destructive) { Downloads.shared.remove(job.id) } label: {
                        Image(systemName: "xmark.circle.fill").font(.title2)
                    }
                    .buttonStyle(.borderless)
                    .accessibilityLabel(L10n.tr("Cancel Download"))
                }
            }
            if !job.complete { ProgressView(value: fraction) }
            Text(status).font(.caption).foregroundStyle(job.failed ? Color.orange : Color.secondary).lineLimit(2)
        }
        .padding(.vertical, 4)
    }
}

private struct AddDownloadSheet: View {
    @Environment(\.dismiss) private var dismiss
    @ObservedObject private var downloads = Downloads.shared
    @State private var link = ""
    @State private var working = false

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    TextField("https://… or http://192.168.1.20:8642/", text: $link)
                        .keyboardType(.URL)
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                } footer: {
                    if let problem = downloads.problem { Text(problem).foregroundStyle(.orange) }
                }
                Section {
                    Text(L10n.tr("An APK is added to your library when it finishes. Anything else goes into the Shared Storage folder, which games see as their phone storage."))
                    Text(L10n.tr("To send a whole folder from a computer on the same Wi‑Fi, run this on it and enter the address it shows:"))
                    Text("python3 serve-folder.py <folder>")
                        .font(.system(.footnote, design: .monospaced))
                        .textSelection(.enabled)
                    Text(L10n.tr("serve-folder.py is in Husk's repository, under tools. The folder's contents land at the top of Shared Storage, so serve the folder that holds what a game expects there."))
                } header: {
                    Text(L10n.tr("How it works"))
                }
                .font(.footnote)
                .foregroundStyle(.secondary)
            }
            .navigationTitle(L10n.tr("Add Download"))
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) { Button(L10n.tr("Cancel")) { dismiss() } }
                ToolbarItem(placement: .confirmationAction) {
                    if working { ProgressView() } else {
                        Button(L10n.tr("Download")) {
                            working = true
                            Task {
                                await downloads.add(link: link)
                                working = false
                                if downloads.problem == nil { dismiss() }
                            }
                        }
                        .disabled(link.trimmingCharacters(in: .whitespaces).isEmpty)
                    }
                }
            }
            .onAppear { downloads.problem = nil }
        }
    }
}
