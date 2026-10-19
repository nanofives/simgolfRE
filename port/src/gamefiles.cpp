// GameFiles implementations (portsim/gamefiles.h), phase 3.0.
#define _CRT_SECURE_NO_WARNINGS  // std::fopen (MSVC /W4 /WX)
#include "portsim/gamefiles.h"

#include <cstdio>
#include <cstring>

#include "portsim/registry.h"

namespace portsim {

std::string GameFiles::normalize(const std::string &name) {
  std::string s;
  s.reserve(name.size());
  for (char c : name) {
    if (c == '\\') c = '/';
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    s.push_back(c);
  }
  while (s.compare(0, 2, "./") == 0) s.erase(0, 2);
  while (!s.empty() && s[0] == '/') s.erase(0, 1);
  return s;
}

std::vector<uint8_t> GameFiles::read_all(const std::string &name) const {
  const int64_t n = size(name);
  if (n < 0) throw MissingResource{"game file not found"};
  std::vector<uint8_t> out(static_cast<size_t>(n));
  if (n > 0 && read(name, 0, out.data(), static_cast<uint64_t>(n)) != n) throw MissingResource{"game file short read"};
  return out;
}

int64_t MemoryGameFiles::size(const std::string &name) const {
  auto it = files_.find(normalize(name));
  return it == files_.end() ? -1 : static_cast<int64_t>(it->second.size());
}

int64_t MemoryGameFiles::read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const {
  auto it = files_.find(normalize(name));
  if (it == files_.end()) return -1;
  const uint64_t n = it->second.size();
  if (offset >= n || cap == 0) return 0;
  const uint64_t k = n - offset < cap ? n - offset : cap;
  std::memcpy(buf, it->second.data() + offset, static_cast<size_t>(k));
  return static_cast<int64_t>(k);
}

std::string DirGameFiles::path_of(const std::string &name) const {
  // the logical name as given (file systems of the user's install are case-insensitive on Windows)
  std::string rel = name;
  for (char &c : rel)
    if (c == '\\') c = '/';
  return root_.empty() ? rel : root_ + "/" + rel;
}

int64_t DirGameFiles::size(const std::string &name) const {
  FILE *f = std::fopen(path_of(name).c_str(), "rb");
  if (!f) return -1;
  int64_t n = -1;
#ifdef _WIN32
  if (_fseeki64(f, 0, SEEK_END) == 0) n = _ftelli64(f);
#else
  if (std::fseek(f, 0, SEEK_END) == 0) n = std::ftell(f);
#endif
  std::fclose(f);
  return n;
}

int64_t DirGameFiles::read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const {
  FILE *f = std::fopen(path_of(name).c_str(), "rb");
  if (!f) return -1;
#ifdef _WIN32
  const bool ok = _fseeki64(f, static_cast<int64_t>(offset), SEEK_SET) == 0;
#else
  const bool ok = std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0;
#endif
  const int64_t n = ok ? static_cast<int64_t>(std::fread(buf, 1, static_cast<size_t>(cap), f)) : 0;
  std::fclose(f);
  return n;
}

}  // namespace portsim
