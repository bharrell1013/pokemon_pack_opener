#include "LSystem.hpp"
#include <iostream> // For potential debug output

LSystem::LSystem() : axiom("") {}

void LSystem::setAxiom(const std::string& ax) {
    axiom = ax;
}

void LSystem::addRule(char variable, const std::string& replacement) {
    rules[variable] = replacement;
}

void LSystem::clearRules() {
    rules.clear();
}

std::string LSystem::generate(int iterations) {
    if (axiom.empty()) {
        std::cerr << "Warning: LSystem::generate called with empty axiom." << std::endl;
        return "";
    }
    if (rules.empty() && iterations > 0) {
         std::cerr << "Warning: LSystem::generate called with iterations > 0 but no rules defined." << std::endl;
         // Fallthrough to return axiom if iterations = 0 or no rules
    }


    std::string currentString = axiom;
    for (int i = 0; i < iterations; ++i) {
        std::stringstream nextStringStream; // Use stringstream for efficiency
        for (char currentChar : currentString) {
            auto it = rules.find(currentChar);
            if (it != rules.end()) {
                // Rule found for this character
                nextStringStream << it->second; // Append the replacement string
            } else {
                // No rule found, append the character itself (constant)
                nextStringStream << currentChar;
            }
        }
        currentString = nextStringStream.str();

        // Optional: Add a check for excessive string length to prevent memory issues
        // if (currentString.length() > SOME_MAX_LENGTH) {
        //     std::cerr << "Warning: L-system string exceeded max length after iteration " << i+1 << ". Stopping generation." << std::endl;
        //     break;
        // }
    }
    return currentString;
}