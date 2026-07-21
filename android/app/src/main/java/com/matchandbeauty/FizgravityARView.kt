package com.matchandbeauty

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.SurfaceTexture
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.opengl.GLES11Ext
import android.opengl.GLES20
import android.opengl.GLSurfaceView
import android.util.AttributeSet
import android.util.Log
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import com.facebook.react.bridge.Arguments
import com.facebook.react.bridge.ReactContext
import com.facebook.react.bridge.WritableArray
import com.facebook.react.uimanager.ThemedReactContext
import com.facebook.react.uimanager.events.RCTEventEmitter
import com.google.mediapipe.framework.image.MediaImageBuilder
import com.google.mediapipe.tasks.core.BaseOptions
import com.google.mediapipe.tasks.core.Delegate
import com.google.mediapipe.tasks.vision.core.ImageProcessingOptions
import com.google.mediapipe.tasks.vision.core.RunningMode
import com.google.mediapipe.tasks.vision.facelandmarker.FaceLandmarker
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class FizgravityARView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : GLSurfaceView(context, attrs), GLSurfaceView.Renderer, SurfaceTexture.OnFrameAvailableListener, SensorEventListener {

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

    private external fun fizgravityInit(): Long
    private external fun fizgravityRelease(enginePtr: Long)
    private external fun fizgravityPushImu(
        enginePtr: Long, gx: Float, gy: Float, gz: Float,
        ax: Float, ay: Float, az: Float, timestampSec: Float
    ): Int
    private external fun fizgravitySetFaceMesh(
        enginePtr: Long, vertices: FloatArray, blendshapes: FloatArray
    ): Int
    private external fun fizgravityGetPredictedLandmarks(
        enginePtr: Long, dtPredict: Float
    ): FloatArray?

    private var cameraProvider: ProcessCameraProvider? = null
    private var surfaceTexture: SurfaceTexture? = null
    private var cameraTextureId = -1
    private val cameraExecutor: ExecutorService = Executors.newSingleThreadExecutor()
    private var updateTexture = false

    private var faceLandmarker: FaceLandmarker? = null
    private var fizgravityEnginePtr: Long = 0L
    private var sensorManager: SensorManager? = null

    @Volatile private var lastGyroX = 0f
    @Volatile private var lastGyroY = 0f
    @Volatile private var lastGyroZ = 0f
    @Volatile private var lastAccelX = 0f
    @Volatile private var lastAccelY = 0f
    @Volatile private var lastAccelZ = 9.81f

    private val startTimeNs = System.nanoTime()

    init {
        setEGLContextClientVersion(3)
        setRenderer(this)
        renderMode = RENDERMODE_WHEN_DIRTY

        initMediaPipe()
        initFizgravityEngine()
        initSensorManager()
    }

    private fun initMediaPipe() {
        try {
            val baseOptions = BaseOptions.builder()
                .setModelAssetPath("face_landmarker.task")
                .setDelegate(Delegate.GPU)
                .build()
            val opts = FaceLandmarker.FaceLandmarkerOptions.builder()
                .setBaseOptions(baseOptions)
                .setMinFaceDetectionConfidence(0.35f)
                .setMinFacePresenceConfidence(0.35f)
                .setMinTrackingConfidence(0.35f)
                .setOutputFaceBlendshapes(true)
                .setRunningMode(RunningMode.VIDEO)
                .build()
            faceLandmarker = FaceLandmarker.createFromOptions(context, opts)
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

            gyro?.let { sensorManager?.registerListener(this, it, SensorManager.SENSOR_DELAY_FASTEST) }
            accel?.let { sensorManager?.registerListener(this, it, SensorManager.SENSOR_DELAY_FASTEST) }
        } catch (e: Exception) {
            Log.e("FizgravityARView", "SensorManager init error: ${e.message}")
        }
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
                fizgravityPushImu(ptr, lastGyroX, lastGyroY, lastGyroZ, lastAccelX, lastAccelY, lastAccelZ, tsSeconds)
            } catch (e: Exception) {}
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        FizgravityRenderer.nativeInitGL()
        cameraTextureId = createCameraTexture()
        surfaceTexture = SurfaceTexture(cameraTextureId).apply {
            setOnFrameAvailableListener(this@FizgravityARView)
        }
        post { startCamera() }
    }

    private val transformMatrix = FloatArray(16)

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        GLES20.glViewport(0, 0, width, height)
        FizgravityRenderer.nativeResize(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        if (updateTexture) {
            surfaceTexture?.updateTexImage()
            surfaceTexture?.getTransformMatrix(transformMatrix)
            updateTexture = false
        }
        val ptr = fizgravityEnginePtr
        var predicted: FloatArray? = null
        if (ptr != 0L && fizgravityLoaded) {
            // Predict based on approx render time horizon (Late Latching)
            predicted = fizgravityGetPredictedLandmarks(ptr, 0.016f)
        }
        FizgravityRenderer.nativeDrawFrame(cameraTextureId, predicted, transformMatrix)

        if (predicted != null) {
            sendEventToReactNative("onFaceDetected", predicted)
        }
    }

    override fun onFrameAvailable(surfaceTexture: SurfaceTexture?) {
        updateTexture = true
        requestRender()
    }

    @SuppressLint("UnsafeOptInUsageError")
    private fun startCamera() {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
        cameraProviderFuture.addListener({
            cameraProvider = cameraProviderFuture.get()
            
            // Fix blur: Request high resolution for the preview
            val resolutionSelector = androidx.camera.core.resolutionselector.ResolutionSelector.Builder()
                .setResolutionStrategy(androidx.camera.core.resolutionselector.ResolutionStrategy(
                    android.util.Size(1080, 1920), 
                    androidx.camera.core.resolutionselector.ResolutionStrategy.FALLBACK_RULE_CLOSEST_HIGHER_THEN_LOWER
                ))
                .build()

            val preview = Preview.Builder()
                .setResolutionSelector(resolutionSelector)
                .build().also {
                it.setSurfaceProvider { request ->
                    val surface = android.view.Surface(surfaceTexture)
                    request.provideSurface(surface, ContextCompat.getMainExecutor(context)) {
                        surface.release()
                    }
                }
            }

            val imageAnalysis = ImageAnalysis.Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .build()

            imageAnalysis.setAnalyzer(cameraExecutor) { imageProxy ->
                val landmarker = faceLandmarker
                if (landmarker != null && imageProxy.image != null) {
                    val rot = imageProxy.imageInfo.rotationDegrees
                    val mpImage = MediaImageBuilder(imageProxy.image!!).build()
                    val timestampMs = (System.nanoTime() - startTimeNs) / 1_000_000L
                    
                    try {
                        val result = landmarker.detectForVideo(
                            mpImage,
                            ImageProcessingOptions.builder().setRotationDegrees(rot).build(),
                            timestampMs
                        )
                        if (result.faceLandmarks().isNotEmpty()) {
                            val fl = result.faceLandmarks()[0]
                            val fa = FloatArray(fl.size * 3)
                            for (i in fl.indices) {
                                // Mirror horizontally: 1 - y()
                                fa[i * 3] = 1.0f - fl[i].y()
                                // 90 deg rotation
                                fa[i * 3 + 1] = 1.0f - fl[i].x()
                                fa[i * 3 + 2] = fl[i].z()
                            }
                            
                            val blendshapes = FloatArray(52)
                            if (result.faceBlendshapes().isPresent) {
                                val bs = result.faceBlendshapes().get()[0]
                                for (j in 0 until minOf(bs.size, 52)) {
                                    blendshapes[j] = bs[j].score()
                                }
                            }
                            
                            val ptr = fizgravityEnginePtr
                            if (ptr != 0L && fizgravityLoaded) {
                                fizgravitySetFaceMesh(ptr, fa, blendshapes)
                            }
                        }
                    } catch (e: Exception) {
                        Log.e("FizgravityARView", "MediaPipe analysis error: ${e.message}")
                    }
                }
                imageProxy.close()
            }

            val cameraSelector = CameraSelector.DEFAULT_FRONT_CAMERA
            try {
                cameraProvider?.unbindAll()
                val lifecycleOwner = getLifecycleOwner()
                if (lifecycleOwner != null) {
                    cameraProvider?.bindToLifecycle(lifecycleOwner, cameraSelector, preview, imageAnalysis)
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

    private fun sendEventToReactNative(eventName: String, landmarks: FloatArray) {
        val reactContext = (context as? ThemedReactContext) ?: return
        val array: WritableArray = Arguments.createArray()
        for (v in landmarks) {
            array.pushDouble(v.toDouble())
        }
        val event = Arguments.createMap()
        event.putArray("landmarks", array)
        reactContext.getJSModule(RCTEventEmitter::class.java)
            .receiveEvent(id, eventName, event)
    }

    private fun createCameraTexture(): Int {
        val textures = IntArray(1)
        GLES20.glGenTextures(1, textures, 0)
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textures[0])
        GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MIN_FILTER, GLES20.GL_LINEAR.toFloat())
        GLES20.glTexParameterf(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_MAG_FILTER, GLES20.GL_LINEAR.toFloat())
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_S, GLES20.GL_CLAMP_TO_EDGE)
        GLES20.glTexParameteri(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, GLES20.GL_TEXTURE_WRAP_T, GLES20.GL_CLAMP_TO_EDGE)
        return textures[0]
    }

    override fun onDetachedFromWindow() {
        super.onDetachedFromWindow()
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
