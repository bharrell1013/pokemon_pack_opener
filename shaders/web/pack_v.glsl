#version 300 es
precision highp float;

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
    fragNormal = mat3(transpose(inverse(model))) * normal; // Normal in world space
    fragTexCoord = texCoord;
    modelPos = position; // Store model space position for overlay mapping

    // Final position
    gl_Position = projection * view * vec4(fragPosition, 1.0);
}
