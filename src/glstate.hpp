#ifndef GLSTATE_HPP
#define GLSTATE_HPP

#include "gl_core_3_3.h"
#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "mesh.hpp"

// Manages OpenGL state, e.g. camera transform, objects, shaders
class GLState {
public:
	GLState();
	~GLState();
	// Disallow copy, move, & assignment
	GLState(const GLState& other) = delete;
	GLState& operator=(const GLState& other) = delete;
	GLState(GLState&& other) = delete;
	GLState& operator=(GLState&& other) = delete;

	// Callbacks
	void initializeGL();
	void paintGL();
	void resizeGL(int w, int h);

	// Mouse control of the objects
	void moveBall(glm::vec2 mousePos);
	void moveLine(glm::vec2 mousePos);

	// Set object to display
	void showObjFile(const std::string& filename);

	// Per-vertex attributes
	struct Vertex {
		glm::vec3 pos;		// Position
		glm::vec3 norm;		// Normal
	};

	// window size:
	static int width, height;  // 600 by default

protected:
	// Initialization
	void initShaders();
	void initLineGeometry();

	// Camera state
	int w, h;		// Width and height of the window
	float fovy;				// Vertical field of view in degrees
	glm::vec3 camCoords;	// Camera spherical coordinates
	glm::vec2 initMousePos;	// Initial mouse position on click

	// Object state
	enum ObjMode {
		OBJMODE_MESH,				// Show a given obj file
	};
	ObjMode objMode;				// Which object state are we in
	std::string meshFilename;		// Name of the obj file being shown
	std::unique_ptr<Mesh> mesh;		// Pointer to mesh object

	// OpenGL states of the tetrahedron
	GLuint shader;		// GPU shader program
	GLuint xformLoc;	// Transformation matrix location
	GLuint vao;			// Vertex array object
	GLuint vbuf;		// Vertex buffer
	GLuint ibuf;		// Index buffer
	GLsizei vcount;		// Number of indices to draw

	// OpenGL states of the platform
	GLuint lineShader;		// GPU shader program
	GLuint lineXformLoc;	// vertical position of the bar on screen
	GLuint lineVao;			// Vertex array object
	GLuint lineVbuf;		// Vertex buffer
	GLuint lineIbuf;		// Index buffer
	GLsizei lineVcount;		// Number of indices to draw

	// Object states:
	glm::vec2 initBallPos = glm::vec2(0.0f, 0.0f);  // initial xy position of the ball
	glm::vec2 ballPos;								// position of the ball
	GLfloat initLinePos = -1.0f;					// initial position of the bar (appear at the bottom of screen)
	GLfloat linePos;								// vertical position of the bar
};

#endif
