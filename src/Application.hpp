#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <memory>
#include <GL/freeglut.h>
#include "CardPack.hpp"
#include "CardDatabase.hpp"
#include "TextureManager.hpp"
#include "InputHandler.hpp"
#include "util.hpp" 

class CardPack;
class CardDatabase;
class TextureManager;
class InputHandler;

class Application {
private:
    // Core components
    std::unique_ptr<CardPack> cardPack;
    std::unique_ptr<CardDatabase> cardDatabase;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<InputHandler> inputHandler;

    // Timing
    float lastFrameTime;
    float currentTime;
    float deltaTime;

    // Application state
    bool isRunning;

    // Shader program
    GLuint shaderProgramID;

    GLuint packTextureID;

public:
    Application();
    ~Application();

    void initialize(int argc, char** argv);
    void run();
    void update();
    void render();
    void cleanup();
    void resetPack();
    TextureManager* getApplicationTextureManager() { return textureManager.get(); }

    // GLUT callback wrappers
    static void displayCallback();
    static void reshapeCallback(int width, int height);
    static void keyboardCallback(unsigned char key, int x, int y);
    static void mouseCallback(int button, int state, int x, int y);
    static void motionCallback(int x, int y);
    static void idleCallback();
};

// Global application instance
extern std::unique_ptr<Application> application;

#endif