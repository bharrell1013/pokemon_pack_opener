#ifndef LSYSTEM_HPP
#define LSYSTEM_HPP

#include <string>
#include <vector>
#include <map>
#include <sstream> // Needed potentially later, good include

class LSystem {
public:
    LSystem();

    void setAxiom(const std::string& ax);
    void addRule(char variable, const std::string& replacement);
    std::string generate(int iterations);
    void clearRules(); // Utility function

private:
    std::string axiom;
    std::map<char, std::string> rules;
};

#endif // LSYSTEM_HPP