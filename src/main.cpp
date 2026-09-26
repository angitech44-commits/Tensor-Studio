#if !defined(__ANDROID__) && !defined(__IPHONEOS__)
#define SDL_MAIN_HANDLED
#endif

#include "ui/Application.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>

int main(int argc, char* argv[]) {
    SDL_Log("TENSORSTUDIO: Avvio main()");
    std::cout << "[1] Entry point reached. Initializing SDL..." << std::endl;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("TENSORSTUDIO CRASH: SDL_Init fallito: %s", SDL_GetError());
        return -1;
    }
    
    SDL_Log("TENSORSTUDIO: SDL Inizializzato. Avvio l'interfaccia...");

    try {
        Application app("TensorStudio", 1280, 720);
        app.Run();
    } catch (const std::exception& e) {
        SDL_Log("TENSORSTUDIO CRASH INTERNO: %s", e.what());
    }

    SDL_Quit();
    return 0;
}