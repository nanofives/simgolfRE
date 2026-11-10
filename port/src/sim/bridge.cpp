// The view / simulation bridge (see portsim/sim/bridge.h). Phase 3.3, written from the specification (analysis notes,
// hook sources as reading reference, the C3 path-1 runs); no original address or code.
#include "portsim/sim/bridge.h"
#include <vector>
#include <memory>
#include <cmath>    // P19A: fabs (world_to_screen_exact)
#include <cstring>

#include <initializer_list>

#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/registry_list.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   worldToScreen  1 = rotation 2's y offset + 5 Z instead of - 5 Z, 2 = the second screen test ignores the margin,
//                  3 = TTA_LEVEL_MAX heights read without the - 3
//   playSoundAt    1 = the clamp arm keeps field34, 2 = pitch 0x12d - draw, 3 = the variation counter is not advanced
//   queueMessage   1 = slot wraps at 7, 2 = time 0x31
//   pointsPopup    1 = the money is added, 2 = the disabled bit is 0x2000000
int g_negctl_world_to_screen = 0;
int g_negctl_play_sound_at = 0;
int g_negctl_queue_message = 0;
int g_negctl_points_popup = 0;

Ret call_peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const int i = registry_find(name);
  if (i < 0) throw MissingResource{name};
  const FuncDef *f = registry_at(i, nullptr);
  uint32_t a[16] = {0};
  size_t n = 0;
  for (uint32_t v : args) a[n++] = v;
  Ret r;
  f->fn(ctx, a, r);
  return r;
}
int32_t corner_heights(Ctx &ctx, int32_t x, int32_t y, int32_t k, int32_t f) {
  return i32(call_peer(ctx, "cornerHeights", {u32(x), u32(y), u32(k), u32(f)}).eax);
}
int32_t height_blend(Ctx &ctx, int32_t x, int32_t y) { return i32(call_peer(ctx, "heightBlend", {u32(x), u32(y)}).eax); }
// cornerRange's min (3.1a's function; its out-pointers keep it off the registry): the min of heightBlend at (x, y),
// (x + 1, y), (x + 1, y - 1), (x, y - 1), as cornerRange computes it
int32_t corner_range_min(Ctx &ctx, int32_t x, int32_t y) {
  int32_t mn = height_blend(ctx, x, y);
  const int32_t xs[3] = {add32(x, 1), add32(x, 1), x}, ys[3] = {y, sub32(y, 1), sub32(y, 1)};
  for (int i = 0; i < 3; ++i) {
    const int32_t w = height_blend(ctx, xs[i], ys[i]);
    if (w < mn) mn = w;
  }
  return mn;
}
// playSound is the audio backend's: a registered one is called, otherwise there is no simulation effect (U-0861)
int32_t play_sound(Ctx &ctx, int32_t idx, int32_t volume, int32_t pan, int32_t pitch, int32_t f34) {
  if (registry_find("playSound") >= 0)
    return i32(call_peer(ctx, "playSound", {u32(idx), u32(volume), u32(pan), u32(pitch), u32(f34)}).eax);
  return idx == -1 ? -1 : 0;
}
}  // namespace

// ------------------------------------------------------------------------------------------------ the view block (P8B)
int g_defect_w2s_wide = 1;   // worldToScreen.wideViewVisibility
int g_defect_t2s_wide = 1;   // tileToScreen.wideViewVisibility (tick_render.inc)
bool view_map(Ctx &ctx) {
  if (ctx.view) return true;
  auto block = std::make_shared<std::vector<uint8_t>>(static_cast<size_t>(kPortViewSize));   // zero-filled
  const uint32_t w = 800;
  std::memcpy(block->data(), &w, 4);
  if (!ctx.mem.map(kPortViewVa, block->data(), kPortViewSize, false)) return false;
  ctx.view = block;
  return true;
}
int32_t view_width(Ctx &ctx) {
  if (!ctx.view) return 800;
  const int32_t w = ctx.mem.load<int32_t>(kPortViewWidthVa);
  return w < 800 ? 800 : w;
}
bool view_click_vacated(Ctx &ctx) { return ctx.view && ctx.mem.load<int32_t>(kPortViewVacatedVa) != 0; }  // P9E fix 3
bool view_take_arrow(Ctx &ctx, int32_t &code, uint32_t &t, int32_t &ups) {  // P21B
  code = 0;
  t = 0;
  ups = 0;
  if (!ctx.view) return false;
  const int32_t c = ctx.mem.load<int32_t>(kPortViewArrowVa);
  const int32_t u = ctx.mem.load<int32_t>(kPortViewArrowUpVa);
  if (c == 0 && u == 0) return false;
  t = ctx.mem.load<uint32_t>(kPortViewArrowTVa);
  ctx.mem.store<int32_t>(kPortViewArrowVa, 0);
  ctx.mem.store<uint32_t>(kPortViewArrowTVa, 0);
  ctx.mem.store<int32_t>(kPortViewArrowUpVa, 0);
  code = c >= 0x25 && c <= 0x28 ? c : 0;
  ups = u & 3;
  if (code == 0) ups &= 1;  // no key-down: one key-up (bit 0)
  return code != 0 || ups != 0;
}
bool view_arrow_held_due(Ctx &ctx, uint32_t head) {  // P22A
  if (!ctx.view) return true;
  const uint32_t down = ctx.mem.load<uint32_t>(kPortViewArrowHeldTVa) + ctx.mem.load<uint32_t>(kPortViewClockOffVa);
  return static_cast<int32_t>(head - down) >= kArrowHoldMs;
}
int g_defect_list_four_columns = 1;  // P22D fix 2: drawEmployeeViewPanel.fourColumns
int32_t view_list_columns(Ctx &ctx) {
  if (SG_DEFECT(g_defect_list_four_columns) || !ctx.view) return 0;
  const int32_t n = ctx.mem.load<int32_t>(kPortViewListColsVa);
  return n < 0 ? 0 : n > kViewListColsMax ? kViewListColsMax : n;
}
// ------------------------------------------------------------------------------------------------ course undo (P11D)
// The original's own undo is one level per tile: setGrids keeps the tile's previous content (code) and the price charged
// (refund), and a right click then a second right click on the tile (mainLoop ML+0xfbac..ML+0xfc4b) runs demolishObject, which
// puts the previous content back and refunds the price. mainLoop.courseUndo = 0 orders those records: every setGrids call is
// journalled (with the code / refund it overwrote), the edits of one left press form a step, and an undo request runs
// demolishObject on each tile of the newest step (newest first), then gives the tile the code / refund it had before that edit,
// so an older step (or the game's own right-click undo) finds its record again. Fix 3: a placed object (H2_Build's
// stampCourseTiles record) is journalled with what its placement charged; its undo removes it as the bulldozer does
// (clearObjectFootprint, no popup) and gives back exactly that charge. Journal and redo list: view block +0x100..
int g_defect_course_undo = 1;
namespace {
// +0 journal entries, +4 redo entries, +8 the left-press counter, +12 1 while a redo builds, +16 the redone step's press,
// +20 the orientation a redone object gets (-1: the placement's own)
constexpr uint32_t kUndoHead = kPortViewVa + 0x100u;
constexpr uint32_t kUndoJournal = kPortViewVa + 0x200u;
constexpr uint32_t kUndoRedo = kPortViewVa + 0x8000u;
constexpr uint32_t kUndoCap = 1000u;  // 32 bytes each: 0x200 + 0x7d00 < 0x8000, 0x8000 + 0x7d00 < kPortViewSize
// kinds: a tile edit setGrids recorded, a paint the original keeps no record of (refused), a placed object, an object placement
// that cannot be undone (refused: the clubhouse, a placement that removed earlier objects of its type, a failed record)
enum : uint8_t { kTileEdit = 0, kTileUnrecorded = 1, kObject = 2, kObjectRefused = 3 };
// tile edit: code_old / code_new / refund_old / refund_new the grids before / after, type_new the tile type after the edit.
// object: code_new the g_placed_objects record, code_old its orientation (set by the undo), refund_old 1 when the placement used
// a free unlock (g_freeUnlockMask bit `variant`), price the cash it charged, ledger its facilities / total charge.
struct UndoEntry {
  uint8_t x, y, code_old, code_new;
  int8_t refund_old, refund_new, type_new;
  uint8_t kind;
  int32_t tool;     // [f_w_4c2848] (terrain tool)
  uint32_t press;
  int32_t tool3;    // [f_w_4c2848 + 0xc] (object tool, -1 none)
  int32_t variant;  // [unk_0056a514] (the selected variant several tools read)
  int32_t price;
  int32_t ledger;
};
static_assert(sizeof(UndoEntry) == 32, "UndoEntry");
uint32_t undo_word(Ctx &ctx, uint32_t off) { return ctx.mem.load<uint32_t>(kUndoHead + off); }
void set_undo_word(Ctx &ctx, uint32_t off, uint32_t v) { ctx.mem.store<uint32_t>(kUndoHead + off, v); }
UndoEntry undo_entry(Ctx &ctx, uint32_t base, uint32_t i) {
  UndoEntry e;
  uint8_t *p = reinterpret_cast<uint8_t *>(&e);
  for (uint32_t k = 0; k < sizeof e; ++k) p[k] = ctx.mem.load<uint8_t>(base + i * 32u + k);
  return e;
}
void set_undo_entry(Ctx &ctx, uint32_t base, uint32_t i, const UndoEntry &e) {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&e);
  for (uint32_t k = 0; k < sizeof e; ++k) ctx.mem.store<uint8_t>(base + i * 32u + k, p[k]);
}
// appends to a list of up to kUndoCap entries (count word `off`); a full list loses its oldest entry
void undo_push(Ctx &ctx, uint32_t base, uint32_t off, const UndoEntry &e) {
  uint32_t n = undo_word(ctx, off);
  if (n > kUndoCap) n = kUndoCap;
  if (n == kUndoCap) {
    for (uint32_t i = 1; i < n; ++i) set_undo_entry(ctx, base, i - 1, undo_entry(ctx, base, i));
    --n;
  }
  set_undo_entry(ctx, base, n, e);
  set_undo_word(ctx, off, n + 1);
}
bool undo_on(Ctx &ctx) { return !SG_DEFECT(g_defect_course_undo) && view_map(ctx); }
UndoEntry undo_base(Ctx &ctx, int32_t x, int32_t y, uint8_t kind) {
  const GameState &gs = ctx.gs();
  UndoEntry e{};
  e.x = static_cast<uint8_t>(x);
  e.y = static_cast<uint8_t>(y);
  e.kind = kind;
  e.type_new = gs.g_tileType[x * 50 + y];
  e.tool = gs.f_w_4c2848[0];
  e.tool3 = gs.f_w_4c2848[3];
  e.variant = gs.unk_0056a514;
  return e;
}
void undo_append(Ctx &ctx, UndoEntry e) {
  const bool redo = undo_word(ctx, 12) != 0;
  e.press = redo ? undo_word(ctx, 16) : undo_word(ctx, 8);
  undo_push(ctx, kUndoJournal, 0, e);
  if (!redo) set_undo_word(ctx, 4, 0);  // a new edit ends the redo list (a redo's own edits keep it)
}
void undo_note(Ctx &ctx, int32_t x, int32_t y, uint8_t kind, int32_t oc, int32_t orf, int32_t nc, int32_t nrf) {
  if (x < 0 || x >= 50 || y < 0 || y >= 50) return;
  UndoEntry e = undo_base(ctx, x, y, kind);
  e.code_old = static_cast<uint8_t>(oc);
  e.code_new = static_cast<uint8_t>(nc);
  e.refund_old = static_cast<int8_t>(orf);
  e.refund_new = static_cast<int8_t>(nrf);
  undo_append(ctx, e);
}
bool tile_free_of_objects(Ctx &ctx, int32_t x, int32_t y) {
  const GameState &gs = ctx.gs();
  if (gs.g_tileFlags[x * 50 + y] & ((1u << 10) | (1u << 15))) return false;
  return call_peer(ctx, "tileBlocked", {u32(x), u32(y)}).eax == 0;
}
// the tile still holds what the entry's edit left there: same code, refund and type, no placed object (demolishObject would
// ask to demolish the object instead), not the refusal bit 0x8000, not blocked
bool undo_entry_live(Ctx &ctx, const UndoEntry &e) {
  const GameState &gs = ctx.gs();
  const int32_t i = e.x * 50 + e.y;
  if (gs.g_tileBuildCode[i] != e.code_new || gs.g_tileRefund[i] != e.refund_new || gs.g_tileType[i] != e.type_new) return false;
  return tile_free_of_objects(ctx, e.x, e.y);
}
// the object the entry placed is still its record: same type at the same tile
bool undo_object_live(Ctx &ctx, const UndoEntry &e) {
  if (e.code_new == 0) return false;  // record 0: H2_Remove never removes it either (ML+0x10732)
  const PlacedObject &o = ctx.gs().g_placed_objects[e.code_new];
  return o.type == e.tool3 && o.x == e.x && o.y == e.y;
}
// fix 3: the object's removal with the exact charge back. clearObjectFootprint is the bulldozer's removal (H2_Remove ML+0x108f0);
// its own lot refund (value / 50 + rateLot / 2 into the cash and the home-sites row) is taken back, then the placement's charge is
// reversed: the cash gets `price` (what H2_Build's ML+0x10613 took), the facilities row and the total dword g_holes+0x2888 the
// `ledger` charge (ML+0x1058f / ML+0x105af), and a lot's home-sites row loses what ML+0x10607 added (ledger - price: the two rateLot
// calls ML+0x105c2 / ML+0x105f0 read the same tile with nothing written between them but that row)
void undo_object(Ctx &ctx, UndoEntry &e) {
  GameState &gs = ctx.gs();
  const int32_t row = gs.g_year % 100;
  const int32_t cash0 = gs.g_cash_div100;
  const int16_t home0 = gs.g_financeYears[row].home_sites;
  e.code_old = gs.g_placed_objects[e.code_new].orientation;
  call_peer(ctx, "clearObjectFootprint", {u32(e.code_new)});
  gs.g_cash_div100 = add32(cash0, e.price);
  gs.g_financeYears[row].home_sites = home0;
  gs.g_financeYears[row].facilities = static_cast<int16_t>(gs.g_financeYears[row].facilities + e.ledger);
  if (e.tool3 == 5) gs.g_financeYears[row].home_sites = static_cast<int16_t>(home0 - (e.ledger - e.price));
  const uint32_t total = state_va(ctx, &GameState::g_holes) + 0x2888u;
  ctx.mem.store<int32_t>(total, sub32(ctx.mem.load<int32_t>(total), e.ledger));
  if (e.refund_old) gs.g_freeUnlockMask |= 1u << (static_cast<uint32_t>(e.variant) & 31u);
  points_popup(ctx, e.price, add32(shl32(e.x, 10), 0x200), add32(shl32(e.y, 10), 0x200), -1);
  call_peer(ctx, "rebuildTileCoverage", {});  // H2_Remove's ending (ML+0x108f8 / ML+0x108fd)
  call_peer(ctx, "rebuildHeightfield", {});
}
void course_undo(Ctx &ctx) {
  uint32_t n = undo_word(ctx, 0);
  if (n > kUndoCap) n = kUndoCap;
  if (n == 0) {
    play_sound(ctx, 0x18, 100, 0, 0, 0);  // nothing to undo: the refusal sound H2_Snd18 plays
    return;
  }
  const uint32_t press = undo_entry(ctx, kUndoJournal, n - 1).press;
  bool refused = false;
  while (n > 0) {
    UndoEntry e = undo_entry(ctx, kUndoJournal, n - 1);
    if (e.press != press) break;
    --n;
    set_undo_word(ctx, 0, n);
    if (e.kind == kObject && undo_object_live(ctx, e)) {
      undo_object(ctx, e);
      undo_push(ctx, kUndoRedo, 4, e);
      continue;
    }
    if (e.kind != kTileEdit || !undo_entry_live(ctx, e)) {
      refused = true;
      continue;
    }
    call_peer(ctx, "demolishObject", {u32(e.x), u32(e.y), 0});
    GameState &gs = ctx.gs();
    const int32_t i = e.x * 50 + e.y;
    gs.g_tileBuildCode[i] = e.code_old;
    gs.g_tileRefund[i] = e.refund_old;
    e.type_new = gs.g_tileType[i];  // the redo list keeps the type the undo left
    undo_push(ctx, kUndoRedo, 4, e);
  }
  if (refused) play_sound(ctx, 0x18, 100, 0, 0, 0);
}
}  // namespace

void course_undo_note(Ctx &ctx, int32_t x, int32_t y, int32_t old_code, int32_t old_refund, int32_t new_code,
                      int32_t new_refund) {
  if (undo_on(ctx)) undo_note(ctx, x, y, kTileEdit, old_code, old_refund, new_code, new_refund);
}
void course_undo_unrecorded(Ctx &ctx, int32_t x, int32_t y) {
  if (!undo_on(ctx)) return;
  if (x < 0 || x >= 50 || y < 0 || y >= 50) return;
  const int32_t i = x * 50 + y;
  const GameState &gs = ctx.gs();
  undo_note(ctx, x, y, kTileUnrecorded, gs.g_tileBuildCode[i], gs.g_tileRefund[i], gs.g_tileBuildCode[i], gs.g_tileRefund[i]);
}
void course_undo_object(Ctx &ctx, int32_t x, int32_t y, int32_t rec, int32_t price, int32_t ledger, bool free_unlock,
                        bool undoable) {
  if (!undo_on(ctx)) return;
  if (x < 0 || x >= 50 || y < 0 || y >= 50) return;
  UndoEntry e = undo_base(ctx, x, y, undoable && rec > 0 && rec < 256 ? kObject : kObjectRefused);
  e.code_new = static_cast<uint8_t>(rec);
  e.refund_old = free_unlock ? 1 : 0;
  e.price = price;
  e.ledger = ledger;
  const int32_t orient = static_cast<int32_t>(undo_word(ctx, 20));
  if (undo_word(ctx, 12) != 0 && orient >= 0 && e.kind == kObject)  // a redone object gets the orientation it had (fix 3)
    ctx.gs().g_placed_objects[rec].orientation = static_cast<uint8_t>(orient);
  undo_append(ctx, e);
}
void course_undo_press(Ctx &ctx) {
  if (undo_on(ctx)) set_undo_word(ctx, 8, undo_word(ctx, 8) + 1);
}
void course_undo_reset(Ctx &ctx) {
  if (!undo_on(ctx)) return;
  set_undo_word(ctx, 0, 0);
  set_undo_word(ctx, 4, 0);
}
int32_t course_undo_step(Ctx &ctx) {
  if (SG_DEFECT(g_defect_course_undo) || !ctx.view) return 0;
  const int32_t req = ctx.mem.load<int32_t>(kPortViewUndoVa);
  if (req == 0) return 0;
  ctx.mem.store<int32_t>(kPortViewUndoVa, 0);
  if (req == 1) course_undo(ctx);
  return req == 1 || req == 2 ? req : 0;
}
// P11D fix 2: the redo list's newest step. The undo pushed a step's entries newest first, so popping gives them in build order.
// A tile is redone only when it still holds what the undo left: the edit's old code / refund (course_undo restored them), the
// type the undo left (kept in type_new), no object, no 0x8000 bit, not blocked. An object is placed again wherever the game's
// own placement test (evalPlacementArea in H2_Build) allows it.
bool course_redo_take(Ctx &ctx, std::vector<CourseRedoItem> &out, uint32_t &press) {
  out.clear();
  if (!undo_on(ctx)) return false;
  uint32_t n = undo_word(ctx, 4);
  if (n > kUndoCap) n = kUndoCap;
  if (n == 0) {
    play_sound(ctx, 0x18, 100, 0, 0, 0);
    return false;
  }
  press = undo_entry(ctx, kUndoRedo, n - 1).press;
  bool refused = false;
  while (n > 0) {
    const UndoEntry e = undo_entry(ctx, kUndoRedo, n - 1);
    if (e.press != press) break;
    --n;
    set_undo_word(ctx, 4, n);
    const GameState &gs = ctx.gs();
    const int32_t i = e.x * 50 + e.y;
    bool live = true;
    if (e.kind == kTileEdit)
      live = gs.g_tileBuildCode[i] == e.code_old && gs.g_tileRefund[i] == e.refund_old && gs.g_tileType[i] == e.type_new &&
             tile_free_of_objects(ctx, e.x, e.y);
    if (!live) {
      refused = true;
      continue;
    }
    out.push_back({e.x, e.y, e.tool, e.tool3, e.variant, e.kind == kObject ? e.code_old : -1});
  }
  if (refused) play_sound(ctx, 0x18, 100, 0, 0, 0);
  return true;
}
void course_redo_begin(Ctx &ctx, uint32_t press) {
  set_undo_word(ctx, 12, 1);
  set_undo_word(ctx, 16, press);
  set_undo_word(ctx, 20, 0xFFFFFFFFu);
}
void course_redo_orientation(Ctx &ctx, int32_t orientation) { set_undo_word(ctx, 20, static_cast<uint32_t>(orientation)); }
void course_redo_end(Ctx &ctx) {
  set_undo_word(ctx, 12, 0);
  set_undo_word(ctx, 20, 0xFFFFFFFFu);
}

int32_t wide_view_margin_w2s(Ctx &ctx) { return g_defect_w2s_wide ? 0 : (view_width(ctx) - 800) / 2; }
int32_t wide_view_margin_t2s(Ctx &ctx) { return g_defect_t2s_wide ? 0 : (view_width(ctx) - 800) / 2; }

// ------------------------------------------------------------------------------------------------ worldToScreen
namespace {
// P19A item 7 (bridge.h world_to_screen_exact): the last call's exact projection on this thread (render-only)
struct W2sExact {
  bool ok = false;
  int32_t wx = 0, wy = 0, sx = 0, sy = 0;
  double ex = 0, ey = 0;
};
thread_local W2sExact g_w2s_exact;
}  // namespace

bool world_to_screen_exact(int32_t wx, int32_t wy, int32_t sx, int32_t sy, double &ex, double &ey) {
  const W2sExact &e = g_w2s_exact;
  if (!e.ok || e.wx != wx || e.wy != wy || e.sx != sx || e.sy != sy) return false;
  ex = e.ex;
  ey = e.ey;
  return true;
}

int32_t world_to_screen(Ctx &ctx, int32_t wx, int32_t wy, int32_t &sx, int32_t &sy, int32_t m) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_world_to_screen);
  g_w2s_exact.ok = false;
  const int32_t Z = gs.g_zoomScale;
  int32_t ix = sar32(mul32(sub32(sub32(wx, shl32(gs.gCameraTileX, 10)), 0x200), Z), 7);
  int32_t iy = sar32(mul32(sub32(sub32(wy, shl32(gs.gCameraTileY, 10)), 0x200), Z), 7);
  // P19A: the same values without the integer divisions (exs / eys follow sx / sy below)
  double ixe = static_cast<double>(sub32(sub32(wx, shl32(gs.gCameraTileX, 10)), 0x200)) * Z / 128.0;
  double iye = static_cast<double>(sub32(sub32(wy, shl32(gs.gCameraTileY, 10)), 0x200)) * Z / 128.0;
  double exs = 0, eys = 0;
  int32_t W = gs.g_screenWidth;
  if (W == 0x400) {
    ix = mul32(ix, 80) / 64;
    iy = mul32(iy, 52) / 40;
    ixe = ixe * 80 / 64;
    iye = iye * 52 / 40;
  } else if (W == 0x500) {
    ix = mul32(ix, 104) / 64;
    iy = mul32(iy, 68) / 40;
    ixe = ixe * 104 / 64;
    iye = iye * 68 / 40;
  }
  const int32_t H = gs.g_screenHeight;
  const int32_t ax = mul32(ix, 20) / 32, ay = mul32(iy, 20) / 32;
  const double axe = ixe * 20 / 32, aye = iye * 20 / 32;
  // four separate equality tests, as in the original (the width is re-read after each block)
  if (gs.g_cameraRot == 0) {
    sx = add32(add32(add32(iy, W / 2), mul32(Z, 8)), ix);
    sy = add32(sub32(ay, ax), H / 2);
    exs = iye + W / 2 + Z * 8 + ixe;
    eys = aye - axe + H / 2;
    W = gs.g_screenWidth;
  }
  if (gs.g_cameraRot == 2) {
    sx = add32(sub32(W / 2, ix), iy);
    const int32_t z5 = mul32(gs.g_zoomScale, 5);
    sy = add32(add32(sub32(mul32(ix, -20) / 32, ay), H / 2), mode == 1 ? z5 : neg32(z5));
    exs = W / 2 - ixe + iye;
    eys = ixe * -20 / 32 - aye + H / 2 - z5;
    W = gs.g_screenWidth;
  }
  if (gs.g_cameraRot == 4) {
    sx = sub32(sub32(sub32(W / 2, mul32(gs.g_zoomScale, 8)), iy), ix);
    sy = add32(sub32(ax, ay), H / 2);
    exs = W / 2 - gs.g_zoomScale * 8 - iye - ixe;
    eys = axe - aye + H / 2;
    W = gs.g_screenWidth;
  }
  if (gs.g_cameraRot == 6) {
    sx = add32(sub32(W / 2, iy), ix);
    sy = add32(add32(add32(ay, ax), H / 2), mul32(gs.g_zoomScale, 5));
    exs = W / 2 - iye + ixe;
    eys = aye + axe + H / 2 + gs.g_zoomScale * 5;
    W = gs.g_screenWidth;
  }
  // P8B: with worldToScreen.wideViewVisibility at 0 the x window is the wide view's (E more on each side); E = 0 at 1
  const int32_t E = wide_view_margin_w2s(ctx);
  const int32_t nm = sub32(neg32(m), E), half = m / 2;
  if (sx < nm || sx >= add32(add32(W, m), E)) return 0;
  if (sy < neg32(half) || sy >= add32(half, gs.g_screenHeight)) return 0;

  const int32_t tx = sar32(wx, 10), ty = sar32(wy, 10);
  const int32_t cell = add32(mul32(tx, 50), ty);
  const int8_t type = state_arr(ctx, &GameState::g_tileType)[cell];
  const uint32_t f = (state_arr(ctx, &GameState::g_tileTypeAttrs) + type).field(&TileTypeAttr::flags).get();
  const int32_t S = gs.f_w_4c2e00[0];
  if (!(f & 8)) {
    if (f & 2) {
      const int32_t mn = corner_range_min(ctx, tx, ty);
      sy = add32(sy, neg32(mul32(mul32(sub32(mn, 3), S), gs.g_zoomScale) / 4));
      eys -= static_cast<double>(mul32(mul32(sub32(mn, 3), S), gs.g_zoomScale)) / 4;
    } else if (f & 4) {
      const int32_t h = state_arr(ctx, &GameState::g_tileLevel)[cell] - (mode == 3 ? 0 : 3);
      sy = sub32(sy, mul32(mul32(h, S), gs.g_zoomScale) / 4);
      eys -= static_cast<double>(mul32(mul32(h, S), gs.g_zoomScale)) / 4;
    } else {
      const int32_t c5 = corner_heights(ctx, tx, ty, 5, 1) - 3;
      const int32_t c7 = corner_heights(ctx, tx, ty, 7, 1) - 3;
      const int32_t c1 = corner_heights(ctx, tx, ty, 1, 1) - 3;
      const int32_t c3 = corner_heights(ctx, tx, ty, 3, 1) - 3;
      int32_t dz;
      if (c5 == c7 && c5 == c1 && c5 == c3) {
        dz = neg32(mul32(mul32(S, gs.g_zoomScale), c5) / 4);
        eys -= static_cast<double>(mul32(mul32(S, gs.g_zoomScale), c5)) / 4;
      } else {
        const int32_t dx = sub32(wx, shl32(tx, 10)), dy = sub32(wy, shl32(ty, 10));
        const int32_t b = add32(mul32(add32(mul32(sub32(0x400, dx), c7), mul32(dx, c1)), sub32(0x400, dy)),
                                mul32(add32(mul32(sub32(0x400, dx), c5), mul32(dx, c3)), dy));
        const int32_t scaled = mul32(mul32(b, S), gs.g_zoomScale);
        dz = (neg32(scaled / 1024) / 1024) / 4;
        eys -= static_cast<double>(scaled) / (1024.0 * 1024.0 * 4.0);
      }
      sy = add32(sy, dz);
    }
  }
  if (gs.unk_005a9cc0 != 0) {
    sx = sub32(mul32(sx, 2), 400);
    sy = sub32(mul32(sy, 2), 300);
    exs = exs * 2 - 400;
    eys = eys * 2 - 300;
  }
  const int32_t m2 = mode == 2 ? 0 : m, h2 = mode == 2 ? 0 : half;
  if (sx < sub32(neg32(m2), E) || sx >= add32(add32(gs.g_screenWidth, m2), E)) return 0;
  if (sy < neg32(h2) || sy >= add32(h2, gs.g_screenHeight)) return 0;
  // P19A: kept only when the exact place lies within 8 pixels of the integer one (the truncations above add up to about 3; a wrapped
  // product is far outside)
  if (std::fabs(exs - sx) <= 8.0 && std::fabs(eys - sy) <= 8.0) g_w2s_exact = W2sExact{true, wx, wy, sx, sy, exs, eys};
  return 1;
}

// ------------------------------------------------------------------------------------------------ playSoundAt
int32_t play_sound_at_with(Ctx &ctx, int32_t sound, int32_t wx, int32_t wy, int32_t field34, const SoundAtCalls &calls) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_play_sound_at);
  int32_t sx = wy, sy = wx;  // worldToScreen writes over the caller's argument slots, which hold wy / wx
  const int32_t on = calls.project(wx, wy, sx, sy, 0);
  int32_t f34 = 0;
  bool clamp_arm = false;
  if (on == 0) {
    if (field34 != -1) return 0;
    clamp_arm = true;
  } else {
    f34 = field34;
    if (f34 == -1) clamp_arm = true;
  }
  if (clamp_arm) {
    sx = clamp(sx, 0, 0x31f);
    sy = clamp(sy, 0, 0x257);
    if (mode != 1) f34 = 0;
  } else {
    int32_t eax = gs.unk_005a9cbc;
    if (gs.unk_005a9cbc != 0) {
      eax = sx;
      sx = sub32(add32(sx, sx), 0x190);
      sy = sub32(add32(sy, sy), 0x190);
    }
    if (sy < 0 || sy > 0x1f4 || sx < 0 || sx >= 0x320) return eax;
  }
  const int32_t base = gs.g_zoomScale >= 4 ? 50 : 10;
  // pan = sx * 127 / 800 - 64 by the original's magic division (0x51eb851f, >> 8 of the high dword, sign bit added)
  const int32_t prod = mul32(sx, 127);
  const int32_t hi = sar32(static_cast<int32_t>((static_cast<int64_t>(prod) * 0x51eb851fLL) >> 32), 8);
  const int32_t pan = sub32(add32(hi, i32(u32(hi) >> 31)), 0x40);
  int32_t volume, pitch;
  if (gs.unk_005a9cd8 == 0) {
    uint32_t &seed = state_obj(ctx, &GameState::g_rng).field(&Random::state).ref();
    const int32_t rnd = i32(random_range(seed, 0x258) & 0xffffu);
    pitch = (mode == 2 ? 0x12d : 0x12c) - rnd;
    const int32_t t = sub32(sx, 0x190);
    volume = add32(abs32(t) / 16, base);
  } else {
    const int32_t c = gs.unk_005a9cdc;
    const int8_t b = state_arr(ctx, &GameState::g_soundVariationCycle)[mod32(c, 0x24)];
    pitch = mul32(b - 5, 100);
    if (mode != 3) gs.unk_005a9cdc = add32(c, 1);
    const int32_t h2 = static_cast<int32_t>((static_cast<int64_t>(sy) * 0x2aaaaaabLL) >> 32);  // sy / 6 by multiply
    const int32_t w = sar32(h2, 1);
    volume = add32(add32(w, base), i32(u32(w) >> 31));
  }
  const int32_t rv = calls.play(sound, volume, pan, pitch, f34);
  gs.unk_005a9cd8 = 0;
  return rv;
}

int32_t play_sound_at(Ctx &ctx, int32_t sound, int32_t wx, int32_t wy, int32_t field34) {
  SoundAtCalls calls;
  calls.project = [&ctx](int32_t x, int32_t y, int32_t &sx, int32_t &sy, int32_t m) { return world_to_screen(ctx, x, y, sx, sy, m); };
  calls.play = [&ctx](int32_t i, int32_t v, int32_t p, int32_t t, int32_t f) { return play_sound(ctx, i, v, p, t, f); };
  return play_sound_at_with(ctx, sound, wx, wy, field34, calls);
}

// ------------------------------------------------------------------------------------------------ the two label rings
void queue_message(Ctx &ctx, int32_t a, int32_t b, int32_t c) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_queue_message);
  const int32_t slot = gs.g_ui_53bba8;
  (state_arr(ctx, &GameState::g_labelQueueC) + slot).set(a);
  (state_arr(ctx, &GameState::g_floatLabelY) + slot).set(b);
  (state_arr(ctx, &GameState::g_labelQueueA) + slot).set(c);
  (state_arr(ctx, &GameState::g_labelQueueB) + slot).set(mode == 2 ? 0x31 : 0x30);
  vstrcpy(ctx.mem, state_va(ctx, &GameState::g_labelQueueText) + u32(mul32(slot, 64)), state_va(ctx, &GameState::g_textBuffer));
  gs.g_ui_53bba8 = mod32(add32(slot, 1), mode == 1 ? 7 : 8);
}

void points_popup(Ctx &ctx, int32_t pts, int32_t x, int32_t y, int32_t who) {
  GameState &gs = ctx.gs();
  const int mode = SG_NEGCTL(g_negctl_points_popup);
  if (pts == 0 || (gs.g_flags & (mode == 2 ? 0x2000000u : 0x1000000u))) return;
  if (who != -1) {
    const Ptr<int32_t> money = (state_arr(ctx, &GameState::g_holes) + who).field(&HoleRecord::money);
    money.set(mode == 1 ? add32(money.get(), pts) : sub32(money.get(), pts));
  }
  const int32_t slot = gs.g_points59abb0;
  (state_arr(ctx, &GameState::g_moneyLabelX) + slot).set(x);
  (state_arr(ctx, &GameState::g_moneyLabelY) + slot).set(y);
  (state_arr(ctx, &GameState::g_moneyLabelValue) + slot).set(pts);
  (state_arr(ctx, &GameState::g_moneyLabelTime) + slot).set(0x18);
  gs.g_points59abb0 = mod32(add32(slot, 1), 8);
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }

// worldToScreen(wx, wy, int *sx, int *sy, margin): the out cells are read first (a rotation outside 0 / 2 / 4 / 6 tests
// them as they are, and the height arm and the doubling still update them) and written back (an untouched cell gets
// its own value again)
void a_world_to_screen(Ctx &c, const uint32_t *a, Ret &r) {
  int32_t sx = c.mem.load<int32_t>(a[2]), sy = c.mem.load<int32_t>(a[3]);
  const int32_t res = world_to_screen(c, S(a, 0), S(a, 1), sx, sy, S(a, 4));
  c.mem.store<int32_t>(a[2], sx);
  c.mem.store<int32_t>(a[3], sy);
  r.set_i32(res);
}
void a_play_sound_at(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(play_sound_at(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3))); }
void a_queue_message(Ctx &c, const uint32_t *a, Ret &) { queue_message(c, S(a, 0), S(a, 1), S(a, 2)); }
void a_points_popup(Ctx &c, const uint32_t *a, Ret &) { points_popup(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3)); }

// Test-only: playSoundAt with the projection and playSound scripted (the C3 runs stood both in). args: sound, wx, wy,
// field34, script (a block: +0 the projection result, +4 write flag, +8 sx, +0xc sy, +0x10 playSound's result; the entry
// logs the projection call at +0x20 {count, wx, wy, m, sx in, sy in} and the playSound call at +0x40 {count, idx, volume,
// pan, pitch, f34}).
void a_play_sound_at_scripted(Ctx &c, const uint32_t *a, Ret &r) {
  const uint32_t blk = a[4];
  const Memory &m = c.mem;
  SoundAtCalls calls;
  calls.project = [&m, blk](int32_t x, int32_t y, int32_t &sx, int32_t &sy, int32_t mg) {
    const uint32_t lg = blk + 0x20;
    m.store<uint32_t>(lg, m.load<uint32_t>(lg) + 1);
    m.store<int32_t>(lg + 4, x);
    m.store<int32_t>(lg + 8, y);
    m.store<int32_t>(lg + 12, mg);
    m.store<int32_t>(lg + 16, sx);
    m.store<int32_t>(lg + 20, sy);
    if (m.load<uint32_t>(blk + 4) != 0) {
      sx = m.load<int32_t>(blk + 8);
      sy = m.load<int32_t>(blk + 12);
    }
    return m.load<int32_t>(blk);
  };
  calls.play = [&m, blk](int32_t i, int32_t v, int32_t p, int32_t t, int32_t f) {
    const uint32_t lg = blk + 0x40;
    m.store<uint32_t>(lg, m.load<uint32_t>(lg) + 1);
    const int32_t vals[5] = {i, v, p, t, f};
    for (int k = 0; k < 5; ++k) m.store<int32_t>(lg + 4 + 4 * u32(k), vals[k]);
    return m.load<int32_t>(blk + 0x10);
  };
  r.set_i32(play_sound_at_with(c, S(a, 0), S(a, 1), S(a, 2), S(a, 3), calls));
}

int g_negctl_none = 0;
const FuncDef kFuncs[] = {
    {"worldToScreen", "default", "int", 5, 0, a_world_to_screen, &g_negctl_world_to_screen},
    {"playSoundAt", "default", "int", 4, 0x027F, a_play_sound_at, &g_negctl_play_sound_at},
    {"queueMessage", "default", "void", 3, 0, a_queue_message, &g_negctl_queue_message},
    {"pointsPopup", "default", "void", 4, 0, a_points_popup, &g_negctl_points_popup},
    {"bridge.playSoundAtScripted", "default", "int", 5, 0x027F, a_play_sound_at_scripted, &g_negctl_none},
};
}  // namespace

const DefectDef kDefects[] = {
    {"worldToScreen.wideViewVisibility", "P8B,G32",
     "worldToScreen's on-screen test (playSoundAt's RNG draw, the story advances of golfers on screen) is the original's 800-wide "
     "window whatever the host's view width; fixed: the window is the wide view's (view block width W: (W - 800) / 2 more on each side)",
     &g_defect_w2s_wide},
    {"tileToScreen.wideViewVisibility", "P8B,G32",
     "tileToScreen caches and returns a tile's screen point only inside x -0x40 .. 800 + 0x40 (the scenery buildSceneSprites queues); "
     "fixed: that x window is the wide view's (view block width W: (W - 800) / 2 more on each side)",
     &g_defect_t2s_wide},
    {"mainLoop.courseUndo", "P11D",
     "the only undo is the original's one level per tile (right click twice: demolishObject restores the tile's previous content "
     "and refunds its price); fixed: every setGrids edit is journalled with what it overwrote, the edits of one left press form a "
     "step, and the host's undo request (view block word +8, Ctrl+Z) runs demolishObject on the newest step's tiles, newest "
     "first, then gives each tile its earlier record back; a redo request (word +8 = 2, Ctrl+Y) builds the newest undone step "
     "again through the game's terrain-tool click path (H2_Terrain) at today's price; a placed object (H2_Build) is undone as the "
     "bulldozer removes it (clearObjectFootprint, no popup) with exactly its placement charge back, and redone through H2_Build",
     &g_defect_course_undo},
    {"drawEmployeeViewPanel.fourColumns", "P22D",
     "the golfer list has four columns of four golfers (a column whose name x passes 0x2b7 is dropped) and its arrows page by 0x10, "
     "whatever room the host's view has; fixed: with the view block's word +0x40 = n (the host's room right of the list, in 0x79-wide "
     "columns) drawEmployeeViewPanel lists 4 + n columns, hitEmployeeSlot435570 hits them and the next-page arrow at its moved place "
     "(0x304 + 0x79 n), and the arrows page by 4 (4 + n); the host draws the added columns right of the panel",
     &g_defect_list_four_columns},
};
extern const ModuleDef kModule_bridge = {"bridge", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                         sizeof(kDefects) / sizeof(kDefects[0])};

}  // namespace portsim
