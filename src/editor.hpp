#pragma once
#include "file_exp.hpp"
#include "markup.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <imgui.h>
#include <string>
#include <vector>

enum class EditorMode { Insert, Select };

class Editor {
  std::vector<Token> format{};
  std::string text{};
  std::filesystem::path filepath{};
  size_t cursor{0};
  EditorMode mode{EditorMode::Insert};
  size_t select_anchor{0};
  bool is_focused{false};
  std::string error{};
  bool show_error{false};
  bool ask_save{false};
  FileExplorer save_explorer{std::filesystem::current_path()};
  size_t row_start{0}, row_max{std::numeric_limits<size_t>().max()};
  bool do_cursor_choose{false};
  int choose_x{0}, choose_y{0};

  void normalize_cursor();
  void select_erase_exit();
  void reparse();
  void error_msg(std::string err);
  void save();

public:
  float width{0.8f}, height{1.0f}, font_size{18.0f};
  ImFont *plain, *bold;
  SDL_Window *window;
  SDL_Renderer *renderer;
  Editor(SDL_Window *window, SDL_Renderer *renderer, ImFont *plain,
         ImFont *bold)
      : plain(plain), bold(bold), window(window), renderer(renderer) {
    save_explorer.on_open([&](auto fp, auto &&_) {
      filepath = fp;
      ask_save = false;
      save();
    });
    save_explorer.title = "Save As";
    save_explorer.x = 100;
    save_explorer.y = 100;
    save_explorer.w = 300;
    save_explorer.h = 300;
  }
  void event(const SDL_Event &event);
  void render();
  void set_text(std::filesystem::path path, std::string &&text);

  ImVec4 get_bg_rect();
};