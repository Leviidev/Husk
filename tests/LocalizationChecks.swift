// SPDX-License-Identifier: GPL-2.0-or-later
import Foundation

@main
enum LocalizationChecks {
    static func main() {
        let resources = URL(fileURLWithPath: CommandLine.arguments[1])
        let chinese = Bundle(url: resources.appendingPathComponent("zh-Hans.lproj"))!
        let english = Bundle(url: resources.appendingPathComponent("en.lproj"))!

        func expect(_ actual: String, _ expected: String) {
            precondition(actual == expected, "\(actual.debugDescription) != \(expected.debugDescription)")
        }

        expect(L10n.tr("Library", bundle: chinese), "应用库")
        expect(L10n.tr("Library", bundle: english), "Library")
        expect(L10n.tr("Untranslated fallback", bundle: chinese), "Untranslated fallback")
        expect(L10n.tr("100% ready", bundle: chinese), "100% ready")
        expect(L10n.tr("Starting · \(75)%", bundle: chinese), "正在启动 · 75%")
        expect(L10n.tr("Version \("3.0.0")", bundle: chinese), "版本 3.0.0")
        expect(L10n.tr("Installing \("游戏 100% %@")…", bundle: chinese), "正在安装 游戏 100% %@…")
        expect(L10n.tr("Restored \(3) file\("s") from \("backup.zip").", bundle: chinese),
               "已从 backup.zip 恢复 3 个文件。")
        expect(L10n.tr("Restored \(3) file\("s") from \("backup.zip").", bundle: english),
               "Restored 3 files from backup.zip.")
        expect(L10n.tr("\(2) DEX file\("s"), \("10 MB")", bundle: chinese), "2 个 DEX 文件，10 MB")
        expect(L10n.tr("Metadata for \(5) of your app\("s") has been updated. Would you like to download the updates?",
                       bundle: chinese), "5 个应用的元数据已有更新。是否下载这些更新？")
        expect(L10n.tr("Doesn\u{2019}t Work", bundle: chinese), "无法运行")
        expect(L10n.tr("Number: \(1.5, specifier: "%.1f")", bundle: english),
               "Number: " + String(format: "%.1f", locale: Locale.current, 1.5))

        let supported = ["en", "zh-Hans"]
        precondition(Bundle.preferredLocalizations(from: supported, forPreferences: ["zh-CN"]).first == "zh-Hans")
        precondition(Bundle.preferredLocalizations(from: supported, forPreferences: ["en-US"]).first == "en")
        print("Localization checks passed: Chinese, English, fallback, interpolation, percentages, plural suffixes and locale selection.")
    }
}
