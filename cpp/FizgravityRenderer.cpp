#include <jni.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <vector>
#include <cmath>
#include <string>
#include "FizgravityMeshIndices.h"
#include "FizgravityMakeupIndices.h"
#include "FizgravityMakeupWeights.h"
#include "FizgravityUVs.h"

#define LOG_TAG "FizgravityRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// --- SHADERS ---

static const char* CAMERA_VERTEX_SHADER = R"(
    attribute vec4 aPosition;
    attribute vec4 aTexCoord;
    uniform vec2 uScale;
    varying vec2 vTexCoord;
    void main() {
        gl_Position = vec4(aPosition.x * uScale.x, aPosition.y * uScale.y, aPosition.z, aPosition.w);
        vTexCoord = aTexCoord.xy;
    }
)";

static const char* CAMERA_FRAGMENT_SHADER = R"(
    precision mediump float;
    varying vec2 vTexCoord;
    uniform sampler2D sTexture;
    void main() {
        gl_FragColor = texture2D(sTexture, vTexCoord);
    }
)";

// --- MAKEUP MESH SHADER (Blush, Contour, Highlight) ---
// Renders a makeup layer directly onto the 3D face mesh.
// Uses per-vertex alpha weights from FizgravityMakeupWeights.h.
// Blend mode is set per-layer: GL_FUNC_ADD for Blush (alpha blend), GL_FUNC_ADD with premult for Contour (multiply).
static const char* MAKEUP_WEIGHT_VERTEX_SHADER = R"(
    attribute vec3 aPosition;
    attribute float aAlpha;
    uniform vec2 uScale;
    uniform vec2 uOffset;
    varying float vAlpha;
    void main() {
        float x = (aPosition.x * 2.0 - 1.0) * uScale.x + uOffset.x;
        float y = (1.0 - aPosition.y * 2.0) * uScale.y + uOffset.y;
        float z = aPosition.z * uScale.x * 2.0;
        gl_Position = vec4(x, y, z, 1.0);
        vAlpha = aAlpha;
    }
)";

static const char* MAKEUP_WEIGHT_FRAGMENT_SHADER = R"(
    precision mediump float;
    varying float vAlpha;
    void main() {
        // Output raw weight, let CPU glColorMask route it to R,G,B, or A
        gl_FragColor = vec4(vAlpha);
    }
)";

static const char* MASK_VERTEX_SHADER = R"(
    attribute vec3 aPosition;
    uniform vec2 uScale;
    uniform vec2 uOffset;
    uniform vec4 uFaceBounds;
    varying vec2 vUV;
    void main() {
        float x = (aPosition.x * 2.0 - 1.0) * uScale.x + uOffset.x;
        float y = (1.0 - aPosition.y * 2.0) * uScale.y + uOffset.y;
        float z = aPosition.z * uScale.x * 2.0;
        gl_Position = vec4(x, y, z, 1.0);
        vUV = vec2(
            (aPosition.x - uFaceBounds.x) / uFaceBounds.z * 0.5 + 0.5,
            (aPosition.y - uFaceBounds.y) / uFaceBounds.w * 0.5 + 0.5
        );
    }
)";

static const char* MASK_FRAGMENT_SHADER = R"(
    precision mediump float;
    varying vec2 vUV;
    uniform int uIsHole;
    void main() {
        if (uIsHole != 0) {
            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
        float r = length((vUV - 0.5) * 2.0);
        float radialFade = mix(1.0, 0.4, smoothstep(0.0, 1.0, r));
        gl_FragColor = vec4(radialFade, 0.0, 0.0, 1.0);
    }
)";

static const char* BLUR_VERTEX_SHADER = R"(
    attribute vec4 aPosition;
    attribute vec2 aTexCoord;
    varying vec2 vTexCoord;
    void main() {
        gl_Position = aPosition;
        vTexCoord = aTexCoord;
    }
)";

static const char* BLUR_FRAGMENT_SHADER = R"(
    precision mediump float;
    varying vec2 vTexCoord;
    uniform sampler2D sTexture;
    uniform vec2 uTexelSize;
    void main() {
        vec4 result = vec4(0.0);
        float radius = 4.0; // Wide blur to soften 3D mesh edges (prevents floating look)
        result += texture2D(sTexture, vTexCoord + vec2(-uTexelSize.x, -uTexelSize.y) * radius) * 0.0625;
        result += texture2D(sTexture, vTexCoord + vec2(0.0, -uTexelSize.y) * radius) * 0.125;
        result += texture2D(sTexture, vTexCoord + vec2(uTexelSize.x, -uTexelSize.y) * radius) * 0.0625;
        result += texture2D(sTexture, vTexCoord + vec2(-uTexelSize.x, 0.0) * radius) * 0.125;
        result += texture2D(sTexture, vTexCoord) * 0.25;
        result += texture2D(sTexture, vTexCoord + vec2(uTexelSize.x, 0.0) * radius) * 0.125;
        result += texture2D(sTexture, vTexCoord + vec2(-uTexelSize.x, uTexelSize.y) * radius) * 0.0625;
        result += texture2D(sTexture, vTexCoord + vec2(0.0, uTexelSize.y) * radius) * 0.125;
        result += texture2D(sTexture, vTexCoord + vec2(uTexelSize.x, uTexelSize.y) * radius) * 0.0625;
        
        gl_FragColor = result;
    }
)";

static const char* FOUNDATION_VERTEX_SHADER = R"(
    attribute vec4 aPosition;
    attribute vec2 aTexCoord;
    uniform vec2 uScale;
    uniform vec2 uOffset;
    uniform vec4 uFaceBounds;
    varying vec2 vTexCoord;
    varying vec2 vFaceUV;
    void main() {
        gl_Position = aPosition;
        vTexCoord = aTexCoord;
        
        // aPosition is NDC (-1 to +1)
        float x_ndc = aPosition.x;
        float y_ndc = aPosition.y;
        
        // Un-transform NDC back to MediaPipe 0-1 coordinate space
        float aPosX = ((x_ndc - uOffset.x) / uScale.x + 1.0) * 0.5;
        // y_ndc is flipped relative to MediaPipe
        float aPosY = 0.5 - ((y_ndc - uOffset.y) / uScale.y) * 0.5;
        
        // Compute Procedural UV mapping per-vertex for smooth interpolation
        vFaceUV = vec2(
            (aPosX - uFaceBounds.x) / uFaceBounds.z * 0.5 + 0.5,
            (aPosY - uFaceBounds.y) / uFaceBounds.w * 0.5 + 0.5
        );
    }
)";

// NOTE (2026-07-28): this shader used to also sample a "sAuxMaskTex" (dynamic
// ambient occlusion + hairline blend, baked from Fizgravity-AR-Engine's
// fizgravity_engine_calculate_dynamic_ao/_hairline_blending) that multiplied
// into foundationMask and currentSkin unconditionally, regardless of any
// user makeup selection. Removed after research showed no top-tier AR beauty
// app treats facial AO as a standalone always-on effect outside user control
// — it's always a sub-behavior of Contour (matches this app's own holy-grail
// spec: hairline shading is a Contour technique, not its own layer) or driven
// by real-time lighting estimation, and unconditional nasolabial/eye-corner
// darkening reads as a tired/aged-face anti-pattern. The Rust FFI functions
// are still correct and present (unused) for reuse when Contour's face-shape
// hairline technique or Fase 1 (Lighting Estimation) gets built — see
// FIZGRAVITY_ROADMAP.md 0.2 and FizgravityARView.kt's fizgravityCalculateAO/
// HairlineBlending declarations.
static const char* COMPOSITING_FRAGMENT_SHADER = R"(
    precision mediump float;
    varying vec2 vTexCoord;
    varying vec2 vFaceUV;
    uniform sampler2D sCameraTex;
    uniform sampler2D sMaskTex;
    uniform vec2 uTexelSize;
    uniform float uFoundationBlurRadius;

    uniform sampler2D sLipMaskTex;
    uniform sampler2D sEyeMaskTex;
    uniform sampler2D sConcealerMaskTex;

    // UI Colors
    uniform vec4 uFoundationColor;
    uniform int uFoundationType;
    uniform int uConcealerStyle;
    uniform vec4 uContourColor;
    uniform vec4 uBlushColor;
    uniform vec4 uHighlightColor;
    uniform vec4 uEyeshadowColor;
    uniform vec4 uLipstickColor;
    uniform vec4 uConcealerColor;
    uniform int uLipstickFinish; // 0=matte, 1=satin, 2=glossy, 3=sheer, 4=shimmer
    uniform float uLipstickGlossiness;

    // Ambient lighting estimation (from face-as-diffuse-light-probe analysis)
    uniform float uAmbientCCT;       // Kelvin
    uniform float uAmbientIntensity; // 0.0 - 1.0-ish

    // Before/after split-screen divider position: 0.0 = divider at left edge (all raw),
    // 1.0 = divider at right edge (all makeup). See usage below for the split logic.
    uniform float uShowMakeup;

    vec3 computeBlur(sampler2D tex, vec2 uv, vec2 texel, float maxRadius) {
        vec3 result = vec3(0.0);
        
        // Unrolled 9-tap Gaussian approximation to prevent mobile GPU shader crashes
        vec2 d = texel * (maxRadius * 0.5);
        
        // Center weight: 0.204164
        result += texture2D(tex, uv).rgb * 0.204164;
        
        // Inner Ring weights: 0.123832
        result += texture2D(tex, uv + vec2(-d.x, -d.y)).rgb * 0.123832;
        result += texture2D(tex, uv + vec2( d.x, -d.y)).rgb * 0.123832;
        result += texture2D(tex, uv + vec2(-d.x,  d.y)).rgb * 0.123832;
        result += texture2D(tex, uv + vec2( d.x,  d.y)).rgb * 0.123832;
        
        // Outer Cross weights: 0.075114
        result += texture2D(tex, uv + vec2(-d.x*2.0, 0.0)).rgb * 0.075114;
        result += texture2D(tex, uv + vec2( d.x*2.0, 0.0)).rgb * 0.075114;
        result += texture2D(tex, uv + vec2(0.0, -d.y*2.0)).rgb * 0.075114;
        result += texture2D(tex, uv + vec2(0.0,  d.y*2.0)).rgb * 0.075114;
        
        return result;
    }

    float blendSoftLightChannel(float base, float blend) {
        return (blend <= 0.5)
            ? base - (1.0 - 2.0 * blend) * base * (1.0 - base)
            : base + (2.0 * blend - 1.0) * (sqrt(base) - base);
    }
    vec3 blendSoftLight(vec3 base, vec3 blend) {
        return vec3(blendSoftLightChannel(base.r, blend.r),
                    blendSoftLightChannel(base.g, blend.g),
                    blendSoftLightChannel(base.b, blend.b));
    }
    vec3 blendScreen(vec3 base, vec3 blend) {
        return 1.0 - (1.0 - base) * (1.0 - blend);
    }
    float blendOverlayChannel(float base, float blend) {
        return (base < 0.5) ? (2.0 * base * blend) : (1.0 - 2.0 * (1.0 - base) * (1.0 - blend));
    }
    vec3 blendOverlay(vec3 base, vec3 blend) {
        return vec3(blendOverlayChannel(base.r, blend.r),
                    blendOverlayChannel(base.g, blend.g),
                    blendOverlayChannel(base.b, blend.b));
    }

    vec3 cctToTint(float cctKelvin) {
        // Simple, robust warm<->cool tint anchored at neutral daylight (6500K) = white (1,1,1).
        // Below 6500K (warmer/incandescent-ish): shift toward orange. Above (cooler/shade/overcast): shift toward blue.
        float t = clamp((cctKelvin - 6500.0) / 3500.0, -1.0, 1.0);
        vec3 warmTint = vec3(1.0, 0.82, 0.65);
        vec3 coolTint = vec3(0.75, 0.85, 1.0);
        return t < 0.0 ? mix(vec3(1.0), warmTint, -t) : mix(vec3(1.0), coolTint, t);
    }

    void main() {
        vec4 origColor = texture2D(sCameraTex, vTexCoord);
        vec4 mask = texture2D(sMaskTex, vTexCoord);

        // faceMask = broad "are we anywhere on the tracked face" gate, used by every
        // layer EXCEPT foundation. foundationMask is narrower: it additionally excludes
        // the outer lip surface (via the same sharpened lip mask the LIPSTICK layer
        // uses below, no new geometry needed) so foundation color
        // never sits underneath lipstick's multiply blend — real MUA technique leaves
        // lips untouched by general face foundation (dedicated neutral lip primer is a
        // separate, deliberately-restrained product, not the user's arbitrary foundation
        // shade at full strength), and multiply is the least forgiving blend mode for a
        // non-neutral color like this app's user-picked foundation to sit underneath.
        float faceMask = mask.r;

        // lipRawMask now comes from a dedicated, tightly-bounded lip-only triangulation
        // (Catmull-Rom-smoothed outer/inner contours — see PASS 1c) with its own
        // deliberately-authored feather (0.15 at the outer/vermilion edge, 1.0 at the
        // inner edge), not the old full-face-mesh bake whose transition width was
        // incidental to triangle size. Use it directly — an earlier smoothstep(0.4,0.6)
        // sharpening pass here was a workaround for THAT old wide/incidental bleed, but
        // now that the geometry itself is precise, that same threshold does the wrong
        // thing: the outer contour's 0.15 baseline never clears 0.4, so it was clipping
        // real coverage off the outer edge instead of fixing a bleed that no longer
        // exists (confirmed on-device: rendered lipstick fell visibly short of the true
        // vermilion border after the geometry fix, until this was removed).
        // Raw baked mask (0=outer/vermilion edge .. 1=inner edge), kept UN-remapped here —
        // foundationMask below excludes the lip AREA from foundation regardless of which
        // lipstick finish is chosen (a real MUA keeps foundation off lips no matter what
        // finish goes on top), so it must not shrink/grow with the finish-specific curve.
        // .g = position along the mouth's horizontal width (0..1, corner to corner) and
        // .b = lower-lip weight (0 upper / 1 lower), baked alongside .r in PASS 1c for the
        // glossy/satin highlight below — see the highlight comment for why.
        vec3 lipMaskRGB = texture2D(sLipMaskTex, vTexCoord).rgb;
        float lipMask = lipMaskRGB.r;
        float lipU = lipMaskRGB.g;
        float lipLowerWeight = lipMaskRGB.b;

        float foundationMask = faceMask * (1.0 - lipMask);
        float contourAlpha   = mask.g * faceMask;
        float blushAlpha     = mask.b * faceMask;
        float highlightAlpha = mask.a * faceMask;
        // NOT gated by faceMask: faceMask carries the PASS-1 hard hole cut for
        // INNER_LIPS_INDICES (meant to keep foundation/general skin smoothing out of an
        // OPEN mouth's cavity), which sits right along the inner lip seam — the same seam
        // that's still real lip surface when the mouth is closed. Gating lipAlpha by
        // faceMask made lipstick visibly gap open along that seam even with the mouth
        // shut (confirmed on-device). lipMask is already tightly scoped to the lip
        // landmarks on its own and doesn't need the broader face gate.
        //
        // Finish-specific alpha remap (TAMO research: matte/satin/glossy/sheer/shimmer
        // aren't the same coverage curve at different intensities — sheer's capped-low,
        // wide-feather translucency isn't reachable by "turning down" matte's curve, and
        // shimmer's per-point sparkle isn't reachable by "turning up" glossy's curve).
        // Curves are hand-tuned starting points (see conversation), not a measured
        // reference dataset — expect to keep refining these against real photos.
        float lipAlpha;
        if (uLipstickFinish == 1) { // Satin: softer ramp, still near-full coverage
            lipAlpha = smoothstep(0.10, 0.40, lipMask);
        } else if (uLipstickFinish == 2) { // Glossy: semi-transparent base, relies on highlight for fullness
            lipAlpha = smoothstep(0.10, 0.35, lipMask) * 0.6;
        } else if (uLipstickFinish == 3) { // Sheer: capped low, wide feather, natural lip shows through
            // Was smoothstep(0.30, 0.85, ...): with the raw baked mask's actual range
            // (0.15 at the true outer landmark, up to 1.0 at the inner edge — see
            // ctrlAlphaOuter above), most of the visible lip surface never reached 0.30
            // at all, so nearly the whole lip got exactly zero alpha (confirmed on-
            // device: "sheer blm ada efect apa apa"). Starting the ramp right at the
            // true outer edge instead means sheer washes color across the WHOLE lip
            // (as a sheer/tinted-balm finish should), just always capped low.
            lipAlpha = smoothstep(0.05, 0.5, lipMask) * 0.35;
        } else if (uLipstickFinish == 4) { // Shimmer: same base coverage as satin, sparkle added at composite
            lipAlpha = smoothstep(0.10, 0.35, lipMask);
        } else { // Matte (0, default): hard edge, near-opaque, no highlight
            lipAlpha = smoothstep(0.05, 0.20, lipMask);
        }
        float eyeAlpha       = texture2D(sEyeMaskTex, vTexCoord).r * faceMask;
        float concealerAlpha = texture2D(sConcealerMaskTex, vTexCoord).r * faceMask;

        if (faceMask > 0.0 || contourAlpha > 0.0 || blushAlpha > 0.0 || highlightAlpha > 0.0 || lipAlpha > 0.0 || eyeAlpha > 0.0 || concealerAlpha > 0.0) {
            // Spatial radius must be large enough (>15px) to blur out pores so they are isolated in highFreq
            float activeRadius = 16.0; 
            vec3 blurred = computeBlur(sCameraTex, vTexCoord, uTexelSize, activeRadius);
            vec3 highFreq = origColor.rgb - blurred;
            
            vec3 currentSkin = origColor.rgb;

            // Ambient-light color tint, shared by every pigmented makeup layer below —
            // computed once so foundation/concealer/contour/blush/highlight/eyeshadow/
            // lipstick all respond consistently to the same estimated lighting, not just
            // foundation. Real pigment under warm/cool light shifts the same way skin does.
            vec3 ambientTint = mix(vec3(1.0), cctToTint(uAmbientCCT), clamp(uAmbientIntensity, 0.0, 1.0));
            vec3 tintedFoundationColor = uFoundationColor.rgb * ambientTint;

            // --- PURE SMOOTHING (Independent of Foundation Color) ---
            // Slider goes from 0 to 20
            float smoothIntensity = clamp(uFoundationBlurRadius / 20.0, 0.0, 1.0);
            float hfMultiplier = mix(1.0, 0.1, smoothIntensity); // Retain only 10% pores at max smooth
            vec3 smoothedSkin = blurred + highFreq * hfMultiplier;
            currentSkin = mix(currentSkin, smoothedSkin, foundationMask);
            
            // --- FOUNDATION COLOR & FINISH ---
            float skinLuma = dot(blurred, vec3(0.299, 0.587, 0.114));
            vec3 litFoundation = tintedFoundationColor * (skinLuma * 0.85 + 0.15);

            float effectiveOpacity = clamp(uFoundationColor.a, 0.0, 1.0);
            float darkBlend = 1.0 - smoothstep(0.30, 0.38, skinLuma);
            effectiveOpacity = mix(effectiveOpacity, min(effectiveOpacity, 0.65), darkBlend);
            litFoundation += vec3(0.04, 0.015, 0.0) * darkBlend;
            
            vec3 foundationEffect = origColor.rgb;
            
            if (uFoundationType == 0) {
                vec3 coveredSkin = litFoundation;
                foundationEffect = coveredSkin + (highFreq * 0.40 * hfMultiplier);
            } else if (uFoundationType == 1) {
                vec3 coveredSkin = mix(blurred, litFoundation, 0.65);
                foundationEffect = coveredSkin + (highFreq * 0.80 * hfMultiplier);
                
                float tZoneSuppression = smoothstep(0.12, 0.38, abs(vFaceUV.x - 0.5));
                float highPointMask = smoothstep(0.35, 0.75, skinLuma) * tZoneSuppression;
                
                vec3 lightDir = normalize(vec3(0.2, 0.4, 0.8));
                vec3 viewDir = vec3(0.0, 0.0, 1.0);
                vec3 halfVector = normalize(lightDir + viewDir);
                vec3 fauxNormal = normalize(vec3((vFaceUV.x - 0.5) * 1.5, (vFaceUV.y - 0.5) * 1.5, 0.8));
                
                float specAngle = max(0.0, dot(fauxNormal, halfVector));
                float wetSpecular = pow(specAngle, 24.0) * 0.5 * highPointMask;
                foundationEffect = blendScreen(foundationEffect, vec3(wetSpecular));
            } else if (uFoundationType == 2) {
                vec3 sheerTarget = blendSoftLight(blurred, tintedFoundationColor);
                vec3 coveredSkin = mix(blurred, sheerTarget, 0.45);
                foundationEffect = coveredSkin + (highFreq * 0.95 * hfMultiplier);
            } else if (uFoundationType == 3) {
                vec3 softLightTarget = blendSoftLight(blurred, tintedFoundationColor);
                vec3 satinTarget = mix(litFoundation, softLightTarget, 0.5);
                foundationEffect = satinTarget + (highFreq * 0.70 * hfMultiplier);
            } else if (uFoundationType == 4) {
                vec3 overlayTarget = blendOverlay(blurred, tintedFoundationColor);
                vec3 luminousTarget = mix(litFoundation, overlayTarget, 0.4);
                foundationEffect = luminousTarget + (highFreq * 0.75 * hfMultiplier);

                vec3 fauxNormal = normalize(vec3((vFaceUV.x - 0.5) * 1.5, (vFaceUV.y - 0.5) * 1.5, 0.9));
                float NdV = max(0.0, dot(fauxNormal, vec3(0.0, 0.0, 1.0)));
                float pearlGlow = (1.0 - NdV * 0.6) * smoothstep(0.3, 0.8, skinLuma) * 0.22;

                vec3 adaptivePearl = mix(vec3(1.0, 0.92, 0.82), tintedFoundationColor, 0.35);
                foundationEffect = blendScreen(foundationEffect, adaptivePearl * pearlGlow);
            }
            
            // Final composite: blend the foundation color effect onto the currentSkin (which may already be smoothed)
            currentSkin = mix(currentSkin, foundationEffect, effectiveOpacity * foundationMask);

            // [2] CONCEALER
            vec3 tintedConcealerColor = uConcealerColor.rgb * ambientTint;
            float concealerStrength = concealerAlpha * uConcealerColor.a;
            if (uConcealerStyle == 2 || uConcealerStyle == 3) {
                float targetingMask = 1.0;
                if (uConcealerStyle == 2) {
                    float redness = origColor.r - (origColor.g + origColor.b * 0.5);
                    targetingMask = smoothstep(-0.05, 0.05, redness);
                }
                vec3 correctorTarget = blendSoftLight(blurred, tintedConcealerColor);
                currentSkin = mix(currentSkin, correctorTarget + highFreq, concealerStrength * targetingMask);
            } else {
                vec3 concealerLowFreq = mix(blurred, tintedConcealerColor, concealerStrength);
                currentSkin = mix(currentSkin, concealerLowFreq + highFreq, concealerStrength);
            }

            // [3] CONTOUR (Hybrid Multiply + Linear Burn)
            vec3 tintedContourColor = uContourColor.rgb * ambientTint;
            float contourAlphaFinal = contourAlpha * uContourColor.a;
            vec3 mulResult = currentSkin * tintedContourColor;
            vec3 burnResult = max(currentSkin + tintedContourColor - vec3(1.0), vec3(0.0));
            vec3 deepContour = mix(mulResult, burnResult, 0.4);
            float effectiveAlpha = pow(contourAlphaFinal, 0.85);
            currentSkin = mix(currentSkin, deepContour, effectiveAlpha);

            // [4] BLUSH (Hybrid Normal + Soft Light)
            vec3 tintedBlushColor = uBlushColor.rgb * ambientTint;
            float blushStrength = blushAlpha * uBlushColor.a;
            vec3 softBlush = blendSoftLight(currentSkin, tintedBlushColor);
            vec3 normalBlush = tintedBlushColor;
            vec3 pigmentedBlush = mix(softBlush, normalBlush, 0.65);
            float skinLum = dot(currentSkin, vec3(0.299, 0.587, 0.114));
            pigmentedBlush *= (skinLum * 0.4 + 0.8);
            currentSkin = mix(currentSkin, pigmentedBlush, blushStrength);

            // [5] HIGHLIGHTER (Screen)
            vec3 tintedHighlightColor = uHighlightColor.rgb * ambientTint;
            vec3 highlightLayer = blendScreen(currentSkin, tintedHighlightColor);
            currentSkin = mix(currentSkin, highlightLayer, highlightAlpha * uHighlightColor.a);

            // [7] EYESHADOW (Overlay / Normal hybrid). Region weight is re-baked every
            // frame from live landmarks (see Pass 1d), so blink compression/stretch
            // already happens via the mesh's own UV deformation — no blendshape
            // plumbing needed for this layer.
            vec3 tintedEyeshadowColor = uEyeshadowColor.rgb * ambientTint;
            vec3 overlayEye = blendOverlay(currentSkin, tintedEyeshadowColor);
            vec3 pigmentedEye = mix(overlayEye, tintedEyeshadowColor, 0.5);
            currentSkin = mix(currentSkin, pigmentedEye, eyeAlpha * uEyeshadowColor.a);

            // [10] LIPSTICK — base tint (multiply, real MUA-accurate technique: color
            // modulates the underlying lip's own shading/texture instead of flatly
            // replacing it) is shared by all finishes; glossy/satin/shimmer add a
            // highlight layer on top (TAMO research: production AR makeup apps use a
            // separate highlight/shine pass rather than folding shine into the base
            // blend — a single blend mode can't represent both a flat matte color and a
            // glossy specular sheen).
            vec3 tintedLipstickColor = uLipstickColor.rgb * ambientTint;
            vec3 lipMultiply = currentSkin * tintedLipstickColor;
            currentSkin = mix(currentSkin, lipMultiply, lipAlpha * uLipstickColor.a);

            // Elliptical highlight, localized (not a proxy derived from lipMask alone).
            // TAMO research: real AR beauty apps (Snap's Makeup Template, ModiFace-family
            // patents) position gloss shine as a pre-authored, spatially-localized spot —
            // never purely from a 1D "distance across the lip band" value. An earlier
            // version drove the highlight only from lipMask (peaking at lipMask==0.5):
            // since that value doesn't vary along the mouth's horizontal width, the
            // "peak" formed a bright line running the lip's ENTIRE length instead of a
            // localized shine (confirmed on-device: "seperti garis ditengah lipstick").
            // Fixed by baking two more mask channels in PASS 1c — lipU (position along
            // the mouth's width) and lipLowerWeight (0 upper / 1 lower lip) — so the
            // highlight can be centered at one (u, mask) point with a real 2D radius,
            // gated to the lower lip where a real specular highlight usually sits.
            if (uLipstickFinish == 1) { // Satin: wide, dim, soft-edged sheen
                vec2 d = vec2(lipU - 0.5, lipMask - 0.7) / vec2(0.32, 0.35);
                float highlight = pow(clamp(1.0 - dot(d, d), 0.0, 1.0), 1.5) * lipLowerWeight;
                currentSkin += vec3(highlight * 0.35 * uLipstickGlossiness * lipAlpha);
                // Secondary highlight: smaller, dimmer, near the cupid's bow (upper lip
                // center, close to the outer/vermilion edge rather than deep inside) —
                // reference photos show a smaller catchlight there alongside the main
                // lower-lip one, not just a single spot.
                vec2 dCupid = vec2(lipU - 0.5, lipMask - 0.3) / vec2(0.12, 0.15);
                float cupidHighlight = pow(clamp(1.0 - dot(dCupid, dCupid), 0.0, 1.0), 2.0) * (1.0 - lipLowerWeight);
                currentSkin += vec3(cupidHighlight * 0.15 * uLipstickGlossiness * lipAlpha);
            } else if (uLipstickFinish == 2) { // Glossy: tight, bright, sharp-edged specular
                vec2 d = vec2(lipU - 0.5, lipMask - 0.75) / vec2(0.18, 0.22);
                float highlight = pow(clamp(1.0 - dot(d, d), 0.0, 1.0), 3.0) * lipLowerWeight;
                currentSkin += vec3(highlight * 0.7 * uLipstickGlossiness * lipAlpha);
                vec2 dCupid = vec2(lipU - 0.5, lipMask - 0.3) / vec2(0.10, 0.12);
                float cupidHighlight = pow(clamp(1.0 - dot(dCupid, dCupid), 0.0, 1.0), 3.0) * (1.0 - lipLowerWeight);
                currentSkin += vec3(cupidHighlight * 0.35 * uLipstickGlossiness * lipAlpha);
            } else if (uLipstickFinish == 4) { // Shimmer: sparse bright sparkle points, not one coherent highlight
                // Went through several rounds of procedural noise (see git history)
                // trying to get this right: screen-space noise (invisible — lip covers
                // too little of the frame), a sin()-based hash (degrades in mobile
                // mediump precision — still invisible), a weak vec2 hash mix (banded
                // into horizontal stripes), a per-cell hash+round-dot version (still
                // elongated, and biased to one side — the cheap integer-cell hash just
                // doesn't have good enough distribution at this grid size on this
                // hardware). Switched to a small FIXED constellation of sparkle points
                // instead of procedural randomness — no hash, so no hash bugs.
                //
                // Each dot needs an ANISOTROPIC radius, not a uniform one: the lip's
                // outer->inner "band" (the mask axis) is much SHORTER on screen than its
                // corner-to-corner width (the u axis), so a dot with equal radius in
                // normalized (u, mask) units renders far wider than tall on screen. First
                // attempt scaled the mask axis UP (÷ a small number) before measuring
                // distance, which is backwards — that shrinks the effective radius in the
                // shorter axis even further, producing exactly the long thin horizontal
                // streaks reported ("garis garis horizontal tipis panjang"). Correct
                // direction: give the mask axis a LARGER radius (dividing by a bigger
                // number) to compensate for its physically compressed scale.
                // 6 large dots read as polka-dots, not glitter (user: "kegedean
                // titiknya") — real shimmer is many FINE flecks. Widened to 20 points,
                // radius shrunk ~3x, scattered irregularly (not a clean grid, which
                // would look artificial/mechanical rather than like real glitter
                // particles).
                vec2 lipUV = vec2(lipU, lipMask);
                vec2 dotRadius = vec2(0.016, 0.08); // (u radius, mask radius) — mask >> u on purpose
                float sparkle = 0.0;
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.08, 0.30)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.15, 0.55)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.12, 0.75)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.22, 0.42)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.28, 0.68)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.35, 0.25)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.38, 0.58)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.45, 0.80)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.42, 0.35)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.50, 0.50)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.55, 0.28)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.58, 0.65)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.62, 0.42)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.68, 0.75)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.72, 0.30)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.78, 0.55)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.82, 0.38)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.88, 0.62)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.92, 0.45)) / dotRadius));
                sparkle += smoothstep(1.0, 0.0, length((lipUV - vec2(0.90, 0.78)) / dotRadius));
                sparkle = clamp(sparkle, 0.0, 1.0);
                currentSkin += vec3(sparkle * 0.8 * uLipstickGlossiness * lipAlpha);
            }

            // Before/after split-screen comparison: uShowMakeup is a horizontal divider
            // position (0.0 = divider at the left edge, so the whole frame is raw; 1.0 =
            // divider at the right edge, so the whole frame is full makeup). Left of the
            // divider shows composited makeup, right of it shows raw camera, with a small
            // feathered band at the divider itself instead of a razor-hard cut.
            float splitFeather = 0.01;
            float rawSide = smoothstep(uShowMakeup - splitFeather, uShowMakeup + splitFeather, vTexCoord.x);
            currentSkin = mix(currentSkin, origColor.rgb, rawSide);

            gl_FragColor = vec4(currentSkin, origColor.a);
        } else {
            gl_FragColor = origColor;
        }
    }
)";

// --- UTILS ---

GLuint loadShader(GLenum type, const char* shaderSrc) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) return 0;
    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint infoLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
        if (infoLen > 1) {
            char* infoLog = new char[infoLen];
            glGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
            LOGE("Error compiling shader: %s", infoLog);
            delete[] infoLog;
        }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint createProgram(const char* vtxSrc, const char* fragSrc) {
    GLuint vtxShader = loadShader(GL_VERTEX_SHADER, vtxSrc);
    if (!vtxShader) return 0;
    GLuint fragShader = loadShader(GL_FRAGMENT_SHADER, fragSrc);
    if (!fragShader) return 0;

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vtxShader);
        glAttachShader(program, fragShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            LOGE("Error linking program");
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

// --- FBO ---
struct FBO {
    GLuint fbo = 0;
    GLuint texture = 0;
    GLuint depthRenderbuffer = 0;
    int width = 0;
    int height = 0;

    void release() {
        if (texture) { glDeleteTextures(1, &texture); texture = 0; }
        if (depthRenderbuffer) { glDeleteRenderbuffers(1, &depthRenderbuffer); depthRenderbuffer = 0; }
        if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
        width = 0; height = 0;
    }

    void setup(int w, int h, bool needsDepth) {
        if (width == w && height == h) return;
        release();
        width = w; height = h;
        
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        if (needsDepth) {
            glGenRenderbuffers(1, &depthRenderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOGE("FBO setup failed: %d", status);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

struct RendererContext {
    GLuint cameraProgram;
    GLint camPositionHandle, camTexCoordHandle, camSamplerHandle, camScaleHandle;

    GLuint maskProgram;
    GLint maskPositionHandle, maskUVHandle, maskScaleHandle, maskOffsetHandle, maskBoundsHandle;
    GLint maskIsHoleH;

    GLuint foundationProgram;
    GLint fndPositionHandle, fndTexCoordHandle;
    GLint fndCameraTexHandle;
    GLint fndMaskTexHandle;
    GLint fndTexelSizeHandle;
    GLint fndBlurRadiusHandle;
    GLint fndColorHandle;
    GLint fndTypeHandle;
    GLint fndContourColorHandle;
    GLint fndHighlightColorHandle;
    GLint fndBlushColorHandle;
    GLint fndLipstickColorHandle;
    GLint fndLipMaskTexHandle;
    GLint fndEyeshadowColorHandle;
    GLint fndEyeMaskTexHandle;
    GLint fndConcealerColorHandle;
    GLint fndConcealerMaskTexHandle;
    GLint fndConcealerStyleHandle;
    GLint fndContourStyleHandle;
    GLint fndBlushStyleHandle;
    GLint fndScaleHandle;
    GLint fndOffsetHandle;
    GLint fndBoundsHandle;
    GLint fndAmbientCctHandle;
    GLint fndAmbientIntensityHandle;
    GLint fndShowMakeupHandle;
    GLint fndLipstickFinishHandle;
    GLint fndLipstickGlossinessHandle;

    // --- Makeup Mesh Program (Blush, Contour rendered directly on face mesh) ---
    GLuint makeupWeightProgram;
    GLint mkPositionHandle, mkAlphaHandle, mkScaleHandle, mkOffsetHandle;

    GLuint blurProgram;
    GLint blurPositionHandle, blurTexCoordHandle, blurSamplerHandle, blurTexelSizeHandle;

    int width = 1080;
    int height = 1920;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float faceCenterX = 0.5f;
    float faceCenterY = 0.5f;
    float faceRadiusX = 0.5f;
    float faceRadiusY = 0.5f;

    FBO maskFbo;
    FBO maskBlurFbo;
    FBO mainFbo;
    FBO lipMaskFbo;
    FBO eyeMaskFbo;
    FBO concealerMaskFbo;

    float foundationColor[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    float contourColor[4] = {0.3f, 0.15f, 0.1f, 0.0f};
    float highlightColor[4] = {1.0f, 0.95f, 0.85f, 0.0f};
    float blushColor[4] = {0.8f, 0.3f, 0.4f, 0.0f};
    float lipstickColor[4] = {0.6f, 0.1f, 0.15f, 0.0f};
    int lipstickFinish = 0; // 0=matte, 1=satin, 2=glossy, 3=sheer, 4=shimmer
    float lipstickGlossiness = 0.5f;
    float eyeshadowColor[4] = {0.3f, 0.2f, 0.15f, 0.0f};
    float concealerColor[4] = {0.85f, 0.65f, 0.5f, 0.0f};
    
    int foundationType = 0; // 0=matte, 1=dewy, 2=sheer
    float foundationBlurRadius = 8.0f; // texel-multiplier for frequency-separation low-pass blur
    int contourStyle = 0;   // 0=normal, 1=slim, 2=pinch, 3=straighten (drives live nose contour+highlight geometry)
    int blushStyle = 0;     // 0=normal, 1=contour_45, 2=horizontal
    int concealerStyle = 0; // 0=Traditional, 1=Facelift, 2=Green, 3=Peach

    float ambientCctKelvin = 6500.0f; // Neutral D65 daylight default — safe no-op tint until real data arrives
    float ambientIntensity = 1.0f;    // Neutral (no darkening) default

    float showMakeup = 1.0f; // Before/after slider: 0=raw camera, 1=full makeup (default)

    // Morphology cache (updated at ~5fps to avoid overhead)
    std::string lastFaceShape = "";
    std::string lastEyeShape = "";
    std::string lastNoseShape = "";
    int morphologyFrameCounter = 0;
};

static RendererContext gCtx;

static float euclidean2D(float* data, int idxA, int idxB) {
    float dx = data[idxA*3] - data[idxB*3];
    float dy = data[idxA*3+1] - data[idxB*3+1];
    return std::sqrt(dx*dx + dy*dy);
}

extern "C" {


JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeInitGL(JNIEnv* env, jclass clazz) {
    LOGI("nativeInitGL called");
    
    // Camera Shader
    gCtx.cameraProgram = createProgram(CAMERA_VERTEX_SHADER, CAMERA_FRAGMENT_SHADER);
    gCtx.camPositionHandle = glGetAttribLocation(gCtx.cameraProgram, "aPosition");
    gCtx.camTexCoordHandle = glGetAttribLocation(gCtx.cameraProgram, "aTexCoord");
    gCtx.camScaleHandle = glGetUniformLocation(gCtx.cameraProgram, "uScale");
    gCtx.camSamplerHandle = glGetUniformLocation(gCtx.cameraProgram, "sTexture");

    // Mask Shader
    gCtx.maskProgram = createProgram(MASK_VERTEX_SHADER, MASK_FRAGMENT_SHADER);
    gCtx.maskPositionHandle = glGetAttribLocation(gCtx.maskProgram, "aPosition");
    gCtx.maskScaleHandle = glGetUniformLocation(gCtx.maskProgram, "uScale");
    gCtx.maskOffsetHandle = glGetUniformLocation(gCtx.maskProgram, "uOffset");
    gCtx.maskBoundsHandle = glGetUniformLocation(gCtx.maskProgram, "uFaceBounds");
    gCtx.maskIsHoleH = glGetUniformLocation(gCtx.maskProgram, "uIsHole");

    // Makeup Mesh Shader (Blush & Contour on real 3D mesh)
    gCtx.makeupWeightProgram = createProgram(MAKEUP_WEIGHT_VERTEX_SHADER, MAKEUP_WEIGHT_FRAGMENT_SHADER);
    gCtx.mkPositionHandle = glGetAttribLocation(gCtx.makeupWeightProgram, "aPosition");
    gCtx.mkAlphaHandle = glGetAttribLocation(gCtx.makeupWeightProgram, "aAlpha");
    gCtx.mkScaleHandle = glGetUniformLocation(gCtx.makeupWeightProgram, "uScale");
    gCtx.mkOffsetHandle = glGetUniformLocation(gCtx.makeupWeightProgram, "uOffset");

    // Foundation Shader
    gCtx.foundationProgram = createProgram(FOUNDATION_VERTEX_SHADER, COMPOSITING_FRAGMENT_SHADER);
    gCtx.fndPositionHandle = glGetAttribLocation(gCtx.foundationProgram, "aPosition");
    gCtx.fndTexCoordHandle = glGetAttribLocation(gCtx.foundationProgram, "aTexCoord");
    gCtx.fndCameraTexHandle = glGetUniformLocation(gCtx.foundationProgram, "sCameraTex");
    gCtx.fndMaskTexHandle = glGetUniformLocation(gCtx.foundationProgram, "sMaskTex");
    gCtx.fndTexelSizeHandle = glGetUniformLocation(gCtx.foundationProgram, "uTexelSize");
    gCtx.fndBlurRadiusHandle = glGetUniformLocation(gCtx.foundationProgram, "uFoundationBlurRadius");
    gCtx.fndColorHandle = glGetUniformLocation(gCtx.foundationProgram, "uFoundationColor");
    gCtx.fndTypeHandle = glGetUniformLocation(gCtx.foundationProgram, "uFoundationType");
    gCtx.fndContourColorHandle = glGetUniformLocation(gCtx.foundationProgram, "uContourColor");
    gCtx.fndHighlightColorHandle = glGetUniformLocation(gCtx.foundationProgram, "uHighlightColor");
    gCtx.fndBlushColorHandle = glGetUniformLocation(gCtx.foundationProgram, "uBlushColor");
    gCtx.fndLipstickColorHandle = glGetUniformLocation(gCtx.foundationProgram, "uLipstickColor");
    gCtx.fndLipstickFinishHandle = glGetUniformLocation(gCtx.foundationProgram, "uLipstickFinish");
    gCtx.fndLipstickGlossinessHandle = glGetUniformLocation(gCtx.foundationProgram, "uLipstickGlossiness");
    gCtx.fndLipMaskTexHandle = glGetUniformLocation(gCtx.foundationProgram, "sLipMaskTex");
    gCtx.fndEyeshadowColorHandle = glGetUniformLocation(gCtx.foundationProgram, "uEyeshadowColor");
    gCtx.fndEyeMaskTexHandle = glGetUniformLocation(gCtx.foundationProgram, "sEyeMaskTex");
    gCtx.fndConcealerColorHandle = glGetUniformLocation(gCtx.foundationProgram, "uConcealerColor");
    gCtx.fndConcealerMaskTexHandle = glGetUniformLocation(gCtx.foundationProgram, "sConcealerMaskTex");
    gCtx.fndConcealerStyleHandle = glGetUniformLocation(gCtx.foundationProgram, "uConcealerStyle");
    gCtx.fndScaleHandle = glGetUniformLocation(gCtx.foundationProgram, "uScale");
    gCtx.fndOffsetHandle = glGetUniformLocation(gCtx.foundationProgram, "uOffset");
    gCtx.fndBoundsHandle = glGetUniformLocation(gCtx.foundationProgram, "uFaceBounds");
    gCtx.fndAmbientCctHandle = glGetUniformLocation(gCtx.foundationProgram, "uAmbientCCT");
    gCtx.fndAmbientIntensityHandle = glGetUniformLocation(gCtx.foundationProgram, "uAmbientIntensity");
    gCtx.fndShowMakeupHandle = glGetUniformLocation(gCtx.foundationProgram, "uShowMakeup");

    // Validation logging for foundation shader handles
    if (gCtx.fndPositionHandle == -1) LOGE("Foundation shader: attribute 'aPosition' not found");
    if (gCtx.fndTexCoordHandle == -1) LOGE("Foundation shader: attribute 'aTexCoord' not found");
    if (gCtx.fndCameraTexHandle == -1) LOGE("Foundation shader: uniform 'sCameraTex' not found");
    if (gCtx.fndMaskTexHandle == -1) LOGE("Foundation shader: uniform 'sMaskTex' not found");
    if (gCtx.fndTexelSizeHandle == -1) LOGE("Foundation shader: uniform 'uTexelSize' not found");
    if (gCtx.fndBlurRadiusHandle == -1) LOGE("Foundation shader: uniform 'uFoundationBlurRadius' not found");
    if (gCtx.fndColorHandle == -1) LOGE("Foundation shader: uniform 'uFoundationColor' not found");
    if (gCtx.fndTypeHandle == -1) LOGE("Foundation shader: uniform 'uFoundationType' not found");
    if (gCtx.fndContourColorHandle == -1) LOGE("Foundation shader: uniform 'uContourColor' not found");
    if (gCtx.fndHighlightColorHandle == -1) LOGE("Foundation shader: uniform 'uHighlightColor' not found");
    if (gCtx.fndBlushColorHandle == -1) LOGE("Foundation shader: uniform 'uBlushColor' not found");
    if (gCtx.fndLipstickColorHandle == -1) LOGE("Foundation shader: uniform 'uLipstickColor' not found");
    if (gCtx.fndLipstickFinishHandle == -1) LOGE("Foundation shader: uniform 'uLipstickFinish' not found");
    if (gCtx.fndLipstickGlossinessHandle == -1) LOGE("Foundation shader: uniform 'uLipstickGlossiness' not found");
    if (gCtx.fndLipMaskTexHandle == -1) LOGE("Foundation shader: uniform 'sLipMaskTex' not found");
    if (gCtx.fndEyeshadowColorHandle == -1) LOGE("Foundation shader: uniform 'uEyeshadowColor' not found");
    if (gCtx.fndEyeMaskTexHandle == -1) LOGE("Foundation shader: uniform 'sEyeMaskTex' not found");
    if (gCtx.fndConcealerColorHandle == -1) LOGE("Foundation shader: uniform 'uConcealerColor' not found");
    if (gCtx.fndConcealerMaskTexHandle == -1) LOGE("Foundation shader: uniform 'sConcealerMaskTex' not found");
    if (gCtx.fndConcealerStyleHandle == -1) LOGE("Foundation shader: uniform 'uConcealerStyle' not found");
    if (gCtx.fndScaleHandle == -1) LOGE("Foundation shader: uniform 'uScale' not found");
    if (gCtx.fndOffsetHandle == -1) LOGE("Foundation shader: uniform 'uOffset' not found");
    if (gCtx.fndBoundsHandle == -1) LOGE("Foundation shader: uniform 'uFaceBounds' not found");
    if (gCtx.fndAmbientCctHandle == -1) LOGE("Foundation shader: uniform 'uAmbientCCT' not found");
    if (gCtx.fndAmbientIntensityHandle == -1) LOGE("Foundation shader: uniform 'uAmbientIntensity' not found");
    if (gCtx.fndShowMakeupHandle == -1) LOGE("Foundation shader: uniform 'uShowMakeup' not found");

    // Blur Shader
    gCtx.blurProgram = createProgram(BLUR_VERTEX_SHADER, BLUR_FRAGMENT_SHADER);
    gCtx.blurPositionHandle = glGetAttribLocation(gCtx.blurProgram, "aPosition");
    gCtx.blurTexCoordHandle = glGetAttribLocation(gCtx.blurProgram, "aTexCoord");
    gCtx.blurSamplerHandle = glGetUniformLocation(gCtx.blurProgram, "sTexture");
    gCtx.blurTexelSizeHandle = glGetUniformLocation(gCtx.blurProgram, "uTexelSize");
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeResize(JNIEnv* env, jclass clazz, jint width, jint height) {
    LOGI("nativeResize: %d x %d", width, height);
    glViewport(0, 0, width, height);
    gCtx.width = width;
    gCtx.height = height;
    
    gCtx.maskFbo.setup(width, height, true);
    gCtx.maskBlurFbo.setup(width, height, false);
    gCtx.mainFbo.setup(width, height, false);
    gCtx.lipMaskFbo.setup(width, height, false);
    gCtx.eyeMaskFbo.setup(width, height, false);
    gCtx.concealerMaskFbo.setup(width, height, false);

    float screenAspect = (float)height / (float)width;
    float cameraAspect = 16.0f / 9.0f; // Typical portrait

    if (screenAspect > cameraAspect) {
        gCtx.scaleX = screenAspect / cameraAspect;
        gCtx.scaleY = 1.0f;
    } else {
        gCtx.scaleX = 1.0f;
        gCtx.scaleY = cameraAspect / screenAspect;
    }
}

// --- Centripetal Catmull-Rom spline smoothing for sparse lip contours ---
// Research (TAMO, 2026-07-29): the lip contour arrays (11 points per half) connected by
// straight triangle edges render as a visibly faceted/angular polygon, not a smooth
// anatomical curve. Centripetal Catmull-Rom is the recommended fix: it INTERPOLATES
// (passes exactly through every original point, preserving the cupid's-bow concave dip,
// unlike approximating techniques like Chaikin's algorithm which would round it away),
// and "centripetal" parametrization (knot spacing by sqrt(distance) rather than uniform
// spacing) avoids loop/cusp artifacts from the uneven spacing between real lip landmarks.
// This is pure geometry computed from whatever landmarks MediaPipe returns for the
// current face, so it adapts to each user's actual lip shape rather than being a value
// tuned against one tester's face.
struct LipVec3 { float x, y, z; };

static LipVec3 catmullRomPoint(const LipVec3& p0, const LipVec3& p1, const LipVec3& p2, const LipVec3& p3, float t) {
    auto dist = [](const LipVec3& a, const LipVec3& b) {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    };
    auto lerp = [](const LipVec3& a, const LipVec3& b, float alpha) {
        return LipVec3{ a.x + (b.x - a.x) * alpha, a.y + (b.y - a.y) * alpha, a.z + (b.z - a.z) * alpha };
    };

    float t0 = 0.0f;
    float t1 = t0 + sqrtf(dist(p0, p1));
    float t2 = t1 + sqrtf(dist(p1, p2));
    float t3 = t2 + sqrtf(dist(p2, p3));
    // Guard degenerate (near-coincident) control points rather than divide by ~0.
    if (t1 - t0 < 1e-5f) t1 = t0 + 1e-5f;
    if (t2 - t1 < 1e-5f) t2 = t1 + 1e-5f;
    if (t3 - t2 < 1e-5f) t3 = t2 + 1e-5f;

    float tt = t1 + t * (t2 - t1); // map local [0,1] onto this segment's knot interval

    LipVec3 A1 = lerp(p0, p1, (tt - t0) / (t1 - t0));
    LipVec3 A2 = lerp(p1, p2, (tt - t1) / (t2 - t1));
    LipVec3 A3 = lerp(p2, p3, (tt - t2) / (t3 - t2));
    LipVec3 B1 = lerp(A1, A2, (tt - t0) / (t2 - t0));
    LipVec3 B2 = lerp(A2, A3, (tt - t1) / (t3 - t1));
    return lerp(B1, B2, (tt - t1) / (t2 - t1));
}

// Subdivides an N-point control polygon into a smooth curve. Segment endpoints use
// clamped phantom points (duplicate the first/last control point) so the curve doesn't
// extrapolate past the real endpoints — critical here since the first/last point of
// every lip contour is a shared mouth-corner landmark that other contours also anchor to.
// Writes (numControlPoints-1)*subdivisions + 1 points to outPositions/outAlphas and
// returns that count. Caller must size the output buffers accordingly.
static int subdivideLipContour(
    const LipVec3* controlPoints, const float* controlAlphas, int numControlPoints,
    int subdivisions, LipVec3* outPositions, float* outAlphas)
{
    int outCount = 0;
    int numSegments = numControlPoints - 1;
    for (int i = 0; i < numSegments; i++) {
        LipVec3 p0 = (i == 0) ? controlPoints[0] : controlPoints[i - 1];
        LipVec3 p1 = controlPoints[i];
        LipVec3 p2 = controlPoints[i + 1];
        LipVec3 p3 = (i == numSegments - 1) ? controlPoints[numControlPoints - 1] : controlPoints[i + 2];
        float a1 = controlAlphas[i];
        float a2 = controlAlphas[i + 1];

        // Include t=1.0 only on the final segment, so shared knot points between
        // segments aren't emitted twice.
        int steps = (i == numSegments - 1) ? (subdivisions + 1) : subdivisions;
        for (int s = 0; s < steps; s++) {
            float t = (float)s / (float)subdivisions;
            outPositions[outCount] = catmullRomPoint(p0, p1, p2, p3, t);
            outAlphas[outCount] = a1 + (a2 - a1) * t; // plain linear feather, no need for spline smoothing here
            outCount++;
        }
    }
    return outCount;
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeDrawSyncFrame(
    JNIEnv* env, jclass clazz, jint textureId, jobject buffer, jint width, jint height, jint rowStride, jfloatArray landmarks, jboolean hasNewImage)
{
    void* pixels = env->GetDirectBufferAddress(buffer);
    if (!pixels) return;

    if (gCtx.width <= 0 || gCtx.height <= 0 || width <= 0 || height <= 0) return;

    // Calculate dynamic aspect ratio
    float screenAspect = (float)gCtx.height / (float)gCtx.width;
    float cameraAspect = (float)height / (float)width;
    if (width > height) cameraAspect = (float)width / (float)height;

    if (screenAspect > cameraAspect) {
        gCtx.scaleX = screenAspect / cameraAspect;
        gCtx.scaleY = 1.0f;
    } else {
        gCtx.scaleX = 1.0f;
        gCtx.scaleY = cameraAspect / screenAspect;
    }

    // Camera texture must be (re)bound every frame regardless of hasNewImage — texture
    // unit 0 gets repointed to other textures (mainFbo, maskBlurFbo, ...) later in this
    // same function, so on interpolation ticks (hasNewImage=false) skipping the bind here
    // would leave PASS 1 sampling whatever texture unit 0 was left on from the previous
    // frame's later passes instead of the camera image. Only the actual pixel upload
    // (glTexImage2D) is skipped when there's no new frame — the texture object already
    // holds the last uploaded image's data.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    if (hasNewImage) {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, rowStride / 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    const float QUAD_VERTICES[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    const float CAM_TEX_COORDS[] = {
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f
    };
    const float FBO_TEX_COORDS[] = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f // FBOs are usually right-side up
    };

    // --- PASS 1: Render Camera to Main FBO ---
    glBindFramebuffer(GL_FRAMEBUFFER, gCtx.mainFbo.fbo);
    glViewport(0, 0, gCtx.width, gCtx.height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gCtx.cameraProgram);
    glVertexAttribPointer(gCtx.camPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, QUAD_VERTICES);
    glEnableVertexAttribArray(gCtx.camPositionHandle);
    glVertexAttribPointer(gCtx.camTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0, CAM_TEX_COORDS);
    glEnableVertexAttribArray(gCtx.camTexCoordHandle);
    glUniform2f(gCtx.camScaleHandle, gCtx.scaleX, gCtx.scaleY);
    glUniform1i(gCtx.camSamplerHandle, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(gCtx.camPositionHandle);
    glDisableVertexAttribArray(gCtx.camTexCoordHandle);

    // --- PASS 1: Generate Hard Face Mask (Face + Holes) ---
    glBindFramebuffer(GL_FRAMEBUFFER, gCtx.maskFbo.fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // lipMaskFbo/eyeMaskFbo/concealerMaskFbo are only WRITTEN inside the
    // `landmarks != nullptr` block below (their bake needs real geometry), but
    // must be CLEARED unconditionally every frame regardless of face detection —
    // otherwise, the moment the face leaves frame, this whole block is skipped
    // and these dedicated FBOs simply keep whatever they last baked (e.g.
    // lipstick staying visibly "stuck" onscreen with no face in view at all).
    // maskFbo above already gets cleared unconditionally; these three need the
    // same treatment since PASS 1c/1d/1e write to separate dedicated targets.
    glBindFramebuffer(GL_FRAMEBUFFER, gCtx.lipMaskFbo.fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, gCtx.eyeMaskFbo.fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, gCtx.concealerMaskFbo.fbo);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, gCtx.maskFbo.fbo);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW); // Flipped for mirrored front camera

    if (landmarks != nullptr) {
        jsize len = env->GetArrayLength(landmarks);
        int vertexCount = len / 3;
        if (vertexCount >= 478) {
            jboolean isCopy = JNI_FALSE;
            float* data = env->GetFloatArrayElements(landmarks, &isCopy);

            // 1. Calculate Face Bounding Box for Fresnel
            float minX = 1.0f, maxX = 0.0f, minY = 1.0f, maxY = 0.0f;
            for (int i = 0; i < vertexCount; i++) {
                float x = data[i * 3 + 0];
                float y = data[i * 3 + 1];
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }

            // 1b. Extend the bounding box toward the estimated hairline. MediaPipe's
            // landmark topology has no hairline point — its sparsest region is the
            // forehead, and its highest standard landmark (10) sits well below the
            // real hairline for most people (worse for taller/wider foreheads), so
            // foundation coverage was capping out short of the hairline. Research
            // (anthropometric studies, trichion/glabella/subnasale measurements)
            // found no literature backing for extrapolating from landmark 10 itself
            // (too unreliable/sparse a region), but a well-documented ratio DOES
            // exist between brow (glabella) and nose-base (subnasale) — the upper
            // face third runs ~0.83-0.85x the length of the middle third — and both
            // reference points sit in MediaPipe's dense, reliably-tracked region.
            // Landmark 168 = glabella/sellion (verified against MediaPipe's own
            // canonical 3D face model spec — see
            // docs/research/mediapipe_nose_contour_landmarks_guide.md), landmark 2 =
            // subnasale/infranasale base. Direction is taken from landmark 10 (upper
            // forehead) minus landmark 152 (chin) — anatomically unambiguous
            // "toward-forehead" regardless of this array's coordinate convention
            // (remapped/rotated upstream in FizgravityARView.kt).
            {
                float glabellaX = data[168 * 3 + 0], glabellaY = data[168 * 3 + 1];
                float subnasaleX = data[2 * 3 + 0], subnasaleY = data[2 * 3 + 1];
                float browToNoseDx = glabellaX - subnasaleX;
                float browToNoseDy = glabellaY - subnasaleY;
                float browToNoseDist = sqrtf(browToNoseDx * browToNoseDx + browToNoseDy * browToNoseDy);

                float fhX = data[10 * 3 + 0] - data[152 * 3 + 0];
                float fhY = data[10 * 3 + 1] - data[152 * 3 + 1];
                float fhLen = sqrtf(fhX * fhX + fhY * fhY);
                if (fhLen > 0.0001f) { fhX /= fhLen; fhY /= fhLen; }

                float hairlineX = glabellaX + fhX * browToNoseDist * 0.84f;
                float hairlineY = glabellaY + fhY * browToNoseDist * 0.84f;

                if (hairlineX < minX) minX = hairlineX;
                if (hairlineX > maxX) maxX = hairlineX;
                if (hairlineY < minY) minY = hairlineY;
                if (hairlineY > maxY) maxY = hairlineY;
            }

            float faceCenterX = (minX + maxX) / 2.0f;
            float faceCenterY = (minY + maxY) / 2.0f;
            float faceRadiusX = (maxX - minX) / 2.0f;
            float faceRadiusY = (maxY - minY) / 2.0f;
            // Prevent division by zero if radii are too small
            if (faceRadiusX < 0.01f) faceRadiusX = 0.01f;
            if (faceRadiusY < 0.01f) faceRadiusY = 0.01f;
            
            gCtx.faceCenterX = faceCenterX;
            gCtx.faceCenterY = faceCenterY;
            gCtx.faceRadiusX = faceRadiusX;
            gCtx.faceRadiusY = faceRadiusY;

            // 2. Setup Vertex Colors for Hard Holes and RGB Mask (Contour/Highlight)
            glUseProgram(gCtx.maskProgram);
            glUniform2f(gCtx.maskScaleHandle, gCtx.scaleX, gCtx.scaleY);
            glUniform2f(gCtx.maskOffsetHandle, gCtx.offsetX, gCtx.offsetY);
            glUniform4f(gCtx.maskBoundsHandle, faceCenterX, faceCenterY, faceRadiusX, faceRadiusY);
            glUniform1i(gCtx.maskIsHoleH, 0);
            
            glVertexAttribPointer(gCtx.maskPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, data);
            glEnableVertexAttribArray(gCtx.maskPositionHandle);
            
            // Draw full face mask
            int numIndices = sizeof(MESH_INDICES) / sizeof(MESH_INDICES[0]);
            glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);
            
            // --- THE HARD HOLES (Stomping the FBO with Black) ---
            glDisable(GL_DEPTH_TEST);
            glUniform1i(gCtx.maskIsHoleH, 1);
            glDrawElements(GL_TRIANGLE_FAN, sizeof(INNER_LIPS_INDICES)/sizeof(short), GL_UNSIGNED_SHORT, INNER_LIPS_INDICES);
            glDrawElements(GL_TRIANGLE_FAN, sizeof(LEFT_EYE_CONTOUR_INDICES)/sizeof(short), GL_UNSIGNED_SHORT, LEFT_EYE_CONTOUR_INDICES);
            glDrawElements(GL_TRIANGLE_FAN, sizeof(RIGHT_EYE_CONTOUR_INDICES)/sizeof(short), GL_UNSIGNED_SHORT, RIGHT_EYE_CONTOUR_INDICES);
            glLineWidth(8.0f);
            glDrawElements(GL_LINE_LOOP, sizeof(LEFT_EYEBROW_INDICES)/sizeof(short), GL_UNSIGNED_SHORT, LEFT_EYEBROW_INDICES);
            glDrawElements(GL_LINE_LOOP, sizeof(RIGHT_EYEBROW_INDICES)/sizeof(short), GL_UNSIGNED_SHORT, RIGHT_EYEBROW_INDICES);
            glDisableVertexAttribArray(gCtx.maskPositionHandle);

            // ============================================================
            // PASS 1b: BAKE MAKEUP WEIGHTS (Contour, Blush, Highlight)
            // Render directly into the mask FBO using color channels!
            // R = Foundation, G = Contour, B = Blush, A = Highlight
            // ============================================================
            glUseProgram(gCtx.makeupWeightProgram);
            glUniform2f(gCtx.mkScaleHandle, gCtx.scaleX, gCtx.scaleY);
            glUniform2f(gCtx.mkOffsetHandle, gCtx.offsetX, gCtx.offsetY);
            glVertexAttribPointer(gCtx.mkPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, data);
            glEnableVertexAttribArray(gCtx.mkPositionHandle);

            // Disable depth test and blending. We just want to write the absolute weights to the channels.
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            
            // --- CONTOUR (Channel G) ---
            // Style 1 (Slim) reshapes the jawline/cheeks and keeps using the static
            // FACE_CONTOUR_SLIM table (unaffected by the nose ghosting bug). Styles
            // 0/2/3 (Normal/Pinch/Straighten) reshape the NOSE, and are now computed
            // LIVE every frame from verified nose-bridge landmarks instead of the old
            // hand-baked NOSE_CONTOUR_* tables, whose shape didn't match real face
            // geometry (it rendered as an asymmetric shadow blob instead of two clean
            // lines flanking the bridge).
            glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
            if (gCtx.contourStyle == 1) {
                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, makeup_weights::FACE_CONTOUR_SLIM);
            } else {
                static float noseContourAlpha[468];
                for (int i = 0; i < 468; i++) noseContourAlpha[i] = 0.0f;
                const int flankLeftCount = sizeof(NOSE_BRIDGE_FLANK_LEFT) / sizeof(NOSE_BRIDGE_FLANK_LEFT[0]);
                for (int i = 0; i < flankLeftCount; i++) noseContourAlpha[NOSE_BRIDGE_FLANK_LEFT[i]] = 1.0f;
                const int flankRightCount = sizeof(NOSE_BRIDGE_FLANK_RIGHT) / sizeof(NOSE_BRIDGE_FLANK_RIGHT[0]);
                for (int i = 0; i < flankRightCount; i++) noseContourAlpha[NOSE_BRIDGE_FLANK_RIGHT[i]] = 1.0f;
                if (gCtx.contourStyle == 2) {
                    // Pinch: widen the shadow slightly toward the nostril flare so the
                    // remaining bright ridge between the two shadow lines reads narrower.
                    noseContourAlpha[129] = 1.0f;
                    noseContourAlpha[358] = 1.0f;
                }
                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, noseContourAlpha);
            }
            glEnableVertexAttribArray(gCtx.mkAlphaHandle);
            glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);

            // --- BLUSH (Channel B) ---
            // Style 0 = Apple of Cheek (round radial), Style 1 = Draped/Lifting (diagonal
            // ellipse toward the temple), Style 2 = Sun-kissed (wide horizontal band).
            glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
            {
                // Build a temporary per-vertex alpha buffer for blush
                static float blushVertexAlpha[468] = {0.0f};
                const int blushCount = sizeof(BLUSH_INDICES) / sizeof(BLUSH_INDICES[0]);
                for (int i = 0; i < 468; i++) blushVertexAlpha[i] = 0.0f;
                int halfCount = blushCount / 2;
                float leftCx = 0, leftCy = 0, rightCx = 0, rightCy = 0;
                for (int i = 0; i < halfCount; i++) {
                    int idx = BLUSH_INDICES[i];
                    leftCx += data[idx * 3 + 0]; leftCy += data[idx * 3 + 1];
                }
                for (int i = halfCount; i < blushCount; i++) {
                    int idx = BLUSH_INDICES[i];
                    rightCx += data[idx * 3 + 0]; rightCy += data[idx * 3 + 1];
                }
                leftCx /= halfCount; leftCy /= halfCount;
                rightCx /= (blushCount - halfCount); rightCy /= (blushCount - halfCount);

                // Scale-aware base radius (was a hardcoded 0.12 regardless of face size/distance).
                float baseRadius = gCtx.faceRadiusX * 0.5f;

                float radiusX = baseRadius, radiusY = baseRadius, axisAngle = 0.0f;
                if (gCtx.blushStyle == 1) {
                    // Draped / Lifting: elongate diagonally up-and-outward toward the temple.
                    radiusX = baseRadius * 1.6f;
                    radiusY = baseRadius * 0.65f;
                    axisAngle = 0.785398f; // 45 degrees
                } else if (gCtx.blushStyle == 2) {
                    // Sun-kissed: wide horizontal band sweeping toward the nose bridge.
                    radiusX = baseRadius * 2.4f;
                    radiusY = baseRadius * 0.5f;
                    axisAngle = 0.0f;
                }

                for (int i = 0; i < blushCount; i++) {
                    int idx = BLUSH_INDICES[i];
                    bool isLeftSide = i < halfCount;
                    float vx = data[idx * 3 + 0];
                    float vy = data[idx * 3 + 1];
                    float cx = isLeftSide ? leftCx : rightCx;
                    float cy = isLeftSide ? leftCy : rightCy;
                    float dx = vx - cx;
                    float dy = vy - cy;

                    // Mirror the diagonal axis per side so both cheeks lift symmetrically outward.
                    float angle = isLeftSide ? -axisAngle : axisAngle;
                    float ca = cosf(angle), sa = sinf(angle);
                    float rdx = dx * ca - dy * sa;
                    float rdy = dx * sa + dy * ca;

                    float normDist = sqrtf((rdx * rdx) / (radiusX * radiusX) + (rdy * rdy) / (radiusY * radiusY));
                    float alpha = 1.0f - fminf(normDist, 1.0f);
                    alpha = alpha * alpha * (3.0f - 2.0f * alpha);
                    blushVertexAlpha[idx] = alpha;
                }
                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, blushVertexAlpha);
                glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);
            }

            // --- HIGHLIGHTER (Channel A) ---
            // Computed live from the same verified nose-bridge midline used for
            // contour above, so the two can never drift out of sync again (the
            // earlier bug: highlighter always stayed on the "Normal" shape while
            // contour switched to Pinch/Straighten, reading as a ghosted nose).
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
            {
                static float noseHighlightAlpha[468];
                for (int i = 0; i < 468; i++) noseHighlightAlpha[i] = 0.0f;
                const unsigned short coreMidline[] = {6, 197, 195, 5, 4};
                for (int i = 0; i < 5; i++) noseHighlightAlpha[coreMidline[i]] = 1.0f;
                if (gCtx.contourStyle == 3) {
                    // Straighten: extend the highlight the full length of the bridge
                    // for a longer, more prominent straight ridge line.
                    noseHighlightAlpha[168] = 1.0f;
                    noseHighlightAlpha[1] = 1.0f;
                    noseHighlightAlpha[19] = 1.0f;
                }
                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, noseHighlightAlpha);
            }
            glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);

            // Restore color mask to RGBA
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            glDisableVertexAttribArray(gCtx.mkPositionHandle);
            glDisableVertexAttribArray(gCtx.mkAlphaHandle);

            // ============================================================
            // PASS 1c: BAKE LIPSTICK WEIGHT into its own dedicated FBO.
            // The maskFbo's RGBA channels are already fully used by
            // Foundation/Contour/Blush/Highlight, so lipstick needs a
            // separate single-channel target.
            // ============================================================
            glBindFramebuffer(GL_FRAMEBUFFER, gCtx.lipMaskFbo.fbo);
            glViewport(0, 0, gCtx.width, gCtx.height);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(gCtx.makeupWeightProgram);
            glUniform2f(gCtx.mkScaleHandle, gCtx.scaleX, gCtx.scaleY);
            glUniform2f(gCtx.mkOffsetHandle, gCtx.offsetX, gCtx.offsetY);
            {
                // Smooth the 4 sparse (11-point) lip contours into dense curves via
                // centripetal Catmull-Rom (see subdivideLipContour above for the
                // technique/why) before triangulating, instead of connecting the
                // original 11 points with straight edges (which rendered as a visibly
                // faceted/angular polygon, confirmed on-device). Subdivided points are
                // NEW positions that don't correspond to any MediaPipe landmark index,
                // so this pass builds and binds its OWN local position/alpha/index
                // buffers instead of indexing into the shared per-frame `data` array
                // like every other mask-baking pass in this function does.
                const int SUBDIV = 5; // ~5 segments per original edge; raise if still visibly faceted
                const int PTS_PER_CONTOUR = 10 * SUBDIV + 1; // (11 control points - 1 segments) * SUBDIV + 1

                LipVec3 ctrlPos[11];
                float ctrlAlphaOuter[11], ctrlAlphaInner[11];
                // Baked mask raw value: 0 (outer/vermilion edge) .. 1 (inner edge).
                // Reverted the earlier diagnostic (which set this to 1.0/solid to isolate
                // a geometry-vs-alpha question — confirmed: geometry margin was already
                // correct, the "faded" look was alpha) — the shader now applies its OWN
                // finish-specific remap (matte/satin/glossy/sheer/shimmer, see PASS 3's
                // fragment shader) on top of this raw gradient, so this needs to stay a
                // real 0..1 gradient across the lip width for those remaps to have
                // anything to work with, not a flat 1.0.
                for (int i = 0; i < 11; i++) { ctrlAlphaOuter[i] = 0.15f; ctrlAlphaInner[i] = 1.0f; }

                static LipVec3 lipSubPos[8 * PTS_PER_CONTOUR];
                static float lipSubAlpha[8 * PTS_PER_CONTOUR];
                // Baked alongside lipSubAlpha (same static/lifetime pattern), for the
                // glossy/satin highlight (see below): lipSubU is this vertex's position
                // along the mouth's horizontal width (0=one corner, 1=the other — cheap,
                // since the subdivided contours are already built corner-to-corner in
                // index order, so u is just i normalized, no extra distance math needed).
                // lipSubLowerWeight is 0 for upper-lip vertices, 1 for lower-lip vertices
                // — gates the highlight to the lower lip (where real specular highlights
                // usually sit), which the mesh topology already knows per contour.
                static float lipSubU[8 * PTS_PER_CONTOUR];
                static float lipSubLowerWeight[8 * PTS_PER_CONTOUR];
                int base = 0;
                int upperOuterBase, upperInnerBase, lowerOuterBase, lowerInnerBase;

                auto extractAndSubdivide = [&](const unsigned short* indices, const float* alphas, int& outBase) {
                    for (int i = 0; i < 11; i++) {
                        ctrlPos[i] = LipVec3{ data[indices[i] * 3 + 0], data[indices[i] * 3 + 1], data[indices[i] * 3 + 2] };
                    }
                    outBase = base;
                    base += subdivideLipContour(ctrlPos, alphas, 11, SUBDIV, &lipSubPos[outBase], &lipSubAlpha[outBase]);
                };
                extractAndSubdivide(UPPER_LIP_OUTER, ctrlAlphaOuter, upperOuterBase);
                extractAndSubdivide(UPPER_LIP_INNER, ctrlAlphaInner, upperInnerBase);
                extractAndSubdivide(LOWER_LIP_OUTER, ctrlAlphaOuter, lowerOuterBase);
                extractAndSubdivide(LOWER_LIP_INNER, ctrlAlphaInner, lowerInnerBase);

                // Reverted (see git history for the two attempts that regressed this):
                // a fixed-distance inward push at full alpha overshot visibly on close
                // (the lower inner lip "crashed" upward past the upper lip), and a
                // centroid/outer-inner-derived outer halo made the outer boundary itself
                // read as imprecise. Back to the last confirmed-stable mechanism: a
                // SEPARATE duplicate of the inner contours with DYNAMIC alpha — full when
                // the mouth is closed (upper/lower inner contours nearly coincide),
                // fading to 0 as the mouth opens (real per-frame distance between them).
                // Reusing upperInnerBase/lowerInnerBase directly would have forced those
                // strips' own alpha to change too, since they're driven by the same
                // vertex data. Distance is normalized by mouth corner-to-corner width so
                // the threshold is scale-invariant (same behavior close or far from the
                // camera). The outer boundary is deliberately left untouched here (no
                // halo) — user confirmed it was already precise before the halo attempts;
                // extending it further for thick/folded lips needs its own separate,
                // carefully-verified pass rather than stacking onto this fix.
                int seamUpperBase = base;
                int seamLowerBase = base + PTS_PER_CONTOUR;
                base += 2 * PTS_PER_CONTOUR;
                {
                    LipVec3 cornerL = lipSubPos[upperOuterBase];
                    LipVec3 cornerR = lipSubPos[upperOuterBase + PTS_PER_CONTOUR - 1];
                    float mdx = cornerR.x - cornerL.x, mdy = cornerR.y - cornerL.y, mdz = cornerR.z - cornerL.z;
                    float mouthWidth = sqrtf(mdx * mdx + mdy * mdy + mdz * mdz);
                    if (mouthWidth < 1e-5f) mouthWidth = 1e-5f;

                    // ONE global "how open is the mouth" measurement (the gap at the
                    // horizontal center point), instead of a per-point local gap. This
                    // exists ONLY to patch tracking noise at a truly-shut mouth (upper/
                    // lower inner contours SHOULD coincide when closed but don't quite,
                    // per MediaPipe's own imprecision) — it is not meant to be the thing
                    // that visually defines "how open is the mouth." That job already
                    // belongs to the real upperInner/lowerInner curves themselves (drawn
                    // via the outer<->inner strips above, right up to their true tracked
                    // positions) — those already trace the mouth's actual elongated shape
                    // correctly on their own.
                    //
                    // A PER-POINT gap (tried first) meant each point along the width
                    // crossed its own open/closed threshold at a different moment as the
                    // mouth opened/closed — since a real mouth's opening profile isn't a
                    // sharp center-peaked parabola, most points cross around the same
                    // time but not exactly together, and the tiny stagger reads as the
                    // seam's covered/uncovered boundary visibly sweeping in from the
                    // sides toward the center (user: "seperti ada garis vertikal pelan
                    // pelan mendekat [dari kiri kanan]") instead of a natural top-meets-
                    // bottom closure. Driving every point's alpha off ONE shared value
                    // makes the whole central seam appear/disappear together as a single
                    // unit — nothing to sweep.
                    int centerIdx = PTS_PER_CONTOUR / 2;
                    LipVec3 cpu = lipSubPos[upperInnerBase + centerIdx];
                    LipVec3 cpl = lipSubPos[lowerInnerBase + centerIdx];
                    float cdx = cpu.x - cpl.x, cdy = cpu.y - cpl.y, cdz = cpu.z - cpl.z;
                    float centerRelGap = sqrtf(cdx * cdx + cdy * cdy + cdz * cdz) / mouthWidth;

                    // Temporal smoothing (exponential moving average across frames): the
                    // 1%-3% window below is tight enough that ordinary per-frame tracking
                    // jitter (worse during camera movement — head pose/perspective noise
                    // feeds into the landmark position, not just genuine mouth motion) can
                    // cross it on its own, making the seam flicker open/closed with no
                    // real mouth movement (user: "kalau kamera digerak-gerakan kadang
                    // lipstick mangap nutup sendiri"). A static (persists across frames)
                    // smoothed value damps that frame-to-frame noise while still tracking
                    // real mouth motion within a few frames. Lowered 0.3 -> 0.15 per user
                    // request: flicker still visible at 0.3 when the camera was far from
                    // the face and moved around (farther distance = smaller pixel/landmark
                    // displacement for the same real-world gap, so tracking noise is
                    // proportionally larger relative to the signal there) — user
                    // explicitly traded a bit more lag on real mouth transitions for
                    // steadiness in that case.
                    static float smoothedCenterRelGap = 0.0f;
                    const float SEAM_SMOOTHING = 0.15f;
                    smoothedCenterRelGap += SEAM_SMOOTHING * (centerRelGap - smoothedCenterRelGap);

                    // Near-binary snap so the seam only ever contributes a few-pixel
                    // anti-noise patch; the moment there's genuine daylight between the
                    // lips, the seam gets out of the way almost immediately and the real
                    // geometry takes over the whole visual. Tightened from 2%-6%: user
                    // confirmed mask still read as fully closed while teeth were still
                    // visibly parted, meaning even a "just barely opening" gap (a few mm)
                    // was still landing inside the old window.
                    float t = (smoothedCenterRelGap - 0.01f) / (0.03f - 0.01f);
                    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                    float globalSeamAlpha = 1.0f - t;

                    for (int i = 0; i < PTS_PER_CONTOUR; i++) {
                        LipVec3 pu = lipSubPos[upperInnerBase + i];
                        LipVec3 pl = lipSubPos[lowerInnerBase + i];
                        float seamAlpha = globalSeamAlpha;

                        // Corner lock: UPPER_LIP_INNER and LOWER_LIP_INNER share the exact
                        // same two corner landmarks (78, 308) — pu==pl exactly (same
                        // MediaPipe landmark read twice) right at i=0/i=last, zero gap
                        // ALWAYS, by construction — no tracking noise is possible there.
                        // A prior version locked a wide 25%-of-width zone near each corner
                        // to solid alpha, which overcorrected: it painted over the natural
                        // V-taper shape that the real upperOuter->upperInner and
                        // lowerOuter->lowerInner strips already form on their own as they
                        // converge toward that shared corner vertex, so the corner read as
                        // a blunt filled patch instead of a proper pointed corner (user:
                        // "ko tidak membentuk sudut ya bang"). Narrowed to a tiny lock zone
                        // (4% of the way to center) — just enough to smooth over the
                        // sub-pixel Catmull-Rom subdivision artifact right at the shared
                        // vertex — so the real geometry's natural taper is what defines the
                        // corner shape almost everywhere else.
                        float distFromCorner = (float)(i < PTS_PER_CONTOUR - 1 - i ? i : PTS_PER_CONTOUR - 1 - i);
                        float distFromCornerNorm = distFromCorner / (float)(PTS_PER_CONTOUR / 2);
                        float cl = distFromCornerNorm / 0.04f;
                        cl = cl < 0.0f ? 0.0f : (cl > 1.0f ? 1.0f : cl);
                        float cornerLock = 1.0f - (cl * cl * (3.0f - 2.0f * cl)); // smoothstep, inverted
                        if (cornerLock > seamAlpha) seamAlpha = cornerLock;

                        lipSubPos[seamUpperBase + i] = pu;
                        lipSubPos[seamLowerBase + i] = pl;
                        lipSubAlpha[seamUpperBase + i] = seamAlpha;
                        lipSubAlpha[seamLowerBase + i] = seamAlpha;
                    }
                }

                // Outer margin halo: a thin extra band just beyond the true
                // UPPER/LOWER_LIP_OUTER landmarks, fading from the existing outer-edge
                // alpha (0.15) down to 0 over a small outward push. MediaPipe's outer lip
                // landmarks track a fixed topological ring, which sits slightly inside the
                // visible vermilion edge for thick or slightly-folded lips — user confirmed
                // the boundary SHAPE is precise, it's just uniformly a bit inside the real
                // lip line. Re-added deliberately isolated from the seam mechanism above
                // (reads upperInnerBase/lowerInnerBase for direction only, never writes to
                // them or to the seam buffers) after two earlier combined attempts
                // regressed elsewhere — this version only touches its own dedicated
                // expandedUpper/LowerOuterBase buffers.
                //
                // Direction is derived per-point as (outer - inner) at the SAME contour
                // index, NOT from a single shared mouth centroid — a centroid-based radial
                // push (tried first) went wrong right at the mouth corners, where "away
                // from centroid" pointed down into the mouth cavity instead of outward.
                // (outer - inner) always points from the mouth interior toward the true
                // outside by construction, and naturally shrinks to ~0 right at the
                // corners (where outer and inner nearly coincide), so it can't push into
                // the cavity there either.
                // Plain 2-row quad, fading from the true landmark's OWN alpha (0.15,
                // continuous — no jump) down to 0 at the pushed-out tip. (A 3-ring
                // "near-solid then sharp drop" version existed briefly to make a flat
                // diagnostic test color look solid, but that was compensating for the
                // now-reverted 1.0 diagnostic alpha; with the real 0.15 outer alpha
                // restored and finish-specific curves now doing the "how solid" job in
                // the shader, a 3rd ring here would just double up on that decision.)
                int expandedUpperOuterBase = base;
                int expandedLowerOuterBase = base + PTS_PER_CONTOUR;
                base += 2 * PTS_PER_CONTOUR;
                {
                    LipVec3 cornerL = lipSubPos[upperOuterBase];
                    LipVec3 cornerR = lipSubPos[upperOuterBase + PTS_PER_CONTOUR - 1];
                    float mdx = cornerR.x - cornerL.x, mdy = cornerR.y - cornerL.y;
                    float mouthWidth = sqrtf(mdx * mdx + mdy * mdy);
                    if (mouthWidth < 1e-5f) mouthWidth = 1e-5f;
                    // Small on purpose — a forgiving margin for thick/folded lips, not a
                    // general resize of the lip shape. 0.0675 = user-calibrated value
                    // (1.5x an initial 0.045 that was still slightly short).
                    float pushDist = 0.0675f * mouthWidth;

                    auto expandOutward = [&](int outerBase, int innerBase, int dstBase) {
                        for (int i = 0; i < PTS_PER_CONTOUR; i++) {
                            LipVec3 p = lipSubPos[outerBase + i];
                            LipVec3 q = lipSubPos[innerBase + i];
                            float dx = p.x - q.x, dy = p.y - q.y;
                            float len = sqrtf(dx * dx + dy * dy);
                            float nx = len > 1e-5f ? dx / len : 0.0f;
                            float ny = len > 1e-5f ? dy / len : 0.0f;
                            lipSubPos[dstBase + i] = LipVec3{ p.x + nx * pushDist, p.y + ny * pushDist, p.z };
                            lipSubAlpha[dstBase + i] = 0.0f;
                        }
                    };
                    expandOutward(upperOuterBase, upperInnerBase, expandedUpperOuterBase);
                    expandOutward(lowerOuterBase, lowerInnerBase, expandedLowerOuterBase);
                }

                // Fill lipSubU/lipSubLowerWeight for every contour built above — u only
                // depends on the local index i (same formula regardless of which contour),
                // so one pass over all 8 bases covers everything.
                {
                    int allBases[8] = { upperOuterBase, upperInnerBase, lowerOuterBase, lowerInnerBase,
                                         seamUpperBase, seamLowerBase, expandedUpperOuterBase, expandedLowerOuterBase };
                    float lowerWeights[8] = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f };
                    for (int b = 0; b < 8; b++) {
                        for (int i = 0; i < PTS_PER_CONTOUR; i++) {
                            lipSubU[allBases[b] + i] = (float)i / (float)(PTS_PER_CONTOUR - 1);
                            lipSubLowerWeight[allBases[b] + i] = lowerWeights[b];
                        }
                    }
                }

                // Quad-strip triangulation across the subdivided (dense) contours —
                // same pattern as before, just over PTS_PER_CONTOUR-1 segments instead
                // of 10, and using LOCAL indices into lipSubPos/lipSubAlpha rather than
                // MediaPipe landmark indices.
                static unsigned short lipTriangleIndices[6 * (10 * 6) * 6]; // generous upper bound (5 strips currently)
                int lipIdx = 0;
                auto addQuadStrip = [&](int outerBase, int innerBase) {
                    for (int i = 0; i < PTS_PER_CONTOUR - 1; i++) {
                        lipTriangleIndices[lipIdx++] = (unsigned short)(outerBase + i);
                        lipTriangleIndices[lipIdx++] = (unsigned short)(outerBase + i + 1);
                        lipTriangleIndices[lipIdx++] = (unsigned short)(innerBase + i);
                        lipTriangleIndices[lipIdx++] = (unsigned short)(innerBase + i);
                        lipTriangleIndices[lipIdx++] = (unsigned short)(outerBase + i + 1);
                        lipTriangleIndices[lipIdx++] = (unsigned short)(innerBase + i + 1);
                    }
                };
                addQuadStrip(upperOuterBase, upperInnerBase);
                addQuadStrip(lowerOuterBase, lowerInnerBase);
                addQuadStrip(seamUpperBase, seamLowerBase);
                addQuadStrip(expandedUpperOuterBase, upperOuterBase);
                addQuadStrip(expandedLowerOuterBase, lowerOuterBase);

                glVertexAttribPointer(gCtx.mkPositionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(LipVec3), lipSubPos);
                glEnableVertexAttribArray(gCtx.mkPositionHandle);
                glEnableVertexAttribArray(gCtx.mkAlphaHandle);
                // GL_CULL_FACE is still enabled from PASS 1 (glCullFace(GL_BACK),
                // glFrontFace(GL_CW), only disabled later at the end of this whole
                // baking sequence) — this is a mask-baking pass, not 3D solid geometry,
                // so winding direction is irrelevant to what we want (rasterize the
                // alpha data regardless of triangle orientation). Confirmed on-device
                // with the previous (non-subdivided) version: with culling on, upper
                // lip triangles were silently discarded entirely.
                glDisable(GL_CULL_FACE);

                // Three passes into the same RGBA FBO, gated by glColorMask (same
                // multi-channel-bake technique already used for contour/blush/highlight
                // sharing maskFbo) — R=coverage alpha (existing), G=horizontal position
                // along the mouth (for the highlight's position), B=lower-lip weight (for
                // gating the highlight to the lower lip). One geometry, one shader
                // (makeupWeightProgram just outputs whatever's bound to aAlpha into all 4
                // channels; colorMask picks which channel actually lands), three
                // attribute rebinds + draws.
                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, lipSubAlpha);
                glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
                glDrawElements(GL_TRIANGLES, lipIdx, GL_UNSIGNED_SHORT, lipTriangleIndices);

                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, lipSubU);
                glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_FALSE);
                glDrawElements(GL_TRIANGLES, lipIdx, GL_UNSIGNED_SHORT, lipTriangleIndices);

                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, lipSubLowerWeight);
                glColorMask(GL_FALSE, GL_FALSE, GL_TRUE, GL_FALSE);
                glDrawElements(GL_TRIANGLES, lipIdx, GL_UNSIGNED_SHORT, lipTriangleIndices);

                glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            }
            glDisableVertexAttribArray(gCtx.mkPositionHandle);
            glDisableVertexAttribArray(gCtx.mkAlphaHandle);

            // ============================================================
            // PASS 1d: BAKE EYESHADOW WEIGHT into its own dedicated FBO.
            // Upper-eyelid-only region (LEFT/RIGHT_EYESHADOW_REGION: lash line ->
            // crease patch, verified against MediaPipe's FACEMESH_LEFT_EYE/
            // FACEMESH_RIGHT_EYE connections). Excludes the lower lid, so this no
            // longer paints a full ring around the eye. Because this mask is
            // re-baked every frame from the live landmark positions (same as
            // every other layer here), blinking already compresses/stretches it
            // naturally via the mesh's own UV deformation — no separate elastic-
            // mesh/blink-blendshape plumbing is needed for that (confirmed in
            // docs/research/ar_beauty_filter_validation_report.md section 5).
            // The existing eye-contour hole punch (Pass 1) already zeroes
            // foundationMask over the open-eye/eyeball area, which protects
            // the sclera/iris from getting painted since eyeAlpha is
            // multiplied by foundationMask in the compositing shader.
            // ============================================================
            glBindFramebuffer(GL_FRAMEBUFFER, gCtx.eyeMaskFbo.fbo);
            glViewport(0, 0, gCtx.width, gCtx.height);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(gCtx.makeupWeightProgram);
            glUniform2f(gCtx.mkScaleHandle, gCtx.scaleX, gCtx.scaleY);
            glUniform2f(gCtx.mkOffsetHandle, gCtx.offsetX, gCtx.offsetY);
            glVertexAttribPointer(gCtx.mkPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, data);
            glEnableVertexAttribArray(gCtx.mkPositionHandle);
            {
                static float eyeVertexAlpha[468] = {0.0f};
                for (int i = 0; i < 468; i++) eyeVertexAlpha[i] = 0.0f;
                const int leftCount = sizeof(LEFT_EYESHADOW_REGION) / sizeof(LEFT_EYESHADOW_REGION[0]);
                for (int i = 0; i < leftCount; i++) {
                    eyeVertexAlpha[LEFT_EYESHADOW_REGION[i]] = 1.0f;
                }
                const int rightCount = sizeof(RIGHT_EYESHADOW_REGION) / sizeof(RIGHT_EYESHADOW_REGION[0]);
                for (int i = 0; i < rightCount; i++) {
                    eyeVertexAlpha[RIGHT_EYESHADOW_REGION[i]] = 1.0f;
                }
                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, eyeVertexAlpha);
                glEnableVertexAttribArray(gCtx.mkAlphaHandle);
                glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);
            }
            glDisableVertexAttribArray(gCtx.mkPositionHandle);
            glDisableVertexAttribArray(gCtx.mkAlphaHandle);

            // ============================================================
            // PASS 1e: BAKE CONCEALER WEIGHT into its own dedicated FBO.
            // Two region types per side: under-eye tear-trough (full
            // strength) and nasolabial fold (anti-caking — weighted down
            // relative to under-eye so it doesn't visibly cake in the
            // deepest part of the fold, per the dictionary's concealer
            // spec). Uses the same radial smoothstep falloff already used
            // for BLUSH_INDICES below, computed per-region around its own
            // live centroid so edges stay soft without a dedicated blur
            // pass (this FBO, like lipMaskFbo/eyeMaskFbo, isn't blurred).
            // ============================================================
            glBindFramebuffer(GL_FRAMEBUFFER, gCtx.concealerMaskFbo.fbo);
            glViewport(0, 0, gCtx.width, gCtx.height);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glUseProgram(gCtx.makeupWeightProgram);
            glUniform2f(gCtx.mkScaleHandle, gCtx.scaleX, gCtx.scaleY);
            glUniform2f(gCtx.mkOffsetHandle, gCtx.offsetX, gCtx.offsetY);
            glVertexAttribPointer(gCtx.mkPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, data);
            glEnableVertexAttribArray(gCtx.mkPositionHandle);
            {
                static float concealerVertexAlpha[468] = {0.0f};
                for (int i = 0; i < 468; i++) concealerVertexAlpha[i] = 0.0f;

                auto bakeConcealerRegion = [&](const unsigned short* regionIndices, int count, float weightMultiplier) {
                    float cx = 0.0f, cy = 0.0f;
                    for (int i = 0; i < count; i++) {
                        cx += data[regionIndices[i] * 3 + 0];
                        cy += data[regionIndices[i] * 3 + 1];
                    }
                    cx /= count; cy /= count;

                    float maxDist = 0.0f;
                    for (int i = 0; i < count; i++) {
                        float dx = data[regionIndices[i] * 3 + 0] - cx;
                        float dy = data[regionIndices[i] * 3 + 1] - cy;
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist > maxDist) maxDist = dist;
                    }
                    float radius = maxDist * 1.6f + 0.01f; // pad past the outermost point for soft falloff

                    for (int i = 0; i < count; i++) {
                        int idx = regionIndices[i];
                        float dx = data[idx * 3 + 0] - cx;
                        float dy = data[idx * 3 + 1] - cy;
                        float normDist = sqrtf(dx * dx + dy * dy) / radius;
                        float alpha = 1.0f - fminf(normDist, 1.0f);
                        alpha = alpha * alpha * (3.0f - 2.0f * alpha);
                        concealerVertexAlpha[idx] = alpha * weightMultiplier;
                    }
                };

                if (gCtx.concealerStyle == 1) { // Facelift
                    bakeConcealerRegion(FACELIFT_OUTER_EYE_LEFT_INDICES, sizeof(FACELIFT_OUTER_EYE_LEFT_INDICES) / sizeof(FACELIFT_OUTER_EYE_LEFT_INDICES[0]), 1.0f);
                    bakeConcealerRegion(FACELIFT_OUTER_EYE_RIGHT_INDICES, sizeof(FACELIFT_OUTER_EYE_RIGHT_INDICES) / sizeof(FACELIFT_OUTER_EYE_RIGHT_INDICES[0]), 1.0f);
                    bakeConcealerRegion(FACELIFT_MOUTH_LEFT_INDICES, sizeof(FACELIFT_MOUTH_LEFT_INDICES) / sizeof(FACELIFT_MOUTH_LEFT_INDICES[0]), 0.6f);
                    bakeConcealerRegion(FACELIFT_MOUTH_RIGHT_INDICES, sizeof(FACELIFT_MOUTH_RIGHT_INDICES) / sizeof(FACELIFT_MOUTH_RIGHT_INDICES[0]), 0.6f);
                } else {
                    bakeConcealerRegion(UNDER_EYE_LEFT_INDICES, sizeof(UNDER_EYE_LEFT_INDICES) / sizeof(UNDER_EYE_LEFT_INDICES[0]), 1.0f);
                    bakeConcealerRegion(UNDER_EYE_RIGHT_INDICES, sizeof(UNDER_EYE_RIGHT_INDICES) / sizeof(UNDER_EYE_RIGHT_INDICES[0]), 1.0f);
                    bakeConcealerRegion(NASOLABIAL_FOLD_LEFT_INDICES, sizeof(NASOLABIAL_FOLD_LEFT_INDICES) / sizeof(NASOLABIAL_FOLD_LEFT_INDICES[0]), 0.6f);
                    bakeConcealerRegion(NASOLABIAL_FOLD_RIGHT_INDICES, sizeof(NASOLABIAL_FOLD_RIGHT_INDICES) / sizeof(NASOLABIAL_FOLD_RIGHT_INDICES[0]), 0.6f);
                }

                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, concealerVertexAlpha);
                glEnableVertexAttribArray(gCtx.mkAlphaHandle);
                glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);
            }
            glDisableVertexAttribArray(gCtx.mkPositionHandle);
            glDisableVertexAttribArray(gCtx.mkAlphaHandle);

            // Release JNI array
            env->ReleaseFloatArrayElements(landmarks, data, JNI_ABORT);
        }
    }
    
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    // --- PASS 2: Blur The Mask ---
    glBindFramebuffer(GL_FRAMEBUFFER, gCtx.maskBlurFbo.fbo);
    glViewport(0, 0, gCtx.width, gCtx.height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gCtx.blurProgram);
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gCtx.maskFbo.texture);
    glUniform1i(gCtx.blurSamplerHandle, 0);
    
    // This single blur pass smooths BOTH the jagged hard-hole edges (R channel: eyes/
    // lips/eyebrows cutouts, which need real smoothing) AND the already-smooth
    // per-vertex weight gradients (G/B/A: contour/blush/highlight, which do NOT need
    // much smoothing since they're interpolated across a dense 468-point mesh).
    // The previous 15px base (x4 taps = ~60px effective spread) was tuned only for
    // the R-channel holes, but since all 4 channels share this one pass, it was also
    // heavily diluting the narrow contour/blush/highlight bands toward zero — which is
    // why raising their opacity sliders barely changed anything visually. A much
    // smaller radius still smooths the hole edges adequately without washing out the
    // localized makeup weight signals.
    glUniform2f(gCtx.blurTexelSizeHandle, 3.0f / gCtx.width, 3.0f / gCtx.height);

    glVertexAttribPointer(gCtx.blurPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, QUAD_VERTICES);
    glEnableVertexAttribArray(gCtx.blurPositionHandle);
    glVertexAttribPointer(gCtx.blurTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0, FBO_TEX_COORDS);
    glEnableVertexAttribArray(gCtx.blurTexCoordHandle);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glDisableVertexAttribArray(gCtx.blurPositionHandle);
    glDisableVertexAttribArray(gCtx.blurTexCoordHandle);

    // --- PASS 3: Apply Foundation & Render to Screen ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, gCtx.width, gCtx.height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gCtx.foundationProgram);

    // Bind Camera FBO texture to Texture Unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gCtx.mainFbo.texture);
    glUniform1i(gCtx.fndCameraTexHandle, 0);

    // Bind Mask Blur FBO texture to Texture Unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gCtx.maskBlurFbo.texture);
    glUniform1i(gCtx.fndMaskTexHandle, 1);

    // Bind Lip Mask FBO texture to Texture Unit 2
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gCtx.lipMaskFbo.texture);
    glUniform1i(gCtx.fndLipMaskTexHandle, 2);

    // Bind Eye Mask FBO texture to Texture Unit 3
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gCtx.eyeMaskFbo.texture);
    glUniform1i(gCtx.fndEyeMaskTexHandle, 3);

    // Bind Concealer Mask FBO texture to Texture Unit 4
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, gCtx.concealerMaskFbo.texture);
    glUniform1i(gCtx.fndConcealerMaskTexHandle, 4);

    glUniform2f(gCtx.fndScaleHandle, gCtx.scaleX, gCtx.scaleY);
    glUniform2f(gCtx.fndOffsetHandle, gCtx.offsetX, gCtx.offsetY);
    glUniform4f(gCtx.fndBoundsHandle, gCtx.faceCenterX, gCtx.faceCenterY, gCtx.faceRadiusX, gCtx.faceRadiusY);

    glUniform2f(gCtx.fndTexelSizeHandle, 1.0f / gCtx.width, 1.0f / gCtx.height);
    glUniform1f(gCtx.fndBlurRadiusHandle, gCtx.foundationBlurRadius);
    glUniform4fv(gCtx.fndColorHandle, 1, gCtx.foundationColor);
    glUniform1i(gCtx.fndTypeHandle, gCtx.foundationType);
    glUniform4fv(gCtx.fndContourColorHandle, 1, gCtx.contourColor);
    glUniform4fv(gCtx.fndHighlightColorHandle, 1, gCtx.highlightColor);
    glUniform4fv(gCtx.fndBlushColorHandle, 1, gCtx.blushColor);
    glUniform4fv(gCtx.fndEyeshadowColorHandle, 1, gCtx.eyeshadowColor);
    glUniform4fv(gCtx.fndLipstickColorHandle, 1, gCtx.lipstickColor);
    glUniform1i(gCtx.fndLipstickFinishHandle, gCtx.lipstickFinish);
    glUniform1f(gCtx.fndLipstickGlossinessHandle, gCtx.lipstickGlossiness);
    glUniform4fv(gCtx.fndConcealerColorHandle, 1, gCtx.concealerColor);
    glUniform1i(gCtx.fndConcealerStyleHandle, gCtx.concealerStyle);
    glUniform1f(gCtx.fndAmbientCctHandle, gCtx.ambientCctKelvin);
    glUniform1f(gCtx.fndAmbientIntensityHandle, gCtx.ambientIntensity);
    glUniform1f(gCtx.fndShowMakeupHandle, gCtx.showMakeup);

    glVertexAttribPointer(gCtx.fndPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, QUAD_VERTICES);
    glEnableVertexAttribArray(gCtx.fndPositionHandle);
    glVertexAttribPointer(gCtx.fndTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0, FBO_TEX_COORDS);
    glEnableVertexAttribArray(gCtx.fndTexCoordHandle);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(gCtx.fndPositionHandle);
    glDisableVertexAttribArray(gCtx.fndTexCoordHandle);
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetMakeup(JNIEnv* env, jclass clazz, jint regionType, jfloat r, jfloat g, jfloat b, jfloat a) {
    if (regionType == 1) { // Blush
        gCtx.blushColor[0] = r; gCtx.blushColor[1] = g; gCtx.blushColor[2] = b; gCtx.blushColor[3] = a;
    } else if (regionType == 2) { // Foundation
        gCtx.foundationColor[0] = r; gCtx.foundationColor[1] = g; gCtx.foundationColor[2] = b; gCtx.foundationColor[3] = a;
    } else if (regionType == 0) { // Lipstick
        gCtx.lipstickColor[0] = r; gCtx.lipstickColor[1] = g; gCtx.lipstickColor[2] = b; gCtx.lipstickColor[3] = a;
    } else if (regionType == 3) { // Eyeshadow
        gCtx.eyeshadowColor[0] = r; gCtx.eyeshadowColor[1] = g; gCtx.eyeshadowColor[2] = b; gCtx.eyeshadowColor[3] = a;
    } else if (regionType == 4) { // Contour
        gCtx.contourColor[0] = r; gCtx.contourColor[1] = g; gCtx.contourColor[2] = b; gCtx.contourColor[3] = a;
    } else if (regionType == 5) { // Highlight
        gCtx.highlightColor[0] = r; gCtx.highlightColor[1] = g; gCtx.highlightColor[2] = b; gCtx.highlightColor[3] = a;
    } else if (regionType == 6) { // Concealer
        gCtx.concealerColor[0] = r; gCtx.concealerColor[1] = g; gCtx.concealerColor[2] = b; gCtx.concealerColor[3] = a;
    }
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetFoundationBlur(JNIEnv* env, jclass clazz, jfloat radius) {
    gCtx.foundationBlurRadius = radius;
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetLipstickGlossiness(JNIEnv* env, jclass clazz, jfloat value) {
    gCtx.lipstickGlossiness = value;
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetAmbientLighting(JNIEnv* env, jclass clazz, jfloat cctKelvin, jfloat intensity) {
    gCtx.ambientCctKelvin = cctKelvin;
    gCtx.ambientIntensity = intensity;
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetShowMakeup(JNIEnv* env, jclass clazz, jfloat value) {
    gCtx.showMakeup = value;
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetConcealerStyle(JNIEnv* env, jclass clazz, jint style) {
    gCtx.concealerStyle = style;
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetMakeupStyle(JNIEnv* env, jclass clazz, jint regionType, jint style) {
    if (regionType == 0) { // Lipstick Finish (0=matte, 1=satin, 2=glossy, 3=sheer, 4=shimmer)
        gCtx.lipstickFinish = style;
    } else if (regionType == 1) { // Blush Style
        // 0 = Apple of Cheek, 1 = Draped/Lifting, 2 = Sun-kissed horizontal.
        // Consumed live in the BLUSH weight-baking pass (nativeDrawSyncFrame).
        gCtx.blushStyle = style;
    } else if (regionType == 2) { // Foundation Type
        gCtx.foundationType = style;
    } else if (regionType == 4) { // Contour Style (0=Normal, 1=Slim, 2=Pinch, 3=Straighten)
        // Contour AND highlight shape are both derived live every frame from this
        // single field (see the CONTOUR/HIGHLIGHTER weight-baking pass in
        // nativeDrawSyncFrame) — there is no separate highlight-style state left to
        // fall out of sync with it.
        gCtx.contourStyle = style;
    }
}

} // extern "C"
