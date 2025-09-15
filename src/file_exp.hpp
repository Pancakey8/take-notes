#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

class FileExplorer {
  std::filesystem::path root;
  std::vector<std::filesystem::path> file_list;
  using open_event_fn = std::function<void(std::string &&)>;
  open_event_fn open_evt = 0;

public:
  float x, y, w, h;
  FileExplorer(std::filesystem::path root);
  void render();
  void on_open(open_event_fn event);
  void update_dir();
};