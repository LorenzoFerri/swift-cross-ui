import Foundation
import Testing

import DummyBackend
@testable import SwiftCrossUI

@MainActor
private var dummyTrayRecords: [ObjectIdentifier: DummyTrayRecord] = [:]

@MainActor
private final class DummyTrayRecord {
    var createdTrayIcons: [DummyBackend.TrayIcon] = []
    var removedTrayIconIDs: [String] = []
}

extension DummyBackend: BackendFeatures.TrayIcons {
    public final class TrayIcon {
        var id: String
        var updates: [Update] = []

        init(id: String) {
            self.id = id
        }
    }

    public struct Update {
        var title: String
        var icon: StatusItemIcon?
        var tooltip: String?
        var menu: ResolvedMenu
        var primaryAction: (@MainActor () -> Void)?
        var environment: EnvironmentValues
    }

    private var trayRecord: DummyTrayRecord {
        let id = ObjectIdentifier(self)
        if let record = dummyTrayRecords[id] {
            return record
        }

        let record = DummyTrayRecord()
        dummyTrayRecords[id] = record
        return record
    }

    fileprivate var createdTrayIcons: [TrayIcon] {
        trayRecord.createdTrayIcons
    }

    fileprivate var removedTrayIconIDs: [String] {
        trayRecord.removedTrayIconIDs
    }

    public func createTrayIcon(id: String) -> TrayIcon {
        let trayIcon = TrayIcon(id: id)
        trayRecord.createdTrayIcons.append(trayIcon)
        return trayIcon
    }

    public func updateTrayIcon(
        _ trayIcon: TrayIcon,
        title: String,
        icon: StatusItemIcon?,
        tooltip: String?,
        menu: ResolvedMenu,
        primaryAction: (@MainActor () -> Void)?,
        environment: EnvironmentValues
    ) {
        trayIcon.updates.append(
            Update(
                title: title,
                icon: icon,
                tooltip: tooltip,
                menu: menu,
                primaryAction: primaryAction,
                environment: environment
            )
        )
    }

    public func removeTrayIcon(_ trayIcon: TrayIcon) {
        trayRecord.removedTrayIconIDs.append(trayIcon.id)
    }
}

@Suite("Testing for StatusItem scenes")
struct StatusItemSceneTests {
    @Test("StatusItem creates one tray icon and reuses it across refreshes")
    @MainActor
    func statusItemReusesTrayIcon() {
        let backend = DummyBackend()
        let environment = EnvironmentValues(backend: backend)
        let scene = StatusItem("Counter", id: "counter", tooltip: "Count: 0") {
            Button("Show Counter") {}
        }

        let node = StatusItemNode(from: scene, backend: backend, environment: environment)
        node.update(backend: backend, environment: environment)
        _ = node.updateNode(scene, environment: environment)
        node.update(backend: backend, environment: environment)

        #expect(backend.createdTrayIcons.count == 1)
        #expect(backend.createdTrayIcons[0].id == "counter")
        #expect(backend.createdTrayIcons[0].updates.count == 2)
        #expect(backend.createdTrayIcons[0].updates[0].title == "Counter")
        #expect(backend.createdTrayIcons[0].updates[0].tooltip == "Count: 0")
    }

    @Test("StatusItem resolves buttons, toggles, dividers, text, and nested menus")
    @MainActor
    func statusItemResolvesMenu() throws {
        let backend = DummyBackend()
        let environment = EnvironmentValues(backend: backend)
        var enabled = true
        let binding = Binding<Bool>(
            get: { enabled },
            set: { enabled = $0 }
        )

        let scene = StatusItem("Counter", icon: .named("counter-tray")) {
            Text("Count: 0")
            Button("Show Counter") {}
            Toggle("Enabled", isOn: binding)
            Divider()
            Menu("More") {
                Button("Quit") {}
            }
        }

        let node = StatusItemNode(from: scene, backend: backend, environment: environment)
        node.update(backend: backend, environment: environment)

        let update = try #require(backend.createdTrayIcons.first?.updates.first)
        #expect(update.icon == .named("counter-tray"))
        #expect(update.menu.items.count == 5)

        guard case .button("Count: 0", nil) = update.menu.items[0] else {
            Issue.record("Expected text to resolve as a disabled button")
            return
        }
        guard case .button("Show Counter", .some) = update.menu.items[1] else {
            Issue.record("Expected button item")
            return
        }
        guard case .toggle("Enabled", true, let onChange) = update.menu.items[2] else {
            Issue.record("Expected toggle item")
            return
        }
        onChange(false)
        #expect(enabled == false)
        guard case .separator = update.menu.items[3] else {
            Issue.record("Expected separator item")
            return
        }
        guard case .submenu(let submenu) = update.menu.items[4] else {
            Issue.record("Expected submenu item")
            return
        }
        #expect(submenu.label == "More")
        #expect(submenu.content.items.count == 1)
    }

    @Test("StatusItem removes its tray icon on deinit")
    @MainActor
    func statusItemRemovesTrayIconOnDeinit() {
        let backend = DummyBackend()
        let environment = EnvironmentValues(backend: backend)
        let scene = StatusItem("Counter", id: "counter") {
            Button("Show Counter") {}
        }

        var node: StatusItemNode? = StatusItemNode(
            from: scene,
            backend: backend,
            environment: environment
        )
        node?.update(backend: backend, environment: environment)
        #expect(backend.removedTrayIconIDs.isEmpty)

        node = nil

        #expect(backend.removedTrayIconIDs == ["counter"])
    }
}
