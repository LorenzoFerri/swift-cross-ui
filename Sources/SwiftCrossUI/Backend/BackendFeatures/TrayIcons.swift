extension BackendFeatures {
    /// Denotes a backend that supports desktop tray/status bar icons.
    @MainActor
    public protocol TrayIcons: Core {
        /// The native tray/status icon type.
        associatedtype TrayIcon

        /// Creates a tray/status icon.
        ///
        /// - Parameter id: A stable identifier for the scene node that owns the icon.
        func createTrayIcon(id: String) -> TrayIcon

        /// Updates a tray/status icon.
        func updateTrayIcon(
            _ trayIcon: TrayIcon,
            title: String,
            icon: StatusItemIcon?,
            tooltip: String?,
            menu: ResolvedMenu,
            primaryAction: (@MainActor () -> Void)?,
            environment: EnvironmentValues
        )

        /// Removes a tray/status icon.
        func removeTrayIcon(_ trayIcon: TrayIcon)
    }
}
