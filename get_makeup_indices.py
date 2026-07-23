import json

lipTopIdx = [61, 185, 40, 39, 37, 0, 267, 269, 270, 409, 291]
lipBotIdx = [291, 375, 321, 405, 314, 17, 84, 181, 91, 146, 61]
innerLipTopIdx = [78, 191, 80, 81, 82, 13, 312, 311, 310, 415, 308]
innerLipBotIdx = [308, 324, 318, 402, 317, 14, 87, 178, 88, 95, 78]

leftEyeTopIdx = [33, 246, 161, 160, 159, 158, 157, 173, 133, 155, 112, 26, 22, 23, 24, 110, 25] # Added some vertices above the eye
rightEyeTopIdx = [362, 398, 384, 385, 386, 387, 388, 466, 263, 382, 341, 256, 252, 253, 254, 339, 255]

# Approximate cheek regions by visually defining a box or polygon
# Let's just use known mediapipe blush indices:
leftBlushIdx = [205, 50, 118, 119, 120, 121, 122, 227, 137, 177, 215, 138, 135, 169, 170, 140, 171, 148, 176, 149, 150, 136, 172, 58, 132, 93, 234, 127, 162, 21, 54, 103, 67, 109, 10, 338, 297, 332, 284, 251, 389, 356, 454, 323, 361, 288, 397, 365, 379, 378, 400, 377, 152]

vertices = []
with open("canonical_face_model.obj", "r") as f:
    for line in f:
        if line.startswith("v "):
            parts = line.split()
            vertices.append((float(parts[1]), float(parts[2])))

def point_in_polygon(x, y, poly):
    n = len(poly)
    inside = False
    p1x, p1y = poly[0]
    for i in range(n+1):
        p2x, p2y = poly[i % n]
        if y > min(p1y, p2y):
            if y <= max(p1y, p2y):
                if x <= max(p1x, p2x):
                    if p1y != p2y:
                        xints = (y-p1y)*(p2x-p1x)/(p2y-p1y)+p1x
                    if p1x == p2x or x <= xints:
                        inside = not inside
        p1x, p1y = p2x, p2y
    return inside

outer_lip_poly = [vertices[i] for i in lipTopIdx + lipBotIdx[1:-1]]
inner_lip_poly = [vertices[i] for i in innerLipTopIdx + innerLipBotIdx[1:-1]]

lip_indices = []
for i in range(len(vertices)):
    if point_in_polygon(vertices[i][0], vertices[i][1], outer_lip_poly):
        # Allow lips to cover inner lip too, it's easier for vertex colors to blend
        lip_indices.append(i)

left_eye_indices = leftEyeTopIdx
right_eye_indices = rightEyeTopIdx

# For blush, let's select vertices based on coordinates:
# Left cheek is around x in [-0.5, -0.2] and y in [-0.2, 0.2] 
# (canonical face model is centered at nose approx)
left_blush = []
right_blush = []
contour = []

for i, v in enumerate(vertices):
    x, y = v
    # Left blush (x < -0.15, y between -0.2 and 0.1)
    if -0.6 < x < -0.2 and -0.2 < y < 0.2:
        left_blush.append(i)
    if 0.2 < x < 0.6 and -0.2 < y < 0.2:
        right_blush.append(i)
    # Contour (jawline and cheek hollows)
    if y > 0.1 and abs(x) > 0.3:
        contour.append(i)
    # Nose contour
    if abs(x) < 0.1 and y < 0.2 and y > -0.4:
        if abs(x) > 0.05:
            contour.append(i)

print("LIPS:", lip_indices)
print("LEFT_EYE:", left_eye_indices)
print("RIGHT_EYE:", right_eye_indices)
print("LEFT_BLUSH:", left_blush)
print("RIGHT_BLUSH:", right_blush)
print("CONTOUR:", contour)
