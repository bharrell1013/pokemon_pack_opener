#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include <memory>
#include <GL/freeglut.h>
#include "CardPack.hpp"
#include "CardDatabase.hpp"
#include "TextureManager.hpp"
#include "InputHandler.hpp"
#include "util.hpp" 
#include <glm/glm.hpp>

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

    // --- Camera Parameters ---
    glm::vec3 cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f); // Point camera looks at
    float cameraRadius = 6.0f;     // Distance from target
    float cameraAzimuth = 0.0f;    // Horizontal angle (radians)
    float cameraElevation = 0.0f;  // Vertical angle (radians)
    const float cameraFov = 60.0f; // Field of view (degrees)
    const float cameraNearPlane = 0.1f;
    const float cameraFarPlane = 100.0f;
    const float minCameraRadius = 1.5f; // Minimum zoom distance
    const float maxCameraRadius = 20.0f; // Maximum zoom distance
    const float maxCameraElevation = glm::radians(85.0f); // Limit vertical angle
    const float minCameraElevation = glm::radians(-85.0f); // Limit vertical angle

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
	static void mouseWheelCallback(int wheel, int direction, int x, int y);
    static void specialCallback(int key, int x, int y);

    // --- Camera Accessors/Mutators ---
    float getCameraRadius() const { return cameraRadius; }
    float getCameraAzimuth() const { return cameraAzimuth; }
    float getCameraElevation() const { return cameraElevation; }

    void setCameraRadius(float radius) {
        cameraRadius = glm::clamp(radius, minCameraRadius, maxCameraRadius);
    }
    void setCameraAzimuth(float azimuth) { cameraAzimuth = azimuth; }
    void setCameraElevation(float elevation) {
        cameraElevation = glm::clamp(elevation, minCameraElevation, maxCameraElevation);
    }
    void regenerateCurrentCardOverlay();
};

// Global application instance
extern std::unique_ptr<Application> application;

#endif