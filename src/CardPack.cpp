#include "CardPack.hpp"
#include "TextureManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <GL/freeglut_std.h>

CardPack::CardPack(TextureManager* texManager) :
    state(CLOSED),
    openingProgress(0.0f),
    position(glm::vec3(0.0f, 0.0f, 0.0f)),
    rotation(glm::vec3(0.0f, 0.0f, 0.0f)),
    textureManager(texManager)
{
    std::cout << "CardPack constructor: Attempting to load pack model..." << std::endl;
    std::string packModelPath = "models/pack.obj"; // Primary model
    std::string fallbackModelPath = "models/cube.obj"; // Fallback

    // Try loading the primary pack model first
    if (std::filesystem::exists(packModelPath)) {
        try {
            packModel = std::make_unique<Mesh>(packModelPath);
            std::cout << "Loaded primary pack model: " << packModelPath << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to load primary pack model '" << packModelPath << "': " << e.what() << std::endl;
            packModel = nullptr; // Ensure it's null if primary failed
        }
    }
    else {
        std::cerr << "Primary pack model not found at: " << packModelPath << std::endl;
    }

    // If primary model failed or wasn't found, try the fallback
    if (!packModel) {
        std::cerr << "Attempting to load fallback model: " << fallbackModelPath << std::endl;
        if (std::filesystem::exists(fallbackModelPath)) {
            try {
                packModel = std::make_unique<Mesh>(fallbackModelPath);
                std::cout << "Loaded fallback model: " << fallbackModelPath << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to load fallback model '" << fallbackModelPath << "': " << e.what() << std::endl;
                // No model could be loaded - this is a problem
                packModel = nullptr; // Explicitly null
                throw std::runtime_error("Failed to load any pack model."); // Or handle differently
            }
        }
        else {
            std::cerr << "Fallback model also not found at: " << fallbackModelPath << std::endl;
            std::cerr << "Searched relative to working directory: " << std::filesystem::current_path() << std::endl;
            throw std::runtime_error("Cannot find any model (pack.obj or cube.obj)");
        }
    }
}

CardPack::~CardPack() {
    // Resources will be cleaned up by unique_ptr
}

// In CardPack::generateCards
void CardPack::generateCards(CardDatabase& database) {
    cards.clear();
    cards.reserve(10); // Optional but good practice: reserve space upfront

    const float Z_SPACING = 0.01f; // Small offset between cards

    for (int i = 0; i < 10; ++i) {
        std::string rarity = (i < 7) ? "normal" : (i < 9) ? "reverse" : "holo";

        // Construct Card directly in the vector
        cards.emplace_back("Test Pokemon", "normal", rarity, textureManager);

        // Get a reference to the newly created card to set properties
        Card& newCard = cards.back();

        // Set card position - arrange in a grid
        float x = (i % 3) * 1.2f - 1.2f;  // Was 1.5f
        float y = (i / 3) * 1.5f - 1.5f;  // Was 2.0f and -2.0f offset
        //float z = -i * Z_SPACING;
        float z = -i * Z_SPACING * 100.0f; // Increase spacing significantly for visibility
        //newCard.setPosition(glm::vec3(x, y, z));
        newCard.setPosition(glm::vec3(x, 0.0f, z));

        // Add some rotation for visual interest
        newCard.setRotation(glm::vec3(0.0f, 0.0f, 0.0f));
        newCard.setScale(glm::vec3(0.8f, 0.8f, 0.8f)); // <<< Add scaling (e.g., 80%)
    }
    std::cout << "Cards generated using emplace_back. Vector size: " << cards.size() << std::endl; // Add logging
}

// *** MODIFIED Signature ***
// CardPack.cpp (render function only)

// *** MODIFIED Signature ***
void CardPack::render(GLuint packShaderProgramID, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint packTextureID) {
    if (!packModel && cards.empty()) return;

    // --- Render the Pack Model (if state is CLOSED) ---
    if (packModel && state == CLOSED) {
        // Use the PACK shader program (passed in)
        // Application::render already set view/projection uniforms for this shader
        glUseProgram(packShaderProgramID); // Redundant if Application::render called UseProgram just before this

        // Create model matrix for the pack
        glm::mat4 packModelMatrix = glm::mat4(1.0f);
        packModelMatrix = glm::translate(packModelMatrix, position);
        packModelMatrix = glm::rotate(packModelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        packModelMatrix = glm::rotate(packModelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        packModelMatrix = glm::rotate(packModelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

        // Set MODEL Matrix for the PACK shader
        GLint modelLoc = glGetUniformLocation(packShaderProgramID, "model");
        if (modelLoc != -1) {
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(packModelMatrix));
        }
        else {
            std::cerr << "Warning: Uniform 'model' not found in pack shader program " << packShaderProgramID << std::endl;
        }

        // *** Setup Texture for the PACK shader ***
        GLint texLoc = glGetUniformLocation(packShaderProgramID, "diffuseTexture");
        if (texLoc != -1 && packTextureID != 0) {
            glActiveTexture(GL_TEXTURE0); // Activate texture unit 0
            glBindTexture(GL_TEXTURE_2D, packTextureID); // Bind the loaded pack texture
            glUniform1i(texLoc, 0); // Tell sampler uniform to use texture unit 0
        }
        else {
            if (texLoc == -1) std::cerr << "Warning: Uniform 'diffuseTexture' not found for pack shader." << std::endl;
            if (packTextureID == 0) std::cerr << "Warning: packTextureID is 0 when trying to render pack." << std::endl;
        }

        // Render the pack model
        packModel->draw();

        // Unbind texture (good practice after drawing with it)
        // Although Application::render will call glUseProgram(0) later,
        // unbinding texture here is cleaner.
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // --- Render Cards (using card/holo shaders) ---
    if (state == OPENED || state == OPENING) {
        // Optional: Hide or don't draw the pack model anymore once opening starts/finishes

        // Render all cards
        for (const auto& card : cards) {
            // Apply appropriate CARD/HOLO shader
            if (card.getRarity() == "holo" /* ... other conditions ... */) {
                textureManager->applyHoloShader(card, static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f);
            }
            else {
                textureManager->applyCardShader(card);
            }

            // Render the individual card, passing view and projection matrices separately
            card.render(viewMatrix, projectionMatrix); // *** MODIFIED CALL ***
        }
    }
}

void CardPack::update(float deltaTime) {
    // Update opening animation if in progress
    if (state == OPENING) {
        // Simple linear progression for now
        openingProgress += deltaTime * 0.5f; // Adjust speed as needed

        if (openingProgress >= 1.0f) {
            openingProgress = 1.0f;
            state = OPENED;
        }
    }

    // Update cards if pack is opened
    if (state == OPENED) {
        for (auto& card : cards) {
            card.update(deltaTime);
        }
    }
}

void CardPack::startOpeningAnimation() {
    if (state == CLOSED) {
        state = OPENING;
        openingProgress = 0.0f;
    }
}

bool CardPack::isOpeningComplete() const {
    return state == OPENED;
}

void CardPack::rotate(float x, float y, float z) {
    rotation.x += x;
    rotation.y += y;
    rotation.z += z;
}

void CardPack::setPosition(float x, float y, float z) {
    position = glm::vec3(x, y, z);
}

bool CardPack::isPointInside(float x, float y) const {
    // Basic implementation - can be enhanced with proper collision detection
    // Convert screen coordinates to world space and check if inside the pack's bounding box
    // This is a placeholder for now
    return false;
}