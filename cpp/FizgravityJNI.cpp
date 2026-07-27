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

// Handle ke library Fizgravity yang dimuat secara lazy
static void* gFizLibHandle = nullptr;
static FnFizInit              gFizInit               = nullptr;
static FnFizRelease           gFizRelease            = nullptr;
static FnFizPushImu           gFizPushImu            = nullptr;
static FnFizSetFaceMesh       gFizSetFaceMesh        = nullptr;
static FnFizGetPredictedLandmarks gFizGetPredicted   = nullptr;
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

    if (!gFizInit || !gFizRelease || !gFizPushImu || !gFizSetFaceMesh || !gFizGetPredicted) {
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

} // extern "C"
