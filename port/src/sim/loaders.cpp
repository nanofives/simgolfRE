// File listings and the roster loader (see portsim/sim/loaders.h). Phase 3.8, stream 38b. Written from the instructions
// and the C3 hook sources' transcriptions (shim/src/re/c3ep.cpp listMatchingFiles, c3fc.cpp loadGolferRoster); no original
// address or text.
#include "portsim/sim/loaders.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/course.h"

#include "commands_internal.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   listMatchingFiles  1 = the `exclude` filter ignored, 2 = the "Shadow" filter ignored, 3 = no separator appended to the
//                      text buffer, 4 = `require` tested for absence
//   loadGolferRoster   1 = celebrity category (c - 'A') % 10, 2 = pro skill letters offset from 'A' also for lower case,
//                      3 = the fallback list does not set the g_flags bit, 4 = .pro skills stored in the same record
//                      (not the next), 5 = story index taken from the LAST match, 6 = pro look_hi stored without the
//                      arithmetic shift (c / 16 instead of c >> 4), 7 = celebrity look_a accepts '4'
int g_negctl_list_matching_files = 0;
int g_negctl_load_golfer_roster = 0;

uint32_t text_va(Ctx &c) { return state_va(c, &GameState::g_textBuffer); }
void cat(Ctx &c, uint32_t src) { vstrcat(c.mem, text_va(c), src); }

std::string read_str(const Memory &m, uint32_t s) {
  std::string r;
  for (uint32_t p = s;; ++p) {
    const uint8_t b = m.load<uint8_t>(p);
    if (b == 0) break;
    r.push_back(static_cast<char>(b));
  }
  return r;
}

void write_str(const Memory &m, uint32_t dst, const std::string &s) {
  for (size_t i = 0; i < s.size(); ++i) m.store<uint8_t>(dst + static_cast<uint32_t>(i), static_cast<uint8_t>(s[i]));
  m.store<uint8_t>(dst + static_cast<uint32_t>(s.size()), 0);
}
}  // namespace

// ------------------------------------------------------------------------------------------------ listMatchingFiles
int32_t list_matching_files(Ctx &ctx, uint32_t pattern, uint32_t require, uint32_t exclude) {
  const int mode = SG_NEGCTL(g_negctl_list_matching_files);
  const uint32_t table = state_va(ctx, &GameState::g_fileNames);
  std::memset(ctx.gs().g_fileNames, 0, sizeof(ctx.gs().g_fileNames));
  const std::vector<DirEntry> entries = ctx.files().list(read_str(ctx.mem, pattern));
  if (entries.empty()) return 0;  // FindFirstFileA: INVALID_HANDLE_VALUE
  const std::string req = read_str(ctx.mem, require), exc = read_str(ctx.mem, exclude);
  const uint32_t pool = state_va(ctx, &GameState::f_strpool_4c8a28);
  const std::string upper_shadow = read_str(ctx.mem, pool + 0xC), lower_shadow = read_str(ctx.mem, pool + 4);
  int32_t count = 0;
  uint32_t slot = table;
  for (const DirEntry &e : entries) {
    const std::string &name = e.name;
    bool keep = true;
    if (!req.empty() && (name.find(req) == std::string::npos) != (mode == 4)) keep = false;
    if (!exc.empty() && mode != 1 && name.find(exc) != std::string::npos) keep = false;
    if (mode != 2 && name.find(upper_shadow) != std::string::npos) keep = false;
    if (name.find(lower_shadow) == std::string::npos && keep) {
      const uint32_t text = text_va(ctx);
      write_str(ctx.mem, text + vstrlen(ctx.mem, text), name);
      if (mode != 3) cat(ctx, pool);
      write_str(ctx.mem, slot, name);
      slot += 0x64;
      ++count;
    }
  }
  return count;
}

// ------------------------------------------------------------------------------------------------ loadGolferRoster
namespace {
// fopen(path, "rt") + fgets over the bytes of a GameFiles file: text mode turns CR LF into LF and ends at the first
// Ctrl-Z; fgets(buf, n) reads up to n - 1 characters through the first '\n'.
struct TextFile {
  std::vector<uint8_t> data;
  size_t pos = 0;
  explicit TextFile(const std::vector<uint8_t> &raw) {
    for (size_t i = 0; i < raw.size(); ++i) {
      if (raw[i] == 0x1A) break;
      if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') continue;
      data.push_back(raw[i]);
    }
  }
  bool fgets(std::string &line, int n) {
    if (pos >= data.size()) return false;
    line.clear();
    for (int k = 0; k < n - 1 && pos < data.size();) {
      const uint8_t c = data[pos++];
      line.push_back(static_cast<char>(c));
      ++k;
      if (c == '\n') break;
    }
    return true;
  }
};

// The static CRT's strtok over a host copy of the line: the delimiter is a comma. A NULL result (no token left) is what
// the original's next `strcpy(text, token)` faults on.
struct Tokens {
  std::string s;
  size_t pos = 0;
  explicit Tokens(std::string line) : s(std::move(line)) {}
  std::string next() {
    while (pos < s.size() && s[pos] == ',') ++pos;
    if (pos >= s.size()) throw Fault{0, 1};
    const size_t start = pos;
    while (pos < s.size() && s[pos] != ',') ++pos;
    std::string t = s.substr(start, pos - start);
    if (pos < s.size()) ++pos;
    return t;
  }
};

}  // namespace

int32_t load_golfer_roster(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_load_golfer_roster);
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const uint32_t themes = state_va(ctx, &GameState::f_strpool_4c84e8);
  const uint32_t theme = state_va(ctx, &GameState::g_theme);
  const uint32_t stxt = state_va(ctx, &GameState::f_strpool_4e18e4);  // "\*.txt" and the four default golfer file names
  const uint32_t scel = state_va(ctx, &GameState::f_strpool_4e18ac);  // celebrity file, story-file list, "OpeningDay"
  const uint32_t spro = state_va(ctx, &GameState::f_strpool_4e1868);  // pro-golfer file, the standard celebrity file
  const uint32_t spro_std = state_va(ctx, &GameState::f_strpool_4e1848);
  const uint32_t none = state_va(ctx, &GameState::g_str4e9a84);
  auto theme_path = [&](uint32_t suffix) {
    vstrcpy(ctx.mem, text, themes);
    cat(ctx, theme);
    cat(ctx, suffix);
  };
  auto load_theme_file_peer = [&](uint32_t name, int32_t index, int32_t slot) { load_theme_file(ctx, name, index, slot); };

  // the four default golfer files into theme records 0x4c..0x4f
  load_theme_file_peer(stxt + 0x3C, 0x4C, -1);
  load_theme_file_peer(stxt + 0x2C, 0x4D, -1);
  load_theme_file_peer(stxt + 0x18, 0x4E, -1);
  load_theme_file_peer(stxt + 0x8, 0x4F, -1);

  std::memset(gs.g_storyDefs, 0, sizeof(gs.g_storyDefs));
  gs.g_flags &= 0xEFFFFFFFu;

  // the theme's story-file list; an empty one falls back to the standard theme's list and sets the flag bit
  theme_path(stxt);
  if (list_matching_files(ctx, text, none, none) == 0) {
    list_matching_files(ctx, scel + 0x20, none, none);
    if (mode != 3) gs.g_flags |= 1u << 28;
  }
  std::memcpy(gs.g_storyDefs, gs.g_fileNames, sizeof(gs.g_storyDefs));
  // the first of the 100 slots that contains the opening-day name is the story index (unchanged when none does)
  const std::string opening = read_str(ctx.mem, scel + 0x14);
  for (int32_t i = 0; i < 100; ++i) {
    const std::string slot = read_str(ctx.mem, state_va(ctx, &GameState::g_fileNames) + u32(i) * 0x64u);
    if (slot.find(opening) != std::string::npos) {
      gs.g_namedStoryIndex = i;
      if (mode != 5) break;
    }
  }
  for (int32_t e = 0; e < 8; ++e) gs.g_storyFiles[e].f_00 = 0xFFFFFFFFu;
  gs.g_story838220 = 0;

  // ---- celebrities: the theme's file, else the standard theme's
  auto open_text = [&](uint32_t path_in_text, uint32_t fallback) -> std::unique_ptr<TextFile> {
    GameFiles &files = ctx.files();
    const std::string a = read_str(ctx.mem, path_in_text);
    if (files.exists(a)) return std::make_unique<TextFile>(files.read_all(a));
    const std::string b = read_str(ctx.mem, fallback);
    if (files.exists(b)) return std::make_unique<TextFile>(files.read_all(b));
    return nullptr;
  };
  auto not_found = [&]() {
    // both opens failed: " - file not found.\n" is appended to the first path and scrollTextWindow shows it
    cat(ctx, spro + 0x10);
    // through the commands module's peer seam: a test that records scrollTextWindow (commands.recordPeer) logs the call
    // instead of running the modal; otherwise the registered function runs (MissingResource when none is registered)
    cm::call_peer(ctx, "scrollTextWindow", {text, 0x64, 0x32});
  };

  theme_path(scel);
  std::unique_ptr<TextFile> f = open_text(text, spro + 0x24);
  if (!f) {
    not_found();
  } else {
    const Ptr<CelebrityRecord> c0 = state_arr(ctx, &GameState::g_celebrities);
    for (int32_t i = 0; i < 100; ++i) (c0 + i).field(&CelebrityRecord::category).set(static_cast<int8_t>(-1));
    std::string line;
    int32_t k = 0;
    while (k < 100 && f->fgets(line, 0xFA)) {
      if (line[0] == '*' || line.size() < 10) continue;
      line.pop_back();
      Tokens tok(line);
      const std::string name = tok.next();
      write_str(ctx.mem, text, name);
      const Ptr<CelebrityRecord> rec = c0 + k;
      vstrcpy(ctx.mem, rec.field(&CelebrityRecord::name).va(), text);
      auto field = [&]() {
        const std::string t = tok.next();
        write_str(ctx.mem, text, t);
        return static_cast<int32_t>(static_cast<int8_t>(t.empty() ? 0 : t[0]));
      };
      int32_t ch = field();
      rec.field(&CelebrityRecord::category).set(static_cast<int8_t>((ch - 0x41) % (mode == 1 ? 10 : 11)));
      ch = field();
      rec.field(&CelebrityRecord::look_a)
          .set(static_cast<int8_t>(ch >= '1' && ch <= (mode == 7 ? '4' : '3') ? ch - 0x30 : 0));
      ch = field();
      rec.field(&CelebrityRecord::look_b).set(static_cast<int8_t>((ch - 0x30) % 10));
      ch = field();
      rec.field(&CelebrityRecord::look_c).set(static_cast<int8_t>((ch - 0x30) % 10));
      ch = field();
      rec.field(&CelebrityRecord::look_d).set(static_cast<int8_t>((ch - 0x30) % 10));
      ++k;
    }
  }

  // ---- pro golfers
  int32_t count = 0;
  theme_path(spro);
  f = open_text(text, spro_std);
  if (!f) {
    not_found();
  } else {
    const Ptr<GolfPro> p0 = state_arr(ctx, &GameState::g_golfPros);
    for (int32_t i = 0; i < 100; ++i) (p0 + i).field(&GolfPro::look_hi).set(static_cast<int8_t>(-1));
    std::string line;
    while (count < 100 && f->fgets(line, 0xFA)) {
      if (line[0] == '*' || line.size() < 10) continue;
      line.pop_back();
      Tokens tok(line);
      const std::string name = tok.next();
      write_str(ctx.mem, text, name);
      const Ptr<GolfPro> rec = p0 + count;
      vstrcpy(ctx.mem, rec.field(&GolfPro::name).va(), text);
      auto field = [&]() {
        const std::string t = tok.next();
        write_str(ctx.mem, text, t);
        return static_cast<int32_t>(static_cast<int8_t>(t.empty() ? 0 : t[0]));
      };
      int32_t ch = field();
      rec.field(&GolfPro::look_hi).set(static_cast<int8_t>((ch - 0x30) % 8));
      ch = field();
      rec.field(&GolfPro::look_27).set(static_cast<int8_t>((ch - 0x30) % 4));
      ch = field();
      rec.field(&GolfPro::look_lo).set(static_cast<int8_t>((ch - 0x30) % 10));
      ch = field();
      rec.field(&GolfPro::look_24).set(static_cast<int8_t>((ch - 0x30) % 10));
      ch = field();
      rec.field(&GolfPro::look_25).set(static_cast<int8_t>((ch - 0x30) % 10));
      // the seventh field goes into the text buffer and ten skill values are read from the buffer's first ten bytes
      // whatever the field's length (the bytes after its NUL are what the buffer held before)
      write_str(ctx.mem, text, tok.next());
      uint16_t sum = 0;
      for (int32_t i = 0; i < 10; ++i) {
        const int32_t c = static_cast<int8_t>(ctx.mem.load<uint8_t>(text + u32(i)));
        int32_t v;
        if (c >= '0' && c <= '9')
          v = (c - 0x30) % 10;
        else if (c >= 'A' && c <= 'Z')
          v = (c - 0x41) % 10 + 10;
        else
          v = (c - (mode == 2 ? 0x41 : 0x61)) % 10 + 10;
        rec.field(&GolfPro::skill).at(i).set(static_cast<uint8_t>(v));
        sum = static_cast<uint16_t>(sum + static_cast<uint8_t>(v));
      }
      rec.field(&GolfPro::skill_sum).set(static_cast<int16_t>(sum));
      ++count;
    }
  }

  // ---- the theme's .pro golfers, appended after the pro records read
  theme_path(themes + 0x8C);
  const int32_t n = list_matching_files(ctx, text, none, none);
  if (n <= 0) return n;
  const uint32_t names = state_va(ctx, &GameState::g_fileNames);
  const Ptr<GolfPro> p0 = state_arr(ctx, &GameState::g_golfPros);
  const Ptr<GolferType> t76 = state_arr(ctx, &GameState::g_golferTypes) + 0x4C;
  const Ptr<Golfer> g76 = state_arr(ctx, &GameState::g_golfers) + 0x4C;
  Ptr<GolfPro> rec = p0 + count;
  for (int32_t k = 0; k < n; ++k) {
    load_theme_file(ctx, names + u32(k) * 0x64u, 0x4C, 0x4C);
    vstrcpy(ctx.mem, rec.field(&GolfPro::name).va(), t76.field(&GolferType::name).va());
    const int8_t b = static_cast<int8_t>(t76.field(&GolferType::look_23).get());
    rec.field(&GolfPro::look_hi).set(static_cast<int8_t>(mode == 6 ? b / 16 : b >> 4));
    rec.field(&GolfPro::look_27).set(static_cast<int8_t>(t76.field(&GolferType::look_27).get()));
    rec.field(&GolfPro::look_lo).set(static_cast<int8_t>(b & 0xF));
    rec.field(&GolfPro::look_24).set(static_cast<int8_t>(t76.field(&GolferType::look_24).get()));
    rec.field(&GolfPro::look_25).set(static_cast<int8_t>(t76.field(&GolferType::look_25).get()));
    // the original advances its record pointer BEFORE these stores, so the ten skill bytes and their sum land in the NEXT
    // record (the last one writes past the table)
    const Ptr<GolfPro> dst = mode == 4 ? rec : rec + 1;
    uint16_t sum = 0;
    for (int32_t i = 0; i < 10; ++i) {
      const uint8_t v = g76.field(&Golfer::skill).at(i).get();
      dst.field(&GolfPro::skill).at(i).set(v);
      sum = static_cast<uint16_t>(sum + v);
    }
    dst.field(&GolfPro::skill_sum).set(static_cast<int16_t>(sum));
    rec = rec + 1;
  }
  return 0;
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_list_matching_files(Ctx &c, const uint32_t *a, Ret &ret) { ret.set_i32(list_matching_files(c, a[0], a[1], a[2])); }
void a_load_golfer_roster(Ctx &c, const uint32_t *, Ret &ret) { ret.set_i32(load_golfer_roster(c)); }

const FuncDef kFuncs[] = {
    {"listMatchingFiles", "default", "int", 3, 0, a_list_matching_files, &g_negctl_list_matching_files},
    {"loadGolferRoster", "default", "int", 0, 0, a_load_golfer_roster, &g_negctl_load_golfer_roster},
};
}  // namespace

extern const ModuleDef kModule_loaders = {"loaders", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
