// SPDX-License-Identifier: GPL-2.0-or-later
import ActivityKit
import SwiftUI
import WidgetKit

/// Husk's download progress on the Lock Screen and in the Dynamic Island.
@main
struct HuskDownloadsWidgets: WidgetBundle {
    var body: some Widget {
        if #available(iOS 16.1, *) { DownloadActivityWidget() }
    }
}

@available(iOS 16.1, *)
struct DownloadActivityWidget: Widget {
    var body: some WidgetConfiguration {
        ActivityConfiguration(for: HuskDownloadAttributes.self) { context in
            LockScreenView(state: context.state)
                .padding(16)
                .activityBackgroundTint(Color.black.opacity(0.75))
                .activitySystemActionForegroundColor(.white)
        } dynamicIsland: { context in
            DynamicIsland {
                DynamicIslandExpandedRegion(.leading) {
                    Image(systemName: stateIcon(context.state)).font(.title2).foregroundStyle(stateTint(context.state))
                }
                DynamicIslandExpandedRegion(.trailing) {
                    Text(statePercent(context.state)).font(.headline.monospacedDigit())
                }
                DynamicIslandExpandedRegion(.bottom) {
                    VStack(alignment: .leading, spacing: 6) {
                        Text(context.state.title).font(.subheadline.weight(.semibold)).lineLimit(1)
                        Bar(state: context.state)
                        HStack {
                            Text(stateDetail(context.state)).font(.caption).foregroundStyle(.secondary)
                            Spacer()
                            TimeLeft(state: context.state)
                        }
                    }
                }
            } compactLeading: {
                Image(systemName: stateIcon(context.state)).foregroundStyle(stateTint(context.state))
            } compactTrailing: {
                Text(statePercent(context.state)).font(.caption.monospacedDigit())
            } minimal: {
                Image(systemName: stateIcon(context.state)).foregroundStyle(stateTint(context.state))
            }
        }
    }
}

@available(iOS 16.1, *)
private struct LockScreenView: View {
    let state: HuskDownloadAttributes.ContentState
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Image(systemName: stateIcon(state)).foregroundStyle(stateTint(state))
                Text(state.title).font(.headline).lineLimit(1)
                Spacer()
                Text(statePercent(state)).font(.headline.monospacedDigit())
            }
            Bar(state: state)
            HStack {
                Text(stateDetail(state)).font(.caption).foregroundStyle(.secondary)
                Spacer()
                TimeLeft(state: state)
            }
        }
        .foregroundStyle(.white)
    }
}

/// The bar: between the app's updates it runs on the estimated finish time, so it keeps moving while Husk is asleep.
@available(iOS 16.1, *)
private struct Bar: View {
    let state: HuskDownloadAttributes.ContentState
    var body: some View {
        if !state.finished, !state.failed, let end = state.finishBy, end > Date() {
            ProgressView(timerInterval: Date()...end, countsDown: false) { EmptyView() } currentValueLabel: { EmptyView() }
                .tint(.accentColor)
        } else {
            ProgressView(value: state.total > 0 ? min(1, Double(state.received) / Double(state.total)) : (state.finished ? 1 : 0))
                .tint(state.failed ? .orange : state.finished ? .green : .accentColor)
        }
    }
}

/// Time left, counting down by itself between updates.
@available(iOS 16.1, *)
private struct TimeLeft: View {
    let state: HuskDownloadAttributes.ContentState
    var body: some View {
        if !state.finished, !state.failed, let end = state.finishBy, end > Date() {
            Text(timerInterval: Date()...end, countsDown: true).font(.caption.monospacedDigit()).foregroundStyle(.secondary)
                .multilineTextAlignment(.trailing).frame(maxWidth: 70, alignment: .trailing)
        }
    }
}

@available(iOS 16.1, *)
private func statePercent(_ s: HuskDownloadAttributes.ContentState) -> String {
    if s.finished { return L10n.tr("Done") }
    guard s.total > 0 else { return "…" }
    return "\(Int(Double(s.received) / Double(s.total) * 100))%"
}

@available(iOS 16.1, *)
private func stateDetail(_ s: HuskDownloadAttributes.ContentState) -> String {
    let f = ByteCountFormatter()
    if s.failed { return L10n.tr("Stopped. Open Husk to retry.") }
    if s.finished { return L10n.tr("\(f.string(fromByteCount: s.total)) downloaded") }
    var text = L10n.tr("\(f.string(fromByteCount: s.received)) of \(f.string(fromByteCount: s.total))")
    if s.speed > 1 { text += " · \(f.string(fromByteCount: Int64(s.speed)))/s" }
    if s.filesTotal > 1 { text += L10n.tr(" · \(s.filesDone)/\(s.filesTotal) files") }
    return text
}

@available(iOS 16.1, *)
private func stateIcon(_ s: HuskDownloadAttributes.ContentState) -> String {
    s.failed ? "exclamationmark.circle.fill" : s.finished ? "checkmark.circle.fill" : "arrow.down.circle.fill"
}

@available(iOS 16.1, *)
private func stateTint(_ s: HuskDownloadAttributes.ContentState) -> Color {
    s.failed ? .orange : s.finished ? .green : .accentColor
}
