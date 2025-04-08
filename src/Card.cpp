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
    holoIntensity(0.0f),
    revealProgress(0.0f),
    isRevealed(false)
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

    // Initialize card mesh
    initializeMesh();
}

Card::~Card() {
    // Cleanup texture if needed
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

void Card::loadTexture() {
    // Get texture path based on Pokemon type
    std::string texturePath = "textures/pokemon/" + pokemonName + ".png";
    std::string cardTemplatePath = "textures/cards/template_" + pokemonType + ".png";
    
    // Try to load the Pokemon texture
    if (textureID == 0) {
        // First try to load specific Pokemon texture
        textureID = TextureManager::getInstance().loadTexture(texturePath);
        
        if (textureID == 0) {
            // If specific texture not found, load default type template
            textureID = TextureManager::getInstance().loadTexture(cardTemplatePath);
        }
        
        // Apply special effects based on rarity
        if (rarity == "holo" || rarity == "reverse" || rarity == "ex" || rarity == "full art") {
            textureID = TextureManager::getInstance().generateHoloEffect(textureID, rarity);
        }
    }
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

    // Apply appropriate shader based on card rarity
    if (rarity == "holo" || rarity == "reverse" || rarity == "ex" || rarity == "full art") {
        TextureManager::getInstance().applyHoloShader(*this, static_cast<float>(glfwGetTime()));
    } else {
        TextureManager::getInstance().applyCardShader(*this);
    }

    // Bind texture
    if (textureID != 0) {
        glBindTexture(GL_TEXTURE_2D, textureID);
    }

    // Render card quad
    // This assumes we have a basic quad mesh set up
    // You'll need to implement the actual rendering code here
    // using your mesh system
}

void Card::update(float deltaTime) {
    // Update reveal animation if in progress
    if (!isRevealed) {
        updateRevealAnimation(deltaTime);
    }

    // Physics update
    position += velocity * deltaTime;
    velocity *= 0.98f; // Damping

    // Update transform
    updateTransform();
}

void Card::initializeMesh() {
    // Create a new mesh for the card
    cardMesh = std::make_unique<Mesh>();

    // Define card vertices (simple rectangle)
    std::vector<float> vertices = {
        // Positions          // Texture coords  // Normals
        -CARD_WIDTH/2, -CARD_HEIGHT/2, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f, 1.0f,
         CARD_WIDTH/2, -CARD_HEIGHT/2, 0.0f,    1.0f, 0.0f,    0.0f, 0.0f, 1.0f,
         CARD_WIDTH/2,  CARD_HEIGHT/2, 0.0f,    1.0f, 1.0f,    0.0f, 0.0f, 1.0f,
        -CARD_WIDTH/2,  CARD_HEIGHT/2, 0.0f,    0.0f, 1.0f,    0.0f, 0.0f, 1.0f
    };

    // Define indices for triangles
    std::vector<unsigned int> indices = {
        0, 1, 2,
        2, 3, 0
    };

    // Initialize the mesh
    cardMesh->initialize(vertices, indices);
}

void Card::startRevealAnimation() {
    revealProgress = 0.0f;
    isRevealed = false;
}

void Card::updateRevealAnimation(float deltaTime) {
    if (!isRevealed) {
        revealProgress += deltaTime;
        if (revealProgress >= 1.0f) {
            revealProgress = 1.0f;
            isRevealed = true;
        }

        // Update rotation based on reveal progress
        float revealAngle = (1.0f - revealProgress) * glm::pi<float>();
        rotation.y = revealAngle;
    }
}

void Card::updateTransform() {
    // This will be called whenever position, rotation, or scale changes
    // The actual transformation matrix is computed in the render function
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