// P24D: the app's side of the language packs (lang_ui.h).
#include "lang_ui.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

#include "portsim/sim/lang.h"

namespace portapp {

namespace fs = std::filesystem;
namespace lang = portsim::lang;

namespace {
const char *code_of(int id) { return id == lang::kSpanish ? "es" : "en"; }
int id_of(const std::string &c) { return c == "es" ? lang::kSpanish : c == "en" ? lang::kEnglish : -1; }

std::string read_all(const fs::path &p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return std::string();
  std::ostringstream s;
  s << f.rdbuf();
  return s.str();
}
std::string trim(std::string s) {
  while (!s.empty() && (s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
  size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  return s.substr(i);
}
}  // namespace

LangUi &lang_ui() {
  static LangUi u;
  return u;
}

void LangUi::startup(const std::string &install, const std::string &data_dir) {
  settings = (fs::path(data_dir) / "settings.ini").string();
  const char *off = std::getenv("SG_APP_P24");
  if (off && !std::strcmp(off, "off")) {
    std::printf("LANG en (SG_APP_P24=off)\n");
    return;
  }
  const char *pp = std::getenv("SG_LANG_PACK");
  const fs::path pack = pp && *pp ? fs::path(pp) : fs::path(install) / "Lang" / "simgolf_es.txt";
  std::error_code ec;
  if (!fs::is_regular_file(pack, ec)) {
    std::printf("LANG en (no pack at %s)\n", pack.string().c_str());
    return;
  }
  art_dir = (pack.parent_path() / "es").string();  // the redrawn art beside the pack (scripts/lang/redraw_art.py)
  std::vector<std::string> errors;
  const int n = lang::load_pack(lang::kSpanish, read_all(pack), &errors);
  for (size_t i = 0; i < errors.size() && i < 20; ++i) std::printf("LANG pack %s\n", errors[i].c_str());
  available = n > 0;
  int id = lang::kEnglish;
  for (std::istringstream in(read_all(settings)); in;) {
    std::string line;
    std::getline(in, line);
    line = trim(line);
    if (line.rfind("language=", 0) == 0 && id_of(trim(line.substr(9))) >= 0) id = id_of(trim(line.substr(9)));
  }
  const char *env = std::getenv("SG_LANG");
  if (env && id_of(env) >= 0) id = id_of(env);
  if (available) lang::set_language(id);
  std::printf("LANG %s pack %s entries %d refused %zu\n", code_of(lang::language()), pack.string().c_str(), n, errors.size());
  std::fflush(stdout);
}

void LangUi::choose(int id) {
  lang::set_language(id);
  // settings.ini: every other line kept, `language=` replaced (or added)
  std::vector<std::string> lines;
  bool put = false;
  for (std::istringstream in(read_all(settings)); in;) {
    std::string line;
    if (!std::getline(in, line)) break;
    if (trim(line).rfind("language=", 0) == 0) {
      if (put) continue;
      line = std::string("language=") + code_of(lang::language());
      put = true;
    }
    lines.push_back(line);
  }
  if (!put) lines.push_back(std::string("language=") + code_of(lang::language()));
  std::error_code ec;
  fs::create_directories(fs::path(settings).parent_path(), ec);
  std::ofstream f(settings, std::ios::binary | std::ios::trunc);
  for (const std::string &l : lines) f << l << "\r\n";
}

bool LangUi::mouse(uint32_t msg, int32_t x, int32_t y) {
  if (!available) return false;
  constexpr uint32_t kMove = 0x200, kDown = 0x201, kUp = 0x202, kDbl = 0x203;
  const bool in_box = x >= kX && x < kX + kW && y >= kY && y < kY + kH;
  int row = -1;
  if (open && x >= kX && x < kX + kW && y >= kY + kH && y < kY + kH + 2 * kRowH) row = (y - kY - kH) / kRowH;
  if (msg == kMove) {
    hover = open ? row : -1;
    return in_box || row >= 0;
  }
  if (msg == kDown || msg == kDbl) {
    if (in_box) {
      open = !open;
      hover = -1;
      down_taken = true;
      return true;
    }
    if (row >= 0) {
      choose(row == 0 ? lang::kEnglish : lang::kSpanish);
      open = false;
      hover = -1;
      down_taken = true;
      return true;
    }
    const bool was_open = open;
    open = false;  // a press elsewhere closes the list (and is the game's when the list was closed)
    down_taken = was_open;
    return was_open;
  }
  if (msg == kUp) {
    const bool t = down_taken;
    down_taken = false;
    return t;
  }
  return false;
}

}  // namespace portapp
