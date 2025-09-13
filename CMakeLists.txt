cmake_minimum_required(VERSION 3.16)
project(take-notes)

add_subdirectory(SDL3)

file(GLOB IMGUI_SRC imgui/*.cpp)
add_library(imgui ${IMGUI_SRC} imgui/backends/imgui_impl_sdl3.cpp imgui/backends/imgui_impl_sdlrenderer3.cpp)
target_include_directories(imgui PRIVATE imgui)

file(GLOB SRCS src/*.cpp)
add_executable(notes ${SRCS})
target_compile_features(notes PRIVATE cxx_std_23)
target_compile_options(notes PRIVATE -Wall -Wextra -Werror)
target_link_libraries(notes PRIVATE GL imgui SDL3::SDL3)
target_include_directories(notes PRIVATE imgui)
