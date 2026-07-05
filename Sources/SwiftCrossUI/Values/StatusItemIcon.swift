import Foundation

/// An icon for a ``StatusItem``.
public enum StatusItemIcon: Sendable, Equatable {
    /// An icon identified by a platform-specific resource or asset name.
    case named(String)
    /// An icon loaded from a file URL.
    case file(URL)
}
