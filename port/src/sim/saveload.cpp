// Saved games, course files and the load-game browser (see portsim/sim/saveload.h). Phase 3.8, stream 38b. Written from
// the instructions and the C3 hook sources' transcriptions (shim/src/re/c3ep.cpp, c3fa.cpp, c3fb.cpp, c3fw.cpp); no
// original address or text. The byte-range tables are generated (log/p38/gen_save_fields.py -> saveload_fields.inc).
#include "portsim/sim/saveload.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "commands_internal.h"
#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/course.h"
#include "portsim/sim/loaders.h"
#include "portsim/sim/text.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   saveGame      1 = the date is not appended (tick.cpp's control numbering: test_tick.py test_save_game_refreshes_the_year),
//                 2 = the ".sve" extension is not appended, 3 = the header is 0x63 bytes
//   loadGame      1 = bit 2 of the entry flags is not kept, 2 = bit 30 not set, 3 = header-only still reads the body,
//                 4 = the browsing save shows the loading message too, 5 = the world assets reload on every finish,
//                 6 = the 8-bit course type compared unsigned (type 0xff against 0 no longer differs... see body),
//                 7 = the championship file reads one header, 8 = the body read leaves g_saveIoRead at 0
//   loadGameBrowserScreen  1 = the scroll-down button scrolls 5, 2 = list rows by 17 pixels, 3 = the load button takes the
//                 row after the selection, 4 = back does not restore the flags, 5 = the browsing save is not reloaded,
//                 6 = loadThemes never runs after a load, 7 = delete removes the next row's file, 8 = load button radius 0x19
//   loadCourse    1 = mode 3 reads the body, 2 = mode 0 does not rebuild the heightfield, 3 = the two slots keep their value
//   serialize     (through saveGame / loadGame) 9 = the third body field is skipped on load
int g_negctl_save_game = 0;
int g_negctl_load_game = 0;
int g_negctl_load_course = 0;
int g_negctl_browser = 0;
int g_negctl_tier_price = 0;          // 1 = a tier outside 0..15 costs 600, 2 = tier 15 costs 9000
int g_negctl_course_action_menu = 0;  // 1 = the purchase takes the next site's record, 2 = unlock bit c + 1, 4 = search radius 0x1d
int g_negctl_save_course = 0;   // 1 = no course body, 2 = a 4-byte marker
int g_negctl_save_game_dialog = 0;   // 1 = a refused name goes on, 2 = an existing file is not asked about
int g_negctl_prompt_and_save_file = 0;   // 1 = the skill record's first byte flipped
int g_negctl_refresh_terrain = 0;   // 1 = the repaint flag byte is not set

struct SaveField {
  uint32_t off;  // offset into GameState
  uint32_t n;    // bytes
};
#include "saveload_fields.inc"

uint32_t text_va(Ctx &c) { return state_va(c, &GameState::g_textBuffer); }

std::string read_str(const Memory &m, uint32_t s) {
  std::string r;
  for (uint32_t p = s;; ++p) {
    const uint8_t b = m.load<uint8_t>(p);
    if (b == 0) break;
    r.push_back(static_cast<char>(b));
  }
  return r;
}

// strcat of a host string onto the NUL-terminated string at dst (the original appends bytes with `rep movs`)
void cat_host(Ctx &c, uint32_t dst, const std::string &s) {
  const uint32_t end = dst + vstrlen(c.mem, dst);
  for (size_t i = 0; i < s.size(); ++i) c.mem.store<uint8_t>(end + static_cast<uint32_t>(i), static_cast<uint8_t>(s[i]));
  c.mem.store<uint8_t>(end + static_cast<uint32_t>(s.size()), 0);
}

// An open file of the CRT's low-level layer (_open / _read / _write / _close), over GameFiles.
struct IoFile {
  bool open = false;
  bool for_write = false;
  bool sink = false;          // a context with no game files: a write is accepted and dropped (the descriptor value is kept)
  std::vector<uint8_t> data;  // read: the file; write: the bytes written so far
  size_t pos = 0;
  std::string name;

  // _read(fd, dst, n): the count read (fewer at the end of the file), -1 on a descriptor that is not open
  int32_t read(Ctx &c, uint32_t dst, uint32_t n) {
    if (!open || for_write) return -1;
    const uint32_t k = static_cast<uint32_t>(std::min<size_t>(n, data.size() - pos));
    if (k != 0) {
      std::memcpy(c.mem.at(dst, k, true), data.data() + pos, k);
      pos += k;
    }
    return static_cast<int32_t>(k);
  }
  int32_t write(const uint8_t *src, uint32_t n) {
    if (!open || !for_write) return -1;
    data.insert(data.end(), src, src + n);
    return static_cast<int32_t>(n);
  }
  // _close: 0, -1 when the descriptor was not open; a file opened for writing reaches GameFiles here
  int32_t close(Ctx &c) {
    if (!open) return -1;
    open = false;
    if (for_write && !sink && !c.files().write(name, data)) return -1;
    return 0;
  }
};

const int32_t kFirstDescriptor = 4;   // the descriptor the CRT hands out for the first file opened (U-0922)

IoFile open_for_read(Ctx &c, const std::string &name) {
  IoFile f;
  f.name = name;
  GameFiles &files = c.files();
  if (files.exists(name)) {
    f.open = true;
    f.data = files.read_all(name);
  }
  c.gs().g_saveFd = f.open ? kFirstDescriptor : -1;
  return f;
}

IoFile open_for_write(Ctx &c, const std::string &name) {
  IoFile f;
  f.name = name;
  f.for_write = true;
  f.sink = c.files_ptr == nullptr;
  f.open = f.sink || c.files().writable();
  c.gs().g_saveFd = f.open ? kFirstDescriptor : -1;
  return f;
}

// saveFieldIo over a table: mode != 0 reads each range from the file into GameState, 0 writes it
int32_t field_io(Ctx &c, IoFile &f, const SaveField &fld, int32_t mode) {
  uint8_t *host = c.bytes() + fld.off;
  if (mode != 0) {
    const int32_t k = f.open && !f.for_write ? static_cast<int32_t>(std::min<size_t>(fld.n, f.data.size() - f.pos)) : -1;
    if (k > 0) {
      std::memcpy(host, f.data.data() + f.pos, static_cast<size_t>(k));
      f.pos += static_cast<size_t>(k);
    }
    return k;
  }
  return f.write(host, fld.n);
}

// serializeCourse(mode): the save body in save (0) or load (non-zero) order; mode goes to g_saveIoRead first. The flag word
// is one of the fields, so in load mode the two tail tests see the value just read.
int32_t serialize_course(Ctx &c, IoFile &f, int32_t mode) {
  GameState &gs = c.gs();
  gs.g_saveIoRead = mode;
  int32_t r = 0;
  const int skip = SG_NEGCTL(g_negctl_load_game) == 9 && mode != 0 ? 2 : -1;
  int idx = 0;
  for (const SaveField &fld : kSerializeFields) {
    if (idx++ == skip) continue;
    r = field_io(c, f, fld, mode);
  }
  r = static_cast<int32_t>(gs.g_flags);
  if ((gs.g_flags & 0x40000000u) != 0) r = field_io(c, f, k_ser_flag30, mode);
  if ((gs.g_flags & 0x200000u) != 0) {
    field_io(c, f, k_ser_flag21_a, mode);
    r = field_io(c, f, k_ser_flag21_b, mode);
  }
  return r;
}

// deserializeCourse(mode): the course file's body (57 fields), the same I/O primitive
int32_t deserialize_course(Ctx &c, IoFile &f, int32_t mode) {
  c.gs().g_saveIoRead = mode;
  int32_t r = 0;
  for (const SaveField &fld : kDeserializeFields) r = field_io(c, f, fld, mode);
  return r;
}

Ret peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) { return cm::call_peer(ctx, name, args); }
// a drawing / backend callee: skipped when no module registers it (a test can still record it by name)
Ret peer_optional(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  try {
    return cm::call_peer(ctx, name, args);
  } catch (const MissingResource &e) {
    if (e.what == nullptr || std::strcmp(e.what, name) != 0) throw;   // a resource missing inside a registered peer
    return Ret{};
  }
}
}  // namespace

// ------------------------------------------------------------------------------------------------ refreshTerrain
// refreshTerrain (re/analysis/terrain/00449520, 24 bytes): Terrain::resetTerrain on the Terrain object (a Terrain.dll call
// with no result the exe reads: the render backend's, U-0800), rebuildTerrainGrid (through the registry: it pushes every
// tile's type, variation and wall bits into Terrain.dll), then the byte g_terrain820f2a[0] = 1 that makes the next
// updateTerrainRender repaint from scratch (terrain_dll.cpp reads it; that repaint writes bit 1 of g_tileWallMask[0]).
// Returns what rebuildTerrainGrid returns. mainLoop's own call sites keep dropping it (tick.cpp kDrops).
int32_t refresh_terrain(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_refresh_terrain);
  const Ret r = peer(ctx, "rebuildTerrainGrid", {});
  if (mode != 1) ctx.gs().g_terrain820f2a[0] = 1;
  return static_cast<int32_t>(r.eax);
}

// ------------------------------------------------------------------------------------------------ saveGame
int32_t save_game(Ctx &ctx, uint32_t name) {
  const int mode = SG_NEGCTL(g_negctl_save_game);
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const std::string given = read_str(ctx.mem, name);   // the original's frame copy of the name
  const uint32_t dirs = state_va(ctx, &GameState::f_strpool_4c3ee0);
  vstrcpy(ctx.mem, text, dirs + 0x6C);
  cat_host(ctx, text, given);
  if (mode != 2) vstrcat(ctx.mem, text, dirs + 0x64);
  IoFile f = open_for_write(ctx, read_str(ctx.mem, text));
  vstrcpy(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c52bc));
  append_course_title(ctx, 0);
  vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c528c) + 0x2C);
  int32_t m = static_cast<int32_t>(static_cast<uint32_t>(gs.g_date) & 0x800003FFu);
  if (m < 0) m = add32(i32((u32(m) - 1u) | 0xFFFFFC00u), 1);
  int32_t v = mul32(m, 30);
  v = sar32(add32(v, sar32(v, 31) & 0x3FF), 10);
  v = add32(v, 1);
  vstrcat(ctx.mem, text, crt_itoa10(ctx, v, state_va(ctx, &GameState::g_textBuf)));
  vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c3f70));
  if (mode != 1) peer(ctx, "appendDate", {u32(gs.g_date)});
  f.write(ctx.mem.at(text, 0x64), mode == 3 ? 0x63u : 0x64u);
  serialize_course(ctx, f, 0);
  return f.close(ctx);
}

// ------------------------------------------------------------------------------------------------ loadGame
namespace {
// the loading message the original shows (text, a window with no golfer selected, a repaint): drawing, called through the
// registry only when something registers it
void show_loading(Ctx &ctx, uint32_t message_in_pool) {
  vstrcpy(ctx.mem, text_va(ctx), message_in_pool);
  ctx.gs().f_w_4c2e00[2] = -1;
  peer_optional(ctx, "drawWindow", {0xC8, 0xC8, 0x80001284u, u32(-2)});
  peer_optional(ctx, "Window::refreshFull", {state_va(ctx, &GameState::g_rootWindow), 0});
}
}  // namespace

int32_t load_game(Ctx &ctx, uint32_t name, int32_t finish, int32_t header_only, int32_t championship) {
  const int mode = SG_NEGCTL(g_negctl_load_game);
  GameState &gs = ctx.gs();
  const int32_t type0 = static_cast<int8_t>(gs.g_courseType);
  const uint32_t flags0 = gs.g_flags;
  const std::string given = read_str(ctx.mem, name);
  const uint32_t text = text_va(ctx);
  const uint32_t dirs = state_va(ctx, &GameState::f_strpool_4c3ee0);
  const uint32_t course_pool = state_va(ctx, &GameState::f_strpool_4c52c0);
  vstrcpy(ctx.mem, text, championship != 0 ? course_pool + 0x68 : dirs + 0x6C);
  cat_host(ctx, text, given);
  IoFile f = open_for_read(ctx, read_str(ctx.mem, text));
  ctx.mem.store<uint8_t>(text, 0);
  f.read(ctx, text, 0x64);
  if (championship != 0 && mode != 7) f.read(ctx, text, 0x64);
  if (header_only != 0 && mode != 3) return f.close(ctx);
  serialize_course(ctx, f, mode == 8 ? 0 : 1);
  f.close(ctx);
  gs.g_flags = (((gs.g_flags & 0xFFFFFFF3u) | (mode == 1 ? 0u : (flags0 & 4u))) | (mode == 2 ? 0u : 0x40000000u));
  gs.g_panelListMode = 0;
  gs.f_w_4c2848[3] = -1;
  gs.f_w_4c2848[0] = -1;
  peer(ctx, "rebuildHeightfield", {});
  peer(ctx, "rebuildTileCoverage", {});
  peer(ctx, "markTileKind11", {});
  if (finish == 0) return 0;
  const std::string browsing = read_str(ctx.mem, course_pool + 0x54);
  if (given != browsing || mode == 4) show_loading(ctx, course_pool + 0x44);
  peer(ctx, "applyCourseType", {u32(static_cast<int32_t>(static_cast<int8_t>(gs.g_courseType)))});
  const int32_t type1 = static_cast<int8_t>(gs.g_courseType);
  if (type1 != type0 || mode == 5) peer_optional(ctx, "loadWorldAssets", {});
  return static_cast<int32_t>(peer(ctx, "refreshTerrain", {}).eax);
}

// ------------------------------------------------------------------------------------------------ loadCourse
int32_t load_course(Ctx &ctx, int32_t mode) {
  const int neg = SG_NEGCTL(g_negctl_load_course);
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const uint32_t course_pool = state_va(ctx, &GameState::f_strpool_4c52c0);
  const std::string path = read_str(ctx.mem, text);
  IoFile f = open_for_read(ctx, path);
  ctx.mem.store<uint8_t>(text, 0);
  f.read(ctx, text, 0x64);
  if (mode == 2) return f.close(ctx);
  if (mode == 0) {
    show_loading(ctx, course_pool + 0x34);
    peer_optional(ctx, "loadInterfaceArt", {u32(mode)});
    peer(ctx, "buildGolferBioText", {});
  }
  f.read(ctx, text, 0x64);
  if (mode == 3 && neg != 1) return f.close(ctx);
  deserialize_course(ctx, f, 1);
  // eight trailing bytes are read over the frame's path slot and the position asked (_tell): no state
  f.pos = std::min(f.pos + 8, f.data.size());
  int32_t r = f.close(ctx);
  if (mode == 0) {
    gs.g_panelListMode = 0;
    if (neg != 3) {
      gs.f_w_4c2848[3] = -1;
      gs.f_w_4c2848[0] = -1;
    }
    if (neg != 2) r = static_cast<int32_t>(peer(ctx, "rebuildHeightfield", {}).eax);
  }
  return r;
}

// ------------------------------------------------------------------------------------------------ saveCourse
// saveCourse (re/analysis/save/0040b680): saves the course whose file name stands in g_textBuffer. The name is copied to a frame
// buffer and handed to setPendingCourseInfo (course.h), the file is opened for writing (g_saveFd) and gets: a 0x64-byte header
// (title, " designed by ", the first golfer type's name), a second 0x64-byte header ("Course Record: " + the best round + " by "
// + the name string 0x14), deserializeCourse(0)'s course body (the 57 fields, written) and the 8-byte marker. The drawing target
// switch is the renderer's. Returns the close result (0, -1 when the file could not be opened).
int32_t save_course(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_save_course);
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const uint32_t course_pool = state_va(ctx, &GameState::f_strpool_4c52c0);
  const std::string path = read_str(ctx.mem, text);   // the frame copy
  set_pending_course_info(ctx, text);
  IoFile f = open_for_write(ctx, path);
  ctx.mem.store<uint8_t>(text, 0);
  append_course_title(ctx, 0);
  vstrcat(ctx.mem, text, course_pool + 0x24);
  vstrcat(ctx.mem, text, state_va(ctx, &GameState::g_golferTypes) + 0x10);
  f.write(ctx.mem.at(text, 0x64), 0x64);
  vstrcpy(ctx.mem, text, course_pool + 0x14);
  vstrcat(ctx.mem, text, crt_itoa10(ctx, gs.g_bestRounds[0], state_va(ctx, &GameState::g_textBuf)));
  vstrcat(ctx.mem, text, course_pool + 0xC);
  append_string(ctx, 0x14);
  f.write(ctx.mem.at(text, 0x64), 0x64);
  if (mode != 1) deserialize_course(ctx, f, 0);
  f.write(ctx.mem.at(course_pool, 8), mode == 2 ? 4u : 8u);
  return f.close(ctx);
}

// ------------------------------------------------------------------------------------------------ saveGameDialog
// saveGameDialog (re/analysis/save/00405b10; c3fa.cpp): the name prompt of the S key. The default name (title, the day of the
// month, the date) is built in g_textBuffer and textInputBox edits it; an empty answer or a name sanitizeFileName refuses shows
// the invalid-name message through startMessage; an existing file asks showPopup (a non-zero answer is returned); then
// saveGame(name) and the saved message. Returns the last callee's eax. The panel and heading drawing is dropped.
int32_t save_game_dialog(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_save_game_dialog);
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const uint32_t dirs = state_va(ctx, &GameState::f_strpool_4c3ee0);
  ctx.mem.store<uint8_t>(text, 0);
  append_course_title(ctx, -1);
  const uint32_t blank = state_va(ctx, &GameState::f_strpool_4c3f70);
  vstrcat(ctx.mem, text, blank);
  const int32_t day = add32(mul32(gs.g_date % 1024, 30) / 1024, 1);
  vstrcat(ctx.mem, text, crt_itoa10(ctx, day, state_va(ctx, &GameState::g_textBuf)));
  vstrcat(ctx.mem, text, blank);
  peer(ctx, "appendDate", {u32(gs.g_date)});
  peer(ctx, "textInputBox", {dirs + 0x7C, 0x40, 0x66, 0x30});
  const bool refused = ctx.mem.load<uint8_t>(text) == 0 || sanitize_file_name(ctx, text) == 0;
  if (refused && mode != 1) {
    vstrcpy(ctx.mem, text, dirs + 0xC);
    return static_cast<int32_t>(peer(ctx, "startMessage", {0x80001284u, 1, u32(-4)}).eax);
  }
  const std::string name = read_str(ctx.mem, text);
  vstrcpy(ctx.mem, text, dirs + 0x6C);
  cat_host(ctx, text, name);
  vstrcat(ctx.mem, text, dirs + 0x64);
  if (file_exists(ctx, text) != 0 && mode != 2) {
    // text = name + the overwrite question
    ctx.mem.store<uint8_t>(text, 0);
    cat_host(ctx, text, name);
    vstrcat(ctx.mem, text, dirs + 0x2C);
    const int32_t r = static_cast<int32_t>(peer(ctx, "showPopup", {0x190, 0x8C, 1, 1, 0}).eax);
    if (r != 0) return r;
  }
  ctx.mem.store<uint8_t>(text, 0);
  cat_host(ctx, text, name);
  save_game(ctx, text);
  vstrcpy(ctx.mem, text, dirs + 0x20);
  return static_cast<int32_t>(peer(ctx, "startMessage", {0x80001284u, 1, u32(-4)}).eax);
}

// ------------------------------------------------------------------------------------------------ promptAndSaveFile
// promptAndSaveFile (re/analysis/save/00437910; c3ep.cpp): writes golfer type `index` as a character file. The path is built in
// g_textBuffer: the themes directory, then for mode -2 the default name, otherwise the theme, the separator and the type's
// name, with the .chr suffix for mode -1 and .pro for any other mode; a non-zero `championship` replaces it by the championship
// directory + name + .pro. An existing file (mode != -2) asks showPopup (a non-zero answer returns 0); a name sanitizeFileName
// refuses, or a file that cannot be created, shows its message through scrollTextWindow (a ui function: MissingResource while no
// module registers it) and returns 0. The file gets the 0x230-byte type record, its 0x4e2-byte phrase record, a 16-byte skill
// record (the golfer `mode`'s skills, or g_playerSkills for index 0) and the 8-byte marker; the portrait that follows in the
// original (an image the renderer composes and Image::savePcx writes) is not produced (U-0979). Returns 1 when written.
int32_t prompt_and_save_file(Ctx &ctx, int32_t index, int32_t mode_arg, int32_t championship) {
  const int mode = SG_NEGCTL(g_negctl_prompt_and_save_file);
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const uint32_t themes = state_va(ctx, &GameState::f_strpool_4c84e8);
  const uint32_t names = state_va(ctx, &GameState::f_strpool_4c80d0);
  const uint32_t course_pool = state_va(ctx, &GameState::f_strpool_4c52c0);
  const uint32_t pro_suffix = state_va(ctx, &GameState::f_strpool_4c581c) + 0x12C;
  const uint32_t type_va = state_va(ctx, &GameState::g_golferTypes) + u32(mul32(index, 0x230));
  const uint32_t name_va = type_va + 0x10;
  if (gs.g_ui_4c7b34 != 0) gs.g_ui_4c7b34 = 0;   // (the original first releases a UI object: the renderer's)
  vstrcpy(ctx.mem, text, themes);
  uint32_t suffix;
  if (mode_arg == -2) {
    vstrcat(ctx.mem, text, names + 0x404);
    suffix = pro_suffix;
  } else {
    vstrcat(ctx.mem, text, state_va(ctx, &GameState::g_theme));
    vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c84e4));
    vstrcat(ctx.mem, text, name_va);
    suffix = mode_arg == -1 ? names + 0x40C : pro_suffix;
  }
  vstrcat(ctx.mem, text, suffix);
  if (championship != 0) {
    vstrcpy(ctx.mem, text, course_pool + 0x68);
    vstrcat(ctx.mem, text, name_va);
    vstrcat(ctx.mem, text, pro_suffix);
  }
  auto scroll = [&](uint32_t message) {
    vstrcat(ctx.mem, text, message);
    peer(ctx, "scrollTextWindow", {text, 0x1E, 0x1E});
  };
  if (file_exists(ctx, text) != 0 && mode_arg != -2) {
    const uint32_t len = vstrlen(ctx.mem, text);
    vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c3ee0) + 0x2C);
    if (peer(ctx, "showPopup", {0xC8, 0x1E, 1, 1, 0}).eax != 0) return 0;
    ctx.mem.store<uint8_t>(text + len, 0);
  }
  if (sanitize_file_name(ctx, name_va) == 0) {
    scroll(names + 0x3EC);
    return 0;
  }
  const std::string path = read_str(ctx.mem, text);
  const bool can_write = ctx.files_ptr == nullptr || ctx.files().writable();
  if (!can_write) {
    scroll(names + 0x3D4);
    return 0;
  }
  std::vector<uint8_t> out;
  auto put = [&](uint32_t va, uint32_t n) {
    const uint8_t *p = ctx.mem.at(va, n);
    out.insert(out.end(), p, p + n);
  };
  put(type_va, 0x230);
  put(state_va(ctx, &GameState::g_golferPhrases) + u32(mul32(index, 0x4E2)), 0x4E2);
  uint8_t rec[16] = {0};
  if (mode_arg != -1) {
    const uint32_t src = state_va(ctx, &GameState::g_golfers) + 0xF0 + (u32(mode_arg) << 8);
    std::memcpy(rec, ctx.mem.at(src, 16), 16);
  }
  if (index == 0) std::memcpy(rec, gs.g_playerSkills, 16);
  if (mode == 1) rec[0] ^= 1;
  out.insert(out.end(), rec, rec + 16);
  put(course_pool, 8);
  if (ctx.files_ptr != nullptr) ctx.files().write(path, out);
  return 1;
}

// ------------------------------------------------------------------------------------------------ the browser
namespace {
// g_rootInput slots the browser reads (commands.cpp RootSlot): the click position and the button word
constexpr int kRootClickX = 2, kRootClickY = 3, kRootButton = 4;

// the file-name table of listMatchingFiles: 100-byte slots; slot -1 is the 100 bytes before the table
uint32_t name_at(Ctx &ctx, int32_t i) { return state_va(ctx, &GameState::g_fileNames) + u32(mul32(i, 100)); }

// itoa into the browser's number buffer, appended after the text buffer's string (`text[0] = 0` first when reset)
void put_number(Ctx &ctx, int32_t v, bool reset = true) {
  const uint32_t text = text_va(ctx);
  if (reset) ctx.mem.store<uint8_t>(text, 0);
  vstrcat(ctx.mem, text, crt_itoa10(ctx, v, state_va(ctx, &GameState::g_panelNumberText)));
}

// the text the selection details page builds (the drawing around it is dropped): the page numbers go through the
// text buffer and the number buffer, and the money and skill values through appendNumber / appendCents
void details_text(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const uint32_t pool88 = state_va(ctx, &GameState::f_strpool_4c8808);
  put_number(ctx, sub32(gs.g_holesPlusOne, 1));
  put_number(ctx, gs.g_coursePar);
  put_number(ctx, gs.g_courseYards);
  ctx.mem.store<uint8_t>(text, 0);
  peer(ctx, "appendNumber", {u32(mul32(gs.g_cash_div100, 100))});
  put_number(ctx, gs.g_funRating);
  ctx.mem.store<uint8_t>(text, 0);
  peer(ctx, "appendCents", {u32(gs.g_lengthSkillX100)});
  ctx.mem.store<uint8_t>(text, 0);
  peer(ctx, "appendCents", {u32(gs.g_accuracySkillX100)});
  ctx.mem.store<uint8_t>(text, 0);
  peer(ctx, "appendCents", {u32(gs.g_imaginationX100)});
  // one row per hole 1 .. holes (the bound re-read every pass): hole number, par (the signed byte of the hole record) and
  // yards (the signed word at +4)
  const uint32_t holes = state_va(ctx, &GameState::g_holes);
  uint32_t rec = holes + 0x208;
  for (int32_t h = 1; h < gs.g_holesPlusOne; ++h) {
    put_number(ctx, h);
    put_number(ctx, ctx.mem.load<int8_t>(rec));
    put_number(ctx, ctx.mem.load<int16_t>(rec + 4));
    rec += 0x208;
  }
  ctx.mem.store<uint8_t>(text, 0);
  append_course_title(ctx, 0);
  vstrcat(ctx.mem, text, pool88 + 0x94);
  vstrcat(ctx.mem, text, state_va(ctx, &GameState::g_theme));
  vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c59e0));
  vstrcpy(ctx.mem, text, pool88 + 0x84);
  vstrcat(ctx.mem, text, state_va(ctx, &GameState::g_golferTypes) + 0x10);
  const int32_t record = gs.g_bestRounds[0];
  if (record != 0) {
    vstrcpy(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c52c0) + 0x14);
    put_number(ctx, record, false);
    vstrcat(ctx.mem, text, pool88 + 0x7C);
    peer(ctx, "appendString", {0x14});
  }
}
}  // namespace

int32_t load_game_browser_screen(Ctx &ctx, int32_t resume, int32_t championship) {
  const int mode = SG_NEGCTL(g_negctl_browser);
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const uint32_t browse = state_va(ctx, &GameState::f_strpool_4c8910);    // pattern strings, the browsing save name
  const uint32_t course_pool = state_va(ctx, &GameState::f_strpool_4c52c0);
  const uint32_t none = state_va(ctx, &GameState::g_str4e9a84);
  const uint32_t tips = state_va(ctx, &GameState::f_strpool_4c8794);
  const uint32_t browsing_name = browse + 0x88, champ_dir = course_pool + 0x68;
  const bool plain = resume == 0 && championship == 0;
  if (plain) save_game(ctx, browsing_name);
  const int32_t saved_type = static_cast<int8_t>(gs.g_courseType);
  const uint32_t saved_flags = gs.g_flags;
  const std::string theme_copy = read_str(ctx.mem, state_va(ctx, &GameState::g_theme));
  int32_t sel = -1, scroll = 0, n = 0, max_scroll = 0, pick = -1;
  bool load = false;
  gs.g_rootInput[kRootButton] = 0;
  for (;;) {
    // ---- redraw: the list of files (the drawing is dropped)
    if (championship != 0) {
      vstrcpy(ctx.mem, text, browse + 0x34);
      n = list_matching_files(ctx, text, none, none);
    } else {
      vstrcpy(ctx.mem, text, browse + 0xC);
      n = list_matching_files(ctx, text, none, browsing_name);
    }
    max_scroll = sub32(n, 0x10);
    if (scroll > max_scroll) scroll = max_scroll;
    if (scroll < 0) scroll = 0;
    for (int32_t i = scroll; i < add32(scroll, 0x10) && i < n; ++i) {
      const uint32_t name = name_at(ctx, i);
      vstrcpy(ctx.mem, text, name);
      if (ctx.mem.load<uint8_t>(name) == 0x26) {   // an autosave: its title is read from the file
        vstrcpy(ctx.mem, text, browse);
        std::string line = read_str(ctx.mem, text);
        load_game(ctx, name, 0, 1, 0);
        line += read_str(ctx.mem, text);
        for (size_t k = 0; k <= line.size(); ++k)
          ctx.mem.store<uint8_t>(text + static_cast<uint32_t>(k), k < line.size() ? static_cast<uint8_t>(line[k]) : 0);
        vstrcat(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c59e0));
      }
    }
    if (sel != -1) details_text(ctx);

    // ---- the input loop
    int32_t hot = -1, dwell = 0, action = 0;
    for (;;) {
      cm::pump_messages(ctx, PumpKind::Blocking, "loadGameBrowserScreen");
      InputSource *src = input_source(ctx);
      int32_t x = 0, y = 0;
      if (!src || !src->cursor(x, y)) throw NeedInput{{"loadGameBrowserScreen"}};
      const int32_t prev = hot;
      hot = -1;
      if (approx_distance(sub32(x, 0x279), sub32(y, 0x22B)) < (mode == 8 ? 0x19 : 0x1A)) hot = 0;
      if (approx_distance(sub32(x, 0x301), sub32(y, 0x22C)) < 0x18) hot = 1;
      if (approx_distance(sub32(x, 0x310), sub32(y, 0x78)) < 0x14) hot = 2;
      if (approx_distance(sub32(x, 0x310), sub32(y, 0x190)) < 0x14) hot = 3;
      if (approx_distance(sub32(x, 0x2C0), sub32(y, 0x22B)) < 0x17) hot = 4;
      if (hot != prev) {
        dwell = 0;
      } else {
        dwell = add32(dwell, 1);
        if (dwell > 0x1E && hot != -1) {   // the button tip: its text goes through the text buffer
          ctx.mem.store<uint8_t>(text, 0);
          if (hot == 0) vstrcpy(ctx.mem, text, sel == -1 ? none : tips + 0x50);
          else if (hot == 1) vstrcpy(ctx.mem, text, state_va(ctx, &GameState::f_strpool_4c84e8) + 0xA8);
          else if (hot == 4) vstrcpy(ctx.mem, text, tips + 0x58);
        }
      }
      if (gs.g_rootInput[kRootButton] != 0) {
        action = hot == -1 ? -2 : hot;
        break;
      }
      if (cm::flush_redraw(ctx) == 0) continue;
      if (gs.g_rootInput[kRootButton] != 0) {
        action = hot == -1 ? -2 : hot;
        break;
      }
      if (cm::take_key(ctx) == 0x1B) {
        hot = 1;
        action = 1;
        break;
      }
      action = -2;
      break;
    }

    if (action != -2) {
      gs.g_rootInput[kRootButton] = 0;
      if (static_cast<uint32_t>(action) > 4) continue;
      if (action == 0) {
        pick = mode == 3 ? add32(sel, 1) : sel;
        load = true;
        break;
      }
      if (action == 1) break;
      if (action == 2) {
        scroll = clamp(sub32(scroll, 4), 0, max_scroll);
        continue;
      }
      if (action == 3) {
        scroll = clamp(add32(scroll, mode == 1 ? 5 : 4), 0, max_scroll);
        continue;
      }
      // 4: delete the selection after a confirmation
      vstrcpy(ctx.mem, text, tips + 0x2C);
      if (championship != 0) vstrcat(ctx.mem, text, champ_dir);
      const uint32_t victim = name_at(ctx, sel);
      vstrcat(ctx.mem, text, victim);
      vstrcat(ctx.mem, text, tips);
      if (peer(ctx, "showPopup", {0x190, 0x64, 1, 1, 0}).eax != 0) continue;
      vstrcpy(ctx.mem, text, championship != 0 ? champ_dir : state_va(ctx, &GameState::f_strpool_4c8808) + 0x6C);
      vstrcat(ctx.mem, text, victim);
      if (mode == 7) vstrcpy(ctx.mem, text, name_at(ctx, add32(sel, 1)));
      ctx.files().remove(read_str(ctx.mem, text));
      continue;
    }

    // a press (or a key) at the pointer position over the list
    if (gs.g_rootInput[kRootClickX] <= 0x140) continue;
    const int32_t my = gs.g_rootInput[kRootClickY];
    if (my < 0x74) continue;
    sel = add32(sub32(my, 0x74) / (mode == 2 ? 17 : 16), scroll);
    if (sel >= add32(scroll, 0x10)) {
      scroll = clamp(add32(scroll, 4), 0, max_scroll);
      continue;
    }
    if (sel < n) {
      if (championship != 0) {
        vstrcpy(ctx.mem, text, champ_dir);
        vstrcat(ctx.mem, text, name_at(ctx, sel));
        load_course(ctx, 1);
      } else {
        load_game(ctx, name_at(ctx, sel), 0, 0, 0);
      }
      continue;
    }
    sel = -1;
  }

  if (load) {
    gs.g_courseType = static_cast<int8_t>(saved_type);
    gs.g_flags = saved_flags;
    if (pick >= 0 && pick < n) {
      if (championship != 0) {
        vstrcpy(ctx.mem, text, champ_dir);
        vstrcat(ctx.mem, text, name_at(ctx, pick));
        load_course(ctx, 0);
      } else {
        load_game(ctx, name_at(ctx, pick), resume == 0 ? 1 : 0, 0, 0);
      }
      if (read_str(ctx.mem, state_va(ctx, &GameState::g_theme)) != theme_copy && mode != 6) load_themes(ctx, 1);
      peer_optional(ctx, "enterModal483d30", {});
      return 1;
    }
    if (plain && mode != 5) load_game(ctx, course_pool + 0x54, 1, 0, 0);
  } else {
    if (mode != 4) gs.g_flags = saved_flags;
    gs.g_courseType = static_cast<int8_t>(saved_type);
    if (plain && mode != 5) load_game(ctx, course_pool + 0x54, 1, 0, 0);
  }
  peer_optional(ctx, "enterModal483d30", {});
  return 0;
}

// ------------------------------------------------------------------------------------------------ tierPrice
// tierPrice (re/analysis/economy/0046f1d0): the price of a course tier 0..15 from a jump table, 500 for any other value (the
// range test is unsigned, so negative tiers too); the original's v * 100 / 100 leaves these values unchanged.
int32_t tier_price(int32_t tier) {
  static const int32_t kPrice[16] = {500, 600, 700, 800, 1200, 1500, 2000, 2500, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000};
  const int mode = SG_NEGCTL(g_negctl_tier_price);
  if (static_cast<uint32_t>(tier) > 15u) return mode == 1 ? 600 : 500;
  return kPrice[mode == 2 && tier == 15 ? 14 : tier];
}

// ------------------------------------------------------------------------------------------------ courseActionMenu
namespace {
// the 16 course records of g_courseTable (0x2e bytes each): the site byte is at +0x1c, then bytes +1, +4, +6, +7 of that
constexpr uint32_t kRecStride = 0x2E, kRecFromSite = 0x1C;

uint32_t rec_byte_va(Ctx &ctx, int32_t c, uint32_t off) {
  return state_va(ctx, &GameState::g_courseTable) + kRecFromSite + mul32(c, kRecStride) + off;
}
uint8_t rec_byte(Ctx &ctx, int32_t c, uint32_t off) { return ctx.mem.load<uint8_t>(rec_byte_va(ctx, c, off)); }

// site k's definition (0x82 bytes): name +0, x word +0x3a, y word +0x3c, bytes +0x3e / +0x3f / +0x40, text +0x41
uint32_t site_def(Ctx &ctx, int32_t k) { return state_va(ctx, &GameState::f_tab_locations_4c1e90) + u32(mul32(k, 0x82)); }
// site k's map spot: two words
int32_t spot_x(Ctx &ctx, int32_t k) { return ctx.mem.load<int16_t>(state_va(ctx, &GameState::f_tab_point16_4e3dc0) + 4u * u32(k)); }
int32_t spot_y(Ctx &ctx, int32_t k) { return ctx.mem.load<int16_t>(state_va(ctx, &GameState::f_tab_point16_4e3dc0) + 4u * u32(k) + 2); }
}  // namespace

// The world map where the player picks, buys, saves or loads a course (re/analysis/ui/0046f550; c3fw.cpp's transcription).
// Simulation effects only: the site table of the frame, the hover and button tests (two cursor reads, then the pump, per
// frame), the nearest site, the purchase (cash, the unlock mask, the sound's RNG draws), seedCourseTable, the save dialog
// (textInputBox, saveGame, startMessage) and the load browser; the drawing is dropped, the strings the drawing would show go
// through g_textBuffer and the number buffer as in the original. Returns -1 after back or a load, or the record bought.
// The frame slots the original never initialises (blink counter, dwell counter, previous hot button, the table entry of a site
// no record names) start at 0 (U-0978).
int32_t course_action_menu(Ctx &ctx) {
  const int mode = SG_NEGCTL(g_negctl_course_action_menu);
  GameState &gs = ctx.gs();
  const uint32_t text = text_va(ctx);
  const uint32_t number_buf = state_va(ctx, &GameState::g_scoreSummaryBuf);
  const uint32_t p4c61dc = state_va(ctx, &GameState::f_strpool_4c61dc), p4e4004 = state_va(ctx, &GameState::f_strpool_4e4004);
  const uint32_t p4e3fbc = state_va(ctx, &GameState::f_strpool_4e3fbc), p4e3fa4 = state_va(ctx, &GameState::f_strpool_4e3fa4);
  const uint32_t p4e3f80 = state_va(ctx, &GameState::f_strpool_4e3f80), p4e3f54 = state_va(ctx, &GameState::f_strpool_4e3f54);
  const uint32_t p4c95f4 = state_va(ctx, &GameState::f_strpool_4c95f4), p4e3f3c = state_va(ctx, &GameState::f_strpool_4e3f3c);
  const uint32_t p4c4944 = state_va(ctx, &GameState::f_strpool_4c4944), p4e3f30 = state_va(ctx, &GameState::f_strpool_4e3f30);
  const uint32_t p4e3e7c = state_va(ctx, &GameState::f_strpool_4e3e7c), p4c84e8 = state_va(ctx, &GameState::f_strpool_4c84e8);
  const uint32_t p4c3f70 = state_va(ctx, &GameState::f_strpool_4c3f70), p4c3ee0 = state_va(ctx, &GameState::f_strpool_4c3ee0);
  const uint32_t p4c4e60 = state_va(ctx, &GameState::f_strpool_4c4e60);
  const uint32_t cash_label = p4c61dc + 4;
  const uint32_t table = state_va(ctx, &GameState::g_courseTable);
  int32_t nearest = -1, blink = 0, dwell = 0, prev_hot = 0, result = 0;
  int32_t site[16] = {0};
  auto put_item = [&](int32_t v) {   // _itoa(v, number buffer, 10) appended to the text buffer
    vstrcat(ctx.mem, text, crt_itoa10(ctx, v, number_buf));
  };
  for (;;) {
    // ---- one frame: the site table (the byte is not bound-checked in the original: a site outside 0..15 would write the frame)
    for (int32_t c = 0; c < 16; ++c) {
      const int32_t s = static_cast<int8_t>(rec_byte(ctx, c, 0));
      if (s >= 0 && s < 16) site[s] = c;
    }
    vstrcpy(ctx.mem, text, cash_label);
    peer(ctx, "appendNumber", {u32(mul32(gs.g_cash_div100, 100))});
    if ((gs.g_flags & 0x1000000u) != 0) {
      vstrcpy(ctx.mem, text, p4e4004);
      vstrcat(ctx.mem, text, cash_label);
    }
    for (int32_t k = 15; k >= 0; --k) {
      const uint32_t d = site_def(ctx, k);
      const int32_t kind = ctx.mem.load<int8_t>(d + 0x3E);
      const int32_t c = site[k];
      vstrcpy(ctx.mem, text, d);   // the name
      const bool open = rec_byte(ctx, c, 6) == 0xFF;
      vstrcpy(ctx.mem, text, d + 0x41);   // the text
      if (k == nearest && open) {
        vstrcpy(ctx.mem, text, p4e3fbc + 0xC);
        put_item(rec_byte(ctx, c, 1));
        vstrcat(ctx.mem, text, p4e3fbc);
        const int32_t b1 = ctx.mem.load<int8_t>(d + 0x40);
        if (b1 == 0) vstrcat(ctx.mem, text, p4e3fa4);
        else if (b1 == 1) vstrcat(ctx.mem, text, p4e3fa4 + 8);
        else if (b1 == 2) vstrcat(ctx.mem, text, p4e3fa4 + 0x10);
        ctx.mem.store<uint8_t>(text, 0);
        const int32_t b2 = ctx.mem.load<int8_t>(d + 0x3F);
        if (b2 == 0) vstrcat(ctx.mem, text, p4e3f80);
        else if (b2 == 1) vstrcat(ctx.mem, text, p4e3f80 + 0xC);
        else if (b2 == 2) vstrcat(ctx.mem, text, p4e3f80 + 0x18);
        if (kind == 0) vstrcat(ctx.mem, text, p4e3f54 + 0x14);
        else if (kind == 1) vstrcat(ctx.mem, text, p4c95f4 + 0x164);
        else if (kind == 2) vstrcat(ctx.mem, text, p4e3f54 + 0x20);
        else if (kind == 3) vstrcat(ctx.mem, text, p4e3f54 + 0xC);
        vstrcat(ctx.mem, text, p4e3f54);
        ctx.mem.store<uint8_t>(text, 0);
        vstrcat(ctx.mem, text, p4e3f3c + 0xC);
        peer(ctx, "appendNumber", {u32(mul32(tier_price(c), 100))});
        vstrcat(ctx.mem, text, p4c4944);
        ctx.mem.store<uint8_t>(text, 0);
      } else {
        ctx.mem.store<uint8_t>(text, 0);
        put_item(rec_byte(ctx, c, 1));
        vstrcat(ctx.mem, text, p4e3f3c);
        peer(ctx, "appendNumber", {u32(mul32(tier_price(c), 100))});
        const uint8_t y6 = rec_byte(ctx, c, 6);
        if (y6 != 0xFF) {
          ctx.mem.store<uint8_t>(text, 0);
          put_item(add32(static_cast<int8_t>(y6), 0x7D1));
          vstrcat(ctx.mem, text, p4e3f30 + 8);
          const uint8_t y7 = rec_byte(ctx, c, 7);
          if (y7 == 0xFF) vstrcat(ctx.mem, text, p4e3f30);
          else vstrcat(ctx.mem, text, crt_itoa10(ctx, add32(static_cast<int8_t>(y7), 0x7D1), number_buf));
        }
      }
    }

    // ---- the pointer: the hot button
    InputSource *src = input_source(ctx);
    int32_t a = 0, b = 0;
    if (!src || !src->cursor(a, b)) throw NeedInput{{"courseActionMenu"}};
    int32_t hot = -1;
    if (approx_distance(sub32(a, 0x300), sub32(b, 0x22D)) < 0x19) hot = 3;
    if (approx_distance(sub32(a, 0x2F8), sub32(b, 0x1DC)) < 0x19) hot = gs.g_date != 0 ? 1 : 0;
    bool tested = false;
    if (gs.g_date != 0 && approx_distance(sub32(a, 0x306), sub32(b, 0x1A9)) < 0x14) {
      hot = 2;
      tested = true;
    }
    if (tested || hot != -1) {
      if (prev_hot != hot) {
        dwell = 0;
      } else {
        dwell = add32(dwell, 1);
        if (dwell > 0x14 && static_cast<uint32_t>(hot) <= 3u) {   // the tip: its text through the text buffer
          static const uint32_t kTipOff[3] = {0x94, 0x88, 0x7C};
          vstrcpy(ctx.mem, text, hot < 3 ? p4e3e7c + kTipOff[hot] : p4c84e8 + 0xA8);
        }
      }
      prev_hot = hot;
    }
    blink = add32(blink, 1);   // only the plate blink reads it (drawing)
    (void)blink;
    if (!src->cursor(a, b)) throw NeedInput{{"courseActionMenu"}};
    // ---- the nearest site within 0x1e of the pointer, against its map spot (+0x19) and its definition x / y
    int32_t best = mode == 4 ? 0x1D : 0x1E;
    nearest = -1;
    for (int32_t k = 0; k < 16; ++k) {
      const int32_t d1 = approx_distance(sub32(sub32(a, spot_x(ctx, k)), 0x19), sub32(sub32(b, spot_y(ctx, k)), 0x19));
      if (d1 < best) {
        best = d1;
        nearest = k;
      }
      const uint32_t d = site_def(ctx, k);
      const int32_t d2 = approx_distance(sub32(a, ctx.mem.load<int16_t>(d + 0x3A)), sub32(b, ctx.mem.load<int16_t>(d + 0x3C)));
      if (d2 < best) {
        best = d2;
        nearest = k;
      }
    }
    cm::pump_messages(ctx, PumpKind::Blocking, "courseActionMenu");
    const int32_t btn = gs.g_rootInput[kRootButton];
    if (btn == 0) continue;
    if (hot == 0) {
      peer(ctx, "seedCourseTable", {});
      continue;
    }
    if (hot == 1) {   // save: ask for a name (prefilled with the title and the date)
      ctx.mem.store<uint8_t>(text, 0);
      append_course_title(ctx, -1);
      vstrcat(ctx.mem, text, p4c3f70);
      const int32_t day = add32(mul32(gs.g_date % 1024, 30) / 1024, 1);
      vstrcat(ctx.mem, text, crt_itoa10(ctx, day, number_buf));
      vstrcat(ctx.mem, text, p4c3f70);
      peer(ctx, "appendDate", {u32(gs.g_date)});
      peer(ctx, "textInputBox", {p4c3ee0 + 0x7C, 0x80, 0x66, 0x28});
      if (ctx.mem.load<uint8_t>(text) == 0) continue;
      save_game(ctx, text);
      vstrcpy(ctx.mem, text, p4c3ee0 + 0x20);
      peer(ctx, "startMessage", {0x80001284u, 1, u32(-4)});
      continue;
    }
    if (hot == 2) {   // load
      if (load_game_browser_screen(ctx, gs.g_date == 0 ? 1 : 0, 0) != 0) {
        result = -1;
        break;
      }
      continue;
    }
    if (hot == 3) {
      result = -1;
      break;
    }
    // a left press on the nearest site whose record is still for sale buys it
    if (btn != 1) continue;
    if (nearest == -1) continue;
    const int32_t c = site[mode == 1 ? ((nearest + 1) & 15) : nearest];
    if (rec_byte(ctx, c, 6) != 0xFF) continue;
    vstrcpy(ctx.mem, text, p4e3e7c + 0x60);
    vstrcat(ctx.mem, text, p4e3e7c + 0x40);
    const int32_t cheat = gs.g_buildingUnlockLevel == 0x11 ? 1 : 0;
    if (gs.g_cash_div100 < tier_price(c) && cheat == 0) {
      peer(ctx, "drawWindow", {0xC8, 0xC8, 0x80006000u, u32(-2)});
      cm::pump_and_redraw(ctx, cheat, "courseActionMenu");
      continue;
    }
    vstrcpy(ctx.mem, text, p4e3e7c + 0x30);
    vstrcat(ctx.mem, text, table + u32(mul32(c, 0x2E)));
    vstrcat(ctx.mem, text, p4c4e60);
    vstrcpy(ctx.mem, text, p4e3e7c + 0x14);
    gs.g_cash_div100 = sub32(gs.g_cash_div100, tier_price(c));
    gs.g_amenityUnlockMask |= 1u << ((u32(c) + (mode == 2 ? 1u : 0u)) & 31u);
    const int32_t sound = static_cast<int8_t>(rec_byte(ctx, c, 4));
    if (sound == 0) {
      peer_optional(ctx, "playSound", {0x33, 0x64, 0, 0, 0});
    } else if (sound >= 1 && sound <= 3) {
      static const int32_t kBase[3] = {0x78, 0x6E, 0x73};
      const int32_t id = add32(cm::rng_range(ctx, 3), kBase[sound - 1]);
      peer_optional(ctx, "playSound", {u32(id), 0x64, 0, 0, 0});
    }
    result = c;
    break;
  }
  peer_optional(ctx, "enterModal483d30", {});
  gs.g_rootInput[kRootButton] = 0;
  return result;
}

// ------------------------------------------------------------------------------------------------ test entries
namespace {
// saveload.setScript(va, n): a scripted InputSource for the browser tests: dwords [ncmd, ncur, ncmd * (handler, a0, a1,
// async_ctrl, async_shift, t), ncur * (x, y)]; the batches follow QueueInput's rule, the i-th cursor read returns the i-th
// point (the last one repeats)
class ScriptInput : public QueueInput {
 public:
  ScriptInput(std::vector<Command> cmds, std::vector<std::pair<int32_t, int32_t>> cur)
      : QueueInput(std::move(cmds)), cur_(std::move(cur)) {}
  bool cursor(int32_t &x, int32_t &y) override {
    if (cur_.empty()) return false;
    const auto &p = cur_[std::min(pos_, cur_.size() - 1)];
    ++pos_;
    x = p.first;
    y = p.second;
    return true;
  }

 private:
  std::vector<std::pair<int32_t, int32_t>> cur_;
  size_t pos_ = 0;
};
std::map<const Ctx *, std::unique_ptr<ScriptInput>> &scripts() {
  static std::map<const Ctx *, std::unique_ptr<ScriptInput>> m;
  return m;
}
void a_set_script(Ctx &c, const uint32_t *a, Ret &) {
  const uint32_t ncmd = c.mem.load<uint32_t>(a[0]), ncur = c.mem.load<uint32_t>(a[0] + 4);
  std::vector<Command> cmds;
  for (uint32_t k = 0; k < ncmd; ++k) {
    const uint32_t w = a[0] + 8 + 24 * k;
    Command cm;
    cm.kind = CmdKind::RootHandler;
    cm.handler = c.mem.load<uint32_t>(w);
    cm.a0 = c.mem.load<int32_t>(w + 4);
    cm.a1 = c.mem.load<int32_t>(w + 8);
    cm.async_ctrl = c.mem.load<uint32_t>(w + 12);
    cm.async_shift = c.mem.load<uint32_t>(w + 16);
    cm.t = c.mem.load<uint32_t>(w + 20);
    cmds.push_back(cm);
  }
  std::vector<std::pair<int32_t, int32_t>> cur;
  for (uint32_t k = 0; k < ncur; ++k) {
    const uint32_t w = a[0] + 8 + 24 * ncmd + 8 * k;
    cur.emplace_back(c.mem.load<int32_t>(w), c.mem.load<int32_t>(w + 4));
  }
  auto s = std::make_unique<ScriptInput>(std::move(cmds), std::move(cur));
  set_input_source(c, s.get());
  scripts()[&c] = std::move(s);
}
void a_clear_script(Ctx &c, const uint32_t *, Ret &) {
  set_input_source(c, nullptr);
  scripts().erase(&c);
}
}  // namespace

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_save_game(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(save_game(c, a[0])); }
void a_load_game(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(load_game(c, a[0], static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2]), static_cast<int32_t>(a[3])));
}
void a_load_course(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(load_course(c, static_cast<int32_t>(a[0]))); }
void a_refresh_terrain(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(refresh_terrain(c)); }
void a_tier_price(Ctx &, const uint32_t *a, Ret &r) { r.set_i32(tier_price(static_cast<int32_t>(a[0]))); }
void a_course_action_menu(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(course_action_menu(c)); }
void a_save_course(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(save_course(c)); }
void a_save_game_dialog(Ctx &c, const uint32_t *, Ret &r) { r.set_i32(save_game_dialog(c)); }
void a_prompt_and_save_file(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(prompt_and_save_file(c, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1]), static_cast<int32_t>(a[2])));
}
void a_browser(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_i32(load_game_browser_screen(c, static_cast<int32_t>(a[0]), static_cast<int32_t>(a[1])));
}

const FuncDef kFuncs[] = {
    {"saveGame", "default", "int", 1, 0, a_save_game, &g_negctl_save_game},
    {"loadGame", "default", "int", 4, 0, a_load_game, &g_negctl_load_game},
    {"loadCourse", "default", "int", 1, 0, a_load_course, &g_negctl_load_course},
    {"refreshTerrain", "default", "int", 0, 0, a_refresh_terrain, &g_negctl_refresh_terrain},
    {"loadGameBrowserScreen", "default", "int", 2, 0, a_browser, &g_negctl_browser},
    {"tierPrice", "default", "int", 1, 0, a_tier_price, &g_negctl_tier_price},
    {"saveCourse", "default", "int", 0, 0, a_save_course, &g_negctl_save_course},
    {"saveGameDialog", "default", "int", 0, 0, a_save_game_dialog, &g_negctl_save_game_dialog},
    {"promptAndSaveFile", "default", "int", 3, 0, a_prompt_and_save_file, &g_negctl_prompt_and_save_file},
    {"courseActionMenu", "default", "int", 1, 0, a_course_action_menu, &g_negctl_course_action_menu},
    {"saveload.setScript", "default", "void", 1, 0, a_set_script, nullptr},
    {"saveload.clearScript", "default", "void", 0, 0, a_clear_script, nullptr},
};
}  // namespace

extern const ModuleDef kModule_saveload = {"saveload", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
