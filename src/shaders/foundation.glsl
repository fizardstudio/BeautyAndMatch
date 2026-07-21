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
