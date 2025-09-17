cmake_minimum_required(VERSION 3.16)
project(take-notes)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

add_subdirectory(SDL3)

file(GLOB IMGUI_SRC imgui/*.cpp)
add_library(imgui ${IMGUI_SRC} imgui/backends/imgui_impl_sdl3.cpp imgui/backends/imgui_impl_sdlrenderer3.cpp)
target_include_directories(imgui PRIVATE imgui SDL3/include)

file(GLOB SRCS src/*.cpp)
add_executable(notes ${SRCS})
target_compile_features(notes PRIVATE cxx_std_23)
target_compile_options(notes PRIVATE -Wall -Wextra -Werror)
target_link_libraries(notes PRIVATE imgui SDL3::SDL3)
target_include_directories(notes PRIVATE imgui)

set(FONTS_SRC ${CMAKE_SOURCE_DIR}/src/fonts)
set(FONTS_OUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fonts)

add_custom_target(
    copy_fonts
    ALL
    VERBATIM
    COMMENT "Copying fonts ${FONTS_SRC} -> ${FONTS_OUT}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${FONTS_SRC} ${FONTS_OUT}
)

install(TARGETS notes RUNTIME DESTINATION bin/)
install(DIRECTORY ${FONTS_SRC} DESTINATION bin/)
