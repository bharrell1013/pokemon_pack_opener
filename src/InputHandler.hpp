#ifndef INPUTHANDLER_HPP
#define INPUTHANDLER_HPP

#include <glm/glm.hpp>
#include "CardPack.hpp" // Include CardPack header
#include <iostream>   // For debugging

class InputHandler {
private:
    CardPack* packPtr; // Pointer to the CardPack object

    // Mouse state (keep if mouse rotation for pack model is desired)
    bool mouseDown;
    int lastMouseX;
    int lastMouseY;

public:
    InputHandler(CardPack* pack, void* camera /* camera pointer unused? */) :
        packPtr(pack),
        mouseDown(false),
        lastMouseX(0),
        lastMouseY(0)
    {
    }

    ~InputHandler() {}

    void handleKeyPress(unsigned char key, int x, int y) {
        if (!packPtr) return; // Safety check

        switch (key) {
        case 27: // ESC key
            exit(0);
            break;
        case ' ': // Space bar
        { // Create a scope for state variable
            PackState currentState = packPtr->getState();
            std::cout << "Space pressed. Pack state: " << static_cast<int>(currentState) << std::endl; // Debug log

            if (currentState ==  CLOSED) {
                packPtr->startOpeningAnimation();
            }
            else if (currentState ==  REVEALING) {
                packPtr->cycleCard();
            }
            // Add case for  FINISHED if needed
        }
        break;
        // Add other key handlers if necessary
        }
    }

    void handleMouseClick(int button, int state, int x, int y) {
        if (!packPtr) return;
        // Handle mouse clicks (e.g., for rotating the closed pack)
        if (button == GLUT_LEFT_BUTTON) {
            if (state == GLUT_DOWN) {
                if (packPtr->getState() ==  CLOSED) { // Only allow rotation if pack is closed
                    mouseDown = true;
                    lastMouseX = x;
                    lastMouseY = y;
                }
            }
            else {
                mouseDown = false;
            }
        }
    }

    void handleMouseMotion(int x, int y) {
        if (!packPtr) return;
        // Handle mouse motion for pack rotation
        if (mouseDown && packPtr->getState() ==  CLOSED) {
            float rotX = (y - lastMouseY) * 0.01f;
            float rotY = (x - lastMouseX) * 0.01f;
            packPtr->rotate(rotX, rotY, 0.0f); // Rotate the pack model
            lastMouseX = x;
            lastMouseY = y;
        }
    }
};

#endif