#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D baseTexture;    // Renamed from cardTexture for clarity
uniform sampler2D overlayTexture; // L-System Overlay
uniform sampler2D backTexture;
uniform int cardType;             // Keep for potential future use, but tinting might be less needed now
uniform int cardRarity;           // Keep for potential future use

// Optional: Define how much the overlay affects the base
uniform float overlayIntensity = 0.6; // Adjust for subtle effect

uniform int renderMode = 0; // Shader render mode (0=Normal, 1=Overlay, 2=Base)

// Optional: Type colors if you still want some tinting
const vec3 typeColors[12] = vec3[12](
    vec3(0.8, 0.8, 0.8), vec3(1.0, 0.4, 0.2), vec3(0.2, 0.4, 1.0),
    vec3(0.4, 0.8, 0.2), vec3(1.0, 0.8, 0.0), vec3(0.8, 0.2, 0.8),
    vec3(0.8, 0.4, 0.0), vec3(0.4, 0.2, 0.4), vec3(0.6, 0.4, 1.0),
    vec3(1.0, 0.6, 0.8), vec3(0.6, 0.6, 0.8), vec3(0.4, 0.2, 0.6)
);

void main()
{
    vec3 finalColor;

    if (gl_FrontFacing) {
        // --- FRONT FACE LOGIC ---
        vec4 overlayColor = texture(overlayTexture, TexCoord);
        vec4 baseColor = texture(baseTexture, TexCoord);

        // Blend overlay onto base (Choose your preferred blend mode)
        vec3 blendedColor = mix(baseColor.rgb, overlayColor.rgb, overlayColor.a * overlayIntensity);
        // Or: vec3 blendedColor = baseColor.rgb + overlayColor.rgb * overlayColor.a * overlayIntensity;

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

        // --- Shader Mode Selection for Front Face ---
        if (renderMode == 1) { // Overlay Only
            finalColor = overlayColor.rgb;
        } else if (renderMode == 2) { // Base Only
            finalColor = baseColor.rgb;
        } else { // Normal Blended (Mode 0 or default)
            finalColor = blendedColor;
        }

        // Keep alpha from the base texture (usually opaque)
        FragColor = vec4(clamp(finalColor, 0.0, 1.0), baseColor.a);

    } else {
        // --- BACK FACE LOGIC ---
        // Simply sample the back texture
        finalColor = texture(backTexture, TexCoord).rgb;

        // Back face ignores renderMode for simplicity, always shows back texture
        // Back face alpha is assumed opaque
        FragColor = vec4(finalColor, 1.0);
    }
}