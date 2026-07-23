**Dokumen "The Holy Grail" (Kitab Suci) Arsitektur Beauty AR**.

Saya telah menyatukan **Master Dictionary** (11 Fitur Makeup beserta tipe-tipenya) dengan **The Master Render Pipeline** (Urutan Z-Index / Perbedaan 2D vs 3D) menjadi satu dokumen utuh yang sangat brutal detailnya.

---

# 📖 THE MASTER AR BEAUTY PIPELINE & DICTIONARY

*(Dokumen Arsitektur: Penggabungan Anatomi MUA Profesional & GPU Render Engine)*

### 🧠 KONSEP DASAR: DIMENSI RENDER (2D vs 3D)

Sebelum melukis, mesin wajib memahami perbedaan sifat *makeup* agar wajah tidak terlihat seperti topeng badut:

* **Sifat 2D (Flat/Datar):** Tujuannya **Menyamarkan**. Mesin mengoleskan warna merata. *Efek:* Membunuh bayangan lekuk alami tulang wajah, membuat wajah mulus sempurna tapi **datar/pesek**.
* **Sifat 3D (Volume & Elastisitas):** Tujuannya **Memahat**. Mesin memanipulasi ketiadaan cahaya (Contour) dan pantulan cahaya (Highlighter) untuk menipu mata. *Efek:* Wajah kembali memiliki kedalaman, rahang tirus, hidung menonjol maju ke depan layar. (Serta mencakup jaring *mesh* yang **elastis** melipat merespons otot wajah/kedipan).

---

### 🎨 URUTAN LUKIS MUTLAK (The Z-Index Execution)

Mesin grafis **DIWAJIBKAN** mengeksekusi 11 fitur ini dari urutan [0] hingga [11] secara berurutan. (Jika Eyeliner dilukis sebelum Foundation, warnanya akan pudar tertimpa bedak!).

#### 📷 [0] WAJAH ASLI (KAMERA) ➡️ The Reality Canvas

* *Kondisi Awal:* Lensa mentah. Masih terdapat jerawat, kulit belang, pori-pori, dan bayangan lampu ruangan asli.

⬇️ *(Masuk ke Layer 1: Meratakan Dimensi menjadi Kanvas Datar 2D)*

#### 🧽 LAYER 1: COMPLEXION & BASE (Sang Kanvas 2D)

**[1] FOUNDATION (Alas Bedak) ➡️ [Sifat: 2D Flat]**

* **Logika Engine:** *Frequency Separation Shader*. Wajah kusam dipisahkan dari tekstur pori-pori. Bedak disapu merata, lalu pori-pori kulit asli manusia ditumpuk kembali di atasnya (agar tidak seperti manekin plastik). Melubangi area mata dan bibir.
* **Katalog Tipe:**
1. *Matte Finish:* Mulus total, menyerap cahaya (bebas kilap/minyak).
2. *Dewy / Glass Skin:* Menambahkan kilauan basah tipis merata (*Specular*) ala kulit artis Korea.
3. *Sheer / Tinted:* Transparan tipis, memperlihatkan bintik hitam (*freckles*) asli pengguna.



**[2] CONCEALER (Penyamar Noda Spesifik) ➡️ [Sifat: 2D Flat]**

* **Logika Engine:** Opacity sangat tinggi, dengan tepi pinggiran blur ekstrem.
* **Target Area:** Segitiga terbalik di bawah mata (menghapus mata panda) dan area ujung garis senyum bibir.

⬇️ *(Masuk ke Layer 2: Membangun Ulang Tulang Wajah menjadi 3D)*

#### 🗿 LAYER 2: FACE SCULPTING (Pemahat Dimensi 3D)

**[3] CONTOUR / SHADING (Pemahat Tulang) ➡️ [Sifat: 3D Bayangan]**

* **Logika Engine:** `Multiply` atau `Linear Burn`. Warna gelap transparan penipu mata ("Cekungan Kedalaman Palsu").
* **Katalog Tipe:**
1. *Cheekbone Lift:* Bayangan miring 45° menukik di bawah tulang pipi (Meniruskan muka bulat/chubby).
2. *Jawline Snatcher:* Bayangan tegas di sudut rahang leher (Menyembunyikan *double chin*).
3. *Nose Reshape:* Garis rapat di batang hidung (Memancungkan) ATAU memotong di bawah ujung hidung (Memendekkan hidung panjang).



**[4] HIGHLIGHTER (Penarik Cahaya) ➡️ [Sifat: 3D Pantulan Dinamis]**

* **Logika Engine:** `Screen Mode`. Kilauan putih yang **wajib bergeser** mengikuti pantulan lampu saat kepala pengguna menoleh ke kiri/kanan.
* **Target Area:** Puncak tulang pipi dan ujung batang hidung (*button tip*). Membuat hidung mencuat menonjol keluar layar!

**[5] BLUSH ON (Perona Pipi) ➡️ [Sifat: Transisi 2.5D]**

* **Logika Engine:** *Radial Gradient* membulat dengan *fade-out* (blur) super lembut tanpa garis tepi.
* **Katalog Tipe:**
1. *Apple of the Cheek:* Bulat di depan pipi (Memperpendek wajah panjang).
2. *Draped / Lifting:* Ditarik miring menanjak ke arah pelipis (Mengangkat kontur wajah).
3. *Sun-kissed / Igari:* Menyeberang horizontal dari pipi kiri ke kanan melewati hidung (Kesan mabuk Korea).



⬇️ *(Masuk ke Layer 3: Riasan Jendela Jiwa / Mata)*
*(Area ini rentan patah. Jaring 3D harus elastis mengikuti otot kelopak mata!)*

#### 👁️ LAYER 3: EYE ENHANCEMENTS (Elastisitas & Bingkai)

**[6] EYE LENSES (Softlens) ➡️ [Sifat: 3D Bola Cembung]**

* **Logika Engine:** Overlay Mode + *Fake Catchlight* (Mesin menambahkan 1 titik cahaya putih buatan agar bola mata terlihat melotot, basah, dan cembung, tidak mati seperti patung).
* **Katalog Tipe:** Natural Ring, Circle/Dolly (Membesarkan bola mata), Exotic (Ice Blue/Emerald).

**[7] EYESHADOW (Kelopak Mata) ➡️ [Sifat: 3D Elastis]**

* **Logika Engine:** Jaring *mesh* di-ikat (Bind) ke otot *Blink/Kedip*. Saat mata berkedip, gambar riasan ini **harus ikut melipat/menyusut** ke bawah.
* **Katalog Tipe:** Smokey Eye (Dramatis gelap), Halo Eye (Tengahnya bersinar/glitter), Aegyo Sal (Shimmer bersinar DI BAWAH kantung mata Korea).

**[8] EYELINER (Garis Mata) ➡️ [Sifat: 2D Vector]**

* **Logika Engine:** *WAJIB dirender DI ATAS Eyeshadow. Jika terbalik, warna hitam tajamnya akan buram tertutup serbuk Eyeshadow.*
* **Katalog Tipe:** Cat Eye (Sayap menukik naik), Puppy Eye (Sayap turun), Smudged Liner (Grunge blur).

**[9] EYEBROWS (Alis Mata) ➡️ [Sifat: 2D Tekstur Helaian]**

* **Logika Engine:** *Masking* dengan tekstur helaian rambut/arsiran. (Haram memblok padat warna seperti spidol!).
* **Katalog Tipe:** Korean Straight (Lurus datar), Western High-Arch (Menukik tinggi seksi), Feathery (Helai disisir naik berantakan).

⬇️ *(Masuk ke Layer 4: Sentuhan Akhir Terluar)*
*(Harus dilukis paling terakhir agar efek pantulan airnya tidak mati tertimpa dempul Layer 1).*

#### 👄 LAYER 4: OUTER SHELL (PBR Mutlak)

**[10] LIPSTICK (Bibir) ➡️ [Sifat: 3D PBR Ekstrem]**

* **Logika Engine:** Diikat ke otot bibir (senyum/bicara).
* **Katalog Tipe:**
1. *Matte:* Mode `Multiply`. Warna meresap ke dalam garis kerutan bibir asli (tidak menutupi tekstur bibir).
2. *Glossy / Glass Lip:* Mesin menciptakan titik silau putih (*Specular*) yang **meluncur dinamis** melintasi bibir saat wajah bergoyang.
3. *Ombre:* Merah pekat di bagian dalam, memudar pucat/concealer ke luar bibir.
4. *Overlined:* Dilukis sengaja melewati batas luar bibir asli 1-2 mm (Ilusi *filler* bibir monyong).



**[11] EYELASHES (Bulu Mata Palsu) ➡️ [Sifat: 3D Tonjolan Ekstrusi]**

* **Logika Engine:** Layer absolut paling luar di area mata. Helaian tebal ini harus menonjol dan menjuntai menutupi kelopak mata, eyeliner, dan eyeshadow di belakangnya secara natural.
* **Katalog Tipe:** Natural Wispy, Doll Eye (Tebal tengah), Foxy Eye (Lebat menyamping).

---

---