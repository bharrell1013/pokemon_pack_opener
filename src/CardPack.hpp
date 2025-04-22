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

enum PackState {
    CLOSED,
    REVEALING, // State for showing the stack and cycling
    FINISHED   // Optional: State after all cards cycled
};

class CardPack {
private:
    std::vector<Card> cards;      // 10 cards per pack
    std::unique_ptr<Mesh> packModel; // 3D model of the pack
    PackState state;           // CLOSED, OPENING, OPENED
    TextureManager* textureManager; // Non-owning pointer to TextureManager
    float openingProgress;        // Animation progress (0.0-1.0)

    // --- Animation & State ---
    int currentCardIndex;        // Index of the card currently at the front
    glm::vec3 frontPosition;       // Target position for the front card
    glm::vec3 backPositionOffset;  // Offset FROM THE STACK CENTER for the back card
    glm::vec3 stackCenter;         // Center position of the initial stack
    float stackSpacing;          // Z-spacing between cards in the stack
    float animationSpeed;        // How fast cards move between positions
    bool isAnimating;            // Flag to prevent cycling during movement 
    int cardMovingToBackIndex; // Index of the card doing the side-then-back move, or -1 if none

    // Pack properties
    glm::vec3 position;
    glm::vec3 rotation;

    GLuint selectedPackPokemonTextureID; // ID of the overlay texture for this pack
    glm::vec3 packColor;                 // Base color for this pack

public:
    CardPack(TextureManager* texManager);
    ~CardPack();

    void generateCards(CardDatabase& database);
    // *** MODIFIED Signature ***
    void render(GLuint packShaderProgramID, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint packTextureID, const glm::vec3& cameraPos);
    void update(float deltaTime);
    //void startOpeningAnimation();
    //bool isOpeningComplete() const;
    void startOpeningAnimation(); // Renamed for clarity, transitions to REVEALING
    void cycleCard();             // Moves front card to back, next card to front
    bool isCycleComplete() const; // Checks if all cards have been cycled

    PackState getState() const { return state; } // Getter for state

    void rotate(float x, float y, float z);
    void setPosition(float x, float y, float z);
    // Additional methods for interaction
    bool isPointInside(float x, float y) const;

    int getCurrentCardIndex() const { return currentCardIndex; }
    std::vector<Card>& getCards() { return cards; } // Return non-const ref (use carefully)
};

#endif