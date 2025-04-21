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
    frontPosition(0.0f),
    backPositionOffset(0.0f, 0.0f, -1.0f), // How far back the cycled card goes relative to stack end
    stackSpacing(0.025f), // Make cards closer in the stack initially
    animationSpeed(8.0f),
    isAnimating(false),
    position(glm::vec3(0.0f)),
    rotation(glm::vec3(0.0f)),
    cardMovingToBackIndex(-1)
{
    if (!textureManager) {
         throw std::runtime_error("CardPack created with null TextureManager!");
    }

    frontPosition = glm::vec3(0.0f, 0.0f, stackCenter.z + stackSpacing * 3.0f);

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

        glDisable(GL_CULL_FACE);

        // Get current time for holo shader animation
        // Ensure glut is initialized before calling this!
        float currentTime = 0.0f;
        // Just get elapsed time directly - if GLUT isn't initialized it will return 0
        currentTime = static_cast<float>(glutGet(GLUT_ELAPSED_TIME)) / 1000.0f;

        // Iterate through cards and render them
        for (size_t i = 0; i < cards.size(); ++i) {
            const auto& card = cards[i];
            bool isFrontCard = (i == currentCardIndex) && (state == REVEALING);
            // Determine if holo shader should be used based on rarity
            bool useHolo = (card.getRarity() == "holo" || card.getRarity() == "reverse" || card.getRarity() == "ex" || card.getRarity() == "full art");
            bool shaderApplied = false; // Flag to track if a valid shader was applied

            // <<< ALWAYS Use Holo Shader Pipeline for All Cards >>>
            if (textureManager->getHoloShaderID() != 0) {
                // ApplyHoloShader sets the uniforms based on rarity internally now (implicitly)
                // It also calls glUseProgram(holoShaderID)
                textureManager->applyHoloShader(card, currentTime);
                shaderApplied = true;
            }
            else {
                // Log error if holo shader is invalid
                static bool holoErrorLogged = false;
                if (!holoErrorLogged) {
                    std::cerr << "Error: Holo shader ID is 0 in CardPack::render. Cards cannot be rendered correctly." << std::endl;
                    holoErrorLogged = true;
                }
                // Maybe break or return if shader is essential and missing?
            }

            // Only render the card if a shader could be successfully applied
            // and the card itself has a valid texture ID
            if (shaderApplied && card.getTextureID() != 0) { // Added check for card's texture ID
                // The Card::render function needs the view and projection matrices
                // It will handle its own model matrix based on its position/rotation/scale
                card.render(viewMatrix, projectionMatrix, cameraPos, isFrontCard);
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
         glUseProgram(0);
         glEnable(GL_CULL_FACE);
    }
    // Add rendering for FINISHED state if different from REVEALING
}

// --- UPDATE Method ---
void CardPack::update(float deltaTime) {
    bool anyCardMoving = false;
    if (state == REVEALING) {
        // Iterate WITH index now
        for (size_t i = 0; i < cards.size(); ++i) {
            auto& card = cards[i]; // Get reference to the card

            // Store previous animation state
            bool wasAnimating = card.isCardAnimating();

            // Update the card's position/interpolation
            card.update(deltaTime);

            // Check if this card is the one doing the special move-to-back
            if (cardMovingToBackIndex != -1 && i == cardMovingToBackIndex) {
                // Check if it *was* animating but *just stopped* (meaning it reached the intermediate point)
                if (wasAnimating && !card.isCardAnimating()) {

                    // --- Start Stage 2: Set final target ---
                    int lastStackIndex = cards.size() - 1;
                    float targetLastStackZ = stackCenter.z - lastStackIndex * stackSpacing;
                    glm::vec3 targetLastStackPos = glm::vec3(stackCenter.x, stackCenter.y, targetLastStackZ);
                    glm::vec3 targetLastStackRot = glm::vec3(0.0f); // Face camera at the end

                    card.setTargetTransform(targetLastStackPos, targetLastStackRot, card.getScale());
                    // --- End Stage 2 setup ---

                    // Reset the tracking index, this card is now doing a normal animation
                    cardMovingToBackIndex = -1;
                }
            }

            // Track if *any* card is still moving (for the main isAnimating flag)
            if (card.isCardAnimating()) {
                anyCardMoving = true;
            }
        } // End card loop
    } // End if (state == REVEALING)

    // Update global pack animation flag
    // This flag now correctly reflects if *any* card animation (including Stage 2) is ongoing
    if (isAnimating && !anyCardMoving) {
        isAnimating = false; // All individual card animations finished
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

            // --- SET TARGET ROTATION TO 0 ---
            glm::vec3 targetRot = glm::vec3(0.0f); // Face the camera

            // Keep them facing away initially until cycled? Or turn them now? Let's keep facing away.
            //glm::vec3 targetRot = cards[i].getRotation(); // Keep initial rotation (facing away)
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
        bool checkIndividualCards = false;
        if (cardMovingToBackIndex != -1) {
            // If we know a card is doing the special move, check specifically if IT is still moving
            if (cards[cardMovingToBackIndex].isCardAnimating()) {
                std::cout << "Cannot cycle card yet, card " << cardMovingToBackIndex << " moving to side." << std::endl;
                return;
            }
            else {
                // It finished stage 1, update() should handle stage 2 trigger.
                // But we still need to check other cards below.
                checkIndividualCards = true;
            }
        }
        else {
            // No special move active, check all cards generally
            checkIndividualCards = true;
        }

        if (checkIndividualCards) {
            bool stillMoving = false;
            for (const auto& card : cards) {
                // Skip the card we might have already checked if it was the special one
                // and skip the one currently at the front (which shouldn't be moving)
                if (&card == &cards[currentCardIndex] || (cardMovingToBackIndex != -1 && &card == &cards[cardMovingToBackIndex])) continue;

                if (card.isCardAnimating()) {
                    stillMoving = true;
                    break;
                }
            }
            if (stillMoving) {
                std::cout << "Cannot cycle card yet, stack animation in progress." << std::endl;
                return; // Wait for the current animation to finish
            }
            else {
                // If we get here, means no cards are animating according to their own state
                isAnimating = false; // Force reset the pack flag if individual cards disagree
            }
        }
    }

    // --- Start cycle animation ---
    isAnimating = true; // Mark that a new animation is starting
    cardMovingToBackIndex = -1;

    int prevFrontCardIdx = currentCardIndex;
    int newFrontCardIdx = (currentCardIndex + 1) % cards.size();

    // *** CHECK AND REGENERATE OVERLAY FOR THE *NEW* FRONT CARD ***
    Card& nextCard = cards[newFrontCardIdx];
    int currentVariationLevel = textureManager->getLSystemVariationLevel();

    if (nextCard.getGeneratedOverlayLevel() != currentVariationLevel) {
        std::cout << "[Cycle Card] Overlay level mismatch for next card " << newFrontCardIdx
            << " (Card: " << nextCard.getGeneratedOverlayLevel()
            << ", Manager: " << currentVariationLevel << "). Regenerating..." << std::endl;

        // Regenerate the overlay. This call will also update nextCard's internal level.
        GLuint newOverlayID = textureManager->generateProceduralOverlayTexture(nextCard);

        // Update the texture ID stored in the card object
        nextCard.setOverlayTextureID(newOverlayID);
        std::cout << "[Cycle Card] Regenerated Overlay ID: " << newOverlayID << std::endl;
    }
    // *** END OVERLAY CHECK/REGEN ***

    // --- 1. Start Stage 1 for the Card Moving FROM Front ---
    // Calculate the INTERMEDIATE position (offset to the side)
    // TUNABLE PARAMETERS: Adjust x, y, z offsets for desired path
    float sideOffsetX = 1.8f;   // How far sideways
    float sideOffsetY = 0.1f;   // Slight lift?
    float sideOffsetZ = -0.4f;  // Move back slightly, but less than stack width
    glm::vec3 intermediateSidePos = frontPosition + glm::vec3(sideOffsetX, sideOffsetY, sideOffsetZ);
    // Keep rotation facing camera during side move (or adjust if desired)
    glm::vec3 intermediateSideRot = glm::vec3(0.0f);

    // Set the target to the intermediate position
    cards[prevFrontCardIdx].setTargetTransform(intermediateSidePos, intermediateSideRot, cards[prevFrontCardIdx].getScale());

    // Mark this card as the one doing the special move
    cardMovingToBackIndex = prevFrontCardIdx;
    // --- End Stage 1 Setup ---

    // 2. Move the NEW front card to the front position - REMAINS SAME
    cards[newFrontCardIdx].setTargetTransform(frontPosition, glm::vec3(0.0f), cards[newFrontCardIdx].getScale());

    // 3. Update ALL OTHER cards in the visible stack - REMAINS SAME
    // (This loop shifts the existing stack forward)
    for (size_t i = 0; i < cards.size(); ++i) {
        if (i == prevFrontCardIdx || i == newFrontCardIdx) continue; // Skip the two active cards
        int stackIndex = (i - newFrontCardIdx + cards.size()) % cards.size();
        float targetZ = stackCenter.z - stackIndex * stackSpacing;
        glm::vec3 targetPos = glm::vec3(stackCenter.x, stackCenter.y, targetZ);
        glm::vec3 targetRot = glm::vec3(0.0f);
        glm::vec3 targetScale = cards[i].getScale();
        cards[i].setTargetTransform(targetPos, targetRot, targetScale);
    }

    // 4. Update the current front index - REMAINS SAME
    currentCardIndex = newFrontCardIdx;

    std::cout << "Cycling card. Prev front " << prevFrontCardIdx << " -> starting move to side. New front " << currentCardIndex << " -> front." << std::endl;
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