// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI
import UIKit

/// Keyboard input for the guest.
///
/// Two routes, because they arrive completely differently:
///
///  * A hardware keyboard (Magic Keyboard, any Bluetooth one) delivers
///    `pressesBegan`/`pressesEnded` with a HID usage code. That maps one-to-one
///    onto QEMU's key codes, including modifiers and auto-repeat, so it is the
///    accurate path and the one worth having.
///  * The on-screen keyboard only ever reports *text*, via `UIKeyInput`. There is
///    no key-down/key-up there, so each character is synthesised as a press and
///    release, with shift wrapped around it when the character needs it.
final class KeyCapturingView: UIView, UIKeyInput {

    override var canBecomeFirstResponder: Bool { true }

    // MARK: - UIKeyInput (software keyboard)

    var hasText: Bool { true }

    var keyboardType: UIKeyboardType = .asciiCapable
    var autocorrectionType: UITextAutocorrectionType = .no
    var autocapitalizationType: UITextAutocapitalizationType = .none
    var spellCheckingType: UITextSpellCheckingType = .no
    var smartQuotesType: UITextSmartQuotesType = .no
    var smartDashesType: UITextSmartDashesType = .no

    func insertText(_ text: String) {
        for ch in text { sendCharacter(ch) }
    }

    func deleteBackward() {
        tap("backspace")
    }

    /// The on-screen keyboard gives no key-up, so press and release immediately.
    private func sendCharacter(_ ch: Character) {
        if ch == "\n" || ch == "\r" { tap("ret"); return }
        if ch == " " { tap("spc"); return }

        guard let (qcode, needsShift) = Self.characterMap[ch] else {
            HuskLog.log("kbd", "no key for character \(String(reflecting: ch))")
            return
        }
        if needsShift { _ = husk_display_send_key("shift", true) }
        tap(qcode)
        if needsShift { _ = husk_display_send_key("shift", false) }
    }

    private func tap(_ qcode: String) {
        _ = husk_display_send_key(qcode, true)
        _ = husk_display_send_key(qcode, false)
    }

    // MARK: - Hardware keyboard

    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        var handled = false
        for press in presses {
            if let key = press.key, let qcode = Self.hidMap[key.keyCode] {
                _ = husk_display_send_key(qcode, true)
                handled = true
            }
        }
        if !handled { super.pressesBegan(presses, with: event) }
    }

    override func pressesEnded(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        var handled = false
        for press in presses {
            if let key = press.key, let qcode = Self.hidMap[key.keyCode] {
                _ = husk_display_send_key(qcode, false)
                handled = true
            }
        }
        if !handled { super.pressesEnded(presses, with: event) }
    }

    override func pressesCancelled(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        // Release everything we were told about, or the guest is left with a key
        // stuck down.
        for press in presses {
            if let key = press.key, let qcode = Self.hidMap[key.keyCode] {
                _ = husk_display_send_key(qcode, false)
            }
        }
    }

    // MARK: - Maps

    /// HID usage -> QEMU QKeyCode name.
    static let hidMap: [UIKeyboardHIDUsage: String] = {
        var m: [UIKeyboardHIDUsage: String] = [:]

        // Letters are contiguous from keyboardA, and so are their names.
        let letters = "abcdefghijklmnopqrstuvwxyz"
        for (i, ch) in letters.enumerated() {
            if let usage = UIKeyboardHIDUsage(rawValue: UIKeyboardHIDUsage.keyboardA.rawValue + i) {
                m[usage] = String(ch)
            }
        }
        // Digits run 1..9 then 0, in that order, in both schemes.
        for (i, name) in ["1","2","3","4","5","6","7","8","9","0"].enumerated() {
            if let usage = UIKeyboardHIDUsage(rawValue: UIKeyboardHIDUsage.keyboard1.rawValue + i) {
                m[usage] = name
            }
        }
        for i in 0..<12 {
            if let usage = UIKeyboardHIDUsage(rawValue: UIKeyboardHIDUsage.keyboardF1.rawValue + i) {
                m[usage] = "f\(i + 1)"
            }
        }

        let rest: [UIKeyboardHIDUsage: String] = [
            .keyboardReturnOrEnter: "ret",
            .keyboardEscape: "esc",
            .keyboardDeleteOrBackspace: "backspace",
            .keyboardTab: "tab",
            .keyboardSpacebar: "spc",
            .keyboardHyphen: "minus",
            .keyboardEqualSign: "equal",
            .keyboardOpenBracket: "bracket_left",
            .keyboardCloseBracket: "bracket_right",
            .keyboardBackslash: "backslash",
            .keyboardSemicolon: "semicolon",
            .keyboardQuote: "apostrophe",
            .keyboardGraveAccentAndTilde: "grave_accent",
            .keyboardComma: "comma",
            .keyboardPeriod: "dot",
            .keyboardSlash: "slash",
            .keyboardCapsLock: "caps_lock",
            .keyboardLeftArrow: "left",
            .keyboardRightArrow: "right",
            .keyboardUpArrow: "up",
            .keyboardDownArrow: "down",
            .keyboardHome: "home",
            .keyboardEnd: "end",
            .keyboardPageUp: "pgup",
            .keyboardPageDown: "pgdn",
            .keyboardDeleteForward: "delete",
            .keyboardInsert: "insert",
            .keyboardPrintScreen: "print",
            .keyboardLeftShift: "shift",
            .keyboardRightShift: "shift_r",
            .keyboardLeftControl: "ctrl",
            .keyboardRightControl: "ctrl_r",
            .keyboardLeftAlt: "alt",
            .keyboardRightAlt: "alt_r",
            .keyboardLeftGUI: "meta_l",
            .keyboardRightGUI: "meta_r",
        ]
        m.merge(rest) { a, _ in a }
        return m
    }()

    /// Character -> (QKeyCode name, needs shift). US layout, which is what the
    /// guest is configured for.
    static let characterMap: [Character: (String, Bool)] = {
        var m: [Character: (String, Bool)] = [:]
        for ch in "abcdefghijklmnopqrstuvwxyz" {
            m[ch] = (String(ch), false)
            m[Character(ch.uppercased())] = (String(ch), true)
        }
        let digits: [(Character, String, Character)] = [
            ("1","1","!"), ("2","2","@"), ("3","3","#"), ("4","4","$"), ("5","5","%"),
            ("6","6","^"), ("7","7","&"), ("8","8","*"), ("9","9","("), ("0","0",")"),
        ]
        for (d, name, shifted) in digits {
            m[d] = (name, false)
            m[shifted] = (name, true)
        }
        let punct: [(Character, String, Character?)] = [
            ("-", "minus", "_"), ("=", "equal", "+"),
            ("[", "bracket_left", "{"), ("]", "bracket_right", "}"),
            ("\\", "backslash", "|"), (";", "semicolon", ":"),
            ("'", "apostrophe", "\""), ("`", "grave_accent", "~"),
            (",", "comma", "<"), (".", "dot", ">"), ("/", "slash", "?"),
        ]
        for (base, name, shifted) in punct {
            m[base] = (name, false)
            if let sh = shifted { m[sh] = (name, true) }
        }
        return m
    }()
}

/// SwiftUI wrapper that owns first-responder state.
struct KeyCapture: UIViewRepresentable {
    @Binding var active: Bool

    func makeUIView(context: Context) -> KeyCapturingView {
        let v = KeyCapturingView()
        v.isUserInteractionEnabled = true
        return v
    }

    func updateUIView(_ view: KeyCapturingView, context: Context) {
        // Showing the on-screen keyboard and capturing hardware keys are the same
        // thing here: first-responder status drives both.
        if active, !view.isFirstResponder {
            view.becomeFirstResponder()
        } else if !active, view.isFirstResponder {
            view.resignFirstResponder()
        }
    }
}

/// The keys a touch-only user cannot otherwise reach, and that Android and a
/// Debian console both need constantly.
struct SpecialKeysBar: View {
    @State private var ctrlLatched = false

    private let keys: [(String, String)] = [
        ("esc", "esc"), ("tab", "tab"), ("⌫", "backspace"), ("↵", "ret"),
        ("←", "left"), ("↓", "down"), ("↑", "up"), ("→", "right"),
    ]

    var body: some View {
        HStack(spacing: 6) {
            // Ctrl latches rather than repeats: there is no chord to hold on a
            // touchscreen, so tap Ctrl, then tap the letter.
            Button {
                ctrlLatched.toggle()
                _ = husk_display_send_key("ctrl", ctrlLatched)
                HuskLog.log("kbd", "ctrl \(ctrlLatched ? "held" : "released")")
            } label: {
                Text("ctrl")
                    .font(.caption2.weight(.medium))
                    .padding(.horizontal, 9).padding(.vertical, 6)
                    .background(ctrlLatched ? AnyShapeStyle(Theme.accent)
                                            : AnyShapeStyle(Theme.surfaceHigh),
                                in: RoundedRectangle(cornerRadius: 7))
            }

            ForEach(keys, id: \.1) { label, qcode in
                Button {
                    _ = husk_display_send_key(qcode, true)
                    _ = husk_display_send_key(qcode, false)
                    if ctrlLatched {
                        ctrlLatched = false
                        _ = husk_display_send_key("ctrl", false)
                    }
                } label: {
                    Text(label)
                        .font(.caption2.weight(.medium))
                        .frame(minWidth: 26)
                        .padding(.horizontal, 7).padding(.vertical, 6)
                        .background(Theme.surfaceHigh, in: RoundedRectangle(cornerRadius: 7))
                }
            }
        }
        .foregroundStyle(.primary)
    }
}
