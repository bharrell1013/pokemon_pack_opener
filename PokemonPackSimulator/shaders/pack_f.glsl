#version 330 core

// Input data
in vec3 fragPosition; // World Space Position
in vec3 fragNormal;   // World Space Normal
in vec2 fragTexCoord; // Original Tex Coords from model
in vec3 modelPos;     // Model Space Position from vertex shader

// Output data
out vec4 fragColor;

// Uniforms
uniform vec3 viewPos;
uniform float shininess = 64.0;         // Pass from C++, perhaps ~32.0 or 64.0
uniform float specularStrength = 0.3;  // <<< NEW Uniform (pass from C++, e.g., 0.3 - 0.5)
uniform sampler2D basePackTexture;
uniform sampler2D pokemonOverlayTexture;
uniform vec3 packBaseColor;
uniform vec3 lightPos;
uniform vec3 lightColor;

uniform float overlayAmbientStrength = 0.5;  // e.g., 0.5
uniform float overlayDiffuseStrength = 0.7; // e.g., 0.7
uniform float overlaySpecularStrength = 0.05; // e.g., 0.05 (Keep low!)

void main() {
   // --- Common Lighting Vectors & Factors ---
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPos - fragPosition);
    vec3 V = normalize(viewPos - fragPosition);
    vec3 R = reflect(-L, N);
    float NdotL = max(dot(N, L), 0.0); // Diffuse factor
    float RdotV = max(dot(R, V), 0.0); // Specular base factor

    // --- Lighting Components ---
    float ambientStrength = 0.2; // Base ambient strength
    vec3 ambient = ambientStrength * lightColor;
    vec3 diffuse = NdotL * lightColor;
    float spec = pow(RdotV, shininess);
    vec3 specular = specularStrength * spec * lightColor; // Full specular calculation

    // --- Calculate Overlay UVs ---
    // !!! --- Use your TUNED values --- !!!
    float overlayMinX = -1.0;
    float overlayMaxX = 1.0;
    float overlayMinY = -1.0;
    float overlayMaxY = 1.0;
    // !!! --------------------------- !!!
    vec2 overlayUV;
    overlayUV.x = (modelPos.x - overlayMinX) / (overlayMaxX - overlayMinX);
    overlayUV.y = (modelPos.y - overlayMinY) / (overlayMaxY - overlayMinY);
    // Clamp slightly inside edges for line artifact
    overlayUV = clamp(overlayUV, 0.005, 0.995);

    // --- Texture Sampling ---
    vec4 baseSample = texture(basePackTexture, fragTexCoord);
    vec4 overlaySample = texture(pokemonOverlayTexture, overlayUV);

    // --- Calculate Border Fade Factor ---
    // (This is for the line artifact, tune threshold if needed)
    float borderThreshold = 0.015;
    float fadeX = smoothstep(0.0, borderThreshold, overlayUV.x) * smoothstep(1.0, 1.0 - borderThreshold, overlayUV.x);
    float fadeY = smoothstep(0.0, borderThreshold, overlayUV.y) * smoothstep(1.0, 1.0 - borderThreshold, overlayUV.y);
    float borderFadeFactor = min(fadeX, fadeY);

    // --- Color Calculation ---

    // 1. Base Color & Raw Overlay Color/Alpha
    vec3 baseColorRaw = packBaseColor * baseSample.rgb;
    vec3 overlayColorRaw = overlaySample.rgb;
    float overlayAlphaRaw = overlaySample.a;

    // 2. <<< Calculate FINAL Combined Alpha (incorporating border fade) >>>
    float finalCombinedAlpha = overlayAlphaRaw * borderFadeFactor;

    // 3. <<< Calculate BASE Lighting (Ambient + Diffuse only) >>>
    float ambientStrengthBase = 0.2; // Or make uniform
    vec3 ambientBase = ambientStrengthBase * lightColor;
    vec3 diffuseBase = NdotL * lightColor;
    vec3 litBaseDiffuseAmbient = baseColorRaw * (ambientBase + diffuseBase);

    // 4. <<< Calculate Specular Component for the BASE >>>
    float specBase = pow(RdotV, shininess); // Use base shininess
    vec3 specularBase = specularStrength * specBase * lightColor; // Use base strength
    vec3 baseSpecularComponent = baseColorRaw * specularBase;

    // 5. <<< Calculate LIT OVERLAY (using overlay strengths/shininess) >>>
    vec3 ambientOverlay = overlayAmbientStrength * lightColor;
    vec3 diffuseOverlay = overlayDiffuseStrength * NdotL * lightColor;
    // Use a separate (e.g., lower) shininess for overlay specular if desired
    float specOverlay = pow(RdotV, shininess * 0.5); // Example: softer highlight
    vec3 specularOverlay = overlaySpecularStrength * specOverlay * lightColor; // Use overlay LOW strength
    // Apply controlled lighting to the overlay color
    vec3 litOverlayLayer = overlayColorRaw * (ambientOverlay + diffuseOverlay + specularOverlay);

    // 6. <<< Calculate Final Lit Base (Masked Specular) >>>
    // Add the base's specular component, but only where the overlay isn't fully visible
    vec3 finalLitBase = litBaseDiffuseAmbient + baseSpecularComponent * (1.0 - finalCombinedAlpha);

    // 7. <<< Final Blend: Mix the LIT OVERLAY onto the MASKED-SPECULAR BASE >>>
    // Use finalCombinedAlpha to control the blend between the two pre-calculated lit layers
    vec3 finalColor = mix(finalLitBase, litOverlayLayer, finalCombinedAlpha);

    // --- Debugging ---
    // finalColor = vec3(finalCombinedAlpha); // Check final alpha mask
    // finalColor = litOverlayLayer;       // Check lit overlay look
    // finalColor = finalLitBase;          // Check lit base look with masked specular

    fragColor = vec4(finalColor, 1.0);
}