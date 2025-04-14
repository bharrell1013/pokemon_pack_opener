#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 WorldPos;
in vec3 Normal;

uniform sampler2D baseTexture;
uniform sampler2D overlayTexture;

uniform float time;
// Suggest increasing holoIntensity passed from C++ (e.g., to 1.5 or 2.0)
uniform float holoIntensity = 1.0; // Keep default 1.0 here, override from C++
uniform int cardType;
uniform int cardRarity; // 1=Reverse, 2=Holo, 3=EX, 4=Full Art
uniform vec3 viewPos;

uniform int renderMode = 0;

const vec3 typeBasedHoloColors[12] = vec3[](
    vec3(0.8, 0.8, 0.8), vec3(1.0, 0.4, 0.2), vec3(0.2, 0.4, 1.0),
    vec3(0.4, 0.8, 0.2), vec3(1.0, 0.8, 0.0), vec3(0.8, 0.2, 0.8),
    vec3(0.8, 0.4, 0.0), vec3(0.4, 0.2, 0.4), vec3(0.6, 0.4, 1.0),
    vec3(1.0, 0.6, 0.8), vec3(0.6, 0.6, 0.8), vec3(0.4, 0.2, 0.6)
);

vec2 rotate2D(vec2 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);
    return rot * p;
}

// Smooth Noise Function (Alternative to rand for less "dotty" look)
// Source: https://www.shadertoy.com/view/4djSRW (IQ's noise)
float noise(vec2 p) {
    vec2 ip = floor(p);
    vec2 u = fract(p);
    u = u*u*(3.0-2.0*u); // smoothstep

    float res = mix(
        mix( fract(sin(dot(ip+vec2(0,0), vec2(12.9898, 78.233))) * 43758.5453),
             fract(sin(dot(ip+vec2(1,0), vec2(12.9898, 78.233))) * 43758.5453), u.x),
        mix( fract(sin(dot(ip+vec2(0,1), vec2(12.9898, 78.233))) * 43758.5453),
             fract(sin(dot(ip+vec2(1,1), vec2(12.9898, 78.233))) * 43758.5453), u.x), u.y);
    return res*res;
}


void main()
{
    // --- Sample Textures ---
    vec4 baseColor = texture(baseTexture, TexCoord);
    vec4 overlay = texture(overlayTexture, TexCoord);

    // --- Calculate View Direction and Fresnel ---
    vec3 N = normalize(Normal);
    vec3 viewDir = normalize(viewPos - WorldPos);
    // Use lower power for MORE widespread Fresnel, higher for sharper edge glow
    float fresnelPower = 2.5; // Make Fresnel more prominent across the surface
    float fresnel = pow(1.0 - clamp(dot(N, viewDir), 0.0, 1.0), fresnelPower);

    // --- Base Output Color ---
    vec3 finalColor = baseColor.rgb;
    vec3 typeColor = (cardType >= 0 && cardType < 12) ? typeBasedHoloColors[cardType] : vec3(0.7);

    // --- Rarity-Dependent Holo Effects ---
    if (cardRarity == 1 || cardRarity == 2) { // Reverse Holo (1) or Regular Holo (2)
        float overlayMask = overlay.a; // Use Alpha channel of the L-system pattern

        // 1. Make the Pattern Itself Shine/Glow
        float patternGlowFactor = (cardRarity == 1) ? 0.4 : 0.6; // Holo pattern glows brighter
        // Additive glow based on the pattern mask and Fresnel
        finalColor += vec3(0.8, 0.8, 0.9) * overlayMask * holoIntensity * patternGlowFactor * (0.5 + fresnel * 0.5);

        // 2. Rainbow Sheen - Make more vibrant and apply more strongly ON the pattern
        vec2 rainbowUV = TexCoord * 1.5 + vec2(0.0, time * 0.15); // Slightly faster scroll, larger scale
        rainbowUV.x += sin(rainbowUV.y * 12.0 + time * 0.6) * 0.08; // More waviness
        float rainbowPhase = rainbowUV.x * 8.0 + time * 1.0; // Different frequency
        vec3 rainbowColor = vec3(0.5 + 0.5 * sin(rainbowPhase),
                                0.5 + 0.5 * sin(rainbowPhase + 2.094),
                                0.5 + 0.5 * sin(rainbowPhase + 4.188));
        vec3 finalRainbow = mix(typeColor * 0.6, rainbowColor, 0.8); // More saturated rainbow

        // Apply rainbow effect strongly where the pattern exists
        float rainbowIntensityFactor = (cardRarity == 1) ? 0.8 : 1.2; // Holo rainbow stronger
        // Use mix for base, add extra brightness based on mask
        finalColor = mix(finalColor, finalRainbow * 1.2, overlayMask * holoIntensity * rainbowIntensityFactor * 0.5); // Blend base rainbow
        finalColor += finalRainbow * 0.5 * overlayMask * holoIntensity * rainbowIntensityFactor; // Add extra brightness on pattern

        // 3. Fresnel Gloss - Apply a noticeable base gloss + strong enhancement ON pattern
        float fresnelIntensityFactor = (cardRarity == 1) ? 1.0 : 1.5; // Significantly stronger enhancement
        // Base global Fresnel
        finalColor += typeColor * fresnel * holoIntensity * 0.4; // Increased base global component
        // Strong enhancement on pattern
        finalColor += typeColor * fresnel * holoIntensity * overlayMask * fresnelIntensityFactor;


    } else if (cardRarity >= 3) { // EX / Full Art / etc. (Rarity 3+)
        // Use L-System Map for subtle texture/enhancement if desired, otherwise ignore it
        float lsystemEnhancement = overlay.a; // Or length(overlay.rgb);

        // 1. VERY Strong Global Glossy Fresnel Reflection
        // Mix type color with white based on Fresnel for a bright metallic shine
        vec3 fresnelColor = mix(typeColor * 0.8, vec3(1.0), fresnel);
        // Apply this color additively, scaled strongly by holoIntensity
        finalColor += fresnelColor * fresnel * holoIntensity * 1.0; // HIGH Fresnel multiplier

        // 2. Add Subtle, Slow-Moving Noise Texture to the Shine
        // Use the noise function, scale by world pos and time for slow movement
        vec2 noiseUV = WorldPos.xy * 0.5 + time * 0.05; // Slow moving noise based on world position
        float subtleNoise = noise(noiseUV * 5.0); // Use the smooth noise function
        subtleNoise = smoothstep(0.2, 0.8, subtleNoise); // Adjust contrast of noise
        // Add this noise to the color, primarily where Fresnel is high
        finalColor += vec3(0.5) * subtleNoise * fresnel * holoIntensity * 0.6; // Make noise affect brightness

        // 3. Enhance with Vibrant Rainbow Sheen (Globally Applied)
        vec2 rainbowUV = TexCoord * 1.0 - vec2(time * 0.1, time * 0.05); // Larger, slower bands
        rainbowUV = rotate2D(rainbowUV - 0.5, time * 0.05) + 0.5; // Slow rotation
        float rainbowPhase = (rainbowUV.x - rainbowUV.y) * 5.0 + time * 0.5; // Diagonal movement
        vec3 rainbowColor = vec3(0.5 + 0.5 * sin(rainbowPhase),
                                0.5 + 0.5 * sin(rainbowPhase + 2.094),
                                0.5 + 0.5 * sin(rainbowPhase + 4.188));
        // Apply rainbow additively and subtly, enhanced by Fresnel
        finalColor += rainbowColor * 0.15 * fresnel * holoIntensity; // Rainbow is part of the shine

        // 4. Optional: Use L-System map to subtly enhance overall brightness/texture
        finalColor += vec3(0.1) * lsystemEnhancement * holoIntensity;
    }

    // --- Apply Render Mode ---
    if (renderMode == 1) { // Overlay Only
        FragColor = vec4(overlay.rgb, overlay.a);
    } else if (renderMode == 2) { // Base Only
        FragColor = baseColor;
    } else { // Normal Holo (Mode 0 or default)
        // Clamp final color and set alpha from base texture
        FragColor = vec4(clamp(finalColor, 0.0, 1.0), baseColor.a);
    }
}