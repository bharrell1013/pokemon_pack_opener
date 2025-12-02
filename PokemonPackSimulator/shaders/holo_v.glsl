// shaders/holo_v.glsl (Restore for Final Effects)
#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec2 TexCoord;
out vec3 WorldPos;
out vec3 WorldNormal;
out vec3 WorldTangent;
out vec3 WorldBitangent;
out vec3 WorldViewDir;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform vec3 viewPos;

void main()
{
    vec4 worldPosVec4 = model * vec4(aPos, 1.0);
    WorldPos = vec3(worldPosVec4); // Use worldPosVec4.xyz / worldPosVec4.w if perspective issues arise
    gl_Position = projection * view * worldPosVec4;
    TexCoord = aTexCoord;
    WorldNormal    = normalize(normalMatrix * aNormal);
    WorldTangent   = normalize(normalMatrix * aTangent);
    WorldBitangent = normalize(normalMatrix * aBitangent);
    WorldViewDir = normalize(viewPos - WorldPos);
}