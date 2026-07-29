"""
Generates a seamless, tileable grayscale glitter/sparkle speckle texture,
used by the lipstick "shimmer" finish (see FizgravityRenderer.cpp's PASS 3
composite shader, sGlitterTex).

Per TAMO research: real AR glitter lipstick filters (Snap Lens Studio's
Glam & Glitter plugin) composite a pre-authored sparkle texture over the
lip mesh UVs, rather than computing per-pixel procedural noise live in the
shader — six rounds of procedural noise attempts in this project each fell
short of a real reference photo's dense, non-banded coverage before
switching to this approach.

Output: grayscale PNG, multiplied against the lipstick color in-shader and
tiled (hardware GL_REPEAT) across the lip's local UV space at a high
repeat count, so this one small texture produces very fine, dense speckle
coverage without needing per-pixel math to get "right".

Seamless tiling: every speckle drawn near an edge is also drawn at its
wrapped position(s) on the opposite edge(s), so repeating the image via
GL_REPEAT shows no visible seam. Re-run this script and rebuild the app to
regenerate/tune the texture (density, speckle size mix, random seed).
"""
import numpy as np
from PIL import Image, ImageDraw
import random

random.seed(7)

SIZE = 256
SUPERSAMPLE = 4  # draw at 4x resolution, then downscale with a smoothing filter —
                 # PIL's ImageDraw.ellipse has NO anti-aliasing on its own (hard,
                 # blocky pixel edges), which read as visibly "kasar" (rough/jagged)
                 # once magnified on screen (confirmed on-device). Supersampling is
                 # the standard fix: render bigger, downsample, edges come out smooth.
OUT_PATH = "android/app/src/main/assets/glitter_texture.png"

BIG = SIZE * SUPERSAMPLE
img = Image.new("L", (BIG, BIG), color=0)
draw = ImageDraw.Draw(img)

# Coverage needs headroom for the BASE LIPSTICK COLOR to still show through
# between flecks — too dense and the glitter layer blocks/replaces the color
# instead of sitting on top of it (confirmed on-device: "warna lipstick nya
# malah terhalang bukan membaur dengan glitter"). Two size classes (fine
# grain + a few larger brighter flecks) reads more like real glitter than
# one uniform size. Targeting ~25-30% coverage, not 50%+.
NUM_SPECKLES = 350


def draw_speckle(cx, cy, r, brightness):
    draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=brightness)


for i in range(NUM_SPECKLES):
    cx = random.uniform(0, BIG)
    cy = random.uniform(0, BIG)
    if i % 6 == 0:
        r = random.uniform(5.0, 8.0) * SUPERSAMPLE   # occasional larger catch-light fleck
        brightness = random.randint(220, 255)
    else:
        r = random.uniform(1.5, 3.2) * SUPERSAMPLE   # fine grain — sized so it stays a
                                        # few screen pixels across at a LOW tile repeat
                                        # count (see uLipstickFinish==4 in the shader);
                                        # tiny sub-2px specks alias into flat noise/"TV
                                        # static" once tiled and minified instead of
                                        # reading as individual sparkle points.
        brightness = random.randint(120, 255)

    # Draw at the base position plus every wrapped offset that could bring
    # this speckle's bounding box back into frame from the opposite edge —
    # guarantees a seamless wrap regardless of how close to an edge/corner
    # the random position landed.
    offsets_x = [0, BIG] if cx - r < 0 else ([0, -BIG] if cx + r > BIG else [0])
    offsets_y = [0, BIG] if cy - r < 0 else ([0, -BIG] if cy + r > BIG else [0])

    for ox in offsets_x:
        for oy in offsets_y:
            draw_speckle(cx + ox, cy + oy, r, brightness)

img = img.resize((SIZE, SIZE), Image.LANCZOS)
img.save(OUT_PATH)
print("saved", img.size, "mode", img.mode, "->", OUT_PATH)

arr = np.array(img)
coverage = (arr > 20).mean() * 100
print(f"coverage: {coverage:.1f}%")
