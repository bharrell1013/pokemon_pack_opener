#include "Card.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "TextureManager.hpp"
#include <freeglut_std.h>

Card::Card(std::string name, std::string type, std::string rarity, TextureManager* texManager) :
    pokemonName(name),
    pokemonType(type),
    rarity(rarity),
    textureID(0),
    textureManager(texManager),
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
    
    // Load texture
    loadTexture();
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
    std::string cardTemplatePath = "textures/cards/card_template.png";
    
    // Try to load the Pokemon texture
    if (textureID == 0) {
        // First try to load specific Pokemon texture
        textureID = textureManager->loadTexture(texturePath);
        
        if (textureID == 0) {
            // If specific texture not found, load default template
            textureID = textureManager->loadTexture(cardTemplatePath);
        }
        
        // Apply special effects based on rarity
        if (rarity == "holo" || rarity == "reverse" || rarity == "ex" || rarity == "full art") {
            textureID = textureManager->generateHoloEffect(textureID, rarity);
        }
    }
}

// Copy constructor
Card::Card(const Card& other) :
    pokemonName(other.pokemonName),
    pokemonType(other.pokemonType),
    rarity(other.rarity),
    textureID(other.textureID),
    position(other.position),
    rotation(other.rotation),
    scale(other.scale),
    velocity(other.velocity),
    shininess(other.shininess),
    holoIntensity(other.holoIntensity),
    revealProgress(other.revealProgress),
    isRevealed(other.isRevealed)
{
    if (other.cardMesh) {
        cardMesh = other.cardMesh; // Shared pointer will handle reference counting
    }
}

// Copy assignment operator
Card& Card::operator=(const Card& other) {
    if (this != &other) {
        pokemonName = other.pokemonName;
        pokemonType = other.pokemonType;
        rarity = other.rarity;
        textureID = other.textureID;
        position = other.position;
        rotation = other.rotation;
        scale = other.scale;
        velocity = other.velocity;
        shininess = other.shininess;
        holoIntensity = other.holoIntensity;
        revealProgress = other.revealProgress;
        isRevealed = other.isRevealed;
        cardMesh = other.cardMesh;
    }
    return *this;
}

// Move constructor
Card::Card(Card&& other) noexcept :
    pokemonName(std::move(other.pokemonName)),
    pokemonType(std::move(other.pokemonType)),
    rarity(std::move(other.rarity)),
    textureID(other.textureID),
    cardMesh(std::move(other.cardMesh)),
    position(other.position),
    rotation(other.rotation),
    scale(other.scale),
    velocity(other.velocity),
    shininess(other.shininess),
    holoIntensity(other.holoIntensity),
    revealProgress(other.revealProgress),
    isRevealed(other.isRevealed)
{
    other.textureID = 0;
}

// Move assignment operator
Card& Card::operator=(Card&& other) noexcept {
    if (this != &other) {
        pokemonName = std::move(other.pokemonName);
        pokemonType = std::move(other.pokemonType);
        rarity = std::move(other.rarity);
        textureID = other.textureID;
        cardMesh = std::move(other.cardMesh);
        position = other.position;
        rotation = other.rotation;
        scale = other.scale;
        velocity = other.velocity;
        shininess = other.shininess;
        holoIntensity = other.holoIntensity;
        revealProgress = other.revealProgress;
        isRevealed = other.isRevealed;
        
        other.textureID = 0;
    }
    return *this;
}

void Card::render(const glm::mat4& viewProjection) {
    if (!cardMesh) {
        return;  // Skip if mesh isn't initialized
    }

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

    // Apply appropriate shader based on card rarity first
    if (rarity == "holo" || rarity == "reverse" || rarity == "ex" || rarity == "full art") {
        textureManager->applyHoloShader(*this, static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f);
    } else {
        textureManager->applyCardShader(*this);
    }

    // Bind texture
    if (textureID != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
    }

    // Set MVP matrix in current shader program
    if (textureManager) {
        GLuint shader = textureManager->getCurrentShader();
        GLint mvpLoc = glGetUniformLocation(shader, "mvpMatrix"); // Changed uniform name to match shader
        if (mvpLoc != -1) {
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));
        }
    }

    // Draw the card mesh
    cardMesh->draw();

    // Cleanup state
    glBindTexture(GL_TEXTURE_2D, 0);
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
    cardMesh = std::make_shared<Mesh>();

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

    // Initialize the mesh with our vertex data and indices
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