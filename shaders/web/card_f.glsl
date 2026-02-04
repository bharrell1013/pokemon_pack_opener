#version 300 es
precision highp float;

out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D baseTexture;    // Renamed from cardTexture for clarity
uniform sampler2D overlayTexture; // L-System Overlay
uniform sampler2D backTexture;
uniform int cardType;             // Keep for potential future use, but tinting might be less needed now
uniform int cardRarity;           // Keep for potential future use

// Optional: Define how much the overlay affects the base
uniform float overlayIntensity; // Adjust for subtle effect

uniform int renderMode; // Shader render mode (0=Normal, 1=Overlay, 2=Base)

// Optional: Type colors if you still want some tinting
const vec3 typeColors[12] = vec3[12](
    vec3(0.8, 0.8, 0.8), vec3(1.0, 0.4, 0.2), vec3(0.2, 0.4, 1.0),
    vec3(0.4, 0.8, 0.2), vec3(1.0, 0.8, 0.0), vec3(0.8, 0.2, 0.8),
    vec3(0.7, 0.6, 0.4), vec3(0.3, 0.2, 0.4), vec3(0.6, 0.4, 0.8),
    vec3(1.0, 0.6, 0.8), vec3(0.7, 0.7, 0.8), vec3(0.6, 0.4, 0.8)
);

void main()
{
    vec4 baseColor = texture(baseTexture, TexCoord);
    vec4 overlay = texture(overlayTexture, TexCoord);

    vec3 finalColor = baseColor.rgb;
    if (cardType >= 0 && cardType < 12) {
        finalColor *= typeColors[cardType];
    }

    if (renderMode == 1) {
        FragColor = vec4(overlay.rgb, overlay.a);
        return;
    }
    if (renderMode == 2) {
        FragColor = vec4(baseColor.rgb, baseColor.a);
        return;
    }

    finalColor = mix(finalColor, overlay.rgb, overlay.a * overlayIntensity);
    FragColor = vec4(finalColor, baseColor.a);
}
