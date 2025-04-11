#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 WorldPos; // Keep if needed for view-dependent effects
in vec3 Normal;   // Keep if needed for view-dependent effects

uniform sampler2D baseTexture;    // API Image
uniform sampler2D overlayTexture; // L-System Overlay (Hexagons/Shiny)

uniform float time;
uniform float holoIntensity; // Overall intensity for holo effects
uniform int cardType;
uniform int cardRarity; // NEW: Pass rarity to potentially adjust effects

// Keep type colors if used for effects
const vec3 holoColors[12] = vec3[](
    vec3(0.8, 0.8, 0.8),   // Normal (Index 0)
    vec3(1.0, 0.4, 0.2),   // Fire (Index 1)
    vec3(0.2, 0.4, 1.0),   // Water (Index 2)
    vec3(0.4, 0.8, 0.2),   // Grass (Index 3)
    vec3(1.0, 0.8, 0.0),   // Electric (Index 4)
    vec3(0.8, 0.2, 0.8),   // Psychic (Index 5)
    vec3(0.8, 0.4, 0.0),   // Fighting (Index 6)
    vec3(0.4, 0.2, 0.4),   // Dark (Index 7)
    vec3(0.6, 0.4, 1.0),   // Dragon (Index 8)
    vec3(1.0, 0.6, 0.8),   // Fairy (Index 9)
    vec3(0.6, 0.6, 0.8),   // Steel (Index 10)
    vec3(0.4, 0.2, 0.6)    // Ghost (Index 11)
);

// Keep helper functions (rand, rotate2D)
vec2 rotate2D(vec2 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);
    return rot * p;
}
float rand(vec2 co) {
    // Simple pseudo-random noise function
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main()
{
    // Sample base texture (API image)
    vec4 baseColor = texture(baseTexture, TexCoord);

    // Sample overlay texture (L-System)
    vec4 overlay = texture(overlayTexture, TexCoord); // Use shorter name

    // --- Holo/Rainbow/Sparkle Logic MODIFIED by Overlay ---

    // 1. Base Color: Start with the API image
    vec3 finalColor = baseColor.rgb;

    // 2. Apply L-System Overlay Pattern (Blending similar to normal card)
    // Blend the L-system pattern onto the base image first
    // Use Screen or Alpha blend depending on desired look (Hexagons might look good with Alpha, Shiny with Screen/Additive)
    if (cardRarity == 1 || cardRarity == 2) { // Reverse/Holo (Hexagon Overlay)
         vec3 patternColor = overlay.rgb;
         vec3 screenBlend = vec3(1.0) - (vec3(1.0) - finalColor) * (vec3(1.0) - patternColor);
         finalColor = mix(finalColor, screenBlend, overlay.a * 0.7);
         vec2 uv = TexCoord;
        uv = rotate2D(uv - 0.5, time * 0.2) + 0.5;

        // Rainbow - appears ON the pattern lines
        vec2 rainbowUV = uv;
        rainbowUV.x += sin(rainbowUV.y * 10.0 + time) * 0.02;
        float rainbowStrength = sin(rainbowUV.x * 20.0 + time) * 0.5 + 0.5;
        vec3 typeHoloColor = typeBasedHoloColors[cardType];
        vec3 rainbowColor = mix(typeHoloColor, vec3(rainbowStrength), 0.7);
        finalColor = mix(finalColor, rainbowColor, overlay.a * holoIntensity * 1.0); // Modulate by overlay alpha

        // Sparkle - appears ON the pattern lines
        float sparkleScale = 40.0; // Slightly larger sparkles maybe
        float sparkleNoise = pow(rand(floor(uv * sparkleScale) + time * 0.6), 18.0); // Sharper
        finalColor += sparkleNoise * overlay.a * holoIntensity * 2.0; // Stronger sparkle on lines

        // Fresnel - global shimmer, maybe slightly enhanced by pattern
        vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0) - WorldPos);
        float fresnel = pow(1.0 - max(dot(normalize(Normal), viewDir), 0.0), 3.0);
        finalColor += fresnel * typeHoloColor * holoIntensity * (0.5 + overlay.a * 0.5); // Mix global + pattern-based fresnel
    } else if (cardRarity >= 3) { // EX/FullArt (Shiny Overlay)
         // Screen blend the shiny pattern, modulate by its alpha
         // 1. Extract intensity from sparkle map (use Alpha or Red channel)
        float sparkleMapIntensity = overlay.a; // Or overlay.r if you drew white dots

        // 2. Greatly enhance base shininess/effects based on intensity
        vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0) - WorldPos);
        float fresnel = pow(1.0 - max(dot(normalize(Normal), viewDir), 0.0), 2.0); // Sharper Fresnel maybe

        // Base glossy reflection (global)
        vec3 baseGloss = vec3(0.8) * fresnel * holoIntensity;
        finalColor += baseGloss;

        // Add INTENSE sparkle/flare where the L-system dots are
        vec2 uv = TexCoord;
        uv = rotate2D(uv - 0.5, time * 0.1) + 0.5; // Slower rotation maybe
        float sparkleNoise = pow(rand(floor(uv * 60.0) + time * 1.1), 25.0); // Very sharp, fast noise
        vec3 sparkleColor = vec3(1.0, 1.0, 0.9); // Bright white/yellow sparkle

        // Apply sparkle VERY strongly where sparkleMapIntensity > 0
        finalColor += sparkleColor * sparkleNoise * sparkleMapIntensity * holoIntensity * 5.0; // Big multiplier

         // Add a moving rainbow sheen, amplified by sparkle map
         vec2 rainbowUV = TexCoord * 1.5 + vec2(time * 0.1, time * 0.05); // Slow moving larger rainbow bands
         float rainbow = (sin(rainbowUV.x * 3.0) + sin(rainbowUV.y * 2.0)) * 0.5 + 0.5;
         vec3 rainbowSheen = mix(vec3(0.9,0.9,1.0), vec3(1.0,0.9,0.9), rainbow) * 0.4; // Subtle rainbow colors
         finalColor += rainbowSheen * sparkleMapIntensity * holoIntensity * 2.0; // Sheen appears brighter on sparkle spots
    }

    // Final color clamp and alpha
    FragColor = vec4(clamp(finalColor, 0.0, 1.0), baseColor.a);
    //FragColor = texture(overlayTexture, TexCoord);
}