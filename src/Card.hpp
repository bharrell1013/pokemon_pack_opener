#ifndef CARD_HPP
#define CARD_HPP

#include "gl_core_3_3.h"
#include <string>
#include <glm/glm.hpp>
#include <memory>
#include "mesh.hpp"

// Forward declaration
class TextureManager;

class Card {
private:
    std::string pokemonName;
    std::string pokemonType;
    std::string rarity;
    GLuint textureID = 0; // Initialize to 0 (invalid texture)
    GLuint overlayTextureID = 0; // Overlay texture (L-System)
    std::shared_ptr<Mesh> cardMesh;
    TextureManager* textureManager;

    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec3 velocity;

    glm::vec3 targetPosition;
    glm::vec3 targetRotation;
    glm::vec3 targetScale;
    bool isAnimating;

    float shininess;
    float holoIntensity; // May not be needed if handled solely by shader+rarity

    float revealProgress;
    bool isRevealed;

    const float CARD_WIDTH = 2.5f * 0.8f;
    const float CARD_HEIGHT = 3.5f * 0.8f;
    const float CARD_DEPTH = 0.01f;

    GLuint cardShaderProgramID = 0; // Store shader ID for rendering

public:
    Card(std::string name, std::string type, std::string rarity, TextureManager* texManager);
    ~Card();

    // Rule of 5/6 if needed (copy/move constructors/assignment)
    Card(const Card& other);
    Card& operator=(const Card& other);
    Card(Card&& other) noexcept;
    Card& operator=(Card&& other) noexcept;

    void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos) const;
    void update(float deltaTime);

    void setTargetTransform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl);
    bool isCardAnimating() const { return isAnimating; }

    // Getters
    std::string getPokemonName() const { return pokemonName; }
    std::string getPokemonType() const { return pokemonType; }
    std::string getRarity() const { return rarity; }
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getRotation() const { return rotation; }
    glm::vec3 getScale() const { return scale; }
    GLuint getTextureID() const { return textureID; }
    GLuint getOverlayTextureID() const { return overlayTextureID; };

    // Setters
    void setPosition(const glm::vec3& pos);
    void setRotation(const glm::vec3& rot);
    void setScale(const glm::vec3& scl);
    void setVelocity(const glm::vec3& vel);
    void setTextureID(GLuint id);
    void setOverlayTextureID(GLuint id);

private:
    void initializeMesh();
    void updateTransform(float deltaTime); // Pass deltaTime for interpolation
};

#endif