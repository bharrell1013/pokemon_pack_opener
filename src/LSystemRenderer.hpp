#ifndef LSYSTEM_RENDERER_HPP
#define LSYSTEM_RENDERER_HPP

#include <string>
#include <vector>
#include <stack>
#include <glm/glm.hpp>

struct TurtleState {
    glm::vec2 position = glm::vec2(0.0f);
    float angleDegrees = 0.0f;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f); // Default white
    // float lineThickness = 1.0f; // Add later if needed
};

class LSystemRenderer {
public:
    LSystemRenderer(int width, int height);

    // Set rendering parameters
    void setParameters(float step, float angle, const glm::vec3& color,
                       const glm::vec2& startPos, float startAngle);

    // Clears the pixel buffer
    void clearBuffer(const glm::vec3& clearColor = glm::vec3(0.0f, 0.0f, 0.0f));

    // Renders the L-system string to the internal pixel buffer
    // Returns a const reference to the pixel data
    const std::vector<unsigned char>& render(const std::string& lsystemString);

    // Getters
    const std::vector<unsigned char>& getPixelData() const;
    int getWidth() const;
    int getHeight() const;

    void setLineThickness(int thickness);

private:
    int textureWidth;
    int textureHeight;
    int lineThickness = 2;
    std::vector<unsigned char> pixelData; // RGBA format

    // Rendering parameters
    float stepLength = 5.0f;
    float angleIncrementDegrees = 90.0f;
    // Note: drawColor from setParameters sets the INITIAL color in initialTurtleState
    TurtleState initialTurtleState;

    // Private helper methods
    void setPixel(int x, int y, const glm::vec3& color);
    void setPixelBlock(int centerX, int centerY, int thickness, const glm::vec3& color);
    void drawLine(const glm::vec2& start, const glm::vec2& end, const glm::vec3& color);
    // Replace with Bresenham or similar later for better lines
    void drawLineSimple(const glm::vec2& start, const glm::vec2& end, const glm::vec3& color);
};

#endif // LSYSTEM_RENDERER_HPP