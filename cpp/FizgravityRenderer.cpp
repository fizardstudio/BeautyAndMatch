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

    // Ambient lighting estimation (from face-as-diffuse-light-probe analysis)
    uniform float uAmbientCCT;       // Kelvin
    uniform float uAmbientIntensity; // 0.0 - 1.0-ish

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

        float foundationMask = mask.r;
        float contourAlpha   = mask.g * foundationMask;
        float blushAlpha     = mask.b * foundationMask;
        float highlightAlpha = mask.a * foundationMask;
        float lipAlpha       = texture2D(sLipMaskTex, vTexCoord).r * foundationMask;
        float eyeAlpha       = texture2D(sEyeMaskTex, vTexCoord).r * foundationMask;
        float concealerAlpha = texture2D(sConcealerMaskTex, vTexCoord).r * foundationMask;

        if (foundationMask > 0.0 || contourAlpha > 0.0 || blushAlpha > 0.0 || highlightAlpha > 0.0 || lipAlpha > 0.0 || eyeAlpha > 0.0 || concealerAlpha > 0.0) {
            // Spatial radius must be large enough (>15px) to blur out pores so they are isolated in highFreq
            float activeRadius = 16.0; 
            vec3 blurred = computeBlur(sCameraTex, vTexCoord, uTexelSize, activeRadius);
            vec3 highFreq = origColor.rgb - blurred;
            
            vec3 currentSkin = origColor.rgb;
            
            // --- PURE SMOOTHING (Independent of Foundation Color) ---
            // Slider goes from 0 to 20
            float smoothIntensity = clamp(uFoundationBlurRadius / 20.0, 0.0, 1.0);
            float hfMultiplier = mix(1.0, 0.1, smoothIntensity); // Retain only 10% pores at max smooth
            vec3 smoothedSkin = blurred + highFreq * hfMultiplier;
            currentSkin = mix(currentSkin, smoothedSkin, foundationMask);
            
            // --- FOUNDATION COLOR & FINISH ---
            float skinLuma = dot(blurred, vec3(0.299, 0.587, 0.114));
            vec3 litFoundation = uFoundationColor.rgb * (skinLuma * 0.85 + 0.15);
            litFoundation *= mix(vec3(1.0), cctToTint(uAmbientCCT), clamp(uAmbientIntensity, 0.0, 1.0));

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
                vec3 sheerTarget = blendSoftLight(blurred, uFoundationColor.rgb);
                vec3 coveredSkin = mix(blurred, sheerTarget, 0.45);
                foundationEffect = coveredSkin + (highFreq * 0.95 * hfMultiplier);
            } else if (uFoundationType == 3) {
                vec3 softLightTarget = blendSoftLight(blurred, uFoundationColor.rgb);
                vec3 satinTarget = mix(litFoundation, softLightTarget, 0.5);
                foundationEffect = satinTarget + (highFreq * 0.70 * hfMultiplier);
            } else if (uFoundationType == 4) {
                vec3 overlayTarget = blendOverlay(blurred, uFoundationColor.rgb);
                vec3 luminousTarget = mix(litFoundation, overlayTarget, 0.4);
                foundationEffect = luminousTarget + (highFreq * 0.75 * hfMultiplier);
                
                vec3 fauxNormal = normalize(vec3((vFaceUV.x - 0.5) * 1.5, (vFaceUV.y - 0.5) * 1.5, 0.9));
                float NdV = max(0.0, dot(fauxNormal, vec3(0.0, 0.0, 1.0)));
                float pearlGlow = (1.0 - NdV * 0.6) * smoothstep(0.3, 0.8, skinLuma) * 0.22;
                
                vec3 adaptivePearl = mix(vec3(1.0, 0.92, 0.82), uFoundationColor.rgb, 0.35);
                foundationEffect = blendScreen(foundationEffect, adaptivePearl * pearlGlow);
            }
            
            // Final composite: blend the foundation color effect onto the currentSkin (which may already be smoothed)
            currentSkin = mix(currentSkin, foundationEffect, effectiveOpacity * foundationMask);

            // [2] CONCEALER
            float concealerStrength = concealerAlpha * uConcealerColor.a;
            if (uConcealerStyle == 2 || uConcealerStyle == 3) {
                float targetingMask = 1.0;
                if (uConcealerStyle == 2) {
                    float redness = origColor.r - (origColor.g + origColor.b * 0.5);
                    targetingMask = smoothstep(-0.05, 0.05, redness);
                }
                vec3 correctorTarget = blendSoftLight(blurred, uConcealerColor.rgb);
                currentSkin = mix(currentSkin, correctorTarget + highFreq, concealerStrength * targetingMask);
            } else {
                vec3 concealerLowFreq = mix(blurred, uConcealerColor.rgb, concealerStrength);
                currentSkin = mix(currentSkin, concealerLowFreq + highFreq, concealerStrength);
            }

            // [3] CONTOUR (Hybrid Multiply + Linear Burn)
            float contourAlphaFinal = contourAlpha * uContourColor.a;
            vec3 mulResult = currentSkin * uContourColor.rgb;
            vec3 burnResult = max(currentSkin + uContourColor.rgb - vec3(1.0), vec3(0.0));
            vec3 deepContour = mix(mulResult, burnResult, 0.4);
            float effectiveAlpha = pow(contourAlphaFinal, 0.85);
            currentSkin = mix(currentSkin, deepContour, effectiveAlpha);

            // [4] BLUSH (Hybrid Normal + Soft Light)
            float blushStrength = blushAlpha * uBlushColor.a;
            vec3 softBlush = blendSoftLight(currentSkin, uBlushColor.rgb);
            vec3 normalBlush = uBlushColor.rgb;
            vec3 pigmentedBlush = mix(softBlush, normalBlush, 0.65);
            float skinLum = dot(currentSkin, vec3(0.299, 0.587, 0.114));
            pigmentedBlush *= (skinLum * 0.4 + 0.8);
            currentSkin = mix(currentSkin, pigmentedBlush, blushStrength);

            // [5] HIGHLIGHTER (Screen)
            vec3 highlightLayer = blendScreen(currentSkin, uHighlightColor.rgb);
            currentSkin = mix(currentSkin, highlightLayer, highlightAlpha * uHighlightColor.a);

            // [7] EYESHADOW (Overlay / Normal hybrid). Region weight is re-baked every
            // frame from live landmarks (see Pass 1d), so blink compression/stretch
            // already happens via the mesh's own UV deformation — no blendshape
            // plumbing needed for this layer.
            vec3 overlayEye = blendOverlay(currentSkin, uEyeshadowColor.rgb);
            vec3 pigmentedEye = mix(overlayEye, uEyeshadowColor.rgb, 0.5);
            currentSkin = mix(currentSkin, pigmentedEye, eyeAlpha * uEyeshadowColor.a);

            // [10] LIPSTICK (Matte / Multiply)
            vec3 lipMultiply = currentSkin * uLipstickColor.rgb;
            currentSkin = mix(currentSkin, lipMultiply, lipAlpha * uLipstickColor.a);

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
    float eyeshadowColor[4] = {0.3f, 0.2f, 0.15f, 0.0f};
    float concealerColor[4] = {0.85f, 0.65f, 0.5f, 0.0f};
    
    int foundationType = 0; // 0=matte, 1=dewy, 2=sheer
    float foundationBlurRadius = 8.0f; // texel-multiplier for frequency-separation low-pass blur
    int contourStyle = 0;   // 0=normal, 1=slim, 2=pinch, 3=straighten (drives live nose contour+highlight geometry)
    int blushStyle = 0;     // 0=normal, 1=contour_45, 2=horizontal
    int concealerStyle = 0; // 0=Traditional, 1=Facelift, 2=Green, 3=Peach

    float ambientCctKelvin = 6500.0f; // Neutral D65 daylight default — safe no-op tint until real data arrives
    float ambientIntensity = 1.0f;    // Neutral (no darkening) default

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
            glVertexAttribPointer(gCtx.mkPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, data);
            glEnableVertexAttribArray(gCtx.mkPositionHandle);
            {
                static float lipVertexAlpha[468] = {0.0f};
                for (int i = 0; i < 468; i++) lipVertexAlpha[i] = 0.0f;
                const int lipCount = sizeof(LIP_INDICES) / sizeof(LIP_INDICES[0]);
                for (int i = 0; i < lipCount; i++) {
                    lipVertexAlpha[LIP_INDICES[i]] = 1.0f;
                }
                glVertexAttribPointer(gCtx.mkAlphaHandle, 1, GL_FLOAT, GL_FALSE, 0, lipVertexAlpha);
                glEnableVertexAttribArray(gCtx.mkAlphaHandle);
                glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);
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
    glUniform4fv(gCtx.fndConcealerColorHandle, 1, gCtx.concealerColor);
    glUniform1i(gCtx.fndConcealerStyleHandle, gCtx.concealerStyle);
    glUniform1f(gCtx.fndAmbientCctHandle, gCtx.ambientCctKelvin);
    glUniform1f(gCtx.fndAmbientIntensityHandle, gCtx.ambientIntensity);

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
Java_com_matchandbeauty_FizgravityRenderer_nativeSetAmbientLighting(JNIEnv* env, jclass clazz, jfloat cctKelvin, jfloat intensity) {
    gCtx.ambientCctKelvin = cctKelvin;
    gCtx.ambientIntensity = intensity;
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetConcealerStyle(JNIEnv* env, jclass clazz, jint style) {
    gCtx.concealerStyle = style;
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetMakeupStyle(JNIEnv* env, jclass clazz, jint regionType, jint style) {
    if (regionType == 1) { // Blush Style
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
