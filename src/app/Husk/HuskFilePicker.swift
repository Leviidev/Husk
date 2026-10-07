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

    static func present(types: [UTType] = [.item], multiple: Bool = true,
                        onPick: @escaping ([URL]) -> Void, onFail: ((String) -> Void)? = nil) {
        guard live == nil else { return }
        let picker = UIDocumentPickerViewController(forOpeningContentTypes: types, asCopy: false)
        picker.allowsMultipleSelection = multiple
        let coordinator = Coordinator(onPick: onPick)
        picker.delegate = coordinator
        live = coordinator
        guard let top = topController() else {
            live = nil
            onFail?(localizedString("There is no screen to show the file picker on."))
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

    private final class Coordinator: NSObject, UIDocumentPickerDelegate {
        let onPick: ([URL]) -> Void
        init(onPick: @escaping ([URL]) -> Void) { self.onPick = onPick }

        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) {
            HuskLog.log("ui", "file picker: \(urls.count) picked: " + urls.map(\.lastPathComponent).joined(separator: ", "))
            HuskFilePicker.live = nil
            if !urls.isEmpty { onPick(urls) }
        }

        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) {
            HuskLog.log("ui", "file picker: cancelled")
            HuskFilePicker.live = nil
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
