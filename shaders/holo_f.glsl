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
const float baseFresnelPower = 2.8; // Used by all fresnel calculations

// ** Normal Card Specific (Rarity 0) **
const float normalOverlayIntensity = 0.3;
const float fresnelIntensityNormal = 0.03;

// ** Reverse Holo Specific (Rarity 1) - Based on ORIGINAL shader logic **
const float reversePatternMaskInfluence = 0.85; // How much the pattern reveals the effects below it
const float reverseEffectIntensity = 0.2;      // Overall brightness scale for effects under the pattern
const float reverseSpecularPower = 50.0;       // Sharpness for effect under pattern
const float reverseSpecularIntensity = 0.5f;   // Brightness for effect under pattern
const float reverseIridescenceIntensity = 0.9; // Rainbow brightness under pattern
const float reverseIridescenceFreq = 2.5;
const float reverseIridescenceContrast = 1.5;
const float reverseFresnelIntensity = 0.25;    // Edge glow brightness under pattern

// ** Glossy (Rarity >= 2) Specific **
const float glossySpecularPower = 60.0;
const float glossySpecularIntensity = 0.7f;
const float glossyIridescenceIntensity = 0.8;
const float glossyIridescenceFreq = 2.5;      // Can share freq/contrast if desired
const float glossyIridescenceContrast = 1.5;
const float glossyFresnelIntensity = 0.35;
const float overallGlossyIntensity = 0.4;     // Master scale for glossy effects ADDED to base

// ** Border Effect (For Glossy Cards, Rarity >= 2) **
const float borderWidth = 0.035;
const float borderShinePower = 30.0;
const float borderIntensityMultiplier = 1.5;

// --- Constants ---
const float PI = 3.1415926535;

// Type Colors
const vec3 typeColors[12] = vec3[](
    vec3(0.9, 0.9, 0.8), vec3(1.0, 0.5, 0.2), vec3(0.3, 0.7, 1.0), // Norm, Fire, Water
    vec3(0.4, 0.9, 0.4), vec3(1.0, 1.0, 0.3), vec3(0.9, 0.5, 0.9), // Grass, Light, Psychic
    vec3(0.8, 0.6, 0.3), vec3(0.5, 0.5, 0.6), vec3(0.6, 0.4, 0.9), // Fight, Dark, Dragon
    vec3(1.0, 0.7, 0.9), vec3(0.7, 0.7, 0.8), vec3(0.6, 0.4, 0.8)  // Fairy, Metal, Ghost
);

// --- Parallax function (Keep implementation) ---
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

    // --- Sample Base and Overlay ---
    vec4 baseColor = texture(baseTexture, parallaxTexCoord);
    vec4 overlay = texture(overlayTexture, TexCoord);
    float l_systemMask = overlay.a;
    vec3 l_systemColor = overlay.rgb; // Only used directly by Normal

    // --- Calculate Base Texture Lighting ---
    float diffuseFactor = max(0.0, dot(N_final_base, L));
    float dotNH_base = clamp(dot(N_final_base, H), 0.0, 1.0);
    float microSpecFactor = pow(dotNH_base, 15.0);
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

    // --- Rarity-Specific Logic ---
    bool isInsideArtwork = TexCoord.x > artworkRectMin.x && TexCoord.x < artworkRectMax.x &&
                           TexCoord.y > artworkRectMin.y && TexCoord.y < artworkRectMax.y;

    if (cardRarity == 0) { // --- Normal Card ---
        // Blend the subtle grey L-System pattern using its alpha
        finalColor = mix(finalColor, l_systemColor, l_systemMask * normalOverlayIntensity);
        // Add subtle non-holo fresnel
        float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
        addedEffects += vec3(0.8, 0.9, 1.0) * fresnelFactor * fresnelIntensityNormal;

    } else if (cardRarity == 1) { // --- Reverse Holo Card (Restored Original Logic) ---
        if (!isInsideArtwork) {
            // Calculate the underlying holo effects (specular, iridescence, fresnel)
            // Use Reverse Holo specific tuning parameters

            // Specular Component
            float specularFactor = pow(dotNH_smooth, reverseSpecularPower);
            vec3 underlyingSpecular = typeColor * specularFactor * reverseSpecularIntensity;

            // Iridescence Component
            float viewFactor = pow(dotNV, reverseIridescenceContrast);
            float rainbowCoord = fract(viewFactor * reverseIridescenceFreq + time * 0.1);
            vec3 iridescentColor = texture(rainbowGradient, rainbowCoord).rgb;
            vec3 underlyingIridescence = mix(iridescentColor, typeColor, 0.4) * reverseIridescenceIntensity;

            // Fresnel Component
            float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
            vec3 underlyingFresnel = mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnelFactor * reverseFresnelIntensity;

            // Combine underlying effects
            vec3 combinedUnderlyingEffects = underlyingSpecular + underlyingIridescence + underlyingFresnel;

            // Modulate the combined effects by the L-System Mask's Alpha
            // This makes the effects only visible *through* the pattern.
            addedEffects += combinedUnderlyingEffects
                           * l_systemMask // <<< CRITICAL MASKING STEP
                           * reversePatternMaskInfluence // Control how strongly mask reveals effects
                           * reverseEffectIntensity;     // Overall brightness scale for the masked effect

        } else {
            // Inside artwork, apply only the minimal normal fresnel
             float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
             addedEffects += vec3(0.8, 0.9, 1.0) * fresnelFactor * fresnelIntensityNormal;
        }

    } else { // --- Glossy Cards (Holo, EX, Full Art) cardRarity >= 2 ---
        // Apply glossy effects everywhere (unmasked)

        // 1. Glossy Specular Highlight
        float glossSpecularFactor = pow(dotNH_smooth, glossySpecularPower);
        addedEffects += typeColor * glossSpecularFactor * glossySpecularIntensity;

        // 2. Iridescence
        float viewFactor = pow(dotNV, glossyIridescenceContrast);
        float rainbowCoord = fract(viewFactor * glossyIridescenceFreq + time * 0.1);
        vec3 iridescentColor = texture(rainbowGradient, rainbowCoord).rgb;
        addedEffects += mix(iridescentColor, typeColor, 0.4) * glossyIridescenceIntensity;

        // 3. Fresnel Edge Glow
        float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
        addedEffects += mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnelFactor * glossyFresnelIntensity;

        // Scale accumulated main glossy effects by overall intensity
        addedEffects *= overallGlossyIntensity;

        // 4. Shiny Type-Colored Border (Added separately)
        vec2 borderDist = min(TexCoord, 1.0 - TexCoord);
        float minBorderDist = min(borderDist.x, borderDist.y);
        float borderMask = 1.0 - smoothstep(borderWidth * 0.9, borderWidth * 1.1, minBorderDist);
              borderMask *= smoothstep(0.0, borderWidth * 0.1, minBorderDist); // Fade inwards

        if (borderMask > 0.01) {
             float borderShineFactor = pow(dotNH_smooth, borderShinePower);
             vec3 borderBaseColor = typeColor * 0.8;
             vec3 borderShineColor = vec3(1.0) * borderShineFactor;
             vec3 calculatedBorderColor = mix(borderBaseColor, borderShineColor, 0.7) * borderMask * borderIntensityMultiplier;
             addedEffects += calculatedBorderColor; // Add border effect
        }
    }

    // --- Combine Base Color and Calculated Effects ---
    finalColor += addedEffects;

    // --- Final Alpha ---
    float finalAlpha = baseColor.a;
    // For Reverse Holo, allow the pattern mask to potentially reveal effects even if base is transparent
    if(cardRarity == 1 && !isInsideArtwork) {
        // Alpha is combination of base transparency and how much pattern is visible
        finalAlpha = max(baseColor.a, l_systemMask * reversePatternMaskInfluence * reverseEffectIntensity);
    }


    // --- Debug Modes (Minor update for clarity) ---
    vec3 debugColor = finalColor;
    if (renderMode != 0) {
         debugColor = vec3(0.0);
         if (renderMode == 1) { // Iridescence
             if(cardRarity==1 && !isInsideArtwork) debugColor = texture(rainbowGradient, fract(pow(dotNV, reverseIridescenceContrast) * reverseIridescenceFreq + time * 0.1)).rgb * reverseIridescenceIntensity * l_systemMask * reversePatternMaskInfluence * reverseEffectIntensity; // Show masked effect
             else if (cardRarity >=2) debugColor = mix(texture(rainbowGradient, fract(pow(dotNV, glossyIridescenceContrast) * glossyIridescenceFreq + time * 0.1)).rgb, typeColor, 0.4) * glossyIridescenceIntensity * overallGlossyIntensity; // Show glossy effect
         }
         else if (renderMode == 2) { // Specular (Reverse) or Gloss (Glossy)
             if (cardRarity == 1 && !isInsideArtwork) debugColor = typeColor * pow(dotNH_smooth, reverseSpecularPower) * reverseSpecularIntensity * l_systemMask * reversePatternMaskInfluence * reverseEffectIntensity; // Show masked specular
             else if (cardRarity >= 2) debugColor = typeColor * pow(dotNH_smooth, glossySpecularPower) * glossySpecularIntensity * overallGlossyIntensity; // Show glossy specular
         }
         else if (renderMode == 3) { // Fresnel + Border
             float fresnelFactor = pow(1.0 - dotNV, baseFresnelPower);
             if(cardRarity==0) debugColor = vec3(0.8, 0.9, 1.0) * fresnelFactor * fresnelIntensityNormal;
             else if(cardRarity==1 && !isInsideArtwork) debugColor = mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnelFactor * reverseFresnelIntensity * l_systemMask * reversePatternMaskInfluence * reverseEffectIntensity; // Masked fresnel
             else if(cardRarity==1 && isInsideArtwork) debugColor = vec3(0.8, 0.9, 1.0) * fresnelFactor * fresnelIntensityNormal;
             else if (cardRarity >= 2) { // Glossy fresnel + Border
                 debugColor = mix(vec3(0.8, 0.9, 1.0), typeColor, 0.3) * fresnelFactor * glossyFresnelIntensity * overallGlossyIntensity;
                 vec2 borderDist = min(TexCoord, 1.0 - TexCoord); float minBorderDist = min(borderDist.x, borderDist.y);
                 float borderMask = 1.0 - smoothstep(borderWidth * 0.9, borderWidth * 1.1, minBorderDist); borderMask *= smoothstep(0.0, borderWidth * 0.1, minBorderDist);
                 if (borderMask > 0.01) { float borderShineFactor = pow(dotNH_smooth, borderShinePower); vec3 borderBaseColor = typeColor * 0.8; vec3 borderShineColor = vec3(1.0) * borderShineFactor; debugColor += mix(borderBaseColor, borderShineColor, 0.7) * borderMask * borderIntensityMultiplier; }
             }
         }
         else if (renderMode == 4) debugColor = litBaseColor;
         else if (renderMode == 5) { debugColor = overlay.rgb; finalAlpha = overlay.a; }
         else if (renderMode == 6) debugColor = N_final_base * 0.5 + 0.5;
         else if (renderMode == 7) debugColor = vec3(isInsideArtwork);
    }

    FragColor = vec4(clamp(debugColor, 0.0, 1.1), finalAlpha);
}