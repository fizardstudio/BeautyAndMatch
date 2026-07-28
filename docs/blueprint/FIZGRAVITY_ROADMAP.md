# 🚀 FIZGRAVITY AR — THE NEXT LEVEL ROADMAP (v3.1)
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

Contoh kenapa ini penting: rencana lama roadmap ini pernah menaruh "IMU Sensor Fusion Warp" sebagai solusi
zero-latency, dan kita coba implementasikan sesi ini — hasilnya malah bikin getar parah, harus di-revert.
Root cause: ide dasarnya BENAR (ARKit/ARCore memang pakai reprojection berbasis IMU), tapi implementasi
kita cuma menggeser posisi vertex overlay, bukan me-reproject seluruh frame kamera via image warp/homography
seperti yang sebenarnya dilakukan ARKit/ARCore. Andai kita riset dulu teknik aslinya secara detail, kita
akan tahu itu perlu warping GAMBAR, bukan cuma overlay — dan bisa putuskan dari awal apakah itu sepadan
dengan kompleksitasnya. Lihat Fase 5 untuk detail lengkap.

---

## 🎯 Keputusan Strategis: EVOLUSI, Bukan Mulai dari Nol

**MatchAndBeauty adalah produk utamanya** — Fizgravity-AR-Engine (repo Rust terpisah) itu mesinnya, bukan
tujuan sendiri. Setiap fase di bawah ini ada karena dampaknya ke pengalaman AR Try-On MatchAndBeauty,
bukan demi kelengkapan engine semata.

Pertanyaan yang sempat muncul: apakah perlu restart total mengikuti brainstorming baru untuk hyper-realistic
next-level, atau bisa lanjutkan dari aplikasi sekarang? **Jawaban: EVOLUSI.** Alasannya:
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
| **Softlens/Iris** | 🔴 **Nol** | Buffer 478-titik (468 wajah + 10 iris) sudah disiapkan di renderer, tapi **belum diverifikasi apakah MediaPipe FaceLandmarker kita sekarang benar-benar mengeluarkan landmark iris** (opsi `refine_landmarks`/iris) — cek ini SEBELUM janji fitur |
| **Eyeliner** | 🔴 **Nol** | Tidak ada sama sekali di kode |
| **Eyebrows** | 🔴 **Nol** | Tidak ada sama sekali di kode |
| **Eyelashes** | 🔴 **Nol** | Tidak ada sama sekali di kode |

**Temuan kunci:** Deteksi face-shape/eye-shape/nose-shape SUDAH akurat dan jalan (dipanggil via morphology
scan), tapi hasilnya cuma ditampilkan di kartu diagnostik UI — **tidak dipakai untuk auto-rekomendasi
teknik**. Ini gap termurah untuk ditutup dengan dampak besar, karena datanya sudah ada.

---

## ⚖️ Catatan Penting Soal Urutan Fase

`THE MASTER AR BEAUTY PIPELINE & DICTIONARY.md` adalah dokumen **SPEK** — rujukan CARA membangun tiap
layer dengan benar (formula shader, blend mode, landmark mapping). Dokumen itu **bukan** penentu urutan
prioritas pengerjaan. Urutan fase di bawah ini murni penilaian dampak-per-usaha dan risiko teknis,
bukan urutan kelengkapan layer di dictionary. Itu sebabnya lighting estimation (bukan bagian dari 11
layer makeup di dictionary sama sekali) ditaruh paling depan — dampaknya ke SEMUA layer sekaligus,
lebih tinggi dari menambah satu layer atau satu gaya baru manapun.

---

## FASE 0: Naikkan Fondasi ke Standar Next-Level (BUKAN Cuma "Sudah Jalan")

Fondasi kamera/render sesi 2026-07-28 sudah **solid dan terbukti** — tapi "solid" ≠ "next-level".
Sebelum lanjut ke Fase 1+, tutup dulu gap konkret berikut (audit jujur per 2026-07-28):

| Item | Status | Gap ke next-level |
|---|---|---|
| Resolusi kamera | ✅ Solid, terverifikasi data | — |
| Tracking MediaPipe real-time | ✅ Solid, terverifikasi data | — |
| AWB / color science | 🟡 Jalan (default OS) | Belum ada color pipeline yang di-engineer sendiri (profile per-device), cuma pakai auto-AWB apa adanya |
| EIS (stabilisasi) | 🟡 Aktif, efeknya belum diukur | Baru terverifikasi HAL menerapkannya — belum pernah diukur kuantitatif seberapa besar mengurangi shake |
| Kalibrasi kamera per-device | 🔴 **Gap nyata** | Semua tuning baru divalidasi di SATU device. App kelas atas biasa punya fallback/tuning untuk populasi hardware kamera Android yang beragam |
| Render-loop smoothness | 🟡 Stabil, di bawah standar kelas atas | ~16-20fps (ngikutin kecepatan deteksi). Kompetitor kelas atas biasa lebih mulus (30fps+) lewat reprojection — sengaja ditunda ke Fase 6, jadi ini gap yang SUDAH DIKETAHUI, belum ditutup |
| Fondasi compositing FBO | 🟡 Stabil untuk pass yang ada, baru kegoyang saat diperluas | Percobaan perluasan terakhir (AO/hairline) nemu bug yang akar masalahnya belum ditemukan — pemahaman kita soal batas aman arsitektur ini masih ada lubang |

### 0.1 — Checkpoint Retuning Visual (WAJIB sebelum Fase 1 dianggap "selesai")

11 layer makeup yang sudah ada di-tuning secara visual (opacity default, blur radius, konstanta warna)
**di atas pipeline yang saat itu belum stabil dan resolusinya salah** (sebelum sesi perbaikan kamera/render
2026-07-28). Rumus shader-nya sendiri tetap valid, tapi angka tuning-nya kemungkinan besar butuh dikalibrasi
ulang sekarang fondasinya sudah benar. Setelah Lighting Estimation (di bawah) aktif, "kanvas" berubah lagi
(warna kena koreksi cahaya) — jadi retuning ini paling masuk akal dilakukan SEKALI, SETELAH lighting
estimation selesai, bukan dua kali terpisah. Cek ulang tiap layer: default opacity, blur radius,
konstanta warna (litFoundation offset, dst) — pakai wajah asli di kondisi cahaya berbeda, bukan cuma satu skenario.

### 0.2 — Root-cause bug AO/hairline yang belum terpecahkan

Sebelum memperluas compositing FBO lagi (Fase 5), selesaikan dulu misteri kenapa `hairlineBlend` terbaca
0 di seluruh layar saat fitur itu diaktifkan (root cause belum ditemukan, sekarang cuma dinonaktifkan).
Tanpa ini, kepercayaan diri kita soal "aman menambah PASS baru ke compositing pipeline" masih rapuh.

### 0.3 — Validasi multi-device

Minimal 2-3 device Android dengan chipset/kamera berbeda (bukan cuma device abang sekarang) sebelum
mengklaim fondasi ini next-level — banyak asumsi (stream kamera 1920x1080, AWB behavior, EIS availability)
divalidasi cuma dari SATU `dumpsys media.camera`.

---

## FASE 1: Lighting Estimation — Unlock Realisme Terbesar

**Kenapa duluan:** ini bukan nambah satu layer baru, tapi bikin SEMUA layer yang sudah ada (foundation,
blush, contour, dan semua yang ditambah nanti) langsung terlihat jauh lebih meyakinkan sekaligus —
respons ke suhu warna cahaya asli itu yang bedain "AR makeup meyakinkan" vs "kayak stiker nempel".
Investasi dengan hasil berlipat, dan paling jarang dikerjakan dengan benar oleh kompetitor.

- `fizgravity_engine_get_ambient_cct_and_intensity` & `estimate_ambient_sh` — sudah ada algoritmanya
  (McCamy CCT estimation lengkap + test), tapi `estimate_ambient_sh` **sengaja dikosongkan** karena
  riwayat bug stack corruption. Riset TAMO dulu (bagaimana ARKit/ARCore/Spark AR melakukan real-time
  ambient lighting estimation dari kamera tanpa depth sensor) → identifikasi apa yang menyebabkan stack
  corruption sebelumnya → implementasi ulang hati-hati, bukan asal aktifkan lagi.

## FASE 2: Auto-Rekomendasi Berbasis Deteksi Wajah

Murah (datanya sudah ada dari morphology scan), dampaknya langsung ke persepsi "app ini pintar."
Sambungkan `faceShape`/`eyeShape`/`noseShape` ke tabel mapping teknik-per-bentuk-wajah yang sudah
lengkap di dictionary (§3B Contour, §4B Blush, §5D Highlighter, §9B Eyebrows). User tetap bisa override
manual, tapi default suggestion harus otomatis dari AI, bukan user pilih buta dari pilihan generik.

## FASE 3: Eyeliner + Eyebrows (BUKAN Softlens Dulu)

Dua kategori makeup paling umum diharapkan ada di app manapun, dan risiko teknisnya paling rendah di
antara 4 layer yang masih kosong (pola render 2D/tekstur, mirip infrastruktur yang sudah terbukti jalan
di Contour/Blush/AO-Hairline). Softlens sengaja ditunda — selain lebih niche penggunaannya, prasyarat
teknisnya (landmark iris) belum terverifikasi aktif di pipeline kita sekarang (lihat tabel audit di atas).

Tiap layer: riset TAMO dulu → desain shader ikuti pola PASS-baking yang terbukti (bake per-vertex weight
ke FBO channel, sample di compositing shader) → implementasi via Haiku dengan instruksi presisi lintas
semua file yang berubah bersamaan.

- **Eyeliner**: vector-based (bukan per-vertex blob) — dictionary §8 sudah punya 8 tipe + adaptasi per eye-shape.
- **Eyebrows**: texture-strand rendering (bukan blok solid — dictionary tegas "Haram memblok padat").

## FASE 4: Perdalam Lipstick + Foundation

Dua layer yang paling sering dipakai & paling dinilai user soal akurasi — prioritas lebih tinggi dari
menambah variasi gaya Blush/Highlighter/Eyeshadow yang sifatnya "nice to have", bukan "harus akurat".

- **Lipstick**: preservasi kerutan bibir (`lipDetailRatio`), aturan overline (haram overline sudut mulut),
  ombré Korean vs Western — dictionary §10.
- **Foundation**: CIELAB/ITA color matching (§1D) + kompensasi ashiness kulit gelap (§1C).

## FASE 5: Softlens + Perluas Katalog Layer Lain

Setelah prasyarat iris landmark terverifikasi (Fase 3 audit):
- **Softlens** — dictionary §6, pupil hole masking + corneal glint re-injection.
- **Blush** — 9 style bernama (Apple Cheek, Draping, Igari, Halo, Contour Blush, Under-eye, Monochromatic,
  Sunset Ombré, Boy Blush).
- **Highlighter** — 4 tipe (Powder/Liquid/Baked-Holographic/Subtle Glow) × 8 area aplikasi presisi.
- **Eyeshadow** — 8 teknik penuh + blink dynamics (vertex weight rigging saat kedip, infra blendshape sudah ada).
- **Eyelashes** — paling terakhir dari semua layer, butuh extrusion 3D paling kompleks.
- **Skin analysis** (`fizgravity_engine_analyze_skin_health` — sudah ADA & lengkap di Rust, belum
  tersambung) → roughness/wrinkle detection → rekomendasi produk personal.

## FASE 6: Revisi Arsitektur Render — Pelajaran dari Sesi Ini

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
  performa lebih baik di device low-end — evaluasi setelah Fase 1-5 selesai, bukan prioritas sekarang.

## FASE 7: Commerce & Sosial

- Export video/foto pakai pipeline render yang SAMA PERSIS dengan preview (bukan jalur kualitas terpisah
  yang lebih rendah — banyak kompetitor gagal di sini).
- Hand-tracking gesture UI nyata (infra JNI sudah ada tapi model Rust-nya sekarang MOCK/data palsu — perlu
  model ONNX asli sebelum layak dipakai).
- Color-accurate shade matching ke SKU produk asli → jembatani try-on virtual ke pembelian nyata.

---

*Dokumen ini adalah kitab kerja MatchAndBeauty AR Try-On — setiap keputusan implementasi fitur baru
merujuk balik ke sini (untuk PRIORITAS) dan ke `THE MASTER AR BEAUTY PIPELINE & DICTIONARY.md`
(untuk CARA membangunnya dengan benar).*
