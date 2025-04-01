#include "CardDatabase.hpp"
#include <fstream>
#include <random>
#include <iostream>
#include "json.hpp"

CardDatabase::CardDatabase() {
    loadLocalDatabase();
}

CardDatabase::~CardDatabase() {
    // Cleanup if needed
}

void CardDatabase::loadLocalDatabase() {
    // For now, let's hardcode some Pokémon for testing
    std::vector<std::string> types = { "normal", "fire", "water", "grass", "electric", "psychic", "fighting", "dark" };

    // Add some basic Pokémon
    pokemonData.push_back(PokemonData("Pikachu", "electric"));
    pokemonData.push_back(PokemonData("Charizard", "fire"));
    pokemonData.push_back(PokemonData("Blastoise", "water"));
    pokemonData.push_back(PokemonData("Venusaur", "grass"));
    pokemonData.push_back(PokemonData("Mewtwo", "psychic"));
    pokemonData.push_back(PokemonData("Jigglypuff", "normal"));
    pokemonData.push_back(PokemonData("Machamp", "fighting"));
    pokemonData.push_back(PokemonData("Gengar", "dark"));

    // Populate the type map for easier access
    for (const auto& pokemon : pokemonData) {
        typeToPokemons[pokemon.type].push_back(pokemon.name);
    }
}

Card CardDatabase::generateNormalCard() {
    std::string pokemon = getRandomPokemon();
    std::string type = "normal"; // This should come from the pokemonData
    return Card(pokemon, type, "normal");
}

Card CardDatabase::generateReverseCard() {
    std::string pokemon = getRandomPokemon();
    std::string type = "normal"; // This should come from the pokemonData
    return Card(pokemon, type, "reverse");
}

Card CardDatabase::generateHoloCard() {
    std::string pokemon = getRandomPokemon();
    std::string type = "normal"; // This should come from the pokemonData
    return Card(pokemon, type, "holo");
}

Card CardDatabase::generateExCard() {
    std::string pokemon = getRandomPokemon();
    std::string type = "normal"; // This should come from the pokemonData
    return Card(pokemon, type, "ex");
}

Card CardDatabase::generateFullArtCard() {
    std::string pokemon = getRandomPokemon();
    std::string type = "normal"; // This should come from the pokemonData
    return Card(pokemon, type, "full art");
}

std::vector<Card> CardDatabase::generatePackCards() {
    std::vector<Card> cards;

    // Generate 7 normal cards
    for (int i = 0; i < NORMAL_CARDS; i++) {
        cards.push_back(generateNormalCard());
    }

    // Generate 2 reverse cards
    for (int i = 0; i < REVERSE_CARDS; i++) {
        cards.push_back(generateReverseCard());
    }

    // Generate 1 special card (holo, ex, or full art)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 2);
    int specialType = dist(gen);

    if (specialType == 0) {
        cards.push_back(generateHoloCard());
    }
    else if (specialType == 1) {
        cards.push_back(generateExCard());
    }
    else {
        cards.push_back(generateFullArtCard());
    }

    return cards;
}

std::string CardDatabase::getRandomPokemon() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, pokemonData.size() - 1);
    return pokemonData[dist(gen)].name;
}

std::string CardDatabase::getRandomPokemonOfType(const std::string& type) {
    if (typeToPokemons.find(type) == typeToPokemons.end() || typeToPokemons[type].empty()) {
        return getRandomPokemon(); // Fallback
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, typeToPokemons[type].size() - 1);
    return typeToPokemons[type][dist(gen)];
}