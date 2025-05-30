#include "TextureManager.hpp"
#include "Card.hpp" // Include Card definition
#include <iostream>
#include <filesystem> // For path operations and checking
#include <fstream>
#include <sstream>
#include <random>    // For random card selection
#include <vector>    // Needed for image data buffer and shader info 
#include <sstream>
#include <iomanip>
#include <functional>
#include <algorithm> // For std::remove_if
#include <glm/gtc/random.hpp>

// --- Dependencies for API ---
#include <cpr/cpr.h>         // For HTTP requests
#include <nlohmann/json.hpp> // For JSON parsing
using json = nlohmann::json; // Alias for convenience
// --- End Dependencies ---

// Define the necessary headers for image loading
//#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <glm/gtc/type_ptr.hpp>

TextureManager::TextureManager() : cardShader(0), holoShader(0), currentShader(0), cardBackTextureID(0) {
    // Ensure OpenGL context is available before initializing shaders
    // This should be guaranteed by the call order in Application::initialize
    initializeShaders();
    std::string backTexPath = "textures/cards/card_back.png";
    std::cout << "Loading card back texture: " << backTexPath << std::endl;
    cardBackTextureID = loadTexture(backTexPath);
    if (cardBackTextureID == 0) {
        std::cerr << "!!!!!!!! FAILED TO LOAD CARD BACK TEXTURE !!!!!!!!" << std::endl;
        // Maybe load placeholder or throw error if essential
    }
    else {
        std::cout << "Card back texture loaded. ID: " << cardBackTextureID << std::endl;
    }
    if (cardShader == 0 || holoShader == 0) {
        std::cerr << "FATAL: One or more shader programs failed to initialize correctly!" << std::endl;
        // Consider throwing an exception or setting an error state
    }
     if (!std::filesystem::exists(imageCacheDirectory)) {
         try {
             std::filesystem::create_directories(imageCacheDirectory);
             std::cout << "Created image cache directory: " << imageCacheDirectory << std::endl;
         }
         catch (const std::filesystem::filesystem_error& e) {
             std::cerr << "Error creating image cache directory: " << e.what() << std::endl;
             // Decide how to handle this - maybe disable caching?
         }
     }
     std::cout << "Loading Pok�mon pack overlay images from: " << packImagesDirectory << std::endl;
     if (std::filesystem::exists(packImagesDirectory) && std::filesystem::is_directory(packImagesDirectory)) {
         for (const auto& entry : std::filesystem::directory_iterator(packImagesDirectory)) {
             if (entry.is_regular_file()) {
                 std::string filepath = entry.path().string();
                 // Optional: Check for specific extensions like .png
                 if (filepath.length() > 4 && filepath.substr(filepath.length() - 4) == ".png") {
                     std::cout << "  Loading overlay: " << filepath << std::endl;
                     GLuint texID = loadTexture(filepath); // Use existing loadTexture
                     if (texID != 0) {
                         packPokemonTextureIDs.push_back(texID);
                         std::cout << "    -> Loaded Overlay Texture ID: " << texID << std::endl;
                     }
                     else {
                         std::cerr << "    -> Failed to load overlay texture: " << filepath << std::endl;
                     }
                 }
             }
         }
         std::cout << "Finished loading " << packPokemonTextureIDs.size() << " pack overlay images." << std::endl;
     }
     else {
         std::cerr << "Warning: Pack images directory not found or not a directory: " << packImagesDirectory << std::endl;
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
    // 1. Check Cache first using the pathOrUrl as the key
    if (textureMap.count(pathOrUrl)) {
        // std::cout << "Cache hit for: " << pathOrUrl << " -> ID: " << textureMap[pathOrUrl] << std::endl;
        return textureMap[pathOrUrl];
    }

    // 2. Check if it's a URL
    if (pathOrUrl.rfind("http://", 0) == 0 || pathOrUrl.rfind("https://", 0) == 0) {
        // It's a URL, download and load from memory
        std::cout << "Attempting to load texture from URL: " << pathOrUrl << std::endl;
        std::vector<unsigned char> imageData = downloadImageData(pathOrUrl);
        if (!imageData.empty()) {
            // Use the URL itself as the cache key when loading from memory
            return loadTextureFromMemory(imageData, pathOrUrl);
        }
        // Download failed, return 0
        std::cerr << "Failed to download or load texture from URL: " << pathOrUrl << std::endl;
        return 0;
    }

    // 3. It's a local file path
    std::cout << "Attempting to load texture from local path: " << pathOrUrl << std::endl;
    // --- Check if file exists before trying to load ---
    if (!std::filesystem::exists(pathOrUrl)) {
         std::cerr << "Error: Local texture file not found: " << pathOrUrl << std::endl;
         std::cerr << "Absolute path attempted: " << std::filesystem::absolute(pathOrUrl) << std::endl;
         std::cerr << "Current working directory: " << std::filesystem::current_path() << std::endl;
         return 0;
    }
    // --- End file existence check ---

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); // Important for OpenGL
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

    // Determine texture format
    GLenum internalFormat = GL_RGB;
    GLenum dataFormat = GL_RGB;
    int bytesPerPixel = 3;         // Default guess
    if (channels == 4) {
        internalFormat = GL_RGBA;
        dataFormat = GL_RGBA;
        bytesPerPixel = 4;
    } else if (channels == 3) {
        internalFormat = GL_RGB;
        dataFormat = GL_RGB;
        bytesPerPixel = 3;
    } else if (channels == 1) {
        internalFormat = GL_RED; // Treat single channel as RED
        dataFormat = GL_RED;
        bytesPerPixel = 1;
        std::cout << "Warning: Loading texture " << pathOrUrl << " as single channel (GL_RED)." << std::endl;
    } else {
        std::cerr << "Error: Unsupported number of channels (" << channels << ") for texture: " << pathOrUrl << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID); // Clean up generated ID
        glBindTexture(GL_TEXTURE_2D, 0);
        return 0;
    }

    // If the row stride (width * bytesPerPixel) is not a multiple of 4, set alignment to 1.
    if ((width * bytesPerPixel) % 4 != 0) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        // std::cout << "[Debug] Setting UNPACK_ALIGNMENT to 1 for " << pathOrUrl << std::endl;
    }
    else {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // Ensure it's 4 otherwise (the default)
    }

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    // Reset Pixel Unpack Alignment to Default
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Or GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // Or GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Check for OpenGL errors during texture creation
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Texture Error (" << pathOrUrl << "): " << err << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, 0); // Unbind before returning error
        return 0;
    }

    stbi_image_free(data); // Free CPU memory
    glBindTexture(GL_TEXTURE_2D, 0); // Unbind texture

    // Store in cache using the original pathOrUrl as key
    textureMap[pathOrUrl] = textureID;
    std::cout << "Texture stored in map: " << pathOrUrl << " -> ID: " << textureID << std::endl;
    return textureID;
}

GLuint TextureManager::getTexture(const std::string& textureName) {
    auto it = textureMap.find(textureName);
    if (it != textureMap.end()) {
        return it->second; // Return existing texture ID
    }
    // std::cerr << "Warning: Texture '" << textureName << "' not found in TextureManager cache." << std::endl;
    return 0; // Return 0 if not found
}


// --- NEW: Helper to map internal rarity names to API query strings ---
std::string TextureManager::mapRarityToApiQuery(const std::string& rarity) {
    // Based on https://docs.pokemontcg.io/api-reference/cards/search-cards
    // Note: API uses specific strings, sometimes combined with subtypes. Adjust as needed.
    //       These are examples, you might need more specific queries for V, VMAX, VSTAR etc.
    if (rarity == "normal") return "rarity:Common OR rarity:Uncommon"; // Combine for less specific search
    if (rarity == "reverse") return "rarity:Common OR rarity:Uncommon"; // Reverse Holo is a print property, base card is C/U
    if (rarity == "holo") return "rarity:\"Rare Holo\"";
    // "ex" could mean many things now (old EX, modern ex). Be more specific if possible.
    // Let's assume modern 'ex' which are often "Double Rare" or similar, or old 'EX' which were "Rare Holo EX"
    if (rarity == "ex") return "(rarity:\"Double Rare\" OR rarity:\"Rare Holo EX\")"; // Broaden search for EX/ex
    // Full Arts are often Ultra Rare or Secret Rare, sometimes Illustration Rares
    if (rarity == "full art") return "(rarity:\"Ultra Rare\" OR rarity:\"Secret Rare\" OR rarity:\"Illustration Rare\" OR rarity:\"Special Illustration Rare\")";

    // Fallback for unknown rarities
    std::cerr << "Warning: Unknown rarity '" << rarity << "' for API query. Defaulting to Common/Uncommon." << std::endl;
    return "rarity:Common OR rarity:Uncommon";
}

// --- NEW: Fetches image URL from Pokemon TCG API ---
std::string TextureManager::fetchCardImageUrl(const Card& card) {
    // 1. Construct the API query string (searchQuery)
    std::string rarityQueryPart = mapRarityToApiQuery(card.getRarity());
    std::string cardType = card.getPokemonType();
    std::string typeQueryPart = "";

    if (!cardType.empty() && cardType != "Colorless" && cardType != "Normal") { // Adjusted logic slightly for clarity
        typeQueryPart = "types:" + cardType;
    }
    else if (cardType == "Normal" || cardType == "Colorless") { // Group Normal/Colorless as Colorless type in API
        typeQueryPart = "types:Colorless";
    }
    // Add handling for other card types (Trainer, Energy) if needed

    std::string searchQuery = rarityQueryPart;
    if (!typeQueryPart.empty()) {
        // Ensure there's a space if both parts exist
        if (!searchQuery.empty()) {
            searchQuery += " ";
        }
        searchQuery += typeQueryPart;
    }

    // Basic check: if query is empty, can't search
    if (searchQuery.empty()) {
        std::cerr << "[API] Cannot generate search query for the card." << std::endl;
        return "";
    }

    // --- 2. Check/Use the Query Cache ---
    bool needsApiFetch = true;
    std::string selectedUrl = "";

    if (apiQueryCache.count(searchQuery)) {
        ApiQueryResult& cachedResult = apiQueryCache[searchQuery];
        if (!cachedResult.urls.empty()) {
            // We have *some* cached URLs for this query.
            needsApiFetch = false; // Assume we can use cache initially

            // --- RANDOMNESS LOGIC ---
            // Decide if we should *force* a fetch of a *new page* for more variety,
            // or if just picking from the current cached list is okay.
            // Example: 10% chance to fetch a new random page if possible
            std::random_device rd_chance;
            std::mt19937 gen_chance(rd_chance());
            std::uniform_real_distribution<> distrib_chance(0.0, 1.0);

            bool forceFetchNewPage = (cachedResult.totalCount > cachedResult.pageSize) && // Only if multiple pages exist
                (cachedResult.pageSize > 0) && // Avoid division by zero later
                (distrib_chance(gen_chance) < 0.10); // 10% chance

            if (forceFetchNewPage) {
                std::cout << "[API Cache] Forcing fetch of new page for query: \"" << searchQuery << "\"" << std::endl;
                needsApiFetch = true; // Trigger API fetch below
            }
            else {
                // Use the existing cached list
                std::random_device rd;
                std::mt19937 gen(rd());
                // Ensure list isn't empty before generating distribution range
                if (cachedResult.urls.empty()) {
                    std::cerr << "[API Cache] Cache Hit for query: \"" << searchQuery << "\", but URL list is unexpectedly empty. Forcing fetch." << std::endl;
                    needsApiFetch = true; // Something's wrong, try fetching again
                }
                else {
                    std::uniform_int_distribution<size_t> distrib(0, cachedResult.urls.size() - 1);
                    auto it = cachedResult.urls.begin();
                    std::advance(it, distrib(gen));
                    selectedUrl = *it;
                    std::cout << "[API Cache] Hit for query: \"" << searchQuery << "\". Picked URL from existing cache: " << selectedUrl << std::endl;
                }
                // Return is handled after the if(needsApiFetch) block now
            }
        }
        else {
            // Cached entry exists, but URL list is empty (maybe previous fetch failed?)
            std::cout << "[API Cache] Hit for query: \"" << searchQuery << "\", but cached URL list is empty. Needs API fetch." << std::endl;
            // needsApiFetch remains true
        }
    }
    else {
        std::cout << "[API Cache] Miss for query: \"" << searchQuery << "\". Needs API fetch." << std::endl;
        // needsApiFetch is already true
    }

    // --- 3. API Fetch (If Needed) ---
    if (needsApiFetch) {
        std::cout << "[API] Preparing API fetch for query: \"" << searchQuery << "\"" << std::endl;

        int pageToFetch = 1; // Default to page 1 for first fetch or if no pagination info
        int resultsToFetch = 100; // Keep fetching a decent batch size
        using json = nlohmann::json; // Alias for convenience

        // If we decided to force fetch a new page, calculate which one
        // Also calculate if it's not the first fetch for this query
        if (apiQueryCache.count(searchQuery)) { // Check if cache entry exists (even if empty)
            ApiQueryResult& cachedMeta = apiQueryCache[searchQuery];
            // Check if pagination is possible based on previous fetch metadata
            if (cachedMeta.totalCount > cachedMeta.pageSize && cachedMeta.pageSize > 0) {
                int maxPages = (cachedMeta.totalCount + cachedMeta.pageSize - 1) / cachedMeta.pageSize; // Ceiling division
                if (maxPages > 1) {
                    std::random_device rd_page;
                    std::mt19937 gen_page(rd_page());
                    std::uniform_int_distribution<int> distrib_page(1, maxPages);
                    pageToFetch = distrib_page(gen_page);
                    // Optional: Avoid fetching the *same* page we already have cached, if desired
                    // while (pageToFetch == cachedMeta.fetchedPage && maxPages > 1) {
                    //     pageToFetch = distrib_page(gen_page);
                    // }
                    std::cout << "[API] Decided to fetch random page " << pageToFetch << " of " << maxPages << std::endl;
                }
                else {
                    std::cout << "[API] Pagination possible but calculated maxPages is 1. Fetching page 1." << std::endl;
                    pageToFetch = 1; // Reset to 1 if calculation ends up weird
                }
            }
            else {
                std::cout << "[API] Cache exists but not enough results for pagination ("
                    << cachedMeta.totalCount << " total, " << cachedMeta.pageSize << " page size). Fetching page 1." << std::endl;
                pageToFetch = 1; // Fetch page 1 if no pagination possible
            }
        }
        else {
            std::cout << "[API] First fetch for this query. Fetching page 1." << std::endl;
            pageToFetch = 1; // First time fetch, get page 1
        }


        // Build parameters, including the page number
        cpr::Parameters params = cpr::Parameters{
            {"q", searchQuery},
            {"pageSize", std::to_string(resultsToFetch)},
            {"page", std::to_string(pageToFetch)}
            // No orderBy needed unless specifically desired
        };

        // Make API call
        std::cout << "[API] Sending request to: " << apiBaseUrl << " with params: q=" << searchQuery
            << ", pageSize=" << resultsToFetch << ", page=" << pageToFetch << std::endl;
        cpr::Response response = cpr::Get(cpr::Url{ apiBaseUrl }, params, cpr::Header{ {"X-Api-Key", apiKey} });

        // Handle Response & Errors
        if (response.status_code != 200) {
            std::cerr << "[API] Error fetching card data. Status code: " << response.status_code
                << ", URL: " << response.url << ", Error: " << response.error.message << std::endl;

            if (response.status_code == 429) {
                std::cerr << "[API] !!! RATE LIMIT HIT (429 Too Many Requests) !!! Consider adding delays or reducing requests." << std::endl;
                // TODO: Implement exponential backoff/retry mechanism here if needed
            }
            else if (response.status_code == 400) {
                std::cerr << "[API] Bad Request (400). Check query syntax: q=" << searchQuery << std::endl;
            }
            else if (response.status_code == 404) {
                std::cerr << "[API] Not Found (404). Possibly invalid endpoint or query parameters?" << std::endl;
            }
            // Log response body for debugging if available
            if (!response.text.empty()) {
                std::cerr << "[API] Response body (truncated): " << response.text.substr(0, 500) << (response.text.length() > 500 ? "..." : "") << std::endl;
            }

            // If cache already exists, don't wipe it on error, just return empty for this attempt
            // If cache didn't exist, maybe cache an empty result to prevent immediate retries?
            if (!apiQueryCache.count(searchQuery)) {
                std::cout << "[API Cache] Caching empty result for query \"" << searchQuery << "\" after API error." << std::endl;
                apiQueryCache[searchQuery] = {}; // Cache empty result placeholder
            }
            return ""; // Return empty string on error
        }

        // Parse JSON
        try {
            json data = json::parse(response.text);
            // Extract metadata (important for pagination)
            int totalCount = 0;
            int pageSizeFromApi = 0; // Use the requested size as fallback

            if (data.contains("totalCount") && data["totalCount"].is_number()) {
                totalCount = data["totalCount"].get<int>();
            }
            else {
                std::cout << "[API] Warning: 'totalCount' not found in response. Pagination may be unreliable." << std::endl;
                // Attempt fallback if data array exists
                if (data.contains("data") && data["data"].is_array()) {
                    totalCount = data["data"].size(); // This is only the count for *this page*
                }
            }
            if (data.contains("pageSize") && data["pageSize"].is_number()) {
                pageSizeFromApi = data["pageSize"].get<int>();
            }
            else {
                pageSizeFromApi = resultsToFetch; // Use the requested size if not in response
                std::cout << "[API] Warning: 'pageSize' not found in response. Using requested size: " << resultsToFetch << std::endl;
            }


            if (data.contains("data") && data["data"].is_array() && !data["data"].empty()) {
                json cardResults = data["data"];
                ApiQueryResult newResult;
                newResult.pageSize = pageSizeFromApi;
                newResult.totalCount = totalCount;
                newResult.fetchedPage = pageToFetch;
                // newResult.fetchedTime = std::chrono::steady_clock::now(); // If using TTL

                // Populate URL list
                for (const auto& resultCard : cardResults) {
                    if (resultCard.contains("images") && resultCard["images"].is_object()) {
                        if (resultCard["images"].contains("small") && resultCard["images"]["small"].is_string()) {
                            newResult.urls.push_back(resultCard["images"]["small"].get<std::string>());
                        }
                        else if (resultCard["images"].contains("large") && resultCard["images"]["large"].is_string()) {
                            // Fallback to large if small isn't there (or isn't a string)
                            newResult.urls.push_back(resultCard["images"]["large"].get<std::string>());
                            std::cout << "[API] Note: Using 'large' image URL as 'small' was unavailable for a card." << std::endl;
                        }
                        // else: No suitable image found for this card entry
                    }
                }

                if (!newResult.urls.empty()) {
                    // --- Cache Update Strategy ---
                    // Option A: Replace cache entirely with this new page's results (Simpler)
                    apiQueryCache[searchQuery] = newResult;

                    // Option B: Merge/Append (More complex, keeps larger pool but needs careful management)
                    /*
                    if (apiQueryCache.count(searchQuery)) {
                        // Merge lists (ensure no duplicates if needed)
                        apiQueryCache[searchQuery].urls.splice(apiQueryCache[searchQuery].urls.end(), newResult.urls);
                        apiQueryCache[searchQuery].urls.unique(); // Remove duplicates if splice adds them
                        // Update metadata - careful here, totalCount/pageSize should reflect overall query
                        apiQueryCache[searchQuery].totalCount = newResult.totalCount; // Assume latest fetch has correct total
                        apiQueryCache[searchQuery].pageSize = newResult.pageSize;
                        // Maybe track multiple fetched pages? More complex state.
                    } else {
                        apiQueryCache[searchQuery] = newResult;
                    }
                    */

                    std::cout << "[API Cache] Stored/Updated " << newResult.urls.size() << " URLs (Page " << pageToFetch
                        << ", Total: " << newResult.totalCount << ") for query: \"" << searchQuery << "\"" << std::endl;

                    // Pick URL from the *newly fetched* list
                    std::random_device rd_new;
                    std::mt19937 gen_new(rd_new());
                    // Check size again before distribution
                    if (newResult.urls.empty()) {
                        std::cerr << "[API] Logic Error: newResult.urls became empty after population." << std::endl;
                        selectedUrl = ""; // Should not happen if population worked
                    }
                    else {
                        std::uniform_int_distribution<size_t> distrib_new(0, newResult.urls.size() - 1);
                        auto it_new = newResult.urls.begin();
                        std::advance(it_new, distrib_new(gen_new));
                        selectedUrl = *it_new;

                        std::cout << "[API] Fresh fetch successful (Page " << pageToFetch << "). Using URL: " << selectedUrl << std::endl;
                    }
                    // Return is handled after the if(needsApiFetch) block now

                }
                else {
                    std::cerr << "[API] Fetch OK but no valid image URLs found in 'data' array on page " << pageToFetch << "." << std::endl;
                    // Cache an empty result to avoid refetching this specific page immediately
                    // Only cache if it's a *new* query or if we want to overwrite existing cache with empty
                    // Option: Only cache empty if it was a cache miss initially
                    if (!apiQueryCache.count(searchQuery)) {
                        std::cout << "[API Cache] Caching empty result for query \"" << searchQuery << "\" after finding no URLs." << std::endl;
                        apiQueryCache[searchQuery] = {}; // Cache empty result placeholder
                        apiQueryCache[searchQuery].totalCount = totalCount; // Still store metadata if known
                        apiQueryCache[searchQuery].pageSize = pageSizeFromApi;
                        apiQueryCache[searchQuery].fetchedPage = pageToFetch;
                    }
                    else {
                        // If cache already existed, maybe don't overwrite with empty? Or mark page as tried?
                        // Current behavior: Existing cache remains, selectedUrl is empty.
                        std::cout << "[API Cache] Not overwriting existing cache for query \"" << searchQuery << "\" despite empty fetch result." << std::endl;
                    }
                    selectedUrl = ""; // Ensure selectedUrl is empty
                    // Return is handled after the if(needsApiFetch) block now
                }
            }
            else {
                // 'data' array not found or empty in a 200 OK response
                std::cout << "[API] 'data' array not found or empty on page " << pageToFetch << " for query: \"" << searchQuery << "\". Total results reported: " << totalCount << std::endl;
                // Cache an empty result similar to above
                if (!apiQueryCache.count(searchQuery)) {
                    std::cout << "[API Cache] Caching empty result for query \"" << searchQuery << "\" due to empty 'data' array." << std::endl;
                    apiQueryCache[searchQuery] = {}; // Cache empty result placeholder
                    apiQueryCache[searchQuery].totalCount = totalCount; // Store metadata
                    apiQueryCache[searchQuery].pageSize = pageSizeFromApi;
                    apiQueryCache[searchQuery].fetchedPage = pageToFetch;
                }
                else {
                    std::cout << "[API Cache] Not overwriting existing cache for query \"" << searchQuery << "\" despite empty 'data' array." << std::endl;
                }
                selectedUrl = ""; // Ensure selectedUrl is empty
                // Return is handled after the if(needsApiFetch) block now
            }
        }
        catch (json::exception& e) {
            std::cerr << "[API] JSON Exception after successful fetch: " << e.what() << std::endl;
            std::cerr << "[API] Response Text (truncated): " << response.text.substr(0, 500) << (response.text.length() > 500 ? "..." : "") << std::endl;
            // Cache empty result similar to API errors
            if (!apiQueryCache.count(searchQuery)) {
                std::cout << "[API Cache] Caching empty result for query \"" << searchQuery << "\" after JSON parsing error." << std::endl;
                apiQueryCache[searchQuery] = {}; // Cache empty result placeholder
            }
            selectedUrl = "";
            // Return is handled after the if(needsApiFetch) block now
        }
    } // End of if(needsApiFetch)

    // --- 4. Return the selected URL (either from cache or fresh fetch) ---
    if (selectedUrl.empty()) {
        std::cerr << "[API Result] No valid URL could be obtained or selected for card query: \"" << searchQuery << "\"" << std::endl;
    }
    return selectedUrl;
}

// --- NEW: Downloads image data from a URL ---
std::vector<unsigned char> TextureManager::downloadImageData(const std::string& imageUrl) {
    std::cout << "Downloading image from: " << imageUrl << std::endl;
    cpr::Response response = cpr::Get(cpr::Url{imageUrl});

    if (response.status_code == 200 && !response.text.empty()) {
        // Convert the downloaded string data to a vector of unsigned char
        return std::vector<unsigned char>(response.text.begin(), response.text.end());
    }

    std::cerr << "Failed to download image. Status code: " << response.status_code
              << ", URL: " << imageUrl << ", Error: " << response.error.message << std::endl;
    if (response.text.empty() && response.status_code == 200) {
        std::cerr << "Downloaded image data was empty." << std::endl;
    }
    return {}; // Return empty vector on failure
}

// --- NEW: Loads texture from image data in memory ---
GLuint TextureManager::loadTextureFromMemory(const std::vector<unsigned char>& imageData, const std::string& cacheKey) {
    // Check cache again (might have been loaded concurrently, though less likely here)
    if (textureMap.count(cacheKey)) {
        return textureMap[cacheKey];
    }

    if (imageData.empty()) {
        std::cerr << "Error: Image data buffer is empty for key: " << cacheKey << std::endl;
        return 0;
    }

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true); // Already set globally, but good practice
    // Use stbi_load_from_memory
    unsigned char* data = stbi_load_from_memory(imageData.data(), static_cast<int>(imageData.size()),
                                              &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Failed to load texture from memory for key: " << cacheKey << std::endl;
        std::cerr << "STB Failure Reason: " << stbi_failure_reason() << std::endl;
        return 0;
    }

     std::cout << "Successfully loaded texture from memory: " << cacheKey
              << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;

    GLuint textureID = 0;
    glGenTextures(1, &textureID);
     if (textureID == 0) {
         std::cerr << "OpenGL Error: Failed to generate texture ID for memory texture " << cacheKey << std::endl;
         stbi_image_free(data);
         return 0;
    }
    glBindTexture(GL_TEXTURE_2D, textureID);

    // Determine format (same as file loading)
    GLenum internalFormat = GL_RGB;
    GLenum dataFormat = GL_RGB;
    int bytesPerPixel = 3;
    if (channels == 4) {
        internalFormat = GL_RGBA;
        dataFormat = GL_RGBA;
        bytesPerPixel = 4;
    } else if (channels == 3) {
        internalFormat = GL_RGB;
        dataFormat = GL_RGB;
        bytesPerPixel = 3;
    } else if (channels == 1) {
        internalFormat = GL_RED;
        dataFormat = GL_RED;
        bytesPerPixel = 1;
    } else {
        std::cerr << "Error: Unsupported number of channels (" << channels << ") for memory texture: " << cacheKey << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return 0;
    }

    // --- FIX: Set Pixel Unpack Alignment ---
    if ((width * bytesPerPixel) % 4 != 0) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        // std::cout << "[Debug] Setting UNPACK_ALIGNMENT to 1 for " << cacheKey << std::endl;
    }
    else {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Set texture parameters (same as file loading)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

     // Check for OpenGL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Texture Error (Memory: " << cacheKey << "): " << err << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, 0);
        return 0;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Store in cache using the provided cacheKey (which should be the URL)
    textureMap[cacheKey] = textureID;
    std::cout << "Texture stored in map: " << cacheKey << " -> ID: " << textureID << std::endl;
    return textureID;
}

// In TextureManager.cpp
GLuint TextureManager::generateCardTexture(const Card& card) {
    GLuint textureID = 0;

    // --- 1. Get Image URL (Uses apiUrlCache internally) ---
    std::string imageUrl = fetchCardImageUrl(card);

    if (imageUrl.empty()) {
        std::cerr << "[Generate Tex] Failed to get image URL for " << card.getPokemonName() << "." << std::endl;
        // Go directly to fallbacks if URL fetch fails
    }
    else {
        // --- 2. Check In-Memory Texture Cache (textureMap) ---
        textureID = getTexture(imageUrl); // Checks if texture is already loaded ON GPU
        if (textureID != 0) {
            // std::cout << "[Generate Tex] Cache hit (In-Memory GPU Texture): " << imageUrl << std::endl;
            return textureID; // Already loaded in this session, done.
        }

        // --- 3. Check Persistent Disk Cache ---
        std::string localPath = getCacheFilename(imageUrl);
        if (std::filesystem::exists(localPath)) {
            // std::cout << "[Generate Tex] Cache hit (Disk): " << localPath << std::endl;
            // Load texture directly from the local file
            // Use loadTexture, which handles file loading AND adds to textureMap
            textureID = loadTexture(localPath);
            if (textureID != 0) {
                // Important: Need to map the ORIGINAL URL to this ID too in textureMap
                // so future calls using the URL hit the memory cache directly.
                textureMap[imageUrl] = textureID;
                return textureID;
            }
            else {
                std::cerr << "[Generate Tex] Error loading texture from disk cache file: " << localPath << std::endl;
                // Proceed to download as if cache file was bad/corrupt
            }
        }

        // --- 4. Download, Save to Disk, and Load ---
        // std::cout << "[Generate Tex] Cache miss (Disk). Attempting download: " << imageUrl << std::endl;
        std::vector<unsigned char> imageData = downloadImageData(imageUrl);
        if (!imageData.empty()) {
            // --- SAVE TO DISK ---
            std::ofstream outFile(localPath, std::ios::binary);
            if (outFile) {
                outFile.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
                outFile.close();
                // std::cout << "[Generate Tex] Saved downloaded image to disk cache: " << localPath << std::endl;
            }
            else {
                std::cerr << "[Generate Tex] Error opening file for writing disk cache: " << localPath << std::endl;
            }
            // --- LOAD FROM MEMORY ---
            // This call will also add it to textureMap using imageUrl as key
            textureID = loadTextureFromMemory(imageData, imageUrl);
            if (textureID != 0) {
                return textureID; // Success
            }
            else {
                std::cerr << "[Generate Tex] Failed to load texture from memory after download." << std::endl;
            }
        }
        else {
            std::cerr << "[Generate Tex] Failed to download image data for URL: " << imageUrl << std::endl;
        }
    } // End of else block (imageUrl was not empty)


    // --- 5. Fallbacks (Only if everything above failed) ---
    if (textureID == 0) {
        std::string templatePath = "textures/cards/card_template.png";
        std::cout << "[Fallback] API/Download/Cache failed for " << card.getPokemonName()
            << ". Falling back to template: " << templatePath << std::endl;
        textureID = loadTexture(templatePath); // Use regular loadTexture for file
    }
    if (textureID == 0) {
        std::string placeholderPath = "textures/pokemon/placeholder.png";
        std::cerr << "[Fallback] Failed to load template texture. Falling back to placeholder: "
            << placeholderPath << std::endl;
        textureID = loadTexture(placeholderPath);
    }
    if (textureID == 0) {
        std::cerr << "FATAL: generateCardTexture failed to load ANY texture (API, caches, fallbacks) for "
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

    GLint modeLoc = glGetUniformLocation(cardShader, "renderMode");
    if (modeLoc != -1) {
        glUniform1i(modeLoc, shaderRenderMode); // Pass the current mode
    }
    else {
        // Warn only once maybe to avoid spamming console
        static bool warned = false;
        if (!warned) {
            std::cerr << "Warning: Uniform 'renderMode' not found in card shader (ID: " << cardShader << ")" << std::endl;
            warned = true;
        }
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

    // Determine if it's a holo card (adjust rarity values as needed)
    int rarityValue = getRarityValue(card.getRarity());
    bool isHoloType = (rarityValue >= 1); // Treat Reverse Holo and up as "holo"

    GLint modeLoc = glGetUniformLocation(holoShader, "renderMode");
    if (modeLoc == -1) {
        // Ensure this warning is active and not commented out
        static bool warnedMode = false;
        if (!warnedMode) {
            std::cerr << "FATAL WARNING: Uniform 'renderMode' not found in holo shader (ID: " << holoShader << "). Mode switching will FAIL." << std::endl;
            warnedMode = true;
        }
    }
    else {
        glUniform1i(modeLoc, holoDebugRenderMode);

        // *** Add Error Check ***
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL Error *after* setting renderMode uniform: " << err << std::endl;
        }
    }

    // Check for GL errors after glUseProgram
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL Error after glUseProgram(holoShader=" << holoShader << "): " << err << std::endl;
        // Potentially return or handle error
    }

    // *** Set ONLY the horizontalShift uniform ***
    //GLint shiftLoc = glGetUniformLocation(holoShader, "horizontalShift");
    //if (shiftLoc != -1) {
    //    glUniform1f(shiftLoc, testHorizontalShift); // Use the member variable
    //}
    //else {
    //    std::cerr << "Warning: Uniform 'horizontalShift' not found in holo shader." << std::endl;
    //}

    // Set time uniform for animated effects
    GLint timeLoc = glGetUniformLocation(holoShader, "time");
    if (timeLoc == -1) {
        std::cerr << "Warning: Uniform 'time' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    }
    else {
        glUniform1f(timeLoc, time);
    }

    // Pass cardRarity - CRUCIAL for conditional logic in shader
    GLint rarityLoc = glGetUniformLocation(holoShader, "cardRarity");
    if (rarityLoc != -1) {
        glUniform1i(rarityLoc, getRarityValue(card.getRarity())); // Pass the numeric rarity value
    }
    else { /* Warn */ }

    // --- NEW: Pass Artwork Rectangle Uniforms ---
    GLint artworkMinLoc = glGetUniformLocation(holoShader, "artworkRectMin");
    GLint artworkMaxLoc = glGetUniformLocation(holoShader, "artworkRectMax");
    if (artworkMinLoc != -1) {
        glUniform2fv(artworkMinLoc, 1, glm::value_ptr(artworkRectMin));
    }
    else { std::cerr << "Warning: Uniform 'artworkRectMin' not found in holo shader." << std::endl; }
    if (artworkMaxLoc != -1) {
        glUniform2fv(artworkMaxLoc, 1, glm::value_ptr(artworkRectMax));
    }
    else { std::cerr << "Warning: Uniform 'artworkRectMax' not found in holo shader." << std::endl; }
    // --- END NEW ---

    GLint lightDirLoc = glGetUniformLocation(holoShader, "lightDir");
    if (lightDirLoc != -1) {
        // Example light direction - make this configurable maybe
        glm::vec3 lightDirection = glm::normalize(glm::vec3(0.5f, 1.0f, 0.8f));
        glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDirection));
    }



    // Set other holo effect parameters
    //GLint intensityLoc = glGetUniformLocation(holoShader, "holoIntensity");
    //GLint typeLoc = glGetUniformLocation(holoShader, "cardType");

    //if (intensityLoc == -1) {
    //    std::cerr << "Warning: Uniform 'holoIntensity' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    //}
    //if (typeLoc == -1) {
    //    std::cerr << "Warning: Uniform 'cardType' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    //}


    float intensity = 1.0f;
    if (card.getRarity() == "holo") intensity = 1.5f;
    else if (card.getRarity() == "reverse") intensity = 1.2f;
    else if (card.getRarity() == "ex") intensity = 2.0f;
    else if (card.getRarity() == "full art") intensity = 2.5f;

    //if (intensityLoc != -1) {
    //    glUniform1f(intensityLoc, intensity);
    //}
    //if (typeLoc != -1) {
    //    glUniform1i(typeLoc, getTypeValue(card.getPokemonType()));
    //}

    //GLint modeLoc = glGetUniformLocation(holoShader, "renderMode");
    //if (modeLoc != -1) {
    //    glUniform1i(modeLoc, shaderRenderMode); // Pass the current mode
    //}
    //else {
    //    static bool warned = false;
    //    if (!warned) {
    //        std::cerr << "Warning: Uniform 'renderMode' not found in holo shader (ID: " << holoShader << ")" << std::endl;
    //        warned = true;
    //    }
    //}

    // Bind Rainbow Gradient (Unit 3)
    GLint rainbowGradLoc = glGetUniformLocation(holoShader, "rainbowGradient");
    if (rainbowGradLoc != -1) {
        glActiveTexture(GL_TEXTURE3); // Activate texture unit 3
        glBindTexture(GL_TEXTURE_1D, rainbowGradientTextureID); // Bind the generated texture
        glUniform1i(rainbowGradLoc, 3); // Tell sampler to use unit 3
    }
    else {
        std::cerr << "Warning: Uniform 'rainbowGradient' sampler not found in holo shader." << std::endl;
    }

    // Bind Normal/Height Map (Unit 4)
    GLint normalMapLoc = glGetUniformLocation(holoShader, "normalMap");
    if (normalMapLoc != -1) {
        glActiveTexture(GL_TEXTURE4); // Activate texture unit 4
        glBindTexture(GL_TEXTURE_2D, holoNormalMapTextureID); // Bind the loaded texture
        glUniform1i(normalMapLoc, 4); // Tell sampler to use unit 4
    }
    else {
        std::cerr << "Warning: Uniform 'normalMap' sampler not found in holo shader." << std::endl;
    }

    // --- Set New Uniforms (Optional for Phase 2 test, but good practice) ---
     //Get locations now, even if we don't set meaningful values yet.

    GLint parallaxScaleLoc = glGetUniformLocation(holoShader, "parallaxHeightScale");
     if (parallaxScaleLoc != -1) glUniform1f(parallaxScaleLoc, 0.03f); // Set default

    GLint anisoDirLoc = glGetUniformLocation(holoShader, "anisotropyDirection");
     if (anisoDirLoc != -1) glUniform2f(anisoDirLoc, 1.0f, 0.0f); // Set default

    glActiveTexture(GL_TEXTURE0);

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
    // --- Generate 1D Rainbow Gradient Texture ---
    std::cout << "Generating 1D Rainbow Gradient Texture..." << std::endl;
    const int gradientWidth = 256;
    std::vector<unsigned char> gradientData(gradientWidth * 3); // RGB format
    for (int i = 0; i < gradientWidth; ++i) {
        float hue = (float)i / (float)(gradientWidth - 1); // Ensure reaches 1.0
        // Simple HSV to RGB (Sat=1, Val=1)
        float r, g, b;
        float h = hue * 6.0f;
        int sector = floor(h);
        float f = h - sector;
        float p = 0.0f; float q = 1.0f - f; float t = f;
        switch (sector) {
        case 0: r = 1; g = t; b = p; break; case 1: r = q; g = 1; b = p; break;
        case 2: r = p; g = 1; b = t; break; case 3: r = p; g = q; b = 1; break;
        case 4: r = t; g = p; b = 1; break; default:r = 1; g = p; b = q; break;
        }
        gradientData[i * 3 + 0] = static_cast<unsigned char>(r * 255.0f);
        gradientData[i * 3 + 1] = static_cast<unsigned char>(g * 255.0f);
        gradientData[i * 3 + 2] = static_cast<unsigned char>(b * 255.0f);
    }

    // --- Generate Texture Object ---
    glGenTextures(1, &rainbowGradientTextureID);
    if (rainbowGradientTextureID == 0) {
        std::cerr << "Error: Failed to generate Rainbow Gradient Texture ID." << std::endl;
    }
    else {
        glBindTexture(GL_TEXTURE_1D, rainbowGradientTextureID);
        // Set alignment for 1D RGB data (3 bytes per pixel)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // Important for non-multiple-of-4 row sizes (though less critical for 1D)
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB, gradientWidth, 0, GL_RGB, GL_UNSIGNED_BYTE, gradientData.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // Reset to default

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Repeat or Clamp? Repeat often better for shifting effects.
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_1D, 0);

        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "OpenGL Error after generating Rainbow Gradient Texture: " << err << std::endl;
        }
        else {
            std::cout << "Successfully generated 1D Rainbow Gradient Texture. ID: " << rainbowGradientTextureID << std::endl;
        }
    }
    // --- End Rainbow Gradient ---
    // --- Load Normal Map (with Height in Alpha) ---
    std::string normalMapPath = "textures/cards/NormalMap.png"; // <<< Make sure this file exists!
    std::cout << "Loading Holo Normal/Height Map: " << normalMapPath << std::endl;
    holoNormalMapTextureID = loadTexture(normalMapPath); // Use existing loader

    if (holoNormalMapTextureID == 0) {
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::cerr << "!!! ERROR: Failed to load holo normal map texture: " << normalMapPath << std::endl;
        std::cerr << "!!! Parallax and Normal Mapping effects will likely fail." << std::endl;
        std::cerr << "!!! Ensure the file exists and is a valid RGBA image." << std::endl;
        std::cerr << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        // Consider loading a flat placeholder if loading fails, e.g.:
        // holoNormalMapTextureID = loadTexture("textures/flat_normal_placeholder.png");
    }
    else {
        std::cout << "Successfully loaded Holo Normal/Height Map. ID: " << holoNormalMapTextureID << std::endl;
        // Note: loadTexture already adds it to textureMap cache if needed
    }
    // --- End Normal Map ---
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

std::string TextureManager::getCacheFilename(const std::string& url) const {
    // Use a hash of the URL for a unique, fixed-length filename.
    // Alternatively, sanitize the URL string (replace special chars), but hashing is safer.
    std::size_t urlHash = std::hash<std::string>{}(url);
    std::stringstream ss;
    // Use hex representation of the hash. Add a common extension.
    ss << imageCacheDirectory << std::hex << urlHash << ".png_cache";
    return ss.str();
}

GLuint TextureManager::generateProceduralOverlayTexture(Card& card) {
    std::string rarity = card.getRarity();
    std::string type = card.getPokemonType();
    std::string typeKey = type;
    std::replace(typeKey.begin(), typeKey.end(), ' ', '_');

    std::string cacheKey;
    bool useGlossyOverlay = (rarity == "holo" || rarity == "ex" || rarity == "full art");

    if (useGlossyOverlay) {
        cacheKey = "gloss_overlay_" + typeKey; // Simpler key, no variation level needed
    }
    else {
        cacheKey = "lsys_overlay_" + rarity + "_" + typeKey + "_v5_" + std::to_string(lsystemVariationLevel);
        if (card.getGeneratedOverlayLevel() == lsystemVariationLevel) {
            auto it_lsys = textureMap.find(cacheKey);
            if (it_lsys != textureMap.end()) {
                return it_lsys->second; // Correct variation level already generated and cached
            }
        }
    }

    auto it = textureMap.find(cacheKey);
    if (it != textureMap.end()) {
        if (useGlossyOverlay) card.setGeneratedOverlayLevel(-2); // Use -2 to indicate glossy overlay
        else card.setGeneratedOverlayLevel(lsystemVariationLevel);
        return it->second;
    }

    std::cout << "[Overlay Gen] Cache miss for: " << cacheKey << ". Generating..." << std::endl;

    int texWidth = 1;
    int texHeight = 1;
    GLuint overlayTextureID = 0;
    std::vector<unsigned char> overlayPixelData;

    if (useGlossyOverlay) {
        std::cout << "[Overlay Gen] Generating BLANK (transparent) overlay for " << rarity << "/" << type << "." << std::endl;
        overlayPixelData = { 0, 0, 0, 0 }; // RGBA = Transparent Black
        texWidth = 1;
        texHeight = 1;

    }
    else {
        texWidth = 256; // Restore original size for L-systems
        texHeight = 256;
        std::cout << "[Overlay Gen] Generating L-SYSTEM overlay for " << rarity << "/" << type << "." << std::endl;

        LSystemRenderer renderer(texWidth, texHeight);
        renderer.clearBuffer(glm::vec3(0.0f)); // Clear to transparent black

        LSystem lSystem;
        int iterations = 4;
        float step = 3.0f;
        float angle = 22.5f;
        glm::vec3 startColor = glm::vec3(1.0f);
        int lineThickness = 2;
        int baseNumPasses = 10;
        int passIncrement = 5;

        const glm::vec3 typeBaseColors[12] = {
            glm::vec3(0.9, 0.9, 0.8), glm::vec3(1.0, 0.5, 0.2), glm::vec3(0.3, 0.7, 1.0),
            glm::vec3(0.4, 0.9, 0.4), glm::vec3(1.0, 1.0, 0.3), glm::vec3(0.9, 0.5, 0.9),
            glm::vec3(0.8, 0.6, 0.3), glm::vec3(0.5, 0.5, 0.6), glm::vec3(0.6, 0.4, 0.9),
            glm::vec3(1.0, 0.7, 0.9), glm::vec3(0.7, 0.7, 0.8), glm::vec3(0.6, 0.4, 0.8)
        };
        int typeIndex = getTypeValue(type); // Use existing function
        glm::vec3 defaultColor = (typeIndex >= 0 && typeIndex < 12) ? typeBaseColors[typeIndex] : glm::vec3(0.8f);
        startColor = defaultColor;

        // Default Settings
        lSystem.setAxiom("F");
        lSystem.addRule('F', "F[+F]F[-F]F"); // Default branching
        iterations = 4;
        angle = 25.0f;
        step = 3.0f;
        baseNumPasses = 15;
        passIncrement = 8;
        lineThickness = 1;

        if (rarity == "normal") {
            lSystem.setAxiom("F");
            lSystem.addRule('F', "F[-F][+F]F"); // Simple branching
            iterations = 3;
            angle = glm::linearRand(25.0f, 45.0f);
            step = 4.0f;
            startColor = glm::vec3(0.7f, 0.7f, 0.7f); // Grey color
            baseNumPasses = 10; // Fewer passes for subtle effect
            passIncrement = 3;
            lineThickness = 1;
            std::cout << "[Overlay Gen] Applying Normal rarity L-System settings." << std::endl;
        }
        else if (rarity == "reverse") {
            // Type-specific L-systems for Reverse Holo patterns
            if (type == "Water") {
                lSystem.setAxiom("F");
                lSystem.addRule('F', "F F + [ + F - F - F ] - [ - F + F + F ]");
                angle = 90.0f; step = 4.0f; iterations = 4; baseNumPasses = 10; passIncrement = 5; lineThickness = 2;
                startColor = glm::vec3(0.5, 0.8, 1.0);
            }
            else if (type == "Fire") {
                lSystem.setAxiom("X");
                lSystem.addRule('X', "F[+X][-X]FX"); lSystem.addRule('F', "FF");
                angle = 22.5f; step = 2.5f; iterations = 5; baseNumPasses = 25; passIncrement = 10; lineThickness = 1;
                startColor = glm::vec3(1.0, 0.6, 0.2);
            }
            else if (type == "Grass") {
                lSystem.setAxiom("X");
                lSystem.addRule('X', "F-[[X]+X]+F[+FX]-X"); lSystem.addRule('F', "FF");
                angle = 25.0f; step = 2.0f; iterations = 5; baseNumPasses = 20; passIncrement = 8; lineThickness = 1;
                startColor = glm::vec3(0.5, 0.9, 0.4);
            }
            else if (type == "Lightning") {
                lSystem.setAxiom("F");
                lSystem.addRule('F', "F+F--F+F");
                angle = 60.0f; step = 4.0f; iterations = 3; baseNumPasses = 15; passIncrement = 6; lineThickness = 2;
                startColor = glm::vec3(1.0, 1.0, 0.5);
            }
            else if (type == "Psychic") {
                lSystem.setAxiom("F+F+F+F");
                lSystem.addRule('F', "F+f-FF+F+FF+Ff+FF-f+FF-F-FF-Ff-FFF"); lSystem.addRule('f', "ffffff");
                angle = 90.0f; step = 1.5f; iterations = 2; baseNumPasses = 10; passIncrement = 4; lineThickness = 1;
                startColor = glm::vec3(0.9, 0.6, 1.0);
            }
            else if (type == "Fighting") {
                lSystem.setAxiom("F+F+F+F");
                lSystem.addRule('F', "F+F-F-F+F");
                angle = 90.0f; step = 5.0f; iterations = 3; baseNumPasses = 12; passIncrement = 5; lineThickness = 2;
                startColor = glm::vec3(0.8, 0.5, 0.3);
            }
            else if (type == "Dragon") {
                lSystem.setAxiom("F-G-G");
                lSystem.addRule('F', "F-G+F+G-F"); lSystem.addRule('G', "GG");
                angle = 120.0f; step = 3.0f; iterations = 4; baseNumPasses = 15; passIncrement = 7; lineThickness = 1;
                startColor = glm::vec3(0.6, 0.4, 0.9);
            }
            else if (type == "Darkness") {
                lSystem.setAxiom("F");
                lSystem.addRule('F', "F[+F-F]F[-F+F]F");
                angle = 35.0f; step = 2.8f; iterations = 4; baseNumPasses = 18; passIncrement = 7; lineThickness = 1;
                startColor = glm::vec3(0.6, 0.5, 0.7);
            }
            else if (type == "Metal") {
                lSystem.setAxiom("F+F+F+F");
                lSystem.addRule('F', "FF+F+F+F+FF");
                angle = 90.0f; step = 3.5f; iterations = 3; baseNumPasses = 14; passIncrement = 6; lineThickness = 2;
                startColor = glm::vec3(0.8, 0.8, 0.85);
            }
            else if (type == "Fairy") {
                lSystem.setAxiom("X");
                lSystem.addRule('X', "F[+X]F[-X]+X"); lSystem.addRule('F', "FF");
                angle = 20.0f; step = 2.2f; iterations = 5; baseNumPasses = 22; passIncrement = 9; lineThickness = 1;
                startColor = glm::vec3(1.0, 0.8, 0.9);
            }
            else if (type == "Ghost") {
                lSystem.setAxiom("YF");
                lSystem.addRule('X', "X+YF+"); lSystem.addRule('Y', "-FX-Y");
                angle = 90.0f; step = 3.0f; iterations = 6; baseNumPasses = 16; passIncrement = 6; lineThickness = 1;
                startColor = glm::vec3(0.7, 0.6, 0.9);
            }
            else {
                // Keep default settings for other types if specific rules aren't defined
                startColor = defaultColor;
            }
            // Make Reverse patterns denser/thicker
            baseNumPasses = std::max(20, baseNumPasses + 5);
            passIncrement = std::max(8, passIncrement + 3);
            lineThickness = std::max(lineThickness, 2);
            std::cout << "[Overlay Gen] Applying REVERSE rarity L-System settings (Type: " << type << ")." << std::endl;
        }

        int effectiveNumPasses = std::max(1, baseNumPasses + lsystemVariationLevel * passIncrement);
        std::string lsystemString = lSystem.generate(iterations);
        if (lsystemString.empty()) {
            std::cerr << "Error: L-System generation resulted in empty string for key: " << cacheKey << std::endl;
            return 0;
        }
        renderer.setLineThickness(lineThickness);

        std::cout << "[Overlay Gen] Rendering L-System. Passes: " << effectiveNumPasses << ", Thickness: " << lineThickness << ", Iterations: " << iterations << std::endl;

        std::random_device rd_pass;
        std::mt19937 gen_pass(rd_pass());
        std::uniform_real_distribution<float> dist_pos_x(0.0f, (float)texWidth);
        std::uniform_real_distribution<float> dist_pos_y(0.0f, (float)texHeight);
        std::uniform_real_distribution<float> dist_angle(0.0f, 360.0f);
        std::uniform_real_distribution<float> dist_color_mod(0.7f, 1.3f);

        for (int p = 0; p < effectiveNumPasses; ++p) {
            glm::vec2 passStartPos(dist_pos_x(gen_pass), dist_pos_y(gen_pass));
            float passStartAngle = dist_angle(gen_pass);
            glm::vec3 passColor = startColor;
            if (rarity != "normal") { // Apply color variation only if not normal rarity
                passColor *= dist_color_mod(gen_pass);
            }
            renderer.setParameters(step, angle, glm::clamp(passColor, 0.0f, 1.0f), passStartPos, passStartAngle);
            renderer.render(lsystemString);
        }

        const std::vector<unsigned char>& lsysPixelData = renderer.getPixelData();
        if (lsysPixelData.empty() || lsysPixelData.size() != static_cast<size_t>(texWidth) * texHeight * 4) {
            std::cerr << "Error: L-System rendering failed or produced invalid data for " << cacheKey << std::endl;
            return 0;
        }
        overlayPixelData = lsysPixelData; // Assign data for upload
    }

    glGenTextures(1, &overlayTextureID);
    if (overlayTextureID == 0) {
        std::cerr << "OpenGL Error: Failed to generate texture ID for overlay " << cacheKey << std::endl;
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, overlayTextureID);

    if (useGlossyOverlay) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // Set alignment before uploading (important for non-power-of-4 widths like 1x1)
    // If width * channels (1*4=4) is multiple of 4, use 4. Otherwise use 1.
    if ((texWidth * 4) % 4 == 0) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
    else {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, overlayPixelData.data());

    // Reset alignment to default
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    if (!useGlossyOverlay) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Texture Error (Overlay " << cacheKey << "): " << err << std::endl;
        glDeleteTextures(1, &overlayTextureID); // Clean up failed texture
        glBindTexture(GL_TEXTURE_2D, 0);
        return 0; // Indicate failure
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    textureMap[cacheKey] = overlayTextureID;
    if (useGlossyOverlay) {
        card.setGeneratedOverlayLevel(-2); // Mark as glossy overlay
    }
    else {
        card.setGeneratedOverlayLevel(lsystemVariationLevel); // Mark with L-System level
    }
    std::cout << "[Overlay Gen] Generated and cached texture ID " << overlayTextureID << " for: " << cacheKey << std::endl;

    return overlayTextureID;
}

void TextureManager::cycleShaderMode() {
    shaderRenderMode = (shaderRenderMode + 1) % 3; // Cycle through 0, 1, 2
    std::cout << "Shader Render Mode set to: ";
    switch (shaderRenderMode) {
    case 0: std::cout << "Normal (Base + Overlay)" << std::endl; break;
    case 1: std::cout << "Overlay Only" << std::endl; break;
    case 2: std::cout << "Base Only" << std::endl; break;
    default: std::cout << "Unknown" << std::endl; break; // Should not happen
    }
}

int TextureManager::getShaderRenderMode() const {
    return shaderRenderMode;
}

GLuint TextureManager::getRandomPackPokemonTextureID() const {
    if (packPokemonTextureIDs.empty()) {
        std::cerr << "Warning: No pack Pokemon overlay textures loaded to choose from." << std::endl;
        return 0; // Return 0 (invalid ID) if none are loaded
    }

    // Use a static generator for better randomness over multiple calls
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> distrib(0, packPokemonTextureIDs.size() - 1);

    return packPokemonTextureIDs[distrib(gen)];
}