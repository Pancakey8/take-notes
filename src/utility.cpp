#include "utility.hpp"
#include <filesystem>
#include <fstream>
#include <ios>

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

std::string read_file_binary(const std::filesystem::path &filepath) {
  std::ifstream fs(filepath, std::ios::in | std::ios::binary);
  fs.seekg(0, std::ios::end);
  size_t sz = fs.tellg();
  fs.seekg(0, std::ios::beg);
  std::string contents{};
  contents.resize(sz);
  fs.read(contents.data(), sz);
  return contents;
}

std::string read_file_text(const std::filesystem::path &filepath) {
  std::ifstream fs(filepath);
  fs.seekg(0, std::ios::end);
  size_t sz = fs.tellg();
  fs.seekg(0, std::ios::beg);
  std::string contents{};
  contents.resize(sz);
  fs.read(contents.data(), sz);
  // For Windows
  for (std::string::size_type pos = 0;
       (pos = contents.find("\r\n", pos)) != std::string::npos; pos += 1) {
    contents.replace(pos, 2, "\n");
  }
  return contents;
}
