// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Settings, as a hierarchy rather than one long form.
///
/// Everything used to live on a single scrolling page, so choices that change
/// the machine sat next to choices that change a colour, and the ones with
/// consequences were easy to reach by accident. Grouping them puts each decision
/// next to its own explanation and keeps the front page short enough to scan.
struct SettingsTab: View {
    @ObservedObject private var runner = QemuRunner.shared
    @ObservedObject private var guest = GuestImage.shared

    var body: some View {
        NavigationStack {
            // A plain inset-grouped list, drawn the way the system draws one.
            // It used to be a list with every row background cleared, floating
            // over a coloured wash -- which reads as a half-finished theme
            // rather than as a settings screen. The one place in the app where
            // the platform's own answer is unimprovable is this one.
            List {
                Section {
                    NavigationLink { MachineSettings() } label: {
                        settingRow("cpu", "Machine",
                                   "Screen, renderer, sound", .indigo)
                    }
                    NavigationLink { SavedMachineSettings() } label: {
                        settingRow("externaldrive", "Saved machine",
                                   "Snapshots and automatic saving", .teal)
                    }
                    NavigationLink { AppearanceSettings() } label: {
                        settingRow("paintbrush", "Appearance",
                                   "App icon", .pink)
                    }
                    NavigationLink { DiagnosticsSettings() } label: {
                        settingRow("stethoscope", "Diagnostics",
                                   "Console, logs, build", .orange)
                    }
                }

                Section {
                    DetailRow(label: "Version",
                              value: Bundle.main.version, mono: false)
                    DetailRow(label: "Build", value: Bundle.main.commit)
                    DetailRow(label: "Guest", value: GuestImage.imageVersion)
                } header: {
                    Text("About")
                }
            }
            .listStyle(.insetGrouped)
            .navigationTitle("Settings")
        }
    }

    /// One row, with its icon carrying the colour so the list scans by shape.
    private func settingRow(_ icon: String, _ title: String,
                            _ detail: String, _ tint: Color) -> some View {
        HStack(spacing: 13) {
            Image(systemName: icon)
                .font(.system(size: 15, weight: .semibold))
                .foregroundStyle(.white)
                .frame(width: 30, height: 30)
                .background(tint, in: RoundedRectangle(cornerRadius: 8, style: .continuous))
            VStack(alignment: .leading, spacing: 1) {
                Text(title)
                Text(detail).font(.caption).foregroundStyle(.secondary)
            }
        }
        .padding(.vertical, 3)
    }
}

// MARK: - Machine

struct MachineSettings: View {
    @State private var landscapeGuest =
        UserDefaults.standard.bool(forKey: "husk.landscapeGuest")
    @State private var gpuMode =
        UserDefaults.standard.object(forKey: "husk.gpuMode") as? Bool ?? true
    @State private var sound = UserDefaults.standard.bool(forKey: "husk.sound")
    @State private var soundDevice =
        UserDefaults.standard.object(forKey: "husk.soundDevice") as? Bool ?? true
    @State private var keepNetwork =
        UserDefaults.standard.object(forKey: "husk.keepNetwork") as? Bool ?? true

    var body: some View {
        Form {
            Section {
                Picker("Screen", selection: $landscapeGuest) {
                    Text("Portrait").tag(false)
                    Text("Landscape").tag(true)
                }
                .pickerStyle(.segmented)
                .onChange(of: landscapeGuest) { v in
                    UserDefaults.standard.set(v, forKey: "husk.landscapeGuest")
                    HuskLog.log("ui", v ? "guest panel will be landscape"
                                        : "guest panel will be portrait")
                }
            } header: {
                Text("Screen")
            } footer: {
                Text("Android cannot reshape a screen once it is running, so a "
                   + "landscape game on a portrait screen gets letterboxed into a "
                   + "band and looks tiny. Creating it landscape is the only way it "
                   + "can fill it — portrait apps are letterboxed instead. Costs one "
                   + "cold boot.")
            }

            Section {
                Picker("Renderer", selection: $gpuMode) {
                    Text("GPU").tag(true)
                    Text("CPU").tag(false)
                }
                .pickerStyle(.segmented)
                .onChange(of: gpuMode) { v in
                    UserDefaults.standard.set(v, forKey: "husk.gpuMode")
                    HuskLog.log("ui", v ? "GPU renderer selected" : "CPU renderer selected")
                }
            } header: {
                Text("Renderer")
            } footer: {
                Text(gpuMode
                     ? "Android draws on the real GPU through Metal — about four times "
                     + "the frame rate. This is the default."
                     : "Every pixel is drawn by the emulated CPU. Much slower, and only "
                     + "worth choosing if the GPU misbehaves.")
            }

            Section {
                Toggle("Sound", isOn: $sound)
                    .onChange(of: sound) { v in
                        UserDefaults.standard.set(v, forKey: "husk.sound")
                        HuskLog.log("ui", v ? "sound on" : "sound off")
                    }
                if sound {
                    Toggle("Attach the sound device", isOn: $soundDevice)
                        .onChange(of: soundDevice) { v in
                            UserDefaults.standard.set(v, forKey: "husk.soundDevice")
                        }
                }
            } header: {
                Text("Sound")
            } footer: {
                Text("Adds a sound device. While it is attached Android cannot be "
                   + "saved — QEMU refuses to snapshot a machine with one — so every "
                   + "launch boots from cold. Turning it on or off costs a cold boot "
                   + "either way.")
            }

            Section {
                Toggle("Keep the network across saves", isOn: $keepNetwork)
                    .onChange(of: keepNetwork) { v in
                        UserDefaults.standard.set(v, forKey: "husk.keepNetwork")
                    }
            } header: {
                Text("Network")
            } footer: {
                Text(keepNetwork
                     ? "Saving closes apps but leaves Android's framework running, so "
                     + "the network still works after a restore."
                     : "Saving stops the framework too. Clears every GPU resource, "
                     + "which is steadier — but the network may not come back until a "
                     + "cold boot.")
            }
        }
        .tint(Theme.accent)
        .navigationTitle("Machine")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Saved machine

struct SavedMachineSettings: View {
    @ObservedObject private var runner = QemuRunner.shared
    @State private var autoSave =
        UserDefaults.standard.object(forKey: "husk.autoSave") as? Bool ?? true
    @State private var useSnapshot =
        UserDefaults.standard.object(forKey: "husk.downloadSnapshot") as? Bool ?? true
    @State private var askWhichToDelete = false
    @State private var deleteResult: String?

    var body: some View {
        Form {
            Section {
                Toggle("Save automatically", isOn: $autoSave)
                    .onChange(of: autoSave) { v in
                        UserDefaults.standard.set(v, forKey: "husk.autoSave")
                        HuskLog.log("ui", v ? "automatic saving on" : "automatic saving off")
                    }
                Button {
                    QemuRunner.shared.saveState(reason: "asked from settings")
                } label: {
                    Label(runner.isSavingState ? "Saving…" : "Save now",
                          systemImage: "externaldrive.badge.checkmark")
                }
                .disabled(runner.isSavingState)
            } footer: {
                Text("Husk restores a saved machine instead of booting it, which takes "
                   + "seconds rather than minutes. The picture freezes while it writes.")
            }

            Section {
                Button(role: .destructive) { askWhichToDelete = true } label: {
                    Label("Delete saved machine", systemImage: "trash")
                }
                .disabled(!QemuRunner.shared.hasSnapshot)
                if let deleteResult {
                    Text(deleteResult).font(.caption).foregroundStyle(.secondary)
                }
            } footer: {
                Text(QemuRunner.shared.hasSnapshot
                     ? "Currently saved: "
                     + ((QemuRunner.shared.snapshotDisplay ?? "sw").contains("gl")
                        ? "GPU" : "software") + "."
                     : "Nothing is saved, so Android boots from cold.")
            }

            Section {
                Toggle("Download pre-booted snapshot", isOn: $useSnapshot)
                    .onChange(of: useSnapshot) { v in
                        UserDefaults.standard.set(v, forKey: "husk.downloadSnapshot")
                    }
            } footer: {
                Text("Adds about 2 GB to the first download. It was captured on the "
                   + "software renderer, so it is not used on GPU — which cold-boots "
                   + "once and then saves its own.")
            }
        }
        .tint(Theme.accent)
        .navigationTitle("Saved machine")
        .navigationBarTitleDisplayMode(.inline)
        .confirmationDialog("Which saved machine?", isPresented: $askWhichToDelete,
                            titleVisibility: .visible) {
            Button("GPU machine", role: .destructive) { forget("gl", "GPU") }
            Button("Software machine", role: .destructive) { forget("sw", "software") }
            Button("Cancel", role: .cancel) { }
        } message: {
            Text("Android will boot from cold once, then save a new one.")
        }
    }

    private func forget(_ mode: String, _ name: String) {
        if QemuRunner.shared.forgetSnapshot(mode: mode) {
            deleteResult = "Deleted the \(name) machine. The next launch boots from cold."
        } else {
            deleteResult = "No \(name) machine is saved, so nothing was deleted."
        }
    }
}

// MARK: - Appearance

struct AppearanceSettings: View {
    @State private var appIcon = HuskAppIcon.current
    @Environment(\.colorScheme) private var scheme

    private let columns = [GridItem(.adaptive(minimum: 86), spacing: 14)]

    var body: some View {
        ZStack {
            Theme.backdrop
            ScrollView {
                LazyVGrid(columns: columns, spacing: 16) {
                    ForEach(HuskAppIcon.allCases) { icon in
                        Button {
                            appIcon = icon
                            HuskAppIcon.apply(icon)
                        } label: {
                            VStack(spacing: 7) {
                                if let art = icon.preview(dark: scheme == .dark) {
                                    Image(uiImage: art)
                                        .resizable().scaledToFit()
                                        .frame(width: 64, height: 64)
                                        .clipShape(RoundedRectangle(cornerRadius: 15,
                                                                    style: .continuous))
                                }
                                Text(icon.title).font(.caption2).lineLimit(1)
                            }
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 13)
                            .background(Theme.surface,
                                        in: RoundedRectangle(cornerRadius: Theme.cardCorner,
                                                             style: .continuous))
                            // The selected icon is ringed in the accent rather
                            // than filled with it: the artwork is the subject
                            // here, and a tinted panel behind it changes how
                            // the thing you are choosing looks.
                            .overlay(RoundedRectangle(cornerRadius: Theme.cardCorner,
                                                      style: .continuous)
                                        .stroke(appIcon == icon ? Theme.accent
                                                                : Theme.hairline,
                                                lineWidth: appIcon == icon ? 2 : 0.5))
                        }
                        .buttonStyle(CardButtonStyle())
                    }
                }
                .padding(.horizontal, 18).padding(.top, 12)

                Text("Automatic follows the system appearance — light, dark and tinted. "
                   + "The others pin one look. iOS shows its own confirmation after a "
                   + "change; that alert cannot be turned off.")
                    .font(.caption).foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 28).padding(.vertical, 18)
            }
        }
        .navigationTitle("Appearance")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - Diagnostics

struct DiagnosticsSettings: View {
    @ObservedObject private var runner = QemuRunner.shared
    @State private var showLogs = false

    var body: some View {
        ZStack {
            Theme.backdrop
            ScrollView {
                VStack(spacing: 14) {
                    Card {
                        VStack(spacing: 10) {
                            DetailRow(label: "Build", value: Bundle.main.commit)
                            DetailRow(label: "Guest image", value: GuestImage.imageVersion)
                            DetailRow(label: "Renderer",
                                      value: runner.displayKind == .gl ? "GPU"
                                           : runner.displayKind == .software ? "CPU"
                                           : "not started")
                            DetailRow(label: "Guest screen",
                                      value: "\(QemuRunner.lastGuestRes.w)×"
                                           + "\(QemuRunner.lastGuestRes.h)")
                        }
                    }

                    Button { showLogs = true } label: {
                        Label("Open console", systemImage: "terminal")
                    }
                    .buttonStyle(PrimaryButtonStyle())

                    Text("The console shows Husk's live log and can share it, the "
                       + "guest's serial output and QEMU's own output. Those three "
                       + "files are what any problem here is diagnosed from.")
                        .font(.caption).foregroundStyle(.secondary)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 14)
                }
                .padding(.horizontal, 18).padding(.vertical, 14)
            }
        }
        .navigationTitle("Diagnostics")
        .navigationBarTitleDisplayMode(.inline)
        .sheet(isPresented: $showLogs) { LogView() }
    }
}

extension Bundle {
    var version: String {
        (infoDictionary?["CFBundleShortVersionString"] as? String) ?? "?"
    }
    var commit: String {
        (infoDictionary?["HuskBuildCommit"] as? String) ?? "?"
    }
}
