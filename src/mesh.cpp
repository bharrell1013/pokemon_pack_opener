#define NOMINMAX
#include "mesh.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

// Helper functions
int indexOfNumberLetter(std::string& str, int offset);
int lastIndexOfNumberLetter(std::string& str);
std::vector<std::string> split(const std::string &s, char delim);

// Constructor - load mesh from file
Mesh::Mesh(std::string filename, bool keepLocalGeometry) {
	minBB = glm::vec3(std::numeric_limits<float>::max());
	maxBB = glm::vec3(std::numeric_limits<float>::lowest());

	vao = 0;
	vbuf = 0;
	vcount = 0;
	load(filename, keepLocalGeometry);
}

// Draw the mesh
void Mesh::draw() const { // Keep const
    if (vao == 0 || vbuf == 0 || vcount == 0) {
        // Don't try to draw if buffers aren't set up or vertex count is zero
        // std::cerr << "Warning: Mesh::draw() called on uninitialized or empty mesh." << std::endl; // Optional warning
        return;
    }

    glBindVertexArray(vao); // Bind the VAO containing buffer and attribute pointers

    // Use glDrawArrays because the 'load' function prepares non-indexed data
    if (ebo != 0) { // If the member variable ebo is valid, use indexed drawing
        glDrawElements(GL_TRIANGLES, vcount, GL_UNSIGNED_INT, 0);
    }
    else { // Otherwise, use non-indexed drawing (like for OBJ files loaded via Mesh::load)
        glDrawArrays(GL_TRIANGLES, 0, vcount);
    }

    glBindVertexArray(0); // Unbind the VAO
}

// Load a wavefront OBJ file
void Mesh::load(std::string filename, bool keepLocalGeometry) {
	// Release resources
	release();

	std::ifstream file(filename);
	if (!file.is_open()) {
		std::stringstream ss;
		ss << "Error reading " << filename << ": failed to open file";
		throw std::runtime_error(ss.str());
	}

	// Store vertex and normal data while reading
    std::vector<glm::vec3> raw_vertices;
    std::vector<glm::vec3> raw_normals;
    std::vector<glm::vec2> raw_texcoords; // Keep this
    std::vector<unsigned int> v_elements;
    std::vector<unsigned int> n_elements; // Keep this
    std::vector<unsigned int> t_elements; // Keep this

    std::string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') { // Skip empty lines and comments
            continue;
        }

        std::stringstream ss(line);
        std::string keyword;
        ss >> keyword; // Read the first word

        if (keyword == "v") { // Vertex Position
            float x, y, z;
            if (!(ss >> x >> y >> z)) { throw std::runtime_error("Error parsing vertex line: " + line); }
            raw_vertices.emplace_back(x, y, z);
            // Update bounding box
            minBB = glm::min(minBB, raw_vertices.back());
            maxBB = glm::max(maxBB, raw_vertices.back());
        }
        else if (keyword == "vt") { // Texture Coordinate
            float u, v;
            if (!(ss >> u >> v)) { throw std::runtime_error("Error parsing vt line: " + line); }
            raw_texcoords.emplace_back(u, v);
        }
        else if (keyword == "vn") { // Vertex Normal
            float nx, ny, nz;
            if (!(ss >> nx >> ny >> nz)) { throw std::runtime_error("Error parsing vn line: " + line); }
            raw_normals.emplace_back(nx, ny, nz);
        }
        else if (keyword == "f") { // Face
            std::string face_data_str;
            std::vector<std::tuple<int, int, int>> face_vertices;
            int v_idx, vt_idx, vn_idx;
            char slash;

            // Read the rest of the line
            std::string segment;
            while (ss >> segment) {
                std::stringstream segment_ss(segment);

                // Try parsing v/vt/vn format
                segment_ss >> v_idx >> slash >> vt_idx >> slash >> vn_idx;
                if (!segment_ss.fail() && segment_ss.eof()) { // Check if fully parsed
                    face_vertices.emplace_back(v_idx, vt_idx, vn_idx);
                }
                else {
                    // Add error handling or support for other formats like v//vn if needed
                    // For now, assume v/vt/vn is required
                    throw std::runtime_error("Unsupported face format or parse error in line: " + line + " segment: " + segment);
                }
            }

            // Triangulate the face (OBJ supports polygons, but we render triangles)
            if (face_vertices.size() >= 3) {
                for (size_t i = 1; i < face_vertices.size() - 1; ++i) {
                    // Add triangle (vertex 0, vertex i, vertex i+1)
                    v_elements.push_back(std::get<0>(face_vertices[0]) - 1);
                    t_elements.push_back(std::get<1>(face_vertices[0]) - 1);
                    n_elements.push_back(std::get<2>(face_vertices[0]) - 1);

                    v_elements.push_back(std::get<0>(face_vertices[i]) - 1);
                    t_elements.push_back(std::get<1>(face_vertices[i]) - 1);
                    n_elements.push_back(std::get<2>(face_vertices[i]) - 1);

                    v_elements.push_back(std::get<0>(face_vertices[i + 1]) - 1);
                    t_elements.push_back(std::get<1>(face_vertices[i + 1]) - 1);
                    n_elements.push_back(std::get<2>(face_vertices[i + 1]) - 1);
                }
            }
            else {
                throw std::runtime_error("Face with less than 3 vertices in line: " + line);
            }
        }
        // Add handling for other keywords like 's', 'usemtl' if needed, otherwise ignore
    }
    file.close();

    // Basic validation
    if (raw_vertices.empty() || v_elements.empty()) {
        throw std::runtime_error("Error reading " + filename + ": No vertices or faces found.");
    }
    if (raw_texcoords.empty() || t_elements.empty()) {
        // Warn or throw if texture coordinates are expected but missing
        std::cerr << "Warning: Missing texture coordinates in " << filename << std::endl;
        // If they are strictly required, throw an error instead.
        // throw std::runtime_error("Missing texture coordinates in " + filename);
    }
    if (raw_normals.empty() || n_elements.empty()) {
        // Warn or throw if normals are expected but missing
        std::cerr << "Warning: Missing normals in " << filename << std::endl;
        // throw std::runtime_error("Missing normals in " + filename);
    }

    // Check index consistency (important!)
    if (v_elements.size() != t_elements.size() || v_elements.size() != n_elements.size()) {
        // This can happen if the OBJ mixes formats like v/vt/vn and v//vn
        throw std::runtime_error("Inconsistent face data (v/vt/vn counts differ) in " + filename);
    }


    // --- Assemble the final vertex data ---
    vertices.resize(v_elements.size()); // Resize to the exact number of vertex indices we have
    for (size_t i = 0; i < v_elements.size(); ++i) {
        unsigned int v_idx = v_elements[i];
        unsigned int vt_idx = t_elements[i];
        unsigned int vn_idx = n_elements[i];

        // Bounds checking before access!
        if (v_idx >= raw_vertices.size()) throw std::runtime_error("Vertex index out of bounds in face data.");
        if (vt_idx >= raw_texcoords.size()) throw std::runtime_error("Texture coordinate index out of bounds in face data.");
        if (vn_idx >= raw_normals.size()) throw std::runtime_error("Normal index out of bounds in face data.");

        vertices[i].pos = raw_vertices[v_idx];
        vertices[i].texCoord = raw_texcoords[vt_idx];
        vertices[i].norm = raw_normals[vn_idx];
    }
    vcount = (GLsizei)vertices.size();


    // --- OpenGL Buffer Setup (remains mostly the same) ---
    // Load vertices into OpenGL
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbuf);
    glBindBuffer(GL_ARRAY_BUFFER, vbuf);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, pos));

    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, norm));

    // Texture coordinate attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, texCoord));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Delete local copy of geometry
    if (!keepLocalGeometry)
        vertices.clear();
}
// Initialize mesh with vertex data and indices
void Mesh::initialize(const std::vector<float>& vertexData, const std::vector<unsigned int>& indices) {
    // Release any existing resources
    release();

    // Calculate number of vertices (3 floats for position + 2 for UV + 3 for normal = 8 floats per vertex)
    vcount = static_cast<GLsizei>(vertexData.size() / 8);

    // Create and bind VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Create and bind VBO for vertex data
    glGenBuffers(1, &vbuf);
    glBindBuffer(GL_ARRAY_BUFFER, vbuf);
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);

    // Calculate stride (total floats per vertex * size of float)
    const GLsizei stride = 14 * sizeof(float);

    // Set up vertex attributes (LOCATION MATTERS!)
// Position (Location 0) - 3 floats
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // Texture coordinates (Location 1) - 2 floats
    glEnableVertexAttribArray(1); // <<< LOCATION is now 1
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float))); // Offset = 3 floats

    // Normals (Location 2) - 3 floats
    glEnableVertexAttribArray(2); // <<< LOCATION is now 2
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(5 * sizeof(float))); // Offset = 3+2 = 5 floats

    // Tangent (Location 3) - 3 floats <<< NEW
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float))); // Offset = 3+2+3 = 8 floats

    // Bitangent (Location 4) - 3 floats <<< NEW
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(11 * sizeof(float))); // Offset = 3+2+3+3 = 11 floats

    // If indices are provided, create and bind element buffer
    if (!indices.empty()) {
        //GLuint ebo;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
        vcount = static_cast<GLsizei>(indices.size());
    }
    else {
        ebo = 0;
    }

    // Unbind VAO and buffers
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (ebo != 0) { // Check if ebo was actually created
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

// Release resources
// Release resources
void Mesh::release() {
	minBB = glm::vec3(std::numeric_limits<float>::max());
	maxBB = glm::vec3(std::numeric_limits<float>::lowest());

	vertices.clear();
	if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
	if (vbuf) { glDeleteBuffers(1, &vbuf); vbuf = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
	vcount = 0;
}

int indexOfNumberLetter(std::string& str, int offset) {
	for (int i = offset; i < int(str.length()); ++i) {
		if ((str[i] >= '0' && str[i] <= '9') || str[i] == '-' || str[i] == '.') return i;
	}
	return (int)str.length();
}
int lastIndexOfNumberLetter(std::string& str) {
	for (int i = int(str.length()) - 1; i >= 0; --i) {
		if ((str[i] >= '0' && str[i] <= '9') || str[i] == '-' || str[i] == '.') return i;
	}
	return 0;
}
std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;

	std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }

    return elems;
}
