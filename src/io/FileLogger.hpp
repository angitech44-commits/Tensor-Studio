#pragma once
#include <string>
#include <fstream>
#include <mutex>

class FileLogger {
private:
    std::ofstream file;
    std::mutex logMutex;

public:
    FileLogger() = default;
    ~FileLogger() { Close(); }

    bool Open(const std::string& path) {
        std::lock_guard<std::mutex> lock(logMutex);
        file.open(path, std::ios::out | std::ios::trunc);
        return file.is_open();
    }

    void WriteLine(const std::string& data) {
        std::lock_guard<std::mutex> lock(logMutex);
        if (file.is_open()) {
            file << data << "\n";
            file.flush(); // Forza la scrittura su disco per evitare perdite se salta la corrente
        }
    }

    void Close() {
        std::lock_guard<std::mutex> lock(logMutex);
        if (file.is_open()) {
            file.close();
        }
    }

    bool IsOpen() const { 
        return file.is_open(); 
    }
};