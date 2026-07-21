package com.matchandbeauty

import android.content.Context
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.util.Log
import com.mrousavy.camera.core.types.Orientation
import com.mrousavy.camera.frameprocessors.Frame
import com.mrousavy.camera.frameprocessors.FrameProcessorPlugin
import com.google.mediapipe.tasks.vision.facelandmarker.FaceLandmarker
import com.google.mediapipe.tasks.vision.facelandmarker.FaceLandmarkerResult
import com.google.mediapipe.tasks.core.BaseOptions
import com.google.mediapipe.tasks.core.Delegate
import com.google.mediapipe.tasks.vision.core.RunningMode
import com.google.mediapipe.tasks.vision.facelandmarker.FaceLandmarker.FaceLandmarkerOptions
import com.google.mediapipe.tasks.vision.core.ImageProcessingOptions
import com.google.mediapipe.framework.image.MediaImageBuilder
import java.util.HashMap
import java.util.ArrayList
import java.util.concurrent.atomic.AtomicLong

/**
 * MediaPipeFrameProcessorPlugin — Versi 3.0 (Fizgravity IMU Fusion)
 *
 * Pipeline:
 * 1. SensorManager mengumpulkan Gyroscope + Accelerometer pada 200Hz
 * 2. Setiap measurement IMU di-push ke Fizgravity engine via JNI (non-blocking)
 * 3. Setiap frame kamera → MediaPipe deteksi 468 landmark
 * 4. Landmark dikirim ke Fizgravity engine (fizgravitySetFaceMesh) untuk:
 *    - Stabilisasi One-Euro Filter
 *    - Normalisasi koordinat
 *    - Pemetaan UV canonical
 * 5. Engine mengembalikan landmark yang sudah diprediksi (Late Latching + Rolling-Shutter)
 * 6. Data landmark stabilized dikembalikan ke React Native JS layer
 */
class MediaPipeFrameProcessorPlugin(
    private val context: Context,
    options: Map<String, Any>?
) : FrameProcessorPlugin(), SensorEventListener {

    companion object {
        // Load native library morfologi wajah (match_and_beauty_core)
        init { System.loadLibrary("match_and_beauty_core") }

        // Load Fizgravity AR Engine native library
        private var fizgravityLoaded = false
        init {
            try {
                System.loadLibrary("fizgravity_ar")
                fizgravityLoaded = true
                Log.d("FizgravityEngine", "fizgravity_ar.so loaded OK")
            } catch (e: UnsatisfiedLinkError) {
                Log.w("FizgravityEngine", "fizgravity_ar.so not found, running without engine: ${e.message}")
            }
        }
    }

    // ── Native JNI Declarations ──────────────────────────────────────────────
    // Morfologi wajah (C++ existing)
    private external fun nativeAnalyzeMorphology(landmarks: FloatArray): FloatArray

    // Fizgravity AR Engine (Rust FFI)
    private external fun fizgravityInit(): Long
    private external fun fizgravityRelease(enginePtr: Long)
    private external fun fizgravityPushImu(
        enginePtr: Long,
        gx: Float, gy: Float, gz: Float,
        ax: Float, ay: Float, az: Float,
        timestampSec: Float
    ): Int
    private external fun fizgravitySetFaceMesh(
        enginePtr: Long,
        vertices: FloatArray,
        blendshapes: FloatArray
    ): Int
    private external fun fizgravityGetPredictedLandmarks(
        enginePtr: Long,
        dtPredict: Float
    ): FloatArray?

    // ── State ────────────────────────────────────────────────────────────────
    private var faceLandmarker: FaceLandmarker? = null
    private var fizgravityEnginePtr: Long = 0L
    private var sensorManager: SensorManager? = null

    // Cache IMU terbaru (thread-safe via @Volatile)
    @Volatile private var lastGyroX = 0f
    @Volatile private var lastGyroY = 0f
    @Volatile private var lastGyroZ = 0f
    @Volatile private var lastAccelX = 0f
    @Volatile private var lastAccelY = 0f
    @Volatile private var lastAccelZ = 9.81f

    // Timestamp frame pertama sebagai referensi
    private val startTimeNs = System.nanoTime()
    private val frameCounter = AtomicLong(0)

    // ── Inisialisasi ─────────────────────────────────────────────────────────
    init {
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
            val opts = FaceLandmarkerOptions.builder()
                .setBaseOptions(baseOptions)
                .setMinFaceDetectionConfidence(0.35f)
                .setMinFacePresenceConfidence(0.35f)
                .setMinTrackingConfidence(0.35f)
                .setOutputFaceBlendshapes(true) // Aktifkan blendshapes untuk AO dinamis
                .setRunningMode(RunningMode.VIDEO)
                .build()
            faceLandmarker = FaceLandmarker.createFromOptions(context, opts)
            Log.d("MediaPipePlugin", "FaceLandmarker OK: VIDEO+GPU+Blendshapes")
        } catch (e: Exception) {
            Log.e("MediaPipePlugin", "MediaPipe init error: ${e.message}")
        }
    }

    private fun initFizgravityEngine() {
        if (!fizgravityLoaded) return
        try {
            fizgravityEnginePtr = fizgravityInit()
            if (fizgravityEnginePtr != 0L) {
                Log.d("FizgravityEngine", "Engine initialized OK, ptr=$fizgravityEnginePtr")
            } else {
                Log.e("FizgravityEngine", "Engine init returned null pointer")
            }
        } catch (e: Exception) {
            Log.e("FizgravityEngine", "Engine init exception: ${e.message}")
        }
    }

    private fun initSensorManager() {
        try {
            sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as? SensorManager
            val gyro = sensorManager?.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
            val accel = sensorManager?.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)

            if (gyro != null) {
                // SENSOR_DELAY_FASTEST = ~200Hz pada kebanyakan Android HP
                sensorManager?.registerListener(this, gyro, SensorManager.SENSOR_DELAY_FASTEST)
                Log.d("FizgravityEngine", "Gyroscope registered at FASTEST (≈200Hz)")
            } else {
                Log.w("FizgravityEngine", "Gyroscope sensor tidak tersedia di perangkat ini!")
            }

            if (accel != null) {
                sensorManager?.registerListener(this, accel, SensorManager.SENSOR_DELAY_FASTEST)
                Log.d("FizgravityEngine", "Accelerometer registered at FASTEST")
            }
        } catch (e: Exception) {
            Log.e("FizgravityEngine", "SensorManager init error: ${e.message}")
        }
    }

    // ── SensorEventListener ──────────────────────────────────────────────────

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

        // Push IMU ke Fizgravity engine (non-blocking, zero-allocation path)
        val ptr = fizgravityEnginePtr
        if (ptr != 0L && fizgravityLoaded) {
            val tsSeconds = (event.timestamp - startTimeNs) / 1_000_000_000f
            try {
                fizgravityPushImu(
                    ptr,
                    lastGyroX, lastGyroY, lastGyroZ,
                    lastAccelX, lastAccelY, lastAccelZ,
                    tsSeconds
                )
            } catch (e: Exception) {
                // Silent: jangan log di hot path (terlalu sering dipanggil)
            }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {
        // Tidak perlu penanganan khusus untuk akurasi sensor di sini
    }

    // ── Frame Processor Callback ─────────────────────────────────────────────

    override fun callback(frame: Frame, params: Map<String, Any>?): Any? {
        val landmarker = this.faceLandmarker ?: return null
        val image = frame.image ?: return null
        val frameIdx = frameCounter.incrementAndGet()

        return try {
            // 1. Deteksi rotasi kamera
            val orientation = try { frame.getOrientation() } catch (e: Exception) { Orientation.PORTRAIT }
            val rot = when (orientation) {
                Orientation.PORTRAIT -> 0
                Orientation.LANDSCAPE_LEFT -> 270
                Orientation.PORTRAIT_UPSIDE_DOWN -> 180
                Orientation.LANDSCAPE_RIGHT -> 90
                else -> 0
            }

            // 2. Jalankan MediaPipe Face Landmarker
            val mpImage = MediaImageBuilder(image).build()
            val timestampMs = (System.nanoTime() - startTimeNs) / 1_000_000L
            val processStartNs = System.nanoTime()
            val result: FaceLandmarkerResult = landmarker.detectForVideo(
                mpImage,
                ImageProcessingOptions.builder().setRotationDegrees(rot).build(),
                timestampMs
            )

            val resultMap = HashMap<String, Any>()

            if (result.faceLandmarks().isNotEmpty()) {
                val fl = result.faceLandmarks()[0]

                // 3. Bangun FloatArray vertices untuk Fizgravity
                val fa = FloatArray(fl.size * 3)
                for (i in fl.indices) {
                    val x = fl[i].y()
                    val y = 1.0f - fl[i].x()
                    val z = fl[i].z()
                    fa[i * 3] = x
                    fa[i * 3 + 1] = y
                    fa[i * 3 + 2] = z
                }

                // 4. Bangun FloatArray blendshapes (52 values, atau zeros jika tidak ada)
                val blendshapeList = if (result.faceBlendshapes().isPresent)
                    result.faceBlendshapes().get() else null
                val blendshapes = FloatArray(52)
                if (blendshapeList != null && blendshapeList.isNotEmpty()) {
                    val bs = blendshapeList[0]
                    for (j in 0 until minOf(bs.size, 52)) {
                        blendshapes[j] = bs[j].score()
                    }
                }

                // 5. Kirim ke Fizgravity engine untuk stabilisasi + normalisasi
                val enginePtr = fizgravityEnginePtr
                var landmarkList: ArrayList<Double>

                if (enginePtr != 0L && fizgravityLoaded) {
                    try {
                        // Kirim mesh mentah MediaPipe ke engine
                        fizgravitySetFaceMesh(enginePtr, fa, blendshapes)

                        // Ambil kembali mesh yang sudah:
                        // - Distabilkan (One-Euro Filter)
                        // - Diprediksi (Late Latching + RK4 extrapolation)
                        // - Dikoreksi Rolling-Shutter
                        val processEndNs = System.nanoTime()
                        val processDelaySec = (processEndNs - processStartNs) / 1_000_000_000f
                        // Horizon prediksi: Waktu pemrosesan (latency MP) + 1 frame ke depan (tampil)
                        val dtPredict = processDelaySec + 0.016f 
                        val predicted = fizgravityGetPredictedLandmarks(enginePtr, dtPredict)

                        landmarkList = if (predicted != null && predicted.size == 468 * 3) {
                            // Gunakan landmark yang sudah diprediksi (ZERO-LAG!)
                            ArrayList<Double>(predicted.size).also { list ->
                                predicted.forEach { list.add(it.toDouble()) }
                            }
                        } else {
                            // Fallback: gunakan landmark MediaPipe langsung
                            buildLandmarkList(fa)
                        }
                    } catch (e: Exception) {
                        Log.w("FizgravityEngine", "Engine call failed, using MediaPipe direct: ${e.message}")
                        landmarkList = buildLandmarkList(fa)
                    }
                } else {
                    // Fizgravity tidak tersedia, pakai MediaPipe langsung
                    landmarkList = buildLandmarkList(fa)
                }

                resultMap["landmarks"] = landmarkList

                // 6. Analisis morfologi wajah via JNI C++
                try {
                    val d = nativeAnalyzeMorphology(fa)
                    resultMap["faceShape"] = arrayOf("Round", "Oblong", "Square")[d[0].toInt().coerceIn(0, 2)]
                    resultMap["eyeShape"] = arrayOf("Downturned", "Monolid", "Hooded", "Normal")[d[1].toInt().coerceIn(0, 3)]
                    resultMap["noseShape"] = arrayOf("Wide", "Crooked", "Normal")[d[2].toInt().coerceIn(0, 2)]
                    resultMap["jawWidth"] = d[3].toDouble()
                    resultMap["faceLength"] = d[4].toDouble()
                    resultMap["canthalTilt"] = d[5].toDouble()
                    resultMap["eyeAspectRatio"] = d[6].toDouble()
                    resultMap["alarBaseWidth"] = d[7].toDouble()
                    resultMap["intercanthalDistance"] = d[8].toDouble()
                } catch (ex: Exception) {
                    Log.e("MediaPipePlugin", "JNI morphology error: ${ex.message}")
                }

                // 7. Tambahkan metadata debug (berguna untuk profiling lag)
                resultMap["frameIndex"] = frameIdx.toDouble()
                resultMap["engineActive"] = (fizgravityEnginePtr != 0L && fizgravityLoaded)

            } else {
                resultMap["landmarks"] = ArrayList<Double>()
            }

            resultMap
        } catch (e: Exception) {
            Log.e("MediaPipePlugin", "Frame callback error: ${e.message}")
            null
        }
    }

    // ── Utilities ────────────────────────────────────────────────────────────

    private fun buildLandmarkList(fa: FloatArray): ArrayList<Double> {
        return ArrayList<Double>(fa.size).also { list ->
            fa.forEach { list.add(it.toDouble()) }
        }
    }

    /**
     * Cleanup: Lepaskan semua resources saat plugin tidak lagi digunakan.
     * Dipanggil oleh VisionCamera saat kamera ditutup.
     */
    fun cleanup() {
        try { sensorManager?.unregisterListener(this) } catch (e: Exception) {}
        val ptr = fizgravityEnginePtr
        if (ptr != 0L && fizgravityLoaded) {
            try { fizgravityRelease(ptr) } catch (e: Exception) {}
            fizgravityEnginePtr = 0L
        }
        try { faceLandmarker?.close() } catch (e: Exception) {}
        Log.d("FizgravityEngine", "Plugin cleanup complete")
    }
}
