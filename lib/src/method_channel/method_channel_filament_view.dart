import 'package:filament/src/platform_interface/filament_view_platform.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter/gestures.dart';
import 'package:flutter/rendering.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';

class MethodChannelFilamentView extends FilamentViewPlatform {
  @override
  Widget buildView(
    int creationId,
    PlatformViewCreatedCallback onPlatformViewCreated,
  ) {
    const filamentViewType = 'filament.flutter.thepipecat.com/filament_view';

    switch (defaultTargetPlatform) {
      case TargetPlatform.android:
        return PlatformViewLink(
          viewType: filamentViewType,
          surfaceFactory:
              (BuildContext context, PlatformViewController controller) {
                print("Got controller $controller");
            return AndroidViewSurface(
              controller: controller as AndroidViewController,
              gestureRecognizers: const <
                  Factory<OneSequenceGestureRecognizer>>{},
              hitTestBehavior: PlatformViewHitTestBehavior.opaque,
            );
          },
          onCreatePlatformView: (PlatformViewCreationParams params) {
            print("CREATED PLATFORM VIEW");
            return PlatformViewsService.initSurfaceAndroidView(
              id: params.id,
              viewType: filamentViewType,
              layoutDirection: TextDirection.ltr,
              creationParams: {},
              creationParamsCodec: StandardMessageCodec(),
            )
              ..addOnPlatformViewCreatedListener(params.onPlatformViewCreated)
              ..create();
          },
        );
      case TargetPlatform.iOS:
        return UiKitView(viewType: filamentViewType);
      default:
        return Text(
            '$defaultTargetPlatform is not yet implemented by Filament plugin.');
    }
  }
}
