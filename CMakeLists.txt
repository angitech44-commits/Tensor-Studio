cmake_minimum_required(VERSION 3.20)

# -------------------------------------------------------------------
# VCPKG TOOLCHAIN (Rigorosamente PRIMA di project())
# -------------------------------------------------------------------
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")

project(TensorStudio VERSION 0.1.0 LANGUAGES CXX)

# ===================================================================
# BUGFIX: Disattiva i PCH globalmente per evitare errori di cache 
# (cmake_pch.h.pch missing) con l'NDK di Android
# ===================================================================
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------
# VCPKG PACKAGES
# -------------------------------------------------------------------
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(STB_INCLUDE_DIR stb_image.h REQUIRED)

# -------------------------------------------------------------------
# EIGEN
# -------------------------------------------------------------------
add_library(Eigen INTERFACE)
target_include_directories(Eigen INTERFACE external/eigen)

# -------------------------------------------------------------------
# SDL3 (Condiviso su Android, Statico su PC)
# -------------------------------------------------------------------
if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "Build SDL shared" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "Build SDL static" FORCE)
    set(SDL_LIB_TARGET SDL3::SDL3)
else()
    set(SDL_SHARED OFF CACHE BOOL "Build SDL shared" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL static" FORCE)
    set(SDL_LIB_TARGET SDL3::SDL3-static)
endif()

set(SDL_TESTS OFF CACHE BOOL "Build SDL tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build SDL examples" FORCE)
add_subdirectory(external/sdl EXCLUDE_FROM_ALL)

# -------------------------------------------------------------------
# IMGUI & IMPLOT
# -------------------------------------------------------------------
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
set(IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot")

file(GLOB IMGUI_SOURCES 
    "${IMGUI_DIR}/*.cpp" 
    "${IMPLOT_DIR}/*.cpp" 
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

add_library(CoreImGui STATIC ${IMGUI_SOURCES})
target_include_directories(CoreImGui PUBLIC ${IMGUI_DIR} ${IMPLOT_DIR} ${IMGUI_DIR}/backends)
target_compile_definitions(CoreImGui PUBLIC "ImDrawIdx=unsigned int")
target_link_libraries(CoreImGui PUBLIC ${SDL_LIB_TARGET})

# -------------------------------------------------------------------
# TENSORSTUDIO
# -------------------------------------------------------------------
file(GLOB_RECURSE SOURCES "src/*.cpp")

if(ANDROID)
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "main")
else()
    add_executable(${PROJECT_NAME} ${SOURCES})
endif()

target_compile_definitions(TensorStudio PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE src ${STB_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE 
    CoreImGui 
    Eigen 
    ${SDL_LIB_TARGET}
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# -------------------------------------------------------------------
# DIPENDENZE DI SISTEMA SPECIFICHE PER OS
# -------------------------------------------------------------------
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 winmm gdi32 opengl32 setupapi version imm32)
elseif(ANDROID)
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl log android GLESv3)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl pthread)
endif()