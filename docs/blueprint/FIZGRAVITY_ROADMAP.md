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
hidung/mata & fade hairline aktif dan smooth, AO mulut nggak nyangkut pas mingkem. Ditemukan juga: area
bibir 0% ke-cover foundation dengan tepi tegas.

**✅ SELESAI (2026-07-29, `MatchAndBeauty@4cf3d20`)** — diriset & di-fix. Kode ASLI ternyata TIDAK punya
exclusion eksplisit buat bibir luar (cuma `INNER_LIPS_INDICES`, lubang rongga mulut, yang dipotong; radial
fade mask.r sendiri floor di 0.4, nggak pernah nol) — tapi riset MUA + matematika blend lipstik kita
(multiply, mode blend paling nggak "maafin" warna non-netral di bawahnya) sama-sama nunjuk ke satu
rekomendasi: bibir HARUS dikecualikan dari foundation. Teknik MUA asli nunjukin priming bibir (kalau ada)
pakai corrector netral yang restrained, BUKAN warna foundation pilihan user yang bisa sembarang. Fix:
`foundationMask` dipisah jadi lebih sempit dari `faceMask` (gate umum buat layer lain), pakai
`lipMaskFbo` yang udah ada (`foundationMask = faceMask * (1 - lipRawMask)`) — reuse infrastruktur,
falloff halus, nggak perlu geometri baru. Diverifikasi device: bibir bersih dari cyan test, tepi halus.

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

1. **Tulis kode defensif, jangan asumsikan kapabilitas device.** ✅ Fixed & audited lengkap (2026-07-29).
   Bug awal: kita minta `CONTROL_VIDEO_STABILIZATION_MODE_ON` tanpa cek dulu apakah
   `availableVideoStabilizationModes` device itu beneran mendukungnya — kebetulan device testing kita
   punya, tapi di device lain yang tidak, perilakunya tidak terjamin. Fixed: query kapabilitas kamera saat
   runtime (`CameraCharacteristics`), minta fitur cuma kalau terdaftar tersedia, fallback jelas kalau
   tidak (sama buat `CONTROL_AE_TARGET_FPS_RANGE`). **Audit lanjutan** (semua tempat lain yang mungkin
   asumsi kapabilitas tanpa cek) sudah dikerjakan — hasilnya bersih: `ResolutionStrategy`/`AspectRatioStrategy`
   CameraX sudah resolve terhadap `StreamConfigurationMap` device secara internal (nggak perlu dicek
   manual), `OUTPUT_IMAGE_FORMAT_RGBA_8888` itu konversi software CameraX sendiri (bukan capability
   hardware), pembacaan AWB/gains cuma diagnostic logging read-only, dan absennya kamera depan udah
   ke-handle try/catch di `bindToLifecycle`. Nggak ada temuan baru yang butuh fix.
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

- `fizgravity_engine_get_ambient_cct_and_intensity` — CCT/intensity dari koefisien SH (formula McCamy)
  sudah lengkap dan valid, tetap dipakai. `estimate_ambient_sh` (yang isi koefisien SH-nya) sengaja
  dikosongkan sejak commit `a8fc0ea` karena "stack corruption", dan **belum pernah tersambung ke app
  sama sekali** — cuma dipanggil dari `fizgravity_engine_update_frame` di Rust, yang sendirinya nggak
  pernah dipanggil dari Kotlin/C++ manapun di MatchAndBeauty. Jadi murni kode mati, belum pernah diuji
  dengan data kamera asli.

  **Riset TAMO (2026-07-29) — hasilnya tegas: JANGAN tambal, GANTI algoritmanya.** Implementasi lama
  proyeksikan SH langsung dari piksel mentah foto selfie, seolah-olah foto itu environment map (bola
  cermin/foto 360°). Ini **category error** — selfie itu foto WAJAH (permukaan diffuse yang KENA
  cahaya), bukan foto sumber cahayanya. Riset (ECCV 2018, CGF 2018, dan terutama Google **"Portrait
  Light" SIGGRAPH Asia 2020** — real-time di smartphone, order SH sama persis: 9 koefisien × RGB) nunjukin
  teknik yang benar buat front-facing camera adalah **"face as light probe"**: pakai geometri wajah yang
  SUDAH kita punya (normal permukaan dari 468 landmark FaceLandmarker) + asumsi kulit reflektor diffuse
  seragam, solve inverse problem (shading yang keliatan di wajah → kombinasi cahaya SH yang paling
  cocok), BUKAN proyeksi piksel mentah ke arah ray kamera. Kontrak output (9 koefisien × RGB) tetap sama,
  jadi konsumen di render pipeline nggak perlu didesain ulang — cuma cara HITUNGnya yang ganti total.
  Bug "stack corruption" di implementasi lama juga kemungkinan besar root cause-nya sudah ketahuan
  (pembacaan piksel `pixels.add(pixel_offset)` di `estimate_ambient_sh` versi lama mengasumsikan buffer
  RGB rapat tanpa row-stride padding — pola bug yang sama persis dengan yang sudah kita hindari di sisi
  Kotlin `FizgravityARView.kt` untuk `trackingBitmap`) — tapi jadi moot karena algoritmanya diganti,
  bukan didaur ulang.

  Sumber riset: arXiv 2301.06143 (Multi-Camera Lighting Estimation for Front-Facing Mobile AR — konfirmasi
  front-facing-only itu genuinely sulit, solusi mereka nambah kamera belakang; kita nggak perlu seambisius
  itu karena cuma butuh lighting buat makeup relighting, bukan rekonstruksi lingkungan penuh), Google
  Portrait Light (arxiv.org/pdf/2008.02396), Calian et al. "From Faces to Outdoor Light Probes" (CGF 2018).

**Alur implementasi (biar nggak kerja dua kali — tiap step checkpoint jelas sebelum lanjut):**

1. ✅ **SELESAI (2026-07-29, `Fizgravity-AR-Engine@22c6da7`)** — Algoritma Rust `estimate_ambient_sh`
   ditulis ulang total pakai face-as-light-probe: exclude landmark mata/alis/bibir
   (`is_valid_skin_sample`, index dicross-check dari `FizgravityMakeupIndices.h`), proyeksi SH pakai
   normal geometris asli (`face::compute_face_normals`) + warna piksel di titik landmark itu, row-stride
   eksplisit + validasi `buffer_len_bytes` NYATA sebelum unsafe read (langsung menyasar kelas bug "stack
   corruption" yang lama). 47/47 test Rust lulus (4 baru: exclude-region check, symmetric-normal math
   check — diverifikasi manual aljabar SH-nya, row-stride padding, out-of-bounds safety). Masih Rust-only,
   BELUM disambungkan ke app.
2. ✅ **SELESAI (2026-07-29, `Fizgravity-AR-Engine@5690e69` + `MatchAndBeauty@3982e65`)** — Wiring
   JNI/Kotlin: entry point FFI baru `fizgravity_engine_estimate_lighting` (dedicated, terpisah dari
   `fizgravity_engine_update_frame` yang berat/penuh side-effect), bridge JNI `fizgravityEstimateLighting`
   di `FizgravityJNI.cpp` (pakai `GetDirectBufferCapacity` asli, bukan tebakan `width*height`), dipanggil
   throttled (tiap 5 frame) dari `FizgravityARView.kt` lewat downscale 128×128 khusus (bukan reuse
   trackingBitmap 640px — algoritmanya cuma sample ~280 titik landmark, convert seluruh bitmap besar
   percuma).
3. ✅ **SELESAI (`MatchAndBeauty@3982e65`)** — Konsumsi di compositing shader: `nativeSetAmbientLighting`
   (plain setter, aman dipanggil dari thread manapun — bukan texture upload kayak percobaan hair mask yang
   gagal), uniform `uAmbientCCT`/`uAmbientIntensity`, `cctToTint()` nge-tint `litFoundation` warm/cool
   relatif ke 6500K netral. Default (6500K, intensity 1.0) resolve ke no-op identity sampai data asli
   masuk. **Diverifikasi di device**: nggak crash, semua uniform ter-resolve, log real-time nunjukin nilai
   NYATA yang stabil & masuk akal (CCT≈4850K, intensity≈0.85 — bukan cuma default), bukan cuma "build
   sukses".
   
   Dikerjakan 3 track paralel (2 subagent + mandor buat glue Kotlin yang butuh konteks pipeline kamera
   penuh) — kontrak interface dikunci dulu sebelum paralel supaya nggak konflik integrasi.
4. ⏳ **SEKARANG BISA MULAI**: retuning 0.1 (11 layer lama) + fitur-fitur baru (Fase 2+) jalan di atas
   kanvas yang sudah benar — sekali kerja, bukan dua kali. **Fase 1 inti sudah tuntas.**

## FASE 2: Auto-Rekomendasi Berbasis Deteksi Wajah

Murah (datanya sudah ada dari morphology scan), dampaknya langsung ke persepsi "app ini pintar."
Sambungkan `faceShape`/`eyeShape`/`noseShape` ke tabel mapping teknik-per-bentuk-wajah yang sudah
lengkap di dictionary (§3B Contour, §4B Blush, §5D Highlighter, §9B Eyebrows). User tetap bisa override
manual, tapi default suggestion harus otomatis dari AI, bukan user pilih buta dari pilihan generik.

### 2.1 — Shade Matcher (konsep, riset legal sudah ada — 2026-07-29)

Alur yang disepakati: AR rendering TETAP pakai palette warna kita sendiri (bebas, tidak pernah
menyentuh data brand sama sekali). Setelah user puas dengan satu look, BARU jalankan reverse
lookup: hex/LAB warna yang sedang dipakai dicocokkan (nearest-color-match) ke tabel referensi
shade brand asli, tampil sebagai teks informasional ("warna ini paling dekat ke Brand A Produk X
Shade Y") — bukan render presisi warna brand di AR.

**Kenapa alur ini dipilih (riset legal, 2 subagent research pass 2026-07-29):**
- Render AR presisi warna asli brand (co: "lihat MAC NC42 di wajahmu secara fotorealistik") butuh
  lisensi data resmi dari brand (pola Perfect Corp/ModiFace) — di luar jangkauan app independen tanpa
  partnership.
- Rekomendasi TEKS nama produk asli sebagai "closest match" (bukan render) masuk kategori
  *nominative fair use* — preseden nyata: Temptalia's Foundation Matrix cross-reference shade
  puluhan brand by name tanpa lisensi, banyak "shade dupe finder" tool serupa jalan bertahun-tahun.
  Karena warna yang dipakai user MURNI dari palette kita sendiri (brand data cuma dipakai di langkah
  lookup terakhir), ini malah risiko LEBIH RENDAH dibanding rekomendasi berbasis skin-tone langsung.
- **Syarat wajib kalau dibangun**: (1) tabel referensi hex→brand+shade sumbernya dari data yang
  brand PUBLIKASIKAN sendiri di web resmi mereka (bukan scraping database proprietary), (2) TIDAK
  PERNAH pakai logo/wordmark brand, (3) disclaimer jelas "tidak berafiliasi dengan brand manapun,
  estimasi berdasar info publik", (4) monetisasi via affiliate link = workstream terpisah, wajib
  daftar resmi affiliate program brand/retailer-nya dulu.
- **Bukan nasihat hukum** — sebelum fitur ini di-launch komersial, tetap perlu konsultasi pengacara IP
  untuk review copy/disclaimer aktual.

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
- **Foundation shade-mixing pad 2D (✅ fungsional 2026-07-30, PERLU TUNING LEBIH LANJUT)**: pengganti 5
  swatch tetap — pad drag 2D (undertone × depth) dalam modal terpisah dari panel dock, grid 2×4 referensi
  warna (cool/netral/warm/olive × terang/gelap, hasil riset TAMO — model bilinear 2 sudut lama gak bisa
  jangkau undertone olive karena olive bukan titik tengah garis warm-cool). Beberapa ronde bug navigasi &
  performa sudah dibereskan sesi ini: drag pakai `react-native-gesture-handler` + Reanimated worklet
  (bukan PanResponder — worklet jalan di UI thread, jadi gak numpang JS thread yang sama dgn MediaPipe),
  throttle commit ke store digeser dari 40ms → 16ms. Item terbuka:
  - **Lag warna di wajah (live AR) masih terasa** — dikonfirmasi user setelah fix throttle. Root cause:
    render loop kamera sendiri cuma ~17fps (MediaPipe ~26ms/frame, lihat log `FizgravityPerf`), jadi
    walau commit warna dari JS sudah cepat (16ms), gambar di wajah tetap kebatas nunggu frame render
    kamera berikutnya. Percepat throttle pad TIDAK akan membantu lebih jauh — perlu riset TAMO terpisah
    soal optimasi pipeline MediaPipe/kamera itu sendiri kalau mau dikejar (proyek performa, bukan UI).
  - Palet 8-titik (2×4 grid) sudah lebih baik dari 4-sudut lama, tapi masih jauh dari akurasi profesional
    (Pantone SkinTone Guide pakai 100+ swatch). Kalau user masih sering gak nemu warna kulitnya sendiri,
    pertimbangkan nambah titik referensi lagi atau riset ulang.
  - `react-native-gesture-handler` baru pertama kali dipakai di app ini — cek gotcha autolinking statis
    di atas kalau nanti nambah native dependency lain lagi (masalah sama bakal muncul lagi kalau lupa).
- **Kualitas batas bibir (✅ SELESAI 2026-07-29)**: bocor ke kulit kumis/dagu fixed (triangulasi khusus
  bibir, bukan mesh wajah penuh), tepi patah-segi fixed (Catmull-Rom spline, 11 titik → ~51 titik per
  kontur), celah/mangap seam fixed (seam band pakai SATU sinyal global "seberapa terbuka mulut" dari
  titik tengah, bukan per-titik independen — versi per-titik bikin animasi nutup terasa "menyapu dari
  samping" alih-alih atas-ketemu-bawah), sudut mulut dikunci sempit (4% dari lebar mulut) biar gak ikut
  animasi buka-tutup tapi tetap bentuk V natural. Margin outer ~6.75% lebar mulut buat bibir tebal/lipatan.
- **Sistem lipstick finish (✅ SELESAI 2026-07-29)**: matte/satin/glossy/sheer/shimmer, masing-masing
  kurva alpha coverage sendiri (bukan cuma beda kilau) + UI picker penuh (shade/opacity/shine/finish) di
  TryOnScreen. Matte/satin/glossy/sheer confirmed realistis di device.
- **Shimmer/glitter — OPEN ITEM, di-skip untuk sekarang**: 7+ ronde percobaan (noise prosedural berbagai
  variasi, lalu tekstur glitter ter-generate via `generate_glitter_texture.py` + mipmap + intensity cap)
  masih belum meyakinkan secara visual ("gak realistic", kata user). Kemungkinan perlu riset lebih dalam
  soal bagaimana app AR profesional benar-benar merender glitter (bukan cuma placeholder texture kita),
  atau terima keterbatasan shader real-time untuk efek ini. Default finish di `makeupStore.ts` sengaja
  di-set ke `'matte'` (bukan `'shimmer'`) sampai ini beres.

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

## FASE 8: Try-On UI Chrome — Navigasi FAB & Polish Visual (ide 2026-07-30, timing diserahkan ke mandor)

Ide user, dicatat lengkap biar gak hilang, dieksekusi belakangan:

- Ganti dock horizontal (pill 5 ikon) jadi **FAB bulat mengambang** di pojok kanan-bawah, dengan idle
  animation (berdenyut/pulsing) supaya mengundang rasa penasaran untuk di-tap.
- Tap FAB → animasi **morph dari lingkaran jadi menu vertikal** (list ikon kategori makeup memanjang ke
  atas dari posisi FAB), lalu setelah kategori dipilih animasi balik jadi lingkaran, dock lagi ke pojok
  kanan-bawah.
- Navigasi tambahan: **swipe horizontal antar panel kategori** (mis. dari panel Foundation swipe ke panel
  Concealer) sebagai alternatif selain lewat FAB.
- Polish terpisah (independen dari FAB, bisa dikerjakan kapan saja): batas siku antara frame card luar dan
  konten di dalam semua panel kategori sekarang masih kelihatan jelas/tidak menyatu — perlu diperhalus
  (border-radius & layering konsisten antar level, bukan cuma nested box bersudut tajam).

**Rekomendasi mandor soal timing**: tunda dulu. Dock/panel yang ada sekarang baru stabil setelah beberapa
ronde perbaikan bug nyata di sesi ini (dock ke-block saat panel terbuka, konten "tenggelam", modal nutupin
preview) — FAB+morph+swipe adalah perubahan besar & animasi-berat (butuh Reanimated gesture state machine
baru) yang akan menimpa ulang fondasi navigasi yang belum genap sehari stabil. Selesaikan dulu retuning
Fase 0.1 sisanya (Concealer/Contour/Blush/Highlighter/Eyeshadow) + konfirmasi on-device untuk fix
Foundation retuning & bug opacity lipstick yang masih pending, baru kerjakan FAB redesign ini sebagai SATU
putaran UI/animasi terpisah — supaya tidak didesain ulang dua kali kalau kebutuhan navigasi berubah begitu
fitur inti lain ditambahkan.

---

*Dokumen ini adalah kitab kerja MatchAndBeauty AR Try-On — setiap keputusan implementasi fitur baru
merujuk balik ke sini (untuk PRIORITAS) dan ke `THE MASTER AR BEAUTY PIPELINE & DICTIONARY.md`
(untuk CARA membangunnya dengan benar).*
