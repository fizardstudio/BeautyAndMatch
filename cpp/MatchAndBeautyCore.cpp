#include "MatchAndBeautyCore.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace match_and_beauty {

    MatchAndBeautyCore::MatchAndBeautyCore() {}
    MatchAndBeautyCore::~MatchAndBeautyCore() {}

    // Helper for calculating Euclidean distance between two 3D landmarks
    float calculateDistance(const Landmark& a, const Landmark& b) {
        return std::sqrt(
            std::pow(a.x - b.x, 2) +
            std::pow(a.y - b.y, 2) +
            std::pow(a.z - b.z, 2)
        );
    }

    DiagnosticsResult MatchAndBeautyCore::analyzeMorphology(const std::vector<Landmark>& landmarks) {
        DiagnosticsResult result;
        
        // Safety check: MediaPipe face mesh landmarks count is usually 468 or 478.
        if (landmarks.size() < 468) {
            result.faceShape = "Unknown";
            result.eyeShape = "Unknown";
            result.noseShape = "Unknown";
            return result;
        }

        // Face Shape
        float jawWidth = calculateDistance(landmarks[172], landmarks[397]);
        float cheekboneWidth = calculateDistance(landmarks[234], landmarks[454]);
        float faceLength = calculateDistance(landmarks[10], landmarks[152]);
        
        float width = std::max(jawWidth, cheekboneWidth);
        
        if (faceLength / width > 1.25f) {
            result.faceShape = "Oblong";
        } else if (width / faceLength > 0.85f) {
            result.faceShape = "Round";
        } else {
            result.faceShape = "Square";
        }

        // Eye Shape & Canthal Tilt
        float leftEyeH = calculateDistance(landmarks[33], landmarks[133]);
        float leftEyeV = calculateDistance(landmarks[159], landmarks[145]);
        float rightEyeH = calculateDistance(landmarks[362], landmarks[263]);
        float rightEyeV = calculateDistance(landmarks[386], landmarks[374]);
        
        float leftEAR = leftEyeV / (leftEyeH > 0.0001f ? leftEyeH : 0.0001f);
        float rightEAR = rightEyeV / (rightEyeH > 0.0001f ? rightEyeH : 0.0001f);
        float ear = (leftEAR + rightEAR) / 2.0f;
        
        if (ear < 0.2f) {
            result.eyeShape = "Monolid";
        } else if (ear >= 0.2f && ear <= 0.28f) {
            result.eyeShape = "Hooded";
        } else {
            result.eyeShape = "Normal";
        }

        // Calculate Canthal Tilt (Angle)
        // Using Cartesian coordinates (y increases upwards for slope calculation)
        // MediaPipe Y increases downwards, so we use (inner.y - outer.y)
        float dyLeft = landmarks[133].y - landmarks[33].y;
        float dxLeft = std::abs(landmarks[133].x - landmarks[33].x);
        float tiltLeft = std::atan2(dyLeft, dxLeft) * 180.0f / M_PI;

        float dyRight = landmarks[362].y - landmarks[263].y;
        float dxRight = std::abs(landmarks[362].x - landmarks[263].x);
        float tiltRight = std::atan2(dyRight, dxRight) * 180.0f / M_PI;

        float avgTilt = (tiltLeft + tiltRight) / 2.0f;
        if (avgTilt < 0.0f) {
            result.eyeShape = "Downturned"; // Overrides EAR based shape if downturned
        }

        // Nose Shape
        float alarBaseWidth = calculateDistance(landmarks[102], landmarks[331]);
        float intercanthalDistance = calculateDistance(landmarks[133], landmarks[362]);
        float noseLength = calculateDistance(landmarks[168], landmarks[2]);
        float noseOffsetX = std::abs(landmarks[168].x - landmarks[2].x);

        if (alarBaseWidth > intercanthalDistance) {
            result.noseShape = "Wide";
        } else if (noseOffsetX > noseLength * 0.05f) {
            result.noseShape = "Crooked";
        } else {
            result.noseShape = "Normal";
        }

        result.jawWidth = jawWidth;
        result.faceLength = faceLength;
        result.canthalTilt = avgTilt;
        result.eyeAspectRatio = ear;
        result.alarBaseWidth = alarBaseWidth;
        result.intercanthalDistance = intercanthalDistance;

        return result;
    }

}

#ifdef __ANDROID__
#include <jni.h>

extern "C"
JNIEXPORT jfloatArray JNICALL
Java_com_matchandbeauty_MediaPipeFrameProcessorPlugin_nativeAnalyzeMorphology(JNIEnv* env, jobject thiz, jfloatArray landmarksArray) {
    jfloat* elements = env->GetFloatArrayElements(landmarksArray, nullptr);
    jsize len = env->GetArrayLength(landmarksArray);
    
    std::vector<match_and_beauty::Landmark> cppLandmarks;
    for (int i = 0; i < len; i += 3) {
        cppLandmarks.push_back({elements[i], elements[i+1], elements[i+2]});
    }
    
    env->ReleaseFloatArrayElements(landmarksArray, elements, JNI_ABORT);
    
    match_and_beauty::MatchAndBeautyCore core;
    match_and_beauty::DiagnosticsResult result = core.analyzeMorphology(cppLandmarks);
    
    float outData[9];
    
    if (result.faceShape == "Round") outData[0] = 0.0f;
    else if (result.faceShape == "Oblong") outData[0] = 1.0f;
    else outData[0] = 2.0f; // Square
    
    if (result.eyeShape == "Downturned") outData[1] = 0.0f;
    else if (result.eyeShape == "Monolid") outData[1] = 1.0f;
    else if (result.eyeShape == "Hooded") outData[1] = 2.0f;
    else outData[1] = 3.0f; // Normal
    
    if (result.noseShape == "Wide") outData[2] = 0.0f;
    else if (result.noseShape == "Crooked") outData[2] = 1.0f;
    else outData[2] = 2.0f; // Normal
    
    outData[3] = result.jawWidth;
    outData[4] = result.faceLength;
    outData[5] = result.canthalTilt;
    outData[6] = result.eyeAspectRatio;
    outData[7] = result.alarBaseWidth;
    outData[8] = result.intercanthalDistance;
    
    jfloatArray outJniArray = env->NewFloatArray(9);
    env->SetFloatArrayRegion(outJniArray, 0, 9, outData);
    return outJniArray;
}
#endif

