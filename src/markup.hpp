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

  std::vector<Token> parse_wrapped(std::string which, Format fmt);

public:
  std::vector<Token> tokens;

  Parser(std::string input) : input(input) {}
  Parser(std::string &&input) : input(std::move(input)) {}

  std::vector<Token> parse_bold();
  std::vector<Token> parse_italic(std::string which);
  std::vector<Token> parse_head();
  std::vector<Token> parse_plain();
  std::vector<Token> parse();
  void parse_all();
};