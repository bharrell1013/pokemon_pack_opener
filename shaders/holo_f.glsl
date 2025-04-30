// shaders/holo_f.glsl (Final Logic - Parameters In-Shader)
#version 330 core
out vec4 FragColor;

// --- Inputs ---
in vec2 TexCoord;
in vec3 WorldPos;
in vec3 WorldNormal;    // Vertex Normal
in vec3 WorldTangent;
in vec3 WorldBitangent;
in vec3 WorldViewDir;   // Normalized View Dir

// --- Textures (Uniforms) ---
uniform sampler2D baseTexture;
uniform sampler2D overlayTexture; // L-System pattern
uniform sampler2D backTexture;
uniform sampler1D rainbowGradient;
uniform sampler2D normalMap;     // RGB=Normal TS, A=Height

// --- Essential Uniforms (Passed from C++) ---
uniform float time;
uniform int cardRarity; // 0=Normal, 1=Reverse, 2=Holo, 3=EX, 4=Full Art etc.
uniform int cardType;   // For dummy usage
uniform vec3 lightDir = normalize(vec3(0.5, 0.8, 1.0)); // Default light, can still be uniform
uniform int renderMode = 0; // Debug Mode

// --- TUNING PARAMETERS (Set directly in shader) ---

// Overall Holo Strength
const float holoIntensity = 0.2;

// L-System Overlay Blending (For ALL cards)
const float overlayBlendIntensity = 0.4; // <<< How strongly overlay RGB blends with base (0=none, 1=fully opaque)
const vec3 overlayGreyColor = vec3(0.7); // <<< Grey color to blend

const float normalOverlayIntensity = 0.3; // Visibility of grey pattern on normal cards (tune this: 0.3-0.7)

// Normal/Parallax Control (Subtle base effect for ALL cards)
const float normalMapStrength = 0.03; // Subtle texture effect
const float parallaxHeightScale = 0.01; // Very subtle depth
const int parallaxMinSteps = 4;
const int parallaxMaxSteps = 16;

// Specular Glow (Used for Holos)
const float specularPower = 50.0;  // Sharp highlight
const float specularIntensity = 0.6f; // Moderate brightness for holo glare

// View-Dependent Color Shift (Used for Holos)
const float iridescenceIntensity = 1.0; // Rainbow brightness
const float iridescenceFreq = 2.5;    // Speed of color change with angle
const float iridescenceContrast = 1.5; // Angle sensitivity contrast

// Fresnel (Subtle Edge Glow - Used for Holos, maybe tiny bit for base)
const float baseFresnelPower = 2.3;
const float baseFresnelIntensityHolo = 0.25; // Fresnel for holo cards
const float baseFresnelIntensityNonHolo = 0.03; // Tiny bit for normal cards

// L-System Mask Control (For Holos)
const float patternMaskInfluence = 0.85; // Effects mostly visible through mask

// Border Effect (For Holos)
const float borderWidth = 0.035;
const float borderFadeFactor = 0.7;
const float borderIntensityMultiplier = 3.0; // Brightness of border effect

// --- Constants ---
const float PI = 3.1415926535;

uniform vec2 artworkRectMin; // NEW: Bottom-left UV coord of artwork
uniform vec2 artworkRectMax; // NEW: Top-right UV coord of artwork

const vec3 typeColors[12] = vec3[](
    vec3(0.9, 0.9, 0.8),   // 0: Normal/Colorless (Light Grey)
    vec3(1.0, 0.5, 0.2),   // 1: Fire (Orange/Red)
    vec3(0.3, 0.7, 1.0),   // 2: Water (Blue/Cyan)
    vec3(0.4, 0.9, 0.4),   // 3: Grass (Green)
    vec3(1.0, 1.0, 0.3),   // 4: Lightning (Yellow)
    vec3(0.9, 0.5, 0.9),   // 5: Psychic (Pink/Purple)
    vec3(0.8, 0.6, 0.3),   // 6: Fighting (Brown/Orange)
    vec3(0.5, 0.5, 0.6),   // 7: Darkness (Dark Grey/Purple)
    vec3(0.6, 0.4, 0.9),   // 8: Dragon (Indigo/Purple)
    vec3(1.0, 0.7, 0.9),   // 9: Fairy (Light Pink)
    vec3(0.7, 0.7, 0.8),   // 10: Metal (Silver/Grey)
    vec3(0.6, 0.4, 0.8)    // 11: Ghost (Lavender/Purple) - Assuming 12 types
);

// --- (Parallax function - Keep implementation) ---
vec2 parallaxMap(vec3 viewDirTS, vec2 texCoords, sampler2D nm, float scale) {
    // ... (same implementation as before) ...
    float numSteps = mix(float(parallaxMaxSteps), float(parallaxMinSteps), abs(viewDirTS.z));
    float layerDepth = 1.0 / numSteps;
    float currentLayerDepth = 0.0;
    vec2 P = viewDirTS.xy / viewDirTS.z * scale;
    vec2 deltaTexCoords = P / numSteps;
    vec2 currentTexCoords = texCoords;
    float currentDepthMapValue = texture(nm, currentTexCoords).a;
    while(currentLayerDepth < currentDepthMapValue) {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(nm, currentTexCoords).a;
        currentLayerDepth += layerDepth;
    }
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(nm, prevTexCoords).a - currentLayerDepth + layerDepth;
    float weight = 0.0;
    if (abs(afterDepth - beforeDepth) > 0.001) { weight = afterDepth / (afterDepth - beforeDepth); }
    vec2 finalTexCoords = mix(currentTexCoords, prevTexCoords, weight);
    return clamp(finalTexCoords, 0.005, 0.9995);
}


void main()
{
    // --- Back Face Logic ---
    if (!gl_FrontFacing) {
        vec2 backTexCoord = vec2(1.0 - TexCoord.x, TexCoord.y);
        FragColor = texture(backTexture, backTexCoord);
        return;
    }

    // --- Front Face Initialization ---
    vec3 N_vert = normalize(WorldNormal);
    vec3 T_vert = normalize(WorldTangent);
    vec3 B_vert = normalize(WorldBitangent);
    T_vert = normalize(T_vert - dot(T_vert, N_vert) * N_vert);
    B_vert = normalize(cross(N_vert, T_vert));
    mat3 TBN = mat3(T_vert, B_vert, N_vert);
    vec3 V_world = WorldViewDir;
    vec3 L = normalize(lightDir); // Use uniform lightDir
    vec3 H = normalize(V_world + L);
    float dotNV = clamp(dot(N_vert, V_world), 0.0, 1.0);

    // --- Parallax & Normal Mapping (For Base Texture) ---
    vec3 V_ts = vec3(dot(V_world, T_vert), dot(V_world, B_vert), dot(V_world, N_vert));
    vec2 parallaxTexCoord = parallaxMap(V_ts, TexCoord, normalMap, parallaxHeightScale);
    vec4 normalHeightSample = texture(normalMap, parallaxTexCoord);
    vec3 N_ts_mapped = normalize(normalHeightSample.rgb * 2.0 - 1.0);
    vec3 N_world_mapped = normalize(TBN * N_ts_mapped);
    vec3 N_final_base = normalize(mix(N_vert, N_world_mapped, normalMapStrength));

    // --- Sample Base and Overlay ---
    vec4 baseColor = texture(baseTexture, parallaxTexCoord);
    vec4 overlay = texture(overlayTexture, TexCoord);
    float l_systemMask = overlay.a;
    vec3 l_systemColor = overlay.rgb; // Get color from overlay texture

    // --- Calculate Base Texture Lighting (ALL Cards) ---
    float diffuseFactor = max(0.0, dot(N_final_base, L));
    float dotNH_base = clamp(dot(N_final_base, H), 0.0, 1.0);
    float microSpecFactor = pow(dotNH_base, 15.0);
    vec3 litBaseColor = baseColor.rgb * (vec3(0.3) + vec3(0.7) * diffuseFactor);
    litBaseColor += vec3(0.08) * microSpecFactor;
    // Add subtle non-holo fresnel to base
    float fresnelNonHolo = pow(1.0 - dotNV, baseFresnelPower);
    litBaseColor += vec3(0.8, 0.9, 1.0) * fresnelNonHolo * baseFresnelIntensityNonHolo;

    vec3 finalBase = litBaseColor; // Start with the lit base color
    if (cardRarity == 0) {
        // For NORMAL cards, blend the subtle grey L-System pattern onto the base
        // l_systemColor should be grey here based on TextureManager logic
        finalBase = mix(litBaseColor, l_systemColor, l_systemMask * normalOverlayIntensity);
    }

    // Mix between the lit base and a fixed grey color, using the overlay's alpha
    // and blend intensity to control how much grey pattern shows through.
    //vec3 baseWithOverlay = mix(litBaseColor, overlayGreyColor, l_systemMask * overlayBlendIntensity);
    vec3 baseWithOverlay = mix(litBaseColor, l_systemColor, l_systemMask * overlayBlendIntensity * (1.0 - baseColor.a) );


    // --- Initialize Holo Effects ---
    vec3 appliedSpecular = vec3(0.0);         // CALCULATED IF RARITY >= 1
    vec3 appliedIridescenceColor = vec3(0.0); // CALCULATED IF RARITY >= 1
    vec3 appliedFresnel = vec3(0.0);          // CALCULATED IF RARITY >= 1
    vec3 calculatedBorderColor = vec3(0.0);   // <<< DECLARED OUTSIDE, CALCULATED IF RARITY >= 2
    vec3 holoLayer = vec3(0.0);

     bool isInsideArtwork = TexCoord.x > artworkRectMin.x && TexCoord.x < artworkRectMax.x &&
                           TexCoord.y > artworkRectMin.y && TexCoord.y < artworkRectMax.y;

   // Determine if holo should be applied based on rarity and location
    // Reverse Holo (Rarity 1): Apply effect *outside* artwork
    // Holo Rare (Rarity 2+): Apply effect *inside* artwork (Classic Holo)
    // Note: EX/FullArt (3, 4) often have unique textures/effects not just a pattern,
    // but we can treat them like Holo Rare for this masking logic.
    bool applyHoloHere = false; // Default to no holo
    if (cardRarity == 1 || cardRarity == 2) { // Reverse Holo OR Holo Rare
        applyHoloHere = !isInsideArtwork;     // Apply *outside* artwork for BOTH
    }
     else if (cardRarity >= 3) { // EX, Full Art, etc.
        applyHoloHere = true; // Apply *everywhere* (ignore isInsideArtwork)
    }

    // --- Calculate Effects ONLY if applyHoloHere is true ---
    if (applyHoloHere) {
        // Get Type Color
        vec3 typeColor = vec3(1.0);
        if (cardType >= 0 && cardType < 12) {
            typeColor = typeColors[cardType];
        }

        // --- Calculate Holo Components (Specular, Iridescence, Fresnel) ---
        // (These calculations remain the same as before, using typeColor for tinting)
        float dotNH_smooth = clamp(dot(N_vert, H), 0.0, 1.0);
        float specularFactor = pow(dotNH_smooth, specularPower);
        appliedSpecular = typeColor * specularFactor * specularIntensity;

        float viewFactor = pow(dotNV, iridescenceContrast);
        float rainbowCoord = fract(viewFactor * iridescenceFreq + time * 0.1);
        vec3 iridescentColor = texture(rainbowGradient, rainbowCoord).rgb;
        appliedIridescenceColor = mix(iridescentColor, typeColor, 0.4) * iridescenceIntensity;

        float fresnel = pow(1.0 - dotNV, baseFresnelPower);
        appliedFresnel = mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnel * baseFresnelIntensityHolo;

        // --- Border Effect (Only for non-reverse holo, Rarity >= 2) ---
        // Calculate for Holo, EX, Full Art, etc.
        if (cardRarity >= 2) {
            // Use distance from the actual card edges (0,0) and (1,1)
            vec2 borderDist = min(TexCoord, 1.0 - TexCoord);
            float minBorderDist = min(borderDist.x, borderDist.y);
            float borderMask = smoothstep(0.0, borderWidth * 1.1, minBorderDist);
                  borderMask -= smoothstep(borderWidth * 1.1, borderWidth * 1.1 + 0.015, minBorderDist);
             if (borderMask > 0.0) {
                 float borderRainbowCoord = fract(TexCoord.x * 2.5 - TexCoord.y * 1.0 + time * 0.2);
                 vec3 borderRainbow = texture(rainbowGradient, borderRainbowCoord).rgb;
                 float borderBrightnessFactor = 1.0 + pow(1.0 - dotNV, 2.0) * 0.5;
                 calculatedBorderColor = borderRainbow * borderBrightnessFactor;
                 calculatedBorderColor *= borderMask; // Apply shape mask
             }
        }

        // Combine Holo Layer (Modulated by L-System Mask)
        // Effects are only calculated if applyHoloHere is true
        holoLayer = (appliedSpecular + appliedIridescenceColor + appliedFresnel) * l_systemMask * patternMaskInfluence;

    } // --- End if (applyHoloHere) ---

    // --- Final Combination ---
    // Start with the base texture (lit, maybe blended slightly with grey overlay pattern)
    vec3 finalColor = finalBase;

    // Add the Holo effects layer, scaled by the main holo intensity
    // holoLayer is zero if applyHoloHere was false or cardRarity == 0
    finalColor += holoLayer * holoIntensity;

    // --- Add Border Separately (Potentially Brighter) ---
    // Increase multiplier for border brightness
    const float borderIntensityMultiplier = 3.5; // <<< INCREASED VALUE (Tune as needed)
    // Only add border color if it was calculated (i.e., rarity >= 2)
    if (cardRarity >= 2) {
        finalColor += calculatedBorderColor * borderIntensityMultiplier;
    }


    // --- Debug Modes (Apply based on renderMode) ---
    vec3 debugColor = finalColor;
    float debugAlpha = baseColor.a * (1.0 - l_systemMask) + l_systemMask; // Blend alpha based on mask

    if (renderMode != 0) {
         if (renderMode == 1) debugColor = applyHoloHere ? appliedIridescenceColor * l_systemMask : vec3(0.0);
         else if (renderMode == 2) debugColor = applyHoloHere ? appliedSpecular * l_systemMask : vec3(0.0);
         // Debug mode 3: show border + fresnel (only where holo applies)
         else if (renderMode == 3) {
             debugColor = vec3(0.0);
             if (applyHoloHere) {
                 debugColor += appliedFresnel * l_systemMask;
                 if (cardRarity >= 2) { // Show calculated border shape/color * multiplier
                     debugColor += calculatedBorderColor * borderIntensityMultiplier;
                 }
             }
         }
         else if (renderMode == 4) debugColor = finalBase;
         else if (renderMode == 5) { debugColor = overlay.rgb; debugAlpha = overlay.a; }
         else if (renderMode == 6) debugColor = N_final_base * 0.5 + 0.5;
         else if (renderMode == 7) debugColor = vec3(applyHoloHere);
    }

    FragColor = vec4(clamp(debugColor, 0.0, 1.0), debugAlpha);
}