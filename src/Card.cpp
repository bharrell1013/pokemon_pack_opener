#include "Card.hpp"
#include <glm/gtc/matrix_transform.hpp>

Card::Card(std::string name, std::string type, std::string rarity) :
    pokemonName(name),
    pokemonType(type),
    rarity(rarity),
    textureID(0),
    position(glm::vec3(0.0f, 0.0f, 0.0f)),
    rotation(glm::vec3(0.0f, 0.0f, 0.0f)),
    scale(glm::vec3(1.0f, 1.0f, 1.0f)),
    velocity(glm::vec3(0.0f, 0.0f, 0.0f)),
    shininess(0.5f),
    holoIntensity(0.0f)
{
    // Set holoIntensity based on rarity
    if (rarity == "holo") {
        holoIntensity = 0.7f;
    }
    else if (rarity == "reverse") {
        holoIntensity = 0.4f;
    }
    else if (rarity == "ex" || rarity == "full art") {
        holoIntensity = 0.9f;
    }
}

Card::~Card() {
    // Cleanup texture if needed
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

void Card::loadTexture() {
    // This will be implemented when we have the TextureManager
    // For now, just a placeholder
}

void Card::render(const glm::mat4& viewProjection) {
    // Create model matrix for the card
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Apply position
    modelMatrix = glm::translate(modelMatrix, position);

    // Apply rotation (in radians)
    modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

    // Apply scale
    modelMatrix = glm::scale(modelMatrix, scale);

    // Calculate final transformation matrix
    glm::mat4 mvpMatrix = viewProjection * modelMatrix;

    // For now, we'll just render a placeholder quad
    // Later, we'll render actual card meshes with textures
}

void Card::update(float deltaTime) {
    // Simple physics update
    position += velocity * deltaTime;

    // Add some damping to the velocity
    velocity *= 0.98f;
}

// Getters and setters
std::string Card::getPokemonName() const { return pokemonName; }
std::string Card::getPokemonType() const { return pokemonType; }
std::string Card::getRarity() const { return rarity; }

void Card::setPosition(const glm::vec3& pos) { position = pos; }
glm::vec3 Card::getPosition() const { return position; }
void Card::setRotation(const glm::vec3& rot) { rotation = rot; }
void Card::setScale(const glm::vec3& s) { scale = s; }
void Card::setVelocity(const glm::vec3& vel) { velocity = vel; }