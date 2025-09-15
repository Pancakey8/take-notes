#pragma once
#include <string>
#include <variant>
#include <vector>

enum Format : int {
  Format_Plain = 0x0,
  Format_Italic = 0x1,
  Format_Bold = 0x2,
  Format_Head = 0x4
};

struct FormattedString {
  int format;
  std::string value;
};

struct NewLine {};

using Token = std::variant<NewLine, FormattedString>;

class Parser {
  std::string input;
  size_t cursor{0};

  bool is_eof();
  bool bump();
  char peek();
  bool match(std::string pat);

public:
  std::vector<Token> tokens;

  Parser(std::string input) : input(input) {}
  Parser(std::string &&input) : input(std::move(input)) {}

  Token parse_bold();
  Token parse_italic(std::string which);
  Token parse_plain();
  Token parse();
  void parse_all();
};