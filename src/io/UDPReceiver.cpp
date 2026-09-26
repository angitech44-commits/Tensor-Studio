#include "UDPReceiver.hpp"
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib") 
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

UDPReceiver::UDPReceiver(ThreadSafeQueue<std::vector<uint8_t>>& queue) : packetQueue(queue) {}

UDPReceiver::~UDPReceiver() {
    Stop();
}

bool UDPReceiver::Start(int port) {
    if (isRunning) return false;

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[UDP] Errore WSAStartup" << std::endl;
        return false;
    }
#endif

    socketFd = socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
    if (socketFd == ~0) {
#else
    if (socketFd < 0) {
#endif
        std::cerr << "[UDP] Errore creazione socket" << std::endl;
        return false;
    }

    // Riutilizza l'indirizzo immediatamente se riavvii il programma al volo
    int optval = 1;
    setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Ascolta esattamente come lo script Python su "0.0.0.0"
    serverAddr.sin_port = htons(port);

    if (bind(socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[UDP] Errore bind sulla porta " << port << std::endl;
        Stop();
        return false;
    }

    isRunning = true;
    receiverThread = std::thread(&UDPReceiver::ReceiveLoop, this);
    std::cout << "[UDP] Ricevitore avviato con successo sulla porta " << port << std::endl;
    return true;
}

void UDPReceiver::Stop() {
    if (!isRunning) return;
    isRunning = false;

#ifdef _WIN32
    if (socketFd != ~0) {
        closesocket(socketFd);
        socketFd = ~0;
    }
    WSACleanup();
#else
    if (socketFd != -1) {
        close(socketFd);
        socketFd = -1;
    }
#endif

    if (receiverThread.joinable()) {
        receiverThread.join();
    }
    std::cout << "[UDP] Ricevitore fermato." << std::endl;
}

void UDPReceiver::ReceiveLoop() {
    std::vector<uint8_t> buffer(2048); 
    sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    while (isRunning) {
#ifdef _WIN32
        int bytesReceived = recvfrom(socketFd, (char*)buffer.data(), buffer.size(), 0, (struct sockaddr*)&clientAddr, &clientLen);
#else
        int bytesReceived = recvfrom(socketFd, buffer.data(), buffer.size(), 0, (struct sockaddr*)&clientAddr, &clientLen);
#endif

        if (bytesReceived > 0) {
            std::vector<uint8_t> packet(buffer.begin(), buffer.begin() + bytesReceived);
            
            // SPIA DI DEBUG: Stampiamo subito in chiaro esattamente come fa il tuo script Python!
            std::string rawStr(packet.begin(), packet.end());
            std::cout << "[UDP RAW] " << rawStr << std::endl;

            packetQueue.push(std::move(packet));
        } else if (bytesReceived < 0) {
            if (!isRunning) break;
        }
    }
}