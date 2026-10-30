// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit
import UniformTypeIdentifiers

/// The system's file picker, presented from UIKit.
///
/// SwiftUI's `.fileImporter` is attached to a view, and it is known to go quiet -- the picker's Open button
/// does nothing -- when more than one is attached anywhere above or beside it, or when its host view is rebuilt
/// while it is up. This is presented from the topmost view controller instead, with nothing of the view tree
/// involved, and a failure is reported rather than dropped.
@MainActor
enum HuskFilePicker {
    private static var live: Coordinator?

    static func present(types: [UTType] = [.item], multiple: Bool = true, asCopy: Bool = false,
                        onPick: @escaping ([URL]) -> Void, onFail: ((String) -> Void)? = nil,
                        onCancel: (() -> Void)? = nil) {
        if let previous = live {
            previous.replace()
            live = nil
            if let shown = previous.picker, shown.presentingViewController != nil {
                shown.dismiss(animated: false)
            }
        }
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: types, asCopy: asCopy)
        picker.allowsMultipleSelection = multiple
        if #available(iOS 16.0, *) { picker.shouldShowFileExtensions = true }
        let coordinator = Coordinator(picker: picker, onPick: onPick, onCancel: onCancel)
        picker.delegate = coordinator
        live = coordinator
        guard let top = topController() else {
            live = nil
            onFail?("There is no screen to show the file picker on.")
            onCancel?()
            return
        }
        HuskLog.log("ui", "file picker: presenting from \(type(of: top))")
        top.present(picker, animated: true)
    }

    private static func topController() -> UIViewController? {
        let scenes = UIApplication.shared.connectedScenes.compactMap { $0 as? UIWindowScene }
        let window = scenes.flatMap { $0.windows }.first { $0.isKeyWindow } ?? scenes.flatMap { $0.windows }.first
        var top = window?.rootViewController
        while let next = top?.presentedViewController, !next.isBeingDismissed { top = next }
        return top
    }

    private static func clear(_ coordinator: Coordinator) {
        if live === coordinator { live = nil }
    }

    private final class Coordinator: NSObject, UIDocumentPickerDelegate {
        let onPick: ([URL]) -> Void
        let onCancel: (() -> Void)?
        weak var picker: UIDocumentPickerViewController?
        private var finished = false
        init(picker: UIDocumentPickerViewController, onPick: @escaping ([URL]) -> Void, onCancel: (() -> Void)?) {
            self.picker = picker
            self.onPick = onPick
            self.onCancel = onCancel
        }

        func replace() {
            guard !finished else { return }
            finished = true
            onCancel?()
        }

        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
            HuskLog.log("ui", "file picker: \(urls.count) picked: " + urls.map(\.lastPathComponent).joined(separator: ", "))
            finishPick(controller, urls)
        }

        /// iOS 14 and some later builds call only the singular method.
        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentAt url: URL) {
            HuskLog.log("ui", "file picker: picked \(url.lastPathComponent)")
            finishPick(controller, [url])
        }

        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            HuskLog.log("ui", "file picker: cancelled")
            finishCancel()
        }

        private func finishPick(_ controller: UIDocumentPickerViewController, _ urls: [URL]) {
            guard !finished else { return }
            finished = true
            let pick = onPick
            let cancel = onCancel
            Self.afterPickerGone(controller) {
                HuskFilePicker.clear(self)
                if urls.isEmpty { cancel?(); return }
                pick(urls)
            }
        }

        private func finishCancel() {
            guard !finished else { return }
            finished = true
            HuskFilePicker.clear(self)
            onCancel?()
        }

        /// The system usually dismisses the picker itself. Dismiss only that controller, never its presenter:
        /// dismissing the presenter after the first pick tore down the game and the next import did nothing.
        private static func afterPickerGone(_ controller: UIViewController, _ work: @escaping () -> Void) {
            let run = { DispatchQueue.main.async(execute: work) }
            if controller.presentingViewController != nil, !controller.isBeingDismissed {
                controller.dismiss(animated: true, completion: run)
            } else {
                run()
            }
        }
    }
}

extension View {
    /// Present the file picker when `isPresented` turns true (and turn it back off), calling `onPick` with what was chosen.
    func huskFilePicker(isPresented: Binding<Bool>, types: [UTType] = [.item], multiple: Bool = true,
                        onPick: @escaping ([URL]) -> Void) -> some View {
        onChange(of: isPresented.wrappedValue) { shown in
            guard shown else { return }
            isPresented.wrappedValue = false
            HuskFilePicker.present(types: types, multiple: multiple, onPick: onPick)
        }
    }
}
