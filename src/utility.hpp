#include <cstddef>
#include <filesystem>
#include <string>

size_t utf8_next_len(const std::string &s, size_t pos);

size_t utf8_prev_len(const std::string &s, size_t pos);

std::string read_file_binary(const std::filesystem::path &filepath);

std::string read_file_text(const std::filesystem::path &filepath);