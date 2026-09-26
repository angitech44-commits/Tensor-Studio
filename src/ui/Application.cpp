#include "Application.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_internal.h> 
#include <implot.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_impl_opengl3.h>
#include <map>
#include <cmath>

// Escludiamo esplicitamente Android dal desktop per non innescare OpenGL Core
#if (defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))) && !defined(__ANDROID__)
    #define IS_DESKTOP_PLATFORM
#endif

// --- TEMA GLOBALE TENSORSTUDIO (Dimensioni in pixel scalate per i DPI) ---
static void ApplyTensorStudioTheme(float dpiScale) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // 1. Geometria e arrotondamenti (Moltiplicati per dpiScale)
    style.WindowRounding    = 8.0f * dpiScale;
    style.ChildRounding     = 6.0f * dpiScale;
    style.FrameRounding     = 4.0f * dpiScale;
    style.PopupRounding     = 6.0f * dpiScale;
    style.ScrollbarRounding = 12.0f * dpiScale;
    style.GrabRounding      = 4.0f * dpiScale;

    style.WindowBorderSize  = 1.0f * dpiScale;
    style.FrameBorderSize   = 0.0f;

    // Palette: Grigi
    ImVec4 bgDark         = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
    ImVec4 bgMed          = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
    ImVec4 grayBase       = ImVec4(0.18f, 0.18f, 0.19f, 1.00f);
    
    // Palette: Rosso Cremisi (Feedback vivo)
    ImVec4 redHover       = ImVec4(0.70f, 0.22f, 0.26f, 1.00f); 
    ImVec4 redAccent      = ImVec4(0.60f, 0.18f, 0.22f, 1.00f); 
    ImVec4 redActive      = ImVec4(0.50f, 0.14f, 0.18f, 1.00f); 

    // 2. Sfondi Finestre
    colors[ImGuiCol_WindowBg]           = bgDark;
    colors[ImGuiCol_ChildBg]            = bgMed;
    colors[ImGuiCol_PopupBg]            = ImVec4(0.08f, 0.08f, 0.09f, 0.95f);
    colors[ImGuiCol_TitleBg]            = bgDark;
    colors[ImGuiCol_TitleBgActive]      = grayBase; 
    colors[ImGuiCol_TitleBgCollapsed]   = bgDark;

    // 3. Sfondi Input
    colors[ImGuiCol_FrameBg]            = grayBase;
    colors[ImGuiCol_FrameBgHovered]     = redHover; 
    colors[ImGuiCol_FrameBgActive]      = redActive;

    // 4. Bottoni
    colors[ImGuiCol_Button]             = grayBase;
    colors[ImGuiCol_ButtonHovered]      = redHover; 
    colors[ImGuiCol_ButtonActive]       = redActive;

    // 5. Header
    colors[ImGuiCol_Header]             = grayBase;
    colors[ImGuiCol_HeaderHovered]      = redHover;
    colors[ImGuiCol_HeaderActive]       = redActive;

    // 6. Tabs
    colors[ImGuiCol_Tab]                = grayBase;
    colors[ImGuiCol_TabHovered]         = redHover;
    colors[ImGuiCol_TabActive]          = redAccent;
    colors[ImGuiCol_TabUnfocused]       = bgMed;
    colors[ImGuiCol_TabUnfocusedActive] = grayBase;

    // 7. Elementi Interattivi
    colors[ImGuiCol_CheckMark]          = redAccent;
    colors[ImGuiCol_SliderGrab]         = redAccent;
    colors[ImGuiCol_SliderGrabActive]   = redActive;
    
    colors[ImGuiCol_ScrollbarBg]        = bgDark;
    colors[ImGuiCol_ScrollbarGrab]      = grayBase;
    colors[ImGuiCol_ScrollbarGrabHovered]= redHover;
    colors[ImGuiCol_ScrollbarGrabActive]= redActive;

    colors[ImGuiCol_Separator]          = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]   = redHover;
    colors[ImGuiCol_SeparatorActive]    = redAccent;
    
    colors[ImGuiCol_ResizeGrip]         = grayBase;
    colors[ImGuiCol_ResizeGripHovered]  = redHover;
    colors[ImGuiCol_ResizeGripActive]   = redAccent;
    
    colors[ImGuiCol_TextSelectedBg]     = ImVec4(0.60f, 0.18f, 0.22f, 0.50f); 
    colors[ImGuiCol_DragDropTarget]     = redAccent;
    colors[ImGuiCol_NavHighlight]       = redAccent; 

    // 8. Testo e Bordi
    colors[ImGuiCol_Text]               = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_Border]             = ImVec4(0.25f, 0.25f, 0.26f, 1.00f);
    colors[ImGuiCol_BorderShadow]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
}

Application::Application(const std::string& windowTitle, int winWidth, int winHeight)
    : title(windowTitle), width(winWidth), height(winHeight), isRunning(false) {}

Application::~Application() {
    Close();
}

void Application::Run() {
    SDL_Log("TENSORSTUDIO: Avvio applicazione");
    SDL_Log("TENSORSTUDIO: [DEBUG] Creating SDL window...");
    
#ifdef IS_DESKTOP_PLATFORM
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    
    window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY);
#else
    window = SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_FULLSCREEN);
#endif

    if (!window) {
        SDL_Log("TENSORSTUDIO: [SDL ERROR] Failed to create window: %s", SDL_GetError());
        return;
    }

#ifdef IS_DESKTOP_PLATFORM
    SDL_Log("TENSORSTUDIO: [DEBUG] Creating OpenGL Context...");
    gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); 
#else
    SDL_Log("TENSORSTUDIO: [DEBUG] Creating SDL renderer...");
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("TENSORSTUDIO: [SDL ERROR] Failed to create renderer: %s", SDL_GetError());
        return;
    }
#endif

    SDL_Log("TENSORSTUDIO: [DEBUG] Initializing ImGui and ImPlot contexts...");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    float dpiScale = SDL_GetWindowDisplayScale(window);
    if (dpiScale <= 0.0f) dpiScale = 1.0f;

#ifdef __ANDROID__
    dpiScale = 3.2f; 
#endif

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpiScale);

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = dpiScale;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
#ifdef IS_DESKTOP_PLATFORM
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; 
#else
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
#endif
    
    ImGui::StyleColorsDark();
    ApplyTensorStudioTheme(dpiScale); // Passiamo dpiScale per scalare tutte le dimensioni in pixel

    SDL_Log("TENSORSTUDIO: [DEBUG] Linking SDL3-ImGui backend...");
#ifdef IS_DESKTOP_PLATFORM
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");
#else
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
#endif

    isRunning = true;
    SDL_Log("TENSORSTUDIO: [DEBUG] Entering Main Loop.");

    std::map<SDL_FingerID, ImVec2> active_touches;
    float last_pinch_dist = -1.0f;

    while (isRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            
            if (event.type == SDL_EVENT_QUIT) {
                isRunning = false;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) {
                isRunning = false;
            }

            if (event.type == SDL_EVENT_FINGER_DOWN) {
                active_touches[event.tfinger.fingerID] = ImVec2(event.tfinger.x, event.tfinger.y);
            } 
            else if (event.type == SDL_EVENT_FINGER_UP) {
                active_touches.erase(event.tfinger.fingerID);
                if (active_touches.size() < 2) {
                    last_pinch_dist = -1.0f;
                }
            } 
            else if (event.type == SDL_EVENT_FINGER_MOTION) {
                active_touches[event.tfinger.fingerID] = ImVec2(event.tfinger.x, event.tfinger.y);
                
                if (active_touches.size() == 1) {
                    float pixel_dx = event.tfinger.dx * width;
                    float pixel_dy = event.tfinger.dy * height;
                    
                    ImGuiContext* g_ptr = GImGui;
                    if (g_ptr && g_ptr->HoveredWindow) {
                        ImGuiWindow* scroll_window = g_ptr->HoveredWindow;
                        
                        while (scroll_window) {
                            bool can_scroll = (scroll_window->Flags & ImGuiWindowFlags_NoScrollWithMouse) == 0;
                            bool has_scroll_area = (scroll_window->ScrollMax.y > 0.0f || scroll_window->ScrollMax.x > 0.0f);
                            
                            if (can_scroll && has_scroll_area) {
                                break; 
                            }
                            scroll_window = scroll_window->ParentWindow; 
                        }
                        
                        if (scroll_window) {
                            scroll_window->Scroll.y -= pixel_dy;
                            scroll_window->Scroll.x -= pixel_dx;
                            
                            if (scroll_window->Scroll.y < 0.0f) scroll_window->Scroll.y = 0.0f;
                            if (scroll_window->Scroll.y > scroll_window->ScrollMax.y) scroll_window->Scroll.y = scroll_window->ScrollMax.y;
                            
                            if (scroll_window->Scroll.x < 0.0f) scroll_window->Scroll.x = 0.0f;
                            if (scroll_window->Scroll.x > scroll_window->ScrollMax.x) scroll_window->Scroll.x = scroll_window->ScrollMax.x;
                        }
                    }
                } 
                else if (active_touches.size() >= 2) {
                    auto it = active_touches.begin();
                    ImVec2 p1 = it->second;
                    it++;
                    ImVec2 p2 = it->second;
                    
                    float dx = (p1.x - p2.x) * width;
                    float dy = (p1.y - p2.y) * height;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    
                    if (last_pinch_dist > 0.0f) {
                        float delta = dist - last_pinch_dist;
                        if (std::abs(delta) > 4.0f) {
                            io.AddMouseWheelEvent(0.0f, delta / 80.0f); 
                            last_pinch_dist = dist; 
                        }
                    } else {
                        last_pinch_dist = dist;
                    }
                }
            }
        }

        Update();

#ifdef IS_DESKTOP_PLATFORM
        ImGui_ImplOpenGL3_NewFrame();
#else
        ImGui_ImplSDLRenderer3_NewFrame();
#endif
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        Render();

        ImGui::Render();
        
#ifdef IS_DESKTOP_PLATFORM
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.102f, 0.102f, 0.102f, 1.00f); 
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }
        SDL_GL_SwapWindow(window);
#else
        SDL_SetRenderDrawColor(renderer, 26, 26, 26, 255);
        SDL_RenderClear(renderer);
        
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
#endif
    }

    SDL_Log("TENSORSTUDIO: [DEBUG] Exiting loop, destroying resources...");
#ifdef IS_DESKTOP_PLATFORM
    ImGui_ImplOpenGL3_Shutdown();
    SDL_GL_DestroyContext(gl_context);
#else
    ImGui_ImplSDLRenderer3_Shutdown();
    SDL_DestroyRenderer(renderer);
#endif
    
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_DestroyWindow(window);
}

void Application::Update() {
    if (window) {
        SDL_GetWindowSizeInPixels(window, &width, &height);
    }
}

void Application::Render() {
    ImGuiIO& io = ImGui::GetIO();
    float scale = io.FontGlobalScale;
    
    // Controlliamo se la finestra è in Fullscreen (solo su desktop per evitare falsi positivi su mobile)
#ifdef IS_DESKTOP_PLATFORM
    Uint32 flags = SDL_GetWindowFlags(window);
    bool isFullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
#else
    bool isFullscreen = false;
#endif

    float navHeight = isFullscreen ? 0.0f : (45.0f * scale);

    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // 1. WORKSPACE PRINCIPALE
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - navHeight));
    ImGui::SetNextWindowViewport(viewport->ID); 

    ImGuiWindowFlags workspaceFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                                      ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("WorkspaceRegion", nullptr, workspaceFlags)) {
        if (activeTab == 0) {
            liveDashboardView.Render(renderer);
        } else if (activeTab == 1) {
            analysisWorkspaceView.Render(renderer); 
        }
        ImGui::End();
    }

    // 2. BARRA DI NAVIGAZIONE IN BASSO (Visibile solo se NON siamo in fullscreen)
    if (!isFullscreen) {
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - navHeight));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, navHeight));
        ImGui::SetNextWindowViewport(viewport->ID); 

        ImGuiWindowFlags navFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

        if (ImGui::Begin("BottomNav", nullptr, navFlags)) {
            float buttonWidth = 140.0f * scale;
            float buttonHeight = 30.0f * scale;
            float spacing = 10.0f * scale;
            float totalWidth = buttonWidth * 2 + spacing;
            
            ImGui::SetCursorPosX((io.DisplaySize.x - totalWidth) * 0.5f);
            ImGui::SetCursorPosY(7.0f * scale);

            if (ImGui::Button("Live Dashboard", ImVec2(buttonWidth, buttonHeight))) {
                activeTab = 0;
            }
            ImGui::SameLine(0.0f, spacing);
            if (ImGui::Button("Analysis", ImVec2(buttonWidth, buttonHeight))) {
                activeTab = 1;
            }
            ImGui::End();
        }
    }
}

void Application::Close() {
    isRunning = false;
}