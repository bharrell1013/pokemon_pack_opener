#include "gl_core_3_3.h"
#include "Application.hpp"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#include <glm/gtc/type_ptr.hpp>
#include <GL/freeglut.h>

std::unique_ptr<Application> application;

Application::Application() :
    lastFrameTime(0),
    currentTime(0),
    deltaTime(0),
    isRunning(true)
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

    // Initialize GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
    glutInitWindowSize(800, 600);

    std::cout << "Calling glutCreateWindow..." << std::endl;
    const char* originalTitle = "Pokémon Pack Simulator"; // Store original title
    glutCreateWindow(originalTitle);
    std::cout << "glutCreateWindow DONE (OpenGL context should exist now)" << std::endl;
    
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

    // Setup GLUT callbacks (These need the application instance, often via a global)
    glutDisplayFunc(displayCallback);
    glutReshapeFunc(reshapeCallback);
    glutKeyboardFunc(keyboardCallback);
    glutMouseFunc(mouseCallback);
    glutMotionFunc(motionCallback);
    glutIdleFunc(idleCallback);

    // Initialize OpenGL states
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

    // Generate a card pack (Now cardPack and cardDatabase exist)
    if (cardPack && cardDatabase) { // Good practice to check pointers
        std::cout << "Generating cards..." << std::endl;
        glutSetWindowTitle("Loading Card Images...");
        cardPack->generateCards(*cardDatabase);
        std::cout << "Cards generated." << std::endl;
        glutSetWindowTitle(originalTitle);
    }
    else {
        std::cerr << "Error: cardPack or cardDatabase is null before generating cards!" << std::endl;
    }

    // Calculate initial time
    lastFrameTime = (float)glutGet(GLUT_ELAPSED_TIME) / 1000.0f;

    std::cout << "Application::initialize END" << std::endl;
}

void Application::run() {
	isRunning = true; // Set running flag to true
    glutMainLoop();
}

void Application::update() {
    // Calculate delta time
    float newTime = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
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

    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 6.0f); // Keep camera position consistent

    // Create view-projection matrix
    glm::mat4 projection = glm::perspective(
        glm::radians(60.0f),
        (float)glutGet(GLUT_WINDOW_WIDTH) / (float)glutGet(GLUT_WINDOW_HEIGHT),
        0.1f,
        100.0f
    );

    glm::mat4 view = glm::lookAt(
        cameraPos,
        glm::vec3(0.0f, 0.0f, 0.0f),  // Look at origin
        glm::vec3(0.0f, 1.0f, 0.0f)   // Up vector
    );

    // Don't combine here yet, pass separately or combine in CardPack/Card as needed
    // glm::mat4 viewProjection = projection * view; // Not needed here anymore

    // *** RENDER PACK ***
    // Activate the PACK shader
    glUseProgram(shaderProgramID); // Use the application's shader (ID 3) for the pack

    // Set PACK shader view and projection uniforms (these are needed by the shader)
    GLint viewLoc = glGetUniformLocation(shaderProgramID, "view");
    GLint projLoc = glGetUniformLocation(shaderProgramID, "projection");
    GLint viewPosLoc = glGetUniformLocation(shaderProgramID, "viewPos");
    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    if (viewPosLoc != -1) glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));
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
        // Pass PACK shader ID, view/projection matrices, AND packTextureID
        cardPack->render(shaderProgramID, view, projection, packTextureID); // *** MODIFIED CALL ***
    }

    // Deactivate the pack shader program and unbind texture AFTER pack and potentially cards have been drawn
    glUseProgram(0);
    //glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture from unit 0

    // Swap buffers
    glutSwapBuffers();
}

void Application::cleanup() {
    // Cleanup will be handled by unique_ptr destructors
	isRunning = false; // Set running flag to false
}

// GLUT callback wrappers
void Application::displayCallback() {
    application->render();
}

void Application::reshapeCallback(int width, int height) {
    glViewport(0, 0, width, height);
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

void Application::idleCallback() {
    application->update();
    glutPostRedisplay();
}

void Application::resetPack() {
    if (!cardPack || !cardDatabase || !textureManager) {
        std::cerr << "Error: Cannot reset pack. Core components missing." << std::endl;
        return;
    }

    std::cout << "Resetting pack..." << std::endl;

    // Change window title during generation
    const char* originalTitle = "Pokémon Pack Simulator"; // Store original title again
    glutSetWindowTitle("Generating New Card Pack...");

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glutSwapBuffers();

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
    glutSetWindowTitle(originalTitle);

    // Optional: Reset InputHandler state if necessary (e.g., mouse drag)
    if (inputHandler) {
        // inputHandler->resetMouseState(); // You would need to add this method if needed
    }
    // No explicit state change needed for CardPack here, generateCards sets it to CLOSED.
}