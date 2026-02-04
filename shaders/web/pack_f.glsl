#version 300 es
precision highp float;

// Input data
in vec3 fragPosition; // World Space Position
in vec3 fragNormal;   // World Space Normal
in vec2 fragTexCoord; // Original Tex Coords from model
in vec3 modelPos;     // Model Space Position from vertex shader

// Output data
out vec4 fragColor;

// Uniforms
uniform vec3 viewPos;
uniform float shininess;         // Pass from C++, perhaps ~32.0 or 64.0
uniform float specularStrength;  // Specular strength from C++ (e.g., 0.3 - 0.5)
uniform sampler2D basePackTexture;
uniform sampler2D pokemonOverlayTexture;
uniform vec3 packBaseColor;
uniform vec3 lightPos;
uniform vec3 lightColor;

void main() {
    // Normalize the inputs
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPosition);
    vec3 viewDir = normalize(viewPos - fragPosition);

    // Ambient
    vec3 ambient = 0.2 * lightColor;

    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (Phong)
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor; // Full specular calculation

    // Sample textures
    vec4 baseSample = texture(basePackTexture, fragTexCoord);

    // Overlay texture coordinates (based on model space position)
    vec2 overlayUV = modelPos.xz * 0.5 + vec2(0.5); // Example mapping, adjust as needed
    vec4 overlaySample = texture(pokemonOverlayTexture, overlayUV);

    // Combine base texture with overlay
    vec3 combinedColor = mix(baseSample.rgb, overlaySample.rgb, overlaySample.a);

    // Final color
    vec3 result = (ambient + diffuse + specular) * combinedColor;

    fragColor = vec4(result, baseSample.a);
}
