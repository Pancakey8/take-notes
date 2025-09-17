#include "editor.hpp"
#include "file_exp.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#include <iostream>
#include <misc/freetype/imgui_freetype.h>

int main(int, char *argv[]) {
  if (!SDL_Init(0))
    return 1;

  SDL_Window *window;
  SDL_Renderer *renderer;
  if (!SDL_CreateWindowAndRenderer("Take Notes", 800, 600,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED,
                                   &window, &renderer))
    return 1;

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  io.Fonts->AddFontDefault();
  // Sharper text
  ImFontConfig cfg;
  cfg.OversampleH = cfg.OversampleV = 1;
  cfg.RasterizerMultiply = 1.0f;
  cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_Monochrome |
                        ImGuiFreeTypeBuilderFlags_MonoHinting;
  float font_size{20.0f};
  std::filesystem::path fonts_dir =
      std::filesystem::weakly_canonical(argv[0]).parent_path() / "fonts";
  ImFont *plain_font = io.Fonts->AddFontFromFileTTF(
      (fonts_dir / "NotoSansMono-Medium.ttf").string().c_str(), font_size,
      &cfg);
  ImFont *bold_font = io.Fonts->AddFontFromFileTTF(
      (fonts_dir / "NotoSansMono-ExtraBold.ttf").string().c_str(), font_size,
      &cfg);

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  Editor editor{window, renderer, plain_font, bold_font};
  editor.font_size = font_size;
  FileExplorer explorer{std::filesystem::current_path()};
  explorer.on_open(
      [&editor](auto fp, auto file) { editor.set_text(fp, std::move(file)); });

  bool is_resizing{false};

  bool is_running{true};
  while (is_running) {
    auto [ex, ey, ew, eh] = editor.get_bg_rect();
    int winw, winh;
    SDL_GetWindowSize(window, &winw, &winh);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      editor.event(event);
      if (event.type == SDL_EVENT_QUIT) {
        is_running = false;
      }
    }

    auto [cx, cy] = ImGui::GetMousePos();
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      if ((ex + ew - 5 <= cx && cx <= ex + ew + 5 && 0 <= cy && cy <= eh) ||
          is_resizing) {
        is_resizing = true;
        auto [xrel, _] = ImGui::GetMouseDragDelta();
        ImGui::ResetMouseDragDelta();
        editor.width += xrel / static_cast<float>(winw);
        auto [x, y, w, h] = editor.get_bg_rect();
        ex = x;
        ey = y;
        ew = w;
        eh = h;
      }
    } else {
      is_resizing = false;
    }

    explorer.x = ex + ew;
    explorer.y = 0;
    explorer.h = eh;
    explorer.w = winw - ew;
    explorer.flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize;

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(renderer);
    editor.render();
    ImGui::SetNextWindowPos({explorer.x, explorer.y});
    ImGui::SetNextWindowSize({explorer.w, explorer.h});
    explorer.render();

    if (!is_running && editor.is_save_needed()) {
      is_running = true;
      ImGui::OpenPopup("Notice");
    }

    if (ImGui::BeginPopupModal("Notice", NULL,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextWrapped("%s", "You haven't saved your file yet. Quit?");
      if (ImGui::Button("Yes", ImVec2(80, 0))) {
        is_running = false;
        ImGui::CloseCurrentPopup();
      }
      if (ImGui::Button("No", ImVec2(80, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}