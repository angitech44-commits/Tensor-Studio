#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <cstdint>
#include "../utils/ThreadSafeQueue.hpp"

class UDPReceiver {
private:
    std::atomic<bool> isRunning{false};
    std::thread receiverThread;
    
#ifdef _WIN32
    uint64_t socketFd = ~0; // INVALID_SOCKET equivalente
#else
    int socketFd = -1;
#endif

    // Riferimento alla coda dove butteremo i byte grezzi ricevuti
    ThreadSafeQueue<std::vector<uint8_t>>& packetQueue;

    void ReceiveLoop();

public:
    explicit UDPReceiver(ThreadSafeQueue<std::vector<uint8_t>>& queue);
    ~UDPReceiver();

    bool Start(int port);
    void Stop();
    bool IsRunning() const { return isRunning; }
};