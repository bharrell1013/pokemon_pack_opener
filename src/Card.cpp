#include "Card.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/compatibility.hpp> // For glm::mix (lerp)
#include <iostream>                  // For std::cerr, std::cout
#include <utility>                   // For std::move
#include <stdexcept>                 // For std::runtime_error (optional)
#include <cmath>                     // For std::abs, std::pow
#include <glm/gtx/string_cast.hpp>
#include "TextureManager.hpp"        // Include TextureManager header

Card::Card(std::string name, std::string type, std::string rarity, TextureManager* texManager) :
    pokemonName(std::move(name)),         // Use move for efficiency
    pokemonType(std::move(type)),
    rarity(std::move(rarity)),
    textureID(0),                         // Initialize textureID to 0 (invalid)
    overlayTextureID(0),                  // Initialize overlay texture ID to 0 (invalid)
	generatedOverlayLevel(-1), 		      // Initialize overlay level to -1 (not generated)
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
    overlayTextureID(other.overlayTextureID),
    generatedOverlayLevel(other.generatedOverlayLevel),
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
        overlayTextureID = other.overlayTextureID;
        generatedOverlayLevel = other.generatedOverlayLevel;
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
    overlayTextureID(other.overlayTextureID),
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
    other.overlayTextureID = 0;
    other.textureManager = nullptr;
}

// --- Move Assignment --- (Update for new members)
Card& Card::operator=(Card&& other) noexcept {
    if (this != &other) {
        pokemonName = std::move(other.pokemonName);
        pokemonType = std::move(other.pokemonType);
        rarity = std::move(other.rarity);
        textureID = other.textureID;
        overlayTextureID = other.overlayTextureID;
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
        other.overlayTextureID = 0;
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
        const float threshold = 0.02f; // Adjust threshold based on animation speed and visual needs
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

void Card::render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec3& cameraPos, bool isFrontCard) const {
    // 1. --- Sanity Checks ---
    if (!cardMesh || !textureManager) { /* ... */ return; }
    if (textureID == 0) { /* ... */ }
    if (overlayTextureID == 0) { /* ... */ }
    GLuint backTextureID = textureManager->getCardBackTextureID();
    if (backTextureID == 0) { /* ... */ }

    // 2. --- Get Active Shader ---
    GLuint currentShader = textureManager->getCurrentShader();
    if (currentShader == 0) { /* ... */ return; }

    // 3. --- Calculate Model Matrix ---
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    modelMatrix = glm::translate(modelMatrix, position);
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    modelMatrix = glm::scale(modelMatrix, scale);

    bool isHoloShader = (currentShader == textureManager->getHoloShaderID());

    // 4. --- Set Uniforms ---
    GLint modelLoc = glGetUniformLocation(currentShader, "model");
    GLint viewLoc = glGetUniformLocation(currentShader, "view");
    GLint projLoc = glGetUniformLocation(currentShader, "projection");
    GLint baseTexSamplerLoc = glGetUniformLocation(currentShader, "baseTexture");
    GLint overlayTexSamplerLoc = glGetUniformLocation(currentShader, "overlayTexture");
    GLint backTexSamplerLoc = glGetUniformLocation(currentShader, "backTexture");

    // Set common matrix uniforms
    if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    // --- Set Shader-Specific Uniforms ---
    if (isHoloShader) {
        // *** Set Holo Shader Uniforms HERE ***

        // Set viewPos (using the dynamic cameraPos argument)
        GLint vsViewPosLoc = glGetUniformLocation(currentShader, "viewPos");
        if (vsViewPosLoc != -1) {
            if (isFrontCard) {
                //std::cout << "Card::render - Setting DYNAMIC viewPos uniform (Loc: " << vsViewPosLoc
                //    << ", ShaderID: " << currentShader << ") to: ("
                //    << cameraPos.x << ", " << cameraPos.y << ", " << cameraPos.z << ")" << std::endl;
            }
            glUniform3fv(vsViewPosLoc, 1, glm::value_ptr(cameraPos));
            // Check error
            GLenum err = glGetError(); if (err != GL_NO_ERROR && isFrontCard) { std::cerr << "OpenGL Error after setting viewPos: " << err << std::endl; }
        }
        else {
            if (isFrontCard) { std::cerr << "ERROR: Uniform 'viewPos' not found in holo shader." << std::endl; }
        }

        // Set Normal Matrix
        GLint normalMatrixLoc = glGetUniformLocation(currentShader, "normalMatrix");
        if (normalMatrixLoc != -1) {
            if (abs(glm::determinant(glm::mat3(modelMatrix))) > 0.0001f) {
                glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMatrix)));
                glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
            }
            else {
                // Handle non-invertible matrix
                glm::mat3 identityMatrix = glm::mat3(1.0f);
                glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(identityMatrix));
            }
        }
        else { /* Optional Warning */ }

        // Set other holo uniforms (time, intensity, etc. - KEEP THESE COMMENTED OUT FOR THE TEST)
        GLint cardTypeLoc = glGetUniformLocation(currentShader, "cardType");
        // if (cardTypeLoc != -1) glUniform1i(cardTypeLoc, textureManager->getTypeValue(pokemonType)); // Keep commented/dummy for now

        // ... (Keep holoIntensity, etc., commented out for position shift test)

        GLint timeLoc = glGetUniformLocation(currentShader, "time"); // Keep commented out
        // if (timeLoc != -1) {
        //     float currentTime = static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f;
        //     glUniform1f(timeLoc, currentTime);
        // }
        // GLint holoIntensityLoc = glGetUniformLocation(currentShader, "holoIntensity"); // Keep commented out
        // if (holoIntensityLoc != -1) { ... }


       // <<< REMOVED the problematic block that overwrote viewPos >>>

    }
    else { // It's the standard Card Shader
        GLint cardTypeLoc = glGetUniformLocation(currentShader, "cardType");
        GLint cardRarityLoc = glGetUniformLocation(currentShader, "cardRarity");
        GLint overlayIntensityLoc = glGetUniformLocation(currentShader, "overlayIntensity");

        if (cardTypeLoc != -1) glUniform1i(cardTypeLoc, textureManager->getTypeValue(pokemonType));
        if (cardRarityLoc != -1) glUniform1i(cardRarityLoc, textureManager->getRarityValue(rarity));
        if (overlayIntensityLoc != -1) glUniform1f(overlayIntensityLoc, 0.15f);
    }

    // --- Bind Textures ---
    // Unit 0: Base
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    if (baseTexSamplerLoc != -1) glUniform1i(baseTexSamplerLoc, 0);

    // Unit 1: Overlay
    if (overlayTextureID != 0 && overlayTexSamplerLoc != -1) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, overlayTextureID);
        glUniform1i(overlayTexSamplerLoc, 1);
    }

    // Unit 2: Back
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, backTextureID);
    if (backTexSamplerLoc != -1) glUniform1i(backTexSamplerLoc, 2);

    // --- Bind other textures for Holo Shader (Units 3, 4) ---
    // (This part should technically be inside the if(isHoloShader) block,
    // but leaving it here might be okay if the non-holo shader ignores them.
    // Moving them inside is cleaner.)
    /*
    if (isHoloShader) {
        GLint rainbowGradLoc = ...;
        if (rainbowGradLoc != -1) { ... bind rainbow tex ... }
        GLint normalMapLoc = ...;
        if (normalMapLoc != -1) { ... bind normal map tex ... }
    }
    */

    // 5. --- Draw the Mesh ---
    cardMesh->draw();

    // 6. --- Clean up ---
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
    // Unbind units 3 and 4 if they were bound
    // glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_1D, 0);
    // glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, 0);
    // glActiveTexture(GL_TEXTURE0); // Reset
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
        // Position          Tex Coords    Normal             Tangent            Bitangent
        -w, -h, 0.0f,        0.0f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f, // Bottom Left
         w, -h, 0.0f,        1.0f, 0.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f, // Bottom Right
         w,  h, 0.0f,        1.0f, 1.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f, // Top Right
        -w,  h, 0.0f,        0.0f, 1.0f,   0.0f, 0.0f, 1.0f,   1.0f, 0.0f, 0.0f,   0.0f, 1.0f, 0.0f  // Top Left
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

void Card::setOverlayTextureID(GLuint id)
{
	this->overlayTextureID = id;
}

void Card::setGeneratedOverlayLevel(int level) {
    this->generatedOverlayLevel = level;
}

