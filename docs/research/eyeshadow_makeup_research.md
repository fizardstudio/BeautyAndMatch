# Laporan Riset Komprehensif: Teknik Eyeshadow Makeup & Pemetaan Engine AR Beauty Tech

**Dokumen Referensi:** Riset Internal Engine AR Beauty Filter "Fizgravity"  
**Lokasi File Workspace:** `docs/research/eyeshadow_makeup_research.md`  
**Penyusun:** Team AI & Graphics Engineering MatchAndBeauty  

---

## 1. Pengantar & Teori Dasar Material Eyeshadow

### 1.1 Fungsi & Peranan Eyeshadow
Eyeshadow adalah teknik pewarnaan zona rias mata yang bertujuan untuk merekayasa dimensi, kedalaman (*depth perception*), rona warna, serta ilusi bentuk mata (*eye shape correction*). 

Dalam kamera digital 2D, area mata sering terlihat *flat* dan kurang berdefinisi. Pengaplikasian eyeshadow mengarahkan kontras visual (*focal point*) langsung ke bola mata pengamat.

### 1.2 Taksonomi Material & Sifat Optis (Finishes)

Dalam MUA profesional dan PBR Shader rendering, eyeshadow dikategorikan berdasarkan sifat optis permukaan (*surface optical properties*):

| Kategori Finish | Sifat Optis & Pemantulan Cahaya | Parameter PBR Shader | Blending Mode AR Engine |
|---|---|---|---|
| **Matte** | Absorpsi cahaya murni, tanpa kilau, pigmen pekat untuk bayangan kedalaman. | Roughness: `0.9 - 1.0`<br>Specular: `0.0` | **Multiply / Color Burn** (Base & Crease Depth) |
| **Satin / Pearl** | Kilau mikro lembut dengan kilapan berkilau mutiara (*pearly sheen*). | Roughness: `0.4 - 0.6`<br>Specular: `0.3` | **Soft Light / Overlay** (Transition & Wash) |
| **Metallic / Foil** | Pantulan spekular tinggi dengan kesan permukaan logam cair (*liquid metal*). | Roughness: `0.1 - 0.2`<br>Metallic: `0.8 - 1.0` | **Additive / Specular Blend** (Lid Center) |
| **Glitter / Sparkle** | Partikel diskrit mikro yang memantulkan kelap-kelip cahaya (*sparkling particles*). | Procedural Noise Mask<br>Fresnel Boost | **Custom Particle / Noise Shader** (Aegyo Sal & Halo Spot) |

---

## 2. Anatomi Zonasi Mata (Anatomical Eye Zones)

Pengaplikasian eyeshadow presisi membagi area mata menjadi **7 zona anatomi utama**:

```
                       [BROW BONE HIGHLIGHT]
                       (Under Eyebrow Arch)
                                |
                                v
     [INNER CORNER] ---> [TRANSITION ZONE] <--- [OUTER V]
      (Tear Duct)        (Upper Crease Line)    (Outer Canthus Lift)
                                |
                                v
                        [MOBILE EYELID]
                   (Center Lid Spotlight Zone)
                                |
                                v
                      [LOWER LASH LINE]
                      (Aegyo Sal Fat Roll)
```

1. **Mobile Lid (Kelopak Mata Bergerak):** Area 2D dari garis bulu mata atas hingga lipatan kelopak mata. Zona utama untuk pengaplikasian warna dasar (*base shade*) atau kilau *spotlight*.
2. **Crease (Lipatan Kelopak Mata):** Fossa lekukan antara kelopak bergerak dan tulang alis. Diisi dengan warna medium-dark matte untuk ilusi kedalaman (*shadow depth*).
3. **Transition Zone:** Area meluruh di atas crease menuju tulang alis. Diisi warna transisi lembut untuk menghilangkan garis keras (*seamless gradient*).
4. **Brow Bone (Tulang Alis):** Tepat di bawah lengkungan alis. Diisi highlight terang matte/satin untuk mengangkat ilusi tulang alis (*eye lift*).
5. **Outer V / Outer Corner:** Sudut luar berbentuk huruf 'V' miring antara garis bulu mata atas dan crease. Tempat warna gelap dipasang untuk memanjangkan atau memiringkan mata (*cat-eye lift*).
6. **Inner Corner / Tear Duct:** Sudut dalam dekat saluran air mata. Diisi shimmer/glitter terang untuk memberi kesan mata segar dan terbuka (*eye opener*).
7. **Lower Lash Line & Aegyo Sal Zone:** Area 2-5mm tepat di bawah bulu mata bawah. Zona penting untuk gaya Asia (Aegyo Sal) dan gaya Western (smudged lower liner).

---

## 3. Taksonomi & Teknik Gaya Eyeshadow Fundamental

```
+-----------------------------------------------------------------------------------+
| SMOKEY EYE   | Gradien pekat dari lash line meluruh memudar ke arah crease.      |
+--------------+--------------------------------------------------------------------+
| HALO EYE     | Gelap di inner & outer V, dengan kilau terang di tengah kelopak.  |
+--------------+--------------------------------------------------------------------+
| CUT CREASE   | Garis kontras tajam di crease memisahkan lid terang & crease gelap.|
+--------------+--------------------------------------------------------------------+
| AEGYO SAL    | Shading cokelat di lipatan lemak + shimmer di atasnya (Korean).   |
+--------------+--------------------------------------------------------------------+
| FOXY / CAT   | Gradient diagonal ditarik memanjang naik ke arah pelipis.          |
+-----------------------------------------------------------------------------------+
```

### 3.1 Smokey Eye (Classic & Modern)
* **Konsep:** Gradien dramatis di mana pigmen terpekat berada di garis bulu mata atas, meluruh (*fading*) semakin terang saat naik mendekati crease.
* **MUA Technique:** Pigmen hitam/cokelat tua di-blend dari lash line ke mobile lid, disusul warna cokelat medium pada crease zone, lalu di-blend halus dengan warna transisi.
* **AR Rendering:** Menggunakan **Alpha Gradient Falloff Mask** vertikal ber-mode *Multiply* di atas mobile lid.

### 3.2 Halo Eye (Spotlight Technique)
* **Konsep:** Menciptakan ilusi sorotan lampu di tengah kelopak mata.
* **MUA Technique:** Warna gelap (matte dark brown/plum) ditempatkan pada *Inner Corner* dan *Outer V*, menyisakan bagian tengah kelopak mata (*Mobile Lid Center*) kosong. Pusat kelopak diisi warna terang ber-shimmer/metallic.
* **AR Rendering:** Masker 3-zona (Left Dark, Center Specular, Right Dark). Center zone menggabungkan Additive Blending + Metallic Specular Shader.

### 3.3 Cut Crease (Dramatic / Glam)
* **Konsep:** Menciptakan garis demarkasi grafis yang sangat tajam antara mobile lid dan crease zone.
* **MUA Technique:** Crease di-shading tebal dengan warna gelap, lalu area mobile lid "dipotong" (*carved out*) menggunakan concealer berujung tajam sebelum diisi eyeshadow terang.
* **AR Rendering:** Menggunakan **Step Function Vector Mask** ($f(y) = \text{smoothstep}(\text{crease\_y})$) untuk menghasilkan garis tepi tajam tanpa buram di area lipatan kelopak.

### 3.4 Aegyo Sal (애교살 - Korean Youthful Under-Eye)
* **Konsep:** Meng-highlight bantalan lemak otot *orbicularis oculi* tepat 2-4mm di bawah bulu mata bawah untuk kesan mata tersenyum dan muda.
* **MUA Technique:** Garis bayangan cokelat tipis digambar tepat di bawah lipatan lemak, disusul dengan kuasan shimmer/glitter terang tepat di atas tonjolan lemak tersebut.
* **AR Rendering:** 2 Sub-layer khusus pada lower eyelid landmarks MediaPipe:
  1. *Shading Line Layer:* Line mask tipis ber-mode *Multiply* di index `362, 382, 381...` (kiri) & `33, 7, 163...` (kanan).
  2. *Highlight Patch Layer:* Oval mask ber-mode *Additive Specular* tepat di antara bulu mata bawah dan garis shading.

---

## 4. Adaptasi Bentuk Mata (Eye Shape Taxonomy)

```
+------------------------------------------------------------------------------------+
| MONOLID      | Kelopak tanpa lipatan. Soft wash gradient vertikal tanpa crease.   |
+--------------+---------------------------------------------------------------------+
| HOODED EYES  | Kelopak tertutup lipatan kulit. Membuat Floating Crease buatan.    |
+--------------+---------------------------------------------------------------------+
| ALMOND EYES  | Proporsi ideal seimbang. Kompatibel dengan seluruh gaya eyeshadow.  |
+--------------+---------------------------------------------------------------------+
| DOWNTURNED   | Sudut luar turun. Lifting Outer V diagonal tajam ke arah pelipis.   |
+------------------------------------------------------------------------------------+
```

1. **Monolid (Tanpa Lipatan Kelopak):**
   * *Strategi MUA:* Menghindari teknik *cut crease* keras. Gunakan gradien vertikal *soft wash* yang memudar halus dari garis bulu mata ke atas.
2. **Hooded Eyes (Kelopak Mata Tertutup Lipatan Kulit):**
   * *Strategi MUA:* Karena lipatan kulit menutupi mobile lid saat mata terbuka, MUA menggambar **Floating Crease** (crease buatan di atas lipatan kulit asli) agar warna eyeshadow tetap terlihat saat mata memandang lurus.
3. **Downturned Eyes (Sudut Luar Mata Turun):**
   * *Strategi MUA:* Memfokuskan warna gelap di *Outer V* yang ditarik ke atas (*winged upward*) menuju ujung alis untuk mengangkat rona mata.

---

## 5. Pemetaan ke Engine AR Rendering Real-Time ("Fizgravity")

### 5.1 Isolasi Landmark MediaPipe 468
Untuk mencegah eyeshadow melingkari seluruh mata seperti kaca mata renang, Fizgravity mengisolasi vertex khusus:

* **Upper Eyelid Boundary (Batas Bawah Eyeshadow):**
  * Kiri: `362, 398, 384, 385, 386, 387, 388, 466, 263`
  * Kanan: `33, 246, 161, 160, 159, 158, 157, 173, 133`
* **Crease & Brow Boundary (Batas Atas Eyeshadow):**
  * Kiri: `467, 341, 256, 252, 253, 254, 339, 255`
  * Kanan: `247, 112, 26, 22, 23, 24, 110, 25`
* **Aegyo Sal Lower Lid Boundary:**
  * Kiri: `362, 382, 381, 380, 374, 373, 390`
  * Kanan: `33, 7, 163, 144, 145, 153, 154`

### 5.2 Formulasi Shader MSL/GLSL & Simulation Glitter Partikel

#### A. Base Shadow & Crease Shading (Multiply + Soft Light):
$$\text{Color}_{\text{base}} = \text{Mix}(C_{\text{skin}}, C_{\text{skin}} \times C_{\text{eyeshadow}}, \text{Alpha}_{\text{mask}})$$

#### B. PBR Shimmer / Metallic Highlight:
$$\text{Color}_{\text{shimmer}} = \text{Color}_{\text{base}} + \text{Light}_{\text{specular}} \times N \cdot H^{\text{shininess}} \times \text{Mask}_{\text{center}}$$

#### C. Dynamic Procedural Glitter Shader (Partikel Kelap-Kelip Dinamis):
Untuk mensimulasikan partikel glitter yang berkilauan saat kepala menoleh, MSL shader menghitung **Simplex 3D Noise** yang terikat pada normal vector wajah ($\vec{N}$) dan vektor sudut pandang kamera ($\vec{V}$):

```metal
float3 viewDir = normalize(in.viewVector);
float noiseVal = simplex3DNoise(in.uv * 150.0 + viewDir * 5.0);
float glitterSparkle = pow(saturate(noiseVal), 12.0) * 8.0;

float4 finalColor = baseColor + float4(glitterColor * glitterSparkle * maskWeight, 0.0);
```

### 5.3 Auto Deformasi Otomatis Kedipan Mata
Karena koordinat UV eyeshadow di-pin pada vertex kelopak mata MediaPipe, ketika pengguna berkedip:
* Vertex kelopak mata atas mendekati kelopak mata bawah.
* Tekstur eyeshadow terkompresi dan ter-stretch secara **alamiah 100%** tanpa membutuhkan custom physics engine.

---

## 6. Rekomendasi Integrasi Pipeline Fizgravity

1. **Preset Eyeshadow Multi-Style:** Menyediakan 4 preset style utama di JavaScript/React Native (`Smokey`, `Halo`, `CutCrease`, `AegyoSal`).
2. **Slider Kontrol Terpisah:** Memisahkan slider `eyeshadowOpacity` (Base Matte) dan `eyeshadowShimmer` (Metallic Specular & Procedural Glitter Sparkle).
3. **Sub-Layer Aegyo Sal Dual-Pass:** Aegyo Sal wajib dipisah menjadi 2 pass: Pass 1 (Multiply Line Shadow) dan Pass 2 (Additive Highlight Shimmer).
