#version 300 es
precision highp float;

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

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    WorldPos = worldPos.xyz;
    WorldNormal = normalize(normalMatrix * aNormal);
    WorldTangent = normalize(normalMatrix * aTangent);
    WorldBitangent = normalize(normalMatrix * aBitangent);

    vec3 cameraPos = vec3(inverse(view)[3]);
    WorldViewDir = normalize(cameraPos - WorldPos);

    TexCoord = aTexCoord;
    gl_Position = projection * view * worldPos;
}
