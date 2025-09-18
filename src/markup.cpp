#include "markup.hpp"
#include "utility.hpp"
#include <iostream>

bool Parser::is_eof() { return cursor >= input.size(); }
bool Parser::bump() {
  if (is_eof())
    return false;
  cursor += utf8_next_len(input, cursor);
  return true;
}
std::string Parser::peek() {
  if (is_eof())
    return "";

  size_t next = utf8_next_len(input, cursor);
  return input.substr(cursor, next);
}
bool Parser::match(std::string pat) {
  if (cursor + pat.size() - 1 >= input.size())
    return false;
  for (size_t i = 0; i < pat.size(); ++i) {
    if (pat[i] != input[cursor + i])
      return false;
  }
  cursor += pat.size();
  return true;
}

bool Parser::is_special() {
  if (is_eof())
    return true;
  std::string c = peek();
  bool spec = c == "*" || c == "\n" || c == "/" || c == "\\" || c == "[";
  if (cursor + 2 < input.size()) {
    std::string pat2 = input.substr(cursor, 2);
    spec |= pat2 == "~~";
  }
  return spec;
}

std::vector<Token> Parser::parse_wrapped(std::string which, Format format) {
  std::vector<Token> total{FormattedString{format, which}};
  size_t start = cursor;
  bool is_closed{false};
  while (!is_eof()) {
    if (match("\n")) {
      size_t end = cursor - 1;
      return {FormattedString{Format_Plain,
                              which + input.substr(start, end - start)},
              NewLine{}};
    }
    if (match(which)) {
      is_closed = true;
      break;
    }
    std::vector<Token> toks = parse();
    for (auto &tok : toks) {
      if (!std::holds_alternative<FormattedString>(tok)) {
        total.emplace_back(tok);
        continue;
      }
      FormattedString fmt = std::get<FormattedString>(tok);
      fmt.format |= format;
      total.emplace_back(fmt);
    }
  }
  if (is_closed) {
    total.push_back(FormattedString{format, which});
  }
  return total;
}

std::vector<Token> Parser::parse_bold() {
  return parse_wrapped("**", Format_Bold);
}

std::vector<Token> Parser::parse_strike() {
  return parse_wrapped("~~", Format_Strike);
}

std::vector<Token> Parser::parse_italic(std::string which) {
  return parse_wrapped(which, Format_Italic);
}

std::vector<Token> Parser::parse_image() {
  size_t start{cursor};
  bool is_closed{false};
  while (!is_eof()) {
    if (peek() == "\n")
      break;
    if (match("]")) {
      is_closed = true;
      break;
    }
    bump();
  }
  if (!is_closed) {
    return {FormattedString{Format_Plain,
                            "[" + input.substr(start, cursor - start)}};
  } else {
    std::filesystem::path fp{input.substr(start, cursor - start - 1)};
    return {FormattedString{Format_Plain,
                            "[" + input.substr(start, cursor - start)},
            Image{fp}};
  }
}

std::vector<Token> Parser::parse_line_wide(std::string which, Format format) {
  std::vector<Token> total{FormattedString{format, which}};
  while (!is_eof()) {
    if (peek() == "\n") {
      break;
    }
    std::vector<Token> toks = parse();
    for (auto &tok : toks) {
      if (!std::holds_alternative<FormattedString>(tok)) {
        total.emplace_back(tok);
        continue;
      }
      FormattedString fmt = std::get<FormattedString>(tok);
      fmt.format |= format;
      total.emplace_back(fmt);
    }
  }
  return total;
}

std::vector<Token> Parser::parse_head(std::string which, Format head_n) {
  return parse_line_wide(which, head_n);
}

std::vector<Token> Parser::parse_plain() {
  FormattedString str{};
  while (!is_eof() && !is_special()) {
    std::string c = peek();
    bump();
    str.value.append(c);
  }
  return {str};
}

std::vector<Token> Parser::parse_code() {
  size_t end = input.find('\n', cursor);
  if (end == input.npos) {
    end = input.size();
  }
  std::vector<Token> toks{
      FormattedString{Format_Code, "\t" + input.substr(cursor, end - cursor)}};
  cursor = end;
  return toks;
}

std::vector<Token> Parser::parse_list() {
  return parse_line_wide("•", Format_List);
}

std::vector<Token> Parser::parse_line_begin() {
  if (match("###")) {
    return parse_head("###", Format_Head3);
  } else if (match("##")) {
    return parse_head("##", Format_Head2);
  } else if (match("#")) {
    return parse_head("#", Format_Head1);
  } else if (match("\t")) {
    return parse_code();
  } else if (match("•")) {
    return parse_list();
  }
  return parse_plain();
}

std::vector<Token> Parser::parse() {
  if (match("\\")) {
    if (!is_eof() && is_special()) {
      std::string c = peek();
      bump();
      if (c == "\n") {
        return {FormattedString{Format_Plain, "\\"}, NewLine{}};
      }
      return {FormattedString{Format_Plain, "\\" + c}};
    } else {
      return {FormattedString{Format_Plain, "\\"}};
    }
  }
  if (match("**")) {
    return parse_bold();
  }
  if (match("~~")) {
    return parse_strike();
  }
  if (match("/")) {
    return parse_italic("/");
  }
  if (match("*")) {
    return parse_italic("*");
  }
  if (match("[")) {
    return parse_image();
  }
  if (match("\n")) {
    std::vector<Token> toks{NewLine{}};
    auto p = parse_line_begin();
    toks.insert(toks.end(), p.begin(), p.end());
    return toks;
  }
  if (cursor == 0) {
    return parse_line_begin();
  }
  return parse_plain();
}

void Parser::parse_all() {
  while (!is_eof()) {
    auto p = parse();
    tokens.insert(tokens.end(), p.begin(), p.end());
  }
}