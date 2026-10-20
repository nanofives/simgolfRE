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
//
// Phase 3.8 (stream 38b) added two services the save / load code needs, both optional (a default that does nothing):
//   list(pattern)    the original's FindFirstFileA / FindNextFileA walk over one directory (see DirEntry)
//   write(name, ..)  creates or replaces a file (saved games); false = this implementation is read-only
// A port function reaches the context's instance through ctx.files() (throws MissingResource when none is attached).
#ifndef PORTSIM_GAMEFILES_H
#define PORTSIM_GAMEFILES_H

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace portsim {

// One entry of a directory listing: the file's own name (its case as stored) and whether it is a directory.
struct DirEntry {
  std::string name;
  bool is_dir = false;
};

class GameFiles {
 public:
  virtual ~GameFiles() = default;
  // size in bytes, or -1 when the file does not exist
  virtual int64_t size(const std::string &name) const = 0;
  // copies up to `cap` bytes starting at `offset` into buf; returns the count copied (0 at or past the end), or -1 when
  // the file does not exist
  virtual int64_t read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const = 0;

  // What FindFirstFileA(pattern) and FindNextFileA return for a logical pattern `<dir>/<name pattern>` (`\` or `/`; the
  // directory part has no wildcard): the entries of <dir> (relative to the install root, empty = the root itself) whose
  // name matches the last component, in the order NTFS lists them. The directory's own entries "." and ".." come first
  // (they match a pattern such as "*"), the others follow sorted by their upper-cased name; subdirectories are entries
  // with is_dir set. An empty result is FindFirstFileA's INVALID_HANDLE_VALUE (no such directory, or no match).
  // Modelled: `*` (any run, also empty), `?` (one character), case-insensitive names, "*.*" = "*". Not modelled: the
  // 8.3 short names a real volume also matches (a pattern `*.sve` finds `a.svex` there) and wildcards in the directory
  // part (U-0970).
  virtual std::vector<DirEntry> list(const std::string &pattern) const {
    (void)pattern;
    return {};
  }
  // Creates or replaces the file `name` (and the directories above it); false when this implementation cannot write.
  virtual bool write(const std::string &name, const std::vector<uint8_t> &bytes) {
    (void)name;
    (void)bytes;
    return false;
  }
  // Whether write() can succeed at all (the original's _open for writing fails for a read-only or missing directory).
  virtual bool writable() const { return false; }
  // Deletes the file `name` (the browser's delete button: _remove); false when it does not exist or cannot be deleted.
  virtual bool remove(const std::string &name) {
    (void)name;
    return false;
  }

  bool exists(const std::string &name) const { return size(name) >= 0; }
  // the whole file; throws MissingResource when it does not exist
  std::vector<uint8_t> read_all(const std::string &name) const;

  // the canonical form of a logical name: ASCII lower case, '\' -> '/', no leading "./" or '/'
  static std::string normalize(const std::string &name);
  // FindFirstFileA's name test (see list): `pattern` and `name` are single path components
  static bool wildcard_match(const std::string &pattern, const std::string &name);
  // The listing both implementations share: `names` are the entries of the directory (without "." and ".."), `dir_exists`
  // says whether the directory is there at all; applies the pattern, adds "." / ".." and sorts.
  static std::vector<DirEntry> make_listing(const std::string &pattern_component, std::vector<DirEntry> names,
                                            bool dir_exists);
  // splits `<dir>/<component>` of a pattern: dir normalised ('/', lower case, no leading or trailing '/'), component as given
  static void split_pattern(const std::string &pattern, std::string &dir, std::string &component);
};

class NullGameFiles : public GameFiles {
 public:
  int64_t size(const std::string &) const override { return -1; }
  int64_t read(const std::string &, uint64_t, void *, uint64_t) const override { return -1; }
};

class MemoryGameFiles : public GameFiles {
 public:
  void put(const std::string &name, std::vector<uint8_t> bytes);
  bool remove(const std::string &name) override { return files_.erase(normalize(name)) != 0; }
  void clear() { files_.clear(); }
  size_t count() const { return files_.size(); }
  int64_t size(const std::string &name) const override;
  int64_t read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const override;
  std::vector<DirEntry> list(const std::string &pattern) const override;
  bool write(const std::string &name, const std::vector<uint8_t> &bytes) override {
    put(name, bytes);
    return true;
  }
  bool writable() const override { return true; }
  // the entries directly below the normalised directory `dir` (no "." / ".."), and whether the directory exists here
  void collect_dir(const std::string &dir, std::vector<DirEntry> &names, bool &exists) const;

 private:
  struct Entry {
    std::string name;  // the name as put (its case), '/'-separated
    std::vector<uint8_t> bytes;
  };
  std::map<std::string, Entry> files_;
};

// The user's install on disk. READS go to the files below `root`. WRITES (saved games) and deletes stay in memory, in an
// overlay over the disk that reads and listings see, and never touch the install, unless write-through is on
// (set_write_through, or the environment variable SG_FILES_WRITE_THROUGH=1 at construction: the host application's
// saved-games directory, and the tests that read a written file back from disk).
class DirGameFiles : public GameFiles {
 public:
  explicit DirGameFiles(std::string root);
  const std::string &root() const { return root_; }
  void set_write_through(bool on) { write_through_ = on; }
  bool write_through() const { return write_through_; }
  int64_t size(const std::string &name) const override;
  int64_t read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const override;
  std::vector<DirEntry> list(const std::string &pattern) const override;
  bool write(const std::string &name, const std::vector<uint8_t> &bytes) override;
  bool writable() const override { return true; }
  bool remove(const std::string &name) override;

 private:
  std::string path_of(const std::string &name) const;
  std::string root_;
  bool write_through_ = false;
  MemoryGameFiles overlay_;
  std::set<std::string> deleted_;  // normalised logical names removed from the disk's view
};

}  // namespace portsim

#endif  // PORTSIM_GAMEFILES_H
