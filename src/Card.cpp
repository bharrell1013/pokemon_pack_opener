#include "Card.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp> // For lerp (mix)
#include "TextureManager.hpp"
#include <GL/freeglut.h>
#include <iostream> // For debugging

// --- Constructor ---
Card::Card(std::string name, std::string type, std::string rarity, TextureManager* texManager) :
    pokemonName(name),
    pokemonType(type),
    rarity(rarity),
    textureID(0),
    textureManager(texManager),
    position(glm::vec3(0.0f)),
    rotation(glm::vec3(0.0f)),
    scale(glm::vec3(1.0f)),
    targetPosition(glm::vec3(0.0f)), // Initialize targets
    targetRotation(glm::vec3(0.0f)),
    targetScale(glm::vec3(1.0f)),
    isAnimating(false),                // Start not animating
    // velocity(glm::vec3(0.0f)), // Removed
    shininess(0.5f), // Keep if used by shaders eventually
    holoIntensity(0.0f), // Keep if used by shaders eventually
    revealProgress(0.0f), // Potentially remove
    isRevealed(false)      // Potentially remove
{
    initializeMesh();
    loadTexture(); // Load texture after mesh is potentially initialized
    targetPosition = position; // Initial target is current position
    targetRotation = rotation;
    targetScale = scale;
}

Card::~Card() {
    // glDeleteTextures(1, &textureID); // TextureManager should handle this
}

// --- Copy Constructor --- (Update for new members)
Card::Card(const Card& other) :
    pokemonName(other.pokemonName),
    pokemonType(other.pokemonType),
    rarity(other.rarity),
    textureID(other.textureID), // Texture Manager owns texture, just copy ID
    cardMesh(other.cardMesh),   // Share the mesh pointer
    textureManager(other.textureManager),
    position(other.position),
    rotation(other.rotation),
    scale(other.scale),
    targetPosition(other.targetPosition),
    targetRotation(other.targetRotation),
    targetScale(other.targetScale),
    isAnimating(other.isAnimating),
    shininess(other.shininess),
    holoIntensity(other.holoIntensity),
    revealProgress(other.revealProgress),
    isRevealed(other.isRevealed)
{
}

// --- Copy Assignment --- (Update for new members)
Card& Card::operator=(const Card& other) {
    if (this != &other) {
        pokemonName = other.pokemonName;
        pokemonType = other.pokemonType;
        rarity = other.rarity;
        textureID = other.textureID;
        cardMesh = other.cardMesh;
        textureManager = other.textureManager;
        position = other.position;
        rotation = other.rotation;
        scale = other.scale;
        targetPosition = other.targetPosition;
        targetRotation = other.targetRotation;
        targetScale = other.targetScale;
        isAnimating = other.isAnimating;
        shininess = other.shininess;
        holoIntensity = other.holoIntensity;
        revealProgress = other.revealProgress;
        isRevealed = other.isRevealed;
    }
    return *this;
}

// --- Move Constructor --- (Update for new members)
Card::Card(Card&& other) noexcept :
    pokemonName(std::move(other.pokemonName)),
    pokemonType(std::move(other.pokemonType)),
    rarity(std::move(other.rarity)),
    textureID(other.textureID),
    cardMesh(std::move(other.cardMesh)),
    textureManager(other.textureManager),
    position(other.position),
    rotation(other.rotation),
    scale(other.scale),
    targetPosition(other.targetPosition),
    targetRotation(other.targetRotation),
    targetScale(other.targetScale),
    isAnimating(other.isAnimating),
    shininess(other.shininess),
    holoIntensity(other.holoIntensity),
    revealProgress(other.revealProgress),
    isRevealed(other.isRevealed)
{
    other.textureID = 0; // Prevent double deletion if Card destructor managed it
    other.textureManager = nullptr;
}

// --- Move Assignment --- (Update for new members)
Card& Card::operator=(Card&& other) noexcept {
    if (this != &other) {
        pokemonName = std::move(other.pokemonName);
        pokemonType = std::move(other.pokemonType);
        rarity = std::move(other.rarity);
        textureID = other.textureID;
        cardMesh = std::move(other.cardMesh);
        textureManager = other.textureManager;
        position = other.position;
        rotation = other.rotation;
        scale = other.scale;
        targetPosition = other.targetPosition;
        targetRotation = other.targetRotation;
        targetScale = other.targetScale;
        isAnimating = other.isAnimating;
        shininess = other.shininess;
        holoIntensity = other.holoIntensity;
        revealProgress = other.revealProgress;
        isRevealed = other.isRevealed;

        other.textureID = 0;
        other.textureManager = nullptr;
    }
    return *this;
}

// --- loadTexture (Keep Existing) ---
void Card::loadTexture() {
    // Check if we already have a texture loaded
    if (textureID != 0) { return; }

    // Try to generate the specific texture using TextureManager logic
    // This function should handle fallback internally now
    textureID = textureManager->generateCardTexture(*this);

    if (textureID == 0) {
        std::cerr << "Error: Card::loadTexture failed to get any texture ID for " << pokemonName << std::endl;
        // Attempt to load a default placeholder as a last resort
        textureID = textureManager->loadTexture("textures/pokemon/placeholder.png");
        if (textureID == 0) {
            std::cerr << "FATAL: Could not load even the default placeholder texture!" << std::endl;
            // Consider throwing or handling this critical failure
        }
    }
    // Holo effect application might be better handled just before rendering
    // based on the shader chosen, rather than modifying the textureID here.
    // if (textureID != 0 && (rarity == "holo" || rarity == "reverse" || rarity == "ex" || rarity == "full art")) {
    //     // textureID = textureManager->generateHoloEffect(textureID, rarity); // Revisit this if FBOs are used
    // }
}

// --- setTargetTransform ---
void Card::setTargetTransform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl) {
    targetPosition = pos;
    targetRotation = rot;
    targetScale = scl;
    // Start animating only if the target is different from the current state
    if (glm::distance(position, targetPosition) > 0.001f ||
        glm::distance(rotation, targetRotation) > 0.001f || // Approx compare angles
        glm::distance(scale, targetScale) > 0.001f)
    {
        isAnimating = true;
    }
    else {
        isAnimating = false; // Already at target
    }
}

// --- update ---
void Card::update(float deltaTime) {
    if (isAnimating) {
        float animationSpeed = 8.0f; // Or get from CardPack if needed
        float lerpFactor = glm::clamp(deltaTime * animationSpeed, 0.0f, 1.0f);

        // Interpolate position, rotation, scale
        position = glm::mix(position, targetPosition, lerpFactor);
        rotation = glm::mix(rotation, targetRotation, lerpFactor); // Simple lerp for angles (can jitter at 180 deg flips, use slerp for perfection)
        scale = glm::mix(scale, targetScale, lerpFactor);

        // Check if close enough to target
        const float threshold = 0.01f; // Adjust threshold as needed
        if (glm::distance(position, targetPosition) < threshold &&
            glm::distance(rotation, targetRotation) < threshold && // Angle check is approximate
            glm::distance(scale, targetScale) < threshold)
        {
            position = targetPosition; // Snap to final target
            rotation = targetRotation;
            scale = targetScale;
            isAnimating = false;      // Stop animating
            // std::cout << "Card " << pokemonName << " finished animating." << std::endl;
        }
    }
    // Remove old continuous rotation logic if present
}

// --- render (Mostly Unchanged, uses current position/rotation/scale) ---
void Card::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const {
    if (!cardMesh) {
        std::cerr << "Error: Card::render called with null cardMesh for " << pokemonName << std::endl;
        return;
    }
    //std::cout << "[DEBUG] Card::render() for " << pokemonName
    //    << " - textureManager ptr: " << textureManager;
    /*if (textureManager) {
        std::cout << ", currentShader via getter: " << textureManager->getCurrentShader() << std::endl;
    }
    else {
        std::cout << ", textureManager is NULL!" << std::endl;
    }*/
    if (textureID == 0) {
        // Maybe render a default color or skip?
        // std::cerr << "Warning: Rendering card " << pokemonName << " with textureID 0." << std::endl;
        // return;
    }

    // Get the currently active shader (set by CardPack::render via TextureManager)
    GLuint currentShader = textureManager->getCurrentShader();
    if (currentShader == 0) {
        std::cerr << "Error: Card::render - current shader from TextureManager is 0 for " << pokemonName << std::endl;
        return;
    }
    // Already checked shader validity in CardPack::render, but double check doesn't hurt
    // glUseProgram(currentShader); // Should already be bound by CardPack::render

   // Calculate Model Matrix using current interpolated values
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = glm::scale(modelMatrix, scale);

    // Set Matrix Uniforms
    GLint modelLoc = glGetUniformLocation(currentShader, "model");
    GLint viewLoc = glGetUniformLocation(currentShader, "view");
    GLint projLoc = glGetUniformLocation(currentShader, "projection");

    if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
    // else { std::cerr << "Warning: Matrix uniform not found..." << std::endl; }

    // Set Texture Uniform
    GLint texSamplerLoc = glGetUniformLocation(currentShader, "cardTexture");
    if (texSamplerLoc != -1) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glUniform1i(texSamplerLoc, 0);
    }
    // else { std::cerr << "Warning: cardTexture uniform not found..." << std::endl; }

    // --- Draw the Card Mesh ---
    cardMesh->draw();

    // --- Clean up ---
    glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture
}

// --- initializeMesh (Keep Existing) ---
void Card::initializeMesh() {
    cardMesh = std::make_shared<Mesh>();
    // Apply scale factor directly to dimensions or handle via setScale
    float w = 2.5f / 2.0f; // Half width
    float h = 3.5f / 2.0f; // Half height
    std::vector<float> vertices = {
        // Positions   // Texture coords  // Normals (Ensure they face +Z if card starts facing camera)
        -w, -h, 0.0f,    0.0f, 0.0f,    0.0f, 0.0f, 1.0f,
         w, -h, 0.0f,    1.0f, 0.0f,    0.0f, 0.0f, 1.0f,
         w,  h, 0.0f,    1.0f, 1.0f,    0.0f, 0.0f, 1.0f,
        -w,  h, 0.0f,    0.0f, 1.0f,    0.0f, 0.0f, 1.0f
    };
    std::vector<unsigned int> indices = { 0, 1, 2, 2, 3, 0 };
    cardMesh->initialize(vertices, indices);
}

// --- Getters/Setters ---
std::string Card::getPokemonName() const { return pokemonName; }
std::string Card::getPokemonType() const { return pokemonType; }
std::string Card::getRarity() const { return rarity; }
void Card::setPosition(const glm::vec3& pos) { position = pos; }
glm::vec3 Card::getPosition() const { return position; }
void Card::setRotation(const glm::vec3& rot) { rotation = rot; }
//glm::vec3 Card::getRotation() const { return rotation; } // Add getter for rotation
void Card::setScale(const glm::vec3& s) { scale = s; }
void Card::setTextureID(GLuint id) { textureID = id; }
// void Card::setVelocity(const glm::vec3& vel) { velocity = vel; } // Removed