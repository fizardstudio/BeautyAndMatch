export const foundationShaderCode = `
uniform shader image;
uniform float blurRadius;

/**
 * Flawless Foundation (Frequency Separation / Smoothing Shader)
 * 
 * Implements a 5-tap bilateral filter to smooth low-frequency (colors) while
 * preserving edges (eyes, lips, outline). It recombines the smoothed color
 * with the high-frequency texture (pores, details) extracted from a 5-tap
 * Gaussian blur, using a soft YCbCr-based skin tone mask.
 */
half4 main(float2 pos) {
    half4 center = image.eval(pos);
    
    // If blurRadius is zero or negative, return the original image
    if (blurRadius <= 0.0) {
        return center;
    }

    // Define 5-tap cross offsets
    float2 offsetLeft  = float2(-blurRadius, 0.0);
    float2 offsetRight = float2(blurRadius, 0.0);
    float2 offsetUp    = float2(0.0, -blurRadius);
    float2 offsetDown  = float2(0.0, blurRadius);

    // Sample neighboring pixels
    half4 c0 = image.eval(pos + offsetLeft);
    half4 c1 = image.eval(pos + offsetRight);
    half4 c2 = image.eval(pos + offsetUp);
    half4 c3 = image.eval(pos + offsetDown);

    // 1. 5-Tap Gaussian Blur (Low-Frequency color base)
    // Gaussian weights: center = 0.25, neighbors = 0.1875 each (sum = 1.0)
    half4 gaussian = center * 0.25 + (c0 + c1 + c2 + c3) * 0.1875;

    // 2. 5-Tap Bilateral Filter (Edge-preserving low-frequency color)
    // sigmaColor controls the color distance threshold for smoothing
    float sigmaColor = 0.15;
    float doubleSigmaSq = 2.0 * sigmaColor * sigmaColor;

    // Calculate range weights based on Euclidean distance in RGB color space
    float w0 = exp(-dot(c0.rgb - center.rgb, c0.rgb - center.rgb) / doubleSigmaSq);
    float w1 = exp(-dot(c1.rgb - center.rgb, c1.rgb - center.rgb) / doubleSigmaSq);
    float w2 = exp(-dot(c2.rgb - center.rgb, c2.rgb - center.rgb) / doubleSigmaSq);
    float w3 = exp(-dot(c3.rgb - center.rgb, c3.rgb - center.rgb) / doubleSigmaSq);

    // Normalize weights and compute the bilateral color
    half4 bilateral = (center + c0 * half(w0) + c1 * half(w1) + c2 * half(w2) + c3 * half(w3)) / 
                      half(1.0 + w0 + w1 + w2 + w3);

    // 3. Extract High-Frequency Detail (Pores, fine textures)
    // High-frequency detail = Original image - Gaussian blur (colors removed)
    half3 highFreq = center.rgb - gaussian.rgb;

    // 4. Soft Skin Tone Detection in YCbCr space
    // Converts RGB to YCbCr to isolate typical human skin color ranges
    float r = float(center.r);
    float g = float(center.g);
    float b = float(center.b);

    float cb = -0.168736 * r - 0.331264 * g + 0.5 * b + 0.5;
    float cr = 0.5 * r - 0.418688 * g - 0.081312 * b + 0.5;

    // Distances to target skin tone center (Cb: 0.40, Cr: 0.60)
    float cbDist = cb - 0.40;
    float crDist = cr - 0.60;
    float dist = length(float2(cbDist, crDist));

    // Smoothstep creates a soft transition (radius 0.08 to 0.12)
    float skinMask = smoothstep(0.12, 0.08, dist);

    // 5. Recombine: Bilateral (smooth color) + High-Frequency (pore texture)
    half3 smoothed = bilateral.rgb + highFreq;
    smoothed = clamp(smoothed, 0.0, 1.0);

    // Blend the smoothed skin only in the detected skin areas
    half3 finalColor = mix(center.rgb, smoothed, half(skinMask));

    return half4(finalColor, center.a);
}
`;

export const contourShaderCode = `
uniform shader image;
uniform vec4 contourColor;
uniform float intensity;

/**
 * Contouring & Highlighting Shader
 * 
 * Computes overlay, multiply, and linear burn blending for dark contour areas,
 * and screen / linear dodge (additive) blending for highlight areas.
 * Automatically switches modes based on the luminance of 'contourColor'.
 */
half4 main(float2 pos) {
    half4 base = image.eval(pos);
    
    // 1. Shadow Blend Modes (For dark contouring areas)
    // Multiply: simple color multiplication
    half3 multiplyBlend = base.rgb * contourColor.rgb;
    
    // Linear Burn: Subtracts the inverse of the contour color
    half3 burnBlend = clamp(base.rgb + contourColor.rgb - half3(1.0), 0.0, 1.0);
    
    // Overlay: preserves highlights and shadows of base image
    half3 cond = step(half3(0.5), base.rgb);
    half3 overlayBlend = mix(
        2.0 * base.rgb * contourColor.rgb, 
        half3(1.0) - 2.0 * (half3(1.0) - base.rgb) * (half3(1.0) - contourColor.rgb), 
        cond
    );
    
    // 2. Highlight Blend Modes (For bright highlighting areas)
    // Screen: inverse multiplication
    half3 screenBlend = half3(1.0) - (half3(1.0) - base.rgb) * (half3(1.0) - contourColor.rgb);
    
    // Linear Dodge (Add): simple addition
    half3 dodgeBlend = clamp(base.rgb + contourColor.rgb, 0.0, 1.0);

    // 3. Mode Selection based on contourColor Luminance
    half luminance = dot(contourColor.rgb, half3(0.299, 0.587, 0.114));
    
    half3 blendedColor;
    if (luminance < 0.5) {
        // Shadow: combination of Overlay (40%), Multiply (36%), and Linear Burn (24%)
        half3 darkResult = mix(multiplyBlend, burnBlend, 0.4);
        blendedColor = mix(overlayBlend, darkResult, 0.6);
    } else {
        // Highlight: combination of Screen (50%) and Linear Dodge (50%)
        blendedColor = mix(screenBlend, dodgeBlend, 0.5);
    }
    
    // 4. Blend based on contourColor alpha (local mask) and global intensity
    half blendStrength = contourColor.a * intensity;
    half3 finalColor = mix(base.rgb, blendedColor, blendStrength);
    
    return half4(finalColor, base.a);
}
`;

export const lipstickShaderCode = `
uniform shader image;
uniform vec4 lipColor;
uniform float glossiness;
uniform float luminance;

/**
 * PBR Lipstick Shader
 * 
 * Implements 'Multiply' blending for the matte base and adds a specular
 * highlight layer (PBR specular reflection) for glossy finishes, simulated
 * using the camera's original luminance.
 */
half4 main(float2 pos) {
    half4 base = image.eval(pos);
    
    // 1. Matte Blend Mode (Multiply)
    // Blend the lipstick color with the base lips texture
    half3 matteColor = base.rgb * lipColor.rgb;
    
    // Apply matte lipstick based on the color's alpha channel
    half3 coloredLips = mix(base.rgb, matteColor, lipColor.a);
    
    // 2. Glossy / Specular Highlight (PBR Specular Layer)
    // Extract base luminance from the original camera frame
    half baseLuminance = dot(base.rgb, half3(0.299, 0.587, 0.114));
    
    // Extract high-intensity highlights from the lips (specular reflection sites)
    // Smoothstep creates a mask where values from 0.4 to 0.85 are mapped to [0, 1]
    half baseHighlight = smoothstep(0.4, 0.85, baseLuminance);
    
    // Glossiness determines the roughness of the specular microfacets:
    // High glossiness -> low roughness -> sharp, narrow highlight (high power exponent)
    // Low glossiness -> high roughness -> wide, soft highlight (low power exponent)
    half exponent = mix(4.0, 48.0, glossiness);
    
    // Specular intensity is proportional to glossiness and camera light intensity (luminance)
    half specIntensity = glossiness * luminance * 1.8;
    half specular = pow(baseHighlight, exponent) * specIntensity;
    
    // 3. PBR Combination (Additive Specular Reflection)
    // Specular reflection is added on top of the diffuse colored lips
    half3 finalColor = coloredLips + half3(specular);
    
    // Clamp to valid range [0, 1]
    finalColor = clamp(finalColor, 0.0, 1.0);
    
    return half4(finalColor, base.a);
}
`;

export const eyeShaderCode = `
uniform shader image;
uniform vec4 shadowColor;
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
`;
