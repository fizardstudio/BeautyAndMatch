# Laporan Riset Komprehensif: Teknik Sculpt Contour Makeup & Pemetaan Engine AR Beauty Tech

**Dokumen Referensi:** Riset Internal Engine AR Beauty Filter "Fizgravity"  
**Lokasi File:** `docs/research/sculpt_contour_makeup_research.md`  
**Penyusun:** Team AI & Graphics Engineering MatchAndBeauty  

---

## 1. Pengantar & Teori Dasar (Foundational Theory)

### 1.1 Prinsip Chiaroscuro dalam Seni & Makeup
*Sculpting* dan *contouring* dalam dunia profesional Makeup Artist (MUA) berakar dari prinsip seni rupa klasik era Renaissance yang dikenal sebagai **Chiaroscuro** (dari bahasa Italia: *chiaro* = terang, *scuro* = gelap). Pelopor makeup modern seperti **Kevyn Aucoin** dan **Sam Fine** mengadaptasi teori ini ke dalam struktur visual wajah manusia.

* **Fungsi Bayangan (Contour):** Warna gelap menyerap cahaya dan secara visual **memundurkan/memperdalam** (*recede*) area wajah tertentu, menciptakan ilusi struktur tulang yang tajam (*chiseled bone structure*).
* **Fungsi Cahaya (Highlight):** Warna terang memantulkan cahaya dan secara visual **memajukan/menonjolkan** (*bring forward*) area wajah.
* **Tujuan Kamera 2D:** Kamera digital (terutama kamera depan smartphone) cenderung meratakan (*flatten*) wajah 3D menjadi gambar 2D ber-efek *washed-out*. Teknik sculpt contour mengembalikan dimensi 3D dan ilusi kedalaman spasial (*spatial depth perception*).

### 1.2 Perbedaan Mendasar: Contour vs. Bronzer

Banyak pengguna dan developer awam mengacaukan antara *contour* dan *bronzer*. Dalam MUA profesional dan rendering AR, keduanya adalah modul yang sepenuhnya berbeda:

| Parameter | Sculpt Contour | Bronzer |
|---|---|---|
| **Undertone** | **Cool / Neutral Taupe** (Cool Grey-Brown / Ash Brown) | **Warm / Golden / Terracotta** (Red-Orange Warmth) |
| **Fungsi Utama** | Mensimulasikan **bayangan alami kulit** (*natural shadow*) & merekayasa struktur tulang | Mensimulasikan **efek kehangatan sinar matahari** (*sun-kissed warmth*) |
| **Finish Material** | **Pure Matte** (Roughness 1.0, Specular 0.0) | Satin / Subtle Shimmer / Radiant Gold |
| **Area Penempatan** | Hollows of cheeks, bawah jawline, sisi batang hidung, temple | Puncak pipi, kening atas, nose bridge (area yang terkena matahari) |
| **Blending Mode AR** | **Multiply / Color Burn** (Low Saturation, High Depth) | **Soft Light / Overlay** (High Saturation, Warm Boost) |

---

## 2. Anatomi Penempatan Sculpt Contour (Anatomical Landmarks)

Pengaplikasian sculpt contour profesional bertumpu pada titik jangkar anatomi tengkorak manusia (*craniofacial landmarks*):

```
                       [HAIRLINE / TEMPLE]
                       (Temporal Bone Area)
                                |
                                v
      [CHEEKONE LIFT 45°] <------------> [NOSE RESHAPE]
    (Zygomatic Arch -> Tragus)         (Nasal Bone -> Septum)
                                |
                                v
                       [JAWLINE SNATCHER]
                    (Inferior Mandibular Line)
```

### 2.1 Cheekbone Sculpting (Zygomatic Arch Lift)
* **Garis Orientasi Anatomi:** Garis imajiner ditarik dari *tragus* (tonjolan kartilago telinga) mengarah ke *corner of the mouth* (sudut bibir).
* **Batas Pemotongan (Termination Point):** Garis contour **WAJIB berhenti** tepat sejajar dengan *outer canthus* (sudut luar mata / pupillary line). Jika garis ditarik terlalu panjang mendekati bibir, wajah akan terlihat kempot, tua, dan menyeret wajah ke bawah.
* **Sudut 45° (Lifting Effect):** MUA modern menempatkan shading tepat *di atas* atau *di sepanjang* bagian bawah tulang zygomaticus dengan sudut diagonal ~45°, di-blend meluncur ke atas untuk efek *facelift*.

### 2.2 Jawline & Mandibular Sculpting (Jawline Snatcher)
* **Garis Orientasi Anatomi:** Di sepanjang tepi bawah tulang mandibula (*inferior border of mandible*), ditarik dari *angle of mandible* (bawah telinga) menuju *mental protuberance* (dagu).
* **Teknik Blending:** Shading diaplikasikan di bagian bawah tulang rahang dan di-blend meluncur **ke arah bawah leher**. Ini menciptakan bayangan tegas yang menyamarkan *double chin* dan mempertajam batas antara wajah dan leher.

### 2.3 Forehead & Temple Reduction
* **Garis Orientasi Anatomi:** Di sepanjang garis batas rambut (*hairline*) pada tulang frontal dan melengkung masuk ke area pelipis (*temporal fossa*).
* **Fungsi:** Menyempitkan dahi yang lebar dan mengarahkan fokus pandangan mata pengamat ke bagian tengah wajah (mata dan bibir).

### 2.4 Nose Reshaping (Nasal Bridge, Tip & Septum)
* **Nasal Bridge Slimming:** Dua garis vertikal paralel di sepanjang tulang hidung (*nasal bones*). Semakin rapat kedua garis tersebut, semakin sempit ilusi batang hidung.
* **Nose Shortening (V-Shape Septum):** Shading berbentuk V di bawah septum/columella dan bagian bawah ujung hidung (*pronasale*). Ini memotong pantulan cahaya vertikal sehingga hidung panjang terlihat lebih pendek dan mungil (*button nose*).

### 2.5 Mentolabial & Sub-Lip Sculpting
* Titik shading kecil tepat di bawah bibir bawah (*mentolabial sulcus*). Menciptakan bayangan jatuh yang membuat bibir bawah terlihat lebih tebal dan bervolume secara alami.

---

## 3. Taksonomi Bentuk Wajah & Adaptasi Strategi Sculpting

MUA profesional tidak pernah menerapkan pola contour yang sama secara rigid pada semua orang. Pengenalan bentuk wajah (*Face Shape Taxonomy*) menentukan arah dan intensitas shading:

```
+-----------------------------------------------------------------------------------+
| OVAL        | Proporsi seimbang. Contour halus pada hollows pipi & hairline atas.|
+-------------+---------------------------------------------------------------------+
| ROUND       | Wajah lebar. Contour vertikal/diagonal tajam pada pipi & jawline.   |
+-------------+---------------------------------------------------------------------+
| SQUARE      | Mandibula bersudut tajam. Contour pada sudut rahang & dahi luar.   |
+-------------+---------------------------------------------------------------------+
| HEART       | Dahi lebar, dagu runcing. Contour di temple & sudut dahi atas.     |
+-------------+---------------------------------------------------------------------+
| OBLONG/LONG | Wajah panjang. Contour horizontal di hairline atas & puncak dagu.   |
+-----------------------------------------------------------------------------------+
```

1. **Wajah Oval (Ideal Balance):**
   * *Strategi:* Mempertahankan struktur alami. Cukup beri penegasan lembut di bawah tulang pipi dan sedikit di temple.
2. **Wajah Bulat (Round Face):**
   * *Ciri:* Lebar dan panjang wajah hampir sama, tanpa sudut tegas.
   * *Strategi:* Contour diagonal lebih tajam (mendekati vertikal) dari telinga ke arah sudut bibir, ditambah shading pada sisi dahi dan sepanjang rahang untuk memberi ilusi wajah lebih tirus dan berstruktur.
3. **Wajah Persegi (Square / Angular Face):**
   * *Ciri:* Dahi, tulang pipi, dan rahang memiliki lebar yang sama dengan sudut rahang yang tajam.
   * *Strategi:* Shading fokus pada sudut outer rahang dan sudut dahi luar untuk menghaluskan (*soften*) sudut-sudut yang terlalu keras.
4. **Wajah Hati (Heart / Inverted Triangle):**
   * *Ciri:* Dahi lebar dengan dagu runcing mendominasi.
   * *Strategi:* Shading tebal di area pelipis dan sisi dahi atas untuk menyempitkan bagian atas wajah agar seimbang dengan dagu yang kecil.
5. **Wajah Panjang (Oblong / Long Face):**
   * *Ciri:* Rasio vertikal jauh lebih panjang dibanding horizontal.
   * *Strategi:* Contour disebarkan secara **horizontal**. Shading dipasang di puncak hairline atas dahi dan di bawah puncak dagu untuk memotong panjang wajah secara visual.

---

## 6. Kesimpulan & Rekomendasi Fitur Fizgravity

1. **Pemisahan Modul Contour & Bronzer:** Engine Fizgravity wajib memisahkan slider `makeupContour` (Cool Taupe / Multiply) dari `makeupBronzer` (Warm Gold / Soft Light).
2. **Dynamic Skin-Tone Adaptation:** Penambahan kalkulasi luminance otomatis pada fragment shader agar warna contour tidak terlihat *ashy* pada berbagai rona kulit.
3. **Preserving Natural Texture:** Menggunakan *high-pass noise blend* agar area shading tidak menghapus tekstur pori-pori kulit asli pengguna.
