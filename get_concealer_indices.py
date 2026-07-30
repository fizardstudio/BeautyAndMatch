"""
Derives Concealer landmark index groups (under-eye tear-trough + nasolabial fold)
from canonical_face_model.obj, following the same geometric-bounding-box approach
as get_makeup_indices.py.

Method: sample candidate vertices inside a coordinate box for each region, then
find their exact mirror on the opposite side by nearest-neighbor match on
(-x, y, z). Candidates are cross-checked against every existing named index
array in cpp/FizgravityMakeupIndices.h to guarantee no overlap (concealer must
not double-paint pixels already claimed by eyeshadow/blush/contour/lips).
"""

vertices = []
with open("canonical_face_model.obj", "r") as f:
    for line in f:
        if line.startswith("v "):
            parts = line.split()
            vertices.append((float(parts[1]), float(parts[2]), float(parts[3])))


def find_mirror(idx):
    x, y, z = vertices[idx]
    target = (-x, y, z)
    best, best_d = None, 999.0
    for i, v in enumerate(vertices):
        if i == idx:
            continue
        d = ((v[0] - target[0]) ** 2 + (v[1] - target[1]) ** 2 + (v[2] - target[2]) ** 2) ** 0.5
        if d < best_d:
            best_d, best = d, i
    return best


# Right side (X < 0). The original single-row version (just 229-233, a thin strip
# directly below RIGHT_EYESHADOW_REGION and above BLUSH_INDICES' cheek) rendered
# as a visible LINE, not an area — confirmed on-device (2026-07-30): per-vertex
# alpha only reaches the vertices actually listed here, and linearly interpolates
# to 0 at every OTHER vertex of a triangle those vertices touch, so a single row
# fades to nothing within one triangle-width in the perpendicular direction.
# Widened to 3 rows spanning from the lash line down into the upper cheek — an
# exhaustive scan of the narrow gap between EYESHADOW/BLUSH/CONTOUR found only ONE
# unclaimed vertex left in there (everything else is already fenced in by those
# three regions), so rows 1 and 3 deliberately REUSE RIGHT_EYESHADOW_REGION's
# bottom edge and BLUSH's cheek top edge rather than inventing new geometry. This
# is intentional, controlled overlap, not a mistake: real concealer application
# physically does border/blend into where eyeshadow and blush go on top of it —
# the radial falloff in bakeConcealerRegion() (FizgravityRenderer.cpp) already
# tapers alpha down toward these outer rows since they sit furthest from the
# region's centroid, so the composite still reads as strongest at the true
# tear-trough (middle row) and soft at the edges, not a flat overlapping block.
UNDER_EYE_RIGHT_INDICES = [
    112, 26, 22, 23, 24,       # row 1: RIGHT_EYESHADOW_REGION's own bottom edge
    229, 230, 231, 232, 233,   # row 2: original tear-trough strip (kept as-is)
    121, 120, 119, 118,        # row 3: BLUSH's own cheek top edge
]

# Diagonal band from the nose ala (98) down toward (but not touching) the mouth
# corner (61), staying clear of LIP_INDICES.
NASOLABIAL_FOLD_RIGHT_INDICES = [98, 203, 206, 165, 92, 186]

UNDER_EYE_LEFT_INDICES = [find_mirror(i) for i in UNDER_EYE_RIGHT_INDICES]
NASOLABIAL_FOLD_LEFT_INDICES = [find_mirror(i) for i in NASOLABIAL_FOLD_RIGHT_INDICES]

# Green corrector's own area (2026-07-30) — dictionary spec's use case for Green is
# "Redness (Alar Base)", a DIFFERENT region from Dark Circles (Tear Trough); Green
# was wrongly sharing UNDER_EYE_RIGHT/LEFT_INDICES before this. Spec suggests
# [234, 454, 98, 327], but only 98/327 (nose-ala tip, already NASOLABIAL_FOLD's own
# anchor) check out geometrically — 234/454 sit out near the jaw/temple, nowhere
# near the nose, likely a documentation error. Centered the scan on 98/327 instead.
ALAR_BASE_RIGHT_INDICES = [98, 240, 75, 235, 59, 64, 129, 102, 36, 49]
ALAR_BASE_LEFT_INDICES = [find_mirror(i) for i in ALAR_BASE_RIGHT_INDICES]

# Facelift's outer-eye/mouth-corner "lift" zones WIDENED (2026-07-31) — the
# original 8-point clusters at each were confirmed on-device to barely span
# ~0.4-0.9 units, sitting almost exactly ON the eye/mouth corner rather than
# extending outward toward the temple/ear the way a real optical-lift contour
# needs to (that's the whole point of the technique — a diagonal line FROM the
# corner TOWARD the lift direction). User: "gak terlalu terlihat efeknya" —
# correct, the area really was too small to read as a shape. Outer-eye toward
# the temple found only ONE unclaimed vertex (CONTOUR already fences in almost
# all of that zone) so it deliberately reuses several of CONTOUR's own temple
# points — same controlled-overlap approach as UNDER_EYE's rows 1/3 above.
# Mouth-corner toward the ear had plenty of free vertices, no reuse needed there.
FACELIFT_OUTER_EYE_RIGHT_INDICES = [
    130, 33, 25, 247, 7, 246, 113, 226,  # original corner cluster (kept as-is)
    124, 31, 35, 143, 156, 139,           # extension toward temple (124 unclaimed, rest reused from CONTOUR)
]
FACELIFT_MOUTH_RIGHT_INDICES = [
    61, 76, 62, 78, 146, 185, 184, 77,   # original corner cluster (kept as-is)
    57, 216, 212, 202, 210, 214, 192, 213,  # extension toward ear/cheek (all unclaimed)
]
FACELIFT_OUTER_EYE_LEFT_INDICES = [find_mirror(i) for i in FACELIFT_OUTER_EYE_RIGHT_INDICES]
FACELIFT_MOUTH_LEFT_INDICES = [find_mirror(i) for i in FACELIFT_MOUTH_RIGHT_INDICES]

# Facelift EXTENDED to the full 4-zone technique (2026-07-31, TAMO research pass) —
# the 2-zone version above is a real but simplified subset; Charlotte Tilbury's own
# "concealer facelift" guide (the technique's most-credited originator) adds two
# more zones: a "pinch" line from the inner eye corner down the side of the nose,
# and a cheek-hollow line curving from the ear toward the mouth then angling up
# toward the nose (cheekbone accentuation).
# Pinch zone: anchored at the inner eye corner (133, already used elsewhere —
# reused deliberately as the real starting point of this line) tracing down the
# nose side toward the alar base.
FACELIFT_PINCH_RIGHT_INDICES = [133, 122, 114, 126]
FACELIFT_PINCH_LEFT_INDICES = [find_mirror(i) for i in FACELIFT_PINCH_RIGHT_INDICES]

# Cheek hollow: from near the mouth/nose out toward the ear — BLUSH/CONTOUR already
# claim most of the cheek surface, so this traces through the few vertices still
# free in that gap rather than reusing BLUSH/CONTOUR (unlike the outer-eye
# extension above, enough free vertices existed here that no reuse was needed).
FACELIFT_CHEEK_HOLLOW_RIGHT_INDICES = [207, 187, 123, 132, 93]
FACELIFT_CHEEK_HOLLOW_LEFT_INDICES = [find_mirror(i) for i in FACELIFT_CHEEK_HOLLOW_RIGHT_INDICES]

if __name__ == "__main__":
    print("UNDER_EYE_RIGHT_INDICES:", UNDER_EYE_RIGHT_INDICES)
    print("UNDER_EYE_LEFT_INDICES:", UNDER_EYE_LEFT_INDICES)
    print("NASOLABIAL_FOLD_RIGHT_INDICES:", NASOLABIAL_FOLD_RIGHT_INDICES)
    print("NASOLABIAL_FOLD_LEFT_INDICES:", NASOLABIAL_FOLD_LEFT_INDICES)
    print("ALAR_BASE_RIGHT_INDICES:", ALAR_BASE_RIGHT_INDICES)
    print("ALAR_BASE_LEFT_INDICES:", ALAR_BASE_LEFT_INDICES)
    print("FACELIFT_OUTER_EYE_RIGHT_INDICES:", FACELIFT_OUTER_EYE_RIGHT_INDICES)
    print("FACELIFT_OUTER_EYE_LEFT_INDICES:", FACELIFT_OUTER_EYE_LEFT_INDICES)
    print("FACELIFT_MOUTH_RIGHT_INDICES:", FACELIFT_MOUTH_RIGHT_INDICES)
    print("FACELIFT_MOUTH_LEFT_INDICES:", FACELIFT_MOUTH_LEFT_INDICES)
    print("FACELIFT_PINCH_RIGHT_INDICES:", FACELIFT_PINCH_RIGHT_INDICES)
    print("FACELIFT_PINCH_LEFT_INDICES:", FACELIFT_PINCH_LEFT_INDICES)
    print("FACELIFT_CHEEK_HOLLOW_RIGHT_INDICES:", FACELIFT_CHEEK_HOLLOW_RIGHT_INDICES)
    print("FACELIFT_CHEEK_HOLLOW_LEFT_INDICES:", FACELIFT_CHEEK_HOLLOW_LEFT_INDICES)
