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
    std::string pokemonType;  // fire, water, grass, etc.
    std::string rarity;       // normal, reverse, holo, ex, full art
    GLuint textureID;
    std::shared_ptr<Mesh> cardMesh;  // Changed to shared_ptr for proper copying
    TextureManager* textureManager;   // Non-owning pointer to TextureManager

    // Positioning and animation properties
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec3 velocity;       // For physics-based animation

    glm::vec3 targetPosition;
    glm::vec3 targetRotation;
    glm::vec3 targetScale;
    bool isAnimating;

    // Material properties for different effects
    float shininess;
    float holoIntensity;

    // Animation properties
    float revealProgress;     // 0.0 to 1.0 for card reveal animation
    bool isRevealed;
    
    // Card dimensions
    const float CARD_WIDTH = 2.5f * 0.8f;
    const float CARD_HEIGHT = 3.5f * 0.8f;
    const float CARD_DEPTH = 0.01f;

public:
    Card(std::string name, std::string type, std::string rarity, TextureManager* texManager);
    ~Card();

    // Copy constructor and assignment operator
    Card(const Card& other);
    Card& operator=(const Card& other);

    // Move constructor and assignment operator
    Card(Card&& other) noexcept;
    Card& operator=(Card&& other) noexcept;

    void loadTexture();
    void render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const;
    void update(float deltaTime);

    void setTargetTransform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl);
    bool isCardAnimating() const { return isAnimating; }
    
    // Animation controls
    void startRevealAnimation();
    void updateRevealAnimation(float deltaTime);
    bool isRevealComplete() const { return isRevealed; }

    // Getters and setters
    std::string getPokemonName() const;
    std::string getPokemonType() const;
    std::string getRarity() const;
    void setPosition(const glm::vec3& pos);
    glm::vec3 getPosition() const;
    void setRotation(const glm::vec3& rot);
    void setScale(const glm::vec3& scale);
    void setVelocity(const glm::vec3& vel);
    glm::vec3 getScale() const { return scale; } // Add getter for scale
	glm::vec3 getRotation() const { return rotation; } // Add getter for rotation

private:
    void initializeMesh();
    void updateTransform();
};

#endif