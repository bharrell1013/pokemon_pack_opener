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
uniform sampler2D overlayTexture; // L-System pattern (Normal/Reverse) OR blank (Glossy)
uniform sampler2D backTexture;
uniform sampler1D rainbowGradient; // Used for Iridescence
uniform sampler2D normalMap;     // RGB=Normal TS, A=Height

// --- Essential Uniforms (Passed from C++) ---
uniform float time;
uniform int cardRarity; // 0=Normal, 1=Reverse, 2=Holo, 3=EX, 4=Full Art etc.
uniform int cardType;
uniform vec3 lightDir = normalize(vec3(0.5, 0.8, 1.0));
uniform int renderMode = 0; // Debug Mode
uniform vec2 artworkRectMin;
uniform vec2 artworkRectMax;

// --- TUNING PARAMETERS ---

// ** Shared Base Effects **
const float normalMapStrength = 0.03;
const float parallaxHeightScale = 0.01;
const int parallaxMinSteps = 4;
const int parallaxMaxSteps = 16;
const float baseFresnelPower = 2.8;

// ** Normal Card Specific (Rarity 0) **
const float normalOverlayIntensity = 0.3;
const float fresnelIntensityNormal = 0.03;

// ** Reverse Holo Specific (Rarity 1) **
const float reversePatternMaskInfluence = 0.85;
const float reverseEffectIntensity = 0.2;
const float reverseSpecularPower = 50.0;
const float reverseSpecularIntensity = 0.5f;
const float reverseIridescenceIntensity = 0.9;
const float reverseIridescenceFreq = 2.5;
const float reverseIridescenceContrast = 1.5;
const float reverseFresnelIntensity = 0.25;

// ** Glossy (Rarity >= 2) Specific - ADJUSTED ARTWORK HOLO **
const float glossySpecularPower = 50.0;
const float glossySpecularIntensity = 0.45;     // ADJUSTED: Reduced intensity over artwork (was 0.6)
const float glossyIridescenceIntensity = 0.5;   // ADJUSTED: Reduced intensity over artwork (was 0.7)
const float glossyIridescenceFreq = 2.5;
const float glossyIridescenceContrast = 1.5;
const float glossyFresnelIntensity = 0.3;       // Kept fresnel the same for now
const float glossyBlendFactor = 0.45;           // Kept blend factor the same

// ** Border Effect (For Glossy Cards, Rarity >= 2) - ADJUSTED SHINE & TRANSPARENCY **
const float borderWidth = 0.04;
const float borderBaseMetalness = 0.4;        // ADJUSTED: Darker base for more contrast (was 0.5)
const float borderSheenPower = 28.0;
const float borderSheenIntensity = 1.6;       // ADJUSTED: Increased highlight intensity (was 1.1)
const vec3 borderSheenBaseColor = vec3(1.0);
const float borderSheenTintStrength = 0.6;    // How much type color influences the sheen (0=white, 1=full type color)
const float borderIridescenceIntensity = 0.05;// Subtle rainbow overlay intensity
const float borderOverallIntensity = 1.0;     // Multiplier for the final calculated border color
const float borderTransparencyFactor = 0.75;  // ADJUSTED: More transparent blend (was 0.9)

// --- Constants ---
const float PI = 3.1415926535;

// Type Colors
const vec3 typeColors[12] = vec3[](
    vec3(0.9, 0.9, 0.8), vec3(1.0, 0.5, 0.2), vec3(0.3, 0.7, 1.0), // Norm, Fire, Water
    vec3(0.4, 0.9, 0.4), vec3(1.0, 1.0, 0.3), vec3(0.9, 0.5, 0.9), // Grass, Light, Psychic
    vec3(0.8, 0.6, 0.3), vec3(0.5, 0.5, 0.6), vec3(0.6, 0.4, 0.9), // Fight, Dark, Dragon
    vec3(1.0, 0.7, 0.9), vec3(0.7, 0.7, 0.8), vec3(0.6, 0.4, 0.8)  // Fairy, Metal, Ghost
);

// --- Parallax function ---
vec2 parallaxMap(vec3 viewDirTS, vec2 texCoords, sampler2D nm, float scale) {
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
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V_world + L);
    float dotNV = clamp(dot(N_vert, V_world), 0.0, 1.0);
    float dotNH_smooth = clamp(dot(N_vert, H), 0.0, 1.0);

    // --- Parallax & Normal Mapping ---
    vec3 V_ts = vec3(dot(V_world, T_vert), dot(V_world, B_vert), dot(V_world, N_vert));
    vec2 parallaxTexCoord = parallaxMap(V_ts, TexCoord, normalMap, parallaxHeightScale);
    vec4 normalHeightSample = texture(normalMap, parallaxTexCoord);
    vec3 N_ts_mapped = normalize(normalHeightSample.rgb * 2.0 - 1.0);
    vec3 N_world_mapped = normalize(TBN * N_ts_mapped);
    vec3 N_final_base = normalize(mix(N_vert, N_world_mapped, normalMapStrength));
    float dotNH_mapped = clamp(dot(N_final_base, H), 0.0, 1.0);

    // --- Sample Base and Overlay ---
    vec4 baseColor = texture(baseTexture, parallaxTexCoord);
    vec4 overlay = texture(overlayTexture, TexCoord);
    float l_systemMask = overlay.a;
    vec3 l_systemColor = overlay.rgb;

    // --- Calculate Base Texture Lighting ---
    float diffuseFactor = max(0.0, dot(N_final_base, L));
    float microSpecFactor = pow(dotNH_mapped, 15.0);
    vec3 litBaseColor = baseColor.rgb * (vec3(0.3) + vec3(0.7) * diffuseFactor);
    litBaseColor += vec3(0.08) * microSpecFactor;

    // --- Determine Type Color ---
    vec3 typeColor = vec3(1.0);
    if (cardType >= 0 && cardType < 12) {
        typeColor = typeColors[cardType];
    }

    // --- Initialize Final Color & Effects ---
    vec3 finalColor = litBaseColor;
    vec3 addedEffects = vec3(0.0);
    vec3 glossyEffectsAccumulator = vec3(0.0);

    // --- Rarity-Specific Logic ---
    bool isInsideArtwork = TexCoord.x > artworkRectMin.x && TexCoord.x < artworkRectMax.x &&
                           TexCoord.y > artworkRectMin.y && TexCoord.y < artworkRectMax.y;

    if (cardRarity == 0) { // --- Normal Card ---
        finalColor = mix(finalColor, l_systemColor, l_systemMask * normalOverlayIntensity);
        float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
        addedEffects += vec3(0.8, 0.9, 1.0) * fresnelFactor * fresnelIntensityNormal;
        finalColor += addedEffects;

    } else if (cardRarity == 1) { // --- Reverse Holo Card ---
        if (!isInsideArtwork) {
            float specularFactor = pow(dotNH_mapped, reverseSpecularPower);
            vec3 underlyingSpecular = typeColor * specularFactor * reverseSpecularIntensity;
            float viewFactor = pow(dotNV, reverseIridescenceContrast);
            float rainbowCoord = fract(viewFactor * reverseIridescenceFreq + time * 0.1);
            vec3 iridescentColor = texture(rainbowGradient, rainbowCoord).rgb;
            vec3 underlyingIridescence = mix(iridescentColor, typeColor, 0.4) * reverseIridescenceIntensity;
            float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
            vec3 underlyingFresnel = mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnelFactor * reverseFresnelIntensity;
            vec3 combinedUnderlyingEffects = underlyingSpecular + underlyingIridescence + underlyingFresnel;
            addedEffects += combinedUnderlyingEffects * l_systemMask * reversePatternMaskInfluence * reverseEffectIntensity;
        } else {
             float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
             addedEffects += vec3(0.8, 0.9, 1.0) * fresnelFactor * fresnelIntensityNormal;
        }
        finalColor += addedEffects;

    } else { // --- Glossy Cards (Holo, EX, Full Art) cardRarity >= 2 ---

        // 1. Calculate Glossy Effects for the main area (Using ADJUSTED Intensities)
        float glossSpecularFactor = pow(dotNH_mapped, glossySpecularPower);
        glossyEffectsAccumulator += vec3(1.0) * glossSpecularFactor * glossySpecularIntensity; // Reduced intensity

        float viewFactor = pow(dotNV, glossyIridescenceContrast);
        float rainbowCoord = fract(viewFactor * glossyIridescenceFreq + time * 0.1);
        vec3 iridescentColor = texture(rainbowGradient, rainbowCoord).rgb;
        glossyEffectsAccumulator += mix(iridescentColor, typeColor, 0.4) * glossyIridescenceIntensity; // Reduced intensity

        float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
        glossyEffectsAccumulator += mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnelFactor * glossyFresnelIntensity;

        // Blend glossy effects with base color
        finalColor = mix(finalColor, finalColor + glossyEffectsAccumulator, glossyBlendFactor);

        // 2. CALCULATE BORDER EFFECT SEPARATELY (ADJUSTED Shine & Transparency)
        vec3 borderFinalColor = vec3(0.0);
        vec2 borderDist = min(TexCoord, 1.0 - TexCoord);
        float minBorderDist = min(borderDist.x, borderDist.y);
        float borderMask = 1.0 - smoothstep(borderWidth * 0.95, borderWidth * 1.05, minBorderDist); // Outer fade only

        if (borderMask > 0.001) {
            // a) Base metallic color (Darker base)
            vec3 baseBorderColor = typeColor * borderBaseMetalness;

            // b) Additive Sheen (Tinted & Brighter)
            float borderSheenFactor = pow(dotNH_smooth, borderSheenPower);
            vec3 tintedSheenColor = mix(borderSheenBaseColor, typeColor, borderSheenTintStrength);
            vec3 addedSheen = tintedSheenColor * borderSheenFactor * borderSheenIntensity; // Increased intensity

            // c) Subtle Additive Iridescence
            float borderRainbowCoord = fract(TexCoord.x * 1.8 - TexCoord.y * 0.7 + time * 0.2);
            vec3 addedIridescence = texture(rainbowGradient, borderRainbowCoord).rgb * borderIridescenceIntensity;

            // d) Combine
            borderFinalColor = (baseBorderColor + addedSheen + addedIridescence) * borderOverallIntensity;

            // 3. MIX using the mask AND transparency factor (More transparent)
            finalColor = mix(finalColor, borderFinalColor, borderMask * borderTransparencyFactor);
        }
    }

    // --- Final Alpha ---
    float finalAlpha = baseColor.a;
    if(cardRarity == 1 && !isInsideArtwork) {
        finalAlpha = max(baseColor.a, l_systemMask * reversePatternMaskInfluence);
    }

    // --- Debug Modes ---
    vec3 debugColor = finalColor;
     if (renderMode != 0) {
         debugColor = vec3(0.0);

         if (renderMode == 1) { // Iridescence Component Only
             vec3 baseIridContribution = vec3(0.0);
             if (cardRarity == 1 && !isInsideArtwork) {
                 float viewFactor = pow(dotNV, reverseIridescenceContrast);
                 float rainbowCoord = fract(viewFactor * reverseIridescenceFreq + time * 0.1);
                 vec3 reverseIrid = mix(texture(rainbowGradient, rainbowCoord).rgb, typeColor, 0.4) * reverseIridescenceIntensity;
                 baseIridContribution = reverseIrid * l_systemMask * reversePatternMaskInfluence * reverseEffectIntensity;
             } else if (cardRarity >= 2) {
                 float viewFactor = pow(dotNV, glossyIridescenceContrast);
                 float rainbowCoord = fract(viewFactor * glossyIridescenceFreq + time * 0.1);
                 vec3 glossyIrid = mix(texture(rainbowGradient, rainbowCoord).rgb, typeColor, 0.4) * glossyIridescenceIntensity;
                 baseIridContribution = glossyIrid * glossyBlendFactor;
             }

             vec3 finalIridDebug = baseIridContribution;
             if (cardRarity >= 2) {
                 vec2 bDist = min(TexCoord, 1.0 - TexCoord); float mBDist = min(bDist.x, bDist.y);
                 float bMask = (1.0 - smoothstep(borderWidth * 0.95, borderWidth * 1.05, mBDist));
                 if (bMask > 0.001) {
                     float borderRainbowCoord = fract(TexCoord.x * 1.8 - TexCoord.y * 0.7 + time * 0.2);
                     vec3 borderIridescenceOnly = texture(rainbowGradient, borderRainbowCoord).rgb * borderIridescenceIntensity * borderOverallIntensity;
                     finalIridDebug = mix(baseIridContribution, borderIridescenceOnly, bMask * borderTransparencyFactor);
                 }
             }
             debugColor = finalIridDebug;
         }
         else if (renderMode == 2) { // Specular/Gloss/Sheen Component Only
             vec3 baseSpecContribution = vec3(0.0);
             if (cardRarity == 1 && !isInsideArtwork) {
                 float specularFactor = pow(dotNH_mapped, reverseSpecularPower);
                 vec3 reverseSpec = typeColor * specularFactor * reverseSpecularIntensity;
                 baseSpecContribution = reverseSpec * l_systemMask * reversePatternMaskInfluence * reverseEffectIntensity;
             } else if (cardRarity >= 2) {
                 float glossSpecularFactor = pow(dotNH_mapped, glossySpecularPower);
                 vec3 glossySpec = vec3(1.0) * glossSpecularFactor * glossySpecularIntensity;
                 baseSpecContribution = glossySpec * glossyBlendFactor;
             }

             vec3 finalSpecDebug = baseSpecContribution;
             if (cardRarity >= 2) {
                 vec2 bDist = min(TexCoord, 1.0 - TexCoord); float mBDist = min(bDist.x, bDist.y);
                 float bMask = (1.0 - smoothstep(borderWidth * 0.95, borderWidth * 1.05, mBDist));
                 if (bMask > 0.001) {
                     float borderSheenFactor = pow(dotNH_smooth, borderSheenPower);
                     vec3 tintedSheenColor = mix(borderSheenBaseColor, typeColor, borderSheenTintStrength);
                     vec3 borderSheenOnly = tintedSheenColor * borderSheenFactor * borderSheenIntensity * borderOverallIntensity;
                     finalSpecDebug = mix(baseSpecContribution, borderSheenOnly, bMask * borderTransparencyFactor);
                 }
             }
             debugColor = finalSpecDebug;
         }
         else if (renderMode == 3) { // Fresnel + Border Base Only
            vec3 baseFresnelContribution = vec3(0.0);
            float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
            if (cardRarity == 0) {
                 baseFresnelContribution = vec3(0.8, 0.9, 1.0) * fresnelFactor * fresnelIntensityNormal;
            } else if (cardRarity == 1) {
                 if (!isInsideArtwork) {
                     vec3 reverseFresnel = mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnelFactor * reverseFresnelIntensity;
                     baseFresnelContribution = reverseFresnel * l_systemMask * reversePatternMaskInfluence * reverseEffectIntensity;
                 } else {
                     baseFresnelContribution = vec3(0.8, 0.9, 1.0) * fresnelFactor * fresnelIntensityNormal;
                 }
            } else if (cardRarity >= 2) {
                  vec3 glossyFresnel = mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnelFactor * glossyFresnelIntensity;
                  baseFresnelContribution = glossyFresnel * glossyBlendFactor;
            }

            vec3 finalFresnelDebug = baseFresnelContribution;
            if (cardRarity >= 2) {
                  vec2 bDist = min(TexCoord, 1.0 - TexCoord); float mBDist = min(bDist.x, bDist.y);
                  float bMask = (1.0 - smoothstep(borderWidth * 0.95, borderWidth * 1.05, mBDist));
                  if (bMask > 0.001) {
                      vec3 borderBaseOnly = typeColor * borderBaseMetalness * borderOverallIntensity;
                      finalFresnelDebug = mix(baseFresnelContribution, borderBaseOnly, bMask * borderTransparencyFactor);
                  }
            }
            debugColor = finalFresnelDebug;
         }
         else if (renderMode == 4) debugColor = litBaseColor;
         else if (renderMode == 5) { debugColor = overlay.rgb; finalAlpha = overlay.a; }
         else if (renderMode == 6) debugColor = N_final_base * 0.5 + 0.5;
         else if (renderMode == 7) debugColor = vec3(isInsideArtwork);
         else if (renderMode == 8) { // Effective Border Mask
              vec2 borderDist = min(TexCoord, 1.0 - TexCoord); float minBorderDist = min(borderDist.x, borderDist.y);
              float borderMaskVal = (1.0 - smoothstep(borderWidth * 0.95, borderWidth * 1.05, minBorderDist));
              debugColor = vec3(borderMaskVal * borderTransparencyFactor);
         }
         else if (renderMode == 9) { // Border Raw Color Only
              if (cardRarity >= 2) {
                  vec2 bDist = min(TexCoord, 1.0 - TexCoord); float mBDist = min(bDist.x, bDist.y);
                  float bMask = (1.0 - smoothstep(borderWidth * 0.95, borderWidth * 1.05, mBDist));
                  if (bMask > 0.001) {
                       vec3 baseBorderColor = typeColor * borderBaseMetalness;
                       float borderSheenFactor = pow(dotNH_smooth, borderSheenPower);
                       vec3 tintedSheenColor = mix(borderSheenBaseColor, typeColor, borderSheenTintStrength);
                       vec3 addedSheen = tintedSheenColor * borderSheenFactor * borderSheenIntensity;
                       float borderRainbowCoord = fract(TexCoord.x * 1.8 - TexCoord.y * 0.7 + time * 0.2);
                       vec3 addedIridescence = texture(rainbowGradient, borderRainbowCoord).rgb * borderIridescenceIntensity;
                       vec3 rawBorderColor = (baseBorderColor + addedSheen + addedIridescence) * borderOverallIntensity;
                       debugColor = rawBorderColor;
                  } else { debugColor = vec3(0.1); }
              } else { debugColor = vec3(0.0); }
         }
    }

    // --- Final Output ---
    FragColor = vec4(clamp(debugColor, 0.0, 1.0), finalAlpha);
}