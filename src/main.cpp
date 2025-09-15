#include "editor.hpp"
#include "file_exp.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#include <misc/freetype/imgui_freetype.h>

int main() {
  if (!SDL_Init(0))
    return 1;

  SDL_Window *window;
  SDL_Renderer *renderer;
  if (!SDL_CreateWindowAndRenderer("Take Notes", 800, 600,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED,
                                   &window, &renderer))
    return 1;

  SDL_StartTextInput(window);

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
  // TODO: No hardcoded font
  ImFont *editor_font = io.Fonts->AddFontFromFileTTF(
      "/usr/share/fonts/TTF/FiraCodeNerdFontPropo-Regular.ttf", 18.0f, &cfg);

  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  Editor editor{window, renderer, editor_font};
  // TODO: No hardcoded path
  FileExplorer explorer{std::filesystem::current_path().parent_path()};
  explorer.on_open([&editor](auto file) { editor.set_text(std::move(file)); });

  bool is_running{true};
  while (is_running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      editor.event(event);
      if (event.type == SDL_EVENT_QUIT) {
        is_running = false;
      }
    }

    auto [ex, ey, ew, eh] = editor.get_bg_rect();
    int winw, winh;
    SDL_GetWindowSize(window, &winw, &winh);
    explorer.x = ex + ew;
    explorer.y = 0;
    explorer.h = eh;
    explorer.w = winw - ew;

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderClear(renderer);
    editor.render();
    explorer.render();
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}