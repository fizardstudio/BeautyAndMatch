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
| Fondasi compositing FBO | ✅ AO/hairline root-cause ketemu & fixed (2026-07-28) | Bug utamanya kontrak return-value FFI Rust (return 0=sukses padahal caller expect count), bukan arsitektur FBO-nya sendiri — lihat 0.2 di bawah |

### 0.1 — Checkpoint Retuning Visual (WAJIB sebelum Fase 1 dianggap "selesai")

11 layer makeup yang sudah ada di-tuning secara visual (opacity default, blur radius, konstanta warna)
**di atas pipeline yang saat itu belum stabil dan resolusinya salah** (sebelum sesi perbaikan kamera/render
2026-07-28). Rumus shader-nya sendiri tetap valid, tapi angka tuning-nya kemungkinan besar butuh dikalibrasi
ulang sekarang fondasinya sudah benar. Setelah Lighting Estimation (di bawah) aktif, "kanvas" berubah lagi
(warna kena koreksi cahaya) — jadi retuning ini paling masuk akal dilakukan SEKALI, SETELAH lighting
estimation selesai, bukan dua kali terpisah. Cek ulang tiap layer: default opacity, blur radius,
konstanta warna (litFoundation offset, dst) — pakai wajah asli di kondisi cahaya berbeda, bukan cuma satu skenario.

### 0.2 — Root-cause bug AO/hairline (✅ SELESAI 2026-07-28)

Root cause ketemu, bukan satu bug tapi rantai tiga:

1. **Kontrak return-value FFI Rust salah** — `fizgravity_engine_calculate_dynamic_ao` dan
   `_calculate_hairline_blending` me-return `0` di jalur sukses (gaya C "0=OK"), padahal setiap fungsi FFI
   lain di file yang sama (mis. `get_stabilized_landmarks`) me-return JUMLAH ELEMEN yang ditulis. Wrapper
   JNI C++ menganggap return value itu count dan bail kalau `n<=0` — jadi kedua panggilan itu SELALU dianggap
   gagal walau datanya sudah benar dan buffer sudah keisi. Efeknya PASS 1f (bake `auxMaskFbo`) nggak pernah
   jalan, dan karena clear-ke-netral-nya ikut kebungkus di dalam pengecekan yang sama, `auxMaskFbo` tertinggal
   di nilai nol — bikin `foundationMask *= hairlineBlend` jadi nol PERMANEN dan **semua layer makeup mati
   total**, bukan cuma AO/hairline-nya. Fix: `Fizgravity-AR-Engine@fed6bfd` (return count, bukan 0) + pindah
   clear netral `(1,1,0,0)` ke luar pengecekan data-availability.
2. **Formula AO mulut kebalik** — mulut tertutup (`jawOpen=0`) malah dapat AO gelap (0.15), mulut kebuka cuma
   sampai 0.85. Sudah dibalik: tertutup = netral (1.0), kebuka = gelap (0.15).
3. **`auxMaskFbo` nggak pernah di-blur** — beda dengan `maskFbo` yang sudah lewat PASS 2 (blur 3px), jadi
   hairline-fade nongol sebagai garis banding tegas di dahi (interpolasi vertex yang jarang), bukan gradien.
   Fix: PASS 2b baru (`MatchAndBeauty@81dfe6e`) meniru PASS 2 persis, blur `auxMaskFbo` → `auxMaskBlurFbo`.

Sudah diverifikasi di device fisik: makeup (blush/lipstik/dst) balik normal, AO dinamis di lipatan
hidung/mata & fade hairline aktif dan smooth, AO mulut nggak nyangkut pas mingkem. Ditemukan juga (tidak
di-scope ke sesi ini): area bibir 0% ke-cover foundation dengan tepi tegas — kemungkinan besar disengaja
(foundation tidak boleh nutup bibir, itu tugas layer lipstik terpisah), belum diverifikasi ke kode-nya
langsung, dicatat sebagai item terpisah untuk nanti.

**Addendum (2026-07-29) — fitur "dynamic AO" dihapus setelah riset TAMO.** Setelah bug di atas selesai
dan efeknya akhirnya kelihatan (sebelumnya nggak pernah nyala sama sekali), user menyadari efeknya
sendiri terasa nggak perlu — bukan bug, tapi pertanyaan desain: apa AO ini memang seharusnya ada?
Riset ke YouCam/Perfect Corp, ModiFace, Banuba, Snap Lens Studio: **tidak ada satupun app kelas atas
yang punya layer ambient-occlusion berdiri sendiri, always-on, di luar kontrol user.** Shading semacam
ini selalu jadi sub-behavior Contour (persis spec holy-grail kita — hairline shading = teknik Contour,
bukan layer sendiri) atau didorong oleh lighting estimation real-time. Darkening always-on di nasolabial
fold & sudut mata malah anti-pattern dikenal (bikin wajah kelihatan capek/tua). **Keputusan: dihapus**
(`MatchAndBeauty@df4550d`) — `auxMaskFbo`, bake/blur pass, dan shader sampling-nya dicabut semua. Fungsi
FFI Rust-nya (`fizgravity_engine_calculate_dynamic_ao`/`_hairline_blending`) TIDAK dihapus, sengaja
dibiarkan lengkap + didokumentasikan buat dipakai ulang kalau nanti Contour beneran butuh teknik
hairline-nya (sesuai spec) atau Fase 1 (Lighting Estimation) butuh AO yang adaptif ke cahaya asli.

**Temuan sampingan → fixed juga**: user (dahi lebar) melihat cakupan foundation berhenti jauh di bawah
garis rambut asli. Root cause: bounding box mask cuma dihitung dari 478 landmark MediaPipe, dan landmark
teratasnya (10) memang nggak pernah sampai ke garis rambut (MediaPipe tidak melacak rambut sama sekali)
— jadi orang berdahi lebar/tinggi selalu nyisa area polos lebih banyak. Riset antropometri (Farkas-style):
rasio glabella(168)-ke-subnasale(2) ≈ 0.84× jarak brow-ke-hairline yang sebenarnya — dipakai buat
extrapolate bounding box ke arah dahi. **Ini heuristik fallback, BUKAN deteksi hairline asli** — app
kelas atas (Banuba dkk.) pakai deteksi warna piksel di atas dahi atau model segmentasi wajah
(BiSeNet/CelebAMask-HQ, kelas "skin" vs "hair" terpisah) buat akurasi per-orang. Segmentasi rambut asli
dicatat sebagai follow-up terpisah (butuh model ML tambahan), bukan dikerjakan sesi ini.

### 0.4 — Hair segmentation asli (percobaan MediaPipe malam ini: DIBATALKAN, coba lagi lewat Fizgravity/ONNX)

Percobaan pertama (2026-07-29 dini hari): pakai model resmi Google `hair_segmenter.tflite` via MediaPipe
`ImageSegmenter` (library `tasks-vision` yang sama dengan `FaceLandmarker`), untuk gate `foundationMask`
per-piksel presisi (bukan cuma heuristik geometri 0.3 di atas). **Dibatalkan setelah 2 masalah nyata
ketemu berturut-turut:**

1. **Crash native di GPU delegate** — `image_frame.cc:291 Format UNKNOWN`, bug upstream MediaPipe yang
   masih terbuka (`google-ai-edge/mediapipe-samples#484`, `mediapipe#5265`, `#5503`, `#5788`): jalur
   convert GPU-texture→CPU-ImageFrame buat output confidence-mask (bukan buat landmark biasa) gagal di
   banyak device Android, nggak ada fix resmi — semua thread solusinya "pakai CPU delegate". Fixed dengan
   paksa `Delegate.CPU` (FaceLandmarker tetap aman di GPU karena outputnya cuma koordinat angka, bukan
   gambar/mask — beda jalur konversi sama sekali).
2. **Mismatch orientasi** — mask hasil segmenter (640×360, sesuai `trackingBitmap` mentah/landscape
   sensor) di-sample di shader compositing pakai `vTexCoord` yang sudah dalam ruang portrait/ter-rotasi
   (sama seperti `sCameraTex`). Landmark wajah sudah dapat koreksi rotasi eksplisit di Kotlin
   (`fa[i*3]=1-fl[i].y()`, swap x/y), tapi mask gambar hair segmenter nggak dapat koreksi yang sama —
   hasilnya foundation cuma nongol di garis vertikal sempit yang kebetulan align, bukan ngikutin bentuk
   wajah. Belum di-fix (diputuskan cabut fitur, bukan tambal).

**Arah selanjutnya (disepakati bareng user, TAMO penuh di sesi baru)**: bangun hair segmentation sendiri
lewat **Fizgravity-AR-Engine (Rust) + ONNX Runtime**, BUKAN lewat MediaPipe Java Task API — sepenuhnya
menghindari bug GPU di atas (karena nggak lewat jalur convert MediaPipe sama sekali) dan kita pegang
kontrol penuh soal orientasi dari desain awal. **Preseden sudah ada di codebase**: `Fizgravity-AR-Engine/
src/face.rs` (`FaceModelSession`, pakai crate `ort` yang sudah ada di `Cargo.toml`) sudah punya pipeline
inference ONNX yang jalan (dipakai sebagai fallback tracker wajah 256×256×3 → 468 titik) — pattern-nya
tinggal diadaptasi buat model segmentasi (input gambar → output mask per-piksel, bukan koordinat).
Sisi C++/shader (`sHairMaskTex`, `foundationMask *= (1.0 - hairConfidence)`, upload tekstur `GL_LUMINANCE`)
yang ditulis Haiku malam ini masih valid secara desain dan bisa dipakai ulang — cuma sumber datanya yang
ganti dari Kotlin/MediaPipe ke Rust/Fizgravity.

**PR buat sesi berikutnya**: cari/siapkan model segmentasi rambut format ONNX (`hair_segmenter.tflite`
yang sudah di-download nggak kepakai lagi, TFLite ≠ ONNX — perlu model ONNX terpisah atau hasil konversi),
tulis fungsi inference Rust baru mengikuti pattern `FaceModelSession`, tambah JNI bridge baru, pastikan
orientasi output mask konsisten dengan `sCameraTex`/landmark sejak awal desain.

### 0.3 — Ketahanan Lintas-Device (bukan "harus punya banyak HP")

Constraint nyata: cuma ada satu device fisik untuk testing. Solusinya BUKAN beli banyak HP — dua jalan
yang realistis:

1. **Tulis kode defensif, jangan asumsikan kapabilitas device.** Bug konkret yang SUDAH ada sekarang:
   kita minta `CONTROL_VIDEO_STABILIZATION_MODE_ON` tanpa cek dulu apakah `availableVideoStabilizationModes`
   device itu beneran mendukungnya — kebetulan device testing kita punya, tapi di device lain yang tidak,
   perilakunya tidak terjamin (silent-ignore atau berpotensi masalah). Perbaikannya: query kapabilitas
   kamera saat runtime (`CameraCharacteristics`) dan hanya minta fitur yang benar-benar terdaftar tersedia,
   dengan fallback yang jelas kalau tidak — bukan asumsi "device saya punya, berarti semua punya". Audit
   semua tempat lain di kode yang mengasumsikan kapabilitas kamera tanpa cek runtime (resolusi, AE range,
   dst) dengan pola yang sama.
2. **Firebase Test Lab** (tier gratis terbatas — beberapa run/hari) untuk spot-check periodik di device
   fisik REAL milik Google secara remote, tanpa perlu beli unit sendiri. Bukan pengganti testing utama,
   cukup buat verifikasi sesekali sebelum rilis besar bahwa asumsi runtime di atas benar-benar bekerja
   di chipset/kamera lain.

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
