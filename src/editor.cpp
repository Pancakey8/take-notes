#include "editor.hpp"
#include "SDL3/SDL_error.h"
#include "file_exp.hpp"
#include "markup.hpp"
#include "utility.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <backends/imgui_impl_sdlrenderer3.h>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <unordered_set>
#include <variant>
#include <vector>

float apply_head(float fsize, int head_n);

void Editor::select_erase_exit() {
  if (select_anchor > cursor) {
    text.erase(cursor, select_anchor - cursor);
  } else {
    text.erase(select_anchor, cursor - select_anchor);
    cursor = select_anchor;
  }
  mode = EditorMode::Insert;
  normalize_cursor();
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
  case SDL_EVENT_MOUSE_WHEEL: {
    int dir = event.wheel.integer_y;
    if (dir > 0 && row_start > 0) {
      --row_start;
    } else if (dir < 0) {
      ++row_start;
    }
  } break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    if (mode == EditorMode::Select) {
      mode = EditorMode::Insert;
    }
    if (event.button.clicks == 1 && event.button.button == SDL_BUTTON_LEFT) {
      do_cursor_choose = true;
      choose_x = event.button.x;
      choose_y = event.button.y;
    }
  } break;
  case SDL_EVENT_TEXT_INPUT: {
    if (mode == EditorMode::Select) {
      select_erase_exit();
    }
    std::string inp(event.text.text);
    Token hover = get_hovered_token();
    if (!(std::holds_alternative<FormattedString>(hover) &&
          std::get<FormattedString>(hover).format & Format_Code)) {
      if (cursor > 0) {
        size_t prev = utf8_prev_len(text, cursor);
        if (text.substr(cursor - prev, prev) == "\\") {
          text.insert(cursor, inp.data(), inp.size());
          cursor += inp.size();
          reparse();
          break;
        }
      }
      if (inp == "*") {
        text.insert(cursor, "**");
        ++cursor;
        reparse();
        break;
      } else if (inp == "/") {
        text.insert(cursor, "//");
        ++cursor;
        reparse();
        break;
      } else if (inp == "~") {
        text.insert(cursor, "~~");
        ++cursor;
        reparse();
        break;
      } else if (inp == "-") {
        if (text.size() != 0 && text[cursor - 1] != '\n') {
          text.insert(cursor, "-");
          ++cursor;
          reparse();
          break;
        }
        std::string dot{"•"};
        text.insert(cursor, dot);
        cursor += dot.size();
        reparse();
        break;
      } else if (inp == "[") {
        text.insert(cursor, "[]");
        ++cursor;
        reparse();
        break;
      }
    }
    text.insert(cursor, inp.data(), inp.size());
    cursor += inp.size();
    reparse();
  } break;
  case SDL_EVENT_KEY_DOWN: {
    switch (event.key.key) {
    case SDLK_BACKSPACE: {
      if (mode == EditorMode::Select) {
        select_erase_exit();
        reparse();
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
      reparse();
      normalize_cursor();
    } break;
    case SDLK_RETURN: {
      if (mode == EditorMode::Select) {
        select_erase_exit();
      }
      text.insert(cursor++, "\n", 1);
      Token tok{get_hovered_token()};
      if (std::holds_alternative<FormattedString>(tok)) {
        FormattedString fmt{std::get<FormattedString>(tok)};
        if (fmt.format & Format_Code) {
          text.insert(cursor++, "\t");
        } else if (fmt.format & Format_List) {
          std::string dot{"•"};
          text.insert(cursor, dot);
          cursor += dot.size();
        }
      }
      reparse();
      normalize_cursor();
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
      normalize_cursor();
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
      normalize_cursor();
    } break;
    case SDLK_UP: {
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
      normalize_cursor();
    } break;

    case SDLK_DOWN: {
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
      normalize_cursor();
    } break;
    case SDLK_TAB: {
      if (mode == EditorMode::Select) {
        select_erase_exit();
      }
      text.insert(cursor, "\t", 1);
      cursor += 1;
      reparse();
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
        reparse();
        normalize_cursor();
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
          reparse();
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
    case SDLK_S:
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        save();
      }
      break;

    case SDLK_HOME: {
      if (event.key.mod & SDL_KMOD_LSHIFT || event.key.mod & SDL_KMOD_RSHIFT) {
        if (mode == EditorMode::Insert) {
          mode = EditorMode::Select;
          select_anchor = cursor;
        }
      } else {
        mode = EditorMode::Insert;
      }
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        cursor = 0;
        normalize_cursor();
        break;
      }
      size_t start = text.rfind('\n', cursor == 0 ? 0 : (cursor - 1));
      if (start == text.npos) {
        start = 0;
      } else {
        start += 1;
      }
      cursor = start;
      normalize_cursor();
    } break;

    case SDLK_END: {
      if (event.key.mod & SDL_KMOD_LSHIFT || event.key.mod & SDL_KMOD_RSHIFT) {
        if (mode == EditorMode::Insert) {
          mode = EditorMode::Select;
          select_anchor = cursor;
        }
      } else {
        mode = EditorMode::Insert;
      }
      if (event.key.mod & SDL_KMOD_LCTRL || event.key.mod & SDL_KMOD_RCTRL) {
        cursor = text.size();
        normalize_cursor();
        break;
      }
      size_t end = text.find('\n', cursor > text.size() ? text.size() : cursor);
      if (end == text.npos) {
        end = text.size();
      }
      cursor = end;
      normalize_cursor();
    } break;

    default:
      break;
    }
  } break;
  default:
    break;
  }

  update_title();
}

void Editor::render() {
  auto [x, y, w, h] = get_bg_rect();
  ImGui::SetNextWindowPos({x, y});
  ImGui::SetNextWindowSize({w, h});
  ImGui::Begin("Editor", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);

  is_focused = ImGui::IsWindowFocused();

  if (is_focused)
    SDL_StartTextInput(window);

  ImDrawList *draw_list = ImGui::GetWindowDrawList();

  draw_list->AddRectFilled({x, y}, {x + w, y + h},
                           IM_COL32(0x20, 0x20, 0x20, 0xFF));

  float padding = 40.0f;
  float content_x = x + padding;
  float content_y = y + padding;
  float content_w = w - 2.0f * padding;
  float content_h = h - 2.0f * padding;

  float cx{content_x}, cy{content_y};
  size_t idx{0};

  float current_size{font_size};

  auto render = [&](ImFont *font, const std::string &text, int fmt) {
    size_t vtx_start = draw_list->VtxBuffer.Size;
    draw_list->AddText(font, current_size, {cx, cy},
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

  auto draw_linenumber = [&](size_t row, Token &token) {
    std::string num{std::to_string(row + 1)};
    size_t sz =
        plain->CalcTextSizeA(font_size, content_w, content_w + 10, num.c_str())
            .x;
    float fsize = font_size;
    if (std::holds_alternative<FormattedString>(token)) {
      auto fmt = std::get<FormattedString>(token).format;
      fsize = apply_head(fsize, fmt);
    }
    draw_list->AddText(
        plain, font_size,
        {cx - sz - (padding - sz) / 2, cy + (fsize - font_size) / 2},
        IM_COL32(0xFF, 0xFF, 0xFF, 0x7F), num.c_str());
  };

  size_t sel_start = std::min(cursor, select_anchor);
  size_t sel_end = std::max(cursor, select_anchor);

  size_t row{0};
  size_t closest_idx{0};
  float closest_len{std::numeric_limits<float>().max()};

  bool in_table{false}, is_mismatched{false};
  std::vector<size_t> col_lengths{};
  std::vector<size_t> table_elems{};
  size_t col_count{};
  float deferred_gap{0};

  std::vector<Image> imgs_buffer{};
  auto display_images = [&]() {
    float image_row = cy;
    float image_col = content_x;
    for (auto &img : imgs_buffer) {
      draw_list->AddImage(ImTextureRef(images[get_path_proper(img.fp)]),
                          {image_col, image_row},
                          {image_col + 100, image_row + 100});
      image_col += 105;
      if (image_col + 100 >= content_x + content_w) {
        image_col = content_x;
        image_row += 105;
      }
      if (image_row + 100 >= content_y + content_h) {
        break;
      }
    }
    cy = image_row + 105;
    imgs_buffer.clear();
  };

  for (size_t i = 0; i < format.size(); ++i) {
    Token &token = format[i];
    if (row >= row_start && cx == content_x) {
      draw_linenumber(row, token);
    }

    if (std::holds_alternative<NewLine>(token)) {
      if (row < row_start) {
        ++row;
        ++idx;
        continue;
      }
      if (idx == cursor) {
        draw_cursor(cx, cy, current_size);
      }
      cy += current_size;
      if (imgs_buffer.size() > 0) {
        display_images();
      }
      if (cy >= content_y + content_h - current_size)
        break;
      if (do_cursor_choose) {
        float dx = cx - choose_x;
        float dy = cy + current_size / 2 - choose_y;
        float dist = SDL_sqrtf(dx * dx + dy * dy);
        if (dist < closest_len) {
          closest_len = dist;
          closest_idx = idx;
        }
      }
      cx = content_x;
      ++idx;
      ++row;
      current_size = font_size;
      continue;
    }

    if (std::holds_alternative<FormattedString>(token)) {
      auto fmt = std::get<FormattedString>(token);
      if (row < row_start) {
        idx += fmt.value.size();
        continue;
      }

      if (fmt.format & Format_Table && !in_table) {
        in_table = true;
        is_mismatched = false;
        col_lengths.clear();
        table_elems.clear();
        col_count = 0;
        size_t idx = i;
        std::vector<std::vector<std::string>> table;
        std::vector<std::string> current_row;

        while (idx < format.size()) {
          if (std::holds_alternative<NewLine>(format[idx])) {
            if (idx != format.size() - 1) {
              if (!std::holds_alternative<FormattedString>(format[idx + 1]))
                break;
              if (!(std::get<FormattedString>(format[idx + 1]).format &
                    Format_Table))
                break;
            }
            if (!current_row.empty()) {
              table.push_back(current_row);
              current_row.clear();
            }
            ++idx;
            continue;
          }
          if (!std::holds_alternative<FormattedString>(format[idx])) {
            break;
          }
          auto fmt = std::get<FormattedString>(format[idx]);
          if (!(fmt.format & Format_Table))
            break;
          if (fmt.value == "|") {
            ++idx;
            continue;
          }
          current_row.push_back(fmt.value);
          table_elems.push_back(idx);
          ++idx;
        }

        if (!current_row.empty())
          table.push_back(current_row);

        for (auto &row : table) {
          if (col_count == 0) {
            col_count = row.size();
          } else {
            if (col_count != row.size()) {
              is_mismatched = true;
            }
          }
        }

        if (!is_mismatched) {
          col_lengths.clear();
          col_lengths.resize(col_count);

          for (size_t c = 0; c < col_count; ++c) {
            for (auto &row : table) {
              col_lengths[c] = std::max(col_lengths[c], row[c].size());
            }
          }
        }
      } else if (!(fmt.format & Format_Table)) {
        in_table = false;
      }

      ImFont *font = (fmt.format & Format_Bold) ? bold : plain;

      current_size = apply_head(font_size, fmt.format);

      float block_start = cy;

      if ((i == 0 || std::holds_alternative<NewLine>(format[i - 1])) &&
          fmt.format & Format_List) {
        cx += 15;
      }

      std::string text = fmt.value;
      size_t pos = 0;

      float gap_len{0};
      bool is_gap = (fmt.format & Format_Table && !is_mismatched);
      if (is_gap) {
        for (size_t m = 0; m < table_elems.size(); ++m) {
          size_t elem_idx = table_elems[m];
          if (i != elem_idx)
            continue;
          size_t col = m % col_count;
          size_t gap = col_lengths[col] - fmt.value.size();
          std::string fill{};
          for (size_t i = 0; i < gap; ++i)
            fill += " ";
          gap_len =
              font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX, fill.c_str()).x;
        }
      }

      if (is_gap)
        cx += gap_len / 2;

      while (pos < text.size()) {
        size_t next_space = text.find(' ', pos);
        if (next_space == std::string::npos)
          next_space = text.size();

        std::string word = text.substr(pos, next_space - pos);
        if (next_space < text.size())
          word += ' ';

        float word_width =
            font->CalcTextSizeA(current_size, FLT_MAX, FLT_MAX, word.c_str()).x;

        if (fmt.format & Format_Strike) {
          float sz = 1.0f;
          sz = apply_head(sz, fmt.format);
          draw_list->AddRectFilled(
              {cx, cy + current_size / 2 - sz / 2},
              {cx + word_width, cy + current_size / 2 + sz / 2},
              IM_COL32(0xFF, 0xFF, 0xFF, 0xFF));
        }

        if (fmt.format & Format_Table &&
            std::ranges::contains(table_elems, i) && !is_mismatched) {
          font = bold;
          fmt.format |= Format_Bold;
        }

        if (cx + word_width > content_x + content_w) {
          cx = content_x;
          cy += current_size;
          if (cy >= content_y + content_h - current_size)
            break;
        }

        size_t char_pos = 0;
        while (char_pos < word.size()) {
          size_t len = utf8_next_len(word, char_pos);

          if (idx == cursor) {
            std::string sub = word.substr(0, char_pos);
            float cursor_x = cx + font->CalcTextSizeA(current_size, FLT_MAX,
                                                      FLT_MAX, sub.c_str())
                                      .x;
            draw_cursor(cursor_x, cy, current_size);
          }

          if (deferred_gap) {
            cx += deferred_gap;
            deferred_gap = 0;
          }

          if (do_cursor_choose) {
            std::string sub = word.substr(0, char_pos);
            float cursor_x = cx + font->CalcTextSizeA(current_size, FLT_MAX,
                                                      FLT_MAX, sub.c_str())
                                      .x;
            float dx = cursor_x - choose_x;
            float dy = cy + current_size / 2 - choose_y;
            float dist = SDL_sqrtf(dx * dx + dy * dy);
            if (dist < closest_len) {
              closest_len = dist;
              closest_idx = idx;
            }
          }

          if (mode == EditorMode::Select && idx >= sel_start && idx < sel_end) {
            std::string sub = word.substr(0, char_pos + len);
            std::string prev = word.substr(0, char_pos);
            float sub_width =
                font->CalcTextSizeA(current_size, FLT_MAX, FLT_MAX, sub.c_str())
                    .x;
            float prev_width = font->CalcTextSizeA(current_size, FLT_MAX,
                                                   FLT_MAX, prev.c_str())
                                   .x;
            draw_selection(cx + prev_width, cy, sub_width - prev_width,
                           current_size);
          }

          idx += len;
          char_pos += len;
        }

        render(font, word, fmt.format);
        cx += word_width;
        pos = next_space + 1;
      }

      if (cursor != idx && is_gap)
        cx += gap_len / 2;
      else if (cursor == idx) {
        deferred_gap = gap_len / 2;
      }

      if (fmt.format & Format_Code) {
        draw_list->AddRectFilled({content_x, block_start},
                                 {content_x + 2, cy + current_size},
                                 IM_COL32(0xFF, 0xFF, 0xFF, 0xFF));
      }

      continue;
    }

    if (std::holds_alternative<Image>(token)) {
      in_table = false;
      if (row < row_start) {
        continue;
      }
      imgs_buffer.emplace_back(std::get<Image>(token));
      continue;
    }
  }

  if (format.size() > 0 && std::holds_alternative<NewLine>(format.back())) {
    draw_linenumber(row, format.back());
  }

  if (idx == cursor) {
    draw_cursor(cx, cy, current_size);
  }

  if (do_cursor_choose) {
    if (choose_y > cy + current_size) {
      cursor = text.size() == 0 ? 0 : text.size() - 1;
    } else {
      cursor = closest_idx;
    }
    do_cursor_choose = false;
  }

  row_max = std::max<float>(row, content_h / font_size);

  ImGui::End();

  if (show_error) {
    ImGui::OpenPopup("Error");
    show_error = false;
  }

  if (ImGui::BeginPopupModal("Error", NULL,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("%s", error.c_str());
    if (ImGui::Button("OK", ImVec2(80, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (ask_save) {
    if (!save_explorer.is_closed) {
      ImGui::SetNextWindowFocus();
      save_explorer.render();
    } else {
      ask_save = false;
    }
  }
}

void Editor::reparse() {
  Parser parser{text};
  parser.parse_all();
  format = std::move(parser.tokens);
  update_imgs();
}

void Editor::update_imgs() {
  for (auto &token : format) {
    if (std::holds_alternative<Image>(token)) {
      Image img = std::get<Image>(token);
      std::filesystem::path img_fp = get_path_proper(img.fp);
      SDL_Surface *surf = IMG_Load(img_fp.string().c_str());
      if (!surf) {
        SDL_Log("IMG_Load failed for '%s': %s", img_fp.string().c_str(),
                SDL_GetError());
        return;
      }
      SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
      SDL_DestroySurface(surf);
      if (!tex)
        return;
      if (images.contains(img_fp)) {
        SDL_DestroyTexture(reinterpret_cast<SDL_Texture *>(images[img_fp]));
      }
      images[img_fp] = reinterpret_cast<ImTextureID>(tex);
    };
  }
}

std::filesystem::path Editor::get_path_proper(std::filesystem::path img_fp) {
  if (std::filesystem::exists(img_fp) &&
      std::filesystem::is_regular_file(img_fp)) {
    return img_fp;
  } else {
    std::filesystem::path fp = filepath.parent_path() / img_fp;
    if (std::filesystem::exists(fp) && std::filesystem::is_regular_file(fp)) {
      return fp;
    }
  }
  return {};
}

void Editor::set_text(std::filesystem::path path, std::string &&text) {
  filepath = path;
  this->text = std::move(text);
  reparse();
  normalize_cursor();
  update_title();
}

void Editor::error_msg(std::string err) {
  error = err;
  show_error = true;
}

void Editor::save() {
  if (is_example())
    return;
  std::ofstream fs(filepath);
  if (!fs.is_open()) {
    if (filepath.empty()) {
      ask_save = true;
      save_explorer.is_closed = false;
    } else {
      error_msg("Failed to save file!");
    }
  }
  fs << text;
  fs.flush();
  fs.close();
  update_title();
}

void Editor::normalize_cursor() {
  if (cursor > text.size()) {
    cursor = text.size();
  }
  size_t row = std::count(text.begin(), text.begin() + cursor, '\n');
  if (row > row_max) {
    row_start += row - row_max;
  } else if (row < row_start) {
    row_start = row;
  }
}

Token Editor::get_hovered_token() {
  size_t idx{0};
  for (auto &token : format) {
    if (std::holds_alternative<NewLine>(token)) {
      ++idx;
    } else if (std::holds_alternative<FormattedString>(token)) {
      idx += std::get<FormattedString>(token).value.size();
    } else if (std::holds_alternative<Image>(token)) {
      // None
    }
    if (idx >= cursor) {
      return token;
    }
  }
  return format.size() == 0 ? Token{NewLine{}} : format.back();
}

float apply_head(float fsize, int head_n) {
  switch (head_n & (Format_Head1 | Format_Head2 | Format_Head3)) {
  case Format_Head1:
    return 2 * fsize;
  case Format_Head2:
    return 1.6 * fsize;
  case Format_Head3:
    return 1.2 * fsize;
  default:
    return fsize;
  }
}

bool Editor::is_save_needed() {
  if (is_example())
    return false;
  if (filepath.empty()) {
    return true;
  }

  std::string contents{read_file_text(filepath)};

  return contents != text;
}

void Editor::update_title() {
  std::string save{is_save_needed() ? "*" : ""};
  std::string fp{filepath.empty() ? "(No file)" : filepath.string()};
  std::string title{save + "Take Notes - " + fp};
  SDL_SetWindowTitle(window, title.c_str());
}

Editor::~Editor() {
  for (auto &pair : images) {
    SDL_DestroyTexture(reinterpret_cast<SDL_Texture *>(pair.second));
  }
}

bool Editor::is_example() { return filepath == example_file; }