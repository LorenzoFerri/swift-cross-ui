import DefaultBackend
import Foundation
import SwiftCrossUI

#if canImport(SwiftBundlerRuntime)
    import SwiftBundlerRuntime
#endif

@main
struct StatusItemExample: App {
    @State var count = 0

    @Environment(\.openWindow) var openWindow

    var body: some Scene {
        Window("Counter", id: "counter") {
            VStack {
                Text("Counter")
                    .font(.system(size: 22))
                Text("\(count)")
                    .font(.system(size: 42))
                HStack {
                    Button("-") { count -= 1 }
                    Button("Reset") { count = 0 }
                    Button("+") { count += 1 }
                }
            }
            .padding()
        }
        .defaultLaunchBehavior(.suppressed)

        StatusItem(
            "Counter",
            icon: .file(URL(fileURLWithPath: "Examples/Icons/MusicPlayerExample.png")),
            tooltip: "Counter: \(count)"
        ) {
            Text("Count: \(count)")
            Button("Show Counter") { openWindow(id: "counter") }
            Divider()
            Button("Increment") { count += 1 }
            Button("Decrement") { count -= 1 }
            Button("Reset") { count = 0 }
            Divider()
            Button("Quit") { Foundation.exit(0) }
        } primaryAction: {
            openWindow(id: "counter")
        }
    }
}
