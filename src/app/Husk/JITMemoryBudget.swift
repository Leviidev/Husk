// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// One manually configured code-region size for every app. No per-app estimates or learned profiles.
enum JITMemoryBudget {
    static let mib = 1024 * 1024
    static let minimumMiB = 2
    static let maximumMiB = 1024
    // Preserve the existing setting and the user's saved value.
    static let settingKey = "husk.jit.fallbackMegabytes"
    static let defaultMiB = 512

    static func validMegabytes(_ text: String) -> Int? {
        guard let value = Int(text.trimmingCharacters(in: .whitespacesAndNewlines)),
              (minimumMiB...maximumMiB).contains(value) else { return nil }
        return value
    }

    static func megabytes(defaults: UserDefaults = .standard) -> Int {
        validMegabytes(String(defaults.integer(forKey: settingKey))) ?? defaultMiB
    }

    static func bytes(defaults: UserDefaults = .standard) -> Int { megabytes(defaults: defaults) * mib }

}
