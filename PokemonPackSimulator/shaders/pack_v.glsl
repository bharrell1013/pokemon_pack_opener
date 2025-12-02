#version 330 core

// Input vertex data
layout(location = 0) in vec3 position; // This is MODEL SPACE position
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

// Output data
out vec3 fragPosition; // World Space Position
out vec3 fragNormal;   // World Space Normal
out vec2 fragTexCoord; // Original Tex Coords
out vec3 modelPos;     // <<< NEW: Pass Model Space Position

// Uniforms
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Calculate position in world space
    fragPosition = vec3(model * vec4(position, 1.0));

    // Calculate normal in world space
    // Avoid recalculating inverse if using normal matrix later
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    fragNormal = normalize(normalMatrix * normal); // Normalize here

    // Pass through texture coordinates
    fragTexCoord = texCoord;

    // <<< NEW: Pass model space position directly >>>
    modelPos = position;

    // Calculate position in clip space
    gl_Position = projection * view * vec4(fragPosition, 1.0); // Use world pos here
}