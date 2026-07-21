uniform shader image;
uniform half4 shadowColor;
uniform float feathering;
uniform float2 eyeCenter;
uniform float2 eyeSize;
uniform float eyeRotation;

/**
 * Eyeshadow & Eyeliner Shader
 * 
 * Maps coordinates to a normalized, rotated coordinate space aligned with eye landmarks.
 * Defines the upper eyelid contour using a quadratic arch and applies:
 * 1. A sharp, dark eyeliner strip along the lid arch.
 * 2. A Gaussian-feathered eyeshadow gradient above the arch.
 * Both fade out naturally at the horizontal corners of the eye shape.
 */
half4 main(float2 pos) {
    half4 base = image.eval(pos);

    // 1. Convert to normalized, rotated space aligned with the eye landmarks
    float2 d = pos - eyeCenter;
    float cosR = cos(eyeRotation);
    float sinR = sin(eyeRotation);
    
    // Prevent division by zero
    float2 size = max(eyeSize, float2(0.001));
    float2 normPos = float2(
        (d.x * cosR + d.y * sinR) / size.x,
        (-d.x * sinR + d.y * cosR) / size.y
    );

    // 2. Compute the upper eyelid arch using a parabola
    // The peak is at x = 0, y = 0.3
    float arch = 0.3 * (1.0 - normPos.x * normPos.x);
    
    // Distance relative to the arch (positive is above, negative is inside/below)
    float distAboveArch = normPos.y - arch;

    // 3. Eyeliner Effect (very thin, sharp Gaussian band along the arch)
    float eyelinerWeight = exp(-distAboveArch * distAboveArch / 0.002);
    
    // 4. Eyeshadow Effect (broad Gaussian gradient above the arch, zero inside the eye)
    float safeFeather = max(feathering, 0.01);
    float doubleFeatherSq = 2.0 * safeFeather * safeFeather;
    float eyeshadowWeight = step(0.0, distAboveArch) * exp(-distAboveArch * distAboveArch / doubleFeatherSq);

    // 5. Horizontal Fade (ensures color fades at the inner and outer corners)
    float horizFade = smoothstep(1.1, 0.5, abs(normPos.x));
    
    eyelinerWeight *= horizFade;
    eyeshadowWeight *= horizFade;

    // 6. Color blending
    half3 shadowRGB = shadowColor.rgb;
    half3 eyelinerColor = half3(0.05, 0.05, 0.05); // near-black eyeliner
    
    // Eyeliner blends into shadow
    half3 mixedShadow = mix(shadowRGB, eyelinerColor, half(eyelinerWeight * 0.9));

    // Combine eyeshadow and eyeliner weight
    half blendStrength = half(eyeshadowWeight) * shadowColor.a;
    blendStrength = clamp(blendStrength + half(eyelinerWeight * 0.8), 0.0, 1.0);
    
    // 7. Composite onto the base image
    half3 finalColor = mix(base.rgb, mixedShadow, blendStrength);

    return half4(finalColor, base.a);
}
