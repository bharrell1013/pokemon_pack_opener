#define NOMINMAX
#include "glstate.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include "util.hpp"
#include <iostream>
#include <chrono>  // for high_resolution_clock
#include <cmath>

int GLState::width = 600;
int GLState::height = 600;

// Constructor
GLState::GLState() :
	w(1), h(1),
	fovy(45.0f),
	camCoords(0.0f, 0.0f, 2.0f),
	objMode(OBJMODE_MESH),  // OBJMODE_TETRAHEDRON
	// states of the tetrahedron:
	shader(0),
	xformLoc(0),
	vao(0),
	vbuf(0),
	ibuf(0),
	vcount(0),
	// states of the platform:
	lineShader(0),
	lineXformLoc(0),
	lineVao(0),
	lineVbuf(0),
	lineIbuf(0),
	lineVcount(0),
	// position:
	ballPos(glm::vec2(0.0f)),
	linePos(-1.0f)
{
	if (objMode == OBJMODE_MESH)
		this->showObjFile("models/sphere.obj");  // load the sphere at the start
}

// Destructor
GLState::~GLState() {
	// Release OpenGL resources
	if (shader)	glDeleteProgram(shader);
	if (vao)	glDeleteVertexArrays(1, &vao);
	if (vbuf)	glDeleteBuffers(1, &vbuf);
	if (ibuf)	glDeleteBuffers(1, &ibuf);

	if (lineShader)	glDeleteProgram(lineShader);
	if (lineVao)	glDeleteVertexArrays(1, &lineVao);
	if (lineVbuf)	glDeleteBuffers(1, &lineVbuf);
	if (lineIbuf)	glDeleteBuffers(1, &lineIbuf);
}

// Called when OpenGL context is created (some time after construction)
void GLState::initializeGL() {
	// General settings
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);

	// Initialize OpenGL state
	initShaders();
	initLineGeometry();
}

// ################### TODO1 ###################
// define global variables controlling ball bouncing (e.g., location of the ball, velocity, acceleration, radius of the ball, etc.).
static GLfloat yLoc = 0.0f;		// height of the ball

// Called when window requests a screen redraw
void GLState::paintGL() {
	// Clear the color and depth buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// #################### First pass: draw the ball / other object ####################
	// Set shader to draw with
	glUseProgram(shader);

	switch (objMode) {
	case OBJMODE_MESH: {
		glm::mat4 objXform = glm::scale(glm::mat4(1.0f), glm::vec3(0.3f));  // the transformation matrix of the object
		// when right click, translate the ball to the cursor
		objXform = glm::translate(glm::vec3(initBallPos.x - ballPos.x, 0.0f, 0.0f)) * objXform;  // move the ball to the cursor

		// ################### TODO2 ###################
		// update the status variables and implement bouncing effect

		// ################### TODO3 ###################
		// use "glm::translate" the ball (pass "yLoc" to the function) to simulate its drop and bounce.

		glUniformMatrix4fv(xformLoc, 1, GL_FALSE, glm::value_ptr(objXform));

		// Draw the mesh
		mesh->draw();
		break; }
	}

	glUseProgram(0);

	// #################### Second pass: draw the line platform ####################
	// Set shader to draw with
	glUseProgram(lineShader);

	glm::mat4 lineXform = glm::mat4(1.0f);
	// ################### TODO4 ###################
	// when left click, move the bar to the position of the cursor;
	// use "glm::translate" to implement it.

	// Send transform matrix to shader
	glUniformMatrix4fv(lineXformLoc, 1, GL_FALSE, glm::value_ptr(lineXform));

	// Use our vertex format and buffers
	glBindVertexArray(lineVao);
	// Draw the geometry
	glLineWidth((GLfloat)10.0f);  // define the width of the bar
	glDrawElements(GL_LINES, lineVcount, GL_UNSIGNED_INT, NULL);  // NOTE: use GL_LINE will fail to draw the line
	// Cleanup state
	glBindVertexArray(0);

	glUseProgram(0);  // end second pass
}

// Called when window is resized
void GLState::resizeGL(int w, int h) {
	// Tell OpenGL the new dimensions of the window
	width = w;
	height = h;
	glViewport(0, 0, w, h);
}

void GLState::moveBall(glm::vec2 mousePos) {  // x: col; y: row
	// get the current position of the ball by converting mousePos (in pixel space) to OpenGL space:
	ballPos.x = (GLState::width / 2 - mousePos.x) / static_cast<float>(GLState::width / 2);
	ballPos.y = (GLState::height / 2 - mousePos.y) / static_cast<float>(GLState::height / 2);

	// update the status variables of the ball (e.g., velocity, position):
	yLoc = ballPos.y;  // update the height status variable of the ball

	// ################### TODO5 ###################
	// reset the velocity you defined.
}

void GLState::moveLine(glm::vec2 mousePos) {
	// get the current position of the bar by converting mousePos (in pixel space) to OpenGL space:
	linePos = (GLState::height / 2 - mousePos.y) / static_cast<float>(GLState::height / 2);
}

// Display a given .obj file
void GLState::showObjFile(const std::string& filename) {
	std::cout << filename << std::endl;
	// Load the .obj file if it's not already loaded
	if (!mesh || meshFilename != filename) {
		mesh = std::unique_ptr<Mesh>(new Mesh(filename));
		meshFilename = filename;
	}
	objMode = OBJMODE_MESH;
}

// Create shaders and associated state
void GLState::initShaders() {
	// Compile and link shader files
	std::vector<GLuint> shaders;
	shaders.push_back(compileShader(GL_VERTEX_SHADER, "shaders/v.glsl"));
	shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "shaders/f.glsl"));
	shader = linkProgram(shaders);
	// Cleanup extra state
	for (auto s : shaders)
		glDeleteShader(s);
	shaders.clear();
	// Get uniform locations
	xformLoc = glGetUniformLocation(shader, "xform");

	// Do the same for the second line shader
	std::vector<GLuint> lineShaders;
	lineShaders.push_back(compileShader(GL_VERTEX_SHADER, "shaders/line_v.glsl"));
	lineShaders.push_back(compileShader(GL_FRAGMENT_SHADER, "shaders/line_f.glsl"));
	lineShader = linkProgram(lineShaders);
	for (auto s : lineShaders)
		glDeleteShader(s);
	lineShaders.clear();
	lineXformLoc = glGetUniformLocation(lineShader, "xform");
}

void GLState::initLineGeometry() {
	// Vertices
	std::vector<Vertex> verts{
		// Position                 // Normal
	  { {  1.0f,  -1.0f, 0.0f }, {  0.8164f, 0.0f, -0.5773f }, },	// v0
	  { { -1.0f,  -1.0f, 0.0f }, { -0.8164f, 0.0f, -0.5773f }, },	// v1
	};

	// Triangle indices
	std::vector<GLuint> inds{
		0, 1
	};
	lineVcount = (GLsizei)inds.size();

	// Create vertex array object
	glGenVertexArrays(1, &lineVao);
	glBindVertexArray(lineVao);

	// Create OpenGL buffers for vertex and index data
	glGenBuffers(1, &lineVbuf);
	glBindBuffer(GL_ARRAY_BUFFER, lineVbuf);
	glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(Vertex), verts.data(), GL_STATIC_DRAW);
	glGenBuffers(1, &lineIbuf);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lineIbuf);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, inds.size() * sizeof(GLuint), inds.data(), GL_STATIC_DRAW);

	// Specify vertex attributes
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)sizeof(glm::vec3));

	// Cleanup state
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
