#pragma once
#include <string>
#include <variant>
#include <vector>

enum Format : int {
  Format_Plain = 0x0,
  Format_Italic = 0x1,
  Format_Bold = 0x2,
  Format_Head1 = 0x4,
  Format_Head2 = 0x8,
  Format_Head3 = 0x10,
  Format_Code = 0x20,
  Format_Strike = 0x40,
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
  bool is_special();

  std::vector<Token> parse_wrapped(std::string which, Format fmt);

public:
  std::vector<Token> tokens;

  Parser(std::string input) : input(input) {}
  Parser(std::string &&input) : input(std::move(input)) {}

  std::vector<Token> parse_bold();
  std::vector<Token> parse_italic(std::string which);
  std::vector<Token> parse_strike();
  std::vector<Token> parse_head(std::string which, Format head_n);
  std::vector<Token> parse_line_begin();
  std::vector<Token> parse_code();
  std::vector<Token> parse_plain();
  std::vector<Token> parse();
  void parse_all();
};