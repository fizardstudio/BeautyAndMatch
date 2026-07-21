#ifndef MATCH_AND_BEAUTY_CORE_H
#define MATCH_AND_BEAUTY_CORE_H

#include <vector>
#include <string>

namespace match_and_beauty {

    struct Landmark {
        float x;
        float y;
        float z;
    };

    struct DiagnosticsResult {
        std::string faceShape;  // "Round", "Square", "Oblong", etc.
        std::string eyeShape;   // "Downturned", "Monolid", "Hooded", etc.
        std::string noseShape;  // "Wide", "Drooping", "Crooked", etc.
        float jawWidth;
        float faceLength;
        float canthalTilt;
        float eyeAspectRatio;
        float alarBaseWidth;
        float intercanthalDistance;
    };

    class MatchAndBeautyCore {
    public:
        MatchAndBeautyCore();
        ~MatchAndBeautyCore();

        // Performs Euclidean calculations and returns face, eye, and nose morphology shapes
        DiagnosticsResult analyzeMorphology(const std::vector<Landmark>& landmarks);
    };

}

#endif // MATCH_AND_BEAUTY_CORE_H
