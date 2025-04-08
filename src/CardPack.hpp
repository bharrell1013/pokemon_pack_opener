#ifndef CARDPACK_HPP
#define CARDPACK_HPP

#include <vector>
#include "Card.hpp"
#include "mesh.hpp"
#include "CardDatabase.hpp"
#include <memory>
#include <glm/glm.hpp> // Include glm

// Forward declaration
class TextureManager;

enum OpeningState {
    CLOSED,
    OPENING,
    OPENED
};

class CardPack {
private:
    std::vector<Card> cards;      // 10 cards per pack
    std::unique_ptr<Mesh> packModel; // 3D model of the pack
    OpeningState state;           // CLOSED, OPENING, OPENED
    TextureManager* textureManager; // Non-owning pointer to TextureManager
    float openingProgress;        // Animation progress (0.0-1.0)

    // Pack properties
    glm::vec3 position;
    glm::vec3 rotation;

public:
    CardPack(TextureManager* texManager);
    ~CardPack();

    void generateCards(CardDatabase& database);
    // *** MODIFIED Signature ***
    void render(GLuint packShaderProgramID, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint packTextureID);
    void update(float deltaTime);
    void startOpeningAnimation();
    bool isOpeningComplete() const;

    void rotate(float x, float y, float z);
    void setPosition(float x, float y, float z);
    // Additional methods for interaction
    bool isPointInside(float x, float y) const;
};

#endif