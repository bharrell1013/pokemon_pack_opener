#version 300 es
precision highp float;

smooth in vec3 fragNorm;    // Interpolated model-space normal

out vec4 outCol;    // Final pixel color

void main() {
    // Visualize normals as colors
    outCol = vec4(1.0, 0.0, 0.0, 1.0);
}
