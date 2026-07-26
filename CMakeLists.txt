cmake_minimum_required(VERSION 3.20)
project(TensorStudio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------
# EIGEN
# -------------------------------------------------------------------
add_library(Eigen INTERFACE)
target_include_directories(Eigen INTERFACE external/eigen)

# -------------------------------------------------------------------
# SDL3 (Compilato staticamente per CachyOS e Windows)
# -------------------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "Build SDL shared" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build SDL static" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build SDL tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL examples" FORCE)
add_subdirectory(external/sdl EXCLUDE_FROM_ALL)

# -------------------------------------------------------------------
# IMGUI & IMPLOT
# -------------------------------------------------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
set(IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot")

# Sorgenti core e backend specifici per SDL3
file(GLOB IMGUI_SOURCES 
    "${IMGUI_DIR}/*.cpp" 
    "${IMPLOT_DIR}/*.cpp" 
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)

add_library(CoreImGui STATIC ${IMGUI_SOURCES})
target_include_directories(CoreImGui PUBLIC 
    ${IMGUI_DIR} 
    ${IMPLOT_DIR} 
    ${IMGUI_DIR}/backends
)
target_link_libraries(CoreImGui PUBLIC SDL3::SDL3-static)

# -------------------------------------------------------------------
# TENSORSTUDIO (Eseguibile Principale)
# -------------------------------------------------------------------
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)

# Link di base valido per entrambi i sistemi
target_link_libraries(${PROJECT_NAME} PRIVATE CoreImGui Eigen SDL3::SDL3-static)

# -------------------------------------------------------------------
# DIPENDENZE DI SISTEMA SPECIFICHE PER OS
# -------------------------------------------------------------------
if(WIN32)
    # Librerie richieste da Windows per SDL3/OpenGL in modalità statica
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        ws2_32 winmm gdi32 opengl32 setupapi version imm32
    )
elseif(UNIX AND NOT APPLE)
    # Librerie richieste da CachyOS/Linux
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        m dl pthread
    )
endif()