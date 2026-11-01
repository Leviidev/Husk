// SPDX-License-Identifier: GPL-2.0-or-later
// What iOS 15 lacks of the SwiftUI Husk uses (#92, #131: TrollStore reaches iOS 15). On iOS 16 and later each of these is the
// system's own; on iOS 15 a stand-in that looks and behaves close enough. LabeledContent and NavigationStack are defined here
// under SwiftUI's names, so the views that use them need no change; the rest are modifiers and views with Husk's own names.
import SwiftUI
import UIKit

// MARK: - LabeledContent

/// A label with its value on the right, as SwiftUI's LabeledContent draws it in a Form.
struct LabeledContent<Label: View, Content: View>: View {
    private let label: Label
    private let content: Content

    init(@ViewBuilder content: () -> Content, @ViewBuilder label: () -> Label) {
        self.label = label()
        self.content = content()
    }

    var body: some View {
        if #available(iOS 16, *) {
            SwiftUI.LabeledContent { content } label: { label }
        } else {
            HStack(alignment: .firstTextBaseline) {
                label
                Spacer(minLength: 12)
                content.foregroundStyle(.secondary).multilineTextAlignment(.trailing)
            }
        }
    }
}

extension LabeledContent where Label == Text, Content == Text {
    init<Title: StringProtocol, Value: StringProtocol>(_ title: Title, value: Value) {
        self.init(content: { Text(value) }, label: { Text(title) })
    }
}

extension LabeledContent where Label == Text {
    init<Title: StringProtocol>(_ title: Title, @ViewBuilder content: () -> Content) {
        self.init(content: content, label: { Text(title) })
    }
}

// MARK: - NavigationStack

/// The pushed screens of an iOS 15 stack: the path, as values, and how to make each value's screen.
final class HuskNavigation: ObservableObject {
    @Published var path: [AnyHashable] = []
    var destinations: [ObjectIdentifier: (AnyHashable) -> AnyView?] = [:]
    /// A binding the stack was given: changes go both ways.
    var external: (get: () -> [AnyHashable], set: ([AnyHashable]) -> Void)?

    func push(_ value: AnyHashable) { setPath(path + [value]) }
    func setPath(_ new: [AnyHashable]) {
        path = new
        external?.set(new)
    }
    func screen(for value: AnyHashable) -> AnyView {
        let typed = ObjectIdentifier(type(of: value.base))
        if let view = destinations[typed]?(value) { return view }
        // NavigationView on iOS 15 sometimes boxes the path value so type(of:) misses;
        // try every registered destination (each returns nil when the type is wrong).
        for build in destinations.values {
            if let view = build(value) { return view }
        }
        return AnyView(
            Text("This page could not be opened.")
                .foregroundStyle(.secondary)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
        )
    }
}

private struct HuskNavigationKey: EnvironmentKey { static let defaultValue: HuskNavigation? = nil }
extension EnvironmentValues {
    var huskNavigation: HuskNavigation? {
        get { self[HuskNavigationKey.self] }
        set { self[HuskNavigationKey.self] = newValue }
    }
}

/// SwiftUI's NavigationStack on iOS 16 and later; on iOS 15 a NavigationView whose pushed screens follow the same path, one hidden
/// link per level.
struct NavigationStack<Root: View>: View {
    private let root: Root
    private let modern: () -> AnyView
    private let pathGet: (() -> [AnyHashable])?
    private let pathSet: (([AnyHashable]) -> Void)?
    @StateObject private var navigation = HuskNavigation()

    init(@ViewBuilder root: () -> Root) {
        let r = root()
        self.root = r
        modern = {
            if #available(iOS 16, *) { return AnyView(SwiftUI.NavigationStack { r }) }
            return AnyView(r)
        }
        pathGet = nil
        pathSet = nil
    }

    init<Element: Hashable>(path: Binding<[Element]>, @ViewBuilder root: () -> Root) {
        let r = root()
        self.root = r
        modern = {
            if #available(iOS 16, *) { return AnyView(SwiftUI.NavigationStack(path: path) { r }) }
            return AnyView(r)
        }
        pathGet = { path.wrappedValue.map { AnyHashable($0) } }
        pathSet = { path.wrappedValue = $0.compactMap { $0.base as? Element } }
    }

    var body: some View {
        if #available(iOS 16, *) {
            modern()
        } else {
            // iOS 15 (#159, #181): hidden NavigationLinks (isActive) pushed empty screens there, so the top of the path is shown
            // instead, in a navigation view of its own over the root, with a Back button that pops it
            ZStack {
                // NavigationView does not pass custom environment values to its root on iOS 15.
                NavigationView {
                    root.environment(\.huskNavigation, navigation)
                }
                .navigationViewStyle(.stack)
                if let top = navigation.path.last {
                    NavigationView {
                        navigation.screen(for: top)
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                            .toolbar {
                                ToolbarItem(placement: .navigationBarLeading) {
                                    Button { navigation.setPath(Array(navigation.path.dropLast())) } label: {
                                        Label("Back", systemImage: "chevron.left").labelStyle(.titleAndIcon)
                                    }
                                }
                            }
                            .environment(\.huskNavigation, navigation)
                    }
                    .navigationViewStyle(.stack)
                    .background(Console.page.ignoresSafeArea())
                    .id(navigation.path.count)
                    .transition(.move(edge: .trailing))
                    .zIndex(Double(navigation.path.count))
                }
            }
            .animation(.easeOut(duration: 0.25), value: navigation.path.count)
            .environment(\.huskNavigation, navigation)
            .onAppear {
                if let pathGet, let pathSet {
                    navigation.external = (pathGet, pathSet)
                    navigation.path = pathGet()
                }
            }
            .onChange(of: pathGet?() ?? []) { new in
                if new != navigation.path { navigation.path = new }
            }
        }
    }
}

/// One pushed screen of an iOS 15 stack, and the link to the next.
private struct HuskNavigationLevel: View {
    let level: Int
    @ObservedObject var navigation: HuskNavigation

    var body: some View {
        NavigationLink(isActive: Binding(get: { navigation.path.count > level },
                                         set: { active in
                                             if !active, navigation.path.count > level { navigation.setPath(Array(navigation.path.prefix(level))) }
                                         })) {
            if navigation.path.count > level {
                navigation.screen(for: navigation.path[level])
                    .background(HuskNavigationLevel(level: level + 1, navigation: navigation))
                    .environment(\.huskNavigation, navigation)
            }
        } label: { EmptyView() }
        .hidden()
    }
}

/// Records an iOS 15 stack's screen for a type of value, as navigationDestination(for:) does.
private struct HuskDestinationRegistrar<D: Hashable, Screen: View>: ViewModifier {
    @Environment(\.huskNavigation) private var navigation
    let builder: (D) -> Screen

    func body(content: Content) -> some View {
        register()
        return content.onAppear(perform: register)
    }

    private func register() {
        navigation?.destinations[ObjectIdentifier(D.self)] = { value in
            (value.base as? D).map { AnyView(builder($0)) }
        }
    }
}

extension View {
    /// navigationDestination(for:destination:), which iOS 15 does not have.
    @ViewBuilder
    func huskDestination<D: Hashable, Screen: View>(for type: D.Type, @ViewBuilder destination: @escaping (D) -> Screen) -> some View {
        if #available(iOS 16, *) {
            navigationDestination(for: type, destination: destination)
        } else {
            modifier(HuskDestinationRegistrar(builder: destination))
        }
    }
}

/// NavigationLink(value:label:): pushes a value onto the stack's path.
struct HuskLink<Value: Hashable, Label: View>: View {
    @Environment(\.huskNavigation) private var navigation
    let value: Value
    let label: Label

    init(value: Value, @ViewBuilder label: () -> Label) {
        self.value = value
        self.label = label()
    }

    var body: some View {
        if #available(iOS 16, *) {
            NavigationLink(value: value) { label }
        } else {
            Button { navigation?.push(AnyHashable(value)) } label: { label }
                .buttonStyle(.plain)
        }
    }
}

// MARK: - Sharing

/// ShareLink(item:subject:message:label:) for a file; on iOS 15 the share sheet.
struct HuskShareLink<Label: View>: View {
    let item: URL
    let subject: Text?
    let message: Text?
    let label: Label
    @State private var sharing = false

    init(item: URL, subject: Text? = nil, message: Text? = nil, @ViewBuilder label: () -> Label) {
        self.item = item
        self.subject = subject
        self.message = message
        self.label = label()
    }

    var body: some View {
        if #available(iOS 16, *) {
            ShareLink(item: item, subject: subject ?? Text(""), message: message ?? Text("")) { label }
        } else {
            Button { sharing = true } label: { label }
                .sheet(isPresented: $sharing) { HuskActivitySheet(items: [item]) }
        }
    }
}

private struct HuskActivitySheet: UIViewControllerRepresentable {
    let items: [Any]
    func makeUIViewController(context: Context) -> UIActivityViewController { UIActivityViewController(activityItems: items, applicationActivities: nil) }
    func updateUIViewController(_ controller: UIActivityViewController, context: Context) {}
}

// MARK: - Modifiers

enum HuskDetent: Hashable { case medium, large, height(CGFloat) }

extension View {
    @ViewBuilder
    func huskDetents(_ detents: Set<HuskDetent>, dragIndicator: Bool = false) -> some View {
        if #available(iOS 16, *) {
            presentationDetents(Set(detents.map { d -> PresentationDetent in
                switch d { case .medium: return .medium; case .large: return .large; case .height(let h): return .height(h) }
            }))
                .presentationDragIndicator(dragIndicator ? .visible : .automatic)
        } else {
            self
        }
    }

    @ViewBuilder
    func huskHideSystemOverlays(_ hide: Bool = true) -> some View {
        if #available(iOS 16, *) { persistentSystemOverlays(hide ? .hidden : .automatic) } else { self }
    }

    @ViewBuilder
    func huskDefersSystemGestures(on edges: Edge.Set) -> some View {
        if #available(iOS 16, *) { defersSystemGestures(on: edges) } else { self }
    }

    @ViewBuilder
    func huskHideScrollIndicators() -> some View {
        if #available(iOS 16, *) { scrollIndicators(.hidden) } else { self }
    }

    @ViewBuilder
    func huskDismissKeyboardOnScroll() -> some View {
        if #available(iOS 16, *) { scrollDismissesKeyboard(.interactively) } else { self }
    }

    @ViewBuilder
    func huskNavigationBar(hidden: Bool) -> some View {
        if #available(iOS 16, *) { toolbar(hidden ? .hidden : .visible, for: .navigationBar) } else { navigationBarHidden(hidden) }
    }

    @ViewBuilder
    func huskBold() -> some View {
        if #available(iOS 16, *) { bold() } else { font(.body.weight(.semibold)) }
    }
}

extension Color {
    /// Color.gradient, which iOS 15 does not have: the plain colour there.
    var huskGradient: AnyShapeStyle {
        if #available(iOS 16, *) { return AnyShapeStyle(gradient) }
        return AnyShapeStyle(self)
    }
}

extension Locale {
    /// The two-letter language and region, from the API each iOS version has.
    var huskLanguageCode: String? {
        if #available(iOS 16, *) { return language.languageCode?.identifier }
        return languageCode
    }
    var huskRegionCode: String? {
        if #available(iOS 16, *) { return region?.identifier }
        return regionCode
    }
}

// MARK: - pages

/// A page's List on Husk's navy: the page colour behind it and cards for its rows (iOS 15 keeps the system's grouped
/// background, which it cannot hide).
struct HuskPageList<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        if #available(iOS 16, *) {
            List { Group { content }.listRowBackground(Console.card) }
                .scrollContentBackground(.hidden)
                .background(Console.page.ignoresSafeArea())
        } else {
            List { content }
        }
    }
}

/// The same for a Form: settings pages on the navy too.
struct HuskForm<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        if #available(iOS 16, *) {
            Form { Group { content }.listRowBackground(Console.card) }
                .scrollContentBackground(.hidden)
                .background(Console.page.ignoresSafeArea())
        } else {
            Form { content }
        }
    }
}
