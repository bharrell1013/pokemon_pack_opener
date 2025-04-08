#ifndef MESH_HPP
#define MESH_HPP

#include "gl_core_3_3.h"
#include <string>
#include <vector>
#include <utility>
#include <glm/glm.hpp>

class Mesh {
public:
    // Default constructor for creating empty mesh
    Mesh() : vao(0), vbuf(0), vcount(0) {}
    
    // Constructor from file
    Mesh(std::string filename, bool keepLocalGeometry = false);
    ~Mesh() { release(); }

    // Initialize mesh with vertex data
    void initialize(const std::vector<float>& vertexData, const std::vector<unsigned int>& indices);

    // Disallow copy, move, & assignment
    Mesh(const Mesh& other) = delete;
    Mesh& operator=(const Mesh& other) = delete;
    Mesh(Mesh&& other) = delete;
    Mesh& operator=(Mesh&& other) = delete;

	// Return the bounding box of this object
	std::pair<glm::vec3, glm::vec3> boundingBox() const
	{ return std::make_pair(minBB, maxBB); }

	void load(std::string filename, bool keepLocalGeometry = false);
	void draw() const;
	void render() { draw(); } // Alias for draw() to maintain consistent naming

	// Mesh vertex format
	struct Vertex {
		glm::vec3 pos;		// Position
		glm::vec3 norm;		// Normal
		glm::vec2 texCoord; // Texture Coordinates
	};
	// Local geometry data
	std::vector<Vertex> vertices;

protected:
	void release();		// Release OpenGL resources

	// Bounding box
	glm::vec3 minBB;
	glm::vec3 maxBB;

	// OpenGL resources
	GLuint vao;		// Vertex array object
	GLuint vbuf;	// Vertex buffer
	GLsizei vcount;	// Number of vertices
	GLuint ebo = 0; // Element Buffer Object ID (0 if not used)

private:
};

#endif
