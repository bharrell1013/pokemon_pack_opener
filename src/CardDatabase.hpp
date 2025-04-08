#ifndef CARDDATABASE_HPP
#define CARDDATABASE_HPP

#include <vector>
#include <map>
#include <string>
#include "Card.hpp"

// Forward declaration
class TextureManager;

struct PokemonData {
    int id;
    std::string name;
    std::string type;
    std::map<std::string, float> rarityWeights;
    // Constructor
    PokemonData(std::string n, std::string t) : id(0), name(n), type(t) {}

    // Default constructor needed as well
    PokemonData() : id(0) {}
};

class CardDatabase {
private:
    std::vector<PokemonData> pokemonData;

    // Maps for organizing data
    std::map<std::string, std::vector<std::string>> typeToPokemons;

    // Rarity distribution settings
    const int NORMAL_CARDS = 7;
    const int REVERSE_CARDS = 2;
    const int SPECIAL_CARDS = 1;  // holo, ex, or full art

    // Reference to TextureManager
    TextureManager* textureManager;

public:
    CardDatabase(TextureManager* texManager);
    ~CardDatabase();

    void loadLocalDatabase();

    // Card generation methods
    Card generateNormalCard();
    Card generateReverseCard();
    Card generateHoloCard();
    Card generateExCard();
    Card generateFullArtCard();

    // Pack generation with proper rarity distribution
    std::vector<Card> generatePackCards();

    // Random selection helpers
    std::string getRandomPokemon();
    std::string getRandomPokemonOfType(const std::string& type);
    std::string getPokemonType(const std::string& pokemonName);
};

#endif