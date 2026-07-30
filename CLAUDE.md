# Project: MatchAndBeauty (Engine AR Filter "Fizgravity")

## Ringkasan Stack
- Frontend: React Native 0.86 + TypeScript, Zustand, Reanimated, Skia, VisionCamera
- Native Android: Kotlin (view/manager) + JNI bridge (FizgravityJNI.cpp)
- AI Tracking: MediaPipe Tasks Vision (FaceLandmarker, 468 landmarks + 52 blendshapes)
- Render Engine: C++17 + OpenGL ES 2.0/3.0 (FizgravityRenderer.cpp), custom GLSL shaders, FBO multi-pass
- Tooling: Python + OpenCV untuk generate mesh index/UV/header C++
- Build: Gradle, CMake, Metro, ESLint/Prettier/Jest

## ⚠️ Gotcha Build: Autolinking Android Statis (WAJIB dibaca sebelum nambah native dependency)

`android/settings.gradle` pakai `autolinkLibrariesFromConfigFile(new File(settingsDir, "autolinking.json"))`
— artinya `android/autolinking.json` adalah file **statis** yang di-copy apa adanya saat build, BUKAN
auto-regenerate dari `package.json` seperti autolinking default RN. Efeknya: `npm install <native-package>`
lalu langsung `gradlew installDebug` akan **build sukses** (exit 0) tapi app **crash saat runtime** dengan
error semacam `Cannot read property 'X' of undefined` — karena modul native barunya gak pernah ke-register
walau APK berhasil dibuat.

**Setiap kali nambah/hapus native dependency** (apapun yang punya kode native, bukan pure-JS package),
WAJIB regenerate dulu sebelum rebuild:
```
npx @react-native-community/cli config > android/autolinking.json
```
Baru setelah itu jalankan `gradlew installDebug`. Lupa langkah ini adalah penyebab bug yang sangat
membingungkan karena build tool sama sekali tidak memberi warning/error di tahap compile.

## Pembagian Tugas Agent

Struktur: **Claude (host) = mandor** → orkestrasi, reasoning, planning, review/QC final. Penulisan kode untuk task besar/fitur baru **boleh** didelegasikan ke subagent Haiku 4.5 lewat workflow `feature-research`, tapi hasilnya wajib direview mandor sebelum dianggap selesai. Untuk edit kecil/langsung (satu-dua baris, fix cepat), Claude host tetap boleh edit sendiri tanpa lewat workflow — tidak semua perubahan perlu difull-orchestrate.

### Claude (host) — mandor: orkestrasi, reasoning, review/QC
- Breakdown task, tulis instruksi/prompt untuk subagent, putuskan kapan perlu riset (Gemini) vs langsung implementasi
- **WAJIB review** hasil kode dari subagent Haiku (git diff + baca file yang diubah) sebelum melaporkan task selesai ke user — cek konsistensi dengan shader/JNI/state yang sudah ada, potensi bug, dan asumsi yang salah
- Edit kecil/langsung, debugging cepat, fix build (Gradle/CMake) — boleh dikerjakan sendiri tanpa lewat subagent kalau lebih cepat daripada orchestrate
- Integrasi akhir & keputusan arsitektur (mis. desain FBO multi-pass, kontrak JNI, struktur render loop)

### Haiku 4.5 (subagent, via workflow `feature-research` fase Implement) — penulisan kode
Dipanggil lewat `.claude/workflows/feature-research.js`, fase "Implement", setelah brief riset dari Gemini siap. Menulis kode berdasarkan brief + instruksi mandor:
- Implementasi GLSL shader (blend mode, blur pass, mask baking) mengikuti brief
- Kotlin ↔ C++ JNI bridge, CameraX/ImageAnalysis pipeline
- Integrasi hasil MediaPipe (landmark, blendshape, transformation matrix) ke render loop
- React Native side: state Zustand, komponen Skia, worklet camera

Subagent ini **tidak boleh** commit ke git — hanya ubah working tree. Hasilnya selalu berstatus "belum final" sampai direview mandor.

### Gemini (subagent `web-researcher`) — riset only, read-only
Delegasikan HANYA untuk hal yang butuh info terkini/eksternal, contoh:
- Update versi/breaking changes MediaPipe Tasks Vision atau CameraX
- Riset teknik shader baru (soft-light/overlay alternatif, subsurface scattering ringan utk kulit)
- Benchmark performa OpenGL ES di device Android tertentu
- Tren warna/style makeup AR terkini utk referensi desain filter
- Cari alternatif library kalau ada masalah kompatibilitas (mis. react-native-vision-camera vs versi RN baru)

**Jangan** delegasikan hal yang Claude sudah tahu dari konteks project (struktur mesh index, cara kerja FBO, konsep dasar blend mode) — itu buang token MCP call.

## Aturan Format Handoff Gemini → Claude
Gemini WAJIB balikin dalam format:
1. Ringkasan max 8 bullet
2. Rekomendasi konkret (bukan opsi mengambang)
3. Sumber (link)
Tidak boleh tempel isi mentah halaman/dokumentasi panjang.
