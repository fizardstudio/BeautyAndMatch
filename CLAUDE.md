# Project: MatchAndBeauty (Engine AR Filter "Fizgravity")

## Ringkasan Stack
- Frontend: React Native 0.86 + TypeScript, Zustand, Reanimated, Skia, VisionCamera
- Native Android: Kotlin (view/manager) + JNI bridge (FizgravityJNI.cpp)
- AI Tracking: MediaPipe Tasks Vision (FaceLandmarker, 468 landmarks + 52 blendshapes)
- Render Engine: C++17 + OpenGL ES 2.0/3.0 (FizgravityRenderer.cpp), custom GLSL shaders, FBO multi-pass
- Tooling: Python + OpenCV untuk generate mesh index/UV/header C++
- Build: Gradle, CMake, Metro, ESLint/Prettier/Jest

## Pembagian Tugas Agent

### Claude (host) — eksekusi & reasoning
Semua penulisan/perubahan kode ada di sini. Termasuk:
- Implementasi & debugging GLSL shader (blend mode, blur pass, mask baking)
- Kotlin ↔ C++ JNI bridge, CameraX/ImageAnalysis pipeline
- Integrasi hasil MediaPipe (landmark, blendshape, transformation matrix) ke render loop
- React Native side: state Zustand, komponen Skia, worklet camera
- Review, refactor, fix build (Gradle/CMake)

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
