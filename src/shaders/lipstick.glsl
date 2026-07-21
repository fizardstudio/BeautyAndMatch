uniform shader image;
uniform half4 lipColor;
uniform half glossiness;
uniform half luminance;

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
