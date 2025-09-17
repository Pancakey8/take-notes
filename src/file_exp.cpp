#include "file_exp.hpp"
#include <algorithm>
#include <fstream>
#include <imgui.h>
#include <iostream>

FileExplorer::FileExplorer(const std::filesystem::path root) : root(root) {
  filename.resize(1024);
  update_dir();
}

void FileExplorer::update_dir() {
  file_list.clear();
  for (auto &entry : std::filesystem::directory_iterator(root)) {
    file_list.emplace_back(entry.path());
  }
  std::sort(file_list.begin(), file_list.end());
}

void FileExplorer::on_open(FileExplorer::open_event_fn fn) { open_evt = fn; }

void FileExplorer::render() {
  update_dir();

  ImGui::Begin(title.c_str(), nullptr, flags);
  ImGui::SetWindowPos({x, y}, ImGuiCond_Once);
  ImGui::SetWindowSize({w, h}, ImGuiCond_Once);

  float avail = ImGui::GetContentRegionAvail().x;
  float item_w = ImGui::CalcTextSize(root.string().c_str()).x +
                 ImGui::CalcTextSize("New File").x +
                 ImGui::GetStyle().ItemSpacing.x;
  auto new_file = [&]() { creating_file = true; };
  if (item_w <= avail) {
    ImGui::Text("%s", root.string().c_str());
    ImGui::SameLine();
    if (ImGui::Button("New File")) {
      new_file();
    }
  } else {
    ImGui::Text("%s", root.string().c_str());
    if (ImGui::Button("New File")) {
      new_file();
    }
  }

  ImGui::Spacing();

  if (root.has_parent_path() && ImGui::Button("..")) {
    root = root.parent_path();
  }

  if (ImGui::IsKeyDown(ImGuiKey_Escape) && can_close) {
    is_closed = true;
  }

  for (const auto &fp : file_list) {
    if (ImGui::Button(fp.filename().string().c_str())) {
      if (std::filesystem::is_regular_file(fp)) {
        std::ifstream file(fp);
        file.seekg(0, std::ios::end);
        size_t len = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string contents{};
        contents.resize(len);
        file.read(contents.data(), len);
        open_evt(fp, std::move(contents));
      }
      if (std::filesystem::is_directory(fp)) {
        root = fp;
      }
    }
  }

  if (creating_file) {
    if (ImGui::InputText("Filename", filename.data(), filename.capacity(),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      if (filename.size() != 0) {
        create_file(filename);
      }
      creating_file = false;
    }
  }

  ImGui::End();
}

void FileExplorer::create_file(std::string filename) {
  std::filesystem::path pt = root / filename;
  if (!std::filesystem::exists(pt.parent_path())) {
    return;
  }

  if (std::filesystem::exists(pt) && std::filesystem::is_directory(pt))
    return;

  std::ofstream ofs(pt, std::ios::binary | std::ios::trunc);
  if (!ofs)
    return;
  ofs.close();

  update_dir();
}
