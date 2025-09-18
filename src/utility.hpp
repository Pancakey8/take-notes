#include <cstddef>
#include <string>

size_t utf8_next_len(const std::string &s, size_t pos);

size_t utf8_prev_len(const std::string &s, size_t pos);
