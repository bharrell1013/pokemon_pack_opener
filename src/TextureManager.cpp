#include "TextureManager.hpp"
#include <iostream>
#include <filesystem>

// Define the necessary headers for image loading
// You'll need to include a library for image loading like stb_image.h
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // Make sure to download this header

TextureManager::TextureManager() : cardShader(0), holoShader(0) {
    // Set up the shader programs
    // For now, just placeholders
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
    // For now, just return a placeholder texture
    // Later, we'll implement dynamic texture generation based on card properties
    return loadTexture("textures/card_template.png");
}

GLuint TextureManager::generateHoloEffect(GLuint baseTexture, const std::string& rarity) {
    // This will be implemented later with advanced shader techniques
    // For now, just return the base texture
    return baseTexture;
}

void TextureManager::applyCardShader(const Card& card) {
    // For now, just use a basic shader
    // Later, we'll implement different shaders based on card rarity
}

void TextureManager::applyHoloShader(const Card& card, float time) {
    // This will be implemented later for holographic effects
}