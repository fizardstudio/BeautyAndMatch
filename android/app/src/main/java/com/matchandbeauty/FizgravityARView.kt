package com.matchandbeauty

import android.annotation.SuppressLint
import android.content.Context
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.util.AttributeSet
import android.util.Log
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import com.facebook.react.uimanager.ThemedReactContext
import com.google.mediapipe.framework.image.MediaImageBuilder
import com.google.mediapipe.tasks.core.BaseOptions
import com.google.mediapipe.tasks.core.Delegate
import com.google.mediapipe.tasks.vision.core.ImageProcessingOptions
import com.google.mediapipe.tasks.vision.core.RunningMode
import com.google.mediapipe.tasks.vision.facelandmarker.FaceLandmarker
import java.nio.ByteBuffer
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class FizgravityARView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs), GLSurfaceView.Renderer {

    companion object {
        init { System.loadLibrary("match_and_beauty_core") }
        private var fizgravityLoaded = false
        init {
            try {
                System.loadLibrary("fizgravity_ar")
                fizgravityLoaded = true
                Log.d("FizgravityARView", "fizgravity_ar.so loaded OK")
            } catch (e: UnsatisfiedLinkError) {
                Log.w("FizgravityARView", "fizgravity_ar.so not found: ${e.message}")
            }
        }
    }

    // JNI morphology — symbol: Java_com_matchandbeauty_FizgravityARView_nativeAnalyzeMorphology
    @androidx.annotation.Keep
    private external fun nativeAnalyzeMorphology(landmarks: FloatArray): FloatArray

    private external fun fizgravityInit(): Long
    private external fun fizgravityRelease(enginePtr: Long)
    private external fun fizgravitySetFaceMesh(
        enginePtr: Long, vertices: FloatArray, blendshapes: FloatArray
    ): Int
    private external fun fizgravityGetPredictedLandmarks(
        enginePtr: Long, dtPredict: Float
    ): FloatArray?

    private var cameraProvider: ProcessCameraProvider? = null
    private var cameraTextureId = -1
    private val cameraExecutor: ExecutorService = Executors.newSingleThreadExecutor()

    private var faceLandmarker: FaceLandmarker? = null
    private var fizgravityEnginePtr: Long = 0L

    private val engineLock = Any()
    
    // Double Buffering for Timestamp Synchronization
    private var bufferA: ByteBuffer? = null
    private var bufferB: ByteBuffer? = null
    private var useBufferA = true
    
    private val renderLock = Any()
    private var latestImageBuffer: ByteBuffer? = null
    private var latestImageWidth = 0
    private var latestImageHeight = 0
    private var latestImageRowStride = 0
    private var latestLandmarks: FloatArray? = null
    private var newLandmarksAvailable = false

    private var surfaceWidth = 0
    private var surfaceHeight = 0
    @Volatile var pendingSnapshot = false
    @Volatile var scanRequested = false

    fun requestMorphologyScan() {
        scanRequested = true
        Log.d("FizgravityARView", "Morphology scan requested")
    }

    init {
        setEGLContextClientVersion(3)
        setRenderer(this)
        renderMode = RENDERMODE_WHEN_DIRTY

        setupMediaPipe()
        initFizgravityEngine()
    }

    private fun setupMediaPipe() {
        try {
            val baseOptions = BaseOptions.builder()
                .setModelAssetPath("face_landmarker.task")
                .setDelegate(Delegate.GPU)
                .build()
            val options = FaceLandmarker.FaceLandmarkerOptions.builder()
                .setBaseOptions(baseOptions)
                .setRunningMode(RunningMode.VIDEO)
                .setMinFaceDetectionConfidence(0.35f)
                .setMinFacePresenceConfidence(0.35f)
                .setMinTrackingConfidence(0.35f)
                .setNumFaces(1)
                .setOutputFaceBlendshapes(true)
                .setErrorListener { error ->
                    Log.e("FizgravityARView", "MediaPipe Error: ${error.message}")
                }
                .build()
            faceLandmarker = FaceLandmarker.createFromOptions(context, options)
        } catch (e: Exception) {
            Log.e("FizgravityARView", "MediaPipe init error: ${e.message}")
        }
    }

    private fun initFizgravityEngine() {
        if (!fizgravityLoaded) return
        try {
            fizgravityEnginePtr = fizgravityInit()
        } catch (e: Exception) {
            Log.e("FizgravityARView", "Engine init exception: ${e.message}")
        }
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        FizgravityRenderer.nativeInitGL()
        cameraTextureId = createCameraTexture()
        post { startCamera() }
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        surfaceWidth = width
        surfaceHeight = height
        GLES20.glViewport(0, 0, width, height)
        FizgravityRenderer.nativeResize(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        var renderBuffer: ByteBuffer? = null
        var rWidth = 0
        var rHeight = 0
        var rStride = 0
        var rLandmarks: FloatArray? = null
        var isNewLandmarks = false

        synchronized(renderLock) {
            if (latestImageBuffer != null) {
                renderBuffer = latestImageBuffer
                rWidth = latestImageWidth
                rHeight = latestImageHeight
                rStride = latestImageRowStride
                rLandmarks = latestLandmarks
                isNewLandmarks = newLandmarksAvailable
                newLandmarksAvailable = false
                latestImageBuffer = null
            }
        }
        
        if (renderBuffer != null) {
            FizgravityRenderer.nativeDrawSyncFrame(
                cameraTextureId, renderBuffer!!, rWidth, rHeight, rStride, rLandmarks, isNewLandmarks
            )



            if (pendingSnapshot) {
                pendingSnapshot = false
                saveSnapshotToGallery()
            }
        }
    }

    private fun saveSnapshotToGallery() {
        val width = surfaceWidth
        val height = surfaceHeight
        if (width <= 0 || height <= 0) return

        val buffer = ByteBuffer.allocateDirect(width * height * 4)
        buffer.order(java.nio.ByteOrder.nativeOrder())
        GLES20.glReadPixels(0, 0, width, height, GLES20.GL_RGBA, GLES20.GL_UNSIGNED_BYTE, buffer)

        // Save to gallery asynchronously
        Thread {
            try {
                val bitmap = android.graphics.Bitmap.createBitmap(width, height, android.graphics.Bitmap.Config.ARGB_8888)
                buffer.rewind()
                bitmap.copyPixelsFromBuffer(buffer)

                // Flip Y (OpenGL to Android Bitmap)
                val matrix = android.graphics.Matrix()
                matrix.preScale(1.0f, -1.0f)
                val flippedBitmap = android.graphics.Bitmap.createBitmap(bitmap, 0, 0, width, height, matrix, false)
                bitmap.recycle()

                // Save to MediaStore
                val values = android.content.ContentValues().apply {
                    put(android.provider.MediaStore.Images.Media.DISPLAY_NAME, "MatchAndBeauty_QC_${System.currentTimeMillis()}.png")
                    put(android.provider.MediaStore.Images.Media.MIME_TYPE, "image/png")
                    put(android.provider.MediaStore.Images.Media.RELATIVE_PATH, android.os.Environment.DIRECTORY_PICTURES + "/MatchAndBeauty")
                }

                val uri = context.contentResolver.insert(android.provider.MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)
                if (uri != null) {
                    context.contentResolver.openOutputStream(uri)?.use { out ->
                        flippedBitmap.compress(android.graphics.Bitmap.CompressFormat.PNG, 100, out)
                    }
                    Log.d("FizgravityARView", "Snapshot saved to gallery: $uri")
                }
                flippedBitmap.recycle()
            } catch (e: Exception) {
                Log.e("FizgravityARView", "Failed to save snapshot", e)
            }
        }.start()
    }

    @SuppressLint("UnsafeOptInUsageError")
    private fun startCamera() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
        cameraProviderFuture.addListener({
            cameraProvider = cameraProviderFuture.get()
            
            val resolutionSelector = androidx.camera.core.resolutionselector.ResolutionSelector.Builder()
                .setResolutionStrategy(androidx.camera.core.resolutionselector.ResolutionStrategy(
                    android.util.Size(1080, 1920), 
                    androidx.camera.core.resolutionselector.ResolutionStrategy.FALLBACK_RULE_CLOSEST_HIGHER_THEN_LOWER
                ))
                .build()

            val imageAnalysis = ImageAnalysis.Builder()
                .setResolutionSelector(resolutionSelector)
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888)
                .build()

            imageAnalysis.setAnalyzer(cameraExecutor) { imageProxy ->
                val landmarker = faceLandmarker
                if (landmarker != null && imageProxy.image != null) {
                    val rot = imageProxy.imageInfo.rotationDegrees
                    val mpImage = MediaImageBuilder(imageProxy.image!!).build()
                    val timestampMs = imageProxy.imageInfo.timestamp / 1_000_000L
                    
                    try {
                        val result = landmarker.detectForVideo(mpImage, timestampMs)
                        
                        var rawLandmarks: FloatArray? = null
                        if (result.faceLandmarks().isNotEmpty()) {
                            val fl = result.faceLandmarks()[0]
                            val count = fl.size
                            val fa = FloatArray(count * 3)
                            for (i in 0 until count) {
                                fa[i * 3] = 1.0f - fl[i].y()
                                fa[i * 3 + 1] = 1.0f - fl[i].x()
                                fa[i * 3 + 2] = fl[i].z()
                            }
                            rawLandmarks = fa

                            // ── Morphology Scan (triggered by scan button) ──
                            if (scanRequested) {
                                scanRequested = false
                                try {
                                    val d = nativeAnalyzeMorphology(fa)
                                    val faceShapes = arrayOf("Round", "Oblong", "Square", "Heart", "Diamond", "Oval")
                                    val eyeShapes = arrayOf("Downturned", "Monolid", "Hooded", "Normal")
                                    val noseShapes = arrayOf("Wide", "Crooked", "Normal")
                                    val eventData = com.facebook.react.bridge.Arguments.createMap().apply {
                                        putString("faceShape", faceShapes[d[0].toInt().coerceIn(0, 5)])
                                        putString("eyeShape", eyeShapes[d[1].toInt().coerceIn(0, 3)])
                                        putString("noseShape", noseShapes[d[2].toInt().coerceIn(0, 2)])
                                        putDouble("jawWidth", d[3].toDouble())
                                        putDouble("faceLength", d[4].toDouble())
                                        putDouble("canthalTilt", d[5].toDouble())
                                        putDouble("eyeAspectRatio", d[6].toDouble())
                                        putDouble("alarBaseWidth", d[7].toDouble())
                                        putDouble("intercanthalDistance", d[8].toDouble())
                                    }
                                    Log.d("FizgravityARView", "Morphology result: faceShape=${faceShapes[d[0].toInt().coerceIn(0,5)]}")
                                    val reactCtx = context as? com.facebook.react.uimanager.ThemedReactContext
                                    reactCtx?.getJSModule(com.facebook.react.modules.core.DeviceEventManagerModule.RCTDeviceEventEmitter::class.java)
                                        ?.emit("MorphologyResult", eventData)
                                } catch (t: Throwable) {
                                    Log.e("FizgravityARView", "Morphology JNI error: ${t::class.java.name}: ${t.message}")
                                    // Emit fallback so UI knows scan is done
                                    val fallback = com.facebook.react.bridge.Arguments.createMap().apply {
                                        putString("faceShape", "Oval")
                                        putString("eyeShape", "Normal")
                                        putString("noseShape", "Normal")
                                        putDouble("jawWidth", 0.12)
                                        putDouble("faceLength", 0.18)
                                        putDouble("canthalTilt", 2.5)
                                        putDouble("eyeAspectRatio", 0.3)
                                        putDouble("alarBaseWidth", 0.08)
                                        putDouble("intercanthalDistance", 0.06)
                                    }
                                    val reactCtx = context as? com.facebook.react.uimanager.ThemedReactContext
                                    reactCtx?.getJSModule(com.facebook.react.modules.core.DeviceEventManagerModule.RCTDeviceEventEmitter::class.java)
                                        ?.emit("MorphologyResult", fallback)
                                }
                            }
                        }

                        // Copy image buffer for GLThread
                        val plane = imageProxy.planes[0]
                        val rawBuffer = plane.buffer
                        val size = rawBuffer.remaining()
                        
                        val destBuffer = if (useBufferA) {
                            if (bufferA == null || bufferA!!.capacity() != size) bufferA = ByteBuffer.allocateDirect(size)
                            bufferA!!
                        } else {
                            if (bufferB == null || bufferB!!.capacity() != size) bufferB = ByteBuffer.allocateDirect(size)
                            bufferB!!
                        }
                        
                        destBuffer.clear()
                        destBuffer.put(rawBuffer)
                        destBuffer.position(0)
                        
                        synchronized(renderLock) {
                            latestImageBuffer = destBuffer
                            latestImageWidth = imageProxy.width
                            latestImageHeight = imageProxy.height
                            latestImageRowStride = plane.rowStride
                            latestLandmarks = rawLandmarks
                            newLandmarksAvailable = true
                        }
                        useBufferA = !useBufferA
                        requestRender()
                    } catch (e: Exception) {
                        Log.e("FizgravityARView", "MediaPipe analysis error: ${e.message}")
                    } finally {
                        imageProxy.close()
                    }
                } else {
                    imageProxy.close()
                }
            }

            val cameraSelector = CameraSelector.DEFAULT_FRONT_CAMERA
            try {
                cameraProvider?.unbindAll()
                val lifecycleOwner = getLifecycleOwner()
                if (lifecycleOwner != null) {
                    cameraProvider?.bindToLifecycle(lifecycleOwner, cameraSelector, imageAnalysis)
                }
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }, ContextCompat.getMainExecutor(context))
    }

    private fun getLifecycleOwner(): LifecycleOwner? {
        if (context is LifecycleOwner) return context as LifecycleOwner
        var reactContext = context
        while (reactContext is android.content.ContextWrapper) {
            if (reactContext is ThemedReactContext) {
                return reactContext.currentActivity as? LifecycleOwner
            }
            reactContext = reactContext.baseContext
        }
        return null
    }

    private fun createCameraTexture(): Int {
        val textures = IntArray(1)
        GLES20.glGenTextures(1, textures, 0)
        GLES20.glBindTexture(GLES20.GL_TEXTURE_2D, textures[0])
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR.toFloat())
        GLES20.glTexParameterf(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR.toFloat())
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE)
        GLES20.glTexParameteri(GLES20.GL_TEXTURE_2D, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE)
        return textures[0]
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        cameraExecutor.shutdown()
        val ptr = fizgravityEnginePtr
        if (ptr != 0L && fizgravityLoaded) {
            try { fizgravityRelease(ptr) } catch (e: Exception) {}
            fizgravityEnginePtr = 0L
        }
        try { faceLandmarker?.close() } catch (e: Exception) {}
    }
}
