#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec3 WorldPos;
in vec3 Normal;

uniform sampler2D cardTexture;
uniform float time;
uniform float holoIntensity;
uniform int cardType;

// Type-based holo colors
const vec3 holoColors[12] = vec3[12](
    vec3(0.8, 0.8, 0.8),   // Normal
    vec3(1.0, 0.4, 0.2),   // Fire
    vec3(0.2, 0.4, 1.0),   // Water
    vec3(0.4, 0.8, 0.2),   // Grass
    vec3(1.0, 0.8, 0.0),   // Electric
    vec3(0.8, 0.2, 0.8),   // Psychic
    vec3(0.8, 0.4, 0.0),   // Fighting
    vec3(0.4, 0.2, 0.4),   // Dark
    vec3(0.6, 0.4, 1.0),   // Dragon
    vec3(1.0, 0.6, 0.8),   // Fairy
    vec3(0.6, 0.6, 0.8),   // Steel
    vec3(0.4, 0.2, 0.6)    // Ghost
);

// Noise function for holographic pattern
float rand(vec2 co) {
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

vec2 rotate2D(vec2 p, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    mat2 rot = mat2(c, -s, s, c);
    return rot * p;
}

void main()
{
    // Sample base texture
    vec4 texColor = texture(cardTexture, TexCoord);
    
    // Create holographic pattern
    vec2 uv = TexCoord;
    uv = rotate2D(uv - 0.5, time * 0.2) + 0.5; // Rotate UV coordinates
    
    float noiseScale = 30.0;
    float noise = rand(uv * noiseScale + time);
    
    // Create rainbow pattern
    vec2 rainbowUV = uv;
    rainbowUV.x += sin(rainbowUV.y * 10.0 + time) * 0.02;
    float rainbow = sin(rainbowUV.x * 20.0 + time) * 0.5 + 0.5;
    
    // Mix base color with holographic effect
    vec3 holoColor = holoColors[cardType];
    vec3 rainbowColor = mix(holoColor, vec3(rainbow), 0.5);
    vec3 finalColor = mix(texColor.rgb, rainbowColor, noise * holoIntensity);
    
    // Add sparkle effect
    float sparkleScale = 50.0;
    float sparkle = pow(rand(floor(uv * sparkleScale) + time), 10.0);
    finalColor += sparkle * holoIntensity;
    
    // Add view-dependent shimmer
    vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0) - WorldPos);
    float fresnel = pow(1.0 - max(dot(Normal, viewDir), 0.0), 3.0);
    finalColor += fresnel * holoColor * holoIntensity;
    
    FragColor = vec4(finalColor, texColor.a);
}