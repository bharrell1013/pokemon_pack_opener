#include "TextureManager.hpp"
#include "Card.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <vector> // Needed for shader info log
#include <cpr/cpr.h>         // For HTTP requests
#include <nlohmann/json.hpp> // For JSON parsing
using json = nlohmann::json; // Alias for convenience

// Define the necessary headers for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TextureManager::TextureManager() : cardShader(0), holoShader(0), currentShader(0) {
    // Ensure OpenGL context is available before initializing shaders
    // This should be guaranteed by the call order in Application::initialize
    initializeShaders();
    if (cardShader == 0 || holoShader == 0) {
        // Optional: Add more specific error handling if initialization failed
        std::cerr << "FATAL: One or more shader programs failed to initialize correctly!" << std::endl;
        // Consider throwing an exception or setting an error state
    }
}

TextureManager::~TextureManager() {
    // Cleanup all textures
    for (auto& pair : textureMap) {
        glDeleteTextures(1, &pair.second);
    }
    textureMap.clear(); // Good practice to clear the map too

    // Cleanup shaders
    if (cardShader != 0) {
        glDeleteProgram(cardShader);
    }
    if (holoShader != 0) {
        glDeleteProgram(holoShader);
    }
}

GLuint TextureManager::loadTexture(const std::string& pathOrUrl) {
    // 1. Check Cache
    if (textureMap.count(pathOrUrl)) {
        return textureMap[pathOrUrl];
    }

    // 2. Check if it's a URL
    if (pathOrUrl.rfind("http://", 0) == 0 || pathOrUrl.rfind("https://", 0) == 0) {
        // It's a URL, download and load from memory
        std::vector<unsigned char> imageData = downloadImageData(pathOrUrl);
        if (!imageData.empty()) {
            return loadTextureFromMemory(imageData, pathOrUrl);
        }
        return 0;
    }

    // 3. It's a local file path
    std::cout << "Attempting to load texture from local path: " << pathOrUrl << std::endl;
    std::filesystem::path fullPath = std::filesystem::absolute(pathOrUrl);
    std::cout << "Absolute path: " << fullPath.string() << std::endl;
    std::cout << "Current working directory: " << std::filesystem::current_path() << std::endl;

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(pathOrUrl.c_str(), &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Failed to load texture from local path: " << pathOrUrl << std::endl;
        std::cerr << "STB Failure Reason: " << stbi_failure_reason() << std::endl;
        return 0;
    }

    std::cout << "Successfully loaded texture: " << pathOrUrl
              << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    if (textureID == 0) {
        std::cerr << "OpenGL Error: Failed to generate texture ID for " << pathOrUrl << std::endl;
        stbi_image_free(data);
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum internalFormat = GL_RGB;
    GLenum dataFormat = GL_RGB;
    if (channels == 4) {
        internalFormat = GL_RGBA;
        dataFormat = GL_RGBA;
    } else if (channels == 3) {
        internalFormat = GL_RGB;
        dataFormat = GL_RGB;
    } else if (channels == 1) {
        internalFormat = GL_RED;
        dataFormat = GL_RED;
        std::cout << "Warning: Loading texture " << pathOrUrl << " as single channel (GL_RED)." << std::endl;
    } else {
        std::cerr << "Error: Unsupported number of channels (" << channels << ") for texture: " << pathOrUrl << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return 0;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Texture Error (" << pathOrUrl << "): " << err << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, 0);
        return 0;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    textureMap[pathOrUrl] = textureID;
    std::cout << "Texture stored in map: " << pathOrUrl << " -> ID: " << textureID << std::endl;
    return textureID;
}

GLuint TextureManager::getTexture(const std::string& textureName) {
    auto it = textureMap.find(textureName);
    if (it != textureMap.end()) {
        return it->second; // Return existing texture ID
    }
    std::cerr << "Warning: Texture '" << textureName << "' not found in TextureManager cache." << std::endl;
    return 0; // Return 0 if not found
}


// --- Pokemon TCG API Integration ---
std::string TextureManager::mapRarityToApiQuery(const std::string& rarity) {
    if (rarity == "normal") return "rarity:Common OR rarity:Uncommon";
    if (rarity == "reverse") return "rarity:Common OR rarity:Uncommon";
    if (rarity == "holo") return "rarity:\"Rare Holo\"";
    if (rarity == "ex") return "subtypes:EX rarity:\"Rare Holo EX\"";
    if (rarity == "full art") return "rarity:\"Rare Ultra\" OR rarity:\"Rare Secret\"";

    std::cerr << "Warning: Unknown rarity '" << rarity << "' for API query. Defaulting to Common." << std::endl;
    return "rarity:Common";
}

std::string TextureManager::fetchCardImageUrl(const Card& card) {
    std::string rarityQuery = mapRarityToApiQuery(card.getRarity());
    std::string cardType = card.getPokemonType();

    std::string typeQuery = "";
    if (!cardType.empty()) {
        typeQuery = " types:" + cardType;
    }

    std::string searchQuery = rarityQuery + typeQuery;

    int resultsToFetch = 20; // Ask for up to 20 cards matching the query
    cpr::Url url = cpr::Url{"https://api.pokemontcg.io/v2/cards"};
    cpr::Parameters params = cpr::Parameters{{"q", searchQuery}, {"pageSize", std::to_string(resultsToFetch)}};

    std::cout << "[API] Searching for: " << searchQuery << std::endl;

    cpr::Response response = cpr::Get(url, params,
                                    cpr::Header{{"X-Api-Key", apiKey}});

    if (response.status_code != 200) {
        std::cerr << "[API] Error fetching card data. Status code: " << response.status_code
                  << ", URL: " << response.url << ", Error: " << response.error.message << std::endl;
        if (!response.text.empty()) {
            std::cerr << "[API] Response body: " << response.text.substr(0, 500) << "..." << std::endl;
        }
        return "";
    }

    // --- Parse JSON and Pick Random ---
    try {
        json data = json::parse(response.text);

        if (data.contains("data") && data["data"].is_array() && !data["data"].empty()) {
            json cardResults = data["data"];
            int numResults = cardResults.size(); // How many did the API actually return?

            if (numResults > 0) {
                // --- Pick a random index from the returned results ---
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<int> distrib(0, numResults - 1); // Range is 0 to numResults-1
                int randomIndex = distrib(gen);
                // --- End random index selection ---

                json randomCard = cardResults[randomIndex]; // Get the JSON object for the random card

                if (randomCard.contains("images") && randomCard["images"].contains("large")) {
                    std::string imageUrl = randomCard["images"]["large"].get<std::string>();
                    std::cout << "[API] Found " << numResults << " results. Picked random index " << randomIndex
                        << ". URL: " << imageUrl << std::endl;
                    return imageUrl; // Return the URL of the randomly selected card
                }
                else {
                    std::cerr << "[API] Error: 'images.large' field not found in randomly selected card data (index " << randomIndex << ")." << std::endl;
                    // Optionally: try another random index? Or just fail.
                    return "";
                }
            }
            else {
                // This case shouldn't happen if the outer check passed, but good to have
                std::cout << "[API] No cards found matching query (empty data array): " << searchQuery << std::endl;
                return "";
            }

        }
        else {
            std::cout << "[API] 'data' array not found or empty in response for query: " << searchQuery << std::endl;
            if (data.contains("error")) {
                std::cerr << "[API] Error message from API: " << data["error"] << std::endl;
            }
            // Example: Sometimes the API might just return a single object on error
            else if (data.contains("status") && data.contains("message")) {
                std::cerr << "[API] Error message from API: Status " << data["status"] << " - " << data["message"] << std::endl;
            }
            return "";
        }
    }
    catch (json::exception& e) { // Catch both parse_error and other json exceptions
        std::cerr << "[API] JSON Exception: " << e.what() << std::endl;
        std::cerr << "[API] Response Text: " << response.text.substr(0, 500) << "..." << std::endl;
        return "";
    }
    return "";
}

std::vector<unsigned char> TextureManager::downloadImageData(const std::string& imageUrl) {
    std::cout << "Downloading image from: " << imageUrl << std::endl;
    cpr::Response response = cpr::Get(cpr::Url{imageUrl});

    if (response.status_code == 200) {
        return std::vector<unsigned char>(response.text.begin(), response.text.end());
    }
    std::cerr << "Failed to download image. Status code: " << response.status_code
              << ", URL: " << imageUrl << ", Error: " << response.error.message << std::endl;
    return {};
}

GLuint TextureManager::loadTextureFromMemory(const std::vector<unsigned char>& imageData, const std::string& cacheKey) {
    if (textureMap.count(cacheKey)) {
        return textureMap[cacheKey];
    }

    if (imageData.empty()) {
        std::cerr << "Error: Image data buffer is empty for key: " << cacheKey << std::endl;
        return 0;
    }

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load_from_memory(imageData.data(), static_cast<int>(imageData.size()),
                                              &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Failed to load texture from memory for key: " << cacheKey << std::endl;
        std::cerr << "STB Failure Reason: " << stbi_failure_reason() << std::endl;
        return 0;
    }

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum internalFormat = (channels == 4) ? GL_RGBA : GL_RGB;
    GLenum dataFormat = (channels == 4) ? GL_RGBA : GL_RGB;
    
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    textureMap[cacheKey] = textureID;
    return textureID;
}

// --- Modified: generateCardTexture ---
GLuint TextureManager::generateCardTexture(const Card& card) {
    GLuint textureID = 0;

    // --- 1. Attempt API Fetch ---
    std::string imageUrl = fetchCardImageUrl(card);

    if (!imageUrl.empty()) {
        std::vector<unsigned char> imageData = downloadImageData(imageUrl);
        if (!imageData.empty()) {
            textureID = loadTextureFromMemory(imageData, imageUrl);
        }
    }

    // --- 2. Fallback to Local Template if API failed ---
    if (textureID == 0) {
        std::string templatePath = "textures/cards/card_template.png";
        std::cout << "[Fallback] API fetch failed or yielded no image for " << card.getPokemonName()
                  << ". Falling back to template: " << templatePath << std::endl;
        textureID = loadTexture(templatePath);
    }

    // --- 3. Fallback to Placeholder if Template ALSO failed ---
    if (textureID == 0) {
        std::string placeholderPath = "textures/pokemon/placeholder.png";
        std::cerr << "[Fallback] Failed to load template texture. Falling back to placeholder: "
                  << placeholderPath << std::endl;
        textureID = loadTexture(placeholderPath);
    }

    // --- 4. Final Error Check ---
    if (textureID == 0) {
        std::cerr << "FATAL: generateCardTexture failed to load ANY texture (API, template, placeholder) for "
                  << card.getPokemonName() << std::endl;
    }

    return textureID;
}

GLuint TextureManager::generateHoloEffect(GLuint baseTexture, const std::string& rarity) {
    if (holoShader == 0) {
        std::cerr << "Error: Cannot generate holo effect, holoShader is invalid." << std::endl;
        return baseTexture; // Return base if shader is bad
    }
    // Bind the holo shader
    glUseProgram(holoShader);

    // Set holo effect intensity based on rarity
    float intensity = 0.0f;
    if (rarity == "holo") intensity = 0.7f;
    else if (rarity == "reverse") intensity = 0.4f;
    else if (rarity == "ex") intensity = 0.8f;
    else if (rarity == "full art") intensity = 0.9f;

    // Set shader uniforms
    GLint intensityLoc = glGetUniformLocation(holoShader, "holoIntensity");
    if (intensityLoc == -1) {
        std::cerr << "Warning: Uniform 'holoIntensity' not found in holo shader." << std::endl;
    }
    else {
        glUniform1f(intensityLoc, intensity);
    }

    // Remember to unbind the shader later if needed, or let the next apply*Shader handle it
    return baseTexture; // For now, return base texture. Later we'll implement FBO rendering
}


void TextureManager::applyCardShader(const Card& card) {
    // *** Check if cardShader is valid before using it ***
    if (cardShader == 0) {
        std::cerr << "Error in applyCardShader: cardShader program ID is invalid (0)." << std::endl;
        return; // Don't proceed if shader is invalid
    }

    currentShader = cardShader;
    glUseProgram(cardShader); // This should now be safe if cardShader > 0

    // Check for GL errors after glUseProgram
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after glUseProgram(cardShader=" << cardShader << "): " << err << std::endl;
        // Potentially return or handle error
    }

    // Set shader uniforms
    GLint typeLoc = glGetUniformLocation(cardShader, "cardType");
    GLint rarityLoc = glGetUniformLocation(cardShader, "cardRarity");

    // Check if uniforms were found
    if (typeLoc == -1) {
        std::cerr << "Warning: Uniform 'cardType' not found in card shader (ID: " << cardShader << ")" << std::endl;
    }
    if (rarityLoc == -1) {
        std::cerr << "Warning: Uniform 'cardRarity' not found in card shader (ID: " << cardShader << ")" << std::endl;
    }

    // Convert type and rarity to numeric values for the shader
    int typeValue = getTypeValue(card.getPokemonType());
    int rarityValue = getRarityValue(card.getRarity());

    // Set uniforms only if locations are valid
    if (typeLoc != -1) {
        glUniform1i(typeLoc, typeValue);
    }
    if (rarityLoc != -1) {
        glUniform1i(rarityLoc, rarityValue);
    }

    // Check for GL errors after setting uniforms
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after setting card shader uniforms: " << err << std::endl;
    }
}

void TextureManager::applyHoloShader(const Card& card, float time) {
    // *** Check if holoShader is valid before using it ***
    if (holoShader == 0) {
        std::cerr << "Error in applyHoloShader: holoShader program ID is invalid (0)." << std::endl;
        return; // Don't proceed if shader is invalid
    }
    currentShader = holoShader;
    glUseProgram(holoShader);

    // Check for GL errors after glUseProgram
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after glUseProgram(holoShader=" << holoShader << "): " << err << std::endl;
        // Potentially return or handle error
    }

    // Set time uniform for animated effects
    GLint timeLoc = glGetUniformLocation(holoShader, "time");
    if (timeLoc == -1) {
        std::cerr << "Warning: Uniform 'time' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    }
    else {
        glUniform1f(timeLoc, time);
    }


    // Set other holo effect parameters
    GLint intensityLoc = glGetUniformLocation(holoShader, "holoIntensity");
    GLint typeLoc = glGetUniformLocation(holoShader, "cardType");

    if (intensityLoc == -1) {
        std::cerr << "Warning: Uniform 'holoIntensity' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    }
    if (typeLoc == -1) {
        std::cerr << "Warning: Uniform 'cardType' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    }


    float intensity = 0.0f;
    if (card.getRarity() == "holo") intensity = 0.7f;
    else if (card.getRarity() == "reverse") intensity = 0.4f;
    else if (card.getRarity() == "ex") intensity = 0.8f;
    else if (card.getRarity() == "full art") intensity = 0.9f;

    if (intensityLoc != -1) {
        glUniform1f(intensityLoc, intensity);
    }
    if (typeLoc != -1) {
        glUniform1i(typeLoc, getTypeValue(card.getPokemonType()));
    }

    // Check for GL errors after setting uniforms
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after setting holo shader uniforms: " << err << std::endl;
    }
}


void TextureManager::initializeShaders() {
    std::cout << "Initializing shaders..." << std::endl;
    // Load and compile shaders
    std::string vertexShaderPath = "shaders/card_v.glsl";
    std::string fragmentShaderPath = "shaders/card_f.glsl";
    std::string holoVertexPath = "shaders/holo_v.glsl";
    std::string holoFragmentPath = "shaders/holo_f.glsl";

    // Create shader programs
    cardShader = createShaderProgram(vertexShaderPath, fragmentShaderPath);
    holoShader = createShaderProgram(holoVertexPath, holoFragmentPath);

    if (cardShader != 0) {
        std::cout << "Card shader program created successfully. ID: " << cardShader << std::endl;
    }
    else {
        std::cerr << "Failed to create card shader program!" << std::endl;
    }

    if (holoShader != 0) {
        std::cout << "Holo shader program created successfully. ID: " << holoShader << std::endl;
    }
    else {
        std::cerr << "Failed to create holo shader program!" << std::endl;
    }
    std::cout << "Shader initialization finished." << std::endl;
    std::cout << "[DEBUG] TextureManager::initializeShaders() - this: " << this
        << ", cardShader ID: " << cardShader
        << ", holoShader ID: " << holoShader << std::endl;
}


// *** MODIFIED createShaderProgram with Error Checking ***
GLuint TextureManager::createShaderProgram(const std::string& vertexPath, const std::string& fragmentPath) {
    // 1. Load shader sources
    std::string vertexSource = loadShaderSource(vertexPath);
    std::string fragmentSource = loadShaderSource(fragmentPath);
    if (vertexSource.empty()) {
        std::cerr << "Error: Vertex shader source file is empty or not found: " << vertexPath << std::endl;
        return 0;
    }
    if (fragmentSource.empty()) {
        std::cerr << "Error: Fragment shader source file is empty or not found: " << fragmentPath << std::endl;
        return 0;
    }

    const char* vertexSourcePtr = vertexSource.c_str();
    const char* fragmentSourcePtr = fragmentSource.c_str();

    GLint success;
    GLchar infoLog[1024]; // Increased buffer size

    // 2. Create and compile vertex shader
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    if (vertexShader == 0) {
        std::cerr << "Error creating vertex shader object for " << vertexPath << std::endl;
        return 0;
    }
    glShaderSource(vertexShader, 1, &vertexSourcePtr, nullptr);
    glCompileShader(vertexShader);

    // Check vertex shader compilation status
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << vertexPath << "\n" << infoLog << std::endl;
        glDeleteShader(vertexShader); // Clean up the failed shader object
        return 0; // Return 0 indicates failure
    }
    else {
        std::cout << "Vertex shader compiled successfully: " << vertexPath << std::endl;
    }


    // 3. Create and compile fragment shader
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    if (fragmentShader == 0) {
        std::cerr << "Error creating fragment shader object for " << fragmentPath << std::endl;
        glDeleteShader(vertexShader); // Clean up vertex shader too
        return 0;
    }
    glShaderSource(fragmentShader, 1, &fragmentSourcePtr, nullptr);
    glCompileShader(fragmentShader);

    // Check fragment shader compilation status
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << fragmentPath << "\n" << infoLog << std::endl;
        glDeleteShader(vertexShader); // Clean up vertex shader
        glDeleteShader(fragmentShader); // Clean up fragment shader
        return 0; // Return 0 indicates failure
    }
    else {
        std::cout << "Fragment shader compiled successfully: " << fragmentPath << std::endl;
    }


    // 4. Create and link shader program
    GLuint program = glCreateProgram();
    if (program == 0) {
        std::cerr << "Error creating shader program object." << std::endl;
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return 0;
    }
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // Check program linking status
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << "Vertex: " << vertexPath << " | Fragment: " << fragmentPath << "\n" << infoLog << std::endl;
        glDeleteProgram(program); // Clean up the failed program object
        glDeleteShader(vertexShader); // Detaching is implicitly handled by deleting shaders
        glDeleteShader(fragmentShader);
        return 0; // Return 0 indicates failure
    }
    else {
        std::cout << "Shader program linked successfully (VS: " << vertexPath << ", FS: " << fragmentPath << "). ID: " << program << std::endl;
    }


    // 5. Detach and delete shaders after successful linking (they are no longer needed)
    // Note: Some argue deleting is sufficient as linking copies the needed info,
    // but detaching first is technically cleaner according to some specs.
    glDetachShader(program, vertexShader);
    glDetachShader(program, fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program; // Return the valid program ID
}

std::string TextureManager::loadShaderSource(const std::string& path) {
    std::cout << "Loading shader source from: " << path << std::endl;
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open shader file: " << path << std::endl;
        std::cerr << "Current working directory: " << std::filesystem::current_path() << std::endl;
        std::filesystem::path absPath = std::filesystem::absolute(path);
        std::cerr << "Attempted absolute path: " << absPath << std::endl;
        return ""; // Return empty string on failure
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close(); // Close the file
    std::string source = buffer.str();
    if (source.empty()) {
        std::cout << "Warning: Shader file is empty: " << path << std::endl;
    }
    return source;
}

// --- getTypeValue and getRarityValue remain the same ---
int TextureManager::getTypeValue(const std::string& type) {
    // Convert type string to numeric value for shader
    if (type == "Normal") return 0;    // Capitalized
    if (type == "Fire") return 1;      // Capitalized
    if (type == "Water") return 2;     // Capitalized
    if (type == "Grass") return 3;     // Capitalized
    if (type == "Lightning") return 4; // Capitalized (API uses Lightning for Electric)
    if (type == "Psychic") return 5;    // Capitalized
    if (type == "Fighting") return 6;   // Capitalized
    if (type == "Darkness") return 7;   // Capitalized (API uses Darkness for Dark)
    if (type == "Dragon") return 8;     // Capitalized
    if (type == "Fairy") return 9;      // Capitalized
    if (type == "Metal") return 10;     // Capitalized (API uses Metal for Steel)
    if (type == "Ghost") return 11;     // Capitalized - Assuming Ghost was intended type 11? Check your shader array order.
    if (type == "Colorless") return 0;  // Map Colorless back to Normal/Index 0 for tinting? Or add a 12th color?
    std::cerr << "Warning: Unknown Pokemon type '" << type << "' encountered in getTypeValue. Defaulting to 0 (normal)." << std::endl;
    return 0; // Default to normal type
}

int TextureManager::getRarityValue(const std::string& rarity) {
    // Convert rarity string to numeric value for shader
    if (rarity == "normal") return 0;
    if (rarity == "reverse") return 1;
    if (rarity == "holo") return 2;
    if (rarity == "ex") return 3;
    if (rarity == "full art") return 4;
    std::cerr << "Warning: Unknown rarity '" << rarity << "' encountered in getRarityValue. Defaulting to 0 (normal)." << std::endl;
    return 0; // Default to normal rarity
}