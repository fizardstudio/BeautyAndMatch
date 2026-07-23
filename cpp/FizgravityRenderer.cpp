#include <jni.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl3.h>
#include <android/log.h>
#include <vector>
#include <cmath>

#define LOG_TAG "FizgravityRenderer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

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

static const char* MESH_VERTEX_SHADER = R"(
    attribute vec3 aPosition;
    attribute vec4 aColor;
    uniform vec2 uScale;
    uniform vec2 uOffset;
    varying vec4 vColor;
    void main() {
        float x = (aPosition.x * 2.0 - 1.0) * uScale.x + uOffset.x;
        float y = (1.0 - aPosition.y * 2.0) * uScale.y + uOffset.y; // Flip Y for GL
        gl_Position = vec4(x, y, 0.0, 1.0);
        vColor = aColor;
    }
)";

static const char* MESH_FRAGMENT_SHADER = R"(
    precision mediump float;
    varying vec4 vColor;
    void main() {
        gl_FragColor = vColor;
    }
)";

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

struct RendererContext {
    GLuint cameraProgram;
    GLint camPositionHandle;
    GLint camTexCoordHandle;
    GLint camSamplerHandle;
    GLint camScaleHandle;

    GLuint meshProgram;
    GLint meshPositionHandle;
    GLint meshColorHandle;
    GLint meshScaleHandle;
    GLint meshOffsetHandle;

    int width = 1080;
    int height = 1920;
    
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;

    // Makeup Configuration (RGBA)
    float lipColor[4] = {0,0,0,0};
    float blushColor[4] = {0,0,0,0};
    float foundationColor[4] = {0,0,0,0};
    float eyeColor[4] = {0,0,0,0};
    float contourColor[4] = {0,0,0,0};
};

static RendererContext gCtx;

#include "FizgravityMeshIndices.h"
#include "FizgravityMakeupIndices.h"

#include "FizgravityMakeupIndices.h"

extern "C" {

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeInitGL(JNIEnv* env, jclass clazz) {
    LOGI("nativeInitGL called");
    gCtx.cameraProgram = createProgram(CAMERA_VERTEX_SHADER, CAMERA_FRAGMENT_SHADER);
    gCtx.camPositionHandle = glGetAttribLocation(gCtx.cameraProgram, "aPosition");
    gCtx.camTexCoordHandle = glGetAttribLocation(gCtx.cameraProgram, "aTexCoord");
    gCtx.camScaleHandle = glGetUniformLocation(gCtx.cameraProgram, "uScale");
    gCtx.camSamplerHandle = glGetUniformLocation(gCtx.cameraProgram, "sTexture");

    gCtx.meshProgram = createProgram(MESH_VERTEX_SHADER, MESH_FRAGMENT_SHADER);
    gCtx.meshPositionHandle = glGetAttribLocation(gCtx.meshProgram, "aPosition");
    gCtx.meshColorHandle = glGetAttribLocation(gCtx.meshProgram, "aColor");
    gCtx.meshScaleHandle = glGetUniformLocation(gCtx.meshProgram, "uScale");
    gCtx.meshOffsetHandle = glGetUniformLocation(gCtx.meshProgram, "uOffset");
    
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeResize(JNIEnv* env, jclass clazz, jint width, jint height) {
    LOGI("nativeResize: %d x %d", width, height);
    glViewport(0, 0, width, height);
    gCtx.width = width;
    gCtx.height = height;

    float screenAspect = (float)height / (float)width;
    float cameraAspect = 16.0f / 9.0f; // Typical portrait camera aspect

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
    if (!pixels) {
        LOGE("nativeDrawSyncFrame: Failed to get direct buffer address");
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (gCtx.width <= 0 || gCtx.height <= 0 || width <= 0 || height <= 0) return;

    // Calculate dynamic aspect ratio to prevent stretching
    float screenAspect = (float)gCtx.height / (float)gCtx.width;
    // Note: buffer width/height from ImageProxy might be rotated.
    // If it's portrait, height > width.
    float cameraAspect = (float)height / (float)width;
    if (width > height) { // Usually ImageProxy returns raw sensor size (landscape)
        cameraAspect = (float)width / (float)height; 
    }

    if (screenAspect > cameraAspect) {
        gCtx.scaleX = screenAspect / cameraAspect;
        gCtx.scaleY = 1.0f;
    } else {
        gCtx.scaleX = 1.0f;
        gCtx.scaleY = cameraAspect / screenAspect;
    }

    // 1. Upload Camera Texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    
    // GLES3 is required for GL_UNPACK_ROW_LENGTH
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowStride / 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 2. Draw Camera Texture
    glUseProgram(gCtx.cameraProgram);

    const float VERTICES[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    
    // Hardcoded 90-degree CCW rotation + Mirror to match ImageAnalysis Buffer perfectly
    const float TEX_COORDS[] = {
        0.0f, 1.0f, // V1: Bottom-Left
        0.0f, 0.0f, // V2: Bottom-Right
        1.0f, 1.0f, // V3: Top-Left
        1.0f, 0.0f  // V4: Top-Right
    };

    glVertexAttribPointer(gCtx.camPositionHandle, 2, GL_FLOAT, GL_FALSE, 0, VERTICES);
    glEnableVertexAttribArray(gCtx.camPositionHandle);

    glVertexAttribPointer(gCtx.camTexCoordHandle, 2, GL_FLOAT, GL_FALSE, 0, TEX_COORDS);
    glEnableVertexAttribArray(gCtx.camTexCoordHandle);

    glUniform2f(gCtx.camScaleHandle, gCtx.scaleX, gCtx.scaleY);
    glUniform1i(gCtx.camSamplerHandle, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(gCtx.camPositionHandle);
    glDisableVertexAttribArray(gCtx.camTexCoordHandle);

    // 3. Draw AR Makeup
    if (landmarks != nullptr) {
        jsize len = env->GetArrayLength(landmarks);
        if (len >= 468 * 3) {
            jboolean isCopy = JNI_FALSE;
            float* data = env->GetFloatArrayElements(landmarks, &isCopy);

            // Populate Vertex Colors
            float vertexColors[468 * 4];
            
            // Base layer: Foundation
            for (int i = 0; i < 468; i++) {
                vertexColors[i*4 + 0] = gCtx.foundationColor[0];
                vertexColors[i*4 + 1] = gCtx.foundationColor[1];
                vertexColors[i*4 + 2] = gCtx.foundationColor[2];
                vertexColors[i*4 + 3] = gCtx.foundationColor[3];
            }
            
            // Clear Foundation from Lips & Eyes
            for (int idx : LIP_INDICES) { vertexColors[idx*4 + 3] = 0.0f; }
            for (int idx : EYE_INDICES) { vertexColors[idx*4 + 3] = 0.0f; }

            // Contour
            for (int idx : CONTOUR_INDICES) {
                if (gCtx.contourColor[3] > 0.0f) {
                    vertexColors[idx*4 + 0] = gCtx.contourColor[0];
                    vertexColors[idx*4 + 1] = gCtx.contourColor[1];
                    vertexColors[idx*4 + 2] = gCtx.contourColor[2];
                    vertexColors[idx*4 + 3] = gCtx.contourColor[3];
                }
            }

            // Blush
            for (int idx : BLUSH_INDICES) {
                if (gCtx.blushColor[3] > 0.0f) {
                    vertexColors[idx*4 + 0] = gCtx.blushColor[0];
                    vertexColors[idx*4 + 1] = gCtx.blushColor[1];
                    vertexColors[idx*4 + 2] = gCtx.blushColor[2];
                    vertexColors[idx*4 + 3] = gCtx.blushColor[3];
                }
            }

            // Eyeshadow
            for (int idx : EYE_INDICES) {
                if (gCtx.eyeColor[3] > 0.0f) {
                    vertexColors[idx*4 + 0] = gCtx.eyeColor[0];
                    vertexColors[idx*4 + 1] = gCtx.eyeColor[1];
                    vertexColors[idx*4 + 2] = gCtx.eyeColor[2];
                    vertexColors[idx*4 + 3] = gCtx.eyeColor[3];
                }
            }

            // Lipstick
            for (int idx : LIP_INDICES) {
                if (gCtx.lipColor[3] > 0.0f) {
                    vertexColors[idx*4 + 0] = gCtx.lipColor[0];
                    vertexColors[idx*4 + 1] = gCtx.lipColor[1];
                    vertexColors[idx*4 + 2] = gCtx.lipColor[2];
                    vertexColors[idx*4 + 3] = gCtx.lipColor[3];
                }
            }

            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glUseProgram(gCtx.meshProgram);
            
            glUniform2f(gCtx.meshScaleHandle, gCtx.scaleX, gCtx.scaleY);
            glUniform2f(gCtx.meshOffsetHandle, gCtx.offsetX, gCtx.offsetY);

            glVertexAttribPointer(gCtx.meshPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, data);
            glEnableVertexAttribArray(gCtx.meshPositionHandle);

            glVertexAttribPointer(gCtx.meshColorHandle, 4, GL_FLOAT, GL_FALSE, 0, vertexColors);
            glEnableVertexAttribArray(gCtx.meshColorHandle);

            int numIndices = sizeof(MESH_INDICES) / sizeof(MESH_INDICES[0]);
            glDrawElements(GL_TRIANGLES, numIndices, GL_UNSIGNED_SHORT, MESH_INDICES);
            
            glDisableVertexAttribArray(gCtx.meshPositionHandle);
            glDisableVertexAttribArray(gCtx.meshColorHandle);
            glDisable(GL_BLEND);
        }
    }
}

JNIEXPORT void JNICALL
Java_com_matchandbeauty_FizgravityRenderer_nativeSetMakeup(JNIEnv* env, jclass clazz, jint regionType, jfloat r, jfloat g, jfloat b, jfloat a) {
    switch (regionType) {
        case 0: // Lips
            gCtx.lipColor[0] = r; gCtx.lipColor[1] = g; gCtx.lipColor[2] = b; gCtx.lipColor[3] = a; break;
        case 1: // Blush
            gCtx.blushColor[0] = r; gCtx.blushColor[1] = g; gCtx.blushColor[2] = b; gCtx.blushColor[3] = a; break;
        case 2: // Foundation
            gCtx.foundationColor[0] = r; gCtx.foundationColor[1] = g; gCtx.foundationColor[2] = b; gCtx.foundationColor[3] = a; break;
        case 3: // Eye
            gCtx.eyeColor[0] = r; gCtx.eyeColor[1] = g; gCtx.eyeColor[2] = b; gCtx.eyeColor[3] = a; break;
        case 4: // Contour
            gCtx.contourColor[0] = r; gCtx.contourColor[1] = g; gCtx.contourColor[2] = b; gCtx.contourColor[3] = a; break;
    }
}

} // extern "C"
