// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// The screen Husk opens on when it is starting Android for you.
///
/// Before this, "start on launch" dropped you into a library whose apps could
/// not be opened yet, with a strip at the top explaining why — which is a UI
/// asking you to wait in front of it. A boot is a boot: it gets its own screen,
/// it says how far along it is, and it talks to you while it works. The phrases
/// are there because two minutes of a progress bar is two minutes of wondering
/// whether it has hung.
struct BootScreen: View {
    let onSkip: () -> Void

    @ObservedObject private var runner = QemuRunner.shared
    @ObservedObject private var host = AndroidHost.shared

    @State private var phrase = 0
    @State private var began = Date()
    @State private var now = Date()
    @State private var pulse = false

    private let tick = Timer.publish(every: 1, on: .main, in: .common).autoconnect()
    private let rotate = Timer.publish(every: 3.4, on: .main, in: .common).autoconnect()

    /// Said in order, not at random: the first two land while someone is still
    /// looking at the screen, and the jokes should not repeat before the
    /// information does.
    private static let phrases = [
        "Prepare for awesomeness",
        "Waking Android up",
        "Teaching an iPhone to speak Android",
        "This is a whole operating system — be patient",
        "App by Levi",
        "Star the repo if you like this sort of thing",
        "Translating arm64, one block at a time",
        "No, it has not frozen",
        "Unpacking the guest",
        "Almost worth the wait",
        "Negotiating with the JIT",
        "Nearly there",
    ]

    var body: some View {
        ZStack {
            Theme.bg.ignoresSafeArea()

            VStack(spacing: 0) {
                Spacer()

                HuskMark(size: 96)
                    .shadow(color: Theme.accent.opacity(pulse ? 0.45 : 0.15),
                            radius: pulse ? 34 : 18, y: 10)
                    .animation(.easeInOut(duration: 1.8).repeatForever(autoreverses: true),
                               value: pulse)
                    .onAppear { pulse = true }

                Text("HUSK")
                    .font(.system(size: 22, weight: .semibold))
                    .tracking(9)
                    .foregroundStyle(Theme.text)
                    .padding(.leading, 9)
                    .padding(.top, 18)

                // The line that talks. Keyed on the index so each one fades
                // into the next rather than snapping.
                Text(Self.phrases[phrase % Self.phrases.count])
                    .font(.system(size: 15))
                    .foregroundStyle(Theme.textDim)
                    .multilineTextAlignment(.center)
                    .frame(height: 42)
                    .padding(.horizontal, 30)
                    .id(phrase)
                    .transition(.opacity)
                    .padding(.top, 10)

                progress
                    .padding(.horizontal, 44)
                    .padding(.top, 6)

                Spacer()

                // What Android itself is doing, small, under everything else.
                // The phrases pass the time; this is the part that is true.
                Text(runner.setupMessage ?? host.status)
                    .font(.system(size: 11))
                    .foregroundStyle(Theme.textDim.opacity(0.75))
                    .lineLimit(1)
                    .padding(.horizontal, 30)

                // An escape hatch, but not an invitation: it turns up only once
                // waiting has stopped being novel.
                if now.timeIntervalSince(began) > 8 {
                    Button("Use Husk while it starts", action: onSkip)
                        .font(.system(size: 13, weight: .medium))
                        .foregroundStyle(Theme.textDim)
                        .padding(.top, 14)
                        .transition(.opacity)
                }
            }
            .padding(.bottom, 26)
        }
        .onReceive(tick) { now = $0 }
        .onReceive(rotate) { _ in
            withAnimation(.easeInOut(duration: 0.45)) { phrase += 1 }
        }
        .animation(.easeInOut(duration: 0.3), value: now.timeIntervalSince(began) > 8)
    }

    private var progress: some View {
        VStack(spacing: 8) {
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    Capsule().fill(Theme.surfaceHigh)
                    Capsule().fill(Theme.accent)
                        .frame(width: geo.size.width * fraction)
                        .animation(.snappy(duration: 0.4), value: fraction)
                }
            }
            .frame(height: 5)

            HStack {
                Text(runner.bootProgress > 0 ? "\(runner.bootProgress)%" : "starting")
                    .font(.technical(12, weight: .medium))
                    .foregroundStyle(Theme.accent)
                Spacer()
                if let left = remaining {
                    Text(left)
                        .font(.system(size: 12))
                        .foregroundStyle(Theme.textDim)
                }
            }
        }
    }

    private var fraction: CGFloat {
        // Never zero: a bar with nothing in it reads as stuck rather than as
        // early, and the guest says nothing at all for the first few seconds.
        max(CGFloat(runner.bootProgress) / 100, 0.03)
    }

    /// An estimate from this boot's own pace, not from a number someone typed
    /// in. Withheld until there is enough of a curve to divide by, and dropped
    /// again near the end, where being wrong is most annoying.
    private var remaining: String? {
        let done = Double(runner.bootProgress)
        guard done >= 8, done <= 92 else { return nil }
        let elapsed = now.timeIntervalSince(began)
        guard elapsed > 6 else { return nil }
        let total = elapsed / (done / 100)
        let left = total - elapsed
        guard left > 2, left < 15 * 60 else { return nil }
        if left < 90 {
            let rounded = Int((left / 5).rounded()) * 5
            return "about \(max(rounded, 5)) seconds remaining"
        }
        return "about \(Int((left / 60).rounded())) minutes remaining"
    }
}
