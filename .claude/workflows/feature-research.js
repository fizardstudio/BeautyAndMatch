export const meta = {
  name: 'feature-research',
  description: 'Riset paralel via Gemini, sintesis jadi 1 brief, lalu Haiku 4.5 menulis kode berdasarkan brief itu — hasil akhirnya direview mandor (Claude host) sebelum dianggap selesai',
  phases: [
    { title: 'Research', detail: '4 sudut riset paralel via Gemini (web-researcher)' },
    { title: 'Synthesize', detail: 'Gabungkan jadi 1 brief ringkas sesuai format handoff CLAUDE.md' },
    { title: 'Implement', detail: 'Tulis kode berdasarkan brief riset', model: 'haiku' },
  ],
}

if (!args || !args.feature) {
  throw new Error('Workflow feature-research butuh args.feature: deskripsi fitur/masalah yang mau diriset')
}

const feature = args.feature
const context = 'Project: MatchAndBeauty, AR Filter Engine "Fizgravity". Stack: React Native 0.86 + TS, Zustand, Skia, VisionCamera; Kotlin + JNI bridge; MediaPipe Tasks Vision (FaceLandmarker, 468 landmarks + 52 blendshapes); C++17 + OpenGL ES 2.0/3.0 (FBO multi-pass, GLSL shaders).'

const ANGLES = [
  { key: 'teknik', prompt: 'Teknik/algoritma implementasi terbaik untuk fitur ini (shader/GLSL, blend mode, mask baking, atau pendekatan render yang relevan).' },
  { key: 'versi', prompt: 'Versi terkini & breaking changes library yang relevan (MediaPipe Tasks Vision, CameraX, react-native-vision-camera, atau library lain yang relevan dengan fitur ini).' },
  { key: 'performa', prompt: 'Pertimbangan performa: benchmark OpenGL ES di device Android low/mid-end, potensi bottleneck, cara optimasi.' },
  { key: 'alternatif', prompt: 'Alternatif pendekatan/library jika ada isu kompatibilitas, atau cara yang lebih simpel/robust dibanding pendekatan default.' },
]

phase('Research')
log(`Riset 4 sudut paralel untuk: "${feature}"`)

const findings = await parallel(ANGLES.map((a) => () =>
  agent(
    `${context}\n\nFitur/masalah yang mau diriset: ${feature}\n\nFokus riset kamu: ${a.prompt}\n\nIni HANYA riset (read-only via Gemini) — jangan tulis/ubah kode apapun.`,
    { agentType: 'web-researcher', phase: 'Research', label: `research:${a.key}` }
  ).then((text) => ({ key: a.key, text }))
))

const validFindings = findings.filter(Boolean)
if (validFindings.length === 0) {
  return { feature, brief: 'Riset gagal — semua agent Gemini error atau di-skip. Coba ulangi.', raw: [] }
}

phase('Synthesize')
const rawCombined = validFindings.map((f) => `## Sudut: ${f.key}\n${f.text}`).join('\n\n')

const brief = await agent(
  `Kamu menerima ${validFindings.length} hasil riset terpisah (dari Gemini) tentang fitur berikut di project AR Filter Engine "Fizgravity":\n\nFitur: ${feature}\n\n${rawCombined}\n\nGabungkan SEMUA hasil riset di atas jadi SATU brief ringkas mengikuti format handoff wajib:\n1. Ringkasan MAKSIMAL 8 bullet total (bukan per sudut riset — total keseluruhan)\n2. Rekomendasi konkret (bukan opsi mengambang) untuk cara implementasi\n3. Sumber (link) yang relevan, dikutip dari hasil riset di atas\n\nJangan tempel isi mentah, selalu parafrase. Jangan tulis kode apapun — ini hanya brief untuk keputusan implementasi yang akan dieksekusi Claude di percakapan utama.`,
  { phase: 'Synthesize', label: 'synthesize', effort: 'low' }
)

log('Brief riset siap — lanjut ke implementasi.')

phase('Implement')
const implementation = await agent(
  `Kamu adalah coding agent untuk project AR Filter Engine "Fizgravity" (MatchAndBeauty).\n\n${context}\n\nTugas: implementasikan fitur berikut ke codebase, berdasarkan brief riset di bawah.\n\nFitur/masalah: ${feature}\n\nBrief riset (sudah disintesis dari 4 sudut):\n${brief}\n\nInstruksi WAJIB:\n- Baca dulu file-file relevan (jangan asumsi struktur/nama simbol tanpa cek).\n- Ikuti konvensi kode yang sudah ada di project (penamaan, struktur shader di src/shaders/, JNI di cpp/, state Zustand di src/store/, dsb).\n- Jangan nambah dependency baru tanpa alasan kuat, dan jangan bikin dokumentasi/README baru.\n- Jangan lakukan git commit — cukup ubah file di working tree, commit adalah keputusan manusia.\n- Setelah selesai, laporkan (sebagai teks, bukan kode mentah semua): daftar file yang diubah/ditambah, ringkasan perubahan per file, dan risiko/asumsi yang perlu di-review manusia.`,
  { phase: 'Implement', label: 'implement', model: 'haiku' }
)

log('Implementasi oleh Haiku 4.5 selesai — hasil ini BELUM final, wajib direview mandor (Claude host) via git diff sebelum dilaporkan ke user.')

return { feature, brief, implementation, raw: validFindings }
