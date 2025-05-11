#version 330

smooth in vec3 fragNorm;	// Interpolated model-space normal

out vec3 outCol;	// Final pixel color

void main() {
	// Visualize normals as colors
	outCol = vec3(1.0, 0.0, 0.0);
}
