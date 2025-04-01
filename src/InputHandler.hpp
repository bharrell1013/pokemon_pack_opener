#ifndef INPUTHANDLER_HPP
#define INPUTHANDLER_HPP

#include <glm/glm.hpp>
#include "CardPack.hpp"

class InputHandler {
private:
    CardPack* packPtr;

    // Mouse state
    bool mouseDown;
    int lastMouseX;
    int lastMouseY;

public:
    InputHandler(CardPack* pack, void* camera) :
        packPtr(pack),
        mouseDown(false),
        lastMouseX(0),
        lastMouseY(0) {
    }

    ~InputHandler() {}

    void handleKeyPress(unsigned char key, int x, int y) {
        // Handle key presses
        switch (key) {
        case 27: // ESC key
            exit(0);
            break;
        case ' ': // Space bar
            if (packPtr) packPtr->startOpeningAnimation();
            break;
        }
    }

    void handleMouseClick(int button, int state, int x, int y) {
        // Handle mouse clicks
        if (button == GLUT_LEFT_BUTTON) {
            if (state == GLUT_DOWN) {
                mouseDown = true;
                lastMouseX = x;
                lastMouseY = y;
            }
            else {
                mouseDown = false;
            }
        }
    }

    void handleMouseMotion(int x, int y) {
        // Handle mouse motion
        if (mouseDown && packPtr) {
            // Calculate rotation based on mouse movement
            float rotX = (y - lastMouseY) * 0.01f;
            float rotY = (x - lastMouseX) * 0.01f;

            // Rotate the pack
            packPtr->rotate(rotX, rotY, 0.0f);

            lastMouseX = x;
            lastMouseY = y;
        }
    }
};

#endif