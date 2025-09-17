#pragma once
#include <filesystem>
#include <functional>
#include <imgui.h>
#include <string>
#include <vector>

class FileExplorer {
  std::filesystem::path root;
  std::vector<std::filesystem::path> file_list;
  using open_event_fn =
      std::function<void(std::filesystem::path, std::string &&)>;
  open_event_fn open_evt = 0;
  bool creating_file{false};
  std::string filename;

public:
  bool can_close{false}, is_closed{false};
  float x, y, w, h;
  ImGuiWindowFlags flags{};
  std::string title{"Explorer"};
  FileExplorer(std::filesystem::path root);
  void render();
  void on_open(open_event_fn event);
  void update_dir();
  void create_file(std::string filename);
};