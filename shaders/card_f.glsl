#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D cardTexture;
uniform int cardType;    // Pokemon type (0-11 for different types)
uniform int cardRarity;  // Card rarity (0-4 for different rarities)

// Type-based color tints
const vec3 typeColors[12] = vec3[12](
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

void main()
{
    // Sample base texture
    vec4 texColor = texture(cardTexture, TexCoord);
    
    // Apply type-based color tint
    vec3 typeColor = typeColors[cardType];
    vec3 finalColor = mix(texColor.rgb, typeColor, 0.2); // Subtle type-based tinting
    
    // Adjust based on rarity
    float brightness = 1.0;
    float contrast = 1.0;
    
    if (cardRarity > 0) {
        brightness = 1.1;  // Slightly brighter for rare cards
        contrast = 1.2;    // More contrast for rare cards
    }
    
    // Apply brightness and contrast
    finalColor = (finalColor - 0.5) * contrast + 0.5;
    finalColor *= brightness;
    
    FragColor = vec4(finalColor, texColor.a);
}