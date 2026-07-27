# Rencana Arsitektur iOS Metal: Porting Pipeline AR Beauty Filter "Fizgravity"

Dokumen ini berisi cetak biru (*architectural blueprint*) untuk memindahkan pipeline AR beauty filter native **Fizgravity** dari Android (CameraX + GLES 2/3 C++ via JNI) ke iOS menggunakan **Metal framework** dan **React Native Legacy Architecture**.

---

## 1. Camera Capture iOS (AVFoundation to Metal Texture)

### Komponen Utama API iOS
* `AVCaptureSession`: Mengelola alur input hardware kamera (depan/belakang) dengan preset resolusi (misal `.hd1280x720` atau `.iframe1280x720`).
* `AVCaptureVideoDataOutput`: Menangkap frame video mentah secara real-time.
* `AVCaptureVideoDataOutputSampleBufferDelegate`: Protocol callback `captureOutput(_:didOutput:from:)`.
* `CVPixelBufferRef`: Buffer memori gambar yang didapat dari `CMSampleBufferGetImageBuffer(sampleBuffer)`.
* `CVMetalTextureCache`: Cache GPU khusus iOS (`CVMetalTextureCacheCreateTextureFromImage`) untuk mengonversi `CVPixelBuffer` menjadi `MTLTexture` dengan nol *cost* penyalinan memori CPU (*zero-copy memory mapping*).

### Pilihan Format Piksel (YUV vs BGRA) & Rekomendasi

| Format Piksel | `CVMetalTextureCache` Result | Kelebihan | Kekurangan |
|---|---|---|---|
| **`kCVPixelFormatType_32BGRA`** | Single `MTLTexture` (`.bgra8Unorm`) | Struktur persis RGBA di OpenGL. Langsung bisa di-sample di shader tanpa konversi warna. | `AVCaptureSession` melakukan konversi YUV→RGB di CPU/ISP sebelum melempar buffer. Konsumsi daya lebih tinggi. |
| **`kCVPixelFormatType_420YpCbCr8BiPlanarFullRange` (YUV)** | 2 `MTLTexture`:<br>1. Plane 0 (Y/Luma - `.r8Unorm`)<br>2. Plane 1 (UV/Chroma - `.rg8Unorm`) | **Sangat Efisien / Idiomatis iOS.** Zero-copy dari ISP kamera ke Metal GPU. Suhu perangkat & baterai hemat. | Membutuhkan 1 pass konversi YUV-to-RGB sederhana (matrix multiplication) di Metal fragment shader awal. |

**Rekomendasi:** Gunakan **BiPlanar YUV (`420YpCbCr`)**. Konversi YUV-to-RGB di Metal fragment shader hanya membutuhkan 3 baris perkalian matriks dan jauh lebih hemat daya pada perangkat iOS saat running 60 FPS AR.

### Management Threading & Gotchas
* **Processing Queue:** Callback `captureOutput` **WAJIB** berjalan di *serial dispatch queue* khusus berprioritas tinggi:
  ```swift
  let cameraQueue = DispatchQueue(label: "com.fizgravity.cameraQueue", qos: .userInteractive)
  videoOutput.setSampleBufferDelegate(self, queue: cameraQueue)
  ```
* **Orientasi Frame:** Sensor kamera iOS secara fisik terpasang lanskap. Frame dari `AVCaptureVideoDataOutput` yang ditangkap di mode potret berotasi 90°. Orientasi harus dikompensasi via matriks rotasi UV di Metal vertex shader atau `AVCaptureConnection.videoOrientation = .portrait` + `isVideoMirrored = true` (untuk kamera depan).
* **Texture Cache Flushing:** Wajib memanggil `CVMetalTextureCacheFlush(textureCache, 0)` secara berkala atau melepas referensi `CVMetalTexture` untuk mencegah *memory leak* buffer kamera.

---

## 2. MediaPipe Face Landmark di iOS

### Package & Modul Resmi
* **Package Manager:** CocoaPods (`pod 'MediaPipeTasksVision'`) atau Swift Package Manager (SPM).
* **Class Utama:** `MPPFaceLandmarker` dari modul `MediaPipeTasksVision`.

### Evaluasi Fitur & Paritas API (iOS vs Android)

| Fitur MediaPipe | Ketersediaan di iOS (`MPPFaceLandmarker`) | Paritas dengan Android |
|---|---|---|
| **468 3D Landmarks (+ 10 Iris = 478)** | ✅ `MPPFaceLandmarkerResult.faceLandmarks` | **100% Paritas Identik** |
| **52 Face Blendshapes** | ✅ `MPPFaceLandmarkerResult.faceBlendshapes` | **100% Paritas Identik** |
| **Facial Transformation Matrixes (Head Pose)** | ✅ `MPPFaceLandmarkerResult.facialTransformationMatrixes` | **100% Paritas Identik** |

### Alur Input & Output di iOS
* **Mode Operasi:** Gunakan `MPPRunningMode.liveStream`.
* **Konversi Frame:** Input kamera `CVPixelBuffer` dikonversi menjadi `MPPImage`:
  ```swift
  let mppImage = try MPPImage(sampleBuffer: sampleBuffer, orientation: .up)
  faceLandmarker.detectAsync(image: mppImage, timestampMs: timestampMs)
  ```
* **Callback Delegate:** Menerima hasil via `MPPFaceLandmarkerDelegate`:
  ```swift
  func faceLandmarker(_ landmarker: MPPFaceLandmarker, didFinishDetection result: MPPFaceLandmarkerResult?, timestampMs: Int, error: Error?)
  ```
* **Kesimpulan Gap API:** **TIDAK ADA GAP API.** C++ core engine MediaPipe Tasks Vision yang melandasi Android dan iOS adalah kode yang sama. Data landmark float array, blendshape score, dan matriks transformasi 4x4 dapat langsung diekstrak di Swift/ObjC.

---

## 3. Jembatan ke React Native (Legacy vs New Architecture)

### Strategi Arsitektur Bridge
Karena Android saat ini menggunakan arsitektur **LAMA** (`SimpleViewManager` + `requireNativeComponent`), pendekatan iOS paling minim friksi dan 100% kompatibel tanpa merusak kode JavaScript adalah:

* **iOS Architecture:** **RCTViewManager (Legacy Architecture)**
* **Kompatibilitas JS:** Komponen React Native di JS tetap memanggil:
  ```javascript
  import { requireNativeComponent } from 'react-native';
  const FizgravityARView = requireNativeComponent('FizgravityARView');
  ```

### Komponen Class iOS Native Bridge
1. **`FizgravityARViewManager.m` (Objective-C Bridge):**
   * Mengatur registrasi module dengan macro `RCT_EXPORT_MODULE(FizgravityARView)`.
   * Memetakan seluruh props makeup yang ada via `RCT_EXPORT_VIEW_PROPERTY`:
     * `makeupLipstick`, `makeupBlush`, `makeupFoundation`, `makeupEyeshadow`, `makeupContour`, `makeupHighlight`, `makeupContourStyle`, `makeupBlushStyle`.
   * Mengekspos method/command `takeSnapshot` via `RCT_EXPORT_METHOD`.
2. **`FizgravityARView.swift` (Native View Wrapper):**
   * Meng-inherit `UIView`.
   * Berisi `MTKView` (MetalKit View) untuk rendering + Instance `AVCaptureSession` & `MPPFaceLandmarker`.
   * Menerima perubahan props dari JavaScript dan meneruskannya ke render pipeline Metal.

---

## 4. Arsitektur Render Metal (vs OpenGL ES / GLSL)

### Pemetaan Konsep GLES ke Metal

| OpenGL ES (Android Existing) | Metal (iOS New) | Keterangan & Perubahan |
|---|---|---|
| `FBO` (Frame Buffer Object) | `MTLRenderPassDescriptor` + `MTLTexture` | Di Metal, pass render lepas dibuat dengan `MTLRenderPassDescriptor` yang `.colorAttachments[0].texture`-nya ditunjuk ke `MTLTexture` khusus (`.bgra8Unorm` / `.rgba8Unorm`). |
| `glUseProgram` / Shaders | `MTLRenderPipelineState` | Shader GLSL dikompilasi ulang ke **MSL (Metal Shading Language)**. Pipeline dibuat via `MTLRenderPipelineDescriptor`. |
| `glUniform*` | Constant Buffers (`MTLBuffer`) | Uniforms dikirim sebagai struct C/MSL lewat `setVertexBuffer` / `setFragmentBuffer`. |
| `glColorMask(R, G, B, A)` | `MTLRenderPipelineColorAttachmentDescriptor.writeMask` / **MRT Pass** | Di GLES, mask di-bake 1-per-1 ke channel R/G/B/A. Di Metal, ini diubah menjadi **MRT (Multiple Render Targets)**. |

### Restrukturisasi Mask Baking: Dari Sequential glColorMask ke MRT (Multiple Render Targets)

#### Pendekatan Lama di GLES (Android):
Menulis 11 layer mask secara sekuensial dengan mengatur mask channel warna:
1. Render Pass 1: Mask Foundation & Concealer → `glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE)` (Channel R).
2. Render Pass 2: Mask Contour & Blush → `glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE)` (Channel G).
3. Render Pass 3: Mask Eyeshadow & Highlighter → `glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE)` (Channel B).

#### Pendekatan Baru di Metal (iOS MRT Optimization):
Metal mendukung hingga 8 Color Attachments secara paralel dalam 1 single render pass tanpa *switching* FBO/RenderPassDescriptor.
* Buat 2 atau 3 offscreen `MTLTexture` sebagai **Multiple Render Targets**.
* Dalam 1 kali Fragment Shader execution, output MSL langsung menuliskan nilai alpha mask ke struktur:
  ```metal
  struct FragmentOutput {
      float4 maskGroup1 [[color(0)]]; // R=Foundation, G=Concealer, B=Contour, A=Blush
      float4 maskGroup2 [[color(1)]]; // R=Highlight, G=Eyeshadow, B=Eyeliner, A=Lipstick
      float4 maskGroup3 [[color(2)]]; // R=Eyebrows, G=Lenses, B=Eyelashes, A=Reserved
  };
  ```
* **Keuntungan:** Mengeliminasi 3-4 pass render sekuensial menjadi **1 pass MRT tunggal**, meningkatkan bandwidth memori GPU secara drastis pada Apple Silicon GPU (TBDR - Tile-Based Deferred Rendering).

---

## 5. Strategi Jembatan Native (Core Logic C++ vs Swift Metal)

### Analisis Porting Logic Core C++ (`FizgravityRenderer.cpp`)
`FizgravityRenderer.cpp` di Android dipenuhi dengan panggilan API OpenGL ES (`glGenFramebuffers`, `glBindTexture`, `glUseProgram`, `glDrawArrays`, `glColorMask`). **Metal BUKAN drop-in replacement untuk GLES**, dan API Metal-CPP wrapper akan membuat arsitektur sangat kompleks jika dipaksa membungkus panggilan GLES.

### Strategi Pemisahan Kode (*Separation of Concerns*)

1. **Keep & Share Pure Math Logic in C++ (`SharedCore`):**
   * Pindahkan logika non-GL dari `FizgravityRenderer.cpp` (kalkulasi matriks 3D, deformasi titik UV landmark 468, pembentukan indeks segitiga mesh/triangulation, interpolasi blendshape) ke header/source C++ murni (misal: `FizgravityMath.hpp` / `.cpp`).
   * Kode C++ ini dapat di-import langsung di iOS via **Objective-C++ (`.mm`)** atau Swift via Bridging Header.

2. **Rewrite Rendering Pipeline in Swift + MSL (`Engine Metal`):**
   * Tulis `FizgravityMetalRenderer.swift` untuk mengelola `MTLDevice`, `MTLCommandQueue`, `MTLCommandBuffer`, dan `MTLRenderCommandEncoder`.
   * Terjemahkan file shader GLSL menjadi file `.metal` (MSL Shading Language). Logika matematika blending (Multiply, SoftLight, Screen, Overlay) di fragment shader MSL persis sama dengan GLSL.

---

## 6. Analisis Risiko & Effort Porting

| Komponen | Tingkat Effort / Risiko | Catatan Mitigasi |
|---|---|---|
| **Restrukturisasi glColorMask ke Metal MRT & MSL Shaders** | 🔴 **SANGAT TINGGI (High Risk & Effort)** | Pengalihan logika dari sequential GLES color mask ke Metal MRT & MSL butuh re-validasi hasil blending 11 layer agar output visual di iOS 100% identik dengan Android. |
| **Kamera YUV to RGB Conversion & Pipeline Synchronization** | 🟡 **SEDANG (Medium Effort)** | Menyelaraskan frame rate `AVCaptureSession` (60fps), thread deteksi MediaPipe, dan callback `draw(in: MTKView)` agar tidak terjadi frame tearing atau drop frame. |
| **MediaPipe Tasks Vision iOS Integration** | 🟢 **RENDAH (Low Risk)** | API `MPPFaceLandmarker` iOS sangat stabil dan memiliki paritas 100% dengan Android. |
| **React Native Bridge (RCTViewManager)** | 🟢 **RENDAH (Low Risk)** | Menggunakan arsitektur lama `RCTViewManager` sangat straightforward dan menjamin paritas props dengan Android. |

---

## 7. Usulan Struktur File & Modul Native iOS

Struktur file native di direktori `ios/` yang mencerminkan arsitektur Android (`FizgravityARView.kt` / `FizgravityARViewManager.kt` / `FizgravityRenderer.cpp`):

```
ios/
├── Fizgravity/
│   ├── Bridge/
│   │   ├── FizgravityARViewManager.h        # Header RN ViewManager Bridge
│   │   └── FizgravityARViewManager.m        # Objective-C RN Bridge (Props & Commands)
│   │
│   ├── Views/
│   │   ├── FizgravityARView.swift           # Main Native UIView wrapper (Kamera + MTKView + MediaPipe)
│   │   └── FizgravityMetalView.swift        # Subclass MTKView (MTKViewDelegate draw loop)
│   │
│   ├── Engine/
│   │   ├── FizgravityMetalRenderer.swift    # Core Metal Renderer (Device, Pipelining, MRT Pass, Texture Pool)
│   │   ├── FizgravityShaders.metal          # MSL Shaders (YUV Conversion, Mask Baking, Blur, Multi-Blend Compositing)
│   │   └── Camera/
│   │       └── FizgravityCameraManager.swift # AVCaptureSession + CVMetalTextureCache Manager
│   │
│   ├── MediaPipe/
│   │   └── FizgravityLandmarkTracker.swift  # MediaPipe MPPFaceLandmarker wrapper & LiveStream Threading
│   │
│   └── SharedCore/                          # C++ Math Shared (Mirroring Android non-GLES math)
│       ├── FizgravityMath.hpp               # Mesh triangulation, 468 landmark UV calculations, matrix math
│       └── FizgravityMath.cpp
```
