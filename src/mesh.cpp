#define NOMINMAX
#include "mesh.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp> // For quaternion rotations
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

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

glm::mat4 GetNodeLocalTransform(const tinygltf::Node& node) {
    glm::mat4 transform = glm::mat4(1.0f);

    // Use node.matrix if provided (overrides TRS)
    if (node.matrix.size() == 16) {
        // Note: tinygltf matrix is row-major, glm::make_mat4 expects column-major pointer
        // We need to transpose it OR construct manually. Let's construct manually for clarity.
        transform = glm::mat4(
            node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3], // Col 1
            node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7], // Col 2
            node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11], // Col 3
            node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15] // Col 4
        );
        // If glm::make_mat4 is used, ensure the input data is treated correctly (column-major).
        // transform = glm::make_mat4(node.matrix.data()); // Needs check if row/column major matches
        return transform; // Return immediately if matrix is specified
    }

    // Otherwise, combine TRS (Translation * Rotation * Scale)
    glm::mat4 translation = glm::mat4(1.0f);
    if (node.translation.size() == 3) {
        translation = glm::translate(glm::mat4(1.0f), glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
    }

    glm::mat4 rotation = glm::mat4(1.0f);
    if (node.rotation.size() == 4) {
        // glTF quaternion is (x, y, z, w), glm::quat constructor is (w, x, y, z)
        glm::quat q(static_cast<float>(node.rotation[3]), // W
            static_cast<float>(node.rotation[0]), // X
            static_cast<float>(node.rotation[1]), // Y
            static_cast<float>(node.rotation[2])); // Z
        rotation = glm::toMat4(q);
    }

    glm::mat4 scale = glm::mat4(1.0f);
    if (node.scale.size() == 3) {
        scale = glm::scale(glm::mat4(1.0f), glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
    }

    // Combine TRS
    transform = translation * rotation * scale;
    return transform;
}

void ProcessNode(
    // Data accumulation parameters (passed by reference)
    std::vector<float>& combinedVertexData,
    std::vector<unsigned int>& combinedIndices,
    size_t& baseVertexIndex, // Tracks vertex offset for indices
    glm::vec3& minBB,         // Overall min bounding box
    glm::vec3& maxBB,         // Overall max bounding box

    // glTF model data (passed by const reference)
    const tinygltf::Model& model,
    int nodeIndex,            // Index of the current node to process
    const glm::mat4& parentTransform // Accumulated transform from parent nodes
) {
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= model.nodes.size()) {
        std::cerr << "Warning: Invalid node index encountered: " << nodeIndex << std::endl;
        return;
    }

    const tinygltf::Node& node = model.nodes[nodeIndex];
    glm::mat4 localTransform = GetNodeLocalTransform(node);
    glm::mat4 globalTransform = parentTransform * localTransform; // Combine with parent's transform

    // --- Process Mesh associated with this node ---
    if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < model.meshes.size()) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        std::cout << "      Processing Node " << nodeIndex << " ('" << node.name << "') -> Mesh " << node.mesh << " ('" << mesh.name << "'), Primitives: " << mesh.primitives.size() << std::endl;

        // Calculate normal matrix for transforming normals correctly
        // (Inverse transpose of the upper-left 3x3 part of the global transform)
        glm::mat3 normalMatrix = glm::mat3(1.0f);
        if (abs(glm::determinant(glm::mat3(globalTransform))) > 1e-6) { // Check if invertible
            normalMatrix = glm::transpose(glm::inverse(glm::mat3(globalTransform)));
        }
        else {
            std::cerr << "        Warning: Node " << nodeIndex << " global transform is non-invertible. Normals might be incorrect." << std::endl;
        }

        // Process primitives within this mesh
        for (size_t primIdx = 0; primIdx < mesh.primitives.size(); ++primIdx) {
            const tinygltf::Primitive& primitive = mesh.primitives[primIdx];
            std::cout << "        Processing Primitive " << primIdx << std::endl;

            if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                std::cerr << "          Warning: Primitive mode is not TRIANGLES (" << primitive.mode << "). Skipping." << std::endl;
                continue;
            }

            // --- Extract Indices ---
            if (primitive.indices < 0) {
                std::cerr << "          Warning: Primitive is not indexed. Skipping." << std::endl;
                continue;
            }
            // (Same index reading logic as before, but add baseVertexIndex)
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
            const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
            const unsigned char* indexBufferData = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
            size_t indexStride = indexAccessor.ByteStride(indexBufferView);
            if (indexStride == 0) indexStride = tinygltf::GetComponentSizeInBytes(indexAccessor.componentType); // Fallback for tightly packed
            size_t primitiveIndexCount = indexAccessor.count;
            size_t currentCombinedIndexCount = combinedIndices.size();
            combinedIndices.resize(currentCombinedIndexCount + primitiveIndexCount);

            bool indexReadSuccess = true;
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                for (size_t i = 0; i < primitiveIndexCount; ++i) {
                    unsigned short indexVal = *(reinterpret_cast<const unsigned short*>(indexBufferData + i * indexStride));
                    combinedIndices[currentCombinedIndexCount + i] = static_cast<unsigned int>(indexVal + baseVertexIndex);
                }
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                for (size_t i = 0; i < primitiveIndexCount; ++i) {
                    unsigned int indexVal = *(reinterpret_cast<const unsigned int*>(indexBufferData + i * indexStride));
                    combinedIndices[currentCombinedIndexCount + i] = indexVal + static_cast<unsigned int>(baseVertexIndex);
                }
            }
            else {
                std::cerr << "          Error: Unsupported index component type: " << indexAccessor.componentType << ". Skipping primitive." << std::endl;
                combinedIndices.resize(currentCombinedIndexCount); // Revert resize
                indexReadSuccess = false;
            }
            if (!indexReadSuccess) continue; // Skip rest of primitive if indices failed


            // --- Extract Vertex Attribute Pointers and Strides ---
            const float* positions = nullptr; size_t posStride = 0; size_t primitiveVertexCount = 0;
            const float* normals = nullptr;   size_t normStride = 0;
            const float* texcoords = nullptr; size_t tcStride = 0;

            // POSITION (Required)
            if (primitive.attributes.count("POSITION")) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
                if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3) {
                    std::cerr << "          Error: POSITION attribute has unsupported type/componentType. Skipping primitive." << std::endl; continue;
                }
                const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                positions = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[view.byteOffset + accessor.byteOffset]));
                primitiveVertexCount = accessor.count;
                posStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float)) : 3; // Stride in floats
            }
            else { std::cerr << "          Error: POSITION attribute not found. Skipping primitive." << std::endl; continue; }

            // NORMAL (Optional)
            if (primitive.attributes.count("NORMAL")) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC3) {
                    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    normals = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[view.byteOffset + accessor.byteOffset]));
                    normStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float)) : 3;
                }
                else { std::cerr << "          Warning: NORMAL attribute has unsupported type. Ignoring." << std::endl; }
            }

            // TEXCOORD_0 (Optional)
            if (primitive.attributes.count("TEXCOORD_0")) {
                const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && accessor.type == TINYGLTF_TYPE_VEC2) {
                    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
                    texcoords = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[view.byteOffset + accessor.byteOffset]));
                    tcStride = accessor.ByteStride(view) ? (accessor.ByteStride(view) / sizeof(float)) : 2;
                }
                else { std::cerr << "          Warning: TEXCOORD_0 attribute has unsupported type. Ignoring." << std::endl; }
            }

            // --- Interleave and Append Vertex Data, Applying Transforms ---
            size_t currentCombinedVertexDataSize = combinedVertexData.size();
            combinedVertexData.resize(currentCombinedVertexDataSize + primitiveVertexCount * 14); // 14 floats per vertex

            for (size_t i = 0; i < primitiveVertexCount; ++i) {
                size_t targetIndex = currentCombinedVertexDataSize + i * 14;

                // --- Position: Read raw, transform, store ---
                glm::vec3 rawPos(positions[i * posStride], positions[i * posStride + 1], positions[i * posStride + 2]);
                glm::vec4 transformedPos4 = globalTransform * glm::vec4(rawPos, 1.0f);
                glm::vec3 transformedPos = glm::vec3(transformedPos4) / transformedPos4.w; // perspective divide just in case

                combinedVertexData[targetIndex + 0] = transformedPos.x;
                combinedVertexData[targetIndex + 1] = transformedPos.y;
                combinedVertexData[targetIndex + 2] = transformedPos.z;
                minBB = glm::min(minBB, transformedPos); // Update overall bounding box
                maxBB = glm::max(maxBB, transformedPos);

                // --- TexCoord: Read raw, store ---
                if (texcoords) {
                    combinedVertexData[targetIndex + 3] = texcoords[i * tcStride];
                    combinedVertexData[targetIndex + 4] = texcoords[i * tcStride + 1];
                }
                else {
                    combinedVertexData[targetIndex + 3] = 0.0f; combinedVertexData[targetIndex + 4] = 0.0f;
                }

                // --- Normal: Read raw, transform, normalize, store ---
                glm::vec3 transformedNorm = glm::vec3(0.0f, 0.0f, 1.0f); // Default up
                if (normals) {
                    glm::vec3 rawNorm(normals[i * normStride], normals[i * normStride + 1], normals[i * normStride + 2]);
                    transformedNorm = glm::normalize(normalMatrix * rawNorm); // Apply normal matrix
                }
                combinedVertexData[targetIndex + 5] = transformedNorm.x;
                combinedVertexData[targetIndex + 6] = transformedNorm.y;
                combinedVertexData[targetIndex + 7] = transformedNorm.z;

                // --- Tangent/Bitangent: Pad ---
                combinedVertexData[targetIndex + 8] = 1.0f; combinedVertexData[targetIndex + 9] = 0.0f; combinedVertexData[targetIndex + 10] = 0.0f;
                combinedVertexData[targetIndex + 11] = 0.0f; combinedVertexData[targetIndex + 12] = 1.0f; combinedVertexData[targetIndex + 13] = 0.0f;
            }
            std::cout << "          Added " << primitiveVertexCount << " transformed vertices." << std::endl;

            // Update base vertex index for the next primitive within this mesh
            baseVertexIndex += primitiveVertexCount;
        } // End primitive loop
    } // End if node has mesh

    // --- Recursively process children nodes ---
    for (int childNodeIndex : node.children) {
        ProcessNode(combinedVertexData, combinedIndices, baseVertexIndex, minBB, maxBB, model, childNodeIndex, globalTransform); // Pass calculated globalTransform
    }
}

bool Mesh::loadGltf(const std::string& filename, bool keepLocalGeometry) {
    release(); // Clear existing mesh data

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename);

    if (!warn.empty()) {
        std::cout << "TinyGLTF Warning: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "TinyGLTF Error: " << err << std::endl;
        // Optionally try LoadASCIIFromFile if binary fails
        // ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
        // if (!err.empty()) { std::cerr << "TinyGLTF ASCII Error: " << err << std::endl; }
    }
    if (!ret) {
        std::cerr << "Failed to load GLTF file: " << filename << std::endl;
        return false; // Indicate failure
    }

    std::cout << "Processing GLTF model: " << filename << std::endl;
    std::cout << "  Meshes: " << model.meshes.size() << std::endl;
    std::cout << "  Materials: " << model.materials.size() << std::endl;
    std::cout << "  Accessors: " << model.accessors.size() << std::endl;
    std::cout << "  BufferViews: " << model.bufferViews.size() << std::endl;
    std::cout << "  Buffers: " << model.buffers.size() << std::endl;

    // --- Combined data storage ---
    std::vector<float> combinedVertexData;
    std::vector<unsigned int> combinedIndices;
    size_t baseVertexIndex = 0; // Keep track of the starting vertex index for the current primitive

    minBB = glm::vec3(std::numeric_limits<float>::max()); // Initialize bounding box for the whole mesh
    maxBB = glm::vec3(std::numeric_limits<float>::lowest());

    // --- Process scene nodes ---
    if (model.scenes.empty()) {
        std::cerr << "GLTF Error: No scenes found in the model." << std::endl;
        return false;
    }
    int scene_to_display = model.defaultScene > -1 ? model.defaultScene : 0;
    if (static_cast<size_t>(scene_to_display) >= model.scenes.size()) {
        std::cerr << "GLTF Error: Default scene index is invalid." << std::endl;
        return false;
    }
    const tinygltf::Scene& scene = model.scenes[scene_to_display];

    std::cout << "Processing Scene " << scene_to_display << " ('" << scene.name << "') with " << scene.nodes.size() << " root nodes." << std::endl;
    glm::mat4 rootTransform = glm::mat4(1.0f); // Start with identity matrix

    for (int rootNodeIndex : scene.nodes) { // scene.nodes contains indices of the root nodes
        ProcessNode(combinedVertexData, combinedIndices, baseVertexIndex, minBB, maxBB, model, rootNodeIndex, rootTransform);
    }

    // --- Final Check ---
    if (combinedVertexData.empty()) {
        std::cerr << "GLTF Error: No vertex data could be extracted from the scene graph." << std::endl;
        return false;
    }
    if (combinedIndices.empty()) {
        std::cerr << "GLTF Warning: No index data extracted (maybe non-triangle or non-indexed model?)." << std::endl;
        // If you need non-indexed rendering, Mesh::initialize needs adapting or Mesh::draw needs to use glDrawArrays
        // For now, we proceed assuming Mesh::initialize requires indices.
        if (combinedVertexData.empty()) return false; // Still need vertices!
    }

    // --- Call Existing initialize Function with combined data ---
    try {
        std::cout << "Initializing Mesh with combined GLTF data from scene graph..." << std::endl;
        initialize(combinedVertexData, combinedIndices);

        // Bounding box already updated in ProcessNode
        std::cout << "Successfully loaded and initialized mesh from GLTF scene: " << filename << std::endl;
        std::cout << "  Total Vertex Count: " << (vbuf ? (combinedVertexData.size() / 14) : 0) << ", Total Index Count: " << vcount << std::endl; // Use member vcount after initialize
        std::cout << "  Final Bounding Box Min: (" << minBB.x << "," << minBB.y << "," << minBB.z << ")" << std::endl;
        std::cout << "  Final Bounding Box Max: (" << maxBB.x << "," << maxBB.y << "," << maxBB.z << ")" << std::endl;

    }
    catch (const std::exception& e) {
        std::cerr << "Error during Mesh::initialize after combining GLTF data: " << e.what() << std::endl;
        release();
        return false;
    }

    // --- Handle keepLocalGeometry (If needed) ---
    if (keepLocalGeometry) {
        // Populate the 'vertices' member from combinedVertexData
        vertices.resize(combinedVertexData.size() / 14);
        for (size_t i = 0; i < vertices.size(); ++i) {
            size_t base = i * 14;
            vertices[i].pos = glm::vec3(combinedVertexData[base + 0], combinedVertexData[base + 1], combinedVertexData[base + 2]);
            vertices[i].texCoord = glm::vec2(combinedVertexData[base + 3], combinedVertexData[base + 4]);
            vertices[i].norm = glm::vec3(combinedVertexData[base + 5], combinedVertexData[base + 6], combinedVertexData[base + 7]);
            // Ignore tangent/bitangent stored in combinedVertexData for local copy
        }
        std::cout << "  Kept local geometry copy." << std::endl;
    }
    else {
        // Optionally clear combinedVertexData/combinedIndices now to save memory if they aren't needed
    }

    return true; // Success
}
