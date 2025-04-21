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
const float holoIntensity = 0.8;

// Normal/Parallax Control (Subtle base effect for ALL cards)
const float normalMapStrength = 0.05; // Subtle texture effect
const float parallaxHeightScale = 0.01; // Very subtle depth
const int parallaxMinSteps = 4;
const int parallaxMaxSteps = 16;

// Specular Glow (Used for Holos)
const float specularPower = 70.0;  // Sharp highlight
const float specularIntensity = 0.9f; // Moderate brightness for holo glare

// View-Dependent Color Shift (Used for Holos)
const float iridescenceIntensity = 1.3; // Rainbow brightness
const float iridescenceFreq = 2.5;    // Speed of color change with angle
const float iridescenceContrast = 1.5; // Angle sensitivity contrast

// Fresnel (Subtle Edge Glow - Used for Holos, maybe tiny bit for base)
const float baseFresnelPower = 2.3;
const float baseFresnelIntensityHolo = 0.25; // Fresnel for holo cards
const float baseFresnelIntensityNonHolo = 0.03; // Tiny bit for normal cards

// L-System Mask Control (For Holos)
const float patternMaskInfluence = 0.85; // Effects mostly visible through mask

// Border Effect (For Holos)
const float borderWidth = 0.04;
const float borderFadeFactor = 0.7;
const float borderIntensityMultiplier = 2.0; // Brightness of border effect

// --- Constants ---
const float PI = 3.1415926535;

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

    // --- Calculate Base Texture Lighting (ALL Cards) ---
    float diffuseFactor = max(0.0, dot(N_final_base, L));
    float dotNH_base = clamp(dot(N_final_base, H), 0.0, 1.0);
    float microSpecFactor = pow(dotNH_base, 15.0);
    vec3 litBaseColor = baseColor.rgb * (vec3(0.3) + vec3(0.7) * diffuseFactor);
    litBaseColor += vec3(0.08) * microSpecFactor;
    // Add subtle non-holo fresnel to base
    float fresnelNonHolo = pow(1.0 - dotNV, baseFresnelPower);
    litBaseColor += vec3(0.8, 0.9, 1.0) * fresnelNonHolo * baseFresnelIntensityNonHolo;


    // --- Initialize Holo Effects ---
    vec3 appliedSpecular = vec3(0.0);         // CALCULATED IF RARITY >= 1
    vec3 appliedIridescenceColor = vec3(0.0); // CALCULATED IF RARITY >= 1
    vec3 appliedFresnel = vec3(0.0);          // CALCULATED IF RARITY >= 1
    vec3 calculatedBorderColor = vec3(0.0);   // <<< DECLARED OUTSIDE, CALCULATED IF RARITY >= 2
    vec3 holoLayer = vec3(0.0);

    // --- Calculate Effects for HOLO Cards (Rarity 1+) ---
    if (cardRarity >= 1) { // For Reverse Holo and up
        // Specular Glow (Smooth Normal)
        float dotNH_smooth = clamp(dot(N_vert, H), 0.0, 1.0);
        float specularFactor = pow(dotNH_smooth, specularPower);
        // Assign to appliedSpecular declared outside
        appliedSpecular = vec3(1.0) * specularFactor * specularIntensity;

        // View-Dependent Iridescence
        float viewFactor = pow(dotNV, iridescenceContrast);
        float rainbowCoord = fract(viewFactor * iridescenceFreq + time * 0.1);
        vec3 iridescentColor = texture(rainbowGradient, rainbowCoord).rgb;
        // Assign to appliedIridescenceColor declared outside
        appliedIridescenceColor = iridescentColor * iridescenceIntensity;

        // Fresnel Glow (Holo version)
        float fresnel = pow(1.0 - dotNV, baseFresnelPower);
         // Assign to appliedFresnel declared outside
        appliedFresnel = vec3(0.8, 0.9, 1.0) * fresnel * baseFresnelIntensityHolo;

        // --- Border Effect (Calculate only for Rarity >= 2) ---
        if (cardRarity >= 2) { // <<< NESTED RARITY CHECK FOR BORDER
            vec2 borderDist = min(TexCoord, 1.0 - TexCoord);
            float minBorderDist = min(borderDist.x, borderDist.y);
            float borderMask = 1.0 - smoothstep(borderWidth * borderFadeFactor, borderWidth, minBorderDist);
            if (borderMask > 0.0) {
                float borderRainbowCoord = fract(TexCoord.x * 2.5 - TexCoord.y * 1.0 + time * 0.2);
                vec3 borderRainbow = texture(rainbowGradient, borderRainbowCoord).rgb;
                // Use appliedSpecular (which is calculated in the outer Rarity >= 1 block)
                // Assign to calculatedBorderColor declared outside
                calculatedBorderColor = mix(borderRainbow, appliedSpecular + vec3(0.2), 0.6);
                calculatedBorderColor *= borderMask * borderIntensityMultiplier;
            }
        } // <<< END NESTED RARITY CHECK (Rarity >= 2)

        // Combine Holo Layer (Modulated by Mask) - uses variables calculated above
        holoLayer = appliedSpecular * (1.0 - patternMaskInfluence + l_systemMask * patternMaskInfluence);
        holoLayer += appliedIridescenceColor * l_systemMask * patternMaskInfluence;
        holoLayer += appliedFresnel * l_systemMask * patternMaskInfluence;
        holoLayer += calculatedBorderColor; // Add border (will be 0.0 if rarity was 1)
    } // <<< END RARITY CHECK (Rarity >= 1)

    // --- Final Combination ---
    // holoIntensity acts as master switch/scaler (still useful uniform maybe?)
    vec3 finalColor = litBaseColor + holoLayer * holoIntensity;

    // Dummy usage for cardType
    //finalColor.r += float(cardType) * 0.0000001;

    // --- Debug Modes (Apply to all cards) ---
    vec3 debugColor = finalColor;
     float debugAlpha = baseColor.a;
     if (renderMode == 1) debugColor = appliedIridescenceColor * l_systemMask;
     else if (renderMode == 2) debugColor = appliedSpecular;
     else if (renderMode == 3) debugColor = calculatedBorderColor + appliedFresnel * l_systemMask; // Show border + masked fresnel
     else if (renderMode == 4) debugColor = litBaseColor;
     else if (renderMode == 5) { debugColor = overlay.rgb; debugAlpha = overlay.a; }
     else if (renderMode == 6) debugColor = N_final_base * 0.5 + 0.5;
     else if (renderMode == 7) debugColor = vec3(dotNV);

    FragColor = vec4(clamp(debugColor, 0.0, 1.0), debugAlpha);
}