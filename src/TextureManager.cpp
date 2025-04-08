#include "TextureManager.hpp"
#include "Card.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector> // Needed for shader info log

// Define the necessary headers for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TextureManager::TextureManager() : cardShader(0), holoShader(0), currentShader(0) {
    // Ensure OpenGL context is available before initializing shaders
    // This should be guaranteed by the call order in Application::initialize
    initializeShaders();
    if (cardShader == 0 || holoShader == 0) {
        // Optional: Add more specific error handling if initialization failed
        std::cerr << "FATAL: One or more shader programs failed to initialize correctly!" << std::endl;
        // Consider throwing an exception or setting an error state
    }
}

TextureManager::~TextureManager() {
    // Cleanup all textures
    for (auto& pair : textureMap) {
        glDeleteTextures(1, &pair.second);
    }
    textureMap.clear(); // Good practice to clear the map too

    // Cleanup shaders
    if (cardShader != 0) {
        glDeleteProgram(cardShader);
    }
    if (holoShader != 0) {
        glDeleteProgram(holoShader);
    }
}

GLuint TextureManager::loadTexture(const std::string& filePath) {
    // Check if already loaded
    if (textureMap.find(filePath) != textureMap.end()) {
        return textureMap[filePath];
    }
    // Debugging output for paths (keep as is)
    std::filesystem::path fullPath = std::filesystem::absolute(filePath);
    std::cout << "Attempting to load texture from absolute path: " << fullPath.string() << std::endl;
    std::cout << "Relative path used: " << filePath << std::endl;
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;

    // Load image
    int width, height, channels;
    // Ensure vertical flip matches OpenGL's texture coordinate system if needed
    // stbi_set_flip_vertically_on_load(true); // Uncomment if textures are upside down
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << filePath << std::endl;
        std::cerr << "STB Failure Reason: " << stbi_failure_reason() << std::endl;
        return 0; // Return 0 on failure
    }
    else {
        std::cout << "Successfully loaded texture: " << filePath << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;
    }


    // Create OpenGL texture
    GLuint textureID = 0; // Initialize to 0
    glGenTextures(1, &textureID);
    if (textureID == 0) {
        std::cerr << "OpenGL Error: Failed to generate texture ID for " << filePath << std::endl;
        stbi_image_free(data);
        return 0;
    }
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload texture data
    GLenum internalFormat = GL_RGB;
    GLenum dataFormat = GL_RGB;
    if (channels == 4) {
        internalFormat = GL_RGBA;
        dataFormat = GL_RGBA;
    }
    else if (channels == 3) {
        internalFormat = GL_RGB;
        dataFormat = GL_RGB;
    }
    else if (channels == 1) {
        internalFormat = GL_RED; // Or GL_R8 if needed
        dataFormat = GL_RED;
        std::cout << "Warning: Loading texture " << filePath << " as single channel (GL_RED)." << std::endl;
    }
    else {
        std::cerr << "Error: Unsupported number of channels (" << channels << ") for texture: " << filePath << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID); // Clean up generated texture ID
        return 0;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D); // Generate mipmaps AFTER setting data

    // Check for OpenGL errors during texture loading
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Texture Error (" << filePath << "): " << err << std::endl;
        // Decide if error is critical. If so, clean up and return 0.
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind
        return 0;
    }


    // Free image data
    stbi_image_free(data);

    // Unbind texture (good practice)
    glBindTexture(GL_TEXTURE_2D, 0);

    // Store and return
    textureMap[filePath] = textureID;
    std::cout << "Texture stored in map: " << filePath << " -> ID: " << textureID << std::endl;
    return textureID;
}

GLuint TextureManager::getTexture(const std::string& textureName) {
    auto it = textureMap.find(textureName);
    if (it != textureMap.end()) {
        return it->second; // Return existing texture ID
    }
    std::cerr << "Warning: Texture '" << textureName << "' not found in TextureManager cache." << std::endl;
    return 0; // Return 0 if not found
}


// --- generateCardTexture, generateHoloEffect remain the same ---
GLuint TextureManager::generateCardTexture(const Card& card) {
    // Create texture path based on Pokemon type
    std::string templatePath = "textures/cards/template_" + card.getPokemonType() + ".png";
    std::string pokemonPath = "textures/pokemon/" + card.getPokemonName() + ".png";

    // Try to load Pokemon-specific texture first
    GLuint textureID = loadTexture(pokemonPath);

    // If Pokemon-specific texture failed, use type template
    if (textureID == 0) {
        std::cout << "Pokemon-specific texture failed or not found for " << card.getPokemonName() << ", falling back to template: " << templatePath << std::endl;
        textureID = loadTexture(templatePath);
        if (textureID == 0) {
            std::cerr << "Error: Failed to load template texture as well: " << templatePath << std::endl;
        }
    }

    return textureID;
}

GLuint TextureManager::generateHoloEffect(GLuint baseTexture, const std::string& rarity) {
    if (holoShader == 0) {
        std::cerr << "Error: Cannot generate holo effect, holoShader is invalid." << std::endl;
        return baseTexture; // Return base if shader is bad
    }
    // Bind the holo shader
    glUseProgram(holoShader);

    // Set holo effect intensity based on rarity
    float intensity = 0.0f;
    if (rarity == "holo") intensity = 0.7f;
    else if (rarity == "reverse") intensity = 0.4f;
    else if (rarity == "ex") intensity = 0.8f;
    else if (rarity == "full art") intensity = 0.9f;

    // Set shader uniforms
    GLint intensityLoc = glGetUniformLocation(holoShader, "holoIntensity");
    if (intensityLoc == -1) {
        std::cerr << "Warning: Uniform 'holoIntensity' not found in holo shader." << std::endl;
    }
    else {
        glUniform1f(intensityLoc, intensity);
    }

    // Remember to unbind the shader later if needed, or let the next apply*Shader handle it
    return baseTexture; // For now, return base texture. Later we'll implement FBO rendering
}


void TextureManager::applyCardShader(const Card& card) {
    // *** Check if cardShader is valid before using it ***
    if (cardShader == 0) {
        std::cerr << "Error in applyCardShader: cardShader program ID is invalid (0)." << std::endl;
        return; // Don't proceed if shader is invalid
    }

    currentShader = cardShader;
    glUseProgram(cardShader); // This should now be safe if cardShader > 0

    // Check for GL errors after glUseProgram
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after glUseProgram(cardShader=" << cardShader << "): " << err << std::endl;
        // Potentially return or handle error
    }

    // Set shader uniforms
    GLint typeLoc = glGetUniformLocation(cardShader, "cardType");
    GLint rarityLoc = glGetUniformLocation(cardShader, "cardRarity");

    // Check if uniforms were found
    if (typeLoc == -1) {
        std::cerr << "Warning: Uniform 'cardType' not found in card shader (ID: " << cardShader << ")" << std::endl;
    }
    if (rarityLoc == -1) {
        std::cerr << "Warning: Uniform 'cardRarity' not found in card shader (ID: " << cardShader << ")" << std::endl;
    }

    // Convert type and rarity to numeric values for the shader
    int typeValue = getTypeValue(card.getPokemonType());
    int rarityValue = getRarityValue(card.getRarity());

    // Set uniforms only if locations are valid
    if (typeLoc != -1) {
        glUniform1i(typeLoc, typeValue);
    }
    if (rarityLoc != -1) {
        glUniform1i(rarityLoc, rarityValue);
    }

    // Check for GL errors after setting uniforms
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after setting card shader uniforms: " << err << std::endl;
    }
}

void TextureManager::applyHoloShader(const Card& card, float time) {
    // *** Check if holoShader is valid before using it ***
    if (holoShader == 0) {
        std::cerr << "Error in applyHoloShader: holoShader program ID is invalid (0)." << std::endl;
        return; // Don't proceed if shader is invalid
    }
    currentShader = holoShader;
    glUseProgram(holoShader);

    // Check for GL errors after glUseProgram
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after glUseProgram(holoShader=" << holoShader << "): " << err << std::endl;
        // Potentially return or handle error
    }

    // Set time uniform for animated effects
    GLint timeLoc = glGetUniformLocation(holoShader, "time");
    if (timeLoc == -1) {
        std::cerr << "Warning: Uniform 'time' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    }
    else {
        glUniform1f(timeLoc, time);
    }


    // Set other holo effect parameters
    GLint intensityLoc = glGetUniformLocation(holoShader, "holoIntensity");
    GLint typeLoc = glGetUniformLocation(holoShader, "cardType");

    if (intensityLoc == -1) {
        std::cerr << "Warning: Uniform 'holoIntensity' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    }
    if (typeLoc == -1) {
        std::cerr << "Warning: Uniform 'cardType' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    }


    float intensity = 0.0f;
    if (card.getRarity() == "holo") intensity = 0.7f;
    else if (card.getRarity() == "reverse") intensity = 0.4f;
    else if (card.getRarity() == "ex") intensity = 0.8f;
    else if (card.getRarity() == "full art") intensity = 0.9f;

    if (intensityLoc != -1) {
        glUniform1f(intensityLoc, intensity);
    }
    if (typeLoc != -1) {
        glUniform1i(typeLoc, getTypeValue(card.getPokemonType()));
    }

    // Check for GL errors after setting uniforms
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after setting holo shader uniforms: " << err << std::endl;
    }
}


void TextureManager::initializeShaders() {
    std::cout << "Initializing shaders..." << std::endl;
    // Load and compile shaders
    std::string vertexShaderPath = "shaders/card_v.glsl";
    std::string fragmentShaderPath = "shaders/card_f.glsl";
    std::string holoVertexPath = "shaders/holo_v.glsl";
    std::string holoFragmentPath = "shaders/holo_f.glsl";

    // Create shader programs
    cardShader = createShaderProgram(vertexShaderPath, fragmentShaderPath);
    holoShader = createShaderProgram(holoVertexPath, holoFragmentPath);

    if (cardShader != 0) {
        std::cout << "Card shader program created successfully. ID: " << cardShader << std::endl;
    }
    else {
        std::cerr << "Failed to create card shader program!" << std::endl;
    }

    if (holoShader != 0) {
        std::cout << "Holo shader program created successfully. ID: " << holoShader << std::endl;
    }
    else {
        std::cerr << "Failed to create holo shader program!" << std::endl;
    }
    std::cout << "Shader initialization finished." << std::endl;
}


// *** MODIFIED createShaderProgram with Error Checking ***
GLuint TextureManager::createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
    // 1. Load shader sources
    std::string vertexSource = loadShaderSource(vertexPath);
    std::string fragmentSource = loadShaderSource(fragmentPath);
    if (vertexSource.empty()) {
        std::cerr << "Error: Vertex shader source file is empty or not found: " << vertexPath << std::endl;
        return 0;
    }
    if (fragmentSource.empty()) {
        std::cerr << "Error: Fragment shader source file is empty or not found: " << fragmentPath << std::endl;
        return 0;
    }

    const char* vertexSourcePtr = vertexSource.c_str();
    const char* fragmentSourcePtr = fragmentSource.c_str();

    GLint success;
    GLchar infoLog[1024]; // Increased buffer size

    // 2. Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (vertexShader == 0) {
        std::cerr << "Error creating vertex shader object for " << vertexPath << std::endl;
        return 0;
    }
    glShaderSource(vertexShader, 1, &vertexSourcePtr, nullptr);
    glCompileShader(vertexShader);

    // Check vertex shader compilation status
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << vertexPath << "\n" << infoLog << std::endl;
        glDeleteShader(vertexShader); // Clean up the failed shader object
        return 0; // Return 0 indicates failure
    }
    else {
        std::cout << "Vertex shader compiled successfully: " << vertexPath << std::endl;
    }


    // 3. Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        std::cerr << "Error creating fragment shader object for " << fragmentPath << std::endl;
        glDeleteShader(vertexShader); // Clean up vertex shader too
        return 0;
    }
    glShaderSource(fragmentShader, 1, &fragmentSourcePtr, nullptr);
    glCompileShader(fragmentShader);

    // Check fragment shader compilation status
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << fragmentPath << "\n" << infoLog << std::endl;
        glDeleteShader(vertexShader); // Clean up vertex shader
        glDeleteShader(fragmentShader); // Clean up fragment shader
        return 0; // Return 0 indicates failure
    }
    else {
        std::cout << "Fragment shader compiled successfully: " << fragmentPath << std::endl;
    }


    // 4. Create and link shader program
    GLuint program = glCreateProgram();
    if (program == 0) {
        std::cerr << "Error creating shader program object." << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check program linking status
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << "Vertex: " << vertexPath << " | Fragment: " << fragmentPath << "\n" << infoLog << std::endl;
        glDeleteProgram(program); // Clean up the failed program object
        glDeleteShader(vertexShader); // Detaching is implicitly handled by deleting shaders
        glDeleteShader(fragmentShader);
        return 0; // Return 0 indicates failure
    }
    else {
        std::cout << "Shader program linked successfully (VS: " << vertexPath << ", FS: " << fragmentPath << "). ID: " << program << std::endl;
    }


    // 5. Detach and delete shaders after successful linking (they are no longer needed)
    // Note: Some argue deleting is sufficient as linking copies the needed info,
    // but detaching first is technically cleaner according to some specs.
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program; // Return the valid program ID
}

std::string TextureManager::loadShaderSource(const std::string& path) {
    std::cout << "Loading shader source from: " << path << std::endl;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open shader file: " << path << std::endl;
        std::cerr << "Current working directory: " << std::filesystem::current_path() << std::endl;
        std::filesystem::path absPath = std::filesystem::absolute(path);
        std::cerr << "Attempted absolute path: " << absPath << std::endl;
        return ""; // Return empty string on failure
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close(); // Close the file
    std::string source = buffer.str();
    if (source.empty()) {
        std::cout << "Warning: Shader file is empty: " << path << std::endl;
    }
    return source;
}

// --- getTypeValue and getRarityValue remain the same ---
int TextureManager::getTypeValue(const std::string& type) {
    // Convert type string to numeric value for shader
    if (type == "normal") return 0;
    if (type == "fire") return 1;
    if (type == "water") return 2;
    if (type == "grass") return 3;
    if (type == "electric") return 4;
    if (type == "psychic") return 5;
    if (type == "fighting") return 6;
    if (type == "dark") return 7;
    if (type == "dragon") return 8;
    if (type == "fairy") return 9;
    if (type == "steel") return 10;
    if (type == "ghost") return 11;
    std::cerr << "Warning: Unknown Pokemon type '" << type << "' encountered in getTypeValue. Defaulting to 0 (normal)." << std::endl;
    return 0; // Default to normal type
}

int TextureManager::getRarityValue(const std::string& rarity) {
    // Convert rarity string to numeric value for shader
    if (rarity == "normal") return 0;
    if (rarity == "reverse") return 1;
    if (rarity == "holo") return 2;
    if (rarity == "ex") return 3;
    if (rarity == "full art") return 4;
    std::cerr << "Warning: Unknown rarity '" << rarity << "' encountered in getRarityValue. Defaulting to 0 (normal)." << std::endl;
    return 0; // Default to normal rarity
}