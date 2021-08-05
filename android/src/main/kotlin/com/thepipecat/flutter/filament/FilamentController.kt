package com.thepipecat.flutter.filament

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Color
import android.graphics.SurfaceTexture
import android.hardware.Camera
import android.opengl.GLSurfaceView
import android.view.SurfaceView
import android.view.View
import android.widget.TextView
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.lifecycle.DefaultLifecycleObserver
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import io.flutter.plugin.platform.PlatformView
import java.io.IOException

class FilamentController(
    private val viewId: Int,
    private val context: Context,
    private val activity: Activity,
    private val binaryMessenger: BinaryMessenger,
) : DefaultLifecycleObserver, MethodChannel.MethodCallHandler, PlatformView {

    companion object {
        const val TAG = "FilamentController"
    }

    override fun getView(): View = _view

    private val _methodChannel: MethodChannel
    private val _view: SurfaceView

    private var test:Test

    init {
        MethodChannel(binaryMessenger, FilamentPlugin.VIEW_TYPE + '_' + viewId).also {
            _methodChannel = it
            it.setMethodCallHandler(this)
        }

        _view = SurfaceView(context)
        test = Test(_view, activity)
    }

    override fun dispose() {
        _methodChannel.setMethodCallHandler(null)
    }

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            else -> Unit
        }
    }

}
