#include "CardPack.hpp"
#include "TextureManager.hpp" // Include TextureManager
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <filesystem>
#include <glm/gtc/type_ptr.hpp>
#include <GL/freeglut_std.h>
#include <random> // For rarity selection
#include <vector> // For type list

CardPack::CardPack(TextureManager* texManager) :
    state(CLOSED), // Start closed
    textureManager(texManager), // Ensure this is initialized
    currentCardIndex(0),
    stackCenter(0.0f, 0.0f, 0.0f),
    frontPosition(0.0f, 0.0f, 1.5f), // Adjust Z based on camera distance
    backPositionOffset(0.0f, 0.0f, -0.5f), // How far back the cycled card goes relative to stack end
    stackSpacing(0.015f), // Make cards closer in the stack initially
    animationSpeed(8.0f),
    isAnimating(false),
    position(glm::vec3(0.0f)),
    rotation(glm::vec3(0.0f))
{
    if (!textureManager) {
         throw std::runtime_error("CardPack created with null TextureManager!");
    }
    std::cout << "CardPack constructor: Attempting to load pack model..." << std::endl;
    std::string packModelPath = "models/pack.obj";
    std::string fallbackModelPath = "models/cube.obj"; // Fallback if pack fails

    // Try loading the primary pack model first
     if (std::filesystem::exists(packModelPath)) {
        try {
            packModel = std::make_unique<Mesh>(packModelPath);
            std::cout << "Loaded primary pack model: " << packModelPath << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to load primary pack model '" << packModelPath << "': " << e.what() << std::endl;
            packModel = nullptr; // Ensure it's null if primary failed
        }
    } else {
        std::cerr << "Primary pack model not found at: " << packModelPath << std::endl;
         std::cerr << "Searched relative to working directory: " << std::filesystem::current_path() << std::endl;
    }

    // If primary model failed or wasn't found, try the fallback
    if (!packModel) {
        std::cerr << "Attempting to load fallback model: " << fallbackModelPath << std::endl;
        if (std::filesystem::exists(fallbackModelPath)) {
            try {
                packModel = std::make_unique<Mesh>(fallbackModelPath);
                std::cout << "Loaded fallback model: " << fallbackModelPath << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Failed to load fallback model '" << fallbackModelPath << "': " << e.what() << std::endl;
                packModel = nullptr; // Explicitly null
                throw std::runtime_error("Failed to load any pack model (pack.obj or cube.obj).");
            }
        } else {
            std::cerr << "Fallback model also not found at: " << fallbackModelPath << std::endl;
            std::cerr << "Searched relative to working directory: " << std::filesystem::current_path() << std::endl;
            throw std::runtime_error("Cannot find any model (pack.obj or cube.obj)");
        }
    }
     std::cout << "[DEBUG] CardPack Constructor - textureManager ptr: " << textureManager << std::endl;
}

CardPack::~CardPack() {
    // unique_ptr handles mesh cleanup
}

void CardPack::generateCards(CardDatabase& database) { // database might not be needed now
    cards.clear();
    int numCards = 10; // Standard pack size
    cards.reserve(numCards);

    std::cout << "Generating " << numCards << " cards for the pack..." << std::endl;

    // Define possible Pokemon types (use API names where they differ)
    std::vector<std::string> typesToUse = {
       "Grass", "Fire", "Water", "Lightning", "Psychic",
       "Fighting", "Darkness", "Metal", "Dragon", "Colorless"
       // Add "Fairy" if supporting older sets or if desired
    };

    // Random number generation setup
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> typeDist(0, typesToUse.size() - 1);
    std::uniform_real_distribution<> rareDist(0.0, 1.0); // For rare slot probability

    // --- Pack Structure Logic ---
    // Example: 7 Common/Uncommon, 2 Reverse Holo, 1 Rare Slot
    const int numNormal = 7;
    const int numReverse = 2;
    // const int numRare = 1; // Implicitly the last card

    for (int i = 0; i < numCards; ++i) {
        std::string rarity = "normal"; // Default
        std::string cardType = typesToUse[typeDist(gen)]; // Random type for each card

        // --- Determine Rarity Based on Card Index ---
        if (i < numNormal) {
            rarity = "normal"; // These will query Common or Uncommon
        } else if (i < numNormal + numReverse) {
            rarity = "reverse"; // These query C/U but apply reverse holo shader
        } else {
            // This is the "Rare Slot" (last card, index 9)
            float rareRoll = rareDist(gen);
            // Define probabilities for the rare slot (adjust as desired)
            // Example: 60% Holo, 25% EX/V etc., 15% Full Art/Secret
            if (rareRoll < 0.60f) {
                rarity = "holo"; // Standard Rare Holo
            } else if (rareRoll < 0.85f) { // 0.60 + 0.25
                rarity = "ex";     // Represents EX, V, GX, modern ex etc.
            } else { // Remaining 15%
                rarity = "full art"; // Represents Full Art, Secret, Illustration etc.
            }
            std::cout << "Rare slot (Card " << i << ") rolled " << rareRoll << ", assigned rarity: " << rarity << std::endl;
        }

        // Create a placeholder name - API will give the real one, but useful for logs
        std::string cardName = cardType + " Pokemon (" + rarity + ")";

        // --- Create Card Instance ---
        // IMPORTANT: Pass the textureManager pointer!
        cards.emplace_back(cardName, cardType, rarity, textureManager);
        Card& newCard = cards.back(); // Get reference to the newly added card

        // --- Generate and Assign Texture using TextureManager ---
        // This now handles API fetch, download, loading, caching, and fallbacks
        GLuint textureId = textureManager->generateCardTexture(newCard);
        newCard.setTextureID(textureId); // Assign the resulting texture ID to the card

        if (textureId == 0) {
             std::cerr << "WARNING: Failed to obtain a valid texture for card " << i << " (" << cardName << "). It may render incorrectly." << std::endl;
             // The card will likely render black or using whatever texture 0 represents
        }

        GLuint overlayTextureId = textureManager->generateProceduralOverlayTexture(newCard);
        newCard.setOverlayTextureID(overlayTextureId); // Assign OVERLAY texture ID

        if (overlayTextureId == 0) {
            std::cerr << "WARNING: Failed to obtain a valid OVERLAY texture for card " << i << " (" << cardName << ")." << std::endl;
            // Card will render without overlay if this fails
        }

        // --- Set Initial Transform (in the stack, facing away) ---
        float zPos = stackCenter.z - i * stackSpacing;
        glm::vec3 initialPos = glm::vec3(stackCenter.x, stackCenter.y, zPos);
        glm::vec3 initialRot = glm::vec3(0.0f, glm::radians(180.0f), 0.0f); // Start facing away
        glm::vec3 initialScale(0.8f, 0.8f, 0.8f); // Example scale

        newCard.setPosition(initialPos);
        newCard.setRotation(initialRot);
        newCard.setScale(initialScale);

        // Set initial target to its starting position (no animation until pack opens)
        newCard.setTargetTransform(initialPos, initialRot, initialScale);

        std::cout << "Generated Card " << i << ": Type=" << cardType << ", Rarity=" << rarity << ", Initial Z=" << zPos << ", TexID=" << textureId << ", OverlayTexID=" << overlayTextureId << std::endl;
    }

    currentCardIndex = 0; // Reset index for cycling
    state = CLOSED;      // Ensure state is reset
    std::cout << "Finished generating " << cards.size() << " cards." << std::endl;
}

void CardPack::render(GLuint packShaderProgramID, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, GLuint packTextureID, const glm::vec3& cameraPos) {
    if (state == CLOSED) {
        // --- Render the Pack Model ---
        if (packModel) {
            glUseProgram(packShaderProgramID); // Use the shader passed for the pack model

            // Setup pack model matrix (translate, rotate, scale)
            glm::mat4 packModelMatrix = glm::mat4(1.0f);
            packModelMatrix = glm::translate(packModelMatrix, position);
            packModelMatrix = glm::rotate(packModelMatrix, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            packModelMatrix = glm::rotate(packModelMatrix, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            packModelMatrix = glm::rotate(packModelMatrix, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
             //packModelMatrix = glm::scale(packModelMatrix, glm::vec3(0.5f)); // Optional scaling

            // Set model matrix uniform
            GLint modelLoc = glGetUniformLocation(packShaderProgramID, "model");
            if (modelLoc != -1) {
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(packModelMatrix));
            } else {
                 // Only warn once?
                 // std::cerr << "Warning: Uniform 'model' not found in pack shader." << std::endl;
            }
            // Pass View and Projection matrices (assuming they are uniforms in pack shader too)
            GLint viewLoc = glGetUniformLocation(packShaderProgramID, "view");
            GLint projLoc = glGetUniformLocation(packShaderProgramID, "projection");
             if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(viewMatrix));
             if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

            // Set texture uniform
            GLint texLoc = glGetUniformLocation(packShaderProgramID, "diffuseTexture");
            if (texLoc != -1) {
                if (packTextureID != 0) {
                    glActiveTexture(GL_TEXTURE0); // Activate texture unit 0
                    glBindTexture(GL_TEXTURE_2D, packTextureID);
                    glUniform1i(texLoc, 0); // Tell shader to use texture unit 0
                } else {
                    glBindTexture(GL_TEXTURE_2D, 0); // Unbind if texture ID is invalid
                    // Optionally set a default color if texture is missing
                }
            } else {
                 // Only warn once?
                 // std::cerr << "Warning: Uniform 'diffuseTexture' not found in pack shader." << std::endl;
            }

            packModel->draw(); // Draw the pack model
            glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture unit 0
        } else {
            std::cerr << "Error: CardPack::render called in CLOSED state but packModel is null!" << std::endl;
        }
    }
    else if (state == REVEALING) {
        // --- Render Cards ---
        // Ensure textureManager is valid before proceeding
        if (!textureManager) {
            std::cerr << "Error: Cannot render cards, textureManager is null in CardPack." << std::endl;
            return;
        }

        // Get current time for holo shader animation
        // Ensure glut is initialized before calling this!
        float currentTime = 0.0f;
        // Just get elapsed time directly - if GLUT isn't initialized it will return 0
        currentTime = static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f;

        // Iterate through cards and render them
        for (const auto& card : cards) {
            // Determine if holo shader should be used based on rarity
            bool useHolo = (card.getRarity() == "holo" || card.getRarity() == "reverse" || card.getRarity() == "ex" || card.getRarity() == "full art");
            bool shaderApplied = false; // Flag to track if a valid shader was applied

            if (useHolo) {
                // Check if the holo shader ID is valid BEFORE applying
                if (textureManager->getHoloShaderID() != 0) {
                    textureManager->applyHoloShader(card, currentTime);
                    shaderApplied = true;
                } else {
                    // Log error if holo shader is needed but invalid
                    static bool holoErrorLogged = false;
                    if (!holoErrorLogged) {
                        std::cerr << "Error: Holo shader ID is 0 in CardPack::render. Holo cards may not render effects." << std::endl;
                        holoErrorLogged = true;
                    }
                    // Fallback: Try applying the regular card shader? Or render plain?
                    // For now, we just won't apply a *valid* shader if holo is missing.
                     if (textureManager->getCardShaderID() != 0) {
                          textureManager->applyCardShader(card); // Use card shader as fallback
                          shaderApplied = true; // Mark that *a* shader was applied
                     }
                }
            } else {
                // Use the regular card shader
                 // Check if the card shader ID is valid BEFORE applying
                if (textureManager->getCardShaderID() != 0) {
                    textureManager->applyCardShader(card);
                    shaderApplied = true;
                } else {
                    // Log error if card shader is needed but invalid
                    static bool cardErrorLogged = false;
                     if (!cardErrorLogged) {
                         std::cerr << "Error: Card shader ID is 0 in CardPack::render. Non-holo cards may not render correctly." << std::endl;
                         cardErrorLogged = true;
                     }
                    // No valid shader available for this card
                }
            }

            // Only render the card if a shader could be successfully applied
            // and the card itself has a valid texture ID
            if (shaderApplied && card.getTextureID() != 0) { // Added check for card's texture ID
                // The Card::render function needs the view and projection matrices
                // It will handle its own model matrix based on its position/rotation/scale
                card.render(viewMatrix, projectionMatrix, cameraPos);
            } else {
                if (!shaderApplied) {
                    // std::cerr << "Skipping render for " << card.getPokemonName() << " due to invalid or missing shader." << std::endl;
                }
                if (card.getTextureID() == 0) {
                     // std::cerr << "Skipping render for " << card.getPokemonName() << " due to invalid texture ID (0)." << std::endl;
                }
            }
        }
         // After rendering all cards, maybe unbind the last used shader? Optional.
         // glUseProgram(0);
    }
    // Add rendering for FINISHED state if different from REVEALING
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

}

void CardPack::startOpeningAnimation() {
    if (state ==  CLOSED && !cards.empty()) {
        state =  REVEALING;
        currentCardIndex = 0;
        isAnimating = true; // Pack starts animating

        // --- Set initial target positions for the reveal ---
        // Move the first card (index 0) to the front position
        cards[currentCardIndex].setTargetTransform(frontPosition, // Target Position
                                                   glm::vec3(0.0f), // Target Rotation (face camera)
                                                   cards[currentCardIndex].getScale()); // Keep original scale

        // Move all other cards from their initial stack position slightly back
        // to create the initial fan/stack effect for revealing.
        for (size_t i = 1; i < cards.size(); ++i) {
            // Calculate target position in the visible stack (behind the front card)
            float zPos = stackCenter.z - i * stackSpacing; // Use the original stack spacing for target
            glm::vec3 targetPos = glm::vec3(stackCenter.x, stackCenter.y, zPos);

            // Keep them facing away initially until cycled? Or turn them now? Let's keep facing away.
            glm::vec3 targetRot = cards[i].getRotation(); // Keep initial rotation (facing away)
            glm::vec3 targetScale = cards[i].getScale();  // Keep initial scale

            cards[i].setTargetTransform(targetPos, targetRot, targetScale);
        }

        std::cout << "Pack opening animation started. State = REVEALING. Card " << currentCardIndex << " moving to front." << std::endl;
    } else if (state == CLOSED && cards.empty()) {
         std::cerr << "Cannot start opening animation: No cards generated." << std::endl;
    } else {
         std::cout << "Pack already opening or opened." << std::endl;
    }
}

void CardPack::cycleCard() {
     // Can only cycle if in REVEALING state and not currently mid-animation
    if (state != REVEALING || cards.empty()) {
        std::cerr << "Cannot cycle card. State: " << state << ", Card count: " << cards.size() << std::endl;
        return;
    }

    // Prevent cycling if the pack is still animating cards into position
    if (isAnimating) {
        bool stillMoving = false;
        for (const auto& card : cards) {
            if (card.isCardAnimating()) {
                stillMoving = true;
                break;
            }
        }
        if (stillMoving) {
            std::cout << "Cannot cycle card yet, animation in progress." << std::endl;
            return; // Wait for the current animation to finish
        }
         else {
            isAnimating = false; // Reset flag if movement just finished
        }
    }

    // --- Start cycle animation ---
    isAnimating = true; // Mark that a new animation is starting

    // 1. Identify current front card index
    int frontCardIdx = currentCardIndex;

    // 2. Identify the next card index to bring to the front
    int nextCardIdx = (currentCardIndex + 1) % cards.size();

    // 3. Calculate target position for the card moving to the back
    // Place it behind the *last* card's initial stack position + offset
    // We need to find where the "end" of the stack is now dynamically based on card positions?
    // Or simpler: just place it relative to the original stack center but far back.
    float backZ = stackCenter.z - (cards.size()) * stackSpacing + backPositionOffset.z; // Z behind original stack
    glm::vec3 targetBackPos = glm::vec3(stackCenter.x + backPositionOffset.x,
                                      stackCenter.y + backPositionOffset.y,
                                      backZ);
    glm::vec3 targetBackRot = glm::vec3(0.0f, glm::radians(180.0f), 0.0f); // Rotate to face away at the back

    // 4. Set targets
    // Move current front card to the calculated back position
    cards[frontCardIdx].setTargetTransform(targetBackPos, targetBackRot, cards[frontCardIdx].getScale());
    // Move the next card to the front position, facing the camera
    cards[nextCardIdx].setTargetTransform(frontPosition, glm::vec3(0.0f), cards[nextCardIdx].getScale());

    // 5. Update the current card index
    currentCardIndex = nextCardIdx;

    std::cout << "Cycling card. Card " << frontCardIdx << " moving to back. Card " << currentCardIndex << " moving to front." << std::endl;

    // Optional: Transition to FINISHED state after a full cycle?
    // if (currentCardIndex == 0) { // Just completed a full loop
    //     // state = FINISHED;
    //     // std::cout << "All cards cycled." << std::endl;
    // }
}

bool CardPack::isCycleComplete() const {
    return state == FINISHED;
}

void CardPack::rotate(float x, float y, float z) {
    if (state == CLOSED) { // Only rotate pack model when closed
        rotation.x += x;
        rotation.y += y;
        rotation.z += z;
    } else {
        // Maybe rotate the whole card stack? More complex.
    }
}

void CardPack::setPosition(float x, float y, float z) {
    if (state == CLOSED) { // Only position pack model when closed
        position = glm::vec3(x, y, z);
    } else {
         // Maybe move the whole card stack? Requires adjusting stackCenter etc.
    }
}

bool CardPack::isPointInside(float x, float y) const {
    // Placeholder - requires proper ray casting or BBox check in world space
    return false;
}