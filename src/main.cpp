#define SDL_MAIN_HANDLED
#include "ui/Application.hpp"
#include <SDL3/SDL.h>
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "[1] Entry point reached. Initializing SDL..." << std::endl;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "[CRITICAL ERROR] SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }
    std::cout << "[2] SDL initialized successfully." << std::endl;

    try {
        std::cout << "[3] Building Application..." << std::endl;
        Application app("TensorStudio", 1280, 720);
        
        std::cout << "[4] Running Application::Run()..." << std::endl;
        app.Run();
    } catch (const std::exception& e) {
        std::cerr << "[EXCEPTION CAUGHT] " << e.what() << std::endl;
    }

    std::cout << "[5] Clean exit. Shutting down..." << std::endl;
    SDL_Quit();
    return 0;
}