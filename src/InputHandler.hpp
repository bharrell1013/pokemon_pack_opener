// InputHandler.hpp (Modified)
#ifndef INPUTHANDLER_HPP
#define INPUTHANDLER_HPP

#include <glm/glm.hpp>
#include <iostream>
#include "Application.hpp" // Keep for safety, might be removable if only .cpp uses it
#include "CardPack.hpp"

class InputHandler {
private:
    CardPack* packPtr;
    bool mouseDown;
    int lastMouseX;
    int lastMouseY;

public:
    // Keep constructor inline or move definition to .cpp
    InputHandler(CardPack* pack, void* camera = nullptr) :
        packPtr(pack), mouseDown(false), lastMouseX(0), lastMouseY(0) {
    }
    ~InputHandler() {}

    // Declarations only:
    void handleKeyPress(unsigned char key, int x, int y);
    void handleMouseClick(int button, int state, int x, int y);
    void handleMouseMotion(int x, int y);
    void handleMouseWheel(int wheel, int direction, int x, int y);
    void handleSpecialKeyPress(int key, int x, int y);
    void resetState() { mouseDown = false; }
};

#endif // INPUTHANDLER_HPP