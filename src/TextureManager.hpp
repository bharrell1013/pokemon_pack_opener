#ifndef TEXTUREMANAGER_HPP
#define TEXTUREMANAGER_HPP

#include "gl_core_3_3.h"
#include <map>
#include <string>

// Forward declaration
class Card;

class TextureManager {
private:
    std::map<std::string, GLuint> textureMap;
    GLuint cardShader;             // Shader program for cards
    GLuint holoShader;             // Special shader for holo effects
    GLuint currentShader;          // Currently active shader

    // Helper methods for shader compilation
    GLuint createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
    std::string loadShaderSource(const std::string& path);
    int getTypeValue(const std::string& type);
    int getRarityValue(const std::string& rarity);

public:
    TextureManager();
    ~TextureManager();

    // Texture loading
    GLuint loadTexture(const std::string& filePath);
    GLuint getTexture(const std::string& textureName);

    // Card texture generation
    GLuint generateCardTexture(const Card& card);
    GLuint generateHoloEffect(GLuint baseTexture, const std::string& rarity);

    // Shader management
    void applyCardShader(const Card& card);
    void applyHoloShader(const Card& card, float time);
    GLuint getCurrentShader() const { return currentShader; }

    // Initialize shaders
    void initializeShaders();
};

#endif