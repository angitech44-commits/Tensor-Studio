#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <queue>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <SDL3/SDL.h>

struct TileKey {
    int z, x, y;
    int layer; // 0 = Satellite, 1 = Scritte (Labels)
    bool operator==(const TileKey& o) const { 
        return z == o.z && x == o.x && y == o.y && layer == o.layer; 
    }
};

struct TileKeyHash {
    size_t operator()(const TileKey& k) const {
        return std::hash<int>()(k.z) ^ 
              (std::hash<int>()(k.x) << 8) ^ 
              (std::hash<int>()(k.y) << 16) ^ 
              (std::hash<int>()(k.layer) << 24);
    }
};

enum class TileState { NotLoaded, Downloading, ReadyForUpload, Uploaded, Failed };

struct Tile {
    TileState state = TileState::NotLoaded;
    SDL_Texture* texture = nullptr;
    std::vector<unsigned char> rawPixels; 
    int width = 0, height = 0;
};

class TileManager {
public:
    TileManager(int numWorkerThreads = 4);
    ~TileManager();

    void RequestTile(TileKey key);
    void ProcessCompletedDownloads(SDL_Renderer* renderer);
    SDL_Texture* GetTexture(TileKey key);

private:
    void WorkerLoop();
    void DownloadAndDecode(TileKey key);
    std::vector<unsigned char> HttpGet(const std::string& url);

    std::unordered_map<TileKey, Tile, TileKeyHash> tiles_;
    std::mutex mutex_;
    
    std::queue<TileKey> downloadQueue_;
    std::mutex queueMutex_;
    std::condition_variable queueCV_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stopWorkers_;
};