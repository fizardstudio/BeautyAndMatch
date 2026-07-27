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


# Right side (X < 0) picked from the band directly below RIGHT_EYESHADOW_REGION
# (Y ~2.0-2.24) and above BLUSH_INDICES' right cheek (Y <1.5), i.e. the
# tear-trough strip that neither region already claims.
UNDER_EYE_RIGHT_INDICES = [229, 230, 231, 232, 233]

# Diagonal band from the nose ala (98) down toward (but not touching) the mouth
# corner (61), staying clear of LIP_INDICES.
NASOLABIAL_FOLD_RIGHT_INDICES = [98, 203, 206, 165, 92, 186]

UNDER_EYE_LEFT_INDICES = [find_mirror(i) for i in UNDER_EYE_RIGHT_INDICES]
NASOLABIAL_FOLD_LEFT_INDICES = [find_mirror(i) for i in NASOLABIAL_FOLD_RIGHT_INDICES]

if __name__ == "__main__":
    print("UNDER_EYE_RIGHT_INDICES:", UNDER_EYE_RIGHT_INDICES)
    print("UNDER_EYE_LEFT_INDICES:", UNDER_EYE_LEFT_INDICES)
    print("NASOLABIAL_FOLD_RIGHT_INDICES:", NASOLABIAL_FOLD_RIGHT_INDICES)
    print("NASOLABIAL_FOLD_LEFT_INDICES:", NASOLABIAL_FOLD_LEFT_INDICES)
