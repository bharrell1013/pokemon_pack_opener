// InputHandler.cpp (New File)
#include "gl_core_3_3.h"
#include <GL/freeglut.h>    // Needed for exit(), GLUT_LEFT_BUTTON etc.
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
        std::cout << "'T' key pressed. Toggling shader mode." << std::endl;
        if (application) {
            TextureManager* tm = application->getApplicationTextureManager();
            if (tm) {
                tm->cycleShaderMode();
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
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            mouseDown = true; // Always track mouse down state
            lastMouseX = x;
            lastMouseY = y;
        }
        else { // GLUT_UP
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
        glutPostRedisplay(); // Request redraw after movement
    }
}

void InputHandler::handleMouseWheel(int wheel, int direction, int x, int y) {
    if (!application || !packPtr) return;

    // Only allow zooming when cards are revealed
    if (packPtr->getState() != CLOSED) {
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
        glutPostRedisplay(); // Request redraw after zoom
    }
}

void InputHandler::handleSpecialKeyPress(int key, int x, int y) {
    if (!application) return; // Need access to global application
    TextureManager* tm = application->getApplicationTextureManager();
    if (!tm) return;

    switch (key) {
    case GLUT_KEY_UP:
        std::cout << "Up Arrow Pressed - Increasing L-System Variation" << std::endl;
        tm->incrementLSystemVariationLevel(); // Implement this in TextureManager
        application->regenerateCurrentCardOverlay(); // Trigger regeneration
        break;
    case GLUT_KEY_DOWN:
        std::cout << "Down Arrow Pressed - Decreasing L-System Variation" << std::endl;
        tm->decrementLSystemVariationLevel(); // Implement this in TextureManager
        application->regenerateCurrentCardOverlay(); // Trigger regeneration
        break;
        // Handle other special keys if needed (F1, PageUp, etc.)
    default:
        break; // Ignore other special keys
    }
}