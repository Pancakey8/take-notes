#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
#include <string>

enum class EditorMode { Insert, Select };

class Editor {
  std::string text{};
  size_t cursor{0};
  EditorMode mode{EditorMode::Insert};
  size_t select_anchor{0};

  void select_erase_exit();

public:
  float width{0.8f}, height{1.0f}, font_size{18.0f};
  ImFont *font;
  SDL_Window *window;
  SDL_Renderer *renderer;
  Editor(SDL_Window *window, SDL_Renderer *renderer, ImFont *font)
      : font(font), window(window), renderer(renderer) {}
  void event(const SDL_Event &event);
  void update();
  void render();
};