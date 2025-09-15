#include "editor.hpp"
#include <backends/imgui_impl_sdlrenderer3.h>
#include <imgui.h>
#include <iostream>
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
  ImGuiIO io = ImGui::GetIO();
  if (io.WantCaptureKeyboard || io.WantTextInput) {
    is_focused = false;
    return;
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    if (io.WantCaptureMouse) {
      is_focused = false;
      return;
    }
    auto [x, y, w, h] = get_bg_rect();
    float mx = event.button.x, my = event.button.y;
    if (x <= mx && mx <= x + w && y <= my && my <= y + h) {
      is_focused = true;
    } else {
      is_focused = false;
    }
  }

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
  } break;
  default:
    break;
  }
}

void Editor::render() {
  ImDrawList *draw_list = ImGui::GetBackgroundDrawList();

  auto [x, y, w, h] = get_bg_rect();
  draw_list->AddRectFilled({x, y}, {x + w, y + h},
                           IM_COL32(0x20, 0x20, 0x20, 0xFF));

  float padding = 20.0f;
  float content_x = x + padding;
  float content_y = y + padding;
  float content_w = w - 2.0f * padding;
  float content_h = h - 2.0f * padding;

  float cx{content_x}, cy{content_y};
  std::string line{};
  size_t row{0};

  using Rect = std::pair<ImVec2, ImVec2>;
  std::vector<Rect> select_lines{};
  Rect select_now{};

  // TODO: Disgusting, but so many cases??
  // TODO: Also use utf8 methods
  for (size_t i = 0; i < text.size(); ++i) {
    char &ch = text.at(i);

    bool in_selection{cursor > select_anchor
                          ? (select_anchor < i && i < cursor)
                          : (cursor < i && i < select_anchor)};
    bool in_begin{cursor > select_anchor ? (i == select_anchor)
                                         : (i == cursor)};
    bool in_end{cursor > select_anchor ? (i == cursor) : (i == select_anchor)};

    // If we are at the start of the selection, then set start and end points.
    if (mode == EditorMode::Select && in_begin) {
      ImVec2 line_sz = font->CalcTextSizeA(font_size, content_w, content_w + 10,
                                           line.c_str());
      select_now.first.x = content_x + line_sz.x;
      select_now.first.y = content_y + row * font_size;
      select_now.second.x = select_now.first.x;
      select_now.second.y = select_now.first.y + font_size;
    }

    // While within the selection, update just the end point.
    if (mode == EditorMode::Select && (in_end || in_selection)) {
      ImVec2 line_sz = font->CalcTextSizeA(font_size, content_w, content_w + 10,
                                           line.c_str());
      select_now.second.x = content_x + line_sz.x;
      select_now.second.y = select_now.first.y + font_size;
    }

    if (i == cursor) { // If at cursor, save the position
      ImVec2 line_sz = font->CalcTextSizeA(font_size, content_w, content_w + 10,
                                           line.c_str());
      cx = content_x + line_sz.x;
      cy = content_y + row * font_size;
    }

    if (ch == '\n') { // Natural newline, update scene
      draw_list->AddText(
          font, font_size, ImVec2(content_x, content_y + row * font_size),
          IM_COL32(0xFF, 0xFF, 0xFF, 0xFF), line.c_str(), nullptr);
      line.clear();
      ++row;
      // Append current selected area if new line
      if (mode == EditorMode::Select) {
        select_lines.emplace_back(select_now);
        select_now.first.x = content_x;
        select_now.first.y = content_y + row * font_size;
        select_now.second.x = select_now.first.x;
        select_now.second.y = select_now.first.y + font_size;
      }
    } else {
      // Append to line...(1)
      line += ch;
      ImVec2 line_sz = font->CalcTextSizeA(font_size, content_w,
                                           content_w + 10 /*Impossible value */
                                           ,
                                           line.c_str());

      //(1)...but if overflow, then act as if newline.
      if (line_sz.x >= content_w - font_size) {
        line.pop_back();
        draw_list->AddText(
            font, font_size, ImVec2(content_x, content_y + row * font_size),
            IM_COL32(0xFF, 0xFF, 0xFF, 0xFF), line.c_str(), nullptr);
        draw_list->AddText(font, font_size,
                           ImVec2(content_w, content_y + row * font_size),
                           IM_COL32(0xFF, 0xFF, 0xFF, 0x7F), "...", nullptr);
        ++row;
        line.clear();
        line += ch;
        // Also append selection box
        if (mode == EditorMode::Select) {
          select_lines.emplace_back(select_now);
          select_now.first.x = content_x;
          select_now.first.y = content_y + row * font_size;
          select_now.second.x = select_now.first.x;
          select_now.second.y = select_now.first.y + font_size;
        }
      }
    }

    // But if cursor is appending at the end, special case
    if (cursor == text.size() && i == text.size() - 1) {
      ImVec2 line_sz = font->CalcTextSizeA(font_size, content_w, content_w + 10,
                                           line.c_str());
      cx = content_x + line_sz.x;
      cy = content_y + row * font_size;
    }

    // If selecting and either end is at the end, then ensure final character is
    // included
    if (mode == EditorMode::Select)
      if ((cursor == text.size() || select_anchor == text.size()) &&
          i == text.size() - 1) {
        ImVec2 line_sz = font->CalcTextSizeA(font_size, content_w,
                                             content_w + 10, line.c_str());
        select_now.second.x = content_x + line_sz.x;
        select_now.second.y = select_now.first.y + font_size;
      }

    // And if no natural newline, update the final line.
    if (ch != '\n' && i == text.size() - 1) {
      draw_list->AddText(
          font, font_size, ImVec2(content_x, content_y + row * font_size),
          IM_COL32(0xFF, 0xFF, 0xFF, 0xFF), line.c_str(), nullptr);
      line.clear();
      ++row;
      // And also append the final line if selection
      if (mode == EditorMode::Select) {
        select_lines.emplace_back(select_now);
      }
    }

    if (row * font_size >= content_h) {
      break;
    }
  }

  if (is_focused) {
    draw_list->AddRectFilled({cx, cy}, {cx + 5, cy + font_size},
                             IM_COL32(0xFF, 0xFF, 0xFF, 0xFF));
  }

  if (mode == EditorMode::Select) {
    for (auto &line : select_lines) {
      ImGuiCol col = IM_COL32(0xFF, 0xFF, 0, 0x7F);
      if (!is_focused)
        col = IM_COL32(0xAF, 0xAF, 0, 0x7F);
      draw_list->AddRectFilled(line.first, line.second, col);
    }
  }
}

void Editor::set_text(std::string &&text) { this->text = std::move(text); }