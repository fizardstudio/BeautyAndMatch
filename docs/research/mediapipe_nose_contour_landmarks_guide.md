# Panduan Index Landmark MediaPipe 468: Nose Bridge, Ala Nasi & Nose Reshape Contour

Dokumen ini berisi pemetaan presisi titik **MediaPipe 468 Canonical Face Mesh** khusus area **Batang Hidung (Nose Bridge)**, **Cuping Hidung (Ala Nasi)**, dan **Columella/Septum** untuk pembuatan fitur *Nose Reshape* & *Geometrics Nose Shading* pada engine AR beauty filter C++/OpenGL ES/Metal.

---

## 1. Tabel Sumbu Vertikal Hidung (Top-to-Bottom Midline Axis)

Urutan titik dari atas (glabella/pangkal alis) turun ke bawah (ujung hidung & dasar septum):

| Urutan | Index MediaPipe | Nama Titik Anatomi | Deskripsi / Posisi |
|---|---|---|---|
| 1 | **168** | Glabella / Sellion | Titik tengah pangkal hidung (antara kedua alis) |
| 2 | **6** | Superior Nasion | Batang hidung bagian atas |
| 3 | **197** | Mid-Nasion | Batang hidung bagian tengah-atas |
| 4 | **195** | Inferior Nasion | Batang hidung bagian tengah-bawah |
| 5 | **5** | Supratip | Tepat di atas puncak ujung hidung |
| 6 | **4** | Pronasale (Nose Tip) | **Puncak Ujung Hidung** (Titik paling depan 3D) |
| 7 | **1** | Subnasale Superior | Di bawah puncak hidung (bagian atas columella) |
| 8 | **19** | Columella Mid | Batang pembatas antara dua lubang hidung |
| 9 | **94** | Subnasale Inferior | Dasar columella sebelum bibir atas |
| 10 | **2** | Infranasale / Subnasale Base | Titik tengah tepat di dasar septum bibir atas |

---

## 2. Kode C++ Header / Source Siap Pakai

File ini dapat di-copy langsung ke modul C++ AR engine Anda (`FizgravityMath.hpp` / `FizgravityRenderer.cpp`).

```cpp
#ifndef FIZGRAVITY_NOSE_LANDMARKS_HPP
#define FIZGRAVITY_NOSE_LANDMARKS_HPP

#include <cstddef>

namespace Fizgravity {
namespace Landmarks {

// ============================================================================
// 1. SUMBU VERTIKAL TENGAH BATANG HIDUNG (NOSE BRIDGE MIDLINE AXIS)
// Urutan dari Glabella (168) turun ke Dasar Septum (2).
// ============================================================================
static const unsigned short NOSE_MIDLINE_AXIS[] = {
    168, // Glabella (Pangkal Alis)
    6,   // Upper Nose Bridge
    197, // Mid-Upper Nose Bridge
    195, // Mid-Lower Nose Bridge
    5,   // Supratip
    4,   // Pronasale (Nose Tip / Ujung Hidung)
    1,   // Subnasale Superior
    19,  // Columella Mid
    94,  // Subnasale Inferior
    2    // Subnasale Base (Dasar Septum)
};
static const size_t NOSE_MIDLINE_AXIS_COUNT = sizeof(NOSE_MIDLINE_AXIS) / sizeof(NOSE_MIDLINE_AXIS[0]);


// ============================================================================
// 2. CUPING HIDUNG (ALA NASI / NOSTRIL FLARE) - REFERENSI LEBAR HIDUNG
// Digunakan untuk kalkulasi jarak lebar hidung (Distance |P_327 - P_98|).
// ============================================================================

// Cuping Hidung Kiri (Anatomical Left / Frame Right): Dari Luar ke Dalam
static const unsigned short NOSE_ALA_LEFT[] = {
    437, 358, 279, 327, 294
};
static const size_t NOSE_ALA_LEFT_COUNT = sizeof(NOSE_ALA_LEFT) / sizeof(NOSE_ALA_LEFT[0]);

// Cuping Hidung Kanan (Anatomical Right / Frame Left): Dari Luar ke Dalam
static const unsigned short NOSE_ALA_RIGHT[] = {
    217, 129, 49, 98, 64
};
static const size_t NOSE_ALA_RIGHT_COUNT = sizeof(NOSE_ALA_RIGHT) / sizeof(NOSE_ALA_RIGHT[0]);

// Titik Jangkar Paling Luar Cuping (Anchor Points for Nose Width Slider)
static const unsigned short NOSE_LEFT_OUTER_ALA_ANCHOR = 327; // atau 279
static const unsigned short NOSE_RIGHT_OUTER_ALA_ANCHOR = 98; // atau 49


// ============================================================================
// 3. V-SHAPE COLUMELLA & SEPTUM (UNTUK TEKNIK MEMENDEKKAN HIDUNG / NOSE SHORTENING)
// Shading bentuk V di bawah septum/ujung hidung sesuai teknik MUA profesional.
// ============================================================================
static const unsigned short NOSE_V_SHAPE_SEPTUM[] = {
    98,  // Alar Right
    97,  // Right Columella Flank
    2,   // Subnasale Base (Bottom Point of V)
    326, // Left Columella Flank
    327, // Alar Left
    4    // Nose Tip Apex (Puncak Atas V)
};
static const size_t NOSE_V_SHAPE_SEPTUM_COUNT = sizeof(NOSE_V_SHAPE_SEPTUM) / sizeof(NOSE_V_SHAPE_SEPTUM[0]);


// ============================================================================
// 4. GARIS KONTUR SISI BATANG HIDUNG (NOSE BRIDGE SIDE FLANKS)
// Garis vertikal di mana shader shading/contour diletakkan (di luar midline).
// ============================================================================

// Sisi Kiri Batang Hidung (Left Nose Bridge Flank)
static const unsigned short NOSE_BRIDGE_FLANK_LEFT[] = {
    351, 417, 285, 419, 399, 456, 360
};
static const size_t NOSE_BRIDGE_FLANK_LEFT_COUNT = sizeof(NOSE_BRIDGE_FLANK_LEFT) / sizeof(NOSE_BRIDGE_FLANK_LEFT[0]);

// Sisi Kanan Batang Hidung (Right Nose Bridge Flank)
static const unsigned short NOSE_BRIDGE_FLANK_RIGHT[] = {
    122, 193, 55, 196, 174, 236, 131
};
static const size_t NOSE_BRIDGE_FLANK_RIGHT_COUNT = sizeof(NOSE_BRIDGE_FLANK_RIGHT) / sizeof(RIGHT_EYE_UPPER_LID[0]);

} // namespace Landmarks
} // namespace Fizgravity

#endif // FIZGRAVITY_NOSE_LANDMARKS_HPP
```

---

## 3. Peta Topologi 2D Hidung MediaPipe

```
                      (168) [Glabella]
                     /     \
                (122)  (6)  (351)
                  |     |     |
                (193) (197) (417)   <--- Flank Lines (Area Shading Cokelat)
                  |     |     |
                 (55) (195) (285)
                  |     |     |
                (196)  (5)  (419)
               /   \   |   /   \
  [Cuping   (49)---(4) [Nose Tip] --(279) [Cuping
   Kanan]    |      /     \      |         Kiri]
           (98)---(1) (19) (327)
             \     \   |   /     /
              (97)--(94)-(2)-(326)  <--- V-Shape Shortening Area
```

---

## 4. Cara Penggunaan untuk Algoritma *Nose Reshape* di C++

1. **Memendekkan Batang Hidung (*Nose Shortening*):**  
   Gambar polygon shading ber-opacity ~30-50% pada indeks `NOSE_V_SHAPE_SEPTUM` (`{98, 97, 2, 326, 327, 4}`). Hal ini menciptakan ilusi bayangan yang memendekkan hidung panjang.
2. **Menyempitkan Batang Hidung (*Nose Slimming*):**  
   Hitung vektor normal antara `NOSE_MIDLINE_AXIS` ke `NOSE_BRIDGE_FLANK_LEFT` dan `NOSE_BRIDGE_FLANK_RIGHT`. Geser posisi vertex flank mendekati `NOSE_MIDLINE_AXIS` sebesar faktor skala slider `0.0 - 1.0`.
3. **Highlighter Puncak Hidung (*Nose Tip Highlight*):**  
   Tempatkan *radial specular dot highlight* tepat di vertex `4` (Pronasale) dan garis tipis sepanjang vertex `197` sampai `5`.

---

## 5. Sumber & Otentikasi Referensi

1. **Google MediaPipe Repository (`face_mesh_connections.py`):**
   * Pasangan tepi `FACEMESH_NOSE` resmi: `(168, 6), (6, 197), (197, 195), (195, 5), (5, 4), (4, 1), (1, 19), (19, 94), (94, 2)`.
2. **MediaPipe Canonical 3D Face Model Specification (`mediapipe/modules/face_geometry/data/`):**
   * Landmark `4` terdefinisi secara kanonis sebagai *Pronasale* (titik apex terdepan hidung 3D).
   * Landmark `168` terdefinisi sebagai *Sellion/Glabella*.
3. **AR Filter Mesh Standards (Spark AR / Lens Studio):**
   * Indeks `98` dan `327` secara universal digunakan sebagai *left/right alar base landmarks* untuk kalkulasi skala lebar hidung.
