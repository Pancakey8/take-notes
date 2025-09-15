#pragma once
#include "markup.hpp"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <string>
#include <vector>

enum class EditorMode { Insert, Select };

class Editor {
  std::vector<Token> format{};
  std::string text{};
  size_t cursor{0};
  EditorMode mode{EditorMode::Insert};
  size_t select_anchor{0};
  bool is_focused{false};

  void select_erase_exit();
  void reparse();

public:
  float width{0.8f}, height{1.0f}, font_size{18.0f};
  ImFont *plain, *bold;
  SDL_Window *window;
  SDL_Renderer *renderer;
  Editor(SDL_Window *window, SDL_Renderer *renderer, ImFont *plain,
         ImFont *bold)
      : plain(plain), bold(bold), window(window), renderer(renderer) {}
  void event(const SDL_Event &event);
  void render();
  void set_text(std::string &&text);

  ImVec4 get_bg_rect();
};