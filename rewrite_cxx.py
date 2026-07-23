import os

cpp_code = """#include <jni.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <vector>
#include <cmath>

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

static const char* MASK_VERTEX_SHADER = R"(
    attribute vec3 aPosition;
    attribute vec4 aColor;
    uniform vec2 uScale;
    uniform vec2 uOffset;
    varying vec4 vColor;
    void main() {
        float x = (aPosition.x * 2.0 - 1.0) * uScale.x + uOffset.x;
        float y = (1.0 - aPosition.y * 2.0) * uScale.y + uOffset.y;
        gl_Position = vec4(x, y, 0.0, 1.0);
        vColor = aColor;
    }
)";

static const char* MASK_FRAGMENT_SHADER = R"(
    precision mediump float;
    varying vec4 vColor;
    void main() {
        gl_FragColor = vColor;
    }
)";

static const char* FOUNDATION_VERTEX_SHADER = R"(
    attribute vec4 aPosition;
    attribute vec2 aTexCoord;
    varying vec2 vTexCoord;
    void main() {
        gl_Position = aPosition;
        vTexCoord = aTexCoord;
    }
)";

static const char* FOUNDATION_FRAGMENT_SHADER = R"(
    precision mediump float;
    varying vec2 vTexCoord;
    uniform sampler2D sCameraTex;
    uniform sampler2D sMaskTex;
    uniform vec2 uTexelSize;
    uniform vec4 uFoundationColor;

    vec3 computeBlur(sampler2D tex, vec2 uv, vec2 texel) {
        vec3 result = vec3(0.0);
        // Simple 9-tap Gaussian-ish blur
        result += texture2D(tex, uv + vec2(-texel.x, -texel.y) * 2.0).rgb * 0.0625;
        result += texture2D(tex, uv + vec2(0.0, -texel.y) * 2.0).rgb * 0.125;
        result += texture2D(tex, uv + vec2(texel.x, -texel.y) * 2.0).rgb * 0.0625;
        result += texture2D(tex, uv + vec2(-texel.x, 0.0) * 2.0).rgb * 0.125;
        result += texture2D(tex, uv).rgb * 0.25;
        result += texture2D(tex, uv + vec2(texel.x, 0.0) * 2.0).rgb * 0.125;
        result += texture2D(tex, uv + vec2(-texel.x, texel.y) * 2.0).rgb * 0.0625;
        result += texture2D(tex, uv + vec2(0.0, texel.y) * 2.0).rgb * 0.125;
        result += texture2D(tex, uv + vec2(texel.x, texel.y) * 2.0).rgb * 0.0625;
        return result;
    }

    void main() {
        vec4 origColor = texture2D(sCameraTex, vTexCoord);
        float mask = texture2D(sMaskTex, vTexCoord).r;

        if (mask > 0.0) {
            vec3 blurred = computeBlur(sCameraTex, vTexCoord, uTexelSize);
            vec3 highFreq = origColor.rgb - blurred; // Detail extraction
            
            // Tinting the blur (Opacity based on uniform alpha)
            vec3 tintedBlur = mix(blurred, uFoundationColor.rgb, uFoundationColor.a);
            
            vec3 finalSkin = tintedBlur + highFreq;
            
            gl_FragColor = vec4(mix(origColor.rgb, finalSkin, mask), origColor.a);
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
    int width = 0;
    int height = 0;

    void release() {
        if (texture) { glDeleteTextures(1, &texture); texture = 0; }
        if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
        width = 0; height = 0;
    }

    void setup(int w, int h) {
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
    GLint maskPositionHandle, maskColorHandle, maskScaleHandle, maskOffsetHandle;

    GLuint foundationProgram;
    GLint fndPositionHandle, fndTexCoordHandle, fndCameraTexHandle, fndMaskTexHandle, fndTexelSizeHandle, fndColorHandle;

    int width = 1080;
    int height = 1920;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    FBO maskFbo;
    FBO mainFbo;

    float foundationColor[4] = {0.95f, 0.85f, 0.75f, 0.3f}; // Default Foundation Tint
};

static RendererContext gCtx;

#include "FizgravityMeshIndices.h"
#include "FizgravityMakeupIndices.h"

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
    gCtx.maskColorHandle = glGetAttribLocation(gCtx.maskProgram, "aColor");
    gCtx.maskScaleHandle = glGetUniformLocation(gCtx.maskProgram, "uScale");
    gCtx.maskOffsetHandle = glGetUniformLocation(gCtx.maskProgram, "uOffset");

    // Foundation Shader
    gCtx.foundationProgram = createProgram(FOUNDATION_VERTEX_SHADER, FOUNDATION_FRAGMENT_SHADER);
    gCtx.fndPositionHandle = glGetAttribLocation(gCtx.foundationProgram, "aPosition");
    gCtx.fndTexCoordHandle = glGetAttribLocation(gCtx.foundationProgram, "aTexCoord");
    gCtx.fndCameraTexHandle = glGetUniformLocation(gCtx.foundationProgram, "sCameraTex");
    gCtx.fndMaskTexHandle = glGetUniformLocation(gCtx.foundationProgram, "sMaskTex");
    gCtx.fndTexelSizeHandle = glGetUniformLocation(gCtx.foundationProgram, "uTexelSize");
    gCtx.fndColorHandle = glGetUniformLocation(gCtx.foundationProgram, "uFoundationColor");
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeResize(JNIEnv* env, jclass clazz, jint width, jint height) {
    LOGI("nativeResize: %d x %d", width, height);
    glViewport(0, 0, width, height);
    gCtx.width = width;
    gCtx.height = height;
    
    gCtx.maskFbo.setup(width, height);
    gCtx.mainFbo.setup(width, height);

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
    JNIEnv* env, jclass clazz, jint textureId, jobject buffer, jint width, jint height, jint rowStride, jfloatArray landmarks, jboolean isNewLandmarks) 
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

    // Update Camera Texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowStride / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

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

    // --- PASS 2: Generate Face Mask ---
    glBindFramebuffer(GL_FRAMEBUFFER, gCtx.maskFbo.fbo);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (landmarks != nullptr) {
        jsize len = env->GetArrayLength(landmarks);
        int vertexCount = len / 3;
        if (vertexCount >= 478) {
            jboolean isCopy = JNI_FALSE;
            float* data = env->GetFloatArrayElements(landmarks, &isCopy);

            std::vector<float> maskColors(vertexCount * 4);
            // Default: Face is White (Foundation area)
            for (int i = 0; i < vertexCount; i++) {
                maskColors[i*4+0] = 1.0f; maskColors[i*4+1] = 1.0f; maskColors[i*4+2] = 1.0f; maskColors[i*4+3] = 1.0f;
            }
            // Mask out holes: Eyes, Lips, Eyebrows = Black
            for (int idx : EYE_INDICES) { maskColors[idx*4+0]=0; maskColors[idx*4+1]=0; maskColors[idx*4+2]=0; maskColors[idx*4+3]=1; }
            for (int idx : LIP_INDICES) { maskColors[idx*4+0]=0; maskColors[idx*4+1]=0; maskColors[idx*4+2]=0; maskColors[idx*4+3]=1; }
            for (int idx : INNER_LIPS_INDICES) { maskColors[idx*4+0]=0; maskColors[idx*4+1]=0; maskColors[idx*4+2]=0; maskColors[idx*4+3]=1; }

            glUseProgram(gCtx.maskProgram);
            glUniform2f(gCtx.maskScaleHandle, gCtx.scaleX, gCtx.scaleY);
            glUniform2f(gCtx.maskOffsetHandle, gCtx.offsetX, gCtx.offsetY);
            glVertexAttribPointer(gCtx.maskPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, data);
            glEnableVertexAttribArray(gCtx.maskPositionHandle);
            glVertexAttribPointer(gCtx.maskColorHandle, 4, GL_FLOAT, GL_FALSE, 0, maskColors.data());
            glEnableVertexAttribArray(gCtx.maskColorHandle);

            int numIndices = sizeof(MESH_INDICES) / sizeof(MESH_INDICES[0]);
            glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);
            
            glDisableVertexAttribArray(gCtx.maskPositionHandle);
            glDisableVertexAttribArray(gCtx.maskColorHandle);
            env->ReleaseFloatArrayElements(landmarks, data, JNI_ABORT);
        }
    }

    // --- PASS 3: Apply Foundation & Render to Screen ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, gCtx.width, gCtx.height);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(gCtx.foundationProgram);

    // Bind Camera FBO texture to Texture Unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gCtx.mainFbo.texture);
    glUniform1i(gCtx.fndCameraTexHandle, 0);

    // Bind Mask FBO texture to Texture Unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gCtx.maskFbo.texture);
    glUniform1i(gCtx.fndMaskTexHandle, 1);

    glUniform2f(gCtx.fndTexelSizeHandle, 1.0f / gCtx.width, 1.0f / gCtx.height);
    glUniform4fv(gCtx.fndColorHandle, 1, gCtx.foundationColor);

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
    if (regionType == 2) { // Foundation
        gCtx.foundationColor[0] = r; gCtx.foundationColor[1] = g; gCtx.foundationColor[2] = b; gCtx.foundationColor[3] = a;
    }
}

} // extern "C"
"""
with open("cpp/FizgravityRenderer.cpp", "w") as f:
    f.write(cpp_code)
print("FizgravityRenderer.cpp successfully updated.")
