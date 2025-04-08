#include "TextureManager.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

// Define the necessary headers for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Initialize static member
TextureManager* TextureManager::instance = nullptr;

TextureManager::TextureManager() : cardShader(0), holoShader(0) {
    initializeShaders();
}

TextureManager::~TextureManager() {
    // Cleanup all textures
    for (auto& pair : textureMap) {
        glDeleteTextures(1, &pair.second);
    }

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
    std::filesystem::path fullPath = std::filesystem::absolute(filePath);
    std::cout << "Attempting to load texture from absolute path: " << fullPath.string() << std::endl;
    std::cout << "Relative path used: " << filePath << std::endl;
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;
    // Load image
    int width, height, channels;
    unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Failed to load texture: " << filePath << std::endl;
        std::cerr << "STB Failure Reason: " << stbi_failure_reason() << std::endl;
        return 0;
    }

    // Create OpenGL texture
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload texture data
    GLenum format = channels == 4 ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Free image data
    stbi_image_free(data);

    // Store and return
    textureMap[filePath] = textureID;
    return textureID;
}

GLuint TextureManager::getTexture(const std::string& textureName) {
    if (textureMap.find(textureName) != textureMap.end()) {
        return textureMap[textureName];
    }
    return 0;
}

GLuint TextureManager::generateCardTexture(const Card& card) {
    // Create texture path based on Pokemon type
    std::string templatePath = "textures/cards/template_" + card.getPokemonType() + ".png";
    std::string pokemonPath = "textures/pokemon/" + card.getPokemonName() + ".png";

    // Try to load Pokemon-specific texture first
    GLuint textureID = loadTexture(pokemonPath);
    
    // If Pokemon-specific texture failed, use type template
    if (textureID == 0) {
        textureID = loadTexture(templatePath);
    }

    return textureID;
}

GLuint TextureManager::generateHoloEffect(GLuint baseTexture, const std::string& rarity) {
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
    glUniform1f(intensityLoc, intensity);

    return baseTexture; // For now, return base texture. Later we'll implement FBO rendering
}

void TextureManager::applyCardShader(const Card& card) {
    glUseProgram(cardShader);

    // Set shader uniforms
    GLint typeLoc = glGetUniformLocation(cardShader, "cardType");
    GLint rarityLoc = glGetUniformLocation(cardShader, "cardRarity");
    
    // Convert type and rarity to numeric values for the shader
    int typeValue = getTypeValue(card.getPokemonType());
    int rarityValue = getRarityValue(card.getRarity());
    
    glUniform1i(typeLoc, typeValue);
    glUniform1i(rarityLoc, rarityValue);
}

void TextureManager::applyHoloShader(const Card& card, float time) {
    glUseProgram(holoShader);

    // Set time uniform for animated effects
    GLint timeLoc = glGetUniformLocation(holoShader, "time");
    glUniform1f(timeLoc, time);

    // Set other holo effect parameters
    GLint intensityLoc = glGetUniformLocation(holoShader, "holoIntensity");
    GLint typeLoc = glGetUniformLocation(holoShader, "cardType");
    
    float intensity = 0.0f;
    if (card.getRarity() == "holo") intensity = 0.7f;
    else if (card.getRarity() == "reverse") intensity = 0.4f;
    else if (card.getRarity() == "ex") intensity = 0.8f;
    else if (card.getRarity() == "full art") intensity = 0.9f;
    
    glUniform1f(intensityLoc, intensity);
    glUniform1i(typeLoc, getTypeValue(card.getPokemonType()));
}

void TextureManager::initializeShaders() {
    // Load and compile shaders
    std::string vertexShaderPath = "shaders/card_v.glsl";
    std::string fragmentShaderPath = "shaders/card_f.glsl";
    std::string holoVertexPath = "shaders/holo_v.glsl";
    std::string holoFragmentPath = "shaders/holo_f.glsl";

    // Create shader programs
    cardShader = createShaderProgram(vertexShaderPath, fragmentShaderPath);
    holoShader = createShaderProgram(holoVertexPath, holoFragmentPath);
}

GLuint TextureManager::createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
    // Load shader sources
    std::string vertexSource = loadShaderSource(vertexPath);
    std::string fragmentSource = loadShaderSource(fragmentPath);

    // Create and compile shaders
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    const char* vertexSourcePtr = vertexSource.c_str();
    const char* fragmentSourcePtr = fragmentSource.c_str();

    glShaderSource(vertexShader, 1, &vertexSourcePtr, nullptr);
    glShaderSource(fragmentShader, 1, &fragmentSourcePtr, nullptr);

    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);

    // Create and link program
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Cleanup
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

std::string TextureManager::loadShaderSource(const std::string& path) {
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

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
    return 0; // Default to normal type
}

int TextureManager::getRarityValue(const std::string& rarity) {
    // Convert rarity string to numeric value for shader
    if (rarity == "normal") return 0;
    if (rarity == "reverse") return 1;
    if (rarity == "holo") return 2;
    if (rarity == "ex") return 3;
    if (rarity == "full art") return 4;
    return 0; // Default to normal rarity
}