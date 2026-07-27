# Laporan Validasi Teknikal & MUA: Architecture 11-Layer Real-Time AR Beauty Filter

**Dokumen Referensi:** Design Document internal ("Holy Grail")  
**Target Platform:** Real-Time AR Rendering Engine (MediaPipe 468/478 Face Mesh)  
**Tujuan Laporan:** Menilai akurasi teknis, kesesuaian dengan praktik Makeup Artist (MUA) profesional, serta efisiensi performa industri beauty-tech (ModiFace, Perfect Corp YouCam, Meitu).

---

## 1. Analisis & Verdict Per Layer [1] - [11]

### [1] Foundation (Finish: Matte / Dewy-GlassSkin / Sheer-Tinted)
* **Verdict:** **Akurat**
* **Evaluasi MUA & Beauty-Tech:** Foundation adalah *base layer* mutlak dalam makeup asli maupun AR. Di industri AR (YouCam, ModiFace), foundation diimplementasikan menggunakan kombinasi *skin smoothing/blurring filter* (seperti Bilateral Filter atau Guided Filter) + *color blending* (Alpha blending / Soft Light mode).
  * **Matte:** Diffuse reflection tinggi, Roughness ~0.8-1.0, Specular ~0.0.
  * **Dewy / Glass Skin:** Roughness rendah (~0.1-0.2), Fresnel/Specular highlight di area kening, pipi, dan dagu.
  * **Sheer-Tinted:** Low opacity blend (15-30%) mempertahankan tekstur pori asli.
* **Sumber Validasi:** Konvensi MUA, Patent L'Oréal/ModiFace (US10467793B2 - Real-time skin color modification), Dokumentasi SDK Banuba & Spark AR.

---

### [2] Concealer (Segitiga Terbalik Bawah Mata + Garis Senyum)
* **Verdict:** **Sebagian Akurat (Anatomi Outdated / Tren MUA Modern Berubah)**
* **Evaluasi MUA & Beauty-Tech:**
  * **Segitiga Terbalik (Inverted Triangle):** Ini adalah teknik tren YouTube/Instagram era 2015-2018 (style Kim Kardashian). MUA profesional modern *menghindari* segitiga besar karena membuat wajah terlihat tebal (*cakey*) dan menarik wajah ke bawah. Teknik MUA modern (2023-2026) menggunakan **Targeted Lifting Placement**: titik kecil di *inner corner* (tear duct) untuk mencerahkan bayangan gelap, dan garis diagonal kecil di *outer corner* mengarah ke pelipis untuk efek *facelift*.
  * **Garis Senyum (Nasolabial Folds):** Mengaplikasikan concealer tebal di garis senyum berisiko tinggi memicu *creasing* (penumpukan produk di garis lipatan). Di AR, ini hanya membutuhkan *soft feathering gradient* tipis untuk menyamarkan bayangan, bukan blok concealer opacity tinggi.
* **Sumber Validasi:** Konvensi MUA Modern, Tutorial Professional MUA (Robert Welsh, Hindash), Engine Masking YouCam (menggunakan gradient radial soft-edge pada tear trough).

---

### [3] Contour / Shading (Cheekbone Lift 45°, Jawline Snatcher, Nose Reshape)
* **Verdict:** **Akurat (Praktik MUA), Sebagian Overengineered (Jika Statis)**
* **Evaluasi MUA & Beauty-Tech:**
  * **Cheekbone Lift:** Penempatan dari tragus telinga mengarah ke sudut bibir (selesai di sejajar outer corner mata) dengan sudut ~45° adalah standar MUA profesional untuk memberi dimensi tulang pipi.
  * **Jawline Snatcher:** Shading tepat di bawah garis ramus mandibula yang di-blend ke arah leher untuk menciptakan bayangan ilusi leher jenjang / menyamarkan double chin.
  * **Nose Reshape:** Dua garis vertikal paralel di batang hidung + V-shape di bawah septum/ujung hidung.
  * **Catatan Engineering AR:** Menggunakan mode blending **Multiply** atau **Color Burn**. Namun, menetapkan sudut 45° secara rigid pada UV mesh dapat terlihat aneh pada bentuk wajah yang berbeda (misal: wajah bulat vs panjang). Beauty tech profesional menyesuaikan koordinat shading berdasarkan *Face Shape Classifier* (Morph Target / Landmark ratio adjustment).
* **Sumber Validasi:** Buku Teks MUA ("Making Faces" - Kevyn Aucoin), IEEE Paper: *Automated Virtual Makeup Recommendation & Transfer Systems*.

---

### [4] Blush (Apple of Cheek, Draped/Lifting, Sun-kissed/Igari)
* **Verdict:** **Akurat**
* **Evaluasi MUA & Beauty-Tech:** Layer ini mencakup 3 gaya blush paling fundamental dalam sejarah & tren makeup global:
  1. **Apple of Cheek (Bulat):** Gaya klasik untuk kesan muda/fresh (*youthful/plump*).
  2. **Draped / Lifting (Diagonal ke pelipis):** Gaya 80-an / kontemporer untuk memanjangkan & mengangkat struktur wajah.
  3. **Igari / Sun-kissed (Horizontal melintasi hidung):** Tren Korea/Jepang yang menyambungkan blush pipi kiri dan kanan melintasi *nose bridge*.
* **Sumber Validasi:** Konvensi MUA Global, Feature Presets pada YouCam Makeup & TikTok Effect House.

---

### [5] Highlighter (Puncak Tulang Pipi + Ujung Hidung, Shift Dinamis saat Menoleh)
* **Verdict:** **Sebagian Akurat / Overengineered (Jika Diberlakukan Wajib PBR Dynamic Shift tanpa Scene Lighting)**
* **Evaluasi MUA & Beauty-Tech:**
  * **Penempatan Anatomi:** Puncak zygomatic bone, tip of nose, cupid's bow, dan brow bone sangat akurat sesuai standar MUA.
  * **Kebutuhan "Wajib Bergeser Dinamis" (Dynamic Specular Shift):**
    * Pada engine AR tingkat tinggi yang memakai PBR Shader (Physics-Based Rendering seperti ModiFace), specular highlight memang dihitung dari kalkulasi vektor arah kamera ($\vec{V}$) dan arah cahaya ($\vec{L}$) terhadap *normal map* wajah.
    * **NAMUN**, pada mayoritas aplikasi komersial (Meitu, Snapchat filter standar, YouCam fast-mode), highlighter disajikan sebagai 2D alpha texture statis yang dipadukan dengan *alpha/additive blending* sederhana. Mengapa? Karena tanpa rekonstruksi pencahayaan lingkungan 3D (environment lighting estimation), kalkulasi specular dinamis yang dipaksa bergeser sering kali menghasilkan artefak "berkilau liar" (*glitchy reflection*) saat pengguna bergerak cepat atau di tempat redup.
* **Sumber Validasi:** Paper SIGGRAPH (*Real-Time Face Rendering and Material Capture*), ModiFace Tech Patents, Snap AR Lens Studio Shader Guides.

---

### [6] Eye Lenses / Softlens (Natural Ring, Circle/Dolly, Exotic, Fake Catchlight)
* **Verdict:** **Akurat**
* **Evaluasi MUA & Beauty-Tech:** MediaPipe 478 Landmarks menyediakan 10 titik iris tracking yang presisi.
  * **Circle / Dolly:** Memperbesar skala radius tekstur iris melebihi limbal ring asli pengguna.
  * **Fake Catchlight:** Titik pantulan cahaya buatan (putih translusen) di sudut jam 1 atau 11 iris sangat krusial di AR, karena tekstur lensa kontak digital sering kali menutup pantulan cahaya alami mata, membuat mata terlihat mati (*dead eyes*) jika tanpa catchlight.
* **Sumber Validasi:** Modul Iris Tracking MediaPipe, Standard Asset Pipeline AR Filter Lensa Kontak.

---

### [7] Eyeshadow (Smokey Eye, Halo Eye, Aegyo Sal)
* **Verdict:** **Akurat (Variasi Style), Namun Overengineered jika Memakai "Mesh Elastis Otot Kedipan"**
* **Evaluasi MUA & Beauty-Tech:**
  * Style Smokey, Halo, dan Aegyo Sal sangat mewakili kebutuhan makeup riil.
  * **Isu Eyelid Tracking / Mesh Elastis:** Menulis simulasi otot elastis khusus (*custom physics muscle deformation*) untuk eyeshadow adalah **overengineering berlebihan**. 3D Canonical Face Mesh MediaPipe secara alamiah *mengecil dan memanjang* (deformasi UV) ketika titik kelopak mata atas (*eyelid landmarks*) mendekati kelopak mata bawah saat berkedip. Menempelkan tekstur eyeshadow pada UV map mesh standar secara otomatis menangani pergerakan kedipan secara sempurna tanpa perlu engine fisika tambahan.
* **Sumber Validasi:** Dokumentasi MediaPipe Canonical Face Model, Shader Mapping Spark AR / TikTok Effect House.

---

### [8] Eyeliner (WAJIB di atas Eyeshadow — Cat Eye, Puppy Eye, Smudged)
* **Verdict:** **Akurat**
* **Evaluasi MUA & Beauty-Tech:**
  * **Urutan Render (Z-Index):** Mutlak wajib di atas Eyeshadow [7] dan di bawah Eyelashes [11]. Jika eyeliner ditaruh di bawah eyeshadow, pigmen eyeshadow akan mengotori ketajaman garis liner.
  * **Gaya:** Cat Eye (wing naik/western), Puppy Eye (wing turun mengikuti garis kelopak bawah ala Korea), dan Smudged (garis pensil yang difusikan).
* **Sumber Validasi:** Urutan Aplikasi MUA Profesional, Standard Layering Photoshop/AR Cosmetic Engine.

---

### [9] Eyebrows (Masking Tekstur Helai Rambut — Korean Straight, Western High-Arch, Feathery)
* **Verdict:** **Akurat**
* **Evaluasi MUA & Beauty-Tech:** Blok warna solid pada alis akan membuat filter AR terlihat seperti kartun 2D murah. Penggunaan *alpha hair-stroke textures* (tekstur helai demi helai / microblading stroke) dipadu dengan masking area alis asli pengguna adalah konvensi standar industri.
* **Sumber Validasi:** Tech Blog Perfect Corp (YouCam 3D Brow Technology), Standard AR Makeup Design Guidelines.

---

### [10] Lipstick (Matte, Glossy Specular, Ombre, Overlined 1-2mm)
* **Verdict:** **Akurat**
* **Evaluasi MUA & Beauty-Tech:**
  * **Matte:** Blending Multiply + Saturation boost, tanpa specular map.
  * **Glossy:** Additive/Specular layer + Normal Map mikro untuk efek basah (*wet look*).
  * **Ombre (Gradient Lips):** Radial opacity gradient dari inner lip line meluruh ke luar.
  * **Overlined (1-2mm):** Melakukan ekstrapolasi UV vertex keluar dari outer lip landmarks MediaPipe. Teknik ini persis seperti MUA menggambar lip liner di luar garis bibir alami untuk ilusi bibir lebih bervolume.
* **Sumber Validasi:** Modul Lip Makeup ModiFace & YouCam, Teknik MUA Lip Contour/Overlining.

---

### [11] Eyelashes (Layer Terluar Mutlak — Natural Wispy, Doll Eye, Foxy Eye)
* **Verdict:** **Akurat**
* **Evaluasi MUA & Beauty-Tech:**
  * **Urutan Render (Top Layer):** Bulu mata secara anatomi berada di paling depan. Dalam AR graphics, bulu mata merender quad mesh / billboard card 3D bertekstur alpha tepat di garis outer eyelid landmarks.
  * **Style:** Natural Wispy, Doll Eye (fokus tebal tengah mata), Foxy Eye (fokus tebal & panjang di sudut luar mata).
* **Sumber Validasi:** Pipeline AR Asset Creation Lens Studio & Effect House.

---

## 2. Jawaban Khusus & Evaluasi Ragu-Ragu (Deep Dive)

### 1. Aegyo Sal (애교살)
* **Konfirmasi Teknik & Anatomi:** **100% Teknik Nyata & Spesifik.**
* **Anatomi Real:** Aegyo Sal **BUKAN** eyeshadow biasa dan **BUKAN** kantung mata (*dark circles/under-eye bags*). Aegyo Sal adalah lipatan otot *orbicularis oculi* (bantalan lemak kecil persis 2-5mm di bawah garis bulu mata bawah) yang menonjol saat seseorang tersenyum.
* **Teknik MUA:** MUA menggambar garis shading tipis berwarna cokelat muda tepat di bawah bantalan lemak tersebut (*crease illusion*), lalu mengisi area bantalan lemaknya dengan *highlight/shimmer/glitter* terang. Di AR, ini membutuhkan 2 komponen UV mask: 1 garis shading linear dan 1 patch shimmer oval di bawah eyelash bawah.

### 2. Concealer Segitiga Terbalik & Garis Senyum
* **Konfirmasi Anatomi & Tren:** **Bentuk Segitiga Terbalik adalah Tradisional/Outdated.**
* **Penjelasan MUA:** Teknik segitiga terbalik Kim Kardashian era 2015 membuat wajah terlihat berat dan *flat*. MUA modern menggunakan **Micro-Concealing**:
  * Inner corner eye (menutupi warna kebiruan/gelap pembuluh darah).
  * Outer corner eye (ditarik diagonal ke atas untuk ilusi *lifted eye*).
* **Di AR Engine:** Hindari menutupi area pipi atas dengan masker segitiga solid opacity tinggi. Gunakan mask gradient berbentuk bulan sabit (*crescent*) di bawah mata dan garis tipis tersamar di nasolabial crease.

### 3. Contour Angles (Cheekbone 45°, Jawline, Nose Reshape)
* **Konfirmasi Sudut & Posisi:** **Akurat untuk Standar MUA, Namun Perlu Dynamic Adapters.**
* **Penjelasan MUA:** MUA tidak pernah memakai sudut 45° secara buta pada semua orang. Sudut ditarik dari *tragus* telinga menuju sudut bibir, tetapi dihentikan di garis sejajar ujung luar mata (*pupillary line*). 
* **Di AR Engine:** Jika wajah pengguna *Round/Square*, shading pipi lebih vertikal untuk meniruskan. Jika wajah *Heart/Long*, shading lebih horizontal untuk menyeimbangkan. MediaPipe 468 landmark memungkinkan kalkulasi rasio lebar vs panjang wajah (Aspect Ratio Wajah) secara real-time untuk menyesuaikan orientasi mesh contour.

### 4. Highlighter Dinamis Bergeser Saat Kepala Menoleh
* **Apakah Industri Menggunakan PBR Dinamis atau Statis?**
* **Jawaban:** **Statis + Opacity Modifier adalah Standar Industri Massal; PBR Dinamis adalah Feature Premium.**
* **Analisis Engine:**
  * **Statis / Standard AR (Meitu, TikTok, Snapchat standard):** Highlighter berupa tekstur PNG 2D dengan Alpha Channel. Saat kepala menoleh, tekstur menempel pada koordinat UV mesh. Tidak ada kalkulasi pencahayaan 3D dinamis. Ini efisien, *lightweight* (60 FPS di HP low-end), dan sudah dianggap memuaskan bagi pengguna awam.
  * **Dinamis / High-End PBR (ModiFace / YouCam Enterprise):** Menggunakan Shader dengan perhitungan *Phong/Blinn-Phong* atau *GGX Specular Distribution*. Specular highlight bergerak berdasarkan perkiraan posisi lampu utama (*Light Vector*).
* **Rekomendasi:** Menganggap kalkulasi dinamis sebagai **WAJIB** adalah *overengineering* jika targetnya adalah filter ringan. Solusi ideal: gunakan **Normal-Map MatCap Shader** sederhana yang mensimulasikan pantulan cahaya berdasarkan sudut rotasi head-pose MediaPipe ($R_{x}, R_{y}, R_{z}$) tanpa perlu full 3D lighting engine.

### 5. Eyeshadow Mesh Elastis Eyelid
* **Apakah Industri Melacak Landmark Eyelid Per-Frame?**
* **Jawaban:** **Ya, tetapi via UV Mapping 3D Mesh Standar, BUKAN Engine Otot Elastis Custom.**
* **Analisis Engine:** MediaPipe Face Mesh memberikan koordinat 3D kelopak mata yang diperbarui setiap frame. Ketika pengguna berkedip, vertex mesh di area kelopak mata atas secara otomatis bergerak mendekati kelopak mata bawah. Tekstur eyeshadow yang di-pin pada koordinat UV vertex tersebut akan terkompresi dan ter-stretch secara alami. Tidak perlu menulis kalkulasi *elastic physics/cloth simulation* khusus.

---

## 3. Teknik MUA Penting yang HILANG dari Daftar 11 Layer

Berikut adalah teknik-teknik MUA profesional penting yang belum dimasukkan ke dalam dokumen [1]-[11] dan disarankan untuk menjadi roadmap pengembangan fitur masa depan:

| No | Teknik MUA | Fungsi MUA Riil | Implementasi Engine AR Beauty Tech |
|---|---|---|---|
| 1 | **Color Corrector (Neutralizer)** | Menetralkan rona kemerahan (jerawat/rosacea) dengan warna Hijau, atau lingkar hitam mata dengan rona Peach/Oranye sebelum Foundation. | Layer [0.5] (sebelum Foundation): Selective Color Chromatic Adjustment mask pada area bermasalah. |
| 2 | **Setting Powder / Baking / Mattifying** | Menyerap minyak di T-Zone (jidat, hidung, dagu) dan menghaluskan pori-pori. | High-Pass Blur Filter + Roughness Boost (1.0) khusus pada area T-Zone segmentation mask. |
| 3 | **Lip Liner (Lip Contour)** | Menggarisi bingkai bibir dengan warna lebih gelap dibanding isi lipstick untuk dimensi 3D. | Sub-layer pada Lip [10]: Inner Lip Fill vs Outer Lip Border Stroke Shader. |
| 4 | **Tightlining / Waterline Eyeliner** | Menggambar garis hitam di selaput basah mata (*waterline*) di bawah bulu mata untuk efek mata tajam. | Sub-layer Eyeliner: Mesh line yang terikat pada inner rim landmarks kelopak mata. |
| 5 | **Brow Gel / Lamination / Tint** | Menyisir helai alis ke atas (*soap brows/feathered look*) dan memberi efek kilau basah pada alis. | Specular/Wetness Shader mask di atas tekstur alis [9]. |
| 6 | **Faux Freckles / Beauty Marks** | Bintik-bintik manis buatan di atas tulang pipi & hidung (tren Gen-Z & Festival makeup). | Procedural Noise Pattern Mask di atas layer Blush [4]. |
| 7 | **Plumping Lip Gloss Specular** | Efek kilau tebal (*glass/jelly lip*) dengan ilusi garis-garis bibir tersamarkan. | Normal Map Distortion + High Specular Exponent di atas Layer Lipstick [10]. |

---

## 4. Rangkuman Urutan Render Efisien & Disarankan (Pipelining)

Urutan render 11 layer awal Anda **secara teknis sudah tepat dalam hal stacking order**. Berikut adalah penyempurnaan urutan render komprehensif termasuk layer yang hilang:

1. **[Pre-Processing]** Color Corrector (Green/Peach spot correction)
2. **[1] Foundation** (Skin smoothing + Tone evening)
3. **[2] Concealer** (Targeted inner/outer eye brightener)
4. **[Fixation]** Setting Powder / T-Zone Mattifier
5. **[3] Contour / Shading** (Cheekbone, Jawline, Nose)
6. **[4] Blush** (Apple / Draped / Igari) + *Faux Freckles (Optional)*
7. **[5] Highlighter** (Zygomatic, Nose tip, Cupid bow via MatCap/Specular)
8. **[6] Eye Lenses** (Iris scale + Fake catchlight)
9. **[7] Eyeshadow** (Smokey / Halo / Aegyo Sal under-eye)
10. **[8] Eyeliner** (Cat / Puppy + Tightline Waterline)
11. **[9] Eyebrows** (Hair-stroke alpha texture + Brow gel specular)
12. **[10] Lipstick & Lip Liner** (Matte / Glossy / Ombre / Overline)
13. **[11] Eyelashes** (Top-most Quad Mesh - Natural / Doll / Foxy)
