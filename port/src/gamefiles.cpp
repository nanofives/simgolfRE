// GameFiles implementations (portsim/gamefiles.h), phase 3.0.
#define _CRT_SECURE_NO_WARNINGS  // std::fopen (MSVC /W4 /WX)
#include "portsim/gamefiles.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

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

namespace {
char upper(char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c; }
bool match_from(const std::string &p, size_t pi, const std::string &n, size_t ni) {
  while (pi < p.size()) {
    if (p[pi] == '*') {
      while (pi < p.size() && p[pi] == '*') ++pi;
      if (pi == p.size()) return true;
      for (size_t k = ni; k <= n.size(); ++k)
        if (match_from(p, pi, n, k)) return true;
      return false;
    }
    if (ni >= n.size()) return false;
    if (p[pi] != '?' && upper(p[pi]) != upper(n[ni])) return false;
    ++pi;
    ++ni;
  }
  return ni == n.size();
}
}  // namespace

bool GameFiles::wildcard_match(const std::string &pattern, const std::string &name) {
  const std::string p = pattern == "*.*" ? std::string("*") : pattern;
  return match_from(p, 0, name, 0);
}

void GameFiles::split_pattern(const std::string &pattern, std::string &dir, std::string &component) {
  std::string s = pattern;
  for (char &c : s)
    if (c == '\\') c = '/';
  const size_t slash = s.rfind('/');
  if (slash == std::string::npos) {
    dir.clear();
    component = s;
  } else {
    dir = normalize(s.substr(0, slash));
    while (!dir.empty() && dir.back() == '/') dir.pop_back();
    component = s.substr(slash + 1);
  }
}

std::vector<DirEntry> GameFiles::make_listing(const std::string &component, std::vector<DirEntry> names,
                                              bool dir_exists) {
  std::vector<DirEntry> out;
  if (!dir_exists) return out;
  std::vector<DirEntry> rest;
  for (const char *dots : {".", ".."}) {
    DirEntry e;
    e.name = dots;
    e.is_dir = true;
    if (wildcard_match(component, e.name)) out.push_back(e);
  }
  for (DirEntry &e : names)
    if (wildcard_match(component, e.name)) rest.push_back(std::move(e));
  std::stable_sort(rest.begin(), rest.end(), [](const DirEntry &a, const DirEntry &b) {
    std::string ua = a.name, ub = b.name;
    for (char &c : ua) c = upper(c);
    for (char &c : ub) c = upper(c);
    return ua < ub;
  });
  out.insert(out.end(), rest.begin(), rest.end());
  return out;
}

std::vector<uint8_t> GameFiles::read_all(const std::string &name) const {
  const int64_t n = size(name);
  if (n < 0) throw MissingResource{"game file not found"};
  std::vector<uint8_t> out(static_cast<size_t>(n));
  if (n > 0 && read(name, 0, out.data(), static_cast<uint64_t>(n)) != n) throw MissingResource{"game file short read"};
  return out;
}

void MemoryGameFiles::put(const std::string &name, std::vector<uint8_t> bytes) {
  Entry e;
  e.name = name;
  for (char &c : e.name)
    if (c == '\\') c = '/';
  while (e.name.compare(0, 2, "./") == 0) e.name.erase(0, 2);
  while (!e.name.empty() && e.name[0] == '/') e.name.erase(0, 1);
  e.bytes = std::move(bytes);
  files_[normalize(name)] = std::move(e);
}

int64_t MemoryGameFiles::size(const std::string &name) const {
  auto it = files_.find(normalize(name));
  return it == files_.end() ? -1 : static_cast<int64_t>(it->second.bytes.size());
}

std::vector<DirEntry> MemoryGameFiles::list(const std::string &pattern) const {
  std::string dir, component;
  split_pattern(pattern, dir, component);
  std::vector<DirEntry> names;
  bool exists = false;
  collect_dir(dir, names, exists);
  return make_listing(component, std::move(names), exists);
}

void MemoryGameFiles::collect_dir(const std::string &dir, std::vector<DirEntry> &names, bool &exists) const {
  const std::string prefix = dir.empty() ? std::string() : dir + "/";
  std::set<std::string> seen;
  exists = dir.empty();
  for (const auto &kv : files_) {
    if (kv.first.compare(0, prefix.size(), prefix) != 0) continue;
    exists = true;
    const std::string &stored = kv.second.name;
    const std::string rest = stored.substr(prefix.size());
    const size_t slash = rest.find('/');
    DirEntry e;
    e.name = slash == std::string::npos ? rest : rest.substr(0, slash);
    e.is_dir = slash != std::string::npos;
    if (seen.insert(normalize(e.name)).second) names.push_back(e);
  }
}

int64_t MemoryGameFiles::read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const {
  auto it = files_.find(normalize(name));
  if (it == files_.end()) return -1;
  const uint64_t n = it->second.bytes.size();
  if (offset >= n || cap == 0) return 0;
  const uint64_t k = n - offset < cap ? n - offset : cap;
  std::memcpy(buf, it->second.bytes.data() + offset, static_cast<size_t>(k));
  return static_cast<int64_t>(k);
}

std::string DirGameFiles::path_of(const std::string &name) const {
  // the logical name as given (file systems of the user's install are case-insensitive on Windows)
  std::string rel = name;
  for (char &c : rel)
    if (c == '\\') c = '/';
  return root_.empty() ? rel : root_ + "/" + rel;
}

DirGameFiles::DirGameFiles(std::string root) : root_(std::move(root)) {
  const char *e = std::getenv("SG_FILES_WRITE_THROUGH");
  write_through_ = e != nullptr && e[0] == '1';
}

int64_t DirGameFiles::size(const std::string &name) const {
  const std::string key = normalize(name);
  if (deleted_.count(key)) return -1;
  const int64_t o = overlay_.size(name);
  if (o >= 0) return o;
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
  const std::string key = normalize(name);
  if (deleted_.count(key)) return -1;
  if (overlay_.size(name) >= 0) return overlay_.read(name, offset, buf, cap);
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

std::vector<DirEntry> DirGameFiles::list(const std::string &pattern) const {
  namespace fs = std::filesystem;
  std::string dir, component;
  split_pattern(pattern, dir, component);
  // the directory as the caller spelled it (the file system decides about case), not the lower-cased form
  std::string raw = pattern;
  for (char &c : raw)
    if (c == '\\') c = '/';
  const size_t slash = raw.rfind('/');
  std::string rdir = slash == std::string::npos ? std::string() : raw.substr(0, slash);
  while (rdir.compare(0, 2, "./") == 0) rdir.erase(0, 2);
  const std::string path = root_.empty() ? (rdir.empty() ? "." : rdir) : (rdir.empty() ? root_ : root_ + "/" + rdir);
  std::error_code ec;
  std::vector<DirEntry> names;
  std::set<std::string> seen;
  bool exists = false;
  if (fs::is_directory(fs::path(path), ec)) {
    exists = true;
    const std::string prefix = dir.empty() ? std::string() : dir + "/";
    for (fs::directory_iterator it(fs::path(path), ec), end; !ec && it != end; it.increment(ec)) {
      DirEntry e;
      e.name = it->path().filename().string();
      e.is_dir = it->is_directory(ec);
      if (!e.is_dir && deleted_.count(prefix + normalize(e.name))) continue;
      seen.insert(normalize(e.name));
      names.push_back(std::move(e));
    }
  }
  std::vector<DirEntry> extra;
  bool extra_exists = false;
  overlay_.collect_dir(dir, extra, extra_exists);
  for (DirEntry &e : extra)
    if (seen.insert(normalize(e.name)).second) names.push_back(std::move(e));
  return make_listing(component, std::move(names), exists || (extra_exists && !dir.empty()));
}

bool DirGameFiles::write(const std::string &name, const std::vector<uint8_t> &bytes) {
  if (!write_through_) {
    deleted_.erase(normalize(name));
    overlay_.put(name, bytes);
    return true;
  }
  namespace fs = std::filesystem;
  const fs::path p(path_of(name));
  std::error_code ec;
  if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  if (!bytes.empty()) f.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(f);
}

bool DirGameFiles::remove(const std::string &name) {
  if (!write_through_) {
    const bool existed = size(name) >= 0;
    overlay_.remove(name);
    if (existed) deleted_.insert(normalize(name));
    return existed;
  }
  std::error_code ec;
  return std::filesystem::remove(std::filesystem::path(path_of(name)), ec) && !ec;
}

}  // namespace portsim
