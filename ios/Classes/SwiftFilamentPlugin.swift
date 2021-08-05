import Flutter
import UIKit

public class SwiftFilamentPlugin: NSObject, FlutterPlugin {

  static let PLATFORM_VIEW_TYPE = "filament.flutter.thepipecat.com/filament_view"

  public static func register(with registrar: FlutterPluginRegistrar) {
    let channel = FlutterMethodChannel(name: "filament", binaryMessenger: registrar.messenger())
    let instance = SwiftFilamentPlugin()
    registrar.addMethodCallDelegate(instance, channel: channel)
    let factory = FilamentNativeViewFactory(messenger: registrar.messenger)
    registrar.register(factory, withId: "<platform-view-type>")
  }

  public func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    result("iOS " + UIDevice.current.systemVersion)
  }
}
