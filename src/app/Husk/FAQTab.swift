// SPDX-License-Identifier: GPL-2.0-or-later
import SwiftUI

/// Help: common problems and what to do about them, so a user stuck on JIT or on an app that will not open has an answer
/// without leaving Husk. A page in Settings (it was a tab of its own; four tabs read simpler than five).
struct FAQTab: View {
    @State private var query = ""

    private var sections: [FAQ.Section] {
        let q = query.trimmingCharacters(in: .whitespaces)
        guard !q.isEmpty else { return FAQ.sections }
        return FAQ.sections.compactMap { s in
            let items = s.items.filter { $0.question.localizedCaseInsensitiveContains(q) || $0.answer.localizedCaseInsensitiveContains(q) }
            return items.isEmpty ? nil : FAQ.Section(title: s.title, symbol: s.symbol, items: items)
        }
    }

    var body: some View {
            HuskPageList {
                ForEach(sections) { s in
                    Section {
                        ForEach(s.items) { item in FAQRow(item: item, expanded: !query.isEmpty) }
                    } header: {
                        Label(s.title, systemImage: s.symbol)
                    }
                }
                if sections.isEmpty {
                    Text("Nothing matches “\(query)”. Open Settings › About › Open Console to see what Husk is doing.")
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Help")
            .searchable(text: $query, prompt: "Search problems")
    }
}

private struct FAQRow: View {
    let item: FAQ.Item
    var expanded: Bool
    @State private var open = false

    var body: some View {
        DisclosureGroup(isExpanded: Binding(get: { open || expanded }, set: { open = $0 })) {
            Text(.init(item.answer))
                .font(.subheadline)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
                .padding(.vertical, 4)
        } label: {
            Text(item.question).font(.body.weight(.medium))
        }
    }
}

enum FAQ {
    struct Item: Identifiable { let question: String; let answer: String; var id: String { question } }
    struct Section: Identifiable { let title: String; let symbol: String; let items: [Item]; var id: String { title } }

    static let sections: [Section] = [
        Section(title: "JIT", symbol: "bolt.fill", items: [
            Item(question: "What is JIT, and why does Husk need it?",
                 answer: "Android apps and games are compiled while they run, which needs memory that can be written and then run. "
                       + "iOS only allows that while a debugger is attached to the app. Turning on JIT attaches one for a moment. "
                       + "Without it, Husk can show your library but cannot start Android or a game."),
            Item(question: "Which way of turning on JIT should I use?",
                 answer: "• **iOS 27 or later:** Settings › JIT › Pair on this iPhone. No computer needed.\n"
                       + "• **iOS 26:** StikJIT (built in) with a pairing file made on a computer, plus LocalDevVPN.\n"
                       + "• **Older iOS, or Husk inside LiveContainer:** the StikDebug app.\n"
                       + "• **Installed with TrollStore:** TrollStore, with its URL Scheme setting on.\n"
                       + "• **Jailbroken with Dopamine:** turn on JIT for apps in the jailbreak's settings."),
            Item(question: "StikJIT says it cannot connect, or Enable JIT fails",
                 answer: "1. Make sure **LocalDevVPN** is installed and connected. The VPN icon should be in the status bar.\n"
                       + "2. Wi-Fi must be on, even though nothing leaves your iPhone.\n"
                       + "3. **Developer Mode** must be on (Settings › Privacy & Security › Developer Mode).\n"
                       + "4. The first time, Husk downloads Apple's Developer Disk Image, which can take a minute. Let it finish.\n"
                       + "5. Still failing? Toggle LocalDevVPN off and on, then pair again."),
            Item(question: "“Pair with Husk” does not appear in Developer Mode",
                 answer: "Tap **Start pairing** in Husk first and allow **Local Network** access when asked. If you said no earlier, "
                       + "turn it on in Settings › Privacy & Security › Local Network › Husk. Wi-Fi has to be on. "
                       + "Developer Mode must already be enabled, which restarts the iPhone the first time."),
            Item(question: "My pairing file stopped working",
                 answer: "Pairing files stop working when the iPhone's trust settings are reset, and sometimes after an iOS update. "
                       + "Make a new one, or pair on the iPhone itself if you are on iOS 27, then import it again in Settings › JIT."),
            Item(question: "StikDebug opens but JIT stays off",
                 answer: "Open StikDebug on its own and check that it has your pairing file and that LocalDevVPN is connected. "
                       + "Update StikDebug to the latest release. Husk sends StikDebug its script itself, so nothing needs setting up for Husk inside StikDebug."),
            Item(question: "Why do I have to turn JIT on again?",
                 answer: "JIT lasts as long as Husk is running. When iOS closes Husk in the background, or you swipe it away, "
                       + "the next launch needs JIT again. With Pair on this iPhone or a pairing file it is one tap."),
            Item(question: "Can I use Husk without JIT?",
                 answer: "You can browse and import apps, but nothing Android can run. Every game and app needs JIT."),
        ]),
        Section(title: "Husk Itself", symbol: "app.badge", items: [
            Item(question: "Husk will not open at all",
                 answer: "• **Opens and closes instantly:** the signing certificate has probably expired. Free Apple accounts last 7 days. "
                       + "Re-sign or reinstall Husk with your sideloader; your apps and saves stay.\n"
                       + "• **“Untrusted Developer”:** go to Settings › General › VPN & Device Management and trust your certificate.\n"
                       + "• **Asks for Developer Mode:** turn it on in Settings › Privacy & Security."),
            Item(question: "Husk closed while I was playing",
                 answer: "When Husk opens again it shows what happened, with a report you can share. Usually the game ran out of memory "
                       + "or hit something Husk does not support yet. Close other apps, and share the report if it keeps happening."),
            Item(question: "How do I get logs to report a problem?",
                 answer: "Open the app's page in your Library and tap **Share Report**. For Husk's own log, go to "
                       + "Settings › About › **Open Console**. Turning on **Developer Info** there adds more detail."),
            Item(question: "How do I update Husk?",
                 answer: "Husk tells you when a new version is out. Install the new IPA over the old one with the same sideloader. "
                       + "Your library, apps and saves are kept."),
        ]),
        Section(title: "Apps and Games", symbol: "gamecontroller.fill", items: [
            Item(question: "How do I add a PS4 game?",
                 answer: "Tap + and pick a fake package (.pkg), a folder of packages, or an extracted game folder (eboot.bin and sce_sys). "
                       + "Husk installs it into PS4 Games in Files, and an update package goes beside the game it updates. "
                       + "Retail packages are locked to a PS4 and do not install. PS4 games need iOS 17.4 or later."),
            Item(question: "An app will not open",
                 answer: "Open its page in the Library. Husk says why it stopped:\n"
                       + "• **Protected app:** it checks that it is on a real Android phone, or uses anti-cheat or DRM. Husk does not get around these.\n"
                       + "• **Wrong kind of CPU code:** the APK only has x86 or another non-ARM architecture. Find an ARM (arm64-v8a or armeabi-v7a) version.\n"
                       + "• **Incomplete app or missing game data:** the APK needs its split files or an OBB. Import the full .apks, .xapk or .apkm.\n"
                       + "• **Needs Google:** the app requires a Google account or Play services Husk cannot provide.\n"
                       + "• **Uses something Husk does not have yet:** share the report so it can be added."),
            Item(question: "The screen stays black",
                 answer: "The first launch of a big game prepares its code and data, and can take up to a minute. "
                       + "If it is still black after that, close it, open it once more and share the report if nothing changes."),
            Item(question: "A modded or repacked APK crashes",
                 answer: "Repacks from sites like APKVISION often turn the app's code into their own library or add tamper checks, "
                       + "and these break more often. Try the original APK from the developer or from APKMirror."),
            Item(question: "Which files can I import?",
                 answer: ".apk, and split bundles as .apks, .xapk or .apkm. Share them to Husk, open them from Files, or tap + in the Library."),
            Item(question: "A game runs slowly or the phone gets hot",
                 answer: "Close other apps and let the phone cool. Games run natively, but some do work on the CPU that a real Android phone does on hardware. "
                       + "Low Power Mode slows Husk down too."),
            Item(question: "Can I use a controller?",
                 answer: "Yes. Connect it to your iPhone in Bluetooth settings, and games that support controllers see it."),
            Item(question: "Can I use Husk while a game runs?",
                 answer: "Turn on **Open Apps in Windows** in Settings. Apps then open in a window you can move, resize and minimise."),
            Item(question: "Sign-in with Google does not work",
                 answer: "Husk has no Google Play services, so “Sign in with Google” buttons can fail. Use email or another sign-in if the app offers one."),
        ]),
        Section(title: "Saves and Storage", symbol: "externaldrive.fill", items: [
            Item(question: "Where are my saves?",
                 answer: "Each app keeps its data inside Husk. On an app's page, **Back Up Saves** exports them and **Restore Saves** brings them back, "
                       + "including onto another iPhone."),
            Item(question: "Husk is using a lot of storage",
                 answer: "Imported apps and their data live in Husk's folder. Remove games you no longer play from their page. "
                       + "The full Android system download is about 2 GB more and is only needed for Android itself."),
        ]),
        Section(title: "Android", symbol: "cpu", items: [
            Item(question: "What is the difference between apps and Android?",
                 answer: "Most apps and games run directly on your iPhone, the way native apps do. The full Android system is a separate, "
                       + "slower option for anything that only runs inside real Android."),
            Item(question: "Android takes a long time to start",
                 answer: "The first start downloads the Android image and boots it from cold, which takes a few minutes. "
                       + "After that Husk restores a saved machine, which is much faster."),
        ]),
    ]
}
