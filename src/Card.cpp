#include "Card.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "TextureManager.hpp"
#include <GL/freeglut.h>

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
    // Check if we already have a texture loaded
    if (textureID != 0) {
        return;
    }

    // Try to get cached placeholder texture
    textureID = textureManager->getTexture("textures/pokemon/placeholder.png");
    if (textureID != 0) {
        goto apply_effects;
    }

    // Try to load placeholder
    textureID = textureManager->loadTexture("textures/pokemon/placeholder.png");
    if (textureID != 0) {
        goto apply_effects;
    }

    // As last resort, use card template
    textureID = textureManager->getTexture("textures/cards/card_template.png");
    if (textureID == 0) {
        textureID = textureManager->loadTexture("textures/cards/card_template.png");
    }

apply_effects:
    // Apply special effects based on rarity
    if (textureID != 0 && (rarity == "holo" || rarity == "reverse" || rarity == "ex" || rarity == "full art")) {
        textureID = textureManager->generateHoloEffect(textureID, rarity);
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

// Card.cpp (render function only)

#include <glm/gtc/type_ptr.hpp> // For glm::value_ptr
#include <iostream> // For cerr

// *** MODIFIED Signature ***
void Card::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const {
    if (!cardMesh) {
        std::cerr << "Error: Card::render called with null cardMesh for " << pokemonName << std::endl;
        return;
    }
    // Removed redundant textureID check log, keep behavior
    if (textureID == 0) {
        // Optionally render untextured or skip
        // return;
    }


    // Get the currently active shader (set by TextureManager::apply*)
    // Getting the shader doesn't modify the Card, so this is okay in a const method
    GLuint currentShader = textureManager->getCurrentShader();
    if (currentShader == 0) {
        std::cerr << "Error: Card::render - current shader from TextureManager is 0." << std::endl;
        return;
    }

    // --- Calculate Model Matrix --- (Reading members is okay in const)
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = glm::scale(modelMatrix, scale);

    // --- Set Matrix Uniforms --- (OpenGL calls don't modify the C++ object)
    GLint modelLoc = glGetUniformLocation(currentShader, "model");
    GLint viewLoc = glGetUniformLocation(currentShader, "view");
    GLint projLoc = glGetUniformLocation(currentShader, "projection");

    if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    // --- Set Texture Uniform --- (OpenGL calls don't modify the C++ object)
    GLint texSamplerLoc = glGetUniformLocation(currentShader, "cardTexture");
    if (texSamplerLoc != -1) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID); // Reading textureID is okay
        glUniform1i(texSamplerLoc, 0);
    }

    // --- Draw the Card Mesh ---
    // cardMesh is a unique_ptr. Accessing via -> calls Mesh::draw()
    // We need Mesh::draw() to be const too! (See below)
    cardMesh->draw();

    // --- Clean up ---
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Card::update(float deltaTime) {
    // Update reveal animation if in progress
    if (!isRevealed) {
        updateRevealAnimation(deltaTime);
    }

    // Physics update
    //position += velocity * deltaTime;
    //velocity *= 0.98f; // Damping

    // -- - ADD CONTINUOUS ROTATION(or other idle animation) HERE-- -
        // This will run *after* the reveal animation is complete.
        if (isRevealed) { // Only apply continuous rotation after reveal
            rotation.y += deltaTime * 0.5f; // Rotate around Y axis continuously
            // Keep rotation within 0-2PI range to avoid large numbers (optional)
            // rotation.y = fmod(rotation.y, 2.0f * glm::pi<float>());
        }

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