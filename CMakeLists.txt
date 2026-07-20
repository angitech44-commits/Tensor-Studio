cmake_minimum_required(VERSION 3.20)
project(TensorStudio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/raylib)

add_library(Eigen INTERFACE)
target_include_directories(Eigen INTERFACE external/eigen)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
set(IMPLOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/implot")
set(RLIMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/rlImGui")

file(GLOB IMGUI_SOURCES 
    "${IMGUI_DIR}/*.cpp" 
    "${IMPLOT_DIR}/*.cpp" 
    "${RLIMGUI_DIR}/*.cpp"
)

add_library(rlImGui STATIC ${IMGUI_SOURCES})
target_include_directories(rlImGui PUBLIC 
    ${IMGUI_DIR} 
    ${IMPLOT_DIR} 
    ${RLIMGUI_DIR} 
    external/raylib/src
)
target_link_libraries(rlImGui PUBLIC raylib)

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib rlImGui Eigen ws2_32 winmm gdi32 opengl32)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib rlImGui Eigen)
endif()