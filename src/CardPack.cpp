#include "CardPack.hpp"
#include "TextureManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <GL/freeglut_std.h>


CardPack::CardPack(TextureManager* texManager) :
    state(CLOSED), // Start closed
    textureManager(texManager),
    currentCardIndex(0),
    stackCenter(0.0f, 0.0f, 0.0f),   // Stack cards around the origin
    frontPosition(0.0f, 0.0f, 1.5f), // Position the front card closer to camera
    backPositionOffset(0.0f, 0.0f, -0.5f), // How far back the cycled card goes relative to stack end
    stackSpacing(0.05f),           // Very small Z spacing for the stack
    animationSpeed(8.0f),            // Speed of card movement (adjust as needed)
    isAnimating(false),              // Global pack animation flag (optional)
    position(glm::vec3(0.0f)),     // Initial pack model position
    rotation(glm::vec3(0.0f))      // Initial pack model rotation
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
    int numCards = 10; // Or get from database settings
    cards.reserve(numCards);

    glm::vec3 initialScale(0.8f, 0.8f, 0.8f); // Use consistent scale

    std::vector<std::string> typesToUse = {
       "Grass", "Fire", "Water", "Lightning", "Psychic",
       "Fighting", "Darkness", "Metal", "Fairy", "Dragon", "Colorless"
       // Using API names directly now
    };

    for (int i = 0; i < numCards; ++i) {
        // Determine rarity (example logic)
        std::string rarity = (i < 7) ? "normal" : (i < 9) ? "reverse" : "holo";
        std::string cardType = typesToUse[i % typesToUse.size()];
        std::string cardName = cardType + " Pokemon " + std::to_string(i + 1);
        // Get actual card data from database eventually
        cards.emplace_back(cardName, cardType, rarity, textureManager);

        Card& newCard = cards.back();

        // Calculate initial position in the stack
        float zPos = stackCenter.z - i * stackSpacing;
        glm::vec3 initialPos = glm::vec3(stackCenter.x, stackCenter.y, zPos);

        newCard.setPosition(initialPos);
        newCard.setRotation(glm::vec3(0.0f, glm::radians(180.0f), 0.0f)); // Start facing away? Or towards (0 rad)
        newCard.setScale(initialScale);

        // Set initial target to its starting position (no animation yet)
        newCard.setTargetTransform(initialPos, newCard.getRotation(), initialScale); // Use getRotation()
    }
    currentCardIndex = 0; // Reset index
    state =  CLOSED; // Ensure state is reset if regenerating
    std::cout << "Cards generated for stack. Vector size: " << cards.size() << std::endl;
}

// *** MODIFIED Signature ***
// CardPack.cpp (render function only)

// --- render ---
void CardPack::render(GLuint packShaderProgramID, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint packTextureID) {
    if (state == CLOSED) {
        // --- Render the Pack Model --- (Keep Existing Logic)
        if (packModel) {
            glUseProgram(packShaderProgramID);

            glm::mat4 packModelMatrix = glm::mat4(1.0f);
            packModelMatrix = glm::translate(packModelMatrix, position);
            packModelMatrix = glm::rotate(packModelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            packModelMatrix = glm::rotate(packModelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            packModelMatrix = glm::rotate(packModelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            // Add scaling if pack model needs it
           // packModelMatrix = glm::scale(packModelMatrix, glm::vec3(0.5f));

            GLint modelLoc = glGetUniformLocation(packShaderProgramID, "model");
            if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(packModelMatrix));
            // else std::cerr << "Warning: Uniform 'model' not found in pack shader." << std::endl;

            GLint texLoc = glGetUniformLocation(packShaderProgramID, "diffuseTexture");
            if (texLoc != -1 && packTextureID != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, packTextureID);
                glUniform1i(texLoc, 0);
            }
            // else { /* Warnings */ }

            packModel->draw();
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    else if (state == REVEALING) {
        // --- Render Cards ---
        //std::cout << "[DEBUG] CardPack::render() - textureManager ptr: " << textureManager
        //    << ", CardShaderID Check: " << textureManager->getCardShaderID() // Use getter
        //    << ", HoloShaderID Check: " << textureManager->getHoloShaderID() // Use getter
        //    << std::endl;
        float currentTime = static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f;
        for (const auto& card : cards) {
            // Apply appropriate shader (Holo or Card)
            // Ensure TextureManager has valid shaders!
            bool useHolo = (card.getRarity() == "holo" || card.getRarity() == "reverse" || card.getRarity() == "ex" || card.getRarity() == "full art");
            bool shaderApplied = false; // Flag to track if a shader was attempted
            if (useHolo) {
                // --- Use the getter in the check ---
                if (textureManager->getHoloShaderID() != 0) {
                    textureManager->applyHoloShader(card, currentTime);
                    shaderApplied = true; // Mark shader as applied
                }
                else {
                    std::cerr << "Error: Holo shader ID is 0 in CardPack::render. Skipping apply." << std::endl;
                    // continue; // REMOVE continue for now
                }
            }
            else {
                // --- Use the getter in the check ---
                if (textureManager->getCardShaderID() != 0) {
                    textureManager->applyCardShader(card);
                    shaderApplied = true; // Mark shader as applied
                }
                else {
                    std::cerr << "Error: Card shader ID is 0 in CardPack::render. Skipping apply." << std::endl;
                    // continue; // REMOVE continue for now
                }
            }

            // Render the card (Card::render uses its own position/rotation)
            //card.render(viewMatrix, projectionMatrix);
            if (shaderApplied) {
                card.render(viewMatrix, projectionMatrix);
            }
            else {
                std::cerr << "Skipping render for " << card.getPokemonName() << " due to invalid shader." << std::endl;
            }
        }
    }
    // Add rendering for FINISHED state if desired
}

void CardPack::update(float deltaTime) {
    // Update cards (handles their individual animations)
    bool anyCardMoving = false;
    if (state == REVEALING) {
        for (auto& card : cards) {
            card.update(deltaTime);
            if (card.isCardAnimating()) {
                anyCardMoving = true;
            }
        }
    }

    // Update global pack animation flag if needed
    if (isAnimating && !anyCardMoving) {
        isAnimating = false; // Last card finished moving
        // std::cout << "Pack animation finished." << std::endl;
    }

    // Update pack model rotation/position only if closed?
    // if (state == asdCLOSED) {
    //     // rotation.y += deltaTime * 0.1f; // Example idle rotation
    // }
}

// --- startOpeningAnimation ---
void CardPack::startOpeningAnimation() {
    if (state ==  CLOSED && !cards.empty()) {
        state =  REVEALING;
        currentCardIndex = 0;
        isAnimating = true; // Pack starts animating

        // Set the first card's target to the front position
        cards[currentCardIndex].setTargetTransform(frontPosition, glm::vec3(0.0f), cards[currentCardIndex].getScale());

        // Optional: Set targets for other cards to slightly fan out or just stay put initially
        for (size_t i = 1; i < cards.size(); ++i) {
            float zPos = stackCenter.z - i * stackSpacing;
            glm::vec3 stackPos = glm::vec3(stackCenter.x, stackCenter.y, zPos);
            cards[i].setTargetTransform(stackPos, cards[i].getRotation(), cards[i].getScale());
        }

        std::cout << "Pack opening, state = REVEALING. Card " << currentCardIndex << " moving to front." << std::endl;
    }
}

// --- cycleCard ---
void CardPack::cycleCard() {
    if (state !=  REVEALING || cards.empty()) {
        return; // Can only cycle in this state
    }

    // Optional: Prevent cycling if previous animation isn't finished
    if (isAnimating) {
        // Check if all cards have stopped animating
        bool stillMoving = false;
        for (const auto& card : cards) {
            if (card.isCardAnimating()) {
                stillMoving = true;
                break;
            }
        }
        if (stillMoving) return; // Wait for previous animation
        else isAnimating = false; // Reset pack animating flag
    }


    // --- Start new cycle animation ---
    isAnimating = true;

    // 1. Identify current front card
    int frontCardIdx = currentCardIndex;

    // 2. Identify the next card to bring to front
    int nextCardIdx = (currentCardIndex + 1) % cards.size();

    // 3. Calculate target position for the card moving to the back
    // Place it behind the last card's initial stack position
    float backZ = stackCenter.z - (cards.size()) * stackSpacing + backPositionOffset.z;
    glm::vec3 targetBackPos = glm::vec3(stackCenter.x + backPositionOffset.x, stackCenter.y + backPositionOffset.y, backZ);
    glm::vec3 targetBackRot = glm::vec3(0.0f, glm::radians(180.0f), 0.0f); // Rotate to face away

    // 4. Set targets
    cards[frontCardIdx].setTargetTransform(targetBackPos, targetBackRot, cards[frontCardIdx].getScale());
    cards[nextCardIdx].setTargetTransform(frontPosition, glm::vec3(0.0f), cards[nextCardIdx].getScale()); // Bring next card to front, facing camera

    // 5. Update index
    currentCardIndex = nextCardIdx;

    std::cout << "Cycling card. Card " << frontCardIdx << " moving to back. Card " << currentCardIndex << " moving to front." << std::endl;

    // Optional: Check if we've cycled all cards
    // if (currentCardIndex == 0 && frontCardIdx == cards.size() - 1) { // Just completed a full loop
    //     // state =  FINISHED;
    //     // std::cout << "All cards cycled." << std::endl;
    // }
}

bool CardPack::isCycleComplete() const {
    // Implement logic if needed, e.g., return true if state ==  FINISHED
    return state ==  FINISHED;
}

// --- Keep rotate/setPosition for pack ---
void CardPack::rotate(float x, float y, float z) {
    if (state ==  CLOSED) { // Only rotate pack model
        rotation.x += x;
        rotation.y += y;
        rotation.z += z;
    }
}

void CardPack::setPosition(float x, float y, float z) {
    if (state ==  CLOSED) { // Only position pack model
        position = glm::vec3(x, y, z);
    }
}

bool CardPack::isPointInside(float x, float y) const {
    // Basic implementation - can be enhanced with proper collision detection
    // Convert screen coordinates to world space and check if inside the pack's bounding box
    // This is a placeholder for now
    return false;
}