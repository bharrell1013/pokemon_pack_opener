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
#include <glm/gtc/random.hpp>

// --- Dependencies for API ---
#include <cpr/cpr.h>         // For HTTP requests
#include <nlohmann/json.hpp> // For JSON parsing
using json = nlohmann::json; // Alias for convenience
// --- End Dependencies ---

// Define the necessary headers for image loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

TextureManager::TextureManager() : cardShader(0), holoShader(0), currentShader(0) {
    // Ensure OpenGL context is available before initializing shaders
    // This should be guaranteed by the call order in Application::initialize
    initializeShaders();
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

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Or GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // Or GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Determine texture format
    GLenum internalFormat = GL_RGB;
    GLenum dataFormat = GL_RGB;
    if (channels == 4) {
        internalFormat = GL_RGBA;
        dataFormat = GL_RGBA;
    } else if (channels == 3) {
        internalFormat = GL_RGB;
        dataFormat = GL_RGB;
    } else if (channels == 1) {
        internalFormat = GL_RED; // Treat single channel as RED
        dataFormat = GL_RED;
        std::cout << "Warning: Loading texture " << pathOrUrl << " as single channel (GL_RED)." << std::endl;
    } else {
        std::cerr << "Error: Unsupported number of channels (" << channels << ") for texture: " << pathOrUrl << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID); // Clean up generated ID
        return 0;
    }

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

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
        typeQueryPart = " types:" + cardType;
    }
    else if (cardType == "Normal" || cardType == "Colorless") { // Group Normal/Colorless as Colorless type in API
        typeQueryPart = " types:Colorless";
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

    // Set texture parameters (same as file loading)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Determine format (same as file loading)
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
    } else {
        std::cerr << "Error: Unsupported number of channels (" << channels << ") for memory texture: " << cacheKey << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return 0;
    }

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

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

    GLint modeLoc = glGetUniformLocation(holoShader, "renderMode");
    if (modeLoc != -1) {
        glUniform1i(modeLoc, shaderRenderMode); // Pass the current mode
    }
    else {
        static bool warned = false;
        if (!warned) {
            std::cerr << "Warning: Uniform 'renderMode' not found in holo shader (ID: " << holoShader << ")" << std::endl;
            warned = true;
        }
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

std::string TextureManager::getCacheFilename(const std::string& url) const {
    // Use a hash of the URL for a unique, fixed-length filename.
    // Alternatively, sanitize the URL string (replace special chars), but hashing is safer.
    std::size_t urlHash = std::hash<std::string>{}(url);
    std::stringstream ss;
    // Use hex representation of the hash. Add a common extension.
    ss << imageCacheDirectory << std::hex << urlHash << ".png_cache";
    return ss.str();
}

GLuint TextureManager::generateProceduralOverlayTexture(const Card& card) {
    // --- 1. Generate Cache Key ---
    // Key based on unique card properties affecting the overlay
    std::string cacheKey = "lsys_overlay_" + card.getRarity() + "_" + card.getPokemonType() + "_v3";
    // Optional: Add more factors like a version number if you change L-systems later
    // cacheKey += "_v1";

    // --- 2. Check Cache ---
    auto it = textureMap.find(cacheKey);
    if (it != textureMap.end()) {
        // std::cout << "[Overlay Gen] Cache hit for: " << cacheKey << std::endl;
        return it->second; // Return cached texture ID
    }

    std::cout << "[Overlay Gen] Cache miss for: " << cacheKey << ". Generating..." << std::endl;

    // --- 3.1 Determine Texture Dimensions and Create Renderer EARLY ---
    int texWidth = 256;
    int texHeight = 256;
    LSystemRenderer renderer(texWidth, texHeight); // <<< CREATE RENDERER HERE
    renderer.clearBuffer(glm::vec3(0.0f)); // <<< CLEAR BUFFER ONCE HERE

    // --- 3.2 Setup L-System based on Rarity/Type ---
    LSystem lSystem;
    int iterations = 4; // Default iterations
    float step = 3.0f;
    float angle = 22.5f;
    glm::vec3 startColor = glm::vec3(1.0f); // Default white overlay
    glm::vec2 startPos = glm::vec2(texWidth / 2.0f, texHeight / 2.0f); // Start at center
    float startAngle = 90.0f; // Start facing up
	int numPasses = 1; // Default to 1 pass
    int lineThickness = 2; // Default thickness

    std::string rarity = card.getRarity();
    std::string type = card.getPokemonType();

    static const glm::vec3 typeBasedHoloColors[] = {
        glm::vec3(1.0f, 0.0f, 0.0f), // Red
        glm::vec3(0.0f, 1.0f, 0.0f), // Green
        glm::vec3(0.0f, 0.0f, 1.0f)  // Blue
        // You can add additional colors if needed.
    };

    if (rarity == "normal") {
        // Simple deformation - e.g., subtle wavy lines or branching
        iterations = 3;
        lSystem.setAxiom("F");
        lSystem.addRule('F', "F[-F][+F]F"); // More symmetrical branching maybe? Or F[+F]F[-F]F
        angle = glm::linearRand(25.0f, 45.0f); // Widen angle variation
        step = 4.0f; // Smaller step can make denser patterns
        startColor = glm::vec3(0.7f, 0.7f, 0.7f); // Keep greyish
        // --- NEW: Random Start Angle & Multiple Passes ---
        numPasses = 15; // Set number of passes for normal
		lineThickness = 1; // Set line thickness for normal

        // Generate the L-System string ONCE if rules/iterations are fixed for all passes
        std::string lsystemStringPass = lSystem.generate(iterations);
        if (lsystemStringPass.empty()) { /* handle error, return 0 */ }

        for (int p = 0; p < numPasses; ++p) {
            startPos = glm::vec2(glm::linearRand(texWidth * 0.1f, texWidth * 0.9f), // Widen start area
                glm::linearRand(texHeight * 0.1f, texHeight * 0.9f));
            startAngle = glm::linearRand(0.0f, 360.0f);
            glm::vec3 passColor = startColor * glm::linearRand(0.7f, 1.0f);

            // Set parameters FOR THIS PASS
            renderer.setParameters(step, angle, passColor, startPos, startAngle);
            // Render THIS pass onto the existing buffer
            renderer.render(lsystemStringPass); // Assumes render doesn't clear buffer
        }
        // --- End Multiple Passes ---
    }
    else if (rarity == "reverse" || rarity == "holo") {
        // Dense, overlapping, colored pattern for full coverage holo
        iterations = 5; // More iterations needed for density
        // Rule for dense branching/filling
        lSystem.setAxiom("X");
        lSystem.addRule('X', "F[-X][+X]FX"); // Good branching rule
        lSystem.addRule('F', "FF");          // Lines get longer/denser
        angle = glm::linearRand(20.0f, 25.0f); // Consistent small angle
        step = 2.0f; // Small steps for density
        numPasses = 30; // MANY passes from random points
        lineThickness = 2; // Standard thickness

        int typeIndex = getTypeValue(type);
        startColor = (typeIndex >= 0 && typeIndex < 12) ? (typeBasedHoloColors[typeIndex] * 0.7f + 0.3f) : glm::vec3(0.8f);

        std::string lsystemStringPass = lSystem.generate(iterations);
        if (lsystemStringPass.empty()) { /* error */ return 0; }
        renderer.setLineThickness(lineThickness);

        for (int p = 0; p < numPasses; ++p) {
            glm::vec2 startPos(glm::linearRand(0.0f, (float)texWidth),
                glm::linearRand(0.0f, (float)texHeight));
            float startAngle = glm::linearRand(0.0f, 360.0f);
            // Vary color slightly per pass for more visual interest
            glm::vec3 passColor = startColor * glm::linearRand(0.7f, 1.3f);
            renderer.setParameters(step, angle, glm::clamp(passColor, 0.0f, 1.0f), startPos, startAngle);
            renderer.render(lsystemStringPass);
        }

    }
    else if (rarity == "ex" || rarity == "full art") {
        // Generate a "sparkle map" using dots
        iterations = 7; // High iterations for many dots
        lSystem.setAxiom("A");
        // Rules using the '.' dot command, turning randomly
        lSystem.addRule('A', ". F [+A] [-A] F A"); // Draw dot, move, turn, branch
        lSystem.addRule('F', "F"); // Move command (can be short)
        angle = glm::linearRand(45.0f, 135.0f); // Random turns
        step = 1.0; // Very short step just to move between dots
        numPasses = 50; // LOTS of passes/starting points for random dots
        lineThickness = 2; // Make dots 2x2
        startColor = glm::vec3(1.0f); // Draw dots as white

        std::string lsystemStringPass = lSystem.generate(iterations);
        if (lsystemStringPass.empty()) { /* error */ return 0; }
        renderer.setLineThickness(lineThickness);

        for (int p = 0; p < numPasses; ++p) {
            glm::vec2 startPos(glm::linearRand(0.0f, (float)texWidth),
                glm::linearRand(0.0f, (float)texHeight));
            float startAngle = glm::linearRand(0.0f, 360.0f);
            // Color is always white for sparkle map intensity
            renderer.setParameters(step, angle, startColor, startPos, startAngle);
            renderer.render(lsystemStringPass);
        }
    }
        const std::vector<unsigned char>& overlayPixelData = renderer.getPixelData(); // Get data after all passes
        if (overlayPixelData.empty() || overlayPixelData.size() != static_cast<size_t>(texWidth) * texHeight * 4) {
            std::cerr << "Error: L-System rendering failed or produced invalid data for " << cacheKey << std::endl;
            return 0; // Indicate failure
        }

    // --- 7. Upload to OpenGL ---
    GLuint overlayTextureID = 0;
    glGenTextures(1, &overlayTextureID);
    if (overlayTextureID == 0) {
        std::cerr << "OpenGL Error: Failed to generate texture ID for overlay " << cacheKey << std::endl;
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, overlayTextureID);

    // Set texture parameters
    // GL_NEAREST might be good for sharp L-system lines initially
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Use mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // How should the pattern repeat or clamp? Repeat is often good for overlays.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Upload the pixel data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, // Internal format
                 texWidth, texHeight, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, // Format and type of input data
                 overlayPixelData.data());
    glGenerateMipmap(GL_TEXTURE_2D); // Generate mipmaps

    // Check for errors during texture creation
     GLenum err;
     while ((err = glGetError()) != GL_NO_ERROR) {
         std::cerr << "OpenGL Texture Error (Overlay " << cacheKey << "): " << err << std::endl;
         glDeleteTextures(1, &overlayTextureID); // Clean up failed texture
         glBindTexture(GL_TEXTURE_2D, 0);
         return 0; // Indicate failure
     }

    glBindTexture(GL_TEXTURE_2D, 0); // Unbind

    // --- 8. Update Cache ---
    textureMap[cacheKey] = overlayTextureID;
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