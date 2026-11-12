// The app's game files (saves.h): the install read-only, the saves overlay under the app's data directory.
#include "saves.h"

#include "portsim/sim/lang.h"  // P24D: set_lang_overlay

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <system_error>

namespace portapp {

namespace fs = std::filesystem;

namespace {
// `a` is `b` or lies below it (both canonical)
bool inside(const fs::path &a, const fs::path &b) {
  auto ai = a.begin(), bi = b.begin();
  for (; bi != b.end(); ++ai, ++bi) {
    if (ai == a.end()) return false;
    std::wstring x = ai->wstring(), y = bi->wstring();
    if (x.size() != y.size()) return false;
    for (size_t k = 0; k < x.size(); ++k)
      if (towlower(x[k]) != towlower(y[k])) return false;
  }
  return true;
}
fs::path canon(const std::string &p) {
  std::error_code ec;
  fs::path c = fs::weakly_canonical(fs::absolute(fs::path(p), ec), ec);
  return ec ? fs::absolute(fs::path(p)) : c;
}
bool is_memory_name(const std::string &name) { return !name.empty() && name[0] == '$'; }
}  // namespace

std::string AppFiles::default_data_dir() {
  const char *base = std::getenv("LOCALAPPDATA");
  fs::path p = base && *base ? fs::path(base) : fs::current_path();
  return (p / "SimGolfPort").string();
}

bool AppFiles::safe_name(const std::string &name) {
  if (name.empty() || name[0] == '/' || name[0] == '\\') return false;
  if (name.find(':') != std::string::npos) return false;  // a drive (C:) or an alternate data stream (x:stream)
  std::string part;
  for (size_t i = 0; i <= name.size(); ++i) {
    const char c = i < name.size() ? name[i] : '/';
    if (c == '/' || c == '\\') {
      if (part == "..") return false;
      part.clear();
    } else {
      part += c;
    }
  }
  return normalize(name).size() > 0;
}

AppFiles::AppFiles(const std::string &install, const std::string &data, SavesDefect defect) : defect_(defect) {
  std::error_code ec;
  const fs::path ip = canon(install);
  if (!fs::is_directory(ip, ec)) {
    error_ = "install directory not found: " + install;
    return;
  }
  // the test runs before anything is created, so a refused directory never appears inside the install
  if (defect_ != SavesDefect::AllowInsideInstall && (inside(canon(data), ip) || inside(ip, canon(data)))) {
    error_ = "the data directory must not be inside the install (or hold it): " + canon(data).string();
    return;
  }
  fs::create_directories(fs::path(data) / "files", ec);
  const fs::path dp = canon(data);
  if (!fs::is_directory(dp, ec)) {
    error_ = "data directory cannot be created: " + data;
    return;
  }
  if (defect_ != SavesDefect::AllowInsideInstall && (inside(dp, ip) || inside(ip, dp))) {  // a junction resolved now
    error_ = "the data directory must not be inside the install (or hold it): " + dp.string();
    return;
  }
  install_ = ip.string();
  data_ = dp.string();
  install_files_ = std::make_unique<portsim::DirGameFiles>(install_);
  install_files_->set_write_through(false);
  data_files_ = std::make_unique<portsim::DirGameFiles>((dp / "files").string());
  data_files_->set_write_through(true);
  std::ifstream f(dp / "deleted.lst");
  for (std::string line; std::getline(f, line);)
    if (!line.empty()) deleted_.insert(line);
}

void AppFiles::set_lang_overlay(int lang, const std::string &dir) {
  std::error_code ec;
  if (!fs::is_directory(fs::path(dir), ec)) return;
  lang_files_ = std::make_unique<portsim::DirGameFiles>(dir);
  lang_files_->set_write_through(false);
  lang_id_ = lang;
}

bool AppFiles::tombstoned(const std::string &name) const { return deleted_.count(normalize(name)) != 0; }

void AppFiles::save_tombstones() const {
  std::ofstream f(fs::path(data_) / "deleted.lst", std::ios::trunc);
  for (const std::string &s : deleted_) f << s << "\n";
}

int64_t AppFiles::size(const std::string &name) const {
  if (is_memory_name(name)) return memory_.size(name);
  if (!ok()) return -1;
  if (defect_ != SavesDefect::NoReadBack) {
    const int64_t d = data_files_->size(name);
    if (d >= 0) return d;
  }
  if (tombstoned(name)) return -1;
  if (lang_files_ && portsim::lang::language() == lang_id_) {  // P24D
    const int64_t l = lang_files_->size(name);
    if (l >= 0) return l;
  }
  const int64_t f = fixtures_.size(name);
  if (f >= 0) return f;
  return install_files_->size(name);
}

int64_t AppFiles::read(const std::string &name, uint64_t offset, void *buf, uint64_t cap) const {
  if (is_memory_name(name)) return memory_.read(name, offset, buf, cap);
  if (!ok()) return -1;
  if (defect_ != SavesDefect::NoReadBack && data_files_->size(name) >= 0) return data_files_->read(name, offset, buf, cap);
  if (tombstoned(name)) return -1;
  if (lang_files_ && portsim::lang::language() == lang_id_ && lang_files_->size(name) >= 0) {  // P24D
    if (lang_served_.insert(normalize(name)).second) {  // once per name, into the app's log
      std::printf("LANG art %s\n", name.c_str());
      std::fflush(stdout);
    }
    return lang_files_->read(name, offset, buf, cap);
  }
  if (fixtures_.size(name) >= 0) return fixtures_.read(name, offset, buf, cap);
  return install_files_->read(name, offset, buf, cap);
}

std::vector<portsim::DirEntry> AppFiles::list(const std::string &pattern) const {
  if (!ok()) return {};
  std::string dir, component;
  split_pattern(pattern, dir, component);
  // every entry of the directory in both trees ("*" matches all of them), then the pattern on the merged set
  std::string raw = pattern;
  for (char &c : raw)
    if (c == '\\') c = '/';
  const size_t slash = raw.rfind('/');
  const std::string rdir = slash == std::string::npos ? std::string() : raw.substr(0, slash);
  const std::string all = rdir.empty() ? "*" : rdir + "/*";
  std::map<std::string, portsim::DirEntry> merged;  // normalised name -> entry
  const std::string prefix = dir.empty() ? std::string() : dir + "/";
  for (const portsim::DirEntry &e : install_files_->list(all)) {
    if (e.name == "." || e.name == "..") continue;
    if (!e.is_dir && deleted_.count(prefix + normalize(e.name))) continue;
    merged.emplace(normalize(e.name), e);
  }
  for (const portsim::DirEntry &e : fixtures_.list(all)) {
    if (e.name == "." || e.name == ".." || deleted_.count(prefix + normalize(e.name))) continue;
    merged.emplace(normalize(e.name), e);
  }
  if (defect_ != SavesDefect::NoReadBack)
    for (const portsim::DirEntry &e : data_files_->list(all)) {
      if (e.name == "." || e.name == "..") continue;
      merged.emplace(normalize(e.name), e);  // the install's spelling wins when both hold the name
    }
  std::error_code ec;
  const bool exists = fs::is_directory(fs::path(install_) / rdir, ec) || fs::is_directory(fs::path(data_) / "files" / rdir, ec);
  std::vector<portsim::DirEntry> names;
  for (auto &kv : merged) names.push_back(kv.second);
  std::vector<portsim::DirEntry> out = make_listing(component, std::move(names), exists);
  if (saves_.on() && dir == "saved games") {
    // P23A item 2 / P23B fix 3: "." and ".." stay first, then the saves written this session (last write first), then the
    // ranking taken when the option was switched on, then the rest in name order
    size_t first = 0;
    while (first < out.size() && (out[first].name == "." || out[first].name == "..")) ++first;
    saves_.sort(dir, out, first);
  }
  return out;
}

void AppFiles::set_newest_first(bool on) {
  if (!on || !ok()) return;
  // the ranking by write time: the data directory's copy shadows the install's, a name with no file on disk counts as oldest
  const std::vector<portsim::DirEntry> names = list("Saved Games/*");
  std::vector<std::pair<fs::file_time_type, size_t>> key;
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i].name == "." || names[i].name == ".." || names[i].is_dir) continue;
    std::error_code tec;
    fs::file_time_type best = fs::file_time_type::min();
    for (const fs::path &root : {fs::path(data_) / "files", fs::path(install_)}) {
      const fs::file_time_type t = fs::last_write_time(root / "Saved Games" / names[i].name, tec);
      if (!tec) {
        best = t;
        break;
      }
      tec.clear();
    }
    key.emplace_back(best, i);
  }
  std::stable_sort(key.begin(), key.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
  std::vector<std::string> initial;
  for (const auto &k : key) initial.push_back("saved games/" + normalize(names[k.second].name));
  saves_.enable(initial);
}

bool AppFiles::write(const std::string &name, const std::vector<uint8_t> &bytes) {
  if (is_memory_name(name)) {
    memory_.put(name, bytes);
    return true;
  }
  if (!ok() || !safe_name(name)) {
    ++refused_;
    return false;
  }
  if (defect_ == SavesDefect::WriteToInstall) {
    portsim::DirGameFiles wrong(install_);
    wrong.set_write_through(true);
    return wrong.write(name, bytes);
  }
  if (!data_files_->write(name, bytes)) return false;
  saves_.touch(name);
  if (deleted_.erase(normalize(name))) save_tombstones();
  return true;
}

bool AppFiles::remove(const std::string &name) {
  if (is_memory_name(name)) return memory_.remove(name);
  if (!ok() || !safe_name(name)) {
    ++refused_;
    return false;
  }
  bool existed = false;
  saves_.forget(name);
  if (data_files_->size(name) >= 0) existed = data_files_->remove(name);
  if (!tombstoned(name) && (install_files_->size(name) >= 0 || fixtures_.size(name) >= 0)) {
    deleted_.insert(normalize(name));
    save_tombstones();
    existed = true;
  }
  return existed;
}

}  // namespace portapp
