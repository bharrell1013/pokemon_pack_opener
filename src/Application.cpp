#include "gl_core_3_3.h"
#include "Application.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cmath>

std::unique_ptr<Application> application;

Application::Application() :
    lastFrameTime(0),
    currentTime(0),
    deltaTime(0),
    isRunning(true),
    cameraTarget(0.0f, 0.0f, 0.0f),
    cameraRadius(6.0f),
    cameraAzimuth(0.0f), // Start looking along -Z
    cameraElevation(0.0f)
{
    // Initialize components
    /*cardDatabase = std::make_unique<CardDatabase>();*/
    //textureManager = std::make_unique<TextureManager>();
    //cardPack = std::make_unique<CardPack>();
    //inputHandler = std::make_unique<InputHandler>(cardPack.get(), nullptr);
    std::cout << "Application constructor START" << std::endl;
    // Note: We'll initialize OpenGL-dependent components in initialize()
    std::cout << "Application constructor END (OpenGL components NOT created yet)" << std::endl;
}

Application::~Application() {
    // Resources will be cleaned up by unique_ptr
}

void Application::initialize(int argc, char** argv) {
    std::cout << "Application::initialize START" << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // Set OpenGL attributes
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create window
    window = SDL_CreateWindow("Pokémon Pack Simulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600, SDL_WINDOW_OPENGL);
    if (window == NULL) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // Create context
    glContext = SDL_GL_CreateContext(window);
    if (glContext == NULL) {
        std::cerr << "OpenGL context could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        exit(1);
    }

    // Initialize GLEW (or Galogen, or manually load function pointers)
    // This is where you would typically call glewInit() or gladLoadGL()
    // For Galogen, just make sure the first GL call happens after context creation

    //GLenum err = glGetError(); // Harmless call to potentially trigger loading
    //if (err != GL_NO_ERROR) {
    //    std::cerr << "OpenGL error after window creation check: " << err << std::endl;
    //}
    //std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    //std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
    
    try {
        std::vector<GLuint> shaders;
        shaders.push_back(compileShader(GL_VERTEX_SHADER, "shaders/pack_v.glsl"));
        shaders.push_back(compileShader(GL_FRAGMENT_SHADER, "shaders/pack_f.glsl"));
        shaderProgramID = linkProgram(shaders);
        std::cout << "Shaders loaded and linked successfully. Program ID: " << shaderProgramID << std::endl;

        // Cleanup shader objects after linking
        for (GLuint s : shaders) {
            glDeleteShader(s);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Shader Error: " << e.what() << std::endl;
        // Handle error appropriately (e.g., exit)
        cleanup(); // Or some other error handling
        exit(1);
    }

    // *** IMPORTANT: Initialize OpenGL function pointers ***
    // Although Galogen loads implicitly, it's safer AFTER window creation.
    // If using GLEW/GLAD explicitly, call glewInit() or gladLoadGL() here.
    // For Galogen, the first GL call after this point should work. Let's
    // make a harmless call just to be sure or to trigger the loading.
    GLenum err = glGetError(); // Call a basic GL function to trigger pointer loading if needed.
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error after window creation: " << err << std::endl;
        // Consider exiting or throwing an exception
    }
    // Or, if Galogen has an explicit init function, call it here.

    std::cout << "Initializing OpenGL-dependent components..." << std::endl;

    // Create TextureManager first since other components depend on it
    textureManager = std::make_unique<TextureManager>();
    std::cout << "TextureManager created" << std::endl;

    // Create CardDatabase with TextureManager
    cardDatabase = std::make_unique<CardDatabase>(textureManager.get());
    std::cout << "CardDatabase created" << std::endl;

    // Load pack texture
    std::cout << "Loading textures..." << std::endl;
    packTextureID = textureManager->loadTexture("textures/pack/pack_diffuse.png");
    if (packTextureID == 0) {
        std::cerr << "Failed to load pack texture!" << std::endl;
    } else {
        std::cout << "Pack texture loaded. ID: " << packTextureID << std::endl;
    }

    // Create CardPack after TextureManager and CardDatabase are ready
    cardPack = std::make_unique<CardPack>(textureManager.get());
    std::cout << "CardPack created" << std::endl;

    // Create InputHandler last since it depends on CardPack
    inputHandler = std::make_unique<InputHandler>(cardPack.get(), nullptr);
    std::cout << "InputHandler created" << std::endl;

    std::cout << "OpenGL-dependent components Initialized." << std::endl;

    // Initialize OpenGL states
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE); // Optional: Cull back faces
    glCullFace(GL_BACK);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    // Generate a card pack (Now cardPack and cardDatabase exist)
    if (cardPack && cardDatabase) { // Good practice to check pointers
        const char* originalTitle = "Pokémon Pack Simulator";
        std::cout << "Generating cards..." << std::endl;
        SDL_SetWindowTitle(window, "Loading Card Images...");
        cardPack->generateCards(*cardDatabase);
        std::cout << "Cards generated." << std::endl;
        SDL_SetWindowTitle(window, originalTitle);
    }
    else {
        std::cerr << "Error: cardPack or cardDatabase is null before generating cards!" << std::endl;
    }

    // Calculate initial time
    lastFrameTime = (float)SDL_GetTicks() / 1000.0f;

    std::cout << "Application::initialize END" << std::endl;
}

void Application::run() {
    isRunning = true; // Set running flag to true

    // Main loop
    SDL_Event e;
    while (isRunning) {
        // Handle events
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                isRunning = false;
            }
            else if (e.type == SDL_KEYDOWN) {
                keyboardCallback(e.key.keysym.sym, 0, 0);
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                mouseCallback(e.button.button, SDL_PRESSED, e.button.x, e.button.y);
            }
            else if (e.type == SDL_MOUSEBUTTONUP) {
                mouseCallback(e.button.button, SDL_RELEASED, e.button.x, e.button.y);
            }
            else if (e.type == SDL_MOUSEMOTION) {
                motionCallback(e.motion.x, e.motion.y);
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                mouseWheelCallback(0, e.wheel.y, e.motion.x, e.motion.y);
            }
        }

        update();
        render();

        SDL_GL_SwapWindow(window);
    }
}

void Application::update() {
    // Calculate delta time
    float newTime = SDL_GetTicks() / 1000.0f;
    deltaTime = newTime - currentTime;
    currentTime = newTime;

    // Update components
    if (cardPack) {
        cardPack->update(deltaTime);
    }
}

void Application::render() {
    // Clear the screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // --- End Loading Screen Check ---

    //glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 6.0f); // Keep camera position consistent

    // --- Calculate Camera Position from Orbit Parameters ---
    glm::vec3 cameraPos;
    cameraPos.x = cameraTarget.x + cameraRadius * cos(cameraElevation) * sin(cameraAzimuth);
    cameraPos.y = cameraTarget.y + cameraRadius * sin(cameraElevation);
    cameraPos.z = cameraTarget.z + cameraRadius * cos(cameraElevation) * cos(cameraAzimuth);
    // --- End Camera Calculation ---

    // Create view-projection matrix
    glm::mat4 projection = glm::perspective(
        glm::radians(cameraFov),
        800.0f / 600.0f, // Hardcoded window dimensions
        cameraNearPlane,
        cameraFarPlane
    );

    glm::mat4 view = glm::lookAt(
        cameraPos,          // Calculated orbit position
        cameraTarget,       // Still look at the origin (or card stack center)
        glm::vec3(0.0f, 1.0f, 0.0f) // Standard up vector
    );

    // Don't combine here yet, pass separately or combine in CardPack/Card as needed
    // glm::mat4 viewProjection = projection * view; // Not needed here anymore

    // *** RENDER PACK ***
    // Activate the PACK shader
    //glUseProgram(shaderProgramID); // Use the application's shader (ID 3) for the pack

    //// Set PACK shader view and projection uniforms (these are needed by the shader)
    //GLint viewLoc = glGetUniformLocation(shaderProgramID, "view");
    //GLint projLoc = glGetUniformLocation(shaderProgramID, "projection");
    //GLint viewPosLoc = glGetUniformLocation(shaderProgramID, "viewPos");
    //if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    //if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    //if (viewPosLoc != -1) glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));
    // else std::cerr << "Warning: Uniform 'viewPos' not found in shader." << std::endl; // Less critical maybe


    //// Setup texture for the PACK
    //GLint texLoc = glGetUniformLocation(shaderProgramID, "diffuseTexture");
    //if (texLoc != -1 && packTextureID != 0) { // Check packTextureID too
    //    glActiveTexture(GL_TEXTURE0); // Activate texture unit 0
    //    glBindTexture(GL_TEXTURE_2D, packTextureID); // Bind the loaded pack texture
    //    glUniform1i(texLoc, 0); // Tell sampler uniform to use texture unit 0
    //}
    //else {
    //    if (texLoc == -1) std::cerr << "Warning: Uniform 'diffuseTexture' not found in pack shader." << std::endl;
    //    if (packTextureID == 0) std::cerr << "Warning: packTextureID is 0." << std::endl;
    //}

    // Render components that use the PACK shader
    if (cardPack) {
        PackState currentState = cardPack->getState();

        if (currentState == CLOSED) {
            // --- RENDER PACK MODEL using pack shader (ID: shaderProgramID) ---
            glUseProgram(shaderProgramID); // Activate the pack's lighting shader

            // Set common matrix uniforms (view, projection)
            GLint viewLoc = glGetUniformLocation(shaderProgramID, "view");
            GLint projLoc = glGetUniformLocation(shaderProgramID, "projection");
            if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

            // Set lighting uniforms for the pack shader
            GLint viewPosLoc = glGetUniformLocation(shaderProgramID, "viewPos");
            GLint lightPosLoc = glGetUniformLocation(shaderProgramID, "lightPos");
            GLint lightColorLoc = glGetUniformLocation(shaderProgramID, "lightColor");
            GLint shininessLoc = glGetUniformLocation(shaderProgramID, "shininess");
            GLint texLoc = glGetUniformLocation(shaderProgramID, "diffuseTexture"); // Sampler

            if (viewPosLoc != -1) glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));
            // TODO: Make lightPos dynamic or keep it fixed?
            if (lightPosLoc != -1) glUniform3f(lightPosLoc, 1.0f, 2.0f, 3.0f); // Example light pos
            if (lightColorLoc != -1) glUniform3f(lightColorLoc, 1.0f, 1.0f, 1.0f); // White light
            if (shininessLoc != -1) glUniform1f(shininessLoc, 32.0f); // Example shininess

            // Set texture for the pack model
            if (texLoc != -1 && packTextureID != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, packTextureID);
                glUniform1i(texLoc, 0); // Tell sampler to use texture unit 0
            }

            // Let CardPack handle its own model matrix and drawing for the pack
            cardPack->render(shaderProgramID, view, projection, packTextureID, cameraPos);

            // Clean up texture binding
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0); // Deactivate pack shader

        }
        else { // REVEALING or FINISHED state
            // --- RENDER CARDS using card/holo shaders ---
            // CardPack::render will handle activating card/holo shaders internally
            // It now receives cameraPos needed for holo shader's viewPos uniform
            cardPack->render(0, view, projection, 0, cameraPos); // Pass 0 for pack shader/texture IDs
        }
    }

    // Deactivate the pack shader program and unbind texture AFTER pack and potentially cards have been drawn
    glUseProgram(0);
    //glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture from unit 0

    // Swap buffers
    SDL_GL_SwapWindow(window);
}

void Application::cleanup() {
    // Cleanup will be handled by unique_ptr destructors
    isRunning = false; // Set running flag to false

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Application::keyboardCallback(unsigned char key, int x, int y) {
    if (application && application->inputHandler) {
        application->inputHandler->handleKeyPress(key, x, y);
    }
}

void Application::mouseCallback(int button, int state, int x, int y) {
    if (application && application->inputHandler) {
        application->inputHandler->handleMouseClick(button, state, x, y);
    }
}

void Application::motionCallback(int x, int y) {
    if (application && application->inputHandler) {
        application->inputHandler->handleMouseMotion(x, y);
    }
}

void Application::mouseWheelCallback(int wheel, int direction, int x, int y) {
    // Pass to InputHandler or handle directly here
    if (application && application->inputHandler) {
        // Let InputHandler process it, as it knows the application state context
        application->inputHandler->handleMouseWheel(wheel, direction, x, y);
    }
    // Or handle directly (simpler if InputHandler doesn't need complex state for zoom):
    // if (application) {
    //    float currentRadius = application->getCameraRadius();
    //    float zoomSpeed = 0.5f;
    //    if (direction > 0) { // Wheel scrolled up (or forward) -> Zoom in
    //        application->setCameraRadius(currentRadius - zoomSpeed);
    //    } else if (direction < 0) { // Wheel scrolled down (or backward) -> Zoom out
    //        application->setCameraRadius(currentRadius + zoomSpeed);
    //    }
    //    SDL_PostRedisplay(); // Request redraw after zoom
    // }
}

void Application::resetPack() {
    if (!cardPack || !cardDatabase || !textureManager) {
        std::cerr << "Error: Cannot reset pack. Core components missing." << std::endl;
        return;
    }

    std::cout << "Resetting pack..." << std::endl;

    // Change window title during generation
    const char* originalTitle = "Pokémon Pack Simulator"; // Store original title again
    SDL_SetWindowTitle(window, "Generating New Card Pack...");

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    SDL_GL_SwapWindow(window);

    // Clear any API/Texture cache *if desired* for a completely fresh pull.
    // Optional - uncomment if you want 'N' to force re-downloading everything.
    // Be mindful of API rate limits if you do this frequently.
    // textureManager->clearApiCache(); // Needs implementation in TextureManager
    // textureManager->clearTextureCache(); // Needs implementation in TextureManager

    try {
        // Regenerate the cards. This should also reset the CardPack state to CLOSED.
        cardPack->generateCards(*cardDatabase);
        std::cout << "New card pack generated successfully." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error during pack regeneration: " << e.what() << std::endl;
        // Handle error state if needed (e.g., show an error message)
    }

    // Reset window title
    SDL_SetWindowTitle(window, originalTitle);

    // Optional: Reset InputHandler state if necessary (e.g., mouse drag)
    if (inputHandler) {
        // inputHandler->resetMouseState(); // You would need to add this method if needed
    }
    // No explicit state change needed for CardPack here, generateCards sets it to CLOSED.
}

void Application::specialCallback(int key, int x, int y) {
    if (application && application->inputHandler) {
        application->inputHandler->handleSpecialKeyPress(key, x, y);
    }
}

void Application::regenerateCurrentCardOverlay() {
    if (!cardPack || !textureManager) {
        std::cerr << "Error: Cannot regenerate overlay, CardPack or TextureManager missing." << std::endl;
        return;
    }
    if (cardPack->getState() == REVEALING || cardPack->getState() == FINISHED) { // Only if cards are visible
        int currentIdx = cardPack->getCurrentCardIndex(); // Need a getter in CardPack
        if (currentIdx >= 0 && currentIdx < cardPack->getCards().size()) { // Need getCards() in CardPack
            Card& currentCard = cardPack->getCards()[currentIdx]; // Need non-const access or a specific method

            std::cout << "Regenerating overlay for card " << currentIdx
                << " with variation level: " << textureManager->getLSystemVariationLevel() // Need getter
                << std::endl;

            // Regenerate (this handles caching internally, using the modified key)
            GLuint newOverlayID = textureManager->generateProceduralOverlayTexture(currentCard);

            // Update the card's overlay texture ID
            currentCard.setOverlayTextureID(newOverlayID);

            // Request redraw
            SDL_UpdateWindowSurface(window);
        }
    }
}