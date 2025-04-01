#include "CardPack.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include <filesystem> // Add this include at the top of the file
#include <glm/gtc/type_ptr.hpp>

CardPack::CardPack() :
    state(CLOSED),
    openingProgress(0.0f),
    position(glm::vec3(0.0f, 0.0f, 0.0f)),
    rotation(glm::vec3(0.0f, 0.0f, 0.0f))
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

void CardPack::generateCards(CardDatabase& database) {
    // We'll implement this later when the CardDatabase is implemented
    cards.clear();

    // For now, just create empty cards for testing
    for (int i = 0; i < 10; i++) {
        cards.push_back(Card("Test Pokemon", "normal", (i < 7) ? "normal" :
            (i < 9) ? "reverse" : "holo"));
    }
}

void CardPack::render(GLuint shaderProgramID) {
    if (!packModel) return;

    // Create model matrix for the pack
    glm::mat4 modelMatrix = glm::mat4(1.0f);

    // Apply position
    modelMatrix = glm::translate(modelMatrix, position);

    // Apply rotation (in radians)
    modelMatrix = glm::rotate(modelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    modelMatrix = glm::rotate(modelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));

    // *** Get Uniform Location and Set MODEL Matrix ***
    GLint modelLoc = glGetUniformLocation(shaderProgramID, "model");
    if (modelLoc != -1) {
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    }
    else {
        // This warning should NOT appear if pack_v.glsl is compiled correctly
        std::cerr << "Warning: Uniform 'model' not found in shader program " << shaderProgramID << std::endl;
    }

    // Set other uniforms if needed (e.g., shininess for pack_f.glsl)
    GLint shinyLoc = glGetUniformLocation(shaderProgramID, "shininess");
    if (shinyLoc != -1) {
        glUniform1f(shinyLoc, 32.0f); // Example shininess value
    }
    else {
        // std::cerr << "Warning: Uniform 'shininess' not found." << std::endl;
    }
    // Set light uniforms (example fixed values)
   // GLint lightPosLoc = glGetUniformLocation(shaderProgramID, "lightPos");
   // if (lightPosLoc != -1) glUniform3f(lightPosLoc, 1.0f, 1.0f, 2.0f);
   // GLint lightColorLoc = glGetUniformLocation(shaderProgramID, "lightColor");
   // if (lightColorLoc != -1) glUniform3f(lightColorLoc, 1.0f, 1.0f, 1.0f);


   // Render the pack model
    packModel->draw();

    // If the pack is opened or opening and progress is sufficient, render cards
    if (state == OPENED || (state == OPENING /* && openingProgress > 0.5f */)) { // Render cards sooner?
        // You'll need a loop here to render each card
        // Each card might have its own model matrix
        for (const auto& card : cards) {
            // Calculate card's model matrix (similar to pack, using card.getPosition(), etc.)
            glm::mat4 cardModelMatrix = glm::translate(glm::mat4(1.0f), card.getPosition());
            // Add card rotation/scale...

            // Set the model uniform *again* for this card
            if (modelLoc != -1) {
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(cardModelMatrix));
            }

            // *** Bind appropriate texture for the card ***
            // GLuint cardTex = textureManager->getTexture(card.getPokemonName()); // Or however you get it
            // glActiveTexture(GL_TEXTURE0);
            // glBindTexture(GL_TEXTURE_2D, cardTex);
            // glUniform1i(texLoc, 0); // Make sure sampler uses unit 0

            // Set other card-specific uniforms (e.g., holo intensity?)

            // *** Draw the card geometry ***
            // packModel->draw(); // Placeholder - you need card geometry
        }
    }

    // Calculate final transformation matrix
    //glm::mat4 mvpMatrix = viewProjection * modelMatrix;

    //// If the pack is opening, apply transformations for the animation
    //if (state == OPENING) {
    //    // Simple animation for now - we'll enhance this later
    //    // This will be replaced with proper vertex manipulation for tearing
    //}

    //GLint mvpLoc = glGetUniformLocation(shaderProgramID, "mvp");
    //if (mvpLoc == -1) {
    //    std::cerr << "Warning: Uniform 'mvp' not found in shader program " << shaderProgramID << std::endl;
    //}
    //else {
    //    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvpMatrix));
    //}

    //packModel->draw();

    //// If the pack is opened or opening and progress is sufficient, render cards
    //if (state == OPENED || (state == OPENING && openingProgress > 0.5f)) {
    //    // We'll implement card rendering later
    //}
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