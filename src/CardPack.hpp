#ifndef CARDPACK_HPP
#define CARDPACK_HPP

#include <vector>
#include "Card.hpp"
#include "mesh.hpp"
#include "CardDatabase.hpp"
#include <memory>

enum OpeningState {
    CLOSED,
    OPENING,
    OPENED
};

class CardPack {
private:
    std::vector<Card> cards;       // 10 cards per pack
    std::unique_ptr<Mesh> packModel;  // 3D model of the pack
    OpeningState state;            // CLOSED, OPENING, OPENED
    float openingProgress;         // Animation progress (0.0-1.0)

    // Pack properties
    glm::vec3 position;
    glm::vec3 rotation;

public:
    CardPack();
    ~CardPack();

    void generateCards(CardDatabase& database);
    void render(GLuint shaderProgramID);
    void update(float deltaTime);
    void startOpeningAnimation();
    bool isOpeningComplete() const;

    void rotate(float x, float y, float z);
    void setPosition(float x, float y, float z);
    // Additional methods for interaction
    bool isPointInside(float x, float y) const;
};

#endif