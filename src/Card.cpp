#include "Card.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp> // For glm::mix (lerp)
#include <GL/freeglut.h>             // For OpenGL types if needed (like GLuint) - Already included via gl_core_3_3?
#include <iostream>                  // For std::cerr, std::cout
#include <utility>                   // For std::move
#include <stdexcept>                 // For std::runtime_error (optional)
#include <cmath>                     // For std::abs, std::pow
#include "TextureManager.hpp"        // Include TextureManager header

Card::Card(std::string name, std::string type, std::string rarity, TextureManager* texManager) :
    pokemonName(std::move(name)),         // Use move for efficiency
    pokemonType(std::move(type)),
    rarity(std::move(rarity)),
    textureID(0),                         // Initialize textureID to 0 (invalid)
    textureManager(texManager),           // Store pointer to TextureManager
    position(glm::vec3(0.0f)),            // Initial position
    rotation(glm::vec3(0.0f)),            // Initial rotation
    scale(glm::vec3(1.0f)),               // Initial scale
    targetPosition(glm::vec3(0.0f)),      // Initialize target position
    targetRotation(glm::vec3(0.0f)),      // Initialize target rotation
    targetScale(glm::vec3(1.0f)),         // Initialize target scale
    isAnimating(false),                   // Start not animating
    shininess(32.0f),                     // Default shininess (adjust if used)
    holoIntensity(0.0f),                  // Default holo intensity (adjust if used)
    revealProgress(0.0f),                 // Keep if planning separate reveal animation
    isRevealed(false)                     // Keep if planning separate reveal animation
{
    if (!textureManager) {
        // This can be a fatal error depending on your application structure
        std::cerr << "WARNING: Card '" << pokemonName << "' created with a null TextureManager!" << std::endl;
        // Consider throwing std::runtime_error("Card requires a valid TextureManager");
    }
    initializeMesh(); // Create the card's geometry

    // Texture loading is now handled externally by CardPack::generateCards
    // which calls textureManager->generateCardTexture(*this) and then card.setTextureID()

    // Set initial targets to current state so it doesn't animate immediately
    targetPosition = position;
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

// --- setTargetTransform ---
// Sets the target state for the card's animation
void Card::setTargetTransform(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scl) {
    targetPosition = pos;
    targetRotation = rot;
    targetScale = scl;

    // Check if the card is already at the target. Only start animating if it's not.
    // Use a small epsilon for floating-point comparisons.
    const float epsilon = 0.001f;
    bool positionDifferent = glm::distance(position, targetPosition) > epsilon;
    // Comparing Euler angles directly for difference is tricky due to wrapping.
    // A simple distance check might suffice for basic animations, but quaternions are better.
    // For now, we'll use distance, assuming rotations aren't overly complex.
    bool rotationDifferent = glm::distance(rotation, targetRotation) > epsilon;
    bool scaleDifferent = glm::distance(scale, targetScale) > epsilon;

    if (positionDifferent || rotationDifferent || scaleDifferent) {
        isAnimating = true; // Start the animation in the update loop
    } else {
        isAnimating = false; // Already at the target
        // Ensure current state exactly matches target if not animating
        position = targetPosition;
        rotation = targetRotation;
        scale = targetScale;
    }
}

// --- update ---
// Interpolates the card's transform towards the target if animating
void Card::update(float deltaTime) {
    if (isAnimating) {
        // --- Interpolation Factor ---
        // Option 1: Approach style (fast start, slow end)
        // float lerpFactor = 1.0f - std::pow(0.01f, deltaTime); // Adjust base (0.01f) for speed

        // Option 2: Fixed speed (more consistent movement)
        float animationSpeed = 8.0f; // Adjust speed as needed (units/radians per second)
        float lerpFactor = glm::clamp(deltaTime * animationSpeed, 0.0f, 1.0f); // Ensure factor is [0, 1]

        // Interpolate position, rotation, and scale using glm::mix
        position = glm::mix(position, targetPosition, lerpFactor);

        // Interpolate Euler angles. NOTE: This can cause issues like gimbal lock or non-shortest path
        // for large rotations. For robust rotation animation, use quaternions and glm::slerp.
        // For simple card flips/turns, lerp might be acceptable.
        rotation = glm::mix(rotation, targetRotation, lerpFactor);

        scale = glm::mix(scale, targetScale, lerpFactor);

        // --- Check if animation is complete ---
        // Check if the current state is very close to the target state.
        const float threshold = 0.01f; // Adjust threshold based on animation speed and visual needs
        bool positionReached = glm::distance(position, targetPosition) < threshold;
        // Angle check remains approximate with lerp
        bool rotationReached = glm::distance(rotation, targetRotation) < threshold;
        bool scaleReached = glm::distance(scale, targetScale) < threshold;

        if (positionReached && rotationReached && scaleReached) {
            // Snap to the exact target position/rotation/scale to avoid small drifts
            position = targetPosition;
            rotation = targetRotation;
            scale = targetScale;
            isAnimating = false; // Stop animating
            // std::cout << "Card '" << pokemonName << "' finished animating." << std::endl;
        }
    }
}

// --- render ---
void Card::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) const {
    // 1. --- Sanity Checks ---
    if (!cardMesh) {
        std::cerr << "Error: Card::render - Cannot render '" << pokemonName << "', mesh is null." << std::endl;
        return;
    }
    if (!textureManager) {
        std::cerr << "Error: Card::render - Cannot render '" << pokemonName << "', textureManager is null." << std::endl;
        return;
    }
     // Texture ID 0 might be valid if you have a placeholder bound to it,
     // but often indicates an error. Render anyway, but maybe log a warning.
     if (textureID == 0) {
         // std::cerr << "Warning: Card::render - Rendering '" << pokemonName << "' with texture ID 0." << std::endl;
     }

    // 2. --- Get Active Shader ---
    // Assume the correct shader (card or holo) was already activated by CardPack::render
    // using textureManager->applyCardShader or textureManager->applyHoloShader.
    GLuint currentShader = textureManager->getCurrentShader();
    if (currentShader == 0) {
        // This indicates an issue in the calling code (CardPack::render) or shader initialization
        std::cerr << "Error: Card::render - TextureManager reports current shader ID is 0 for '" << pokemonName << "'. Cannot render." << std::endl;
        return;
    }

    // 3. --- Calculate Model Matrix ---
    // Use the *current* (potentially interpolated) position, rotation, and scale.
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    // Apply rotations - order might matter depending on desired effect (e.g., YXZ common)
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw
    modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
    modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Roll
    modelMatrix = glm::scale(modelMatrix, scale);

    // 4. --- Set Uniforms ---
    // Get uniform locations (cache these in a Shader class for better performance)
    GLint modelLoc = glGetUniformLocation(currentShader, "model");
    GLint viewLoc = glGetUniformLocation(currentShader, "view");
    GLint projLoc = glGetUniformLocation(currentShader, "projection");
    // Use "diffuseTexture" to be consistent with CardPack's pack shader uniform name
    GLint texSamplerLoc = glGetUniformLocation(currentShader, "cardTexture");

    // Set matrix uniforms (check locations validity)
    if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    // else std::cerr << "Warning: 'model' uniform not found in shader " << currentShader << std::endl;

    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
    // else std::cerr << "Warning: 'view' uniform not found in shader " << currentShader << std::endl;

    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));
    // else std::cerr << "Warning: 'projection' uniform not found in shader " << currentShader << std::endl;

    // Set texture sampler uniform
    if (texSamplerLoc != -1) {
        glActiveTexture(GL_TEXTURE0);         // Activate texture unit 0
        glBindTexture(GL_TEXTURE_2D, textureID); // Bind the card's specific texture
        glUniform1i(texSamplerLoc, 0);       // Tell the shader sampler to use texture unit 0
    } else {
        // std::cerr << "Warning: 'diffuseTexture' uniform not found in shader " << currentShader << std::endl;
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture if uniform is missing
    }

    // 5. --- Draw the Mesh ---
    cardMesh->draw();

    // 6. --- Clean up ---
    // Unbind texture (optional, but good practice if the next object might not set it)
    glBindTexture(GL_TEXTURE_2D, 0);
}

// --- initializeMesh ---
// Creates the geometry (vertices, indices) for the card mesh (simple quad).
void Card::initializeMesh() {
    // Use make_shared to create the Mesh object managed by shared_ptr
    cardMesh = std::make_shared<Mesh>();

    // Define card dimensions (adjust scale factor as needed)
    float baseWidth = 2.5f;
    float baseHeight = 3.5f;
    float scaleFactor = 0.8f; // Matches scale used elsewhere?
    float w = (baseWidth * scaleFactor) / 2.0f; // Half width
    float h = (baseHeight * scaleFactor) / 2.0f; // Half height

    // Define vertices: Position (x,y,z), Texture Coords (s,t), Normals (nx,ny,nz)
    // Ensure winding order and normals are correct for backface culling if enabled.
    // Normals point towards +Z if the card initially faces the camera along the -Z axis.
    std::vector<float> vertices = {
        // Position          Texture Coords    Normal
        -w, -h, 0.0f,        0.0f, 0.0f,       0.0f, 0.0f, 1.0f, // Bottom Left
         w, -h, 0.0f,        1.0f, 0.0f,       0.0f, 0.0f, 1.0f, // Bottom Right
         w,  h, 0.0f,        1.0f, 1.0f,       0.0f, 0.0f, 1.0f, // Top Right
        -w,  h, 0.0f,        0.0f, 1.0f,       0.0f, 0.0f, 1.0f  // Top Left
    };

    // Define indices for two triangles forming the quad
    std::vector<unsigned int> indices = {
        0, 1, 2,  // First triangle (Bottom Left, Bottom Right, Top Right)
        2, 3, 0   // Second triangle (Top Right, Top Left, Bottom Left)
    };

    // Initialize the mesh with the vertex and index data
    try {
        cardMesh->initialize(vertices, indices);
    } catch (const std::exception& e) {
         std::cerr << "FATAL: Failed to initialize mesh for card '" << pokemonName << "': " << e.what() << std::endl;
         cardMesh = nullptr; // Ensure mesh is null if initialization fails
         throw;
    }
}

// --- Setters ---
// Basic setters - might be useful for initial placement
void Card::setPosition(const glm::vec3& pos) {
    position = pos;
    if (!isAnimating) targetPosition = pos; // Update target if not currently moving
}

void Card::setRotation(const glm::vec3& rot) {
    rotation = rot;
     if (!isAnimating) targetRotation = rot; // Update target if not currently moving
}

void Card::setScale(const glm::vec3& scl) {
    scale = scl;
     if (!isAnimating) targetScale = scl; // Update target if not currently moving
}

// Setter for the texture ID (called by CardPack after TextureManager generates it)
void Card::setTextureID(GLuint id) {
    // Optional: Check if already have an ID? (Shouldn't happen with current flow)
    // if (textureID != 0 && textureID != id) {
    //     std::cerr << "Warning: Overwriting existing texture ID for card '" << pokemonName << "'" << std::endl;
    // }
    this->textureID = id;
}