#pragma once
#include <string>

class Application {
private:
    std::string title;
    int width;
    int height;
    bool isRunning;

    void Update();
    void Render();

public:
    Application(const std::string& windowTitle = "TensorStudio", int winWidth = 1280, int winHeight = 720);
    ~Application();

    void Run();
    void Close();
};