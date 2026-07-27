# 📖 THE MASTER AR BEAUTY PIPELINE & DICTIONARY v2.0
*(Arsitektur Lengkap: Anatomi MUA Profesional × GPU Render Engine × MediaPipe Face Mesh)*

> **Dokumen Acuan Mutlak** — Setiap keputusan implementasi engine harus merujuk dokumen ini.  
> Sumber: Riset Deep Dive 4 Spesialis (MUA Profesional + AR Graphics Engineering).  
> Terakhir diperbarui: 2026-07-27

---

## 🧠 KONSEP DASAR: DIMENSI RENDER (2D vs 3D)

| Sifat | Tujuan | Blend Mode | Contoh |
|-------|--------|-----------|--------|
| **2D (Flat)** | Menyamarkan — mengoleskan warna merata | `Normal Alpha` | Foundation, Concealer, Eyeliner |
| **2.5D (Semi-3D)** | Menghidupkan — dimensi ringan + gradasi | `Soft Light` | Blush On, Eyeshadow |
| **3D (Volume)** | Memahat — manipulasi cahaya & bayangan | `Multiply`, `Screen` | Contour, Highlighter, Lipstick Gloss |

---

## 🎨 URUTAN RENDER MUTLAK (Z-INDEX EXECUTION ORDER)

```
[0] KAMERA ASLI → [1] FOUNDATION → [2] CONCEALER → [3] CONTOUR
→ [4] BLUSH ON → [5] HIGHLIGHTER → [6] SOFTLENS → [7] EYESHADOW
→ [8] EYELINER → [9] EYEBROWS → [10] LIPSTICK → [11] EYELASHES
```

> ⚠️ Pelanggaran urutan ini akan menyebabkan hasil visual yang salah (mis. eyeliner tertimpa foundation).

---

## ═══════════════════════════════════════════
## LAYER 1: COMPLEXION & BASE (Kanvas 2D)
## ═══════════════════════════════════════════

## [1] FOUNDATION (Alas Bedak)
**Sifat Render: 2D Flat | Shader: Frequency Separation + Hybrid Blend**

### A. Tipe Berdasarkan FINISH

#### 1. Matte Finish
- **Karakteristik:** Zero specular reflection, pore-blurring effect, velvet-like surface, oil control.
- **Jenis Kulit:** ✅ Oily (sangat cocok) | ⚠️ Dry (berisiko cakey) | ✅ Combination (T-zone only) | ✅ Normal
- **PBR Params:** Roughness `0.85–1.00` | Specular `0.00–0.05` | Opacity `0.50–0.85`
- **GLSL Blend:** `Normal Alpha Blend + Oren-Nayar Diffuse`
- **High-Freq Treatment:** Bilateral Blur (menyaring 50% pori-pori)
- **Shader Logic:**
  ```glsl
  // Foundation Matte: absorb specular hotspots
  float luma = dot(matteAlbedo, vec3(0.299, 0.587, 0.114));
  vec3 finalMatte = mix(matteAlbedo, vec3(luma), 0.05); // Anti-chalky desaturation
  return mix(baseSkin, finalMatte, opacity);
  ```

#### 2. Dewy / Glass Skin
- **Sub-tipe:**
  - **K-Beauty "Chok Chok":** Efek basah merata, cool-pink undertone, zero shimmer partikel.
  - **Western Sun-Kissed Glow:** Golden warm undertone, terfokus di high-points, fine mica shimmer.
- **Cara Mencegah Efek "Greasy":** WAJIB memutus specular pada T-zone (dahi tengah, alar base, dagu) menggunakan `u_HighPointMask`.
- **PBR Params:** Roughness `0.15–0.35` (high-points) / `0.60` (T-zone) | Specular `0.60–0.85` | Fresnel F₀ `0.04–0.08`
- **GLSL Blend:** `Screen + Additive Specular`
- **High-Freq Treatment:** Retain 90% + additive specular pada high-points

#### 3. Satin / Semi-Matte
- **Karakteristik:** Velvety-smooth radiance, tidak flat & tidak kilap. Benchmark produk premium (Armani Luminous Silk).
- **PBR Params:** Roughness `0.45–0.60` | Specular `0.15–0.30`
- **GLSL Blend:** `Soft Light (50%) + Normal Alpha (50%)`
- **High-Freq Treatment:** Retain 70%

#### 4. Luminous
- **Perbedaan vs Dewy:** Luminous = inner radiance dari mikro-pigmen mutiara (multi-directional diffuse). Dewy = lapisan basah di permukaan (wet film specular).
- **GLSL Blend:** `Overlay/Soft Light + Anisotropic Pearlescent`
- **High-Freq Treatment:** Retain 80%

#### 5. Natural / Skin-Tint
- **Perbedaan vs Sheer:** Sheer = ukuran coverage. Skin-Tint = perilaku material. Skin-Tint mengambil Roughness & Specular dari kulit asli pengguna, hanya mentransfer perubahan hue/saturation.
- **GLSL Blend:** `Color / Chroma Blend`
- **High-Freq Treatment:** Retain 100%

---

### B. Tipe Berdasarkan COVERAGE

| Coverage | Definisi | Freckles/Pori Visible | Opacity AR | Shader Strategy |
|----------|---------|----------------------|-----------|----------------|
| **Sheer/Light** | Menyamarkan rona tidak merata tipis | 80–90% | `0.20–0.40` | Soft Light/Overlay, High-Freq 100% |
| **Medium** | Tutup kemerahan, 70% noda bekas jerawat | 30–50% | `0.45–0.70` | Normal Alpha + Preserved Luminance |
| **Full Coverage** | Tutup hiperpigmentasi, 90–100% noda | 0–10% | `0.75–0.92` | Opaque Alpha + **Synthetic Pore Noise Injection** |
| **Buildable** | Bisa di-layer dari sheer ke full | Dynamic | `0.20–0.90` | Non-Linear: α_eff = 1-(1-α_step)^N |

---

### C. Mencegah Efek ASHINESS pada Kulit Gelap (Fitzpatrick IV-VI)

```glsl
// Luminance Compensation untuk Dark Skin
float skinLuma = dot(userSkinRGB, vec3(0.299, 0.587, 0.114));
if (skinLuma < 0.38) {
    maxOpacity = 0.65; // Turunkan dari 0.85 ke 0.65
    blendMode = SOFT_LIGHT; // Ganti dari NORMAL ke SOFT_LIGHT
    // Warmth injection untuk kompensasi melanin
    foundationColor.r += 0.05;
    foundationColor.g += 0.02;
}
```

---

### D. Color Matching Architecture (CIELAB / ITA)

```
[Camera] → [Face ROI Sampling] → [RGB→CIELAB] → [Hitung ITA]
ITA = arctan((L*-50)/b*) × (180/π)
→ [Skin Tone Category] + [Undertone via a* & b*]
→ [Shade Recommendation via Delta E 2000]
```

| Undertone | Shader Adjustment |
|-----------|-----------------|
| Warm (Yellow/Golden) | Push b* positive, Warm overlay |
| Cool (Pink/Blue) | Push a* positive, Cool overlay |
| Neutral | Balanced L* adjustment only |

---

## [2] CONCEALER (Penyamar Noda)
**Sifat Render: 2D Flat | Opacity Tinggi + Edge Feathering Ekstrem**

### A. Taksonomi Jenis Concealer

#### Color Corrector (Teori Complementary Color)
| Corrector | Warna yang Dicancel | Target Area |
|-----------|-------------------|-------------|
| 🟢 **Green** | Merah (Redness) | Jerawat meradang, rosacea, cuping hidung |
| 🍑 **Peach/Salmon** (untuk kulit terang) | Biru/Ungu | Dark circles bawah mata |
| 🟠 **Orange** (untuk kulit gelap) | Biru/Ungu | Dark circles + hiperpigmentasi |
| 🟡 **Yellow** | Ungu tua/Biru | Memar, pembuluh darah |
| 💜 **Lavender** | Kuning kusam | Sallow skin / kulit pucat kusam |

#### Traditional Concealer
- **Spot Concealer:** Full coverage, matte, 1:1 dengan foundation shade.
- **Brightening Concealer:** Medium coverage, luminous, **1–2 shade lebih terang** dari foundation.
- **Sculpting Concealer:** Full coverage, **2–3 shade lebih gelap** (liquid contouring).

---

### B. Area Aplikasi & MediaPipe Landmarks

| Area | MediaPipe Landmarks | Teknik MUA |
|------|-------------------|------------|
| **Dark Circles (Tear Trough)** | `[33, 263, 230, 450, 130, 359]` | Layer 1: Peach Corrector → Layer 2: Brightening |
| **Blemish/Acne Spots** | Dynamic UV point | Titik pusat + feathered edge |
| **Redness (Alar Base)** | `[234, 454, 98, 327]` | Green Corrector tipis, di-blend keluar |
| **Triangle of Light (Under-eye)** | Inner–Outer Canthus + Nostril | Segitiga terbalik, efek well-rested |
| **Lifting Corner** | Outer canthus → temple, mouth corner → ear | Fox-eye face lift effect |

---

### C. GLSL Concealer Shader (Radial Feathering)

```glsl
vec3 applyConcealerSpot(vec2 uv, vec3 baseColor, vec2 center, float innerR, float outerR, float opacity) {
    float dist = distance(uv, center);
    float edgeMask = 1.0 - smoothstep(innerR, outerR, dist);
    float finalAlpha = edgeMask * opacity;
    return mix(baseColor, u_ConcealerColor, finalAlpha);
}
```

---

## ═══════════════════════════════════════════
## LAYER 2: FACE SCULPTING (Pemahat 3D)
## ═══════════════════════════════════════════

## [3] CONTOUR / SHADING (Pemahat Tulang)
**Sifat Render: 3D Bayangan | Blend Mode: Multiply (60%) + Linear Burn (40%)**

### A. Formula Shader Contour (Hybrid Deep Shadow)

```glsl
float contourAlphaFinal = contourAlpha * uContourColor.a;
vec3 mulResult = currentSkin * uContourColor.rgb;               // Multiply: standard darkening
vec3 burnResult = max(currentSkin + uContourColor.rgb - 1.0, 0.0); // Linear Burn: deep shadow
vec3 deepContour = mix(mulResult, burnResult, 0.4);             // 60% Multiply + 40% Linear Burn
float effectiveAlpha = pow(contourAlphaFinal, 0.85);            // Power curve: non-linear response
currentSkin = mix(currentSkin, deepContour, effectiveAlpha);
```

---

### B. Contour Berdasarkan FACE SHAPE

| Face Shape | Area Utama Contour | MediaPipe Landmarks | Teknik MUA | Intensitas |
|-----------|-------------------|--------------------|-----------|-----------| 
| **Oval** | Cheek hollows + Hairline | `[187,123,205]` + `[10,67,109]` | Natural Soft Shadowing | Opacity 20–30% |
| **Round** | Steep cheek hollows + Jawline | `[187,123,50,205]` + `[132,172]` | **Hatchet Technique** | Opacity 40–55% |
| **Square** | Jaw angles + Outer forehead | `[132,58,172,136]` + `[103,67,109]` | **3/E Softening Arc** | Opacity 45–60% |
| **Oblong** | Top hairline + Chin tip | `[10,67,109,297,338]` + `[152,175,199]` | **Horizontal Truncation** | Opacity 45–60% |
| **Heart** | Temples + Chin apex | `[21,54,103,67]` + `[152]` | **Temporal Fossa Reduction** | Opacity 40–50% |
| **Diamond** | Outer zygomatic arch only | `[187,123,147,207]` | **Zygomatic Flare Cut** | Opacity 35–50% |

---

### C. Teknik MUA Spesifik

1. **Hatchet Technique** (Round Face): Garis dari tragus telinga menyusuri bawah pipi, dipotong vertikal di bawah outer canthus.
2. **3/E Technique** (Square Face): Menyusuri hairline dahi → bawah pipi → membungkus sudut rahang.
3. **Horizontal Truncation** (Oblong Face): Pita shading horizontal di atas hairline dan bawah dagu.
4. **Temporal Fossa Reduction** (Heart Face): Shading diagonal pada pelipis, bukan pada pipi.
5. **Zygomatic Flare Cut** (Diamond Face): Shading vertikal-curved 75–90° hanya pada tonjolan pipi terluar.

---

## [4] BLUSH ON (Perona Pipi)
**Sifat Render: 2.5D | Blend Mode: Hybrid Normal (65%) + Soft Light (35%)**

### A. Formula Shader Blush (Pigmented Hybrid)

```glsl
float blushStrength = blushAlpha * uBlushColor.a;
vec3 softBlush = blendSoftLight(currentSkin, uBlushColor.rgb);  // Soft edge follow skin
vec3 normalBlush = uBlushColor.rgb;                              // Pigmented direct color
vec3 pigmentedBlush = mix(softBlush, normalBlush, 0.65);        // 65% Normal + 35% Soft Light
float skinLum = dot(currentSkin, vec3(0.299, 0.587, 0.114));
pigmentedBlush *= (skinLum * 0.4 + 0.8);                        // Micro-lighting preservation
currentSkin = mix(currentSkin, pigmentedBlush, blushStrength);
```

---

### B. Blush Berdasarkan FACE SHAPE

| Face Shape | Area Aplikasi | Landmarks | Bentuk Gradien | Sudut | Intensitas |
|-----------|--------------|-----------|---------------|-------|-----------|
| **Oval** | Puncak tulang pipi, meluncur ke pelipis | Left `[116,117,123,205]` / Right `[345,346,352,425]` | Soft Elliptical | 30–45° | Opacity 25–40% |
| **Round** | Tinggi di outer cheekbone, HINDARI apple cheek | Left `[123,147,187,207]` | Draped Linear / Upward Crescent | 45–60° | Opacity 35–50% |
| **Square** | Apple of cheek, pusat pipi | Left `[116,123,50,205]` | Soft Circular Radial | 0–15° | Opacity 30–45% |
| **Oblong** | Pita horizontal lintas pipi, opsional sambung hidung | Left `[117,118,123,205]` + `[197,195]` | Horizontal Bar | 0° (Flat) | Opacity 35–50% |
| **Heart** | Di bawah puncak pipi, melengkung ke telinga | Left `[116,123,147,205]` | Soft C-Shape Crescent | 30° | Opacity 25–35% |
| **Diamond** | Tepat di apple cheek, terpusat di depan | Left `[116,50,205,206]` | Compact Circular | 0–15° | Opacity 25–40% |

---

### C. 9 Style Blush On (Katalog Lengkap)

| # | Style | Definisi | Ideal Face Shape | MediaPipe Key Landmarks |
|---|-------|----------|-----------------|------------------------|
| 1 | **Apple Cheek** | Bulat di pusat pembengkakan otot pipi saat senyum | Oval, Oblong, Heart | Left `[50,116,123]` |
| 2 | **Draping/Lifted** | Dari outer cheekbone naik menyapu ke pelipis hingga brow bone | Round, Square, Diamond | `[123→21→70]` |
| 3 | **Igari / Sun-kissed** | Horizontal dari pipi kiri-kanan melewati nose bridge | Oblong, Oval, Heart | `[197,195]` + `[117–121]` |
| 4 | **Halo Blush** | Ring blush + core center dewy highlight | Oval, Diamond | Ring `[116,123]` + Core `[50]` |
| 5 | **Contour Blush** | Di cheek hollows, warna terracotta/warm nude-brown | Round, Oval, Square | `[187,123,205,207]` |
| 6 | **Under-eye Blush** | Di infraorbital rim, mm dari lower lash line | Heart, Oblong, Oval | `[116,230,231,229,228]` |
| 7 | **Monochromatic Flush** | Warna seragam di pipi + crease mata + ujung hidung + tengah bibir | Universal | `[70,63]`+`[50]`+`[4]`+`[0,17]` |
| 8 | **Sunset Ombré Blush** | 2–3 gradasi warna: coral → pink → plum menuju pelipis | Round, Square, Diamond | Inner `[50]`→Mid `[123]`→Outer `[21]` |
| 9 | **Boy Blush / Low Flush** | Lebih rendah dari standar MUA, menuju garis rahang | Oblong, Heart, Oval | Left `[187,207,138,215]` |

---

### D. GLSL: Rotated Elliptical Gaussian Blush Mask

```glsl
float calculateBlushAlpha(vec2 uv, vec2 centerUV, float angleRad, float sigmaU, float sigmaV) {
    vec2 d = uv - centerUV;
    float cosA = cos(angleRad), sinA = sin(angleRad);
    vec2 rotatedD = vec2(d.x*cosA - d.y*sinA, d.x*sinA + d.y*cosA);
    float exponent = -0.5 * (pow(rotatedD.x/sigmaU, 2.0) + pow(rotatedD.y/sigmaV, 2.0));
    return exp(exponent);
}
```

---

## [5] HIGHLIGHTER (Penarik Cahaya)
**Sifat Render: 3D Pantulan Dinamis | Blend Mode: Screen / Linear Dodge**

### A. 4 Tipe Highlighter

| Tipe | Efek Visual | PBR Roughness | Blend Mode | High-Freq |
|------|-----------|--------------|-----------|----------|
| **Powder** | Crystalline shimmer, directional sparkle | `0.15–0.25` | Screen + GGX Specular | Particle Noise Overlay |
| **Liquid/Strobe Cream** | Dewy wet-skin, seamless texture | `0.40–0.50` | Soft Light + Overlay | Frequency Separation Pass |
| **Baked/Holographic** | Duochrome iridescent (warna berubah per sudut) | Dynamic via Fresnel | Fresnel Color Shift + Additive | 1D Gradient Lookup |
| **Subtle Glow** | Satin natural, no visible glitter | `0.55–0.70` | Soft Light (opacity 0.15–0.35) | Gaussian Feathering |

---

### B. 8 Area Aplikasi Highlighter

| Area | Shape Mask | Intensitas Alpha | Aturan Kritis |
|------|-----------|-----------------|--------------|
| **Puncak Tulang Pipi** | Inverted C-Shape / Crescent | 0.60–0.85 | Wajib mengikuti zygomatic arch |
| **Batang Hidung (Bridge)** | Vertical Thin Pill | 0.40–0.65 | Panjang MAKSIMUM 50–60% hidung. **Jangan sambung ke tip hidung!** |
| **Tip Hidung** | Small Circular Dot | 0.70–0.95 | Titik presisi pada landmark 4/5 |
| **Cupid's Bow** | Sharp V/Chevron | 0.75–0.90 | Ikuti vermilion border atas: `[0,37,267]` |
| **Brow Bone** | Curved arc bawah 2/3 luar alis | 0.50–0.70 | Mengangkat dimensi alis |
| **Inner Corner Eye** | Teardrop / Belah ketupat | 0.80–1.00 | Wide-awake effect |
| **Forehead Strobe** | Oval melintang T-zone | 0.15–0.35 | Very sheer, non-greasy |
| **Chin** | Oval kecil di puncak dagu | 0.25–0.45 | Balance aksis vertikal |

---

### C. Shade Highlighter per Skin Tone

| Skin Tone | Recommended Shade | Hindari |
|-----------|------------------|---------|
| Fair/Light | Icy Champagne, Pearl, Opal, Soft Pink | Deep Gold, Bronze (terlihat seperti noda) |
| Medium/Olive | Warm Champagne, Rose Gold, Peach Gold | Pure White, Icy Silver (terlihat ashy) |
| Dark/Deep | Rich Gold, Bronze, Copper, Metallic Amber | Cool Silver, Icy White, Pale Pink (terlihat chalky) |

---

### D. Strategi Highlighter per Face Shape

| Face Shape | Strategi | Efek Visual |
|-----------|---------|------------|
| **Round** | Vertikal di bridge hidung, chin, cheekbone tinggi-miring | Memperpanjang aksis vertikal |
| **Square** | Lingkaran lembut di apex cheekbone + dahi tengah + dagu | Melembutkan sudut rahang |
| **Heart** | Lebar di dagu + sepanjang garis rahang bawah | Menyeimbangkan dahi lebar |
| **Oblong** | Horizontal lebar di puncak pipi, bridge pendek saja | Memberikan ilusi lebar horizontal |

---

## ═══════════════════════════════════════════
## LAYER 3: EYE ENHANCEMENTS
## ═══════════════════════════════════════════

## [6] SOFTLENS (Contact Lens)
**Sifat Render: 3D Bola Cembung | MediaPipe Iris Landmarks 468–477**

### A. 7 Tipe Softlens

| # | Tipe | Graphic Diameter | Limbal Ring | Efek Visual |
|---|------|-----------------|-------------|------------|
| 1 | **Natural Enlarging** | 13.2–13.5mm | Soft, blurred | Sedikit memperbesar, natural |
| 2 | **Circle/Dolly** | 13.8–14.5mm | Hitam tebal dan jelas | Anime look, mata sangat besar |
| 3 | **Hazel/Brown** | 13.5–14.0mm | Warm multi-tone blend | Natural warm iris |
| 4 | **Green/Blue Exotic** | 13.5–14.5mm | High opacity mask | Warna total iris berubah |
| 5 | **Gray** | 13.5–14.0mm | Cool neutral multi-tone | Hazel inner + gray body |
| 6 | **Colored No Ring** | 13.2–14.0mm | Seamless edge (no ring) | Natural color change |
| 7 | **Cat/Slit Pupil** | Variable | Slit vertical | Editorial/cosplay fantasy |

---

### B. MediaPipe Iris Landmarks

- **Left Iris Center:** Landmark `468`
- **Left Iris Perimeter:** `469, 470, 471, 472`
- **Right Iris Center:** Landmark `473`
- **Right Iris Perimeter:** `474, 475, 476, 477`

---

### C. Pupil Hole Masking + Corneal Glint Re-injection

```glsl
// 1. Pupil Alpha Mask (dynamic dilation)
float distToPupil = length(currentUV - irisCenterUV);
float pupilMask = smoothstep(0.08, 0.18, distToPupil); // 0.08=miosis, 0.18=mydriasis
vec4 lensColor = vec4(softlensRGB, softlensTex.a * pupilMask);

// 2. Corneal Glint Re-injection (prevents dead eye effect)
float cameraLum = dot(originalCameraRGB, vec3(0.299, 0.587, 0.114));
float corneaGlint = smoothstep(0.75, 0.98, cameraLum);
vec3 coloredIris = mix(originalCameraRGB, lensColor.rgb, lensColor.a);
vec3 finalEye = coloredIris + vec3(corneaGlint * 0.85); // Additive glint re-inject
```

---

## [7] EYESHADOW (Kelopak Mata)
**Sifat Render: 3D Elastis | Shader: Multi-pass + Blink Dynamics**

### A. MediaPipe Eye Landmarks

```
Brow Bone:      [70, 63, 105, 66]      /  [336, 296, 334, 293]
Upper Crease:   [22,23,24,110,25,26]  /  [252,253,254,339,255,256]
Lid (Mobile):   [246,161,160,159,158] /  [398,384,385,386,387]
Lash Line:      [33,160,159,158,133]  /  [362,398,384,385,386,263]
Lower Lid:      [145,144,153,154,155] /  (mirror)
Aegyo Sal:      [111,117,118,119,120,121] / [340,346,347,348,349,350]
```

---

### B. 8 Teknik Eyeshadow

| # | Teknik | Deskripsi | AR Render Pipeline |
|---|--------|----------|-------------------|
| 1 | **Smokey Eye** | Dark di lash line, di-blend naik ke crease | Multi-pass: Multiply (lash) → Soft Light (crease) → Large Blur |
| 2 | **Cut Crease** | Kontras tajam: lid terang, crease gelap | SDF Hard Alpha Stencil pada crease line |
| 3 | **Halo Eye** | Dark inner/outer, light/shimmer center | 3-Zone horizontal: Multiply / Additive / Multiply |
| 4 | **Monochromatic** | Satu rumpun warna, variasi tekstur | Uniform Color + Roughness Variation Map |
| 5 | **Gradient/Ombré** | Light inner → medium mid → dark outer | UV horizontal interpolation 3-stop gradient |
| 6 | **Aegyo Sal** | Shimmer bawah mata (K-Beauty kantong mata) | Upper: Linear Dodge band + Lower: Thin Multiply strip |
| 7 | **Colored Smokey** | Jewel tones (navy, emerald, purple) | Dual-layer: Dark base (Multiply) + Vivid color (Screen) |
| 8 | **Graphic/Editorial** | Garis geometris, floating crease, hard edge | Vector Path dengan `step()` function, zero feathering |

---

### C. Blink Dynamics (Eyelid Rigging)

| Area | Vertex Weight | Efek Saat Blink |
|------|--------------|----------------|
| Lash line (mobile lid) | 1.0 | Penuh mengikuti gerakan |
| Crease fold | 0.4–0.7 | Terkompres/melipat |
| Brow bone | 0.0 | Statis |

**Specular Flash:** Saat blink, perubahan cepat N·L dan N·H menyebabkan sparkle burst pada shimmer eyeshadow.

---

### D. Blended vs Sharp Eyeshadow

```glsl
// BLENDED (Soft Gradient)
float blendFactor = smoothstep(d_min, d_max, distance(UV, creaseLine));

// SHARP (Cut Crease / Graphic)
float alpha = step(threshold, sdf(UV)); // step() = zero feathering
```

---

## [8] EYELINER (Garis Mata)
**Sifat Render: 2D Vector | WAJIB di atas Eyeshadow**

### A. 8 Tipe Eyeliner

| # | Tipe | Deskripsi & Sudut | AR Shader |
|---|------|------------------|----------|
| 1 | **Classic Wing/Cat Eye** | Wing 30–45° ke atas dari outer canthus | Solid Vector Mask, Opacity 0.95–1.0 |
| 2 | **Puppy Liner** | Wing 0° hingga -15° ke bawah dari outer canthus | Solid Dark Brown/Black, Opacity 0.90 |
| 3 | **Tight-line/Waterline** | Di inter-marginal rim akar bulu mata atas | Multiply Blend, 1–2px width |
| 4 | **Double Liner** | 2 wing (atas + bawah / warna berbeda) | Dual Vector Path |
| 5 | **Graphic Liner** | Garis geometri melayang di atas crease | High-Precision Bezier Spline |
| 6 | **Smudged/Kohl** | Soft diffused smoke di lash line | Gaussian Alpha Falloff, Multiply |
| 7 | **Lower Lash Only** | 1/3 atau full lower lash line | Soft Multiply, Opacity 0.70 |
| 8 | **Floating Liner** | Melayang di atas kelopak, di atas crease | Solid/Neon, parametrik Y superior |

---

### B. Adaptasi Eye Shape

| Eye Shape | Masalah AR | Solusi Khusus |
|-----------|-----------|--------------|
| **Hooded** | Garis patah saat mata terbuka | **Batwing Technique** — gambar berbentuk sayap kelelawar |
| **Monolid** | Liner tenggelam saat mata terbuka | Tebalkan 2.5× pada area outer 1/3 + Floating Liner |
| **Downturned** | Wing mengikuti sudut ke bawah | Wing DIMULAI sebelum outer canthus, langsung ke atas |
| **Almond** | Ideal | Semua teknik cocok |

---

### C. Persepsi Visual per Teknik

| Arah Wing | Efek pada Persepsi Mata |
|-----------|------------------------|
| ↗ Upward (Cat Eye) | Mata terlihat panjang, lifted, sultry, dewasa |
| ↘ Downward (Puppy) | Mata terlihat bulat, besar, imut, innocent |
| Inner Corner Liner | Mata terlihat lebih dekat (inter-pupillary distance menyempit), tajam |
| Inner Corner Highlight | Mata terlihat lebih jauh, terbuka, segar |

---

## [9] EYEBROWS (Alis Mata)
**Sifat Render: 2D Tekstur Helaian | Haram memblok padat!**

### A. 7 Bentuk Alis

| # | Bentuk | Karakteristik | Efek Visual |
|---|--------|--------------|------------|
| 1 | **Korean Straight** | Horizontal flat, arch minimal | Wajah lebih muda, ramah, lembut |
| 2 | **High Arch/Western** | Puncak tinggi di 2/3 outer, dramatic | Lifted, glamor, tegas |
| 3 | **Soft Arch** | Lengkungan moderat, natural | Universal, seimbang |
| 4 | **Feathery/Soap Brow** | Helaian disisir ke atas, glossy wax | Individual hair strokes, natural messy |
| 5 | **Bold/Thick** | Lebat bervolume tinggi, penuh | Power look, Cara Delevingne style |
| 6 | **Thin Penciled** | Tipis melengkung halus, 90s/Y2K | Pamela Anderson retro look |
| 7 | **Ombré Brow** | Terang di pangkal → pekat di ekor | Gradient, modern |

---

### B. Korelasi Alis × Face Shape

| Face Shape | Bentuk Alis Terbaik | Logika MUA |
|-----------|-------------------|-----------|
| **Oval** | Soft Arch / Natural | Mempertahankan keseimbangan alami |
| **Round** | High Arch / Angular | Puncak tinggi menciptakan ilusi vertikal, meniruskan |
| **Square** | Soft Rounded Arch | Kurva lembut melunakkan rahang keras |
| **Heart** | Soft Low Arch / Rounded | Lengkungan rendah menyeimbangkan dahi lebar |
| **Oblong** | Korean Straight / Flat | Garis horizontal memotong panjang vertikal wajah |
| **Diamond** | Curved / Soft High Arch | Melunakkan tonjolan tulang pipi yang lebar |

---

## ═══════════════════════════════════════════
## LAYER 4: OUTER SHELL (PBR Mutlak)
## ═══════════════════════════════════════════

## [10] LIPSTICK (Bibir)
**Sifat Render: 3D PBR Ekstrem | Diikat ke otot bibir (landmark deformasi)**

### A. 10 Tipe Lipstick

| # | Tipe | PBR Roughness | Specular | Opacity | Blend Mode | Teknik Khas |
|---|------|--------------|---------|---------|-----------|------------|
| 1 | **Matte Liquid** | 0.95–1.00 | 0.00 | 0.75–0.90 | Multiply + Normal | Film kering, transfer-proof |
| 2 | **Cream/Satin** | 0.40–0.55 | 0.25–0.40 | Medium | Soft Light/Overlay | Sheen emolien |
| 3 | **Glossy** | 0.05–0.15 | 0.70–0.95 | Medium | Dual-Pass Additive Specular | Pantulan tajam |
| 4 | **Glass Lip** | 0.01–0.05 | Fresnel F₀=0.04 | 0.60 | Stain Base + Clearcoat | Mirror-like vinyl |
| 5 | **Sheer/MLBB** | 0.30–0.50 | Low | 0.25–0.45 | Soft Light / Color Burn | Sub-Surface Scattering mukosa |
| 6 | **Plumping** | Medium | Center boost | Medium | Normal Map Deformation | UV expand 2–4% |
| 7 | **Ombré Lip** | Varies | Varies | Inner 0.85–0.95 / Outer 0.10–0.30 | Radial Falloff | Radial distance UV masking |
| 8 | **Overlined** | 0.90 (matte liner) | Minimal | High edge | Vertex Inflation | Pindah landmark vermilion 0.5–2mm |
| 9 | **Blurred/Smudged** | Medium | Low | Medium | Gaussian Feathering | Hilangkan tepi tajam |
| 10 | **Metallic/Foil** | 0.15–0.25 | 0.75–0.95 | High | Procedural Shimmer Noise | Metallic reflection + glitter |

---

### B. Aturan Overline (WAJIB DIIKUTI)

| Area | Batas Overline | Aturan |
|------|---------------|--------|
| Cupid's bow (tengah atas) | 1.0–1.5mm ke atas | ✅ Boleh |
| Bottom lip center | 1.5–2.0mm ke bawah | ✅ Boleh |
| **Outer corners (commissures `[61, 291]`)** | **0mm** | ❌ **HARAM OVERLINE** — efek badut/mulut melebar |

---

### C. Ombre Lip: Inner vs Outer Rules

| Style | Inner Shade | Outer Shade | Logika |
|-------|------------|------------|--------|
| **Korean Gradient (K-Beauty)** | GELAP (cherry, wine, plum) | TERANG/NUDE (peach, beige) | Bitten lip effect, bibir terlihat kecil & manis |
| **Western 90s Contour** | TERANG (nude pink, cream gloss) | GELAP (brown, mauve liner) | 3D volume: bayangan pinggir + highlight tengah |

---

### D. Preservasi Kerutan Bibir (Lip Lines Retention)

```glsl
// Ekstraksi micro-wrinkle detail dari kamera (lip line preservation)
float baseLuminance = dot(baseCameraRGB, vec3(0.299, 0.587, 0.114));
float lipDetailRatio = baseLuminance / (blurredLuminance + 0.001);
vec3 pigmentedLips = lipColor.rgb * baseCameraRGB.rgb;  // Multiply base
vec3 texturedMatte = pigmentedLips * clamp(lipDetailRatio, 0.6, 1.4); // Apply kerutan
```

---

### E. Deformasi Senyum (Smile Deformation)

- Landmark `61` & `291` (sudut bibir) ditarik lateral+superior saat senyum.
- Jarak vertikal `[0]`↔`[17]` menyempit (vertical compression).
- UV megar horizontal → Roughness turun `~0.05` (specular menyebar melintang).

---

## [11] EYELASHES (Bulu Mata Palsu)
**Sifat Render: 3D Tonjolan Ekstrusi | Layer paling luar area mata**

### Katalog Tipe

| Tipe | Karakteristik | Efek |
|------|--------------|------|
| **Natural Wispy** | Helaian tipis mengembang di ujung | Natural everyday |
| **Doll Eye** | Tebal merata, lebih lebat di tengah | Mata bulat dolly |
| **Foxy Eye** | Lebat dan panjang di sudut luar | Mata memanjang tajam |

---

## ═══════════════════════════════════════════
## LAMPIRAN TEKNIKAL
## ═══════════════════════════════════════════

## FACE SHAPE CLASSIFICATION ALGORITHM (Real-Time MediaPipe)

```glsl
// 3 Euclidean Ratio dari 468 Landmarks
R_aspect       = distance(P10, P152) / distance(P234, P454)   // Height/Width
R_jaw_cheek    = distance(P132, P361) / distance(P234, P454)  // Jaw/Cheek
R_forehead_jaw = distance(P54, P284) / distance(P132, P361)   // Forehead/Jaw

// Classification Thresholds
Oval:    R_aspect [1.35-1.50] | R_jaw_cheek [0.70-0.80]
Round:   R_aspect < 1.30      | R_jaw_cheek >= 0.82
Square:  R_aspect < 1.32      | R_jaw_cheek >= 0.85 | R_forehead_jaw <= 1.05
Oblong:  R_aspect > 1.55
Heart:   R_jaw_cheek < 0.68   | R_forehead_jaw >= 1.30
Diamond: R_jaw_cheek < 0.68   | R_forehead_jaw < 1.05
```

---

## MASTER BLEND MODE REFERENCE TABLE

| Layer | Blend Mode | Formula |
|-------|-----------|---------|
| Foundation | Normal Alpha + Overlay | `mix(skin, foundation, opacity * mask)` |
| Contour | Multiply (60%) + Linear Burn (40%) | `mix(skin*contour, max(skin+contour-1,0), 0.4)` |
| Blush | Soft Light (35%) + Normal (65%) | See §4.A |
| Highlighter | Screen | `1-(1-base)*(1-highlight)` |
| Eyeshadow | Multi-pass (Multiply+Screen) | See §7.B |
| Lipstick Matte | Multiply | `base * lipColor` |
| Lipstick Gloss | Dual Additive Specular | `diffuse + pow(NdotH, exp) * specular` |

---

*Dokumen ini adalah Kitab Suci (Holy Grail) AR Beauty Engine MatchAndBeauty.*
*Setiap perubahan arsitektur WAJIB merujuk kembali ke dokumen ini.*
*v2.0 — Disusun dari riset MUA Expert & AR Graphics Engineering.*