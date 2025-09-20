#pragma once
#include <filesystem>
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
  Format_List = 0x80,
  Format_Table = 0x100,
};

struct FormattedString {
  int format{0};
  std::string value{};
};

struct Image {
  std::filesystem::path fp{};
};

struct NewLine {};

using Token = std::variant<NewLine, FormattedString, Image>;

class Parser {
  std::string input;
  size_t cursor{0};

  bool is_eof();
  bool bump();
  std::string peek();
  bool match(std::string pat);
  bool is_special();

  std::vector<Token> parse_wrapped(std::string which, Format fmt);
  std::vector<Token> parse_line_wide(std::string which, Format fmt);

public:
  std::vector<Token> tokens;

  Parser(std::string input) : input(input) {}
  Parser(std::string &&input) : input(std::move(input)) {}

  std::vector<Token> parse_bold();
  std::vector<Token> parse_italic(std::string which);
  std::vector<Token> parse_strike();
  std::vector<Token> parse_head(std::string which, Format head_n);
  std::vector<Token> parse_list();
  std::vector<Token> parse_image();
  std::vector<Token> parse_table();
  std::vector<Token> parse_line_begin();
  std::vector<Token> parse_code();
  std::vector<Token> parse_plain();
  std::vector<Token> parse();
  void parse_all();
};