#ifndef CARD_HPP
#define CARD_HPP

#include "gl_core_3_3.h"
#include <string>
#include <glm/glm.hpp>

class Card {
private:
    std::string pokemonName;
    std::string pokemonType;  // fire, water, grass, etc.
    std::string rarity;       // normal, reverse, holo, ex, full art
    GLuint textureID;

    // Positioning and animation properties
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    glm::vec3 velocity;       // For physics-based animation

    // Material properties for different effects
    float shininess;
    float holoIntensity;

public:
    Card(std::string name, std::string type, std::string rarity);
    ~Card();

    void loadTexture();
    void render(const glm::mat4& viewProjection);
    void update(float deltaTime);

    // Getters and setters
    std::string getPokemonName() const;
    std::string getPokemonType() const;
    std::string getRarity() const;
    void setPosition(const glm::vec3& pos);
    glm::vec3 getPosition() const;
    void setRotation(const glm::vec3& rot);
    void setScale(const glm::vec3& scale);
    void setVelocity(const glm::vec3& vel);
};

#endif