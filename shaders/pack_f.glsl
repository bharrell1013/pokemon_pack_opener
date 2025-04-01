#version 330 core

// Input data
in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragTexCoord;

// Output data
out vec4 fragColor;

// Uniforms
uniform vec3 viewPos;
uniform sampler2D diffuseTexture;
uniform float shininess;

// Light properties
uniform vec3 lightPos = vec3(1.0, 1.0, 2.0);
uniform vec3 lightColor = vec3(1.0, 1.0, 1.0);

void main() {
    // Ambient lighting
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;
    
    // Diffuse lighting
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPosition);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - fragPosition);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;
    
    // Apply lighting to texture
    vec3 result = (ambient + diffuse + specular) * texture(diffuseTexture, fragTexCoord).rgb;
    fragColor = vec4(result, 1.0);
}