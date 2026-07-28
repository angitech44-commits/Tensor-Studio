#include "TileManager.hpp"
#include <curl/curl.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>

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
        if (pair.second.texture) SDL_DestroyTexture(pair.second.texture);
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
            
            tile.texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, tile.width, tile.height);
            SDL_UpdateTexture(tile.texture, nullptr, tile.rawPixels.data(), tile.width * 4);
            SDL_SetTextureScaleMode(tile.texture, SDL_SCALEMODE_LINEAR);
            
            tile.rawPixels.clear();
            tile.rawPixels.shrink_to_fit();
            tile.state = TileState::Uploaded;
        }
    }
}

SDL_Texture* TileManager::GetTexture(TileKey key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = tiles_.find(key);
    if (it != tiles_.end() && it->second.state == TileState::Uploaded) {
        return it->second.texture;
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

    int w, h, ch;
    unsigned char* pixels = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &w, &h, &ch, 4);
    if (!pixels) {
        std::lock_guard<std::mutex> lock(mutex_);
        tiles_[key].state = TileState::Failed;
        return;
    }

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

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) buffer.clear();
    return buffer;
}