#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <functional>
#include <vector>

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

    // Aggiunto il parametro filters con i file JSON di default per non rompere il codice esistente
    static void OpenFile(SDL_Window* window, std::function<void(const std::string&)> onResult, 
                         const std::vector<SDL_DialogFileFilter>& filters = { { "JSON Files", "json" } }) {
        auto* cb = new std::function<void(const std::string&)>(std::move(onResult));
        
        // Usiamo data() e size() del vector per passarli a SDL3
        SDL_ShowOpenFileDialog(DialogCallback, cb, window, filters.data(), filters.size(), nullptr, false);
    }

    static void SaveFile(SDL_Window* window, const std::string& defaultFileName, std::function<void(const std::string&)> onResult,
                         const std::vector<SDL_DialogFileFilter>& filters = { { "JSON Files", "json" } }) {
        auto* cb = new std::function<void(const std::string&)>(std::move(onResult));
        
        const char* defaultPath = defaultFileName.empty() ? nullptr : defaultFileName.c_str();
        SDL_ShowSaveFileDialog(DialogCallback, cb, window, filters.data(), filters.size(), defaultPath);
    }
};