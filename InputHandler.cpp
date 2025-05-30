// InputHandler.cpp (New File)
#include "InputHandler.hpp"
#include "Application.hpp"  // Needed for the global 'application'
#include "CardPack.hpp"     // Needed for packPtr methods
#include "TextureManager.hpp" // Needed for getApplicationTextureManager()->cycleShaderMode()
#include <iostream>
#include <GL/freeglut.h>    // Needed for exit(), GLUT_LEFT_BUTTON etc.

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
    }
}

void InputHandler::handleMouseClick(int button, int state, int x, int y) {
    if (!packPtr) return;
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            if (packPtr->getState() == CLOSED) {
                mouseDown = true;
                lastMouseX = x;
                lastMouseY = y;
            }
        }
        else { // GLUT_UP
            mouseDown = false;
        }
    }
}

void InputHandler::handleMouseMotion(int x, int y) {
    if (!packPtr) return;
    if (mouseDown && packPtr->getState() == CLOSED) {
        float rotX = (y - lastMouseY) * 0.01f;
        float rotY = (x - lastMouseX) * 0.01f;
        packPtr->rotate(rotX, rotY, 0.0f);
        lastMouseX = x;
        lastMouseY = y;
    }
}