// InputHandler.cpp (New File)
#include "gl_core_3_3.h"
#include <SDL2/SDL.h>    // Needed for exit(), SDL_BUTTON_LEFT etc.
#include "InputHandler.hpp"
#include "Application.hpp"  // Needed for the global 'application'
#include "CardPack.hpp"     // Needed for packPtr methods
#include "TextureManager.hpp" // Needed for getApplicationTextureManager()->cycleShaderMode()
#include <iostream>
#include <cmath>

void InputHandler::handleKeyPress(unsigned char key, int x, int y) {
    if (!packPtr) return;

    switch (key) {
    case 27: // ESC key
        // Consider using glutLeaveMainLoop() or a flag instead of exit(0)
        // for cleaner shutdown in some cases.
        exit(0);
        break;
    case ' ': // Space bar
    {
        PackState currentState = packPtr->getState();
        std::cout << "Space pressed. Pack state: " << static_cast<int>(currentState) << std::endl;
        if (currentState == CLOSED) {
            packPtr->startOpeningAnimation();
        }
        else if (currentState == REVEALING) {
            packPtr->cycleCard();
        }
    }
    break; // Added missing break

    case 'n': // Lowercase n
    case 'N': // Uppercase N
        std::cout << "'N' key pressed. Requesting new pack generation." << std::endl;
        if (application) { // 'application' is visible here
            application->resetPack();
        }
        else {
            std::cerr << "Error: Global application instance not available." << std::endl;
        }
        break;
    case 't': // Lowercase t
    case 'T': // Uppercase T
        std::cout << "--- Toggling Holo Shader Debug Mode ---" << std::endl;
        if (application) {
            TextureManager* tm = application->getApplicationTextureManager();
            if (tm) {
                // Cycle through debug modes 0 (final output) through 9 (border raw color)
                static int holoDebugMode = 0;
                holoDebugMode = (holoDebugMode + 1) % 10; // Cycle 0-9 (10 modes total)
                tm->setHoloDebugMode(holoDebugMode); // Pass the mode index to TextureManager

                std::cout << "Setting Holo Debug Mode to: " << holoDebugMode << std::endl;
                std::cout << "  ---> Showing: ";

                if (holoDebugMode == 0) {
                    std::cout << "Final Composite Color" << std::endl;
                    std::cout << "     (Shows the final calculated pixel color after all effects and layers are combined)." << std::endl;
                }
                else if (holoDebugMode == 1) {
                    std::cout << "Combined Iridescence Component" << std::endl;
                    std::cout << "     (Isolates the rainbow color shift effect from both the main card area and the border, combined as they would be)." << std::endl;
                }
                else if (holoDebugMode == 2) {
                    std::cout << "Combined Specular/Gloss/Sheen Component" << std::endl;
                    std::cout << "     (Isolates the bright highlights: Reverse Specular, Glossy Specular, and Border Sheen, combined)." << std::endl;
                }
                else if (holoDebugMode == 3) {
                    std::cout << "Combined Fresnel + Border Base Component" << std::endl;
                    std::cout << "     (Isolates the edge glow (Fresnel) from the main area and the base metallic color of the border)." << std::endl;
                }
                else if (holoDebugMode == 4) {
                    std::cout << "Lit Base Texture Color Only" << std::endl;
                    std::cout << "     (Shows the base card image after lighting (diffuse+micro-specular) is applied, using parallax/normal mapping, but *before* any rarity-specific effects like overlays or holo layers are added)." << std::endl;
                }
                else if (holoDebugMode == 5) {
                    std::cout << "Raw Overlay Texture (L-System/Blank)" << std::endl;
                    std::cout << "     (Shows the raw RGB color and Alpha from the 'overlayTexture'. This is the L-System pattern for Normal/Reverse, or potentially blank for Glossy types)." << std::endl;
                }
                else if (holoDebugMode == 6) {
                    std::cout << "Final World Normal Vector (Base Lighting)" << std::endl;
                    std::cout << "     (Visualizes the world-space normal vector used for the *base* lighting, after normal mapping is applied. Colors represent vector components: R=X, G=Y, B=Z)." << std::endl;
                }
                else if (holoDebugMode == 7) {
                    std::cout << "Artwork Area Mask" << std::endl;
                    std::cout << "     (Shows white where the pixel is *inside* the defined artwork rectangle ('isInsideArtwork' is true), black otherwise. Used for Reverse Holo logic)." << std::endl;
                }
                else if (holoDebugMode == 8) {
                    std::cout << "Effective Border Mask (w/ Transparency)" << std::endl;
                    std::cout << "     (Visualizes the border mask strength *after* the 'borderTransparencyFactor' is applied. Shows where and how strongly the border color is blended)." << std::endl;
                }
                else if (holoDebugMode == 9) {
                    std::cout << "Raw Calculated Border Color (No Blend)" << std::endl;
                    std::cout << "     (Shows the fully calculated color of the border (base metal + sheen + iridescence) *before* it's blended with the rest of the card based on the mask and transparency)." << std::endl;
                }
                std::cout << "-----------------------------------------" << std::endl;

            }
            else {
                std::cerr << "Error: TextureManager instance not available via Application." << std::endl;
            }
        }
        else {
            std::cerr << "Error: Global application instance not available in InputHandler." << std::endl;
        }
        break;
    }

}

void InputHandler::handleMouseClick(int button, int state, int x, int y) {
    if (!packPtr) return;
    if (button == SDL_BUTTON_LEFT) {
        if (state == SDL_PRESSED) {
            mouseDown = true; // Always track mouse down state
            lastMouseX = x;
            lastMouseY = y;
        }
        else { // SDL_RELEASED
            mouseDown = false;
        }
    }
}

void InputHandler::handleMouseMotion(int x, int y) {
    if (!packPtr) return;
    if (mouseDown) {
        float deltaX = (float)(x - lastMouseX);
        float deltaY = (float)(y - lastMouseY);

        if (packPtr->getState() == CLOSED) {
            // --- Rotate Pack Model ---
            float rotY = deltaX * 0.008f; // Sensitivity for pack rotation
            float rotX = deltaY * 0.008f;
            packPtr->rotate(rotX, rotY, 0.0f); // Apply rotation to pack
        }
        else {
            // --- Rotate Camera (Orbit) ---
            float rotateSpeed = 0.005f; // Sensitivity for camera rotation
            float newAzimuth = application->getCameraAzimuth() + deltaX * rotateSpeed;
            float newElevation = application->getCameraElevation() + deltaY * rotateSpeed;

            application->setCameraAzimuth(newAzimuth);
            application->setCameraElevation(newElevation); // setCameraElevation clamps the value
        }

        lastMouseX = x;
        lastMouseY = y;
        //glutPostRedisplay(); // Request redraw after movement
    }
}

void InputHandler::handleMouseWheel(int wheel, int direction, int x, int y) {
    if (!application || !packPtr) return;

    // Only allow zooming when cards are revealed
    //if (packPtr->getState() != CLOSED) {
        float currentRadius = application->getCameraRadius();
        float zoomSpeed = 0.5f; // Adjust sensitivity

        if (direction > 0) { // Wheel scrolled up/forward -> Zoom in
            application->setCameraRadius(currentRadius - zoomSpeed);
            // std::cout << "Zoom In (Wheel). New Radius: " << application->getCameraRadius() << std::endl;
        }
        else if (direction < 0) { // Wheel scrolled down/backward -> Zoom out
            application->setCameraRadius(currentRadius + zoomSpeed);
            // std::cout << "Zoom Out (Wheel). New Radius: " << application->getCameraRadius() << std::endl;
        }
        // Redraw will happen automatically in the next frame
    //}
}

void InputHandler::handleSpecialKeyPress(int key, int x, int y) {
    if (!application) return; // Need access to global application
    TextureManager* tm = application->getApplicationTextureManager();
    if (!tm) return;

    switch (key) {
    case SDLK_UP:
        std::cout << "Up Arrow Pressed - Increasing L-System Variation" << std::endl;
        tm->incrementLSystemVariationLevel(); // Implement this in TextureManager
        application->regenerateCurrentCardOverlay(); // Trigger regeneration
        break;
    case SDLK_DOWN:
        std::cout << "Down Arrow Pressed - Decreasing L-System Variation" << std::endl;
        tm->decrementLSystemVariationLevel(); // Implement this in TextureManager
        application->regenerateCurrentCardOverlay(); // Trigger regeneration
        break;
        // Handle other special keys if needed (F1, PageUp, etc.)
    case SDLK_LEFT:
        std::cout << "Left Arrow Pressed - Decreasing Shift" << std::endl;
        // tm->setTestShift(currentShift - 0.05f); // Need getter/setter or modify directly
        // Let's modify directly for now (requires making testHorizontalShift public or having a modify function)
        // OR pass the amount to change:
        tm->setTestShift(tm->getTestShift() - 0.05f); // Need getter getTestShift() in TM
        break;
    case SDLK_RIGHT:
        std::cout << "Right Arrow Pressed - Increasing Shift" << std::endl;
        tm->setTestShift(tm->getTestShift() + 0.05f); // Need getter getTestShift() in TM
        break;
        // ... (handle Up/Down for L-System as before, or disable temporarily) ...
    default:
        break; // Ignore other special keys
    }
}