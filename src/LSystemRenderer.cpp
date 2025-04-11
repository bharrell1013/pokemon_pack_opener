#include "LSystemRenderer.hpp"
#include <cmath>
#include <iostream> // For errors
#include <algorithm> // For std::fill, std::max, std::min

// Define PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LSystemRenderer::LSystemRenderer(int width, int height) :
    textureWidth(width), textureHeight(height) {
    if (width <= 0 || height <= 0) {
        throw std::runtime_error("LSystemRenderer dimensions must be positive.");
    }
    pixelData.resize(static_cast<size_t>(width) * height * 4); // 4 bytes per pixel (RGBA)
    // Set default initial state (e.g., center, facing up)
    initialTurtleState.position = glm::vec2(width / 2.0f, height / 2.0f);
    initialTurtleState.angleDegrees = 90.0f; // Assuming 0 degrees is right, 90 is up
    clearBuffer(); // Initialize buffer to transparent black
}

void LSystemRenderer::setParameters(float step, float angle, const glm::vec3& color,
                                    const glm::vec2& startPos, float startAngle) {
    stepLength = step;
    angleIncrementDegrees = angle;
    initialTurtleState.position = startPos;
    initialTurtleState.angleDegrees = startAngle;
    initialTurtleState.color = color; // Set initial drawing color
}

void LSystemRenderer::clearBuffer(const glm::vec3& clearColor) {
    unsigned char r = static_cast<unsigned char>(clearColor.r * 255.0f);
    unsigned char g = static_cast<unsigned char>(clearColor.g * 255.0f);
    unsigned char b = static_cast<unsigned char>(clearColor.b * 255.0f);
    unsigned char a = 0; // Default to transparent black background

    for (size_t i = 0; i < pixelData.size(); i += 4) {
        pixelData[i + 0] = r;
        pixelData[i + 1] = g;
        pixelData[i + 2] = b;
        pixelData[i + 3] = a; // Set alpha
    }
}

void LSystemRenderer::setPixel(int x, int y, const glm::vec3& color) {
    // Basic bounds check
    if (x < 0 || x >= textureWidth || y < 0 || y >= textureHeight) {
        return; // Out of bounds
    }

    // Calculate index in the 1D array (assuming origin is top-left for pixel buffer)
    // Flip y if needed: int flippedY = textureHeight - 1 - y;
    size_t index = (static_cast<size_t>(y) * textureWidth + x) * 4;

    // Clamp color values just in case
    float r = std::max(0.0f, std::min(1.0f, color.r));
    float g = std::max(0.0f, std::min(1.0f, color.g));
    float b = std::max(0.0f, std::min(1.0f, color.b));

    pixelData[index + 0] = static_cast<unsigned char>(r * 255.0f);
    pixelData[index + 1] = static_cast<unsigned char>(g * 255.0f);
    pixelData[index + 2] = static_cast<unsigned char>(b * 255.0f);
    pixelData[index + 3] = 255; // Make pixel opaque
}

void LSystemRenderer::setPixelBlock(int centerX, int centerY, int thickness, const glm::vec3& color) {
    int halfThickness = thickness / 2;
    for (int yOffset = -halfThickness; yOffset < halfThickness + (thickness % 2); ++yOffset) {
        for (int xOffset = -halfThickness; xOffset < halfThickness + (thickness % 2); ++xOffset) {
            int x = centerX + xOffset;
            int y = centerY + yOffset;

            // Basic bounds check (copied from original setPixel)
            if (x < 0 || x >= textureWidth || y < 0 || y >= textureHeight) {
                continue; // Out of bounds
            }
            size_t index = (static_cast<size_t>(y) * textureWidth + x) * 4;
            float r = std::max(0.0f, std::min(1.0f, color.r));
            float g = std::max(0.0f, std::min(1.0f, color.g));
            float b = std::max(0.0f, std::min(1.0f, color.b));
            pixelData[index + 0] = static_cast<unsigned char>(r * 255.0f);
            pixelData[index + 1] = static_cast<unsigned char>(g * 255.0f);
            pixelData[index + 2] = static_cast<unsigned char>(b * 255.0f);
            pixelData[index + 3] = 255; // Make pixel opaque
        }
    }
}

void LSystemRenderer::drawLine(const glm::vec2& start, const glm::vec2& end, const glm::vec3& color) {
    int x0 = static_cast<int>(std::floor(start.x));
    int y0 = static_cast<int>(std::floor(start.y));
    int x1 = static_cast<int>(std::floor(end.x));
    int y1 = static_cast<int>(std::floor(end.y));

    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0); // Use negative dy for error calculation

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx + dy; // Initial error value

    while (true) {
        //setPixel(x0, y0, color); // Plot current point
		setPixelBlock(x0, y0, lineThickness, color); // Draw line with thickness

        if (x0 == x1 && y0 == y1) break; // Reached end point

        int e2 = 2 * err;
        if (e2 >= dy) { // Error threshold crossed for x step
            if (x0 == x1) break; // Prevent infinite loop if dx=0
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) { // Error threshold crossed for y step
            if (y0 == y1) break; // Prevent infinite loop if dy=0
            err += dx;
            y0 += sy;
        }
    }
}

// Simple line drawing - just plots end points for now
void LSystemRenderer::drawLineSimple(const glm::vec2& start, const glm::vec2& end, const glm::vec3& color) {
     setPixel(static_cast<int>(round(start.x)), static_cast<int>(round(start.y)), color);
     setPixel(static_cast<int>(round(end.x)), static_cast<int>(round(end.y)), color);
     // TODO: Implement Bresenham's or similar for continuous lines
}


const std::vector<unsigned char>& LSystemRenderer::render(const std::string& lsystemString) {
    TurtleState currentTurtle = initialTurtleState;
    std::stack<TurtleState> stateStack;

    for (char command : lsystemString) {
        if (command == 'F' || command == 'G') { // Common forward commands
            float angleRad = glm::radians(currentTurtle.angleDegrees);
            // Correct direction: cos for x, sin for y (assuming angle 0 is right, 90 is up)
            glm::vec2 direction = glm::vec2(cos(angleRad), sin(angleRad));
            glm::vec2 endPos = currentTurtle.position + direction * stepLength;
            //drawLineSimple(currentTurtle.position, endPos, currentTurtle.color); // Draw using turtle's current color
            drawLine(currentTurtle.position, endPos, currentTurtle.color);
            currentTurtle.position = endPos;
        } else if (command == 'f') { // Move forward without drawing
            float angleRad = glm::radians(currentTurtle.angleDegrees);
            glm::vec2 direction = glm::vec2(cos(angleRad), sin(angleRad));
            currentTurtle.position += direction * stepLength;
        } else if (command == '+') { // Turn right (decrease angle if 0=right, 90=up)
            currentTurtle.angleDegrees -= angleIncrementDegrees;
        } else if (command == '-') { // Turn left (increase angle if 0=right, 90=up)
            currentTurtle.angleDegrees += angleIncrementDegrees;
        } else if (command == '[') { // Push state
            stateStack.push(currentTurtle);
        } else if (command == ']') { // Pop state
            if (!stateStack.empty()) {
                currentTurtle = stateStack.top();
                stateStack.pop();
            } else {
                std::cerr << "Error: LSystemRenderer encountered unmatched ']'" << std::endl;
                // Optionally break or continue
            }
        } else if (command == '.') { // NEW: Dot command
            // Draw a small block at the current position using current color & thickness
            setPixelBlock(static_cast<int>(round(currentTurtle.position.x)),
                static_cast<int>(round(currentTurtle.position.y)),
                lineThickness, // Use existing thickness or a fixed small size like 1 or 2
                currentTurtle.color);
            // Don't move the turtle for a dot command, just draw at location
        }
         // --- Placeholder for Color Change ---
         else if (command == 'C') { // Example: Simple random color change
             currentTurtle.color = glm::vec3(static_cast<float>(rand()) / RAND_MAX,
                                             static_cast<float>(rand()) / RAND_MAX,
                                             static_cast<float>(rand()) / RAND_MAX);
         }
        // Ignore other characters or add more commands here
    }

    return pixelData;
}

void LSystemRenderer::setLineThickness(int thickness) {
    lineThickness = std::max(1, thickness); // Ensure at least 1
}

// Implement Getters
const std::vector<unsigned char>& LSystemRenderer::getPixelData() const { return pixelData; }
int LSystemRenderer::getWidth() const { return textureWidth; }
int LSystemRenderer::getHeight() const { return textureHeight; }