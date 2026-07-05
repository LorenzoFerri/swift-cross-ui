import CGtk
import Foundation
import GtkCHelpers
import SwiftCrossUI

extension GtkBackend: BackendFeatures.TrayIcons {
    public final class TrayIcon {
        let id: String
        var statusNotifierItem: OpaquePointer?
        var primaryAction: (@MainActor () -> Void)?
        var menuItemActions: [Int32: @MainActor () -> Void] = [:]
        var releaseApplication: (() -> Void)?

        init(id: String) {
            self.id = id
        }

        deinit {
            if let statusNotifierItem {
                scui_status_notifier_item_free(statusNotifierItem)
            }
            releaseApplication?()
        }
    }

    public func createTrayIcon(id: String) -> TrayIcon {
        let trayIcon = TrayIcon(id: id)
        g_application_hold(gtkApp.gobjectPointer.cast())
        trayIcon.releaseApplication = { [gtkApp] in
            g_application_release(gtkApp.gobjectPointer.cast())
        }
        trayIcon.statusNotifierItem = scui_status_notifier_item_new(
            id,
            id,
            { userData in
                guard let userData else {
                    return
                }

                let trayIcon = Unmanaged<TrayIcon>
                    .fromOpaque(userData)
                    .takeUnretainedValue()
                trayIcon.primaryAction?()
            },
            Unmanaged.passUnretained(trayIcon).toOpaque()
        )
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
        trayIcon.primaryAction = primaryAction

        let iconName: String?
        let iconPath: String?
        switch icon {
            case nil:
                iconName = nil
                iconPath = nil
            case .named(let name):
                iconName = name
                iconPath = nil
            case .file(let url):
                iconName = url.deletingPathExtension().lastPathComponent
                iconPath = url.deletingLastPathComponent().path
        }

        scui_status_notifier_item_update(
            trayIcon.statusNotifierItem,
            title,
            iconName,
            iconPath,
            tooltip
        )

        var nextMenuItemID: Int32 = 1
        var menuItems: [SCUIStatusNotifierMenuItem] = []
        var labels: [String] = []
        trayIcon.menuItemActions = [:]

        func addMenuItem(
            label: String,
            parentID: Int32 = 0,
            enabled: Bool = true,
            isSeparator: Bool = false,
            isToggle: Bool = false,
            toggleState: Bool = false,
            action: (@MainActor () -> Void)? = nil
        ) {
            let itemID = nextMenuItemID
            nextMenuItemID += 1
            labels.append(label)
            menuItems.append(
                SCUIStatusNotifierMenuItem(
                    id: Int32(itemID),
                    parent_id: parentID,
                    label: nil,
                    enabled: enabled ? 1 : 0,
                    visible: 1,
                    is_separator: isSeparator ? 1 : 0,
                    is_toggle: isToggle ? 1 : 0,
                    toggle_state: toggleState ? 1 : 0
                )
            )
            if let action {
                trayIcon.menuItemActions[itemID] = action
            }
        }

        for item in menu.items {
            renderTopLevelMenuItem(item, environment: environment, addMenuItem: addMenuItem)
        }

        labels.withUnsafeBufferPointer { labelBuffer in
            let cLabels: [UnsafeMutablePointer<CChar>?] = labelBuffer.map { strdup($0) }
            defer {
                for cLabel in cLabels {
                    free(UnsafeMutableRawPointer(mutating: cLabel))
                }
            }

            for index in menuItems.indices {
                menuItems[index].label = UnsafePointer(cLabels[index])
            }

            menuItems.withUnsafeBufferPointer { menuItemsBuffer in
                scui_status_notifier_item_set_menu(
                    trayIcon.statusNotifierItem,
                    menuItemsBuffer.baseAddress,
                    Int32(menuItemsBuffer.count),
                    { itemID, userData in
                        guard let userData else {
                            return
                        }

                        let trayIcon = Unmanaged<TrayIcon>
                            .fromOpaque(userData)
                            .takeUnretainedValue()
                        trayIcon.menuItemActions[Int32(itemID)]?()
                    },
                    Unmanaged.passUnretained(trayIcon).toOpaque()
                )
            }
        }
    }

    public func removeTrayIcon(_ trayIcon: TrayIcon) {
        scui_status_notifier_item_free(trayIcon.statusNotifierItem)
        trayIcon.statusNotifierItem = nil
        trayIcon.releaseApplication?()
        trayIcon.releaseApplication = nil
    }

    private func renderTopLevelMenuItem(
        _ item: ResolvedMenu.Item,
        environment: EnvironmentValues,
        addMenuItem: (
            _ label: String,
            _ parentID: Int32,
            _ enabled: Bool,
            _ isSeparator: Bool,
            _ isToggle: Bool,
            _ toggleState: Bool,
            _ action: (@MainActor () -> Void)?
        ) -> Void
    ) {
        switch item {
            case .button(let label, let action):
                addMenuItem(
                    label,
                    0,
                    environment.isEnabled && action != nil,
                    false,
                    false,
                    false,
                    action
                )
            case .toggle(let label, let value, let onChange):
                addMenuItem(label, 0, environment.isEnabled, false, true, value) {
                    onChange(!value)
                }
            case .separator:
                addMenuItem("", 0, true, true, false, false, nil)
            case .submenu(let submenu):
                addMenuItem(submenu.label, 0, false, false, false, false, nil)
            case .modifiedEnvironment(let item, let modification):
                renderTopLevelMenuItem(
                    item,
                    environment: modification(environment),
                    addMenuItem: addMenuItem
                )
        }
    }
}
