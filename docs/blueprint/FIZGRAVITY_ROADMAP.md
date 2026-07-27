# 🚀 FIZGRAVITY AR ENGINE - THE ULTIMATE ROADMAP
*Blueprint to Surpass Top-Tier AR Engines (Snapchat, TikTok, Spark AR)*

---

## 🎯 FASE 1: VISUAL HYPER-REALISM (Shaders & Pencahayaan)
Saat ini *makeup* kita menggunakan *Alpha Blending* standar. Agar tidak terlihat seperti stiker murahan, kita harus meniru hukum fisika cahaya nyata:

1. **Photoshop-Grade Blend Modes (C++ OpenGL)**
   - **Lipstik & Blush On:** Gunakan teknik **Multiply** atau **Soft Light** *blending* di dalam *shader* GPU. Ini akan membuat tekstur asli bibir (kerutan alami) dan pori-pori kulit tetap menembus warna *makeup*.
   - **Foundation/Bedak:** Implementasikan **Frequency Separation**. Blur warna kulit (low-pass) tapi pertahankan ketajaman tekstur pori-pori (high-pass).

2. **3D Relighting & Normal Mapping**
   - Wajah bukan kanvas datar. Kita akan menggunakan **Normal Maps** (arah lekukan wajah) untuk menciptakan pantulan cahaya buatan (Specular Highlights). 
   - Hasilnya: *Lipgloss* akan terlihat berkilau saat terkena cahaya, dan kontur wajah akan memiliki bayangan 3D yang realistis.

## 🎯 FASE 2: FITUR EKSPANSI (Memanfaatkan 478 Titik)
Kapasitas memori array sudah kita buka untuk menampung seluruh titik wajah (termasuk iris mata).

1. **Hyper-Realistic Custom Eye Lenses (Softlens)**
   - Menggunakan 10 titik ekstra dari iris mata untuk memetakan tekstur *softlens* bulat sempurna.
   - Efek basah (mata berair) dan pantulan lingkungan (Environment Mapping) pada kornea mata.

2. **Teeth Whitening & Lip Segmentation**
   - Masking gigi yang sangat akurat agar pemutihan gigi tidak bocor ke gusi atau bibir.

## 🎯 FASE 3: ULTIMATE ZERO LATENCY (True 60-120 FPS)
Saat ini kita menggunakan **Synchronous Rendering** (menahan *frame*) yang menempel 100% sempurna, namun mengorbankan sedikit FPS layar (mengikuti 30 FPS dari AI). Top-tier *engine* menggunakan kombinasi maut berikut:

1. **IMU Sensor Fusion Warp (Gyroscope)**
   - Kelemahan kamera tertahan (30 FPS) akan diobati dengan Gyroscope (bekerja di 200-1000Hz).
   - Setiap kali kepala bergerak, *engine* kita (C++/Rust) akan memutar ulang *frame* kamera terakhir secara grafis (*Warping*) sebelum menayangkannya ke layar. 
   - Hasilnya: Layar berjalan di 60 FPS mentok, sangat *smooth*, dan *makeup* tetap menempel 100%. (Ini adalah rahasia terbesar ARKit/ARCore).

## 🎯 FASE 4: OPTIMASI KELAS KAKAP (Low-End Devices)
1. **Asynchronous Compute & Vulkan API**
   - Upgrade dari OpenGL (yang sudah mulai usang) ke **Vulkan API**. Vulkan mengizinkan CPU untuk berbicara ke GPU menggunakan banyak antrean (multithreading), menurunkan beban CPU secara drastis, sehingga HP kentang pun tidak akan panas saat menjalankan AR berat.

---
*Roadmap ini adalah kerangka kerja (blueprint) rahasia untuk mengubah Fizgravity menjadi AR Engine tingkat dunia.*
