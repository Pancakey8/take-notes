#include "editor.hpp"
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#include <unordered_set>
#include <vector>

// TODO: Swap for proper utf8
size_t utf8_next_len(const std::string &s, size_t pos) {
  if (pos >= s.size())
    return 0;
  unsigned char c = (unsigned char)s[pos];
  if ((c & 0x80) == 0)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

size_t utf8_prev_len(const std::string &s, size_t pos) {
  if (pos == 0)
    return 0;
  size_t i = pos;
  while (i > 0) {
    --i;
    unsigned char c = (unsigned char)s[i];
    if ((c & 0xC0) != 0x80)
      return pos - i;
  }
  return pos;
}

void Editor::select_erase_exit() {
  if (select_anchor > cursor) {
    text.erase(cursor, select_anchor - cursor);
  } else {
    text.erase(select_anchor, cursor - select_anchor);
    cursor = select_anchor;
  }
  mode = EditorMode::Insert;
}

ImVec4 Editor::get_bg_rect() {
  int winw, winh;
  SDL_GetWindowSize(window, &winw, &winh);
  float w = winw * width, h = winh * height;
  float x = 0, y = 0.5f * (winh - h);
  return {x, y, w, h};
}

void Editor::event(const SDL_Event &event) {
  if (!is_focused)
    return;

  static const std::unordered_set<std::string> ctrl_stop_at{
      "\n", " ", "\t", "(", ")", "{", "}",  "[",  "]",  "<", ">",
      ",",  ".", ";",  ":", "!", "?", "\"", "\'", "-",  "_", "+",
      "=",  "*", "/",  "%", "&", "|", "^",  "~",  "\\", "`"};

  switch (event.type) {
  case SDL_EVENT_TEXT_INPUT: {
    if (mode == EditorMode::Select) {
      select_erase_exit();
    }
    size_t inp_len = strlen(event.text.text);
    text.insert(cursor, event.text.text, inp_len);
    cursor += inp_len;
    reparse();
  } break;
  case SDL_EVENT_KEY_DOWN: {
    switch (event.key.key) {
    case SDLK_BACKSPACE: {
      if (mode == EditorMode::Select) {
        select_erase_exit();
        break;
      }
      if (cursor == 0)
        break;
      size_t len;
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        long i;
        for (i = cursor - 1; i > 0;) {
          size_t l = utf8_prev_len(text, i);
          std::string str{text.substr(i - l, l)};
          i -= l;
          if (ctrl_stop_at.contains(str))
            break;
        }
        len = cursor - i;
      } else {
        len = utf8_prev_len(text, cursor);
      }
      cursor -= len;
      text.erase(cursor, len);
    } break;
    case SDLK_RETURN: {
      if (mode == EditorMode::Select) {
        select_erase_exit();
      }
      text.insert(cursor++, "\n", 1);
    } break;
    case SDLK_LEFT: {
      if (cursor == 0)
        break;
      if (event.key.mod & SDL_KMOD_LSHIFT || event.key.mod & SDL_KMOD_RSHIFT) {
        if (mode == EditorMode::Insert) {
          mode = EditorMode::Select;
          select_anchor = cursor;
        }
      } else {
        mode = EditorMode::Insert;
      }
      size_t len;
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        long i;
        for (i = cursor - 1; i > 0;) {
          size_t l = utf8_prev_len(text, i);
          std::string str{text.substr(i - l, l)};
          if (str == "\n" && static_cast<size_t>(i) != cursor)
            break;
          i -= l;
          if (ctrl_stop_at.contains(str))
            break;
        }
        len = cursor - i;
      } else {
        len = utf8_prev_len(text, cursor);
      }
      cursor -= len;
    } break;
    case SDLK_RIGHT: {
      if (cursor >= text.size())
        break;
      if (event.key.mod & SDL_KMOD_LSHIFT || event.key.mod & SDL_KMOD_RSHIFT) {
        if (mode == EditorMode::Insert) {
          mode = EditorMode::Select;
          select_anchor = cursor;
        }
      } else {
        mode = EditorMode::Insert;
      }
      size_t len;
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        long i;
        for (i = cursor; static_cast<size_t>(i) < text.size();) {
          size_t l = utf8_next_len(text, i);
          std::string str{text.substr(i, l)};
          if (str == "\n" && static_cast<size_t>(i) != cursor)
            break;
          i += l;
          if (ctrl_stop_at.contains(str))
            break;
        }
        len = i - cursor;
      } else {
        len = utf8_next_len(text, cursor);
      }
      cursor += len;
    } break;
    case SDLK_UP: {
      if (cursor == 0)
        break;

      size_t cur_line_start = text.rfind('\n', cursor - 1);
      if (cur_line_start == text.npos) {
        cursor = 0;
        break;
      }
      cur_line_start = cur_line_start + 1;

      size_t col_bytes = cursor;
      size_t col = 0;
      for (size_t i = cur_line_start; i < col_bytes;
           i += utf8_next_len(text, i))
        ++col;

      size_t prev_line_end = (cur_line_start == 0) ? 0 : cur_line_start - 1;
      if (prev_line_end == 0 && text.size() > 0 && text[0] == '\n') {
      }
      size_t prev_line_start =
          (prev_line_end == 0) ? 0 : text.rfind('\n', prev_line_end - 1);
      prev_line_start =
          (prev_line_start == text.npos) ? 0 : prev_line_start + 1;

      size_t prev_line_len = 0;
      for (size_t i = prev_line_start; i < prev_line_end;
           i += utf8_next_len(text, i))
        ++prev_line_len;

      size_t target_col = std::min(col, prev_line_len);

      size_t byte_pos = prev_line_start;
      for (size_t k = 0; k < target_col; ++k)
        byte_pos += utf8_next_len(text, byte_pos);

      cursor = byte_pos;
    } break;

    case SDLK_DOWN: {
      if (cursor >= text.size())
        break;

      size_t cur_line_start;
      if (cursor > 0) {
        cur_line_start = text.rfind('\n', cursor - 1);
        cur_line_start = (cur_line_start == text.npos) ? 0 : cur_line_start + 1;
      } else {
        cur_line_start = 0;
      }

      size_t col = 0;
      for (size_t i = cur_line_start; i < cursor; i += utf8_next_len(text, i))
        ++col;

      size_t cur_line_end = text.find('\n', cursor);
      if (cur_line_end == text.npos)
        cur_line_end = text.size();

      if (cur_line_end >= text.size()) {
        cursor = text.size();
        break;
      }

      size_t next_line_start = cur_line_end + 1;
      size_t next_line_end = text.find('\n', next_line_start);
      if (next_line_end == text.npos)
        next_line_end = text.size();

      size_t next_line_len = 0;
      for (size_t i = next_line_start; i < next_line_end;
           i += utf8_next_len(text, i))
        ++next_line_len;

      size_t target_col = std::min(col, next_line_len);

      size_t byte_pos = next_line_start;
      for (size_t k = 0; k < target_col; ++k)
        byte_pos += utf8_next_len(text, byte_pos);

      cursor = byte_pos;
    } break;
    case SDLK_TAB: {
      if (mode == EditorMode::Select) {
        select_erase_exit();
      }
      text.insert(cursor, "  ", 2);
      cursor += 2;
    } break;
    case SDLK_V:
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        if (mode == EditorMode::Select) {
          select_erase_exit();
        }

        char *clip = SDL_GetClipboardText();
        size_t clip_len = strlen(clip);
        text.insert(cursor, clip, clip_len);
        cursor += clip_len;
      }
      break;
    case SDLK_C:
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        if (mode == EditorMode::Select) {
          std::string select{};
          if (select_anchor > cursor) {
            select = text.substr(cursor, select_anchor - cursor);
          } else {
            select = text.substr(select_anchor, cursor - select_anchor);
          }
          SDL_SetClipboardText(select.c_str());
          cursor = select_anchor;
          mode = EditorMode::Insert;
        }
      }
      break;
    case SDLK_X:
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        if (mode == EditorMode::Select) {
          std::string select{};
          if (select_anchor > cursor) {
            select = text.substr(cursor, select_anchor - cursor);
          } else {
            select = text.substr(select_anchor, cursor - select_anchor);
          }
          SDL_SetClipboardText(select.c_str());
          select_erase_exit();
        }
      }
      break;
    case SDLK_A:
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        mode = EditorMode::Select;
        select_anchor = 0;
        cursor = text.size();
      }
      break;
    default:
      break;
    }
    reparse();
  } break;
  default:
    break;
  }
}

void Editor::render() {
  auto [x, y, w, h] = get_bg_rect();
  ImGui::SetNextWindowPos({x, y});
  ImGui::SetNextWindowSize({w, h});
  ImGui::Begin("Editor", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);

  is_focused = ImGui::IsWindowFocused();

  ImDrawList *draw_list = ImGui::GetWindowDrawList();

  draw_list->AddRectFilled({x, y}, {x + w, y + h},
                           IM_COL32(0x20, 0x20, 0x20, 0xFF));

  float padding = 20.0f;
  float content_x = x + padding;
  float content_y = y + padding;
  float content_w = w - 2.0f * padding;
  float content_h = h - 2.0f * padding;

  float cx{content_x}, cy{content_y};
  size_t idx{0};

  auto render = [&](ImFont *font, const std::string &text, int fmt) {
    size_t vtx_start = draw_list->VtxBuffer.Size;
    draw_list->AddText(font, font_size, {cx, cy},
                       IM_COL32(0xFF, 0xFF, 0xFF, 0xFF), text.c_str());
    size_t vtx_end = draw_list->VtxBuffer.Size;
    if (fmt & Format_Italic) {
      for (size_t i = vtx_start; i < vtx_end; ++i) {
        ImDrawVert &vtx = draw_list->VtxBuffer[i];
        float dy = vtx.pos.y - cy;
        vtx.pos.x -= 0.20f * dy;
      }
    }
  };

  auto draw_cursor = [&](float x, float y, float height) {
    if (!is_focused)
      return;
    draw_list->AddRectFilled({x, y}, {x + 3.0f, y + height},
                             IM_COL32(0xFF, 0xFF, 0xFF, 0xAF));
  };

  auto draw_selection = [&](float x, float y, float w, float h) {
    draw_list->AddRectFilled({x, y}, {x + w, y + h},
                             IM_COL32(0xFF, 0xFF, 0, 0x7F));
  };

  size_t sel_start = std::min(cursor, select_anchor);
  size_t sel_end = std::max(cursor, select_anchor);

  for (auto &token : format) {
    if (std::holds_alternative<NewLine>(token)) {
      if (idx == cursor) {
        draw_cursor(cx, cy, font_size);
      }
      cy += font_size;
      if (cy >= content_y + content_h - font_size)
        break;
      cx = content_x;
      ++idx;
      continue;
    }

    if (std::holds_alternative<FormattedString>(token)) {
      auto fmt = std::get<FormattedString>(token);
      ImFont *font = (fmt.format & Format_Bold) ? bold : plain;

      std::string text = fmt.value;
      size_t pos = 0;

      while (pos < text.size()) {
        size_t next_space = text.find(' ', pos);
        if (next_space == std::string::npos)
          next_space = text.size();

        std::string word = text.substr(pos, next_space - pos);

        if (next_space < text.size())
          word += ' ';

        float word_width =
            font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX, word.c_str()).x;

        if (cx + word_width > content_x + content_w) {
          cx = content_x;
          cy += font_size;
          if (cy >= content_y + content_h - font_size)
            break;
        }
        for (size_t i = 0; i <= word.size(); ++i) {
          if (idx == cursor) {
            std::string sub = word.substr(0, i);
            float cursor_x =
                cx +
                font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX, sub.c_str()).x;
            draw_cursor(cursor_x, cy, font_size);
          }
          if (mode == EditorMode::Select && idx >= sel_start && idx < sel_end) {
            float sub_width = font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX,
                                                  word.substr(0, i + 1).c_str())
                                  .x;
            float prev_width = font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX,
                                                   word.substr(0, i).c_str())
                                   .x;
            draw_selection(cx + prev_width, cy, sub_width - prev_width,
                           font_size);
          }
          if (i != word.size())
            ++idx;
        }

        render(font, word, fmt.format);
        cx += word_width;

        pos = next_space + 1;
      }
      continue;
    }
  }

  if (idx == cursor) {
    draw_cursor(cx, cy, font_size);
  }

  ImGui::End();
}

void Editor::reparse() {
  Parser parser{text};
  parser.parse_all();
  format = std::move(parser.tokens);
}

void Editor::set_text(std::string &&text) {
  this->text = std::move(text);
  reparse();
}