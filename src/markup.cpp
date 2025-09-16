#include "markup.hpp"
#include <iostream>

bool Parser::is_eof() { return cursor >= input.size(); }
bool Parser::bump() {
  if (is_eof())
    return false;
  ++cursor;
  return true;
}
char Parser::peek() {
  if (is_eof())
    return EOF;

  return input[cursor];
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

bool is_special(char c) { return c == '*' || c == '\n' || c == '/'; }

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
    std::vector<Token> toks = parse();
    for (auto &tok : toks) {
      if (std::holds_alternative<NewLine>(tok)) {
        total.emplace_back(tok);
        continue;
      }
      FormattedString fmt = std::get<FormattedString>(tok);
      fmt.format |= format;
      total.emplace_back(fmt);
    }
    if (match(which)) {
      is_closed = true;
      break;
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

std::vector<Token> Parser::parse_italic(std::string which) {
  return parse_wrapped(which, Format_Italic);
}

std::vector<Token> Parser::parse_head() {
  std::vector<Token> total{FormattedString{Format_Head, "#"}};
  while (!is_eof()) {
    if (match("\n")) {
      total.push_back(NewLine{});
      break;
    }
    std::vector<Token> toks = parse();
    for (auto &tok : toks) {
      if (std::holds_alternative<NewLine>(tok)) {
        total.emplace_back(tok);
        continue;
      }
      FormattedString fmt = std::get<FormattedString>(tok);
      fmt.format |= Format_Head;
      total.emplace_back(fmt);
    }
  }
  return total;
}

std::vector<Token> Parser::parse_plain() {
  FormattedString str{};
  char c;
  while (!is_eof() && !is_special(c = peek())) {
    bump();
    str.value.push_back(c);
  }
  return {str};
}

std::vector<Token> Parser::parse() {
  if (match("**")) {
    return parse_bold();
  }
  if (match("/")) {
    return parse_italic("/");
  }
  if (match("*")) {
    return parse_italic("*");
  }
  if (match("\n")) {
    std::vector<Token> toks{NewLine{}};
    if (match("#")) {
      auto p = parse_head();
      toks.insert(toks.end(), p.begin(), p.end());
    }
    return toks;
  }
  if (cursor == 0 && match("#")) {
    return parse_head();
  }
  return parse_plain();
}

void Parser::parse_all() {
  while (!is_eof()) {
    auto p = parse();
    tokens.insert(tokens.end(), p.begin(), p.end());
  }
}