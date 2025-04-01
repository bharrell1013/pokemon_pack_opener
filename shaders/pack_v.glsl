#version 330 core

// Input vertex data
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 texCoord;

// Output data
out vec3 fragPosition;
out vec3 fragNormal;
out vec2 fragTexCoord;

// Uniforms
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Calculate position in world space
    fragPosition = vec3(model * vec4(position, 1.0));
    
    // Calculate normal in world space
    fragNormal = mat3(transpose(inverse(model))) * normal;
    
    // Pass through texture coordinates
    fragTexCoord = texCoord;
    
    // Calculate position in clip space
    gl_Position = projection * view * model * vec4(position, 1.0);
}