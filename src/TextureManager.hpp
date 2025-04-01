#ifndef TEXTUREMANAGER_HPP
#define TEXTUREMANAGER_HPP

#include "gl_core_3_3.h"
#include <map>
#include <string>
#include "Card.hpp"

class TextureManager {
private:
    std::map<std::string, GLuint> textureMap;
    GLuint cardShader;             // Shader program for cards
    GLuint holoShader;             // Special shader for holo effects

public:
    TextureManager();
    ~TextureManager();

    GLuint loadTexture(const std::string& filePath);
    GLuint getTexture(const std::string& textureName);

    // Card texture generation
    GLuint generateCardTexture(const Card& card);
    GLuint generateHoloEffect(GLuint baseTexture, const std::string& rarity);

    // Shader management
    void applyCardShader(const Card& card);
    void applyHoloShader(const Card& card, float time);
};

#endif