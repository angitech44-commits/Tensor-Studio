#include "TileManager.hpp"
#include <curl/curl.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <cmath>

// LA CORREZIONE FONDAMENTALE: Escludiamo Android per evitare chiamate OpenGL Desktop!
#if (defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))) && !defined(__ANDROID__)
    #define IS_DESKTOP_PLATFORM
    #include <SDL3/SDL_opengl.h>
#endif

static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::vector<unsigned char>* buffer = (std::vector<unsigned char>*)userp;
    size_t totalSize = size * nmemb;
    buffer->insert(buffer->end(), (unsigned char*)contents, (unsigned char*)contents + totalSize);
    return totalSize;
}

TileManager::TileManager(int numWorkerThreads) : stopWorkers_(false) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    for (int i = 0; i < numWorkerThreads; i++) {
        workers_.emplace_back(&TileManager::WorkerLoop, this);
    }
}

TileManager::~TileManager() {
    stopWorkers_ = true;
    queueCV_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    curl_global_cleanup();
    
    for (auto& pair : tiles_) {
        if (pair.second.textureID) {
#ifdef IS_DESKTOP_PLATFORM
            GLuint glTex = (GLuint)(intptr_t)pair.second.textureID;
            glDeleteTextures(1, &glTex);
#else
            SDL_DestroyTexture((SDL_Texture*)pair.second.textureID);
#endif
        }
    }
}

void TileManager::RequestTile(TileKey key) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tiles_.find(key) != tiles_.end()) return;
    
    tiles_[key] = Tile{};
    tiles_[key].state = TileState::Downloading;
    
    {
        std::lock_guard<std::mutex> qlock(queueMutex_);
        downloadQueue_.push(key);
    }
    queueCV_.notify_one();
}

void TileManager::ProcessCompletedDownloads(SDL_Renderer* renderer) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : tiles_) {
        Tile& tile = pair.second;
        if (tile.state == TileState::ReadyForUpload && !tile.rawPixels.empty()) {
            
#ifdef IS_DESKTOP_PLATFORM
            GLuint glTex;
            glGenTextures(1, &glTex);
            glBindTexture(GL_TEXTURE_2D, glTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tile.width, tile.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, tile.rawPixels.data());
            glBindTexture(GL_TEXTURE_2D, 0); 
            tile.textureID = (void*)(intptr_t)glTex;
            tile.state = TileState::Uploaded;
#else
            // ORA SIAMO FINALMENTE SU ANDROID!
            if (renderer == nullptr || tile.width <= 0 || tile.height <= 0) {
                tile.state = TileState::Failed;
            } else {
                SDL_Texture* sdlTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, tile.width, tile.height);
                if (sdlTex) {
                    SDL_UpdateTexture(sdlTex, nullptr, tile.rawPixels.data(), tile.width * 4);
                    SDL_SetTextureScaleMode(sdlTex, SDL_SCALEMODE_LINEAR);
                    tile.textureID = (void*)sdlTex;
                    tile.state = TileState::Uploaded;
                } else {
                    SDL_Log("TENSORSTUDIO ERROR: Creazione Texture Mappa fallita - %s", SDL_GetError());
                    tile.state = TileState::Failed;
                }
            }
#endif
            
            tile.rawPixels.clear();
            tile.rawPixels.shrink_to_fit();
        }
    }
}

void* TileManager::GetTexture(TileKey key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tiles_.find(key);
    if (it != tiles_.end() && it->second.state == TileState::Uploaded) {
        return it->second.textureID;
    }
    return nullptr;
}

void* TileManager::GetTextureWithFallback(TileKey key, int maxFallbackDepth, float& u0, float& v0, float& u1, float& v1) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (int dz = 0; dz <= maxFallbackDepth; ++dz) {
        if (key.z - dz < 0) break; 
        
        int scale = 1 << dz; 
        TileKey checkKey = {
            key.z - dz,
            key.x / scale,
            key.y / scale,
            key.layer
        };
        
        auto it = tiles_.find(checkKey);
        if (it != tiles_.end() && it->second.state == TileState::Uploaded) {
            float uv_scale = 1.0f / scale;
            u0 = (key.x % scale) * uv_scale;
            v0 = (key.y % scale) * uv_scale;
            u1 = u0 + uv_scale;
            v1 = v0 + uv_scale;
            return it->second.textureID;
        }
    }
    return nullptr;
}

void TileManager::WorkerLoop() {
    while (!stopWorkers_) {
        TileKey key;
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            queueCV_.wait(lock, [this] { return !downloadQueue_.empty() || stopWorkers_; });
            if (stopWorkers_) return;
            key = downloadQueue_.front();
            downloadQueue_.pop();
        }
        DownloadAndDecode(key);
    }
}

void TileManager::DownloadAndDecode(TileKey key) {
    std::string url;
    if (key.layer == 0) {
        url = "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/" 
            + std::to_string(key.z) + "/" + std::to_string(key.y) + "/" + std::to_string(key.x);
    } else {
        url = "https://server.arcgisonline.com/ArcGIS/rest/services/Reference/World_Boundaries_and_Places/MapServer/tile/" 
            + std::to_string(key.z) + "/" + std::to_string(key.y) + "/" + std::to_string(key.x);
    }

    std::vector<unsigned char> fileData = HttpGet(url);
    if (fileData.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        tiles_[key].state = TileState::Failed;
        return;
    }

    int w = 0, h = 0, ch = 0;
    unsigned char* pixels = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &w, &h, &ch, 4);
    if (!pixels) {
        std::lock_guard<std::mutex> lock(mutex_);
        tiles_[key].state = TileState::Failed;
        return;
    }

    // --- EURISTICA ANTI-PLACEHOLDER ESRI ---
    bool isPlaceholder = true;
    unsigned char r_ref = pixels[0];
    unsigned char g_ref = pixels[1];
    unsigned char b_ref = pixels[2];
    
    for (int i = 0; i < w; i++) {
        int idx = i * 4;
        if (std::abs(pixels[idx] - r_ref) > 3 || 
            std::abs(pixels[idx+1] - g_ref) > 3 || 
            std::abs(pixels[idx+2] - b_ref) > 3) {
            isPlaceholder = false;
            break;
        }
    }

    if (isPlaceholder && key.layer == 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        tiles_[key].state = TileState::Failed;
        stbi_image_free(pixels);
        return;
    }
    // ---------------------------------------

    std::lock_guard<std::mutex> lock(mutex_);
    Tile& tile = tiles_[key];
    tile.rawPixels.assign(pixels, pixels + (w * h * 4));
    tile.width = w;
    tile.height = h;
    tile.state = TileState::ReadyForUpload;
    stbi_image_free(pixels);
}

std::vector<unsigned char> TileManager::HttpGet(const std::string& url) {
    std::vector<unsigned char> buffer;
    CURL* curl = curl_easy_init();
    if (!curl) return buffer;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "TensorStudio/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    // Sicurezza Multi-Thread per evitare crash del kernel di rete
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

#ifdef __ANDROID__
    // Bypass per l'ambiente chiuso di Android
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#endif

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    if (res != CURLE_OK) {
        SDL_Log("TENSORSTUDIO CURL ERROR: %s (HTTP %ld)", curl_easy_strerror(res), httpCode);
    }
    
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) buffer.clear();
    return buffer;
}