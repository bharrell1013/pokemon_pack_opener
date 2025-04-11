#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D baseTexture;    // Renamed from cardTexture for clarity
uniform sampler2D overlayTexture; // NEW: L-System Overlay
uniform int cardType;             // Keep for potential future use, but tinting might be less needed now
uniform int cardRarity;           // Keep for potential future use

// Optional: Define how much the overlay affects the base
uniform float overlayIntensity = 0.6; // Adjust for subtle effect

// Optional: Type colors if you still want some tinting
const vec3 typeColors[12] = vec3[12](
    vec3(0.8, 0.8, 0.8), vec3(1.0, 0.4, 0.2), vec3(0.2, 0.4, 1.0),
    vec3(0.4, 0.8, 0.2), vec3(1.0, 0.8, 0.0), vec3(0.8, 0.2, 0.8),
    vec3(0.8, 0.4, 0.0), vec3(0.4, 0.2, 0.4), vec3(0.6, 0.4, 1.0),
    vec3(1.0, 0.6, 0.8), vec3(0.6, 0.6, 0.8), vec3(0.4, 0.2, 0.6)
);

void main()
{
    // Sample overlay texture (L-System)
    vec4 overlayColor = texture(overlayTexture, TexCoord);

    // Sample base texture (API image)
    vec4 baseColor = texture(baseTexture, TexCoord);

    // --- Blending Logic ---
    // Option 1: Additive Blend (Makes things brighter where overlay exists)
     vec3 blendedColor = baseColor.rgb + overlayColor.rgb * overlayColor.a * overlayIntensity;

    // Option 2: Alpha Blend (Overlay drawn 'on top' based on its alpha)
    // vec3 blendedColor = mix(baseColor.rgb, overlayColor.rgb, overlayColor.a * overlayIntensity);

    // Option 3: Multiplicative Blend (Darkens/tints based on overlay)
    // vec3 blendedColor = baseColor.rgb * mix(vec3(1.0), overlayColor.rgb, overlayColor.a * overlayIntensity);

    // Option 4: Screen Blend (Brighter, good for light patterns)
     //vec3 blendedColor = vec3(1.0) - (vec3(1.0) - baseColor.rgb) * (vec3(1.0) - overlayColor.rgb * overlayIntensity);
     //blendedColor = mix(baseColor.rgb, blendedColor, overlayColor.a); // Use overlay alpha

     // --- !!! TEMPORARY: Force Usage of Uniforms !!! ---
    // Add a slight tint based on type and rarity just to ensure they are used
    // This prevents the compiler from optimizing them away.
    if (cardRarity > 0) {
       blendedColor.r += 0.001 * float(cardRarity); // Tiny modification using rarity
    }
    if (cardType >= 0 && cardType < 12) {
       blendedColor.g += 0.001 * float(cardType); // Tiny modification using type
    }
    // --- End Temporary Usage ---


    // --- Optional: Apply previous rarity/type adjustments if still desired ---
    // vec3 finalColor = blendedColor; // Start with blended result
    // vec3 typeColor = typeColors[cardType];
    // finalColor = mix(finalColor, typeColor, 0.1); // Even more subtle tint now

    // float brightness = 1.0 + float(cardRarity > 0) * 0.05; // Less adjustment maybe
    // float contrast = 1.0 + float(cardRarity > 0) * 0.1;
    // finalColor = (finalColor - 0.5) * contrast + 0.5;
    // finalColor *= brightness;

    // Ensure final alpha uses base texture's alpha (or 1.0 if base is opaque)
    FragColor = vec4(clamp(blendedColor, 0.0, 1.0), baseColor.a);
    //FragColor = texture(overlayTexture, TexCoord);
}