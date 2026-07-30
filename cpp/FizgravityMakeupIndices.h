#ifndef FIZGRAVITY_MAKEUP_INDICES_H
#define FIZGRAVITY_MAKEUP_INDICES_H

// These are the MediaPipe canonical face mesh indices for specific makeup regions.

static const unsigned short LIP_INDICES[] = {
    0, 11, 12, 13, 14, 15, 16, 17, 37, 38, 39, 40, 41, 42, 61, 62, 72, 73, 74, 76, 77, 78, 80, 81, 82, 84, 85, 86, 87, 88, 89, 90, 91, 95, 96, 146, 178, 179, 180, 181, 183, 184, 185, 191, 267, 268, 269, 270, 271, 272, 291, 292, 302, 303, 304, 306, 307, 308, 310, 311, 312, 314, 315, 316, 317, 318, 319, 320, 321, 324, 325, 375, 402, 403, 404, 405, 407, 408, 409, 415
};

// Dedicated lip-only contour rings, ported from Fizgravity-AR-Engine's
// src/makeup_triangulator.rs (get_upper_lip_triangles/get_lower_lip_triangles +
// calculate_lip_feathering — that Rust code already existed but was never wired into
// this renderer). Used to build a lip-only triangle list for baking the lip mask,
// instead of drawing the full-face MESH_INDICES triangulation with per-vertex 0/1 alpha
// — the full-mesh approach's transition width was incidental to however large the
// triangles bordering the lip happened to be (confirmed on-device: a saturated test
// lipstick color bled visibly onto the mustache/chin/cheek skin). A triangulation
// bounded to just the outer->inner lip band can't reach a vertex outside that band at
// all, which fixes the bleeding at the geometry level rather than papering over it with
// a sharper shader-side threshold.
static const unsigned short UPPER_LIP_OUTER[] = {61, 185, 40, 39, 37, 0, 267, 269, 270, 409, 291};
static const unsigned short UPPER_LIP_INNER[] = {78, 191, 80, 81, 82, 13, 312, 311, 310, 415, 308};
static const unsigned short LOWER_LIP_OUTER[] = {61, 146, 91, 181, 84, 17, 314, 405, 321, 375, 291};
static const unsigned short LOWER_LIP_INNER[] = {78, 95, 88, 178, 87, 14, 317, 402, 318, 324, 308};

static const unsigned short EYE_INDICES[] = {
    // Left eye + eyeshadow region + inner eyeball
    33, 246, 161, 160, 159, 158, 157, 173, 133, 155, 112, 26, 22, 23, 24, 110, 25,
    130, 247, 30, 29, 27, 28, 56, 190, 243, 244, 245, 128, 121, 120, 119, 118, 117, 111, 143, 156, 144, 145, 153, 154, 7, 163,
    // Right eye + eyeshadow region + inner eyeball
    362, 398, 384, 385, 386, 387, 388, 466, 263, 382, 341, 256, 252, 253, 254, 339, 255,
    359, 467, 260, 259, 257, 258, 286, 414, 463, 464, 465, 357, 350, 349, 348, 347, 346, 340, 372, 383, 373, 374, 380, 381, 246, 390
};

static const unsigned short BLUSH_INDICES[] = {
    // Left cheek
    116, 117, 118, 119, 120, 121, 147, 213, 192, 214, 205, 50, 227, 137, 177, 215, 138, 135,
    // Right cheek
    345, 346, 347, 348, 349, 350, 376, 433, 416, 434, 425, 280, 447, 366, 401, 435, 367, 364
};

static const unsigned short CONTOUR_INDICES[] = {
    // Jawline, cheeks hollow, nose bridge
    3, 7, 21, 31, 33, 34, 35, 46, 47, 51, 52, 53, 54, 55, 56, 63, 65, 66, 67, 68, 69, 70, 71, 
    100, 101, 103, 104, 105, 107, 108, 109, 127, 128, 130, 133, 139, 143, 144, 145, 153, 154, 
    155, 156, 157, 158, 159, 160, 161, 162, 163, 173, 174, 188, 189, 190, 193, 196, 198, 217, 
    221, 222, 223, 224, 225, 226, 234, 236, 243, 244, 245, 246, 247, 248, 249, 251, 252, 253, 
    254, 255, 256, 257, 258, 259, 260, 261, 263, 264, 265, 276, 277, 281, 282, 283, 284, 285, 
    286, 293, 295, 296, 297, 298, 299, 300, 301, 329, 330, 332, 333, 334, 336, 337, 338, 339, 
    340, 341, 342, 343, 357, 359, 362, 368, 372, 373, 374, 380, 381, 382, 383, 384, 385, 386, 
    387, 388, 389, 390, 398, 399, 412, 413, 414, 417, 419, 420, 437, 441, 442, 443, 444, 445, 
    446, 454, 456, 463, 464, 465, 466, 467
};


static const unsigned short LEFT_EYE_CONTOUR_INDICES[] = {
    33, 7, 163, 144, 145, 153, 154, 155, 133, 173, 157, 158, 159, 160, 161, 246
};

static const unsigned short RIGHT_EYE_CONTOUR_INDICES[] = {
    362, 382, 381, 380, 374, 373, 390, 249, 263, 466, 388, 387, 386, 385, 384, 398
};

static const unsigned short INNER_EYE_INDICES[] = {
    // Left eye contour
    33, 7, 163, 144, 145, 153, 154, 155, 133, 173, 157, 158, 159, 160, 161, 246,
    // Right eye contour
    362, 382, 381, 380, 374, 373, 390, 249, 263, 466, 388, 387, 386, 385, 384, 398,
    // Iris and pupils (10 vertices)
    468, 469, 470, 471, 472, 473, 474, 475, 476, 477
};

static const unsigned short INNER_LIPS_INDICES[] = {
    78, 191, 80, 81, 82, 13, 312, 311, 310, 415, 308, 324, 318, 402, 317, 14, 87, 178, 88, 95
};
#define NUM_INNER_LIPS_INDICES 20

static const unsigned short LEFT_EYEBROW_INDICES[] = {
    70, 63, 105, 66, 107, 55, 65, 52, 53, 46
};

static const unsigned short RIGHT_EYEBROW_INDICES[] = {
    300, 293, 334, 296, 336, 285, 295, 282, 283, 276
};

// Nose bridge midline + flank landmarks, verified against MediaPipe's official
// FACEMESH_NOSE connections (see docs/research/mediapipe_nose_contour_landmarks_guide.md).
// Used to compute nose contour/highlight LIVE every frame from real geometry,
// replacing the old hand-baked static weight tables whose shape didn't match
// real face geometry (visible as an asymmetric shadow blob instead of two
// clean lines flanking the bridge).
static const unsigned short NOSE_BRIDGE_FLANK_LEFT[] = {
    351, 417, 285, 419, 399, 456, 360
};

static const unsigned short NOSE_BRIDGE_FLANK_RIGHT[] = {
    122, 193, 55, 196, 174, 236, 131
};

// Upper-eyelid-only eyeshadow region (lash line -> crease), verified against
// MediaPipe's FACEMESH_LEFT_EYE/FACEMESH_RIGHT_EYE connections. Unlike
// EYE_INDICES (which mixes in the lower lid and reads as a ring around the
// whole eye), these two polygons cover only the upper lid + crease patch.
static const unsigned short LEFT_EYESHADOW_REGION[] = {
    362, 398, 384, 385, 386, 387, 388, 466, 263,
    467, 341, 256, 252, 253, 254, 339, 255
};

static const unsigned short RIGHT_EYESHADOW_REGION[] = {
    33, 246, 161, 160, 159, 158, 157, 173, 133,
    247, 112, 26, 22, 23, 24, 110, 25
};

static const unsigned short FACE_OVAL_INDICES[] = {
    10, 338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288, 397, 365, 379, 378, 400, 377, 152, 148, 176, 149, 150, 136, 172, 58, 132, 93, 234, 127, 162, 21, 54, 103, 67, 109
};

// Concealer target regions, derived via get_concealer_indices.py (geometric
// bounding-box selection on canonical_face_model.obj). Nasolabial fold runs
// from the nose ala down toward (not touching) the mouth corner, staying clear
// of LIP_INDICES. Under-eye WIDENED 2026-07-30: the original single-row version
// (just 229-233) rendered as a visible LINE, not an area — confirmed on-device,
// since per-vertex alpha fades to 0 within one triangle-width of any listed
// vertex, and a single row has no second row to sustain coverage further out.
// Now 3 rows spanning lash-line down into the upper cheek; rows 1 and 3
// deliberately REUSE RIGHT/LEFT_EYESHADOW_REGION's bottom edge and BLUSH's
// cheek top edge (the narrow gap between those regions + CONTOUR left only ONE
// unclaimed vertex to work with) — intentional, controlled overlap that mirrors
// how concealer physically borders eyeshadow/blush in real application. The
// radial falloff in bakeConcealerRegion() (FizgravityRenderer.cpp) already
// tapers alpha down toward these outer rows since they sit furthest from the
// region's centroid, so it still reads as strongest at the true tear-trough.
static const unsigned short UNDER_EYE_RIGHT_INDICES[] = {
    112, 26, 22, 23, 24,
    229, 230, 231, 232, 233,
    121, 120, 119, 118
};

static const unsigned short UNDER_EYE_LEFT_INDICES[] = {
    341, 256, 252, 253, 254,
    449, 450, 451, 452, 453,
    350, 349, 348, 347
};

static const unsigned short NASOLABIAL_FOLD_RIGHT_INDICES[] = {
    98, 203, 206, 165, 92, 186
};

static const unsigned short NASOLABIAL_FOLD_LEFT_INDICES[] = {
    327, 423, 426, 391, 322, 410
};

// Facelift's "lift" zones WIDENED (2026-07-31) — the original 8-point clusters
// below were confirmed on-device to barely span ~0.4-0.9 units, sitting almost
// exactly ON the eye/mouth corner instead of extending outward toward the
// temple/ear the way a real optical-lift contour needs to (the technique IS a
// diagonal line FROM the corner TOWARD the lift direction — a tight dot at the
// corner alone can't read as that shape). User confirmed: "gak terlalu terlihat
// efeknya". Outer-eye toward the temple found only ONE unclaimed vertex (124 —
// CONTOUR already fences in almost all of that zone), so the extension
// deliberately reuses several of CONTOUR's own temple points (31/35/143/156/139)
// — same controlled-overlap approach already used for UNDER_EYE's outer rows.
// Mouth-corner toward the ear had plenty of free vertices, no reuse needed.
static const unsigned short FACELIFT_OUTER_EYE_RIGHT_INDICES[] = {
    130, 33, 25, 247, 7, 246, 113, 226, // original corner cluster
    124, 31, 35, 143, 156, 139          // extension toward temple
};

static const unsigned short FACELIFT_OUTER_EYE_LEFT_INDICES[] = {
    359, 263, 255, 467, 249, 466, 342, 446,
    353, 261, 265, 372, 383, 368
};

static const unsigned short FACELIFT_MOUTH_RIGHT_INDICES[] = {
    61, 76, 62, 78, 146, 185, 184, 77,  // original corner cluster
    57, 216, 212, 202, 210, 214, 192, 213 // extension toward ear/cheek
};

static const unsigned short FACELIFT_MOUTH_LEFT_INDICES[] = {
    291, 306, 292, 308, 375, 409, 408, 307,
    287, 436, 432, 422, 430, 434, 416, 433
};

// Facelift EXTENDED to the full 4-zone technique (2026-07-31, TAMO research pass)
// — the 2 zones above are a real but simplified subset; Charlotte Tilbury's own
// "concealer facelift" guide (the technique's most-credited originator) adds two
// more zones: a "pinch" line from the inner eye corner down the side of the nose,
// and a cheek-hollow line curving from the ear toward the mouth then angling up
// toward the nose (cheekbone accentuation).
static const unsigned short FACELIFT_PINCH_RIGHT_INDICES[] = {
    133, 122, 114, 126
};

static const unsigned short FACELIFT_PINCH_LEFT_INDICES[] = {
    362, 351, 343, 355
};

static const unsigned short FACELIFT_CHEEK_HOLLOW_RIGHT_INDICES[] = {
    207, 187, 123, 132, 93
};

static const unsigned short FACELIFT_CHEEK_HOLLOW_LEFT_INDICES[] = {
    427, 411, 352, 361, 323
};

// Green corrector's own target area (2026-07-30) — dictionary spec lists Green
// Corrector's use case as "Redness (Alar Base)" with suggested indices
// [234, 454, 98, 327], a DIFFERENT area from Dark Circles (Tear Trough). Green
// was previously wired to the SAME under-eye+nasolabial mask as Traditional/
// Peach, which is conceptually wrong (green neutralizes redness — nose/cheek
// flare, rosacea — not blue-purple dark circles) and was the root cause of user
// confusion testing it ("kok cuma dikit, baru muncul pas pindah ke Traditional").
// Of the spec's 4 suggested indices, only 98/327 (the true nose-ala tip, already
// reused as NASOLABIAL_FOLD's anchor point) sit where they should geometrically;
// 234/454 sit far out near the jaw/temple, not the nose — likely a documentation
// error, so not used here. Point set found via the same exhaustive-scan approach
// as UNDER_EYE above, centered on 98/327's actual coordinates.
static const unsigned short ALAR_BASE_RIGHT_INDICES[] = {
    98, 240, 75, 235, 59, 64, 129, 102, 36, 49
};

static const unsigned short ALAR_BASE_LEFT_INDICES[] = {
    327, 460, 305, 455, 289, 294, 358, 331, 266, 279
};

#endif
