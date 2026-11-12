// The app's game files: the user's install READ-ONLY plus a saves overlay in the app's own data directory (roadmap S7.3).
//
// The port reads every game file by logical name (portsim/gamefiles.h: the path relative to the install root). The host
// gives it an AppFiles:
//   - reads look in the data directory first, then in the fixtures (add_fixture), then in the install (a saved game the port wrote shadows the install's);
//   - writes (saved games, top10.sve, Default.pro, course files ...) go to <data>/<logical name>, never to the install;
//   - deletes (the load browser's delete button) remove the data directory's copy and hide the install's copy with a
//     tombstone kept in <data>/deleted.lst, so the install is never changed;
//   - listings (FindFirstFileA walks) merge both directories, minus the tombstones, in GameFiles::make_listing's order;
//   - names starting with '$' (`$gfx/...`: the graphics backend's snapshots) live in memory only.
// A data directory inside the install (or equal to it) is refused at construction, and a logical name that could leave
// the data directory (`..`, a drive or a stream name, an absolute path) is refused at write and remove: the original's
// _open then fails, which the game already handles.
#ifndef PORTAPP_SAVES_H
#define PORTAPP_SAVES_H

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "portsim/gamefiles.h"

namespace portapp {

// Deliberately wrong bodies for the negative controls (`--negctl saves-*`; production code uses None).
enum class SavesDefect : uint8_t {
  None = 0,
  WriteToInstall = 1,     // writes go to the install directory instead of the data directory
  NoReadBack = 2,         // reads ignore the data directory (a written save is not read back)
  AllowInsideInstall = 3, // a data directory inside the install is accepted
};

class AppFiles : public portsim::GameFiles {
 public:
  // install: the directory holding the user's golf_clean.exe; data: the app's data directory (created when missing).
  // ok() is false (and error() says why) when the install is missing or the data directory is inside the install.
  AppFiles(const std::string &install, const std::string &data, SavesDefect defect = SavesDefect::None);
  bool ok() const { return error_.empty(); }
  const std::string &error() const { return error_; }
  const std::string &install_root() const { return install_; }
  const std::string &data_root() const { return data_; }

  int64_t size(const std::string &name) const override;
  int64_t read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const override;
  std::vector<portsim::DirEntry> list(const std::string &pattern) const override;
  bool write(const std::string &name, const std::vector<uint8_t> &bytes) override;
  bool writable() const override { return ok(); }
  bool remove(const std::string &name) override;

  // P23A item 2: list the "Saved Games" directory newest write first (the original: name order). Off by default here.
  // P23B fix 3: the order is by EVENTS (portsim::SavesOrder), so a recording replays anywhere: the saves present now are ranked
  // once by their write times (newest first, equal times in name order), later writes go to the front in write order.
  // session.json carries saves_order = that initial ranking; the replay gives it to sg_files_saves_order.
  void set_newest_first(bool on);
  bool newest_first() const { return saves_.on(); }
  const std::vector<std::string> &saves_order() const { return saves_.initial(); }

  // A file of the install the recording ran against that the user's install lacks (a scenario's fixture save, port/test/
  // segment_files.py): kept in memory, read after the data directory and before the install, listed with the install.
  void add_fixture(const std::string &name, std::vector<uint8_t> bytes) { fixtures_.put(name, std::move(bytes)); }

  // P24D (lang_ui.h): while portsim::lang::language() is `lang`, a name the directory `dir` holds (the language's redrawn art,
  // game\Lang\es\<name> in the LAN build) is read from there instead of the install (after the data directory's saves)
  void set_lang_overlay(int lang, const std::string &dir);

  // whether a logical name stays inside a root directory (no `..`, drive, stream or absolute part)
  static bool safe_name(const std::string &name);
  // the default data directory: %LOCALAPPDATA%\SimGolfPort (else the current directory's SimGolfPort)
  static std::string default_data_dir();
  // writes refused so far (unsafe names, or no data directory)
  uint32_t refused() const { return refused_; }

 private:
  bool tombstoned(const std::string &name) const;
  void save_tombstones() const;
  std::string install_, data_;
  std::string error_;
  SavesDefect defect_;
  std::unique_ptr<portsim::DirGameFiles> install_files_;  // read-only use: write / remove are never called on it
  std::unique_ptr<portsim::DirGameFiles> data_files_;     // write-through: its writes land under data_
  std::unique_ptr<portsim::DirGameFiles> lang_files_;     // P24D: set_lang_overlay (read-only use)
  int lang_id_ = -1;
  mutable std::set<std::string> lang_served_;  // P24D: the names served from lang_files_ (logged once)
  portsim::MemoryGameFiles memory_;                       // '$' names
  portsim::MemoryGameFiles fixtures_;                     // add_fixture
  std::set<std::string> deleted_;                         // normalised names hidden from the install
  uint32_t refused_ = 0;
  portsim::SavesOrder saves_;
};

}  // namespace portapp

#endif
