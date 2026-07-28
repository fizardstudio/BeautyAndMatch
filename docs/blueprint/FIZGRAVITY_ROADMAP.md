# 🚀 FIZGRAVITY AR — THE NEXT LEVEL ROADMAP (v3.0)
*Blueprint untuk melangkahi AR engine kelas atas (Snapchat, TikTok/ByteDance, Perfect Corp/YouCam, ModiFace, ARKit/ARCore)*

> Menggantikan v1-v2 roadmap ini. Dicocokkan ulang terhadap `THE MASTER AR BEAUTY PIPELINE & DICTIONARY.md`
> (spesifikasi 11-layer makeup) dan pelajaran empiris dari sesi debugging kamera/render 2026-07-28.
> Terakhir diperbarui: 2026-07-28

---

## 🧭 Prinsip Kerja: TIRU — AMATI — MODIFIKASI — OPTIMALISASI (TAMO)

Setiap fitur hyper-realistic BARU (bukan bug fix kecil) **wajib** dimulai dari riset — delegasikan ke Gemini
(`web-researcher`) untuk pelajari bagaimana kompetitor kelas atas benar-benar mengimplementasikan fitur
setara, SEBELUM merancang versi kita sendiri. Baru setelah itu modifikasi dan optimalkan. Jangan
reinvent blind — buang-buang waktu untuk menemukan ulang solusi yang sudah ada, seringkali lebih buruk.

Contoh kenapa ini penting: Fase 3 roadmap versi lama (di bawah) merencanakan "IMU Sensor Fusion Warp"
sebagai solusi zero-latency, dan kita coba implementasikan sesi ini — hasilnya malah bikin getar parah,
harus di-revert. Root cause: ide dasarnya BENAR (ARKit/ARCore memang pakai reprojection berbasis IMU),
tapi implementasi kita cuma menggeser posisi vertex overlay, bukan me-reproject seluruh frame kamera
via image warp/homography seperti yang sebenarnya dilakukan ARKit/ARCore. Andai kita riset dulu teknik
aslinya secara detail, kita akan tahu itu perlu warping GAMBAR, bukan cuma overlay — dan bisa putuskan
dari awal apakah itu sepadan dengan kompleksitasnya.

---

## 🎯 Keputusan Strategis: EVOLUSI, Bukan Mulai dari Nol

Pertanyaan yang sering muncul: apakah perlu restart total mengikuti brainstorming baru untuk hyper-realistic
next-level, atau bisa lanjutkan dari aplikasi sekarang?

**Jawaban: EVOLUSI.** Alasannya:
- Arsitektur render sekarang (MediaPipe tracking + multi-pass GPU FBO mask-baking + compositing shader)
  itu SUDAH pola yang sama dipakai engine kelas atas (Snap Lens Studio, Spark AR internal-nya serupa).
- Beberapa layer (Contour, Blush, klasifikasi face-shape) formula shader-nya **sudah persis sama** dengan
  riset MUA profesional di dictionary — bukti arah kita sudah benar, bukan asal-asalan.
- Infrastruktur kamera/render yang sulit dan gak kelihatan (stabilitas pipeline kamera, jitter, resolusi,
  AWB, EIS, latensi MediaPipe) baru saja diselesaikan dengan biaya besar sesi ini — kalau restart, semua
  ini harus dikerjakan ulang.
- Gap yang ditemukan (layer belum ada, kedalaman kurang, kapasitas Rust yang belum kepake) itu semua
  ADDITIVE — nambah di atas fondasi yang ada — bukan cacat arsitektur yang butuh dirombak.

---

## 📊 Audit Status (per 2026-07-28) — vs `THE MASTER AR BEAUTY PIPELINE & DICTIONARY.md`

| Layer | Status | Detail |
|-------|--------|--------|
| Face Shape Classification | ✅ **Match persis** | Formula `R_aspect`/`R_jaw_cheek`/`R_forehead_jaw` + threshold identik dengan dictionary |
| Contour | ✅ **Match persis** | Formula 60% Multiply + 40% Linear Burn identik. Style: 4 generik (bukan 6 teknik per-face-shape bernama) |
| Blush | ✅ **Match persis (formula)** | 65% Normal + 35% Soft Light identik. Style: 3 dari 9 teknik bernama di dictionary |
| Foundation | 🟡 Dasar ada | 5 finish match (matte/dewy/sheer/satin/luminous). Belum: CIELAB/ITA color matching, kompensasi ashiness kulit gelap |
| Concealer | 🟡 Dasar ada | 4 style, taksonomi sebagian match. Belum: varian brightening/sculpting terpisah |
| Highlighter | 🟡 Dasar ada | 1 region/style. Dictionary minta 4 tipe × 8 area aplikasi |
| Eyeshadow | 🟡 Dasar ada | 4 style dari 8 teknik. **Blink dynamics (rigging saat kedip) belum ada** meski blendshape sudah dipakai untuk AO |
| Lipstick | 🟡 Sangat dasar | Cuma multiply polos. Belum: preservasi kerutan bibir, ombré, aturan overline |
| **Softlens/Iris** | 🔴 **Nol** | Buffer 478-titik (468 wajah + 10 iris) sudah disiapkan di renderer, logika/shader belum ada sama sekali |
| **Eyeliner** | 🔴 **Nol** | Tidak ada sama sekali di kode |
| **Eyebrows** | 🔴 **Nol** | Tidak ada sama sekali di kode |
| **Eyelashes** | 🔴 **Nol** | Tidak ada sama sekali di kode |

**Temuan kunci:** Deteksi face-shape/eye-shape/nose-shape SUDAH akurat dan jalan (dipanggil via morphology
scan), tapi hasilnya cuma ditampilkan di kartu diagnostik UI — **tidak dipakai untuk auto-rekomendasi
teknik**. Ini gap termurah untuk ditutup dengan dampak besar, karena datanya sudah ada.

---

## FASE 1: Isi Kekosongan Layer (0% → ada)

Urutan: Eyeliner → Eyebrows → Softlens → (Eyelashes ditunda ke Fase 5, butuh extrusion 3D paling kompleks).

Setiap layer: riset TAMO dulu (bagaimana Snap/YouCam render eyeliner vector/eyebrow-hair-stroke/iris-lens
secara real-time) → desain shader mengikuti pola PASS-baking yang sudah terbukti (persis pola yang dipakai
untuk Contour/Blush/AO-Hairline sesi ini: bake per-vertex weight ke FBO channel, sample di compositing
shader) → implementasi via Haiku dengan instruksi presisi lintas semua file yang perlu diubah bersamaan.

- **Eyeliner**: vector-based (bukan per-vertex blob) — dictionary sudah punya 8 tipe + adaptasi per eye-shape.
- **Eyebrows**: texture-strand rendering (bukan blok solid — dictionary tegas "Haram memblok padat").
- **Softlens**: pakai landmark iris 468-477 yang sudah tersedia dari MediaPipe, ikuti dictionary §6 (pupil
  hole masking + corneal glint re-injection supaya mata tidak terlihat "mati").

## FASE 2: Auto-Rekomendasi Berbasis Deteksi Wajah

Sambungkan `faceShape`/`eyeShape`/`noseShape` (sudah akurat) ke tabel mapping teknik-per-bentuk-wajah yang
sudah lengkap di dictionary (§3B Contour, §4B Blush, §5D Highlighter, §9B Eyebrows). User tetap bisa
override manual, tapi default suggestion harus otomatis dari AI, bukan user pilih buta dari 4 pilihan generik.

## FASE 3: Perdalam Layer yang Sudah Ada

Prioritas berdasar "paling kena mata" duluan:
1. **Lipstick** — preservasi kerutan bibir (`lipDetailRatio`), aturan overline (haram overline sudut mulut),
   ombré Korean vs Western.
2. **Blush** — 9 style bernama (Apple Cheek, Draping, Igari, Halo, Contour Blush, Under-eye, Monochromatic,
   Sunset Ombré, Boy Blush).
3. **Highlighter** — 4 tipe (Powder/Liquid/Baked-Holographic/Subtle Glow) × 8 area aplikasi presisi.
4. **Eyeshadow** — 8 teknik penuh + blink dynamics (vertex weight rigging saat kedip, infra blendshape sudah ada).
5. **Foundation** — CIELAB/ITA color matching (§1D dictionary) + kompensasi ashiness kulit gelap (§1C).

## FASE 4: Kecerdasan Real (Bukan Cuma Render)

Dari brainstorm "next-level" — dua fitur paling bernilai karena kompetitor jarang kerjakan dengan benar:
- **Skin analysis** (`fizgravity_engine_analyze_skin_health` — sudah ADA & lengkap di Rust, belum tersambung)
  → roughness/wrinkle detection → rekomendasi produk personal, bukan cuma overlay kosmetik.
- **Lighting estimation** (`estimate_ambient_sh` — sengaja dikosongkan dulu karena riwayat bug stack
  corruption, perlu diimplementasi ulang dengan hati-hati) → makeup beradaptasi ke suhu warna cahaya asli.
  Ini yang bedain "AR makeup meyakinkan" vs "kayak stiker nempel" — prioritas tertinggi untuk melangkahi
  kelas atas karena paling jarang dikerjakan dengan benar oleh kompetitor.

## FASE 5: Revisi Arsitektur Render — Pelajaran dari Sesi Ini

- ⚠️ **IMU frame-warping (rencana lama)**: PERNAH DICOBA sesi ini, **GAGAL**, sudah di-revert. Ide dasarnya
  benar secara prinsip (ARKit/ARCore beneran pakai reprojection IMU), tapi implementasi kita cuma
  menggeser vertex overlay sementara gambar kamera tetap beku — overlay jadi "mengambang" dari gambar,
  itu sumber getarnya. Kalau mau dikejar lagi ke depan: **wajib riset TAMO dulu** teknik reprojection
  gambar penuh via image warp/homography (bukan cuma vertex offset) — kompleksitas & risiko tinggi, cuma
  layak dikejar kalau MediaPipe kembali jadi bottleneck nyata (saat ini sudah teratasi via downscaled
  tracking input, ~28ms rata-rata, stabil).
- **Camera2 + Preview surface binding**: sekarang cuma pakai `ImageAnalysis` tanpa `Preview`, terbukti
  dapat perlakuan 3A/AWB yang beda dari kamera bawaan. Tambahkan Preview surface (bisa minimal/tersembunyi)
  untuk convergence 3A yang lebih akurat, atau turun penuh ke Camera2 API tanpa CameraX.
- **Vulkan API**: migrasi jangka panjang dari OpenGL ES untuk kontrol sinkronisasi GPU eksplisit dan
  performa lebih baik di device low-end — evaluasi setelah Fase 1-4 selesai, bukan prioritas sekarang.

## FASE 6: Commerce & Sosial

- Export video/foto pakai pipeline render yang SAMA PERSIS dengan preview (bukan jalur kualitas terpisah
  yang lebih rendah — banyak kompetitor gagal di sini).
- Hand-tracking gesture UI nyata (infra JNI sudah ada tapi model Rust-nya sekarang MOCK/data palsu — perlu
  model ONNX asli sebelum layak dipakai).
- Color-accurate shade matching ke SKU produk asli → jembatani try-on virtual ke pembelian nyata.

---

*Dokumen ini adalah kitab kerja Fizgravity AR Engine — setiap keputusan implementasi fitur baru merujuk
balik ke sini dan ke `THE MASTER AR BEAUTY PIPELINE & DICTIONARY.md`.*
