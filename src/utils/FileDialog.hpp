#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <functional>

class FileDialog {
public:
    static void SDLCALL DialogCallback(void* userdata, const char* const* filelist, int filter) {
        auto* cb = static_cast<std::function<void(const std::string&)>*>(userdata);
        if (filelist && filelist[0]) {
            (*cb)(std::string(filelist[0]));
        } else {
            (*cb)(""); // Errore o annullamento
        }
        delete cb; // Pulizia heap
    }

    // Passiamo nullptr come window, SDL3 lo aggancerà alla finestra principale automaticamente
    static void OpenFile(SDL_Window* window, std::function<void(const std::string&)> onResult) {
        auto* cb = new std::function<void(const std::string&)>(std::move(onResult));
        SDL_DialogFileFilter filters[1] = { { "JSON Files", "json" } };
        SDL_ShowOpenFileDialog(DialogCallback, cb, window, filters, 1, nullptr, false);
    }

    static void SaveFile(SDL_Window* window, const std::string& defaultFileName, std::function<void(const std::string&)> onResult) {
        auto* cb = new std::function<void(const std::string&)>(std::move(onResult));
        SDL_DialogFileFilter filters[1] = { { "JSON Files", "json" } };
        
        // Passiamo defaultFileName come ultimo argomento a SDL_ShowSaveFileDialog
        const char* defaultPath = defaultFileName.empty() ? nullptr : defaultFileName.c_str();
        SDL_ShowSaveFileDialog(DialogCallback, cb, window, filters, 1, defaultPath);
    }
};