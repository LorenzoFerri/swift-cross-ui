#if !os(WASI)
    import Foundation
#endif

/// A scene that presents an app-level tray/status bar icon with a menu.
public struct StatusItem<MenuContent: View>: Scene {
    public typealias Node = StatusItemNode<MenuContent>

    let id: String
    var title: String
    var icon: StatusItemIcon?
    var tooltip: String?
    var menu: () -> MenuContent
    var primaryAction: (@MainActor () -> Void)?

    /// Creates a status item scene.
    ///
    /// - Parameters:
    ///   - title: The title used by backends when no icon is available, or as an
    ///     accessibility-oriented fallback.
    ///   - id: A stable identifier for the status item.
    ///   - icon: The icon to display.
    ///   - tooltip: The tooltip to show when supported by the backend.
    ///   - menu: The status item's menu content.
    ///   - primaryAction: An action to perform for a primary click when supported
    ///     by the backend.
    public init(
        _ title: String,
        id: String? = nil,
        icon: StatusItemIcon? = nil,
        tooltip: String? = nil,
        @ViewBuilder menu: @escaping () -> MenuContent,
        primaryAction: (@MainActor () -> Void)? = nil
    ) {
        self.id = id ?? title
        self.title = title
        self.icon = icon
        self.tooltip = tooltip
        self.menu = menu
        self.primaryAction = primaryAction
    }
}

/// The scene graph node for ``StatusItem``.
public final class StatusItemNode<MenuContent: View>: SceneGraphNode {
    public typealias NodeScene = StatusItem<MenuContent>

    private var scene: StatusItem<MenuContent>
    private var trayIcon: Any?
    private var removeTrayIcon: (@MainActor () -> Void)?

    public init<Backend: BaseAppBackend>(
        from scene: StatusItem<MenuContent>,
        backend: Backend,
        environment: EnvironmentValues
    ) {
        self.scene = scene
    }

    deinit {
        MainActor.assumeIsolated {
            removeTrayIcon?()
        }
    }

    public func updateNode(
        _ newScene: NodeScene?,
        environment: EnvironmentValues
    ) -> SceneNodeUpdateResult {
        if let newScene {
            self.scene = newScene
        }

        return .leafScene()
    }

    public func update<Backend: BaseAppBackend>(
        backend: Backend,
        environment: EnvironmentValues
    ) {
        guard let backend = backend as? any BackendFeatures.TrayIcons else {
            logger.warnOnce(
                "\(Backend.self) does not support StatusItem scenes; the status item will not be shown"
            )
            return
        }

        updateTrayIcon(
            backend: backend,
            environment: environment
        )
    }

    private func updateTrayIcon<Backend: BackendFeatures.TrayIcons>(
        backend: Backend,
        environment: EnvironmentValues
    ) {
        let concreteTrayIcon: Backend.TrayIcon

        if let existingTrayIcon = trayIcon as? Backend.TrayIcon {
            concreteTrayIcon = existingTrayIcon
        } else {
            removeTrayIcon?()

            let createdTrayIcon = backend.createTrayIcon(id: scene.id)
            trayIcon = createdTrayIcon
            removeTrayIcon = { [backend] in
                backend.removeTrayIcon(createdTrayIcon)
            }
            concreteTrayIcon = createdTrayIcon
        }

        backend.updateTrayIcon(
            concreteTrayIcon,
            title: scene.title,
            icon: scene.icon,
            tooltip: scene.tooltip,
            menu: Menu.resolve(items: scene.menu()._asMenuItems),
            primaryAction: scene.primaryAction,
            environment: environment
        )
    }
}
