#ifndef TEXTUREMANAGER_HPP
#define TEXTUREMANAGER_HPP

#include "gl_core_3_3.h"
#include <map>
#include <string>
#include <vector>
#include <list>

// Forward declaration
class Card;

class TextureManager {
private:
    // --- Texture Management ---
    std::map<std::string, GLuint> textureMap;  // Cache for loaded textures
    std::map<std::string, std::list<std::string>> apiUrlCache; // Cache for API query results

    // --- Shader Programs ---
    GLuint cardShader = 0;        // Standard card shader program
    GLuint holoShader = 0;        // Holographic effect shader program
    GLuint currentShader = 0;     // Currently active shader program

    // --- API Configuration ---
    const std::string apiKey = "56f39a72-5758-495c-ac18-134248507b5a";
    const std::string apiBaseUrl = "https://api.pokemontcg.io/v2/cards";

    // --- Private Helper Methods ---
    // Shader Management
    GLuint createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath);
    std::string loadShaderSource(const std::string& path);
    
    // Type/Rarity Conversion
    int getTypeValue(const std::string& type);      // Converts Pokemon type to shader value
    int getRarityValue(const std::string& rarity);  // Converts card rarity to shader value

    // API & Image Loading
    std::vector<unsigned char> downloadImageData(const std::string& imageUrl);  // Downloads image data from URL
    GLuint loadTextureFromMemory(const std::vector<unsigned char>& imageData, const std::string& cacheKey);
    std::string fetchCardImageUrl(const Card& card);  // Gets image URL from Pokemon TCG API
    std::string mapRarityToApiQuery(const std::string& rarity);  // Converts rarity to API query

public:
    TextureManager();
    ~TextureManager();

    // --- Texture Loading ---
    // Loads texture from file or URL, with caching
    GLuint loadTexture(const std::string& pathOrUrl);
    // Retrieves cached texture by name/path/URL
    GLuint getTexture(const std::string& textureName);

    // --- Card Texture Generation ---
    // Generates card texture using API or fallbacks
    GLuint generateCardTexture(const Card& card);
    // Applies holographic effect (may use FBO in future)
    GLuint generateHoloEffect(GLuint baseTexture, const std::string& rarity);

    // --- Shader Management ---
    // Initialize all shader programs
    void initializeShaders();
    // Apply appropriate shader for card rendering
    void applyCardShader(const Card& card);
    // Apply holographic effect shader with time parameter
    void applyHoloShader(const Card& card, float time);

    // --- Shader State Getters ---
    GLuint getCurrentShader() const { return currentShader; }
    GLuint getCardShaderID() const { return cardShader; }
    GLuint getHoloShaderID() const { return holoShader; }
};

#endif