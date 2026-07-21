uniform shader image;
uniform half4 contourColor;
uniform half intensity;

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
