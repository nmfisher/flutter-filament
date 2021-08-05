/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.thepipecat.flutter.filament

import android.annotation.SuppressLint
import android.content.Context
import android.content.res.AssetManager
import android.app.Activity
import android.os.Bundle
import android.util.Log
import android.view.*
import android.view.GestureDetector
import android.widget.TextView
import android.widget.Toast
import com.google.android.filament.Fence
import com.google.android.filament.IndirectLight
import com.google.android.filament.Skybox
import com.google.android.filament.utils.*
import com.google.android.filament.utils.RemoteServer
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileInputStream
import java.io.RandomAccessFile
import java.nio.Buffer
import java.nio.ByteBuffer
import java.nio.charset.StandardCharsets
import java.util.zip.ZipInputStream

class ModelExample(val view:SurfaceView, val activity:Activity) {

    private var choreographer: Choreographer
    private val frameScheduler = FrameCallback()
    private var modelViewer: ModelViewer

    private var latestDownload: String? = null
    private val automation = AutomationEngine()
    private var loadStartTime = 0L
    private var loadStartFence: Fence? = null
    private val viewerContent = AutomationEngine.ViewerContent()

    companion object {
        // Load the library for the utility layer, which in turn loads gltfio and the Filament core.
        init { Utils.init() }
        private const val TAG = "gltf-viewer"
    }

    init {
        
        choreographer = Choreographer.getInstance()

        modelViewer = ModelViewer(view)
        viewerContent.view = modelViewer.view
        viewerContent.sunlight = modelViewer.light
        viewerContent.lightManager = modelViewer.engine.lightManager
        viewerContent.scene = modelViewer.scene
        viewerContent.renderer = modelViewer.renderer

        createDefaultRenderables()
        createIndirectLight()

        val view = modelViewer.view
        view.dynamicResolutionOptions = view.dynamicResolutionOptions.apply {
            enabled = true
        }

        view.ambientOcclusionOptions = view.ambientOcclusionOptions.apply {
            enabled = true
        }

        view.bloomOptions = view.bloomOptions.apply {
            enabled = true
        }
        choreographer.postFrameCallback(frameScheduler)
    }

    private fun createDefaultRenderables() {
        val buffer = activity.assets.open("flutter_assets/assets/scene.gltf").use { input ->
            val bytes = ByteArray(input.available())
            input.read(bytes)
            ByteBuffer.wrap(bytes)
        }

        modelViewer.loadModelGltfAsync(buffer) { uri -> readCompressedAsset("flutter_assets/assets/$uri") }
        updateRootTransform()
    }

    private fun createIndirectLight() {
        val engine = modelViewer.engine
        val scene = modelViewer.scene
        val ibl = "default_env"
        readCompressedAsset("flutter_assets/assets/$ibl/${ibl}_ibl.ktx").let {
            scene.indirectLight = KTXLoader.createIndirectLight(engine, it)
            scene.indirectLight!!.intensity = 30_000.0f
            viewerContent.indirectLight = modelViewer.scene.indirectLight
        }
        readCompressedAsset("flutter_assets/assets/$ibl/${ibl}_skybox.ktx").let {
            scene.skybox = KTXLoader.createSkybox(engine, it)
        }
    }

    private fun readCompressedAsset(assetName: String): ByteBuffer {
        val input = activity.assets.open(assetName)
        val bytes = ByteArray(input.available())
        input.read(bytes)
        return ByteBuffer.wrap(bytes)
    }

    private suspend fun loadGlb(message: RemoteServer.ReceivedMessage) {
        withContext(Dispatchers.Main) {
            modelViewer.destroyModel()
            modelViewer.loadModelGlb(message.buffer)
            updateRootTransform()
            loadStartTime = System.nanoTime()
            loadStartFence = modelViewer.engine.createFence()
        }
    }

    private suspend fun loadHdr(message: RemoteServer.ReceivedMessage) {
        withContext(Dispatchers.Main) {
            val engine = modelViewer.engine
            val equirect = HDRLoader.createTexture(engine, message.buffer)
            if (equirect == null) {

            } else {
                val context = IBLPrefilterContext(engine)
                val equirectToCubemap = IBLPrefilterContext.EquirectangularToCubemap(context)
                val skyboxTexture = equirectToCubemap.run(equirect)!!
                engine.destroyTexture(equirect)

                val specularFilter = IBLPrefilterContext.SpecularFilter(context)
                val reflections = specularFilter.run(skyboxTexture)

                val ibl = IndirectLight.Builder()
                         .reflections(reflections)
                         .intensity(30000.0f)
                         .build(engine)

                val sky = Skybox.Builder().environment(skyboxTexture).build(engine)

                modelViewer.scene.skybox = sky
                modelViewer.scene.indirectLight = ibl
            }
        }
    }

    private fun updateRootTransform() {
        if (automation.viewerOptions.autoScaleEnabled) {
            modelViewer.transformToUnitCube()
        } else {
            modelViewer.clearRootTransform()
        }
    }

    inner class FrameCallback : Choreographer.FrameCallback {
        private val startTime = System.nanoTime()
        override fun doFrame(frameTimeNanos: Long) {
            choreographer.postFrameCallback(this)

            loadStartFence?.let {
                if (it.wait(Fence.Mode.FLUSH, 0) == Fence.FenceStatus.CONDITION_SATISFIED) {
                    val end = System.nanoTime()
                    val total = (end - loadStartTime) / 1_000_000
                    Log.i(TAG, "The Filament backend took $total ms to load the model geometry.")
                    modelViewer.engine.destroyFence(it)
                    loadStartFence = null
                }
            }

            modelViewer.animator?.apply {
                if (animationCount > 0) {
                    val elapsedTimeSeconds = (frameTimeNanos - startTime).toDouble() / 1_000_000_000
                    applyAnimation(0, elapsedTimeSeconds.toFloat())
                }
                updateBoneMatrices()
            }

            modelViewer.render(frameTimeNanos)
        }
    }

}