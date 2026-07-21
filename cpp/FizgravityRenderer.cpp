#include <jni.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
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
    #extension GL_OES_EGL_image_external : require
    precision mediump float;
    varying vec2 vTexCoord;
    uniform samplerExternalOES sTexture;
    void main() {
        gl_FragColor = texture2D(sTexture, vTexCoord);
    }
)";

static const char* MESH_VERTEX_SHADER = R"(
    attribute vec3 aPosition;
    uniform vec2 uScale;
    uniform vec2 uOffset;
    void main() {
        float x = (aPosition.x * 2.0 - 1.0) * uScale.x + uOffset.x;
        float y = (1.0 - aPosition.y * 2.0) * uScale.y + uOffset.y; // Flip Y for GL
        gl_Position = vec4(x, y, 0.0, 1.0);
        gl_PointSize = 4.0;
    }
)";

static const char* MESH_FRAGMENT_SHADER = R"(
    precision mediump float;
    void main() {
        gl_FragColor = vec4(0.0, 1.0, 0.5, 1.0);
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
    GLint meshScaleHandle;
    GLint meshOffsetHandle;

    int width = 1080;
    int height = 1920;
    
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

static RendererContext gCtx;

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
Java_com_matchandbeauty_FizgravityRenderer_nativeDrawFrame(
    JNIEnv* env, jclass clazz, jint textureId, jfloatArray landmarks, jfloatArray matrix) 
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gCtx.cameraProgram);

    const float VERTICES[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    
    // Hardcoded 90-degree CCW rotation + Mirror to match MediaPipe FaceLandmarker logic perfectly
    // Maps Top of Head (X=1) to Top of Screen (V3, V4)
    // Maps Left of Face (Y=1) to Left of Screen (V1, V3) for MIRRORING
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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, textureId);
    glUniform1i(gCtx.camSamplerHandle, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(gCtx.camPositionHandle);
    glDisableVertexAttribArray(gCtx.camTexCoordHandle);

    if (landmarks != nullptr) {
        jsize len = env->GetArrayLength(landmarks);
        if (len >= 468 * 3) {
            jboolean isCopy = JNI_FALSE;
            float* data = env->GetFloatArrayElements(landmarks, &isCopy);

            glUseProgram(gCtx.meshProgram);
            
            glUniform2f(gCtx.meshScaleHandle, gCtx.scaleX, gCtx.scaleY);
            glUniform2f(gCtx.meshOffsetHandle, gCtx.offsetX, gCtx.offsetY);

            glVertexAttribPointer(gCtx.meshPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, data);
            glEnableVertexAttribArray(gCtx.meshPositionHandle);

            glDrawArrays(GL_POINTS, 0, 468);
            
            glDisableVertexAttribArray(gCtx.meshPositionHandle);
            env->ReleaseFloatArrayElements(landmarks, data, JNI_ABORT);
        }
    }
}

} // extern "C"
