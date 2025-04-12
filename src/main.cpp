#include "gl_core_3_3.h" 
#include <iostream>
#include <memory>
#include "Application.hpp"

// Global application instance
//std::unique_ptr<Application> application;

int main(int argc, char** argv) {
    try {
        // Create and initialize the application
        application = std::make_unique<Application>();
        application->initialize(argc, argv);

        // Start the main loop
        application->run();

    }
    catch (const std::exception& e) {
        // Handle any errors
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}