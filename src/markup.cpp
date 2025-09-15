#include "markup.hpp"

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

Token Parser::parse_bold() {
  FormattedString str{};
  str.format |= Format_Bold;
  size_t start = cursor;
  while (!is_eof() && !match("**")) {
    Token t = parse();
    if (std::holds_alternative<NewLine>(t)) {
      cursor = start;
      FormattedString str = std::get<FormattedString>(parse_plain());
      str.value = "**" + str.value;
      return str;
    } else if (std::holds_alternative<FormattedString>(t)) {
      auto s = std::get<FormattedString>(t);
      str.format |= s.format;
    }
  }
  str.value = "**" + input.substr(start, cursor - start);
  return str;
}

Token Parser::parse_italic(std::string which) {
  FormattedString str{};
  str.format |= Format_Italic;
  size_t start = cursor;
  while (!is_eof() && !match(which)) {
    Token t = parse();
    if (std::holds_alternative<NewLine>(t)) {
      cursor = start;
      FormattedString str = std::get<FormattedString>(parse_plain());
      str.value = which + str.value;
      return str;
    } else if (std::holds_alternative<FormattedString>(t)) {
      auto s = std::get<FormattedString>(t);
      str.format |= s.format;
    }
  }
  str.value = which + input.substr(start, cursor - start);
  return str;
}

Token Parser::parse_plain() {
  FormattedString str{};
  char c;
  while (!is_eof() && !is_special(c = peek())) {
    bump();
    str.value.push_back(c);
  }
  return str;
}

Token Parser::parse() {
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
    return NewLine{};
  }
  return parse_plain();
}

void Parser::parse_all() {
  while (!is_eof()) {
    tokens.emplace_back(parse());
  }
}