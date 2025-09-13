#pragma once
#include <SDL3/SDL.h>
#include <imgui.h>
#include <string>

class Editor {
  std::string text{};
  size_t cursor{0};

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