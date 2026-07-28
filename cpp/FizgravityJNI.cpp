// FizgravityJNI.cpp
// JNI Bridge: Kotlin (Java) <-> Fizgravity AR Engine (Rust via C FFI)
//
// Fungsi-fungsi di sini adalah wrapper tipis yang menerjemahkan panggilan JNI
// dari MediaPipeFrameProcessorPlugin.kt ke fungsi C extern dari libfizgravity_ar.so.
//
// Pattern yang digunakan: "transparent bridge" — tidak ada logika bisnis di sini,
// hanya type conversion dan null safety checks.

#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <cstring>
#include <cstdint>

#define LOG_TAG "FizgravityJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)

// ── Deklarasi pointer fungsi Rust (dimuat secara dinamis via dlopen) ────────────

typedef void*   (*FnFizInit)(const char*);
typedef void    (*FnFizRelease)(void*);
typedef int     (*FnFizPushImu)(void*, float, float, float, float, float, float, float);
typedef int     (*FnFizSetFaceMesh)(void*, const float*, const float*);
typedef int     (*FnFizGetPredictedLandmarks)(void*, float*, int, float);
typedef int     (*FnFizGetStabilizedLandmarks)(void*, float*, int);
typedef int     (*FnFizCalculateDynamicAO)(void*, float*, int);
typedef int     (*FnFizCalculateHairlineBlending)(void*, float*, int);
typedef int     (*FnFizEstimateLighting)(void*, const void*, int, int, int, size_t, float*, float*);

// Handle ke library Fizgravity yang dimuat secara lazy
static void* gFizLibHandle = nullptr;
static FnFizInit              gFizInit               = nullptr;
static FnFizRelease           gFizRelease            = nullptr;
static FnFizPushImu           gFizPushImu            = nullptr;
static FnFizSetFaceMesh       gFizSetFaceMesh        = nullptr;
static FnFizGetPredictedLandmarks gFizGetPredicted   = nullptr;
static FnFizGetStabilizedLandmarks gFizGetStabilized = nullptr;
static FnFizCalculateDynamicAO gFizCalculateDynamicAO = nullptr;
static FnFizCalculateHairlineBlending gFizCalculateHairlineBlending = nullptr;
static FnFizEstimateLighting   gFizEstimateLighting   = nullptr;
static bool                   gFizLoaded             = false;

// ── Lazy-load libfizgravity_ar.so saat pertama kali dibutuhkan ─────────────────

static bool ensureFizgravityLoaded() {
    if (gFizLoaded) return true;

    // Coba load dari beberapa path yang mungkin
    const char* paths[] = {
        "libfizgravity_ar.so",
        "/data/app/libfizgravity_ar.so",
        nullptr
    };

    for (int i = 0; paths[i] != nullptr; ++i) {
        gFizLibHandle = dlopen(paths[i], RTLD_NOW | RTLD_GLOBAL);
        if (gFizLibHandle != nullptr) break;
    }

    if (gFizLibHandle == nullptr) {
        LOGE("dlopen libfizgravity_ar.so failed: %s", dlerror());
        return false;
    }

    // Load semua function pointers
    gFizInit       = (FnFizInit)              dlsym(gFizLibHandle, "fizgravity_engine_init");
    gFizRelease    = (FnFizRelease)           dlsym(gFizLibHandle, "fizgravity_engine_release");
    gFizPushImu    = (FnFizPushImu)           dlsym(gFizLibHandle, "fizgravity_engine_push_imu");
    gFizSetFaceMesh= (FnFizSetFaceMesh)       dlsym(gFizLibHandle, "fizgravity_engine_set_face_mesh");
    gFizGetPredicted=(FnFizGetPredictedLandmarks)dlsym(gFizLibHandle, "fizgravity_engine_get_predicted_landmarks");
    gFizGetStabilized=(FnFizGetStabilizedLandmarks)dlsym(gFizLibHandle, "fizgravity_engine_get_stabilized_landmarks");
    gFizCalculateDynamicAO=(FnFizCalculateDynamicAO)dlsym(gFizLibHandle, "fizgravity_engine_calculate_dynamic_ao");
    gFizCalculateHairlineBlending=(FnFizCalculateHairlineBlending)dlsym(gFizLibHandle, "fizgravity_engine_calculate_hairline_blending");
    gFizEstimateLighting=(FnFizEstimateLighting)dlsym(gFizLibHandle, "fizgravity_engine_estimate_lighting");

    if (!gFizInit || !gFizRelease || !gFizPushImu || !gFizSetFaceMesh || !gFizGetPredicted || !gFizGetStabilized || !gFizCalculateDynamicAO || !gFizCalculateHairlineBlending || !gFizEstimateLighting) {
        LOGE("dlsym failed — missing symbols in libfizgravity_ar.so: %s", dlerror());
        dlclose(gFizLibHandle);
        gFizLibHandle = nullptr;
        return false;
    }

    gFizLoaded = true;
    LOGI("libfizgravity_ar.so loaded and all symbols resolved OK");
    return true;
}

// ── JNI Implementations ─────────────────────────────────────────────────────────

extern "C" {

// fizgravityInit() → Long (engine pointer as 64-bit integer)
JNIEXPORT jlong JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravityInit(
    JNIEnv* env, jobject thiz)
{
    if (!ensureFizgravityLoaded()) {
        LOGE("Cannot init: libfizgravity_ar.so not loaded");
        return 0L;
    }
    void* ptr = gFizInit(nullptr); // null = use default model path
    LOGI("Engine initialized: ptr=%p", ptr);
    return (jlong)(uintptr_t)ptr;
}

// fizgravityRelease(enginePtr: Long)
JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravityRelease(
    JNIEnv* env, jobject thiz, jlong enginePtr)
{
    if (!gFizLoaded || gFizRelease == nullptr) return;
    void* ptr = (void*)(uintptr_t)enginePtr;
    if (ptr == nullptr) return;
    gFizRelease(ptr);
    LOGI("Engine released");
}

// fizgravityPushImu(enginePtr, gx, gy, gz, ax, ay, az, ts) → Int
JNIEXPORT jint JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravityPushImu(
    JNIEnv* env, jobject thiz,
    jlong enginePtr,
    jfloat gx, jfloat gy, jfloat gz,
    jfloat ax, jfloat ay, jfloat az,
    jfloat timestamp_sec)
{
    if (!gFizLoaded || gFizPushImu == nullptr) return -10;
    void* ptr = (void*)(uintptr_t)enginePtr;
    if (ptr == nullptr) return -1;
    return gFizPushImu(ptr, gx, gy, gz, ax, ay, az, timestamp_sec);
}

// fizgravitySetFaceMesh(enginePtr, vertices: FloatArray, blendshapes: FloatArray) → Int
JNIEXPORT jint JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravitySetFaceMesh(
    JNIEnv* env, jobject thiz,
    jlong enginePtr,
    jfloatArray vertices,
    jfloatArray blendshapes)
{
    if (!gFizLoaded || gFizSetFaceMesh == nullptr) return -10;
    void* ptr = (void*)(uintptr_t)enginePtr;
    if (ptr == nullptr || vertices == nullptr || blendshapes == nullptr) return -1;

    jsize vLen = env->GetArrayLength(vertices);
    jsize bLen = env->GetArrayLength(blendshapes);

    // Validasi ukuran: harus 468*3 = 1404 floats dan 52 blendshapes
    if (vLen < 1404 || bLen < 52) {
        LOGW("Invalid array sizes: vertices=%d (need 1404), blendshapes=%d (need 52)", vLen, bLen);
        return -2;
    }

    // GetFloatArrayElements: zero-copy jika VM mendukung, atau copy jika tidak
    jboolean isCopyV = JNI_FALSE, isCopyB = JNI_FALSE;
    float* vData = env->GetFloatArrayElements(vertices, &isCopyV);
    float* bData = env->GetFloatArrayElements(blendshapes, &isCopyB);

    int result = -3;
    if (vData && bData) {
        result = gFizSetFaceMesh(ptr, vData, bData);
    }

    // Release tanpa commit (JNI_ABORT) karena kita tidak memodifikasi array
    if (vData) env->ReleaseFloatArrayElements(vertices,    vData, JNI_ABORT);
    if (bData) env->ReleaseFloatArrayElements(blendshapes, bData, JNI_ABORT);

    return result;
}

// fizgravityGetPredictedLandmarks(enginePtr, dtPredict) → FloatArray? (468*3 floats atau null)
JNIEXPORT jfloatArray JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravityGetPredictedLandmarks(
    JNIEnv* env, jobject thiz,
    jlong enginePtr,
    jfloat dt_predict)
{
    if (!gFizLoaded || gFizGetPredicted == nullptr) return nullptr;
    void* ptr = (void*)(uintptr_t)enginePtr;
    if (ptr == nullptr) return nullptr;

    // Alokasikan output buffer di stack (468 * 3 = 1404 floats = ~5.5KB, aman di stack)
    static thread_local float out_buf[1404];
    int n = gFizGetPredicted(ptr, out_buf, 468, dt_predict);

    if (n <= 0) return nullptr;

    // Buat jfloatArray dan salin hasilnya
    jfloatArray result = env->NewFloatArray(n * 3);
    if (result == nullptr) return nullptr; // OOM
    env->SetFloatArrayRegion(result, 0, n * 3, out_buf);
    return result;
}

// fizgravityGetStabilizedLandmarks(enginePtr) → FloatArray? (468*3 floats atau null)
// One-Euro filtered only — tanpa ekstrapolasi RK4/rotasi gyro/blend prediktif.
JNIEXPORT jfloatArray JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravityGetStabilizedLandmarks(
    JNIEnv* env, jobject thiz,
    jlong enginePtr)
{
    if (!gFizLoaded || gFizGetStabilized == nullptr) return nullptr;
    void* ptr = (void*)(uintptr_t)enginePtr;
    if (ptr == nullptr) return nullptr;

    static thread_local float out_buf[1404];
    int n = gFizGetStabilized(ptr, out_buf, 468);

    if (n <= 0) return nullptr;

    jfloatArray result = env->NewFloatArray(n * 3);
    if (result == nullptr) return nullptr; // OOM
    env->SetFloatArrayRegion(result, 0, n * 3, out_buf);
    return result;
}

// fizgravityCalculateDynamicAO(enginePtr) → FloatArray? (468 floats atau null)
// 1.0 = terang penuh (tidak ada occlusion), makin kecil = makin gelap.
JNIEXPORT jfloatArray JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravityCalculateDynamicAO(
    JNIEnv* env, jobject thiz,
    jlong enginePtr)
{
    if (!gFizLoaded || gFizCalculateDynamicAO == nullptr) return nullptr;
    void* ptr = (void*)(uintptr_t)enginePtr;
    if (ptr == nullptr) return nullptr;

    static thread_local float out_buf[468];
    int n = gFizCalculateDynamicAO(ptr, out_buf, 468);

    if (n <= 0) return nullptr;

    jfloatArray result = env->NewFloatArray(n);
    if (result == nullptr) return nullptr; // OOM
    env->SetFloatArrayRegion(result, 0, n, out_buf);
    return result;
}

// fizgravityCalculateHairlineBlending(enginePtr) → FloatArray? (468 floats atau null)
// 1.0 = makeup tampil penuh, memudar ke 0.0 dekat garis rambut.
JNIEXPORT jfloatArray JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravityCalculateHairlineBlending(
    JNIEnv* env, jobject thiz,
    jlong enginePtr)
{
    if (!gFizLoaded || gFizCalculateHairlineBlending == nullptr) return nullptr;
    void* ptr = (void*)(uintptr_t)enginePtr;
    if (ptr == nullptr) return nullptr;

    static thread_local float out_buf[468];
    int n = gFizCalculateHairlineBlending(ptr, out_buf, 468);

    if (n <= 0) return nullptr;

    jfloatArray result = env->NewFloatArray(n);
    if (result == nullptr) return nullptr; // OOM
    env->SetFloatArrayRegion(result, 0, n, out_buf);
    return result;
}

// fizgravityEstimateLighting(enginePtr, cameraBuffer, width, height, rowStride) → FloatArray? [cctKelvin, intensity], atau null kalau gagal
JNIEXPORT jfloatArray JNICALL
Java_com_matchandbeauty_FizgravityARView_fizgravityEstimateLighting(
    JNIEnv* env, jobject thiz,
    jlong enginePtr, jobject cameraBuffer, jint width, jint height, jint rowStride)
{
    if (!gFizLoaded || gFizEstimateLighting == nullptr) return nullptr;
    void* ptr = (void*)(uintptr_t)enginePtr;
    if (ptr == nullptr || cameraBuffer == nullptr) return nullptr;

    // Pakai alamat & kapasitas NYATA buffer langsung (bukan width*height*3 turunan),
    // sesuai konvensi buffer_len_bytes otoritatif yang dipakai lighting.rs.
    void* bufPtr = env->GetDirectBufferAddress(cameraBuffer);
    jlong bufCapacity = env->GetDirectBufferCapacity(cameraBuffer);
    if (bufPtr == nullptr || bufCapacity <= 0) return nullptr;

    float cct = 0.0f;
    float intensity = 0.0f;
    int result = gFizEstimateLighting(ptr, bufPtr, width, height, rowStride, (size_t)bufCapacity, &cct, &intensity);

    if (result != 0) return nullptr;

    jfloatArray out = env->NewFloatArray(2);
    if (out == nullptr) return nullptr; // OOM
    float values[2] = { cct, intensity };
    env->SetFloatArrayRegion(out, 0, 2, values);
    return out;
}

} // extern "C"
