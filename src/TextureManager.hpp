#ifndef TEXTUREMANAGER_HPP
#define TEXTUREMANAGER_HPP

#include "gl_core_3_3.h"
#include <map>
#include <string>
#include <vector>

// Forward declaration
class Card;

class TextureManager {
private:
    std::map<std::string, GLuint> textureMap;
    GLuint cardShader;             // Shader program for cards
    GLuint holoShader;             // Special shader for holo effects
    GLuint currentShader;          // Currently active shader

    // --- API ---
    const std::string apiKey = "56f39a72-5758-495c-ac18-134248507b5a"; // Your API Key
    const std::string apiBaseUrl = "https://api.pokemontcg.io/v2/cards";

    // Helper methods for shader compilation
    GLuint createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
    std::string loadShaderSource(const std::string& path);
    int getTypeValue(const std::string& type);
    int getRarityValue(const std::string& rarity);

    // Downloads image data from a URL into a buffer
    std::vector<unsigned char> downloadImageData(const std::string& imageUrl);
    // Loads texture from memory buffer
    GLuint loadTextureFromMemory(const std::vector<unsigned char>& imageData, const std::string& cacheKey);
    // Fetches card data from API and returns image URL
    std::string fetchCardImageUrl(const Card& card);
    // Maps your rarity string to API rarity search term
    std::string mapRarityToApiQuery(const std::string& rarity);

public:
    TextureManager();
    ~TextureManager();

    // Texture loading
    // Tries local path first, then URL download if applicable
    GLuint loadTexture(const std::string& pathOrUrl);

    GLuint getTexture(const std::string& textureName); // Used for caching lookup

    // Card texture generation
    // Will now attempt API fetch
    GLuint generateCardTexture(const Card& card);
    GLuint generateHoloEffect(GLuint baseTexture, const std::string& rarity);

    // Shader management
    void applyCardShader(const Card& card);
    void applyHoloShader(const Card& card, float time);
    GLuint getCurrentShader() const { return currentShader; }
    GLuint getCardShaderID() const { return cardShader; }     // Add this
    GLuint getHoloShaderID() const { return holoShader; }     // Add this

    // Initialize shaders
    void initializeShaders();
};

#endif