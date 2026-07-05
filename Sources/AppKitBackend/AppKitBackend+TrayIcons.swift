import AppKit
import SwiftCrossUI

extension AppKitBackend: BackendFeatures.TrayIcons {
    public final class TrayIcon {
        let statusItem: NSStatusItem
        var primaryActionWrapper: Action?

        init(statusItem: NSStatusItem) {
            self.statusItem = statusItem
        }
    }

    public func createTrayIcon(id _: String) -> TrayIcon {
        TrayIcon(
            statusItem: NSStatusBar.system.statusItem(
                withLength: NSStatusItem.variableLength
            )
        )
    }

    public func updateTrayIcon(
        _ trayIcon: TrayIcon,
        title: String,
        icon: StatusItemIcon?,
        tooltip: String?,
        menu content: ResolvedMenu,
        primaryAction: (@MainActor () -> Void)?,
        environment: EnvironmentValues
    ) {
        let statusItem = trayIcon.statusItem

        if let button = statusItem.button {
            if let image = renderStatusItemIcon(icon) {
                statusItem.length = NSStatusItem.squareLength
                button.imageScaling = .scaleProportionallyDown
                button.image = image
                button.title = ""
            } else {
                statusItem.length = NSStatusItem.variableLength
                button.image = nil
                button.title = title
            }

            button.toolTip = tooltip
        }

        let menu = NSMenu()
        menu.appearance = environment.colorScheme.nsAppearance
        menu.items = content.items.map {
            Self.renderMenuItem($0, environment: environment)
        }

        if menu.items.isEmpty {
            statusItem.menu = nil

            if let primaryAction, let button = statusItem.button {
                let wrapper = Action(primaryAction)
                trayIcon.primaryActionWrapper = wrapper
                button.target = wrapper
                button.action = #selector(wrapper.run)
            } else if let button = statusItem.button {
                trayIcon.primaryActionWrapper = nil
                button.target = nil
                button.action = nil
            }
        } else {
            trayIcon.primaryActionWrapper = nil
            statusItem.button?.target = nil
            statusItem.button?.action = nil
            statusItem.menu = menu
        }
    }

    public func removeTrayIcon(_ trayIcon: TrayIcon) {
        NSStatusBar.system.removeStatusItem(trayIcon.statusItem)
    }

    private func renderStatusItemIcon(_ icon: StatusItemIcon?) -> NSImage? {
        switch icon {
            case nil:
                return nil
            case .named(let name):
                return prepareStatusItemImage(NSImage(named: NSImage.Name(name)))
            case .file(let url):
                return prepareStatusItemImage(NSImage(contentsOf: url))
        }
    }

    private func prepareStatusItemImage(_ image: NSImage?) -> NSImage? {
        guard let image else {
            return nil
        }

        image.size = NSSize(width: 26, height: 26)
        return image
    }
}
