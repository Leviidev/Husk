// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Settings, as a hierarchy rather than one long form.
///
/// Everything used to live on a single scrolling page, so choices that change
/// the machine sat next to choices that change a colour, and the ones with
/// consequences were easy to reach by accident. The grouping here is the
/// concept's: what belongs to the app, what belongs to the emulator, and what
/// the thing actually is.
struct SettingsTab: View {
    @ObservedObject private var runner = QemuRunner.shared
    @ObservedObject private var host = AndroidHost.shared
    @State private var searching = false

    var body: some View {
        NavigationStack {
            ZStack {
                Theme.backdrop
                ScrollView {
                    VStack(alignment: .leading, spacing: 22) {
                        HuskHeader(mark: true, title: "Settings")

                        group("General") {
                            link(LibrarySettings(), "square.grid.2x2", "Library",
                                 "Your apps and their icons")
                            RowDivider()
                            link(PerformanceSettings(), "speedometer", "Performance",
                                 "Renderer, sound")
                            RowDivider()
                            link(AppearanceSettings(), "paintbrush", "Appearance",
                                 "App icon")
                        }

                        group("Emulator") {
                            link(JITSettings(), "bolt.circle", "JIT & sideload",
                                 "Executable memory, starting up")
                            RowDivider()
                            link(InputSettings(), "hand.tap", "Input",
                                 "Screen, touch, keyboard")
                            RowDivider()
                            link(NetworkSettings(), "globe", "Network",
                                 "Internet and saved sessions")
                            RowDivider()
                            link(SavedMachineSettings(), "externaldrive",
                                 "Saved machine", "Snapshots and automatic saving")
                        }

                        group("About") {
                            NavigationLink { AboutSettings() } label: {
                                HStack(spacing: 14) {
                                    HuskMark(size: 34)
                                    VStack(alignment: .leading, spacing: 2) {
                                        Text("Husk")
                                            .font(.system(size: 15, weight: .medium))
                                            .foregroundStyle(Theme.text)
                                        Text("Version \(Bundle.main.version) "
                                           + "· \(Bundle.main.commit)")
                                            .font(.system(size: 12))
                                            .foregroundStyle(Theme.textDim)
                                    }
                                    Spacer(minLength: 8)
                                    Image(systemName: "chevron.right")
                                        .font(.system(size: 13, weight: .semibold))
                                        .foregroundStyle(Theme.textDim.opacity(0.7))
                                }
                                .padding(.horizontal, 14).padding(.vertical, 12)
                                .contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                        }
                    }
                    .padding(.horizontal, 18)
                    .padding(.top, 6)
                    .padding(.bottom, 28)
                }
            }
            .navigationBarHidden(true)
            .toolbar(.hidden, for: .tabBar)
        }
    }

    @ViewBuilder
    private func group<Content: View>(_ title: String,
                                      @ViewBuilder rows: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.system(size: 13, weight: .semibold))
                .foregroundStyle(Theme.textDim)
                .padding(.leading, 4)
            RowGroup { rows() }
        }
    }

    private func link<D: View>(_ destination: D, _ icon: String,
                               _ title: String, _ subtitle: String) -> some View {
        NavigationLink { destination } label: {
            HuskRow(systemImage: icon, title: title, subtitle: subtitle)
        }
        .buttonStyle(.plain)
    }
}

/// A Form, on Husk's page rather than the system's.
private struct DarkForm: ViewModifier {
    func body(content: Content) -> some View {
        content
            .scrollContentBackground(.hidden)
            .background(Theme.backdrop)
            .tint(Theme.accent)
            .navigationBarTitleDisplayMode(.inline)
    }
}

extension View {
    func huskForm() -> some View { modifier(DarkForm()) }
}

// MARK: - Library

struct LibrarySettings: View {
    @ObservedObject private var host = AndroidHost.shared
    @State private var working = false

    var body: some View {
        Form {
            Section {
                DetailRow(label: "Apps", value: "\(host.packages.count)", mono: false)
                DetailRow(label: "With icons",
                          value: "\(host.packages.filter { $0.iconPath != nil }.count)",
                          mono: false)
            } footer: {
                Text("The list is written to disk, so it is on screen before Android "
                   + "has finished starting.")
            }

            Section {
                Button {
                    working = true
                    Task { await host.refreshPackages(); working = false }
                } label: {
                    Label(working ? "Refreshing…" : "Refresh from Android",
                          systemImage: "arrow.clockwise")
                }
                .disabled(working || !host.isReady)

                Button {
                    AndroidHost.forgetIcons()
                    working = true
                    Task { await host.refreshPackages(); working = false }
                } label: {
                    Label("Re-fetch icons", systemImage: "photo.on.rectangle")
                }
                .disabled(working || !host.isReady)
            } footer: {
                Text("Names and icons come from Android's own launcher, which keeps "
                   + "the version it draws. Re-fetching throws away Husk's copies and "
                   + "asks again.")
            }
        }
        .huskForm()
        .navigationTitle("Library")
    }
}

// MARK: - Performance

struct PerformanceSettings: View {
    @ObservedObject private var runner = QemuRunner.shared
    @State private var gpuMode =
        UserDefaults.standard.object(forKey: "husk.gpuMode") as? Bool ?? true
    @State private var sound = UserDefaults.standard.bool(forKey: "husk.sound")
    @State private var soundDevice =
        UserDefaults.standard.object(forKey: "husk.soundDevice") as? Bool ?? true

    var body: some View {
        Form {
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
                DetailRow(label: "Frame rate",
                          value: runner.fps > 0
                                 ? String(format: "%.0f fps", runner.fps) : "—")
                DetailRow(label: "Guest screen",
                          value: "\(QemuRunner.lastGuestRes.w)×\(QemuRunner.lastGuestRes.h)")
            } header: {
                Text("Now")
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
        }
        .huskForm()
        .navigationTitle("Performance")
    }
}

// MARK: - Input

struct InputSettings: View {
    @State private var landscapeGuest =
        UserDefaults.standard.bool(forKey: "husk.landscapeGuest")
    @State private var customRes = UserDefaults.standard.bool(forKey: "husk.customRes")
    @State private var widthText = InputSettings.stored("husk.resWidth", 720)
    @State private var heightText = InputSettings.stored("husk.resHeight", 1280)

    /// Sizes worth offering without typing. Deliberately short: these are the
    /// shapes a phone guest is actually run at, not a catalogue of every panel
    /// ever made.
    private static let presets: [(name: String, w: Int, h: Int)] = [
        ("Small — 360 × 800", 360, 800),
        ("HD — 720 × 1280", 720, 1280),
        ("Full HD — 1080 × 1920", 1080, 1920),
        ("Landscape HD — 1280 × 720", 1280, 720),
        ("Tablet — 1280 × 800", 1280, 800),
    ]

    private static func stored(_ key: String, _ fallback: Int) -> String {
        let v = UserDefaults.standard.integer(forKey: key)
        return String(v > 0 ? v : fallback)
    }

    /// What the typed numbers actually come to, or nothing if they are not a
    /// size the guest can be given.
    private var effective: (w: Int, h: Int)? {
        QemuRunner.validResolution(w: Int(widthText) ?? 0, h: Int(heightText) ?? 0)
    }

    var body: some View {
        Form {
            Section {
                Picker("Screen", selection: $landscapeGuest) {
                    Text("Portrait").tag(false)
                    Text("Landscape").tag(true)
                }
                .pickerStyle(.segmented)
                .disabled(customRes)
                .onChange(of: landscapeGuest) { v in
                    UserDefaults.standard.set(v, forKey: "husk.landscapeGuest")
                    HuskLog.log("ui", v ? "guest panel will be landscape"
                                        : "guest panel will be portrait")
                }
            } header: {
                Text("Screen")
            } footer: {
                Text(customRes
                     ? "A custom resolution sets the shape itself, so this does nothing "
                     + "while it is on. Type a wide size for landscape."
                     : "Android cannot reshape a screen once it is running, so a "
                     + "landscape game on a portrait screen gets letterboxed into a "
                     + "band and looks tiny. Creating it landscape is the only way it "
                     + "can fill it — portrait apps are letterboxed instead. Costs one "
                     + "cold boot.")
            }

            Section {
                Toggle("Custom resolution", isOn: $customRes)
                    .onChange(of: customRes) { v in
                        UserDefaults.standard.set(v, forKey: "husk.customRes")
                        store()
                        HuskLog.log("ui", v ? "custom resolution on: "
                                            + "\(widthText)x\(heightText)"
                                            : "custom resolution off")
                    }

                if customRes {
                    Picker("Preset", selection: Binding(
                        get: { presetIndex },
                        set: { i in
                            guard i >= 0, i < Self.presets.count else { return }
                            widthText = String(Self.presets[i].w)
                            heightText = String(Self.presets[i].h)
                            store()
                        })) {
                        ForEach(0..<Self.presets.count, id: \.self) { i in
                            Text(Self.presets[i].name).tag(i)
                        }
                        Text("Custom").tag(-1)
                    }

                    HStack {
                        Text("Width")
                        Spacer()
                        TextField("720", text: $widthText)
                            .keyboardType(.numberPad)
                            .multilineTextAlignment(.trailing)
                            .font(.technical())
                            .frame(width: 90)
                            .onChange(of: widthText) { _ in store() }
                    }
                    HStack {
                        Text("Height")
                        Spacer()
                        TextField("1280", text: $heightText)
                            .keyboardType(.numberPad)
                            .multilineTextAlignment(.trailing)
                            .font(.technical())
                            .frame(width: 90)
                            .onChange(of: heightText) { _ in store() }
                    }

                    if let size = effective {
                        DetailRow(label: "Android will get",
                                  value: "\(size.w) × \(size.h)")
                    } else {
                        Text("Both sides must be between 240 and 2560.")
                            .font(.caption).foregroundStyle(.orange)
                    }
                }

                DetailRow(label: "Running now", value: running)
            } header: {
                Text("Resolution")
            } footer: {
                Text("The panel is built when the machine starts, so a change costs "
                   + "one cold boot, and the next save replaces the machine saved at "
                   + "the old size — changing back costs another. Sizes are rounded "
                   + "to a multiple of eight. Bigger is slower: every pixel is drawn "
                   + "by an emulated phone. Android's density does not change with "
                   + "the panel, so a larger one shows more rather than bigger.")
            }

            Section {
                Text("Touch is always on. The keyboard and the rotate control are "
                   + "in the pill at the bottom of the guest's screen; a gamepad "
                   + "and a pointer are not wired through yet.")
                    .font(.footnote).foregroundStyle(.secondary)
            } header: {
                Text("Controls")
            }
        }
        .huskForm()
        .navigationTitle("Input")
    }

    /// The panel the guest actually has, which only means anything while there
    /// is a guest: the stored value is last launch's until one starts.
    private var running: String {
        guard QemuRunner.shared.isRunning else { return "not started" }
        return "\(QemuRunner.lastGuestRes.w) × \(QemuRunner.lastGuestRes.h)"
    }

    /// Which preset the typed numbers are, if any.
    private var presetIndex: Int {
        guard let size = effective else { return -1 }
        return Self.presets.firstIndex { $0.w == size.w && $0.h == size.h } ?? -1
    }

    private func store() {
        UserDefaults.standard.set(Int(widthText) ?? 0, forKey: "husk.resWidth")
        UserDefaults.standard.set(Int(heightText) ?? 0, forKey: "husk.resHeight")
    }
}

// MARK: - Network

struct NetworkSettings: View {
    @State private var keepNetwork =
        UserDefaults.standard.object(forKey: "husk.keepNetwork") as? Bool ?? true

    var body: some View {
        Form {
            Section {
                Toggle("Keep the network across saves", isOn: $keepNetwork)
                    .onChange(of: keepNetwork) { v in
                        UserDefaults.standard.set(v, forKey: "husk.keepNetwork")
                    }
            } footer: {
                Text(keepNetwork
                     ? "Saving closes apps but leaves Android's framework running, so "
                     + "the network still works after a restore."
                     : "Saving stops the framework too. Clears every GPU resource, "
                     + "which is steadier — but the network may not come back until a "
                     + "cold boot.")
            }

            Section {
                Text("Android reaches the internet through a virtual ethernet card "
                   + "on QEMU's own network. Nothing on your phone's network can see "
                   + "the guest, and the guest cannot see it.")
                    .font(.footnote).foregroundStyle(.secondary)
            } header: {
                Text("How it connects")
            }
        }
        .huskForm()
        .navigationTitle("Network")
    }
}

// MARK: - JIT and sideloading

struct JITSettings: View {
    @ObservedObject private var runner = QemuRunner.shared
    @State private var autoStart = Onboarding.autoStart

    var body: some View {
        Form {
            Section {
                DetailRow(label: "Debugger",
                          value: JITBootstrap.isDebuggerAttached ? "attached" : "not attached",
                          mono: false)
                DetailRow(label: "Executable memory",
                          value: JITBootstrap.isLive ? "granted" : "not claimed", mono: false)
                // The two routes, named separately. Either one is enough, and
                // when someone reports "JIT does not work" these two rows are
                // the whole diagnosis.
                DetailRow(label: "Trap servicer",
                          value: JITBootstrap.prewarmed ? "answering" : "not answering",
                          mono: false)
                DetailRow(label: "MAP_JIT",
                          value: JITBootstrap.mapJITWorks ? "executes" : "refused",
                          mono: false)
                if let why = JITBootstrap.lastFailure {
                    Text(why).font(.caption).foregroundStyle(.orange)
                }
                if !JITBootstrap.isDebuggerAttached {
                    Button {
                        _ = JITBootstrap.requestAttach()
                    } label: {
                        Label("Enable JIT with StikDebug", systemImage: "bolt.fill")
                    }
                }
            } header: {
                Text("JIT")
            } footer: {
                Text("Husk needs memory it can write and then execute, which on iOS "
                   + "takes an attached debugger. There are two ways to get it: a "
                   + "debugger that services trap requests, or a MAP_JIT mapping, "
                   + "which the kernel allows any debugged process. Either one is "
                   + "enough — which is available depends on the device and the iOS "
                   + "version, so Husk tests both rather than assuming.")
            }

            Section {
                Toggle("Start Android on launch", isOn: $autoStart)
                    .onChange(of: autoStart) { v in
                        UserDefaults.standard.set(v, forKey: "husk.autoStart")
                    }
            } footer: {
                Text("Boots the guest as soon as Husk opens, when JIT is available.")
            }

            Section {
                Text("APKs install from the Library's + button or from the Files tab. "
                   + "Split sets — a base APK plus its config pieces — must be picked "
                   + "together; installing the base alone fails on missing native "
                   + "libraries.")
                    .font(.footnote).foregroundStyle(.secondary)
            } header: {
                Text("Sideloading")
            }
        }
        .huskForm()
        .navigationTitle("JIT & sideload")
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
                   + "seconds rather than minutes. The picture freezes while it writes. "
                   + "With this off, nothing saves by itself — including after an "
                   + "install.")
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
        .huskForm()
        .navigationTitle("Saved machine")
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

    private let columns = [GridItem(.adaptive(minimum: 92), spacing: 14)]

    var body: some View {
        ZStack {
            Theme.backdrop
            ScrollView {
                LazyVGrid(columns: columns, spacing: 14) {
                    ForEach(HuskAppIcon.allCases) { icon in
                        Button {
                            appIcon = icon
                            HuskAppIcon.apply(icon)
                        } label: {
                            VStack(spacing: 8) {
                                if let art = icon.preview(dark: true) {
                                    Image(uiImage: art)
                                        .resizable().scaledToFit()
                                        .frame(width: 60, height: 60)
                                        .clipShape(RoundedRectangle(cornerRadius: 14,
                                                                    style: .continuous))
                                }
                                Text(icon.title)
                                    .font(.system(size: 12))
                                    .foregroundStyle(Theme.text)
                                    .lineLimit(1)
                            }
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 14)
                            // The selected icon is ringed in the accent rather
                            // than filled with it: the artwork is the subject
                            // here, and a tinted panel behind it changes how
                            // the thing you are choosing looks.
                            .background(Theme.surface,
                                        in: RoundedRectangle(cornerRadius: Theme.cardCorner,
                                                             style: .continuous))
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

                Text("Automatic follows the system appearance — light, dark and "
                   + "tinted. The others pin one look. iOS shows its own confirmation "
                   + "after a change; that alert cannot be turned off.")
                    .font(.system(size: 12))
                    .foregroundStyle(Theme.textDim)
                    .multilineTextAlignment(.center)
                    .padding(.horizontal, 28).padding(.vertical, 18)
            }
        }
        .navigationTitle("Appearance")
        .navigationBarTitleDisplayMode(.inline)
    }
}

// MARK: - About

struct AboutSettings: View {
    @ObservedObject private var runner = QemuRunner.shared
    @State private var showLogs = false

    var body: some View {
        ZStack {
            Theme.backdrop
            ScrollView {
                VStack(spacing: 18) {
                    VStack(spacing: 10) {
                        HuskMark(size: 76)
                        Text("Husk")
                            .font(.system(size: 22, weight: .semibold))
                            .foregroundStyle(Theme.text)
                        Text("Version \(Bundle.main.version)")
                            .font(.system(size: 13))
                            .foregroundStyle(Theme.textDim)
                    }
                    .padding(.top, 10)

                    RowGroup {
                        VStack(spacing: 12) {
                            DetailRow(label: "Build", value: Bundle.main.commit)
                            DetailRow(label: "Guest image", value: GuestImage.imageVersion)
                            DetailRow(label: "Renderer",
                                      value: runner.displayKind == .gl ? "GPU"
                                           : runner.displayKind == .software ? "CPU"
                                           : "not started")
                        }
                        .padding(14)
                    }

                    Button { showLogs = true } label: {
                        Label("Open console", systemImage: "terminal")
                    }
                    .buttonStyle(PrimaryButtonStyle())

                    Text("Husk runs unmodified Android APKs in a real Android system "
                       + "on your iPhone. The console shows Husk's live log, the "
                       + "guest's serial output and QEMU's own output — the three "
                       + "files any problem here is diagnosed from.")
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.textDim)
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 14)
                }
                .padding(.horizontal, 18).padding(.vertical, 14)
            }
        }
        .navigationTitle("About")
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
