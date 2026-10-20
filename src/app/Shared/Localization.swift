// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

/// Localizes both SwiftUI labels and strings produced by models, alerts and UIKit.
/// All interpolation arguments use %@, so translated sentences can reorder them
/// without depending on a Swift value's integer width or numeric representation.
enum L10n {
    static func tr(_ text: LocalizedText, bundle: Bundle = .main) -> String {
        let format = bundle.localizedString(forKey: text.key, value: text.key, table: "Localizable")
        return String(format: format, locale: Locale.current, arguments: text.arguments)
    }
}

struct LocalizedText: ExpressibleByStringLiteral, ExpressibleByStringInterpolation {
    let key: String
    let arguments: [CVarArg]

    init(stringLiteral value: String) {
        key = value.replacingOccurrences(of: "%", with: "%%")
        arguments = []
    }

    init(stringInterpolation: StringInterpolation) {
        key = stringInterpolation.key
        arguments = stringInterpolation.arguments
    }

    struct StringInterpolation: StringInterpolationProtocol {
        var key = ""
        var arguments: [CVarArg] = []

        init(literalCapacity: Int, interpolationCount: Int) {
            key.reserveCapacity(literalCapacity)
            arguments.reserveCapacity(interpolationCount)
        }

        mutating func appendLiteral(_ literal: String) {
            key += literal.replacingOccurrences(of: "%", with: "%%")
        }

        mutating func appendInterpolation<T>(_ value: T) {
            key += "%@"
            arguments.append(String(describing: value))
        }

        mutating func appendInterpolation<T: CVarArg>(_ value: T, specifier: String) {
            key += "%@"
            arguments.append(String(format: specifier, locale: Locale.current, value))
        }
    }
}
