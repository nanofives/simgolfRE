// GameFiles: the only way the simulation core reads the user's game data (phase 3.0, 2026-10-19; re/PHASE23_PLAN.md 8.4).
//
// Story file, golfer roster, string tables, course and theme files are read from the user's own install. The core
// never opens a file itself: it asks a GameFiles for BYTES BY LOGICAL NAME. A logical name is the file's path relative
// to the install root (the directory holding golf_clean.exe), compared case-insensitively with '/' and '\' equal, e.g.
// "Data/Golfers.pro" (the exact names are what the original passes to fopen / _open; each loader's port documents its
// own). Parsing a format is the loader's job (phase 3.5 for the story file and the roster, phase 4 for the rest), never
// this interface's.
//
// Implementations:
//   MemoryGameFiles  in-memory name -> bytes (tests: fed by the harness from the corpus or from the user's install)
//   DirGameFiles     reads <root>/<name> from disk (the user's install; plain bytes, no format)
//   NullGameFiles    every file absent (a context with no files attached behaves the same: MissingResource)
// A port function reaches the context's instance through ctx.files() (throws MissingResource when none is attached).
#ifndef PORTSIM_GAMEFILES_H
#define PORTSIM_GAMEFILES_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace portsim {

class GameFiles {
 public:
  virtual ~GameFiles() = default;
  // size in bytes, or -1 when the file does not exist
  virtual int64_t size(const std::string &name) const = 0;
  // copies up to `cap` bytes starting at `offset` into buf; returns the count copied (0 at or past the end), or -1 when
  // the file does not exist
  virtual int64_t read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const = 0;

  bool exists(const std::string &name) const { return size(name) >= 0; }
  // the whole file; throws MissingResource when it does not exist
  std::vector<uint8_t> read_all(const std::string &name) const;

  // the canonical form of a logical name: ASCII lower case, '\' -> '/', no leading "./" or '/'
  static std::string normalize(const std::string &name);
};

class NullGameFiles : public GameFiles {
 public:
  int64_t size(const std::string &) const override { return -1; }
  int64_t read(const std::string &, uint64_t, void *, uint64_t) const override { return -1; }
};

class MemoryGameFiles : public GameFiles {
 public:
  void put(const std::string &name, std::vector<uint8_t> bytes) { files_[normalize(name)] = std::move(bytes); }
  bool remove(const std::string &name) { return files_.erase(normalize(name)) != 0; }
  void clear() { files_.clear(); }
  size_t count() const { return files_.size(); }
  int64_t size(const std::string &name) const override;
  int64_t read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const override;

 private:
  std::map<std::string, std::vector<uint8_t>> files_;
};

class DirGameFiles : public GameFiles {
 public:
  explicit DirGameFiles(std::string root) : root_(std::move(root)) {}
  const std::string &root() const { return root_; }
  int64_t size(const std::string &name) const override;
  int64_t read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const override;

 private:
  std::string path_of(const std::string &name) const;
  std::string root_;
};

}  // namespace portsim

#endif  // PORTSIM_GAMEFILES_H
