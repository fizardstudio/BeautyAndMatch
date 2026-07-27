# Panduan Index Landmark MediaPipe 468: Kelopak Mata Atas & Eyeshadow Region

Dokumen ini berisi pemetaan lengkap titik **MediaPipe 468 Canonical Face Mesh** khusus area **Kelopak Mata Atas (Upper Eyelid & Palpebral Crease)** untuk mata kiri dan kanan. Index ini digunakan sebagai referensi presisi dalam rasterisasi/masking eyeshadow pada engine AR beauty filter C++/OpenGL ES/Metal.

---

## 1. Kode C++ Header / Source Siap Pakai

File ini dapat langsung di-copy ke dalam file C++ engine Anda (misalnya `FizgravityMath.hpp` atau `FizgravityRenderer.cpp`).

```cpp
#ifndef FIZGRAVITY_EYESHADOW_LANDMARKS_HPP
#ifndef FIZGRAVITY_EYESHADOW_LANDMARKS_HPP
#define FIZGRAVITY_EYESHADOW_LANDMARKS_HPP

#include <cstddef>

namespace Fizgravity {
namespace Landmarks {

// ============================================================================
// 1. GARIS KONTUR BULU MATA ATAS (UPPER LASH LINE CONTOUR)
// Mengikuti kurva pertumbuhan bulu mata atas (garis tepi bawah eyeshadow).
// ============================================================================

// Kelopak Mata Atas Kiri (Left Eye Upper Lash Line): Inner Corner (362) -> Outer Corner (263)
static const unsigned short LEFT_EYE_UPPER_LID[] = {
    362, 398, 384, 385, 386, 387, 388, 466, 263
};
static const size_t LEFT_EYE_UPPER_LID_COUNT = sizeof(LEFT_EYE_UPPER_LID) / sizeof(LEFT_EYE_UPPER_LID[0]);

// Kelopak Mata Atas Kanan (Right Eye Upper Lash Line): Outer Corner (33) -> Inner Corner (133)
static const unsigned short RIGHT_EYE_UPPER_LID[] = {
    33, 246, 161, 160, 159, 158, 157, 173, 133
};
static const size_t RIGHT_EYE_UPPER_LID_COUNT = sizeof(RIGHT_EYE_UPPER_LID) / sizeof(RIGHT_EYE_UPPER_LID[0]);


// ============================================================================
// 2. KAWASAN POLYGON EYESHADOW 2D (UPPER EYELID + PALPEBRAL CREASE PATCH)
// Loop tertutup 2D antara garis bulu mata atas & lipatan kelopak mata (crease).
// Digunakan untuk pembentukan mesh / polygon rasterization eyeshadow.
// ============================================================================

// Polygon Eyeshadow Mata Kiri (Anatomical Left Eye)
static const unsigned short LEFT_EYESHADOW_REGION[] = {
    // Segmen Bawah (Garis Bulu Mata Atas): Inner -> Outer
    362, 398, 384, 385, 386, 387, 388, 466, 263,
    // Segmen Atas (Garis Lipatan Kelopak / Crease): Outer -> Inner
    467, 341, 256, 252, 253, 254, 339, 255
};
static const size_t LEFT_EYESHADOW_REGION_COUNT = sizeof(LEFT_EYESHADOW_REGION) / sizeof(LEFT_EYESHADOW_REGION[0]);

// Polygon Eyeshadow Mata Kanan (Anatomical Right Eye)
static const unsigned short RIGHT_EYESHADOW_REGION[] = {
    // Segmen Bawah (Garis Bulu Mata Atas): Outer -> Inner
    33, 246, 161, 160, 159, 158, 157, 173, 133,
    // Segmen Atas (Garis Lipatan Kelopak / Crease): Inner -> Outer
    247, 112, 26, 22, 23, 24, 110, 25
};
static const size_t RIGHT_EYESHADOW_REGION_COUNT = sizeof(RIGHT_EYESHADOW_REGION) / sizeof(RIGHT_EYESHADOW_REGION[0]);

} // namespace Landmarks
} // namespace Fizgravity

#endif // FIZGRAVITY_EYESHADOW_LANDMARKS_HPP
```

---

## 2. Peta Anatomi & Orientasi MediaPipe

```
                    [ALIS KANAN]                           [ALIS KIRI]
               70, 63, 105, 66, 107                   336, 296, 334, 293, 300
               --------------------                   ---------------------
                 \               /                      \               /
   Garis Crease:  247,112,26,22,23,24,110,25             467,341,256,252,253,254,339,255
                  --------------------------             ------------------------------
                     ( AREA EYESHADOW KANAN )               ( AREA EYESHADOW KIRI )
                  --------------------------             ------------------------------
   Bulu Mata Atas: 33,246,161,160,159,158,157,173,133     362,398,384,385,386,387,388,466,263
                    \                      /               \                      /
                     (===== BOLA MATA =====)                (===== BOLA MATA =====)
```

* **Mata Kiri (Anatomical Left Eye):** Di kamera (mode selfie), terletak di **sisi kanan layar**.
* **Mata Kanan (Anatomical Right Eye):** Di kamera (mode selfie), terletak di **sisi kiri layar**.

---

## 3. Sumber & Otentikasi Referensi

1. **Google MediaPipe Official Repository:**
   * File `mediapipe/python/solutions/face_mesh_connections.py` (`FACEMESH_LEFT_EYE` & `FACEMESH_RIGHT_EYE`).
2. **MediaPipe Canonical Face Model UV Visualization:**
   * File `mediapipe/modules/face_geometry/data/canonical_face_model_uv_visualization.png`.
3. **Open-Source AR Beauty Filter Engines:**
   * Standar segmentasi mata pada Spark AR, TikTok Effect House, dan Snapchat Lens Studio.
