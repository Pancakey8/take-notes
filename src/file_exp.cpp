#include "file_exp.hpp"
#include <fstream>
#include <imgui.h>
#include <iostream>

FileExplorer::FileExplorer(const std::filesystem::path root) : root(root) {
  update_dir();
}

void FileExplorer::update_dir() {
  file_list.clear();
  for (auto &entry : std::filesystem::directory_iterator(root)) {
    file_list.emplace_back(entry.path());
  }
}

void FileExplorer::on_open(FileExplorer::open_event_fn fn) { open_evt = fn; }

void FileExplorer::render() {
  update_dir();

  ImGui::Begin("Explorer", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
  ImGui::SetWindowPos({x, y});
  ImGui::SetWindowSize({w, h});

  ImGui::Text("%s", root.c_str());

  if (root.has_parent_path() && ImGui::Button("..")) {
    root = root.parent_path();
  }

  for (const auto &fp : file_list) {
    if (ImGui::Button(fp.filename().c_str())) {
      if (std::filesystem::is_regular_file(fp)) {
        std::ifstream file(fp);
        file.seekg(0, std::ios::end);
        size_t len = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string contents{};
        contents.resize(len);
        file.read(contents.data(), len);
        open_evt(std::move(contents));
      }
      if (std::filesystem::is_directory(fp)) {
        root = fp;
      }
    }
  }

  ImGui::End();
}