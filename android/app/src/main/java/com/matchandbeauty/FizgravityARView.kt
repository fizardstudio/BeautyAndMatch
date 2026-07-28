package com.matchandbeauty

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Rect
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
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
import com.google.mediapipe.framework.image.BitmapImageBuilder
import com.google.mediapipe.tasks.core.BaseOptions
import com.google.mediapipe.tasks.core.Delegate
import com.google.mediapipe.tasks.vision.core.RunningMode
import com.google.mediapipe.tasks.vision.facelandmarker.FaceLandmarker
import java.nio.ByteBuffer
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class FizgravityARView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs), GLSurfaceView.Renderer, SensorEventListener {

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
    private external fun fizgravityGetStabilizedLandmarks(
        enginePtr: Long
    ): FloatArray?
    private external fun fizgravityPushImu(
        enginePtr: Long, gx: Float, gy: Float, gz: Float, ax: Float, ay: Float, az: Float, timestampSec: Float
    ): Int

    private var cameraProvider: ProcessCameraProvider? = null
    private var cameraTextureId = -1
    private val cameraExecutor: ExecutorService = Executors.newSingleThreadExecutor()

    private var faceLandmarker: FaceLandmarker? = null
    private var fizgravityEnginePtr: Long = 0L

    private val engineLock = Any()

    // IMU Sensor fields
    @Volatile private var lastGyroX = 0f
    @Volatile private var lastGyroY = 0f
    @Volatile private var lastGyroZ = 0f
    @Volatile private var lastAccelX = 0f
    @Volatile private var lastAccelY = 0f
    @Volatile private var lastAccelZ = 9.81f
    private var sensorManager: SensorManager? = null
    private val startTimeNs = System.nanoTime()

    // Double Buffering for Timestamp Synchronization
    private var bufferA: ByteBuffer? = null
    private var bufferB: ByteBuffer? = null
    private var useBufferA = true

    // Downscaled tracking input for MediaPipe — the render/compositing pipeline
    // needs the full-res camera image, but the face landmark model doesn't: feeding
    // it the full 1920x1080 frame every call means MediaPipe pays a resize/preprocess
    // cost proportional to that size before it ever gets to its own small internal
    // input tensor. Track on a much smaller downscaled copy instead; render stays
    // full-res and untouched. Bitmaps are allocated once and reused every frame to
    // avoid per-frame GC pressure at 20-60fps.
    private var trackingSourceBitmap: Bitmap? = null
    private var trackingBitmap: Bitmap? = null
    private var trackingCanvas: Canvas? = null
    private val TRACKING_WIDTH = 640
    
    private val renderLock = Any()
    private var latestImageBuffer: ByteBuffer? = null
    private var latestImageWidth = 0
    private var latestImageHeight = 0
    private var latestImageRowStride = 0
    private var latestLandmarks: FloatArray? = null
    private var newLandmarksAvailable = false

    private var lastRenderedBuffer: ByteBuffer? = null
    private var lastRenderedWidth = 0
    private var lastRenderedHeight = 0
    private var lastRenderedRowStride = 0
    private var lastRenderedLandmarks: FloatArray? = null
    private var lastMediaPipeUpdateNs = 0L
    private var lastDrawNs = 0L

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
        // Diagnostic revert (temporary): back to WHEN_DIRTY — draw only when
        // requestRender() is explicitly called after a new camera frame is ready,
        // instead of forcing a redraw every ~33ms regardless of whether new data
        // arrived. This isolates whether CONTINUOUS+the 30fps software gate itself
        // is the source of "getar" (which happens even with no face in frame, so it
        // can't be landmark-prediction related), independent of every other fix
        // already applied (camera resolution, AE range, landmark path cleanup).
        renderMode = RENDERMODE_WHEN_DIRTY

        setupMediaPipe()
        initFizgravityEngine()
        initSensorManager()
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

    private fun initSensorManager() {
        try {
            sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as? SensorManager
            val gyro = sensorManager?.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
            val accel = sensorManager?.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
            if (gyro != null) sensorManager?.registerListener(this, gyro, SensorManager.SENSOR_DELAY_FASTEST)
            if (accel != null) sensorManager?.registerListener(this, accel, SensorManager.SENSOR_DELAY_FASTEST)
        } catch (e: Exception) {
            Log.e("FizgravityARView", "SensorManager init error: ${e.message}")
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
        // Cap the render loop at ~30fps instead of letting RENDERMODE_CONTINUOUS run
        // uncapped at the display's native vsync rate (~60fps). Uncapped rendering
        // competes with MediaPipe's GPU delegate for the same GPU, slowing tracking
        // down and making the whole pipeline feel laggier overall — 30fps is still a
        // large smoothness win over the original MediaPipe-coupled ~17fps while leaving
        // the GPU enough headroom for MediaPipe to run at its natural speed.
        val nowNsGate = System.nanoTime()
        if (nowNsGate - lastDrawNs < 33_000_000L) return
        lastDrawNs = nowNsGate

        var renderBuffer: ByteBuffer? = null
        var rWidth = 0
        var rHeight = 0
        var rStride = 0
        var rLandmarks: FloatArray? = null

        // Check for new camera frame
        synchronized(renderLock) {
            if (latestImageBuffer != null) {
                renderBuffer = latestImageBuffer
                rWidth = latestImageWidth
                rHeight = latestImageHeight
                rStride = latestImageRowStride
                rLandmarks = latestLandmarks
                newLandmarksAvailable = false
                latestImageBuffer = null

                // Cache this frame for interpolation on subsequent ticks
                lastRenderedBuffer = renderBuffer
                lastRenderedWidth = rWidth
                lastRenderedHeight = rHeight
                lastRenderedRowStride = rStride
                lastRenderedLandmarks = rLandmarks
                lastMediaPipeUpdateNs = System.nanoTime()
            }
        }

        // If we have a new frame, draw with it
        if (renderBuffer != null) {
            val drawStartNs = System.nanoTime()
            FizgravityRenderer.nativeDrawSyncFrame(
                cameraTextureId, renderBuffer!!, rWidth, rHeight, rStride, rLandmarks, hasNewImage = true
            )
            val drawMs = (System.nanoTime() - drawStartNs) / 1_000_000.0

            perfFrameCount++
            perfDrawMsSum += drawMs
            if (drawMs > perfDrawMsMax) perfDrawMsMax = drawMs
            trackJitter(rLandmarks, "real")

            if (pendingSnapshot) {
                pendingSnapshot = false
                saveSnapshotToGallery()
            }
        } else if (lastRenderedBuffer != null && lastRenderedLandmarks != null) {
            // No new frame yet: redraw the SAME frozen camera image with the SAME
            // last-real landmarks (no IMU-based extrapolation). Deliberately not
            // pushing the landmarks forward here — the camera texture underneath is
            // frozen (it's literally the last real frame's pixels), so moving the
            // makeup overlay ahead of it via prediction makes the overlay visibly
            // detach/float relative to the static face image beneath it every time
            // MediaPipe hasn't caught up yet. That mismatch — not landmark noise —
            // is what was reading as jitter. Keeping both frozen together means an
            // interpolation tick is visually identical to the last real tick.
            val drawStartNs = System.nanoTime()
            FizgravityRenderer.nativeDrawSyncFrame(
                cameraTextureId, lastRenderedBuffer!!, lastRenderedWidth, lastRenderedHeight, lastRenderedRowStride, lastRenderedLandmarks, hasNewImage = false
            )
            val drawMs = (System.nanoTime() - drawStartNs) / 1_000_000.0

            perfFrameCount++
            perfDrawMsSum += drawMs
            if (drawMs > perfDrawMsMax) perfDrawMsMax = drawMs
            trackJitter(lastRenderedLandmarks, "pred")
        }

        // Log perf stats for all frames (real and interpolated)
        val nowNs = System.nanoTime()
        val elapsedNs = nowNs - perfWindowStartNs
        if (elapsedNs >= 2_000_000_000L && perfFrameCount > 0) {
            val fps = perfFrameCount / (elapsedNs / 1_000_000_000.0)
            val avgDrawMs = perfDrawMsSum / perfFrameCount
            Log.i("FizgravityPerf", "fps=%.1f avgDrawMs=%.2f maxDrawMs=%.2f frames=$perfFrameCount".format(fps, avgDrawMs, perfDrawMsMax))
            perfFrameCount = 0
            perfDrawMsSum = 0.0
            perfDrawMsMax = 0.0
            perfWindowStartNs = nowNs
        }
    }

    private var perfFrameCount = 0
    private var perfDrawMsSum = 0.0
    private var perfDrawMsMax = 0.0
    private var perfWindowStartNs = System.nanoTime()

    // Objective per-vertex jitter measurement (RMS distance between this frame's
    // landmarks and the previous displayed frame's), broken down by whether each
    // frame came from a real MediaPipe detection or an interpolated IMU prediction,
    // and specifically flagging the transition frames (source changed since the
    // last displayed frame) — lets us tell apart "prediction is noisy in general"
    // from "the snap at the real<->predicted boundary is the problem" using actual
    // numbers instead of subjective "does it look shaky" reports.
    private var prevDisplayedLandmarks: FloatArray? = null
    private var prevDisplayedSource: String? = null
    private var jitterWindowStartNs = System.nanoTime()
    private var jitterRealSum = 0.0
    private var jitterRealMax = 0.0
    private var jitterRealCount = 0
    private var jitterPredSum = 0.0
    private var jitterPredMax = 0.0
    private var jitterPredCount = 0
    private var jitterTransitionSum = 0.0
    private var jitterTransitionMax = 0.0
    private var jitterTransitionCount = 0

    private fun trackJitter(landmarks: FloatArray?, source: String) {
        if (landmarks == null) return
        val prev = prevDisplayedLandmarks
        if (prev != null && prev.size == landmarks.size) {
            var sumSq = 0.0
            for (i in landmarks.indices) {
                val d = (landmarks[i] - prev[i]).toDouble()
                sumSq += d * d
            }
            val rms = kotlin.math.sqrt(sumSq / landmarks.size)
            val isTransition = prevDisplayedSource != null && prevDisplayedSource != source
            if (isTransition) {
                jitterTransitionSum += rms
                jitterTransitionCount++
                if (rms > jitterTransitionMax) jitterTransitionMax = rms
            } else if (source == "real") {
                jitterRealSum += rms
                jitterRealCount++
                if (rms > jitterRealMax) jitterRealMax = rms
            } else {
                jitterPredSum += rms
                jitterPredCount++
                if (rms > jitterPredMax) jitterPredMax = rms
            }
        }
        prevDisplayedLandmarks = landmarks
        prevDisplayedSource = source

        val nowNs = System.nanoTime()
        if (nowNs - jitterWindowStartNs >= 2_000_000_000L) {
            val realAvg = if (jitterRealCount > 0) jitterRealSum / jitterRealCount else 0.0
            val predAvg = if (jitterPredCount > 0) jitterPredSum / jitterPredCount else 0.0
            val transAvg = if (jitterTransitionCount > 0) jitterTransitionSum / jitterTransitionCount else 0.0
            Log.i(
                "FizgravityJitter",
                "real(avg=%.5f max=%.5f n=%d) pred(avg=%.5f max=%.5f n=%d) transition(avg=%.5f max=%.5f n=%d)".format(
                    realAvg, jitterRealMax, jitterRealCount,
                    predAvg, jitterPredMax, jitterPredCount,
                    transAvg, jitterTransitionMax, jitterTransitionCount
                )
            )
            jitterRealSum = 0.0; jitterRealMax = 0.0; jitterRealCount = 0
            jitterPredSum = 0.0; jitterPredMax = 0.0; jitterPredCount = 0
            jitterTransitionSum = 0.0; jitterTransitionMax = 0.0; jitterTransitionCount = 0
            jitterWindowStartNs = nowNs
        }
    }

    private var perfMpCount = 0
    private var perfMpMsSum = 0.0
    private var perfMpMsMax = 0.0
    private var perfMpWindowStartNs = System.nanoTime()

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
                // Target is given in the sensor's native (landscape) coordinate space,
                // matching how Camera2's stream configuration list reports sizes —
                // confirmed via `adb shell dumpsys media.camera` that this device's
                // front sensor exposes an exact 1920x1080 stream. Previously this was
                // Size(1080, 1920) (portrait-shaped), which does NOT match that space;
                // CLOSEST_HIGHER_THEN_LOWER then measured "closest" against the wrong
                // orientation and fell back all the way to 2304x1296 (~3MP, 44% more
                // pixels) instead of the intended ~2MP capture. That inflated MediaPipe
                // inference time (measured avg ~50ms, spikes to ~140ms) and, in turn,
                // destabilized the One-Euro filter's dt-based velocity estimate.
                .setResolutionStrategy(androidx.camera.core.resolutionselector.ResolutionStrategy(
                    android.util.Size(1920, 1080),
                    androidx.camera.core.resolutionselector.ResolutionStrategy.FALLBACK_RULE_CLOSEST_HIGHER_THEN_LOWER
                ))
                // Without an explicit AspectRatioStrategy, CameraX defaults to
                // RATIO_4_3_FALLBACK_AUTO_STRATEGY and silently overrides the
                // resolution preference above, picking a 4:3 stream (e.g. the
                // sensor's full 2304x1728) instead of something close to our
                // 16:9 target.
                .setAspectRatioStrategy(androidx.camera.core.resolutionselector.AspectRatioStrategy(
                    androidx.camera.core.AspectRatio.RATIO_16_9,
                    androidx.camera.core.resolutionselector.AspectRatioStrategy.FALLBACK_RULE_AUTO
                ))
                .build()

            val imageAnalysisBuilder = ImageAnalysis.Builder()
                .setResolutionSelector(resolutionSelector)
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888)

            // Ask for a flexible 10-30fps AE range via Camera2Interop instead of leaving it
            // fully unconstrained. A rigid fixed 30fps range forces short exposure regardless
            // of scene brightness and can underexpose to near-black in anything less than
            // strong direct light; this flexible range still lets AE drop fps for exposure
            // in dim conditions while permitting up to 30fps when there's enough light.
            // This device has no OIS hardware (availableOpticalStabilization=[0] per
            // Camera2 characteristics) but does support digital/electronic stabilization
            // (availableVideoStabilizationModes includes ON). The stock camera app almost
            // certainly has this on by default for its Preview; this app was never
            // requesting it at all, leaving raw handheld shake fully visible — worse here
            // than in the stock app because the tight selfie framing amplifies any shake.
            androidx.camera.camera2.interop.Camera2Interop.Extender(imageAnalysisBuilder)
                .setCaptureRequestOption(
                    android.hardware.camera2.CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE,
                    android.util.Range(10, 30)
                )
                .setCaptureRequestOption(
                    android.hardware.camera2.CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE,
                    android.hardware.camera2.CaptureRequest.CONTROL_VIDEO_STABILIZATION_MODE_ON
                )
                // Verify the HAL actually honors the requested mode instead of silently
                // ignoring an unsupported/rejected key (which would show no error at all).
                .setSessionCaptureCallback(object : android.hardware.camera2.CameraCaptureSession.CaptureCallback() {
                    private var lastLogNs = 0L
                    override fun onCaptureCompleted(
                        session: android.hardware.camera2.CameraCaptureSession,
                        request: android.hardware.camera2.CaptureRequest,
                        result: android.hardware.camera2.TotalCaptureResult
                    ) {
                        val now = System.nanoTime()
                        if (now - lastLogNs >= 2_000_000_000L) {
                            lastLogNs = now
                            val applied = result.get(android.hardware.camera2.CaptureResult.CONTROL_VIDEO_STABILIZATION_MODE)
                            Log.i("FizgravityPerf", "stabilizationMode requested=ON(1) appliedByHAL=$applied")
                        }
                    }
                })

            val imageAnalysis = imageAnalysisBuilder.build()

            imageAnalysis.setAnalyzer(cameraExecutor) { imageProxy ->
                val landmarker = faceLandmarker
                if (landmarker != null && imageProxy.image != null) {
                    val plane0 = imageProxy.planes[0]
                    val srcWidth = imageProxy.width
                    val srcHeight = imageProxy.height
                    val bufferWidth = plane0.rowStride / plane0.pixelStride

                    if (trackingSourceBitmap == null || trackingSourceBitmap!!.width != bufferWidth || trackingSourceBitmap!!.height != srcHeight) {
                        trackingSourceBitmap = Bitmap.createBitmap(bufferWidth, srcHeight, Bitmap.Config.ARGB_8888)
                    }
                    // duplicate() so this read doesn't disturb plane0.buffer's own
                    // position/limit — the existing code further below reads the
                    // SAME underlying Plane's buffer again via remaining() to size
                    // the GL upload buffer, and that must see it untouched.
                    val trackingSrcBuffer = plane0.buffer.duplicate()
                    trackingSrcBuffer.rewind()
                    trackingSourceBitmap!!.copyPixelsFromBuffer(trackingSrcBuffer)

                    if (trackingBitmap == null) {
                        val trackingHeight = (srcHeight.toFloat() * TRACKING_WIDTH / srcWidth).toInt()
                        trackingBitmap = Bitmap.createBitmap(TRACKING_WIDTH, trackingHeight, Bitmap.Config.ARGB_8888)
                        trackingCanvas = Canvas(trackingBitmap!!)
                    }
                    trackingCanvas!!.drawBitmap(
                        trackingSourceBitmap!!,
                        Rect(0, 0, srcWidth, srcHeight),
                        Rect(0, 0, trackingBitmap!!.width, trackingBitmap!!.height),
                        null
                    )

                    val mpImage = BitmapImageBuilder(trackingBitmap!!).build()
                    val timestampMs = imageProxy.imageInfo.timestamp / 1_000_000L
                    
                    try {
                        val mpStartNs = System.nanoTime()
                        val result = landmarker.detectForVideo(mpImage, timestampMs)
                        val mpMs = (System.nanoTime() - mpStartNs) / 1_000_000.0
                        perfMpMsSum += mpMs
                        perfMpCount++
                        if (mpMs > perfMpMsMax) perfMpMsMax = mpMs
                        val nowNs2 = System.nanoTime()
                        if (nowNs2 - perfMpWindowStartNs >= 2_000_000_000L) {
                            Log.i("FizgravityPerf", "mediapipe avgMs=%.2f maxMs=%.2f captureRes=${imageProxy.width}x${imageProxy.height} trackRes=${trackingBitmap?.width}x${trackingBitmap?.height} samples=$perfMpCount".format(perfMpMsSum / perfMpCount, perfMpMsMax))
                            perfMpMsSum = 0.0
                            perfMpMsMax = 0.0
                            perfMpCount = 0
                            perfMpWindowStartNs = nowNs2
                        }

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

                            // Extract blendshapes for engine stabilization
                            val blendshapes = FloatArray(52)
                            if (result.faceBlendshapes().isPresent) {
                                val bs = result.faceBlendshapes().get()[0]
                                for (j in 0 until minOf(bs.size, 52)) {
                                    blendshapes[j] = bs[j].score()
                                }
                            }

                            // Feed raw landmarks to Fizgravity engine and read back the
                            // One-Euro-stabilized result. Deliberately NOT going through
                            // fizgravityGetPredictedLandmarks here — that call also applies
                            // RK4 gyro-rotation extrapolation + a 50% blend against the last
                            // predicted frame, which injects raw IMU noise into every single
                            // real detection. That extrapolation path exists to bridge gaps
                            // between detections (used in onDrawFrame's interpolation branch
                            // when there's no new camera frame) — applying it here too was
                            // the actual source of the "bergetar" jitter, and explains why
                            // tuning the One-Euro filter parameters alone had no effect.
                            var finalLandmarks: FloatArray = fa // fallback: raw MediaPipe landmarks
                            val enginePtr = fizgravityEnginePtr
                            if (enginePtr != 0L && fizgravityLoaded) {
                                try {
                                    synchronized(engineLock) {
                                        fizgravitySetFaceMesh(enginePtr, fa, blendshapes)
                                    }
                                    val stabilized = synchronized(engineLock) {
                                        fizgravityGetStabilizedLandmarks(enginePtr)
                                    }
                                    if (stabilized != null && stabilized.size == fa.size) {
                                        finalLandmarks = stabilized
                                    }
                                } catch (e: Exception) {
                                    Log.w("FizgravityARView", "Engine call failed, using MediaPipe direct: ${e.message}")
                                }
                            }
                            rawLandmarks = finalLandmarks

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

    override fun onSensorChanged(event: SensorEvent) {
        when (event.sensor.type) {
            Sensor.TYPE_GYROSCOPE -> {
                lastGyroX = event.values[0]
                lastGyroY = event.values[1]
                lastGyroZ = event.values[2]
            }
            Sensor.TYPE_ACCELEROMETER -> {
                lastAccelX = event.values[0]
                lastAccelY = event.values[1]
                lastAccelZ = event.values[2]
            }
        }
        val ptr = fizgravityEnginePtr
        if (ptr != 0L && fizgravityLoaded) {
            val tsSeconds = (event.timestamp - startTimeNs) / 1_000_000_000f
            try {
                synchronized(engineLock) {
                    fizgravityPushImu(ptr, lastGyroX, lastGyroY, lastGyroZ, lastAccelX, lastAccelY, lastAccelZ, tsSeconds)
                }
            } catch (e: Exception) { /* silent: hot path, don't log every call */ }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
        try { cameraProvider?.unbindAll() } catch (e: Exception) {}
        cameraExecutor.shutdown()
        try { sensorManager?.unregisterListener(this) } catch (e: Exception) {}
        val ptr = fizgravityEnginePtr
        if (ptr != 0L && fizgravityLoaded) {
            try { fizgravityRelease(ptr) } catch (e: Exception) {}
            fizgravityEnginePtr = 0L
        }
        try { faceLandmarker?.close() } catch (e: Exception) {}
    }
}
