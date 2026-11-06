// The character editor (see portsim/sim/chareditor.h). Phase 3.8, stream 38h, written from the specification (the C3 / C4
// hook sources c3fx.cpp showCharacterEditor, c3em.cpp pickCharEditor, c3v.cpp nearestCharControl, c3u.cpp charEditorZone,
// read as reference; every address is in log/p38/38h_notes.md, none here). The hook sources address golfer records from a
// base 8 bytes above g_golfers: their offsets are 8 lower than the model's used here.
#include "portsim/sim/chareditor.h"

#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "commands_internal.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/gfx.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   showCharacterEditor  1 = control 13 steps by 2, 2 = Exit does not copy the preview preferences into the golfer, 3 =
//                        Cancel keeps the edited record, 4 = the hair counter steps by 2, 5 = a comment press edits the
//                        next line, 6 = the tip after 9 frames, 7 = a preset takes look_28 from byte 2 of its quote record,
//                        8 = the pro record's byte 3 from look_25, 9 = a press on the name area divides its line by 21
//   pickCharEditor       1 = a right click returns 0, 2 = the down arrow adds 9
//   nearestCharControl   1 = only the first 7 spots divide the x distance by 3
//   charEditorZone       1 = the right arrow's radius 0x3b
int g_negctl_editor = 0;
int g_negctl_pick = 0;
int g_negctl_nearest = 0;
int g_negctl_zone = 0;
int g_negctl_none = 0;

using cm::call_peer;
using cm::cat;
using cm::cpy;

// g_rootInput slots (int32[16])
constexpr int kButton = 4, kClicked = 7;
// the preview golfer the editor draws
constexpr int32_t kPreview = 0x99;
// the golfer type record the entry copy is kept in
constexpr uint32_t kBackupType = 83;

// golfer record g (0x100 bytes; the address wraps like the original's shl 8) and golfer type record t (0x230 bytes)
inline uint32_t rec(Ctx &ctx, int32_t g) { return GS(g_golfers) + (u32(g) << 8); }
inline uint32_t type_rec(Ctx &ctx, int32_t t) { return GS(g_golferTypes) + u32(mul32(t, 0x230)); }
// a string of the editor's pool f_strpool_4c84e8 by its offset
inline uint32_t pool(Ctx &ctx, uint32_t off) { return GS(f_strpool_4c84e8) + off; }

// the static CRT's __itoa(v, buf, 10): '-' for a negative value, the digits, a NUL; returns buf
uint32_t itoa10(Ctx &ctx, int32_t v, uint32_t buf) {
  char tmp[16];
  int n = 0;
  uint32_t mag = v < 0 ? 0u - u32(v) : u32(v);
  do {
    tmp[n++] = static_cast<char>('0' + mag % 10);
    mag /= 10;
  } while (mag != 0);
  uint32_t p = buf;
  if (v < 0) ctx.mem.store<uint8_t>(p++, '-');
  while (n > 0) ctx.mem.store<uint8_t>(p++, static_cast<uint8_t>(tmp[--n]));
  ctx.mem.store<uint8_t>(p, 0);
  return buf;
}

// ---- test seam (chareditor.scriptPeer): per context, peers whose calls are logged instead of run and answered with
// script_value (the oracle stubs the same callees of the original with the same rule): 8 dwords per call (FNV-1a of the
// name, up to 6 arguments, the argument count), the count at the log base
struct PeerScript {
  uint32_t va = 0, cap = 0;
  std::map<uint32_t, uint32_t> salt;  // name hash -> salt
};
std::map<const Ctx *, PeerScript> &peer_scripts() {
  static std::map<const Ctx *, PeerScript> m;
  return m;
}
uint32_t fnv1a(const char *s) {
  uint32_t h = 2166136261u;
  for (; *s; ++s) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
  return h;
}
// the scripted eax: FNV-1a over the argument dwords from the salt; listMatchingFiles gets a count 0..3 (bits 7..8),
// promptAndSaveFile a flag (bit 7), the others 0
uint32_t script_value(uint32_t name, uint32_t salt, std::initializer_list<uint32_t> args) {
  uint32_t v = salt;
  for (uint32_t a : args) v = (v ^ a) * 16777619u;
  if (name == fnv1a("listMatchingFiles")) return (v >> 7) & 3u;
  if (name == fnv1a("promptAndSaveFile")) return (v >> 7) & 1u;
  return 0;
}
Ret peer(Ctx &ctx, const char *name, std::initializer_list<uint32_t> args) {
  const auto it = peer_scripts().find(&ctx);
  if (it != peer_scripts().end()) {
    const uint32_t h = fnv1a(name);
    const auto s = it->second.salt.find(h);
    if (s != it->second.salt.end()) {
      const PeerScript &ps = it->second;
      const uint32_t k = ctx.mem.load<uint32_t>(ps.va);
      if (k < ps.cap) {
        const uint32_t r = ps.va + 4 + 32 * k;
        ctx.mem.store<uint32_t>(r, h);
        uint32_t j = 0;
        for (uint32_t v : args)
          if (j < 6) ctx.mem.store<uint32_t>(r + 4 + 4 * j++, v);
        ctx.mem.store<uint32_t>(r + 28, static_cast<uint32_t>(args.size()));
      }
      ctx.mem.store<uint32_t>(ps.va, k + 1);
      Ret ret;
      ret.eax = script_value(h, s->second, args);
      return ret;
    }
  }
  return call_peer(ctx, name, args);
}

// ---- the original's frame slots that live in the address space (View_toLocal writes through pointers): a zeroed block
// above the GameState, handed out and taken back in stack order
constexpr uint32_t kScratchSize = 0x1000;
struct ScratchBlock {
  std::vector<uint8_t> host;
  uint32_t va = 0, sp = 0;
};
std::map<const Ctx *, ScratchBlock> &scratch_blocks() {
  static std::map<const Ctx *, ScratchBlock> m;
  return m;
}
uint32_t scratch_base(Ctx &ctx) {
  ScratchBlock &b = scratch_blocks()[&ctx];
  if (b.va != 0) {
    try {
      if (ctx.mem.at(b.va, kScratchSize) == b.host.data()) return b.va;
    } catch (const Fault &) {
    }
  }
  b.host.assign(kScratchSize, 0);
  b.sp = 0;
  uint32_t base = (ctx.mem.data_base() + static_cast<uint32_t>(sizeof(GameState)) + 0x01800000u) & ~0xFFFFu;
  for (int tries = 0; tries < 64; ++tries, base += 0x00100000u)
    if (ctx.mem.map(base, b.host.data(), kScratchSize, false)) {
      b.va = base;
      return base;
    }
  throw MissingResource{"chareditor scratch block (no free address range)"};
}
struct Scratch {
  Ctx &ctx;
  uint32_t n, va;
  Scratch(Ctx &c, uint32_t bytes) : ctx(c), n((bytes + 3u) & ~3u), va(0) {
    const uint32_t base = scratch_base(c);
    ScratchBlock &b = scratch_blocks()[&c];
    if (b.sp + n > kScratchSize) throw MissingResource{"chareditor scratch block overflow"};
    va = base + b.sp;
    b.sp += n;
    std::memset(c.mem.at(va, n, true), 0, n);
  }
  ~Scratch() { scratch_blocks()[&ctx].sp -= n; }
};

// ---- the modal pumps
// Per context: whether the source refused a batch since the last cursor read (a trace-driven source's refusals are
// counted by cursor reads: a second refusal with no cursor read in between would make it deliver the next pass's batch),
// and whether the release wait is pumping (the test source treats those pumps as the oracle does: nothing has arrived)
struct PumpState {
  bool refused = false;
  bool release = false;
};
std::map<const Ctx *, PumpState> &pump_states() {
  static std::map<const Ctx *, PumpState> m;
  return m;
}

// View_toLocal(root window, &x, &y): with a display the cursor (the context's InputSource; none = the editor waits for
// input it cannot have), then View::fromParent; the view-tree root would subtract the game window's screen origin
void view_to_local(Ctx &ctx, uint32_t xva, uint32_t yva, const char *where) {
  pump_states()[&ctx].refused = false;
  if (ctx.gs().g_graphicsDevice != 0) {
    InputSource *src = input_source(ctx);
    int32_t x = 0, y = 0;
    if (!src || !src->cursor(x, y)) throw NeedInput{{where}};
    ctx.mem.store<int32_t>(xva, x);
    ctx.mem.store<int32_t>(yva, y);
  }
  const uint32_t root = GS(g_rootWindow);
  call_peer(ctx, "View::fromParent", {root, xva, yva});
  if (root == ctx.gs().g_viewTreeRoot) throw MissingResource{"View_toLocal: the game window's screen rectangle"};
}

// pumpMessages in a pass of the loop (Blocking: the loop only ends on input; with none left the editor waits for ever)
void pump_pass(Ctx &ctx, const char *where) {
  GameState &gs = ctx.gs();
  if (gs.g_rootInput[kClicked] == 0) gs.g_rootInput[kButton] = 0;
  InputSource *src = input_source(ctx);
  if (!src || src->exhausted()) throw NeedInput{{where}};
  std::vector<Command> batch;
  if (src->batch(PumpKind::Blocking, batch)) {
    for (const Command &c : batch) apply_command(ctx, c);
  } else {
    pump_states()[&ctx].refused = true;
  }
  gs.g_rootInput[kClicked] = 0;
}

// pumpMessages in the release wait: what the original dispatched there, nothing when no input is left (the wait ends when
// the button word is clear, which this pump makes it when no click arrived)
void pump_release(Ctx &ctx) {
  GameState &gs = ctx.gs();
  if (gs.g_rootInput[kClicked] == 0) gs.g_rootInput[kButton] = 0;
  InputSource *src = input_source(ctx);
  PumpState &ps = pump_states()[&ctx];
  if (src && !src->exhausted() && !ps.refused) {
    std::vector<Command> batch;
    ps.release = true;
    const bool got = src->batch(PumpKind::Blocking, batch);
    ps.release = false;
    if (got) {
      for (const Command &c : batch) apply_command(ctx, c);
    } else {
      ps.refused = true;
    }
  }
  gs.g_rootInput[kClicked] = 0;
}

int32_t type_bit7_clear(Ctx &ctx, int32_t g) { return i32(call_peer(ctx, "typeBit7Clear", {u32(g)}).eax); }

}  // namespace

// ------------------------------------------------------------------------------------------------ nearestCharControl
// The control spots f_tab_point16_4c7b38 ({x, y} pairs, the list ends at an x of 0): the first one within distance 0x28
// that is nearer than every earlier one (approxDistance; the first 8 spots divide the x distance by 3); -1 for none.
int32_t nearest_char_control(Ctx &ctx, int32_t x, int32_t y) {
  const Memory &m = ctx.mem;
  const uint32_t tbl = GS(f_tab_point16_4c7b38);
  const int split = SG_NEGCTL(g_negctl_nearest) == 1 ? 7 : 8;
  int32_t best = 0x28, idx = -1;
  uint32_t i = 0;
  do {
    const int32_t dx = sub32(x, m.load<int16_t>(tbl + 4 * i));
    const int32_t dy = sub32(y, m.load<int16_t>(tbl + 4 * i + 2));
    const int32_t d = static_cast<int32_t>(i) < split ? approx_distance(div32(dx, 3), dy) : approx_distance(dx, dy);
    if (d < best) {
      best = d;
      idx = i32(i);
    }
    ++i;
  } while (m.load<int16_t>(tbl + 4 * i) != 0);
  return idx;
}

// ------------------------------------------------------------------------------------------------ charEditorZone
// The ten preset centres f_tab_pair32_4c7be0 (the zone is the first whose offset point, + 0x3c on both axes, is within
// 0x3c), then the two scroll arrows (10 and 11), else -1.
int32_t char_editor_zone(Ctx &ctx, int32_t x, int32_t y) {
  const Memory &m = ctx.mem;
  const uint32_t tbl = GS(f_tab_pair32_4c7be0);
  for (uint32_t i = 0; i < 10; ++i)
    if (approx_distance(sub32(sub32(x, m.load<int32_t>(tbl + 8 * i)), 0x3c),
                        sub32(sub32(y, m.load<int32_t>(tbl + 8 * i + 4)), 0x3c)) < 0x3c)
      return i32(i);
  if (approx_distance(sub32(x, 0x30a), sub32(y, 0x16b)) < (SG_NEGCTL(g_negctl_zone) == 1 ? 0x3b : 0x3c)) return 10;
  return approx_distance(sub32(x, 0x12), sub32(y, 0x1ef)) < 0x3c ? 0xb : -1;
}

// ------------------------------------------------------------------------------------------------ pickCharEditor
// One pass: the cursor read (the original keeps x in a slot of its own and y in its argument's stack slot, so y starts at
// `row`), the zone under it (charEditorZone), the drawing (dropped), the pump (Blocking). No button: a new pass. A right
// button (2) or a press above y 0x102 returns -1, any button other than 1 returns 0; a preset zone returns the scroll
// offset g_ui_5aa55c + zone; the down arrow adds 10 while the offset is below 0x46, the up arrow takes 10 off a non-zero
// offset; the offset is then clamped to the row's count g_loopCounter59b76c[row] - 10 and to 0, and a new pass starts.
int32_t pick_char_editor(Ctx &ctx, int32_t row) {
  GameState &gs = ctx.gs();
  cm::drop_stale_esc(ctx, g_defect_esc_char_picker);  // P9A item 2
  const Memory &m = ctx.mem;
  const int neg = SG_NEGCTL(g_negctl_pick);
  Scratch xy(ctx, 8);
  m.store<int32_t>(xy.va + 4, row);
  for (;;) {
    view_to_local(ctx, xy.va, xy.va + 4, "pickCharEditor");
    const int32_t x = m.load<int32_t>(xy.va), y = m.load<int32_t>(xy.va + 4);
    const int32_t zone = char_editor_zone(ctx, x, y);
    pump_pass(ctx, "pickCharEditor");
    if (cm::esc_pressed(ctx, g_defect_esc_char_picker)) return -1;  // P9A item 2: Esc = the picker's cancel
    const int32_t button = gs.g_rootInput[kButton];
    if (button == 0) continue;
    if (button == 2) return neg == 1 ? 0 : -1;
    if (y < 0x102) return -1;
    if (button != 1) return 0;
    if (zone != -1 && zone < 10) return add32(gs.g_ui_5aa55c, zone);
    int32_t scroll = gs.g_ui_5aa55c;
    if (zone == 10) {
      if (scroll < 0x46) {
        scroll = add32(scroll, neg == 2 ? 9 : zone);
        gs.g_ui_5aa55c = scroll;
      }
    } else if (zone == 0xb && scroll != 0) {
      scroll = sub32(scroll, 10);
      gs.g_ui_5aa55c = scroll;
    }
    const int32_t limit = sub32(m.load<int32_t>(GS(g_loopCounter59b76c) + 4 * u32(row)), 10);
    if (scroll > limit) {
      scroll = limit;
      gs.g_ui_5aa55c = scroll;
    }
    if (scroll < 0) gs.g_ui_5aa55c = 0;
  }
}

// ------------------------------------------------------------------------------------------------ the EditBox family
// Roadmap S1.5 (log/roadmap/S1/5_notes.md, U-1032): the text model of the window-tree EditBox the bio entry builds, written
// from the C3 / C4 hook sources (c3v setText, c3cz resizeBuffer, c3de insertChar, c3fz onKey, c3dl resetFields, c3ef
// releaseState, c3ep create; read as reference) and judged by port/test/test_chareditor_bio.py against the original in
// Unicorn. Fields: text pointer +0x574 (a block of the context's EditBox arena below), capacity +0x578, caret +0x5a4,
// caret-blink flag +0x5a8, the style words +0x584..+0x59c. What the window-tree side does is drawing or ui-class
// bookkeeping and is not modelled: the virtual layout slot +0x120 (Window::doLayout: the +0x220 pass bit, g_curWidget, a
// relayout and an invalidate), Window::create and its ring entry in the parent, the frame quad and cursor character, the
// caret Timer, MessageBeep, Window::ctorInit (U-1191).
namespace {
//   EditBox::insertChar   1 = the caret is not advanced
//   EditBox::onKey        1 = Back removes the character AT the caret (Delete's move), 2 = Home puts the caret at 1
//   EditBox::setText      1 = the caret keeps its value
//   EditBox::resizeBuffer 1 = the old text is not copied into the new buffer
//   EditBox::resetFields  1 = +0x598 takes the config word +8
//   EditBox::releaseState 1 = the fields are not reset
//   EditBox::create       1 = a 0x3f-character buffer
int g_negctl_eb_insert = 0;
int g_negctl_eb_key = 0;
int g_negctl_eb_settext = 0;
int g_negctl_eb_resize = 0;
int g_negctl_eb_reset = 0;
int g_negctl_eb_release = 0;
int g_negctl_eb_create = 0;

// The text buffers resizeBuffer takes from malloc: blocks of a per-context arena mapped above the GameState (the original's
// heap addresses are not part of any compared state: the EditBox is a `ui` object and its text is copied out before the
// buffer is freed). A free only marks the block; the cursor goes back to the start when no block is live.
constexpr uint32_t kArenaSize = 0x10000;
struct Arena {
  std::vector<uint8_t> host;
  uint32_t va = 0, cur = 0;
  std::map<uint32_t, uint32_t> live;  // va -> size
};
std::map<const Ctx *, Arena> &arenas() {
  static std::map<const Ctx *, Arena> m;
  return m;
}
Arena &arena(Ctx &ctx) {
  Arena &a = arenas()[&ctx];
  if (a.va != 0) {
    try {
      if (ctx.mem.at(a.va, kArenaSize) == a.host.data()) return a;
    } catch (const Fault &) {
    }
  }
  a.host.assign(kArenaSize, 0);
  a.cur = 0;
  a.live.clear();
  uint32_t base = (ctx.mem.data_base() + static_cast<uint32_t>(sizeof(GameState)) + 0x02800000u) & ~0xFFFFu;
  for (int tries = 0; tries < 64; ++tries, base += 0x00100000u)
    if (ctx.mem.map(base, a.host.data(), kArenaSize, false)) {
      a.va = base;
      return a;
    }
  throw MissingResource{"EditBox arena (no free address range)"};
}
uint32_t eb_malloc(Ctx &ctx, uint32_t n) {
  Arena &a = arena(ctx);
  if (a.live.empty()) a.cur = 0;
  const uint32_t size = (n + 15u) & ~15u;
  if (n > kArenaSize || a.cur + size > kArenaSize) throw MissingResource{"EditBox arena overflow"};
  const uint32_t p = a.va + a.cur;
  a.cur += size;
  a.live[p] = size;
  return p;
}
void eb_free(Ctx &ctx, uint32_t p) { arena(ctx).live.erase(p); }

uint32_t vstrlen_at(const Memory &m, uint32_t p) {
  uint32_t n = 0;
  while (m.load<uint8_t>(p + n) != 0) ++n;
  return n;
}
// the static CRT's strncpy: at most n bytes, the rest of the n padded with NULs
void vstrncpy(const Memory &m, uint32_t dst, uint32_t src, uint32_t n) {
  uint32_t i = 0;
  for (; i < n; ++i) {
    const uint8_t c = m.load<uint8_t>(src + i);
    m.store<uint8_t>(dst + i, c);
    if (c == 0) break;
  }
  for (; i < n; ++i) m.store<uint8_t>(dst + i, 0);
}
// a forward byte move of n bytes (the CRT's memmove and the inline rep movsd / movsb, which agree for these overlaps)
void vmove(const Memory &m, uint32_t dst, uint32_t src, uint32_t n) {
  if (dst > src)
    for (uint32_t i = n; i-- > 0;) m.store<uint8_t>(dst + i, m.load<uint8_t>(src + i));
  else
    for (uint32_t i = 0; i < n; ++i) m.store<uint8_t>(dst + i, m.load<uint8_t>(src + i));
}

constexpr uint32_t kText = 0x574, kCap = 0x578, kCaret = 0x5a4, kBlink = 0x5a8;

// resizeBuffer(n): the same capacity returns; otherwise a block of n + 1 bytes (NUL at [n] and [0]), the old text copied in
// with strncpy(new, old, n) and the old block freed, the new block and capacity stored.
void editbox_resize_buffer(Ctx &ctx, uint32_t eb, uint32_t n) {
  const Memory &m = ctx.mem;
  if (n == m.load<uint32_t>(eb + kCap)) return;
  const uint32_t p = eb_malloc(ctx, n + 1);
  m.store<uint8_t>(p + n, 0);
  m.store<uint8_t>(p, 0);
  const uint32_t old = m.load<uint32_t>(eb + kText);
  if (old != 0) {
    if (SG_NEGCTL(g_negctl_eb_resize) != 1) vstrncpy(m, p, old, n);
    eb_free(ctx, old);
  }
  m.store<uint32_t>(eb + kText, p);
  m.store<uint32_t>(eb + kCap, n);
}

// setText(text): no buffer does nothing; a NULL text empties it, otherwise strncpy of at most the capacity; the caret goes
// to the length (then the layout slot).
void editbox_set_text(Ctx &ctx, uint32_t eb, uint32_t text) {
  const Memory &m = ctx.mem;
  const uint32_t buf = m.load<uint32_t>(eb + kText);
  if (buf == 0) return;
  if (text == 0) m.store<uint8_t>(buf, 0);
  else vstrncpy(m, buf, text, m.load<uint32_t>(eb + kCap));
  if (SG_NEGCTL(g_negctl_eb_settext) != 1) m.store<uint32_t>(eb + kCaret, vstrlen_at(m, buf));
}

// insertChar(ch, a2): a character 0..0x1f returns 0 (a2 is never read: its arm needs ch == 0, which returned already); a
// text as long as the capacity (an equality test) beeps and returns 1; otherwise the tail from the caret moves up one byte
// (at the end only the byte after the NUL is cleared), the low byte of ch goes in at the caret, the caret advances, 1.
int32_t editbox_insert_char(Ctx &ctx, uint32_t eb, int32_t ch) {
  const Memory &m = ctx.mem;
  if (ch >= 0 && ch < 0x20) return 0;
  const uint32_t buf = m.load<uint32_t>(eb + kText);
  if (vstrlen_at(m, buf) == m.load<uint32_t>(eb + kCap)) return 1;  // MessageBeep(0)
  const uint32_t p = buf + m.load<uint32_t>(eb + kCaret);
  if (m.load<uint8_t>(p) == 0) m.store<uint8_t>(p + 1, 0);
  else vmove(m, p + 1, p, vstrlen_at(m, p) + 1);
  m.store<uint8_t>(m.load<uint32_t>(eb + kText) + m.load<uint32_t>(eb + kCaret), static_cast<uint8_t>(ch));
  if (SG_NEGCTL(g_negctl_eb_insert) != 1) m.store<uint32_t>(eb + kCaret, m.load<uint32_t>(eb + kCaret) + 1);
  return 1;
}

// onKey(key): the low 16 bits select Back 8, End 0x23, Home 0x24, Left 0x25, Up 0x26, Right 0x27, Down 0x28, Delete 0x2e,
// each returning 1 (a refused move beeps); any other key returns 0. Up / Down place the caret through the line wrap
// (wrapToLines, charPosToPixel, pixelToCharPos over the font's widths), which the port does not have (U-1190).
int32_t editbox_on_key(Ctx &ctx, uint32_t eb, int32_t key) {
  const Memory &m = ctx.mem;
  const int neg = SG_NEGCTL(g_negctl_eb_key);
  const uint32_t buf = m.load<uint32_t>(eb + kText);
  const uint32_t caret = m.load<uint32_t>(eb + kCaret);
  switch (static_cast<uint32_t>(key) & 0xffffu) {
    case 0x08:  // Back: the text from the caret on (its NUL included) one byte down, caret - 1
      if (caret == 0) return 1;
      if (neg == 1) vmove(m, buf + caret, buf + caret + 1, vstrlen_at(m, buf + caret + 1) + 1);
      else vmove(m, buf + caret - 1, buf + caret, vstrlen_at(m, buf + caret) + 1);
      m.store<uint32_t>(eb + kCaret, m.load<uint32_t>(eb + kCaret) - 1);
      return 1;
    case 0x23: m.store<uint32_t>(eb + kCaret, vstrlen_at(m, buf)); return 1;  // End
    case 0x24: m.store<uint32_t>(eb + kCaret, neg == 2 ? 1u : 0u); return 1;  // Home
    case 0x25:                                                               // Left
      if (caret != 0) m.store<uint32_t>(eb + kCaret, caret - 1);
      return 1;
    case 0x27:  // Right
      if (m.load<uint8_t>(buf + caret) != 0) m.store<uint32_t>(eb + kCaret, caret + 1);
      return 1;
    case 0x2e:  // Delete: the tail after the caret's byte (its NUL included) one byte down; the caret stays
      if (m.load<uint8_t>(buf + caret) != 0) vmove(m, buf + caret, buf + caret + 1, vstrlen_at(m, buf + caret + 1) + 1);
      return 1;
    case 0x26:
    case 0x28: throw MissingResource{"EditBox::onKey: Up / Down (EditBox::wrapToLines)"};
    default: return 0;
  }
}

// resetFields: text, +0x580, +0x57c, capacity, blink flag and caret cleared; the style words from the EditBox config
// f_cfg_EditBox_4e43f0 (+0, +0x1c, +0x18, +8, +4) and g_editBoxDefaults (+0, +4); eax = the config word +4.
int32_t editbox_reset_fields(Ctx &ctx, uint32_t eb) {
  const Memory &m = ctx.mem;
  for (uint32_t off : {kText, 0x580u, 0x57cu, kCap, kBlink, kCaret}) m.store<uint32_t>(eb + off, 0);
  const uint32_t cfg = GS(f_cfg_EditBox_4e43f0), def = GS(g_editBoxDefaults);
  m.store<uint32_t>(eb + 0x584, m.load<uint32_t>(cfg));
  m.store<uint32_t>(eb + 0x588, m.load<uint32_t>(cfg + 0x1c));
  m.store<uint32_t>(eb + 0x58c, m.load<uint32_t>(cfg + 0x18));
  m.store<uint32_t>(eb + 0x59c, m.load<uint32_t>(cfg + 8));
  m.store<uint32_t>(eb + 0x590, m.load<uint32_t>(def));
  m.store<uint32_t>(eb + 0x594, m.load<uint32_t>(def + 4));
  const uint32_t w = m.load<uint32_t>(cfg + (SG_NEGCTL(g_negctl_eb_reset) == 1 ? 8 : 4));
  m.store<uint32_t>(eb + 0x598, w);
  return i32(w);
}

// releaseState: the text block freed, resetFields; Timer::reset of the caret timer and Window::ctorInit are the window
// tree's (U-1191). The original returns Window::ctorInit's eax; the port returns 0 (no caller in the port reads it).
int32_t editbox_release_state(Ctx &ctx, uint32_t eb) {
  const uint32_t buf = ctx.mem.load<uint32_t>(eb + kText);
  if (buf != 0) eb_free(ctx, buf);
  if (SG_NEGCTL(g_negctl_eb_release) != 1) editbox_reset_fields(ctx, eb);
  return 0;
}

// create(x, y, w, h, parent): releaseState; no parent returns 3; a 0x40-character buffer; Window::create (the window tree,
// taken to succeed: its failures come from Surface allocations); +0x2b4 and +0x5a0 cleared, the caret at the text's length
// (empty); 0.
int32_t editbox_create(Ctx &ctx, uint32_t eb, uint32_t parent) {
  editbox_release_state(ctx, eb);
  if (parent == 0) return 3;
  editbox_resize_buffer(ctx, eb, SG_NEGCTL(g_negctl_eb_create) == 1 ? 0x3f : 0x40);
  const Memory &m = ctx.mem;
  m.store<uint32_t>(eb + 0x2b4, 0);
  m.store<uint32_t>(eb + 0x5a0, 0);
  m.store<uint32_t>(eb + kCaret, vstrlen_at(m, m.load<uint32_t>(eb + kText)));
  return 0;
}
}  // namespace

// ------------------------------------------------------------------------------------------------ showCharacterEditor
namespace {
// the hover tips of controls 8..19 (offsets in f_strpool_4c84e8); control 20's tip is shown for non-members only
constexpr uint32_t kTips[12] = {284, 268, 256, 244, 232, 220, 208, 196, 184, 176, 168, 160};
constexpr uint32_t kTipBio = 148;

// THE BIO ENTRY (control 20, non-members). Drawing first (drawPanelS2, the three pixel reads, Palette::nearest for the
// text colour: dropped); EditBox::create on g_characterEditorEditBox (x 0x1e0, y 0x70, w 0x12c, h 0xc8, parent the root
// window), the caret blink on, a 500-character buffer, setText(bio), the style words +0x584 (the nearest-white colour:
// 0, the drawing's), +0x590 1, +0x594 2, +0x598 0, +0x59c 4, Window::show; the button word and gDirtyFlagB cleared. Then
// the loop: showScreen, redrawAll, leaveModal483d80 (the screen's), pumpMessages, Window::refreshFull (drawing), until a
// press (the button word) or gDirtyFlagB == 0x0d (rootWin_onKey of an Enter). The loop reads neither the clock nor the
// cursor. The bio becomes "" + the EditBox's text (the original's strcat into the NUL'd bio), and releaseState ends it.
//
// KEY ROUTING. In the original the root window's focus ring holds the EditBox from EditBox::create on (measured, cov_r_bio
// probe4: Window::onKeyChar -> Window::key(root) -> Window::key(EditBox) -> EditBox::insertChar; uiKeyEvent -> onCommand ->
// dispatchAccel(root) -> dispatchAccel(EditBox) -> EditBox::onKey), and the root handlers run only for what the EditBox
// returns 0 on. A key it consumed reaches no root handler, so the trace has its MSG record and no HANDLER record; tick.cpp
// parse_trace turns those into RootHandler commands 12 (a WM_KEYDOWN the focus child took: a0 = the virtual key) and 13 (a
// WM_CHAR it took: a0 = the character), which apply_command treats as nothing. Here, while the EditBox has the focus,
// 12 goes to EditBox::onKey and 13 to EditBox::insertChar (with the character's low byte sign-extended, the movsx of
// Window::onKeyChar); a command the EditBox refuses means the recording and the model disagree. The root handlers the
// trace does hold (an Enter's onKey, onChar for keys the EditBox ignores, the press) apply as everywhere.
constexpr uint32_t kFocusKeyDown = 12, kFocusChar = 13;

int32_t bio_command(Ctx &ctx, uint32_t eb, const Command &c) {
  if (c.kind == CmdKind::RootHandler && (c.handler == kFocusKeyDown || c.handler == kFocusChar)) {
    const int32_t r = c.handler == kFocusChar ? editbox_insert_char(ctx, eb, static_cast<int8_t>(c.a0 & 0xff))
                                              : editbox_on_key(ctx, eb, c.a0);
    if (r == 0) throw MissingResource{"showCharacterEditor: a key the recording gives the bio EditBox, which refuses it"};
    return r;
  }
  return apply_command(ctx, c);
}

// Roadmap S3.19: THE BIO ENTRY'S DRAWING, made only with a graphics backend attached (body family editbox, gfx_editbox.cpp):
// the EditBox's window surface (EditBox::create's Window::create at (0x1e0, 0x70), 0x12c x 0xc8, shown by Window::show),
// EditBox::drawContents at the show and after every key the EditBox takes (insertChar and onKey end in its slot +0x120,
// Window::doLayout, which draws it again), Window::refreshFull of the root before the first input and at the end of every pass
// (the window tree composes the EditBox over the root's copy), and the window taken off the tree by releaseState. The original
// stores Palette::nearest(white) at +0x584 and paints with it; the port stores 0 (the palette is not modelled) and hands the
// backend white as a direct RGB555 word (U-1847). Nothing here writes the GameState.
constexpr uint32_t kBioPaint = 0x80000000u | 0x7fffu;

void bio_entry(Ctx &ctx, uint32_t bio) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int neg = SG_NEGCTL(g_negctl_editor);
  const uint32_t eb = GS(g_characterEditorEditBox);
  const bool drawing = g_gfx_attached.load(std::memory_order_relaxed) != 0;
  auto gx = [&](const char *name, std::initializer_list<uint32_t> a) {
    if (drawing) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  };
  const uint32_t root_win = GS(g_rootWindow);
  editbox_create(ctx, eb, GS(g_rootWindow));
  m.store<uint32_t>(eb + kBlink, 1);  // setCaretActive(1): the flag (its Timer::startOnce is the window tree's)
  editbox_resize_buffer(ctx, eb, 0x1f4);
  editbox_set_text(ctx, eb, bio);
  m.store<uint32_t>(eb + 0x584, 0);
  m.store<uint32_t>(eb + 0x590, 1);
  m.store<uint32_t>(eb + 0x594, 2);
  m.store<uint32_t>(eb + 0x598, 0);
  m.store<uint32_t>(eb + 0x59c, 4);
  gx("EditBox.window", {eb, 0x1e0, 0x70, 0x12c, 0xc8});
  gx("EditBox::drawContents", {eb, kBioPaint});
  gx("Window::refreshFull", {root_win, 0});
  gs.g_rootInput[kButton] = 0;
  gs.gDirtyFlagB = 0;
  do {
    if (gs.g_rootInput[kClicked] == 0) gs.g_rootInput[kButton] = 0;
    InputSource *src = input_source(ctx);
    if (!src || src->exhausted()) throw NeedInput{{"showCharacterEditor: the bio EditBox"}};
    std::vector<Command> batch;
    bool edited = false;
    if (src->batch(PumpKind::Blocking, batch))
      for (const Command &c : batch) {
        bio_command(ctx, eb, c);
        if (c.kind == CmdKind::RootHandler && (c.handler == kFocusKeyDown || c.handler == kFocusChar)) edited = true;
      }
    if (edited) gx("EditBox::drawContents", {eb, kBioPaint});
    gx("Window::refreshFull", {root_win, 0});
    gs.g_rootInput[kClicked] = 0;
  } while (gs.g_rootInput[kButton] == 0 && gs.gDirtyFlagB != (neg == 10 ? 0x0a : 0x0d));
  if (neg != 11) {
    m.store<uint8_t>(bio, 0);
    cat(ctx, bio, m.load<uint32_t>(eb + kText));
  }
  editbox_release_state(ctx, eb);
  gx("EditBox.close", {eb});
}
}  // namespace

// Roadmap S2.10: the editor's drawing (c3fx.cpp ShowCharacterEditor_re's callees in its order, with its arguments), made only
// with a graphics backend attached (gfx_drop; bodies in gfx.cpp, gfx_prim_text.cpp, gfx_prim_fill.cpp, gfx_dialogs.cpp,
// gfx_chareditor.cpp and, for the sprite cuts, the scaled draws and the golfer palette, the in-game families). The texts are the
// ones the port builds in g_textBuffer anyway; a draw never writes the GameState.
struct EdDraw {
  Ctx &ctx;
  const bool on;
  const uint32_t root, win;
  explicit EdDraw(Ctx &c)
      : ctx(c),
        on(g_gfx_attached.load(std::memory_order_relaxed) != 0),
        root(c.gs().f_stateptr_4c1570[0]),
        win(state_va(c, &GameState::g_rootWindow)) {}
  void gx(const char *name, std::initializer_list<uint32_t> a) const {
    if (on) gfx_drop(ctx, name, a.begin(), static_cast<int>(a.size()));
  }
  void font(uint32_t f) const { gx("setDrawTarget", {root, f, 0, 0, 0}); }
  // Surface_drawScaled(sprite, root, x, y, 1, 1, 1, 0)
  void scaled(uint32_t s, int32_t x, int32_t y) const { gx("Surface_drawScaled", {s, root, u32(x), u32(y), 1, 1, 1, 0}); }
  void image(uint32_t s, int32_t x, int32_t y, uint32_t pal = 0) const { gx("Image_draw", {s, root, u32(x), u32(y), pal}); }
  void centred(uint32_t t, int32_t x, int32_t y, uint32_t c) const { gx("drawCenteredText", {t, u32(x), u32(y), c}); }
  void centred2(uint32_t t, int32_t x, int32_t y, uint32_t c) const { gx("drawCenteredText2", {t, u32(x), u32(y), c}); }
  void text(uint32_t t, int32_t x, int32_t y, uint32_t c) const { gx("drawTextWithColor", {t, u32(x), u32(y), c}); }
  void fill(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) const {
    gx("fillWindowRect", {win, u32(x), u32(y), u32(w), u32(h), c});
  }
  // a control's spot (f_tab_point16_4c7b90: x, y words per control)
  int32_t spot(int32_t k, int32_t half) const {
    return ctx.mem.load<int16_t>(GS(f_tab_point16_4c7b90) + 4u * u32(k) + 2u * u32(half));
  }
};

// Entry: the tip state word cleared, the edited record copied to golfer type record 83 (Cancel restores it), the preview
// golfer 0x99 set up (type = character, class = Random::range(3), traits = the record's signed demographics byte, kind 0,
// preferences = the golfer's). Every pass: the cursor read, the control under it (nearestCharControl; control 15 only for a
// female character), the name-area hover line; the figure's sprite set (cellAt twice, each stored in g_ui_5aa6b8[3] and
// queued with addSprite) and composeGolferSprite(0x99); the texts drawn (golfer name, title, status lines, the five
// personality and three preference labels, the member's skills or the type card and bio, the 20 comment lines with
// buildGolferComment, which also sets g_commentTone); the hover tip after 10 frames on one control (g_ui_5aa554 counts,
// g_panelState[1] keeps the control). Then the pump; a press acts on the control: 0..2 toggle the preview preferences,
// 3..7 the personality bits, 8 loads a character file (listMatchingFiles, scrollTextWindow, loadThemeFile), 9 saves one
// (promptAndSaveFile, showPopup), 10 the hair style from a counter that starts at 0 (the original's is an uninitialised
// frame slot), 11 toggles custom bit 3, 12 the body / outfit picker (pickCharEditor) and its preset's looks, 13..16 step
// looks 0x24 / 0x25 / 0x28 / 0x27, 17 the gender, 18 Cancel (restores the record, the editor stays open; P13B: with
// showCharacterEditor.cancelStaysOpen = 0 it puts back every edit and leaves), 19 Exit (the
// preview preferences go to the golfer and to custom bits 0..2), 20 the bio (not ported); a press on no control edits the
// name / title (textInputBox), steps the demographics bit fields, edits a comment line (textInputBox), and steps a member's
// skill byte (0..10, the refusal sound at the ends). Each press is followed by the release wait. After Exit: for a member
// with a type, the pro record of its name index gets the looks.
int32_t show_character_editor(Ctx &ctx, int32_t character, int32_t golfer) {
  GameState &gs = ctx.gs();
  cm::drop_stale_esc(ctx, g_defect_esc_char_editor);  // P9A item 2
  const Memory &m = ctx.mem;
  const int neg = SG_NEGCTL(g_negctl_editor);
  const uint32_t tb = GS(g_textBuffer);
  const uint32_t nb = GS(g_panelNumberText);
  const uint32_t R1 = type_rec(ctx, character);
  const uint32_t Gr = rec(ctx, golfer);
  const uint32_t P = rec(ctx, kPreview);
  const uint32_t backup = type_rec(ctx, kBackupType);
  auto B = [&](uint32_t va) { return m.load<uint8_t>(va); };
  auto SB = [&](uint32_t va, uint32_t v) { m.store<uint8_t>(va, static_cast<uint8_t>(v)); };
  auto copy = [&](uint32_t dst, uint32_t src, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) m.store<uint8_t>(dst + i, m.load<uint8_t>(src + i));
  };
  // the preview golfer's type record (re-read at every use: the original reloads the type word each time)
  auto preview_rec = [&] { return type_rec(ctx, m.load<int16_t>(P + 0xb6)); };

  const bool member = (B(Gr + 0x18) & 0xe0) == 0x20;
  int32_t done = 0;
  gs.g_ui_5aa55c = 0;
  copy(backup, R1, 0x230);
  // P13B item 14 (showCharacterEditor.cancelStaysOpen = 0): what else the editor writes in place before Exit, kept on the host
  // for Cancel: the type's phrase lines (comment lines, loadThemeFile), the golfer's skill bytes (skill bars; loadThemeFile's
  // 16-byte record), the player's skills and the portrait slot counters (loadThemeFile)
  const bool cancel_closes = g_defect_editor_cancel_stays == 0;
  const uint32_t phrases = GS(g_golferPhrases) + u32(mul32(character, 0x4e2));
  std::vector<uint8_t> keep_phrases, keep_skills;
  int32_t keep_slots[2] = {};
  int8_t keep_player[16] = {};
  if (cancel_closes) {
    for (uint32_t i = 0; i < 0x4e2; ++i) keep_phrases.push_back(B(phrases + i));
    for (uint32_t i = 0; i < 16; ++i) keep_skills.push_back(B(Gr + 0xf0 + i));
    std::memcpy(keep_slots, gs.g_loopCounter59b76c, sizeof keep_slots);
    std::memcpy(keep_player, gs.g_playerSkills, sizeof keep_player);
  }
  cm::call_optional(ctx, "startSoundSlot", {0x2d});
  cm::call_optional(ctx, "playSound", {(B(R1 + 0x21) & 0x80) ? 0x7du : 0x80u, 0x64, 0, 0, 0});
  m.store<int16_t>(P + 0xb6, static_cast<int16_t>(character));
  SB(P + 0xba, u32(cm::rng_range(ctx, 3)));
  m.store<int16_t>(P + 0xb8, static_cast<int16_t>(m.load<int8_t>(R1 + 0x21)));
  SB(P + 0x18, 0);
  SB(P + 0x19, B(Gr + 0x19));
  gs.g_rootInput[kButton] = 0;
  Scratch xy(ctx, 8);  // x and y: frame slots only View_toLocal writes (zeroed-stack convention before the first read)
  const EdDraw d(ctx);
  // the frame Image and the background of the character's gender (Image_ctor, loadImage(path, 0, 0, 0x100, 2))
  d.gx("showCharacterEditor.setup", {pool(ctx, (B(R1 + 0x21) & 0x80) ? 0x218u : 0x1fcu)});
  int32_t style = 0;   // the hair-style counter: an uninitialised frame slot in the original (zeroed-stack convention)

  for (;;) {
    d.gx("showCharacterEditor.background", {d.root});
    view_to_local(ctx, xy.va, xy.va + 4, "showCharacterEditor");
    const int32_t x = m.load<int32_t>(xy.va), y = m.load<int32_t>(xy.va + 4);
    int32_t ctl = nearest_char_control(ctx, x, y);
    if (ctl == 0xf && (B(R1 + 0x21) & 0x80) == 0) ctl = -1;
    // the name area's hover line (it only picks text colours) or the control's hover sprite
    int32_t hot = -1;
    if (d.on) {
      if (ctl == -1 || ctl == 0x14) {
        if (abs32(sub32(x, 0x9a)) < 0x50 && abs32(sub32(y, 0x40)) < 0x28) {
          const int32_t q = div32(sub32(y, 0x18), 0x14);
          if (u32(q) <= 4) hot = q;
        }
      } else {
        d.scaled(GS(g_sprites59df20) + 0x58u * u32(ctl), d.spot(ctl, 0), d.spot(ctl, 1));
      }
    }
    // ---- the figure
    if (d.on) {
      const uint32_t R = preview_rec();
      if (B(R + 0x2c) & 8) d.image(GS(g_sprite5787a0), 0x160, 0x74);
    }
    call_peer(ctx, "composeGolferSprite", {u32(kPreview)});
    if (d.on) {
      // the golfer's palette layers into g_images[0], the body sheet drawn with it, then the head (three expressions a row)
      d.gx("composeGolferSprite", {u32(kPreview)});
      const uint32_t R = preview_rec();
      const int32_t hs = (B(R + 0x23) >> 4) & 3, body = m.load<int8_t>(R + 0x22);
      const int32_t sheet = (B(R + 0x2c) & 8) ? hs + 4 : hs;
      const int32_t g = type_bit7_clear(ctx, kPreview);
      d.image(GS(g_surfaces5439e0) + 0x2cu * u32(add32(sheet, mul32(g, 8))), 0x163, 0x74, GS(g_images));
      const int32_t tf = i32(call_peer(ctx, "thoughtFlag", {u32(kPreview)}).eax);
      if (body < 0x14)
        d.scaled(GS(g_sprites59e7c0) + 0x2cu * u32(add32(mul32(add32(body, mul32(g, 0x14)), 3), tf)), 0x150, 0x14);
      else
        d.scaled(GS(g_sprites5628e8) + 0x2cu * u32(add32(mul32(add32(body, mul32(g, 0x48)), 3), tf)), 0x13c, 0x14);
      if (ctx.gs().g_graphicsDevice != 0) d.gx("Display::setScale", {1, 1, 1});
      d.image(GS(g_sprites59df20) + 0x6e0, 0x12b, 0xdc);
    }
    int32_t set;
    if (type_bit7_clear(ctx, kPreview) == 0) {
      const uint32_t R = preview_rec();
      set = 7;
      const uint8_t b21 = B(R + 0x21);
      if (b21 & 1) set = 6;
      if (B(R + 0x20) & 8) set = 8;
      if (b21 & 4) set = 5;
      if (m.load<uint32_t>(R + 0x2c) != 0) set = ((B(R + 0x23) >> 4) & 3) + 5;
    } else {
      const uint8_t fl = B(P + 0x19);
      set = (fl & 4) ? ((((~fl) & 0xff) & 2) | 4) >> 1 : 1;
      const uint32_t R = preview_rec();
      if (m.load<uint32_t>(R + 0x2c) != 0) set = (B(R + 0x23) >> 4) & 3;
      if (B(P + 0x18) == 0x20) set = m.load<int8_t>(GS(g_golfPros) + 0x38 * u32(B(P + 0xbb)) + 0x20);
    }
    int32_t cell = i32(call_peer(ctx, "cellAt", {u32(set), 5, 0}).eax);
    gs.g_ui_5aa6b8[3] = cell;
    call_peer(ctx, "addSprite", {0x13b, 0xef, 0xef, 0x1e, 0x28, u32(cell), 4, 0x199});
    cell = i32(call_peer(ctx, "cellAt", {u32(set), 5, 4}).eax);
    gs.g_ui_5aa6b8[3] = cell;
    call_peer(ctx, "addSprite", {0x14f, 0xef, 0xef, 0x1e, 0x28, u32(cell), 4, 0x199});
    // ---- the name lines
    d.font(GS(g_font519928));
    m.store<uint8_t>(tb, 0);
    call_peer(ctx, "buildGolferName", {u32(kPreview), 0});
    if (B(preview_rec() + 0x2c) & 8) cat(ctx, tb, pool(ctx, 500));
    d.centred2(tb, 0x9a, 0x18, hot != 0 ? 0x80000848u : 0x80007b20u);
    cpy(ctx, tb, GS(f_strpool_4c59e0) + 4);
    cat(ctx, tb, preview_rec());
    cat(ctx, tb, GS(f_strpool_4c59e0));
    d.centred2(tb, 0x9a, 0x30, hot != 1 ? 0x80000848u : 0x80007fffu);
    d.font(GS(g_font51b360));
    {
      const uint8_t st = B(P + 0xb8);
      m.store<uint8_t>(tb, 0);
      if (st & 8) cpy(ctx, tb, pool(ctx, 492));
      if (st & 0x10) cpy(ctx, tb, pool(ctx, 484));
      if (st & 0x20) cpy(ctx, tb, pool(ctx, 472));
      if (st & 0x40) cpy(ctx, tb, pool(ctx, 464));
    }
    if (hot == 2) d.centred2(tb, 0x9a, 0x45, 0x80007fffu);
    else d.centred(tb, 0x9a, 0x45, 0x80000848u);
    {
      const uint8_t st = B(P + 0xb8);
      m.store<uint8_t>(tb, 0);
      if (st & 1) cpy(ctx, tb, pool(ctx, 456));
      if (st & 2) cpy(ctx, tb, pool(ctx, 444));
      if (st & 4) cpy(ctx, tb, pool(ctx, 436));
    }
    if (hot == 3) d.centred2(tb, 0x9a, 0x5a, 0x80007fffu);
    else d.centred(tb, 0x9a, 0x5a, 0x80000848u);
    // ---- the five personality labels and the three preference labels (first letter raised by 0xe0)
    d.font(GS(g_font51b360));
    const int32_t traits = m.load<int8_t>(preview_rec() + 0x20);
    for (uint32_t k = 0; k < 5; ++k) {
      const bool bit = ((1 << k) & traits) != 0;
      if (bit) d.scaled(GS(g_sprites59df20) + 0x134 + 0x58 * k, d.spot(i32(3 + k), 0), d.spot(i32(3 + k), 1));
      cpy(ctx, tb, m.load<uint32_t>(GS(f_ptrtab_4c2858) + 4 * (3 + k)));
      d.centred(tb, 0x5d, i32(0x7a + 0x1c * k), bit ? 0x80000848u : 0x80004210u);
    }
    d.font(GS(g_font519928));
    for (uint32_t k = 0; k < 3; ++k) {
      const bool bit = ((1u << k) & B(P + 0x19)) != 0;
      if (bit) d.scaled(GS(g_sprites59df20) + 0x2c + 0x58 * k, d.spot(i32(k), 0), d.spot(i32(k), 1));
      cpy(ctx, tb, m.load<uint32_t>(GS(f_ptrtab_4c2858) + 4 * k));
      SB(tb, B(tb) + 0xe0);
      d.centred(tb, 0xd4, i32(0x8e + 0x23 * k), bit ? 0x80000848u : 0x80004210u);
    }
    if (member) {
      // ---- the member's ten skills (the player's skills for character 0)
      d.centred(pool(ctx, 0x1a0), 0x249, 0x1e, 0x80000000u);
      d.font(GS(g_font51b360));
      for (uint32_t k = 0; k < 10; ++k) {
        const uint32_t skill = character != 0 ? B(Gr + 0xf0 + k) : B(GS(g_playerSkills) + k);
        const int32_t yy = i32(0x3d + 0x10 * k);
        d.fill(0x1f2, yy - 3, 0x2a, 0xe, 0x80000000u);
        d.fill(0x1f3, yy - 2, 0x28, 0xc, skill != 0 ? 0x800023e8u : 0x80007d08u);
        if (skill != 0) {
          cpy(ctx, tb, GS(f_strpool_4c5340));
          cat(ctx, tb, itoa10(ctx, i32(skill * 10), nb));
          cat(ctx, tb, GS(f_strpool_4c60c4));
          d.text(tb, 0x1f5, yy, 0x80000000u);
        }
        d.text(m.load<uint32_t>(GS(f_ptrtab_4c2c3c) + 4 * k), 0x222, yy, skill != 0 ? 0x80000000u : 0x80004210u);
      }
    } else {
      // ---- the character's type card and bio (drawMessageBuffer puts back every NUL it writes)
      const uint32_t T = GS(g_golferTypeState) + u32(mul32(character, 0x2c));
      if (d.on) {
        int32_t kind = B(T + 2) & 7;
        if (B(T + 0x29) == 0xff) kind = -1;
        else if (kind > 3) d.fill(0x1f4, 0x1c, 0xa0, 0x11, 0x80000210u);
        switch (kind + 1) {
          case 0: d.centred(pool(ctx, 0x194), 0x244, 0x1e, 0x80006000u); break;
          case 2: d.centred(GS(f_strpool_4c3ac0), 0x244, 0x1e, 0x80006318u); break;
          case 3: d.centred(pool(ctx, 0x18c), 0x244, 0x1e, 0x80004210u); break;
          case 4: d.centred(pool(ctx, 0x17c), 0x244, 0x1e, 0x80002108u); break;
          case 5: d.centred(pool(ctx, 0x170), 0x244, 0x1e, 0x80007ff0u); break;
          case 6: d.centred2(pool(ctx, 0x160), 0x244, 0x1e, 0x80007fffu); break;
          default: break;
        }
      }
      d.font(GS(g_font51b360));
      cpy(ctx, tb, pool(ctx, 340));
      cat(ctx, tb, B(T) != 0 ? itoa10(ctx, B(T), nb) : GS(f_strpool_4c52bc));
      d.centred(tb, 0x244, 0x32, 0x80002108u);
      cpy(ctx, tb, pool(ctx, 328));
      {
        const int32_t v = m.load<int8_t>(T + 1);
        cat(ctx, tb, v > 0 ? itoa10(ctx, v, nb) : GS(f_strpool_4c52bc));
      }
      d.centred(tb, 0x244, 0x40, 0x80002108u);
      cpy(ctx, tb, pool(ctx, 312));
      {
        const int32_t v = m.load<int16_t>(T + 0x2a);
        cat(ctx, tb, v != 0 ? itoa10(ctx, v, nb) : pool(ctx, 304));
      }
      d.centred(tb, 0x244, 0x4e, 0x80002108u);
      d.centred(pool(ctx, 0x12c), 0x244, 0x62, ctl != 0x14 ? 0x8000211fu : 0x80007b20u);
      cpy(ctx, tb, R1 + 0x30);
      d.gx("drawMessageBuffer", {0x1e, 0x1e4, 0x72, 0x80000018u});
    }
    // ---- the 20 comment lines: buildGolferComment's text is overwritten by the label at once; an empty line shows the
    // comment of its kind in the grey tone
    {
      uint32_t kinds = GS(f_tab_bytes_4c2d10);
      uint32_t labels = GS(f_ptrtab_4c2ca8) + 24;
      uint32_t line = GS(g_golferPhrases) + u32(mul32(character, 0x4e2));
      d.font(GS(g_font51b360));
      int32_t yy = 0x10f;
      for (;;) {
        int32_t kind = m.load<int8_t>(kinds);
        m.store<int16_t>(P + 0xae, 0);
        m.store<uint8_t>(tb, 0);
        peer(ctx, "buildGolferComment",
             {u32(kind), m.load<uint32_t>(GS(g_thoughtArgByKind) + 4 * u32(kind)) & 0x3fffu, 0, u32(kPreview)});
        cpy(ctx, tb, m.load<uint32_t>(labels));
        cat(ctx, tb, GS(f_strpool_4c5d20) + 148);
        d.text(tb, 0x34, yy, gs.g_commentTone);
        gs.g_commentTone = 0x80000848u;
        cpy(ctx, tb, line);
        if (B(tb) == 0) {
          kind = m.load<int8_t>(kinds);
          peer(ctx, "buildGolferComment",
               {u32(kind), m.load<uint32_t>(GS(g_thoughtArgByKind) + 4 * u32(kind)) & 0x3fffu, 0, u32(kPreview)});
          gs.g_commentTone = 0x80004210u;
        }
        d.text(tb, 0x19c, yy, gs.g_commentTone);
        yy += 0x10;
        line += 0x32;
        labels += 4;
        const int8_t next = m.load<int8_t>(kinds + 1);
        ++kinds;
        if (next == -1) break;
      }
    }
    // ---- the hover tip
    {
      const uint32_t lastva = GS(g_panelState) + 4;
      const int32_t last = m.load<int32_t>(lastva);
      m.store<int16_t>(P + 0xb6, static_cast<int16_t>(character));
      if (ctl == last && ctl != -1) {
        const int32_t n = add32(gs.g_ui_5aa554, 1);
        gs.g_ui_5aa554 = n;
        if (n > (neg == 6 ? 9 : 10)) {
          m.store<uint8_t>(tb, 0);
          const uint32_t t = u32(ctl - 8);
          bool draw = true;
          if (t < 12) cpy(ctx, tb, pool(ctx, kTips[t]));
          else if (t == 12 && !member) cpy(ctx, tb, pool(ctx, kTipBio));
          else draw = false;
          if (draw && t <= 12 && B(tb) != 0) d.gx("drawPanelBackdrop", {u32(x), u32(y)});
        }
      } else {
        gs.g_ui_5aa554 = 0;
        m.store<int32_t>(lastva, ctl);
      }
    }
    d.gx("Window::refreshFull", {d.win, 0});
    // P13B item 13: the original passes every 100 ms with no input (waitTicks(5, 0) before and after pumpMessages, 50 ms each,
    // showCharacterEditor+0x11e2 / +0x11f0), so a playing host runs the passes at that period (the hover tip after 10 passes,
    // about 1.1 s) instead of its 200 ms idle passes (P5A's courseActionMenu hint)
    if (InputSource *s = input_source(ctx)) s->timed_pass(100);
    pump_pass(ctx, "showCharacterEditor");
    int32_t btn = gs.g_rootInput[kButton];
    if (cm::esc_pressed(ctx, g_defect_esc_char_editor)) {  // P9A item 2: Esc = a press on Exit (case 19)
      ctl = 19;
      btn = 1;
    }
    if (btn == 0) continue;

    // ---- a press
    if (u32(ctl) <= 0x14) {
      switch (ctl) {
        case 0: SB(P + 0x19, B(P + 0x19) ^ 1); break;
        case 1: SB(P + 0x19, B(P + 0x19) ^ 2); break;
        case 2: SB(P + 0x19, B(P + 0x19) ^ 4); break;
        case 3: SB(R1 + 0x20, B(R1 + 0x20) ^ 1); break;
        case 4: SB(R1 + 0x20, B(R1 + 0x20) ^ 2); break;
        case 5: SB(R1 + 0x20, B(R1 + 0x20) ^ 4); break;
        case 6: SB(R1 + 0x20, B(R1 + 0x20) ^ 8); break;
        case 7: SB(R1 + 0x20, B(R1 + 0x20) ^ 0x10); break;
        case 8: {  // load a character file of the theme
          cpy(ctx, tb, pool(ctx, 0));
          cat(ctx, tb, GS(g_theme));
          cat(ctx, tb, pool(ctx, member ? 140 : 132));
          int32_t n = i32(peer(ctx, "listMatchingFiles", {tb, GS(g_str4e9a84), GS(g_str4e9a84)}).eax);
          if (n == 0) break;
          cpy(ctx, tb, pool(ctx, 116));
          if (n > 0) {
            uint32_t name = GS(g_fileNames);
            do {
              cat(ctx, tb, name);
              name += 0x64;
              cat(ctx, tb, GS(f_strpool_4c7c30));
            } while (--n != 0);
          }
          const int32_t pick = i32(call_peer(ctx, "scrollTextWindow", {tb, 0x64, 0x14}).eax);
          if (pick == -1) break;
          peer(ctx, "loadThemeFile",
               {GS(g_fileNames) + u32(mul32(pick, 100)), u32(character), member ? u32(golfer) : 0xffffffffu});
          break;
        }
        case 9: {  // save the character
          if (peer(ctx, "promptAndSaveFile", {u32(character), member ? u32(golfer) : 0xffffffffu, 0}).eax == 0) break;
          cpy(ctx, tb, pool(ctx, 88));
          cat(ctx, tb, GS(g_theme));
          cat(ctx, tb, GS(f_strpool_4c84e4));
          cat(ctx, tb, R1 + 0x10);
          cat(ctx, tb, pool(ctx, member ? 72 : 80));
          call_peer(ctx, "showPopup", {0x12c, 0x64, 0, 1, 0});
          break;
        }
        case 10: {  // the hair style
          style = btn == 1 ? mod32(add32(style, neg == 4 ? 2 : 1), 4) : mod32(add32(style, 3), 4);
          SB(R1 + 0x23, ((u32(style) & 0xff) << 4) | (B(R1 + 0x23) & 0xf));
          m.store<uint32_t>(R1 + 0x2c, m.load<uint32_t>(R1 + 0x2c) | 0x80);
          break;
        }
        case 11: m.store<uint32_t>(R1 + 0x2c, (m.load<uint32_t>(R1 + 0x2c) ^ 8) | 0x80); break;
        case 12: {  // the body / outfit preset
          const int32_t c = pick_char_editor(ctx, type_bit7_clear(ctx, kPreview));
          if (c == -1) break;
          SB(R1 + 0x22, u32(c));
          if (c >= 0x13) break;
          int32_t idx = c;
          if (type_bit7_clear(ctx, kPreview) == 0) idx = add32(c, 0x14);
          const uint32_t q = GS(f_tab_quotes_4d55e8) + u32(mul32(idx, 0x44));
          SB(R1 + 0x27, B(q));
          SB(R1 + 0x28, B(q + (neg == 7 ? 2 : 3)));
          break;
        }
        case 13: {
          const uint8_t b = B(R1 + 0x24);
          const int32_t v = btn == 1 ? static_cast<int8_t>(b + (neg == 1 ? 2 : 1)) : add32(static_cast<int8_t>(b - 1), 10);
          SB(R1 + 0x24, u32(mod32(v, 10)));
          m.store<uint32_t>(R1 + 0x2c, m.load<uint32_t>(R1 + 0x2c) | 0x80);
          break;
        }
        case 14: {
          const uint8_t b = B(R1 + 0x25);
          const int32_t v = btn == 1 ? static_cast<int8_t>(b + 1) : add32(static_cast<int8_t>(b - 1), 10);
          SB(R1 + 0x25, u32(mod32(v, 10)));
          m.store<uint32_t>(R1 + 0x2c, m.load<uint32_t>(R1 + 0x2c) | 0x80);
          break;
        }
        case 15: {
          const uint8_t b = B(R1 + 0x28);
          const int32_t v = btn == 1 ? static_cast<int8_t>(b + 1) : add32(static_cast<int8_t>(b - 1), 5);
          SB(R1 + 0x28, u32(mod32(v, 5)));
          m.store<uint32_t>(R1 + 0x2c, m.load<uint32_t>(R1 + 0x2c) | 0x80);
          break;
        }
        case 16: {
          const uint8_t b = B(R1 + 0x27);
          const int32_t v = btn == 1 ? static_cast<int8_t>(b + 1) : add32(static_cast<int8_t>(b - 1), 4);
          SB(R1 + 0x27, u32(mod32(v, 4)));
          m.store<uint32_t>(R1 + 0x2c, m.load<uint32_t>(R1 + 0x2c) | 0x80);
          break;
        }
        case 17:  // the gender (the background Image reload is drawing)
          SB(R1 + 0x21, B(R1 + 0x21) ^ 0x80);
          m.store<uint32_t>(R1 + 0x2c, m.load<uint32_t>(R1 + 0x2c) | 0x80);
          d.gx("showCharacterEditor.setup", {pool(ctx, (B(R1 + 0x21) & 0x80) ? 0x218u : 0x1fcu)});
          break;
        case 18:  // Cancel: the entry copy back, the editor stays open
          if (neg != 3) copy(R1, backup, 0x230);
          if (cancel_closes) {  // P13B item 14: every edit put back, then the exit path without Exit's preference writes
            for (uint32_t i = 0; i < 0x4e2; ++i) SB(phrases + i, keep_phrases[i]);
            for (uint32_t i = 0; i < 16; ++i) SB(Gr + 0xf0 + i, keep_skills[i]);
            std::memcpy(gs.g_loopCounter59b76c, keep_slots, sizeof keep_slots);
            std::memcpy(gs.g_playerSkills, keep_player, sizeof keep_player);
            done = 1;
          }
          break;
        case 19:  // Exit
          done = 1;
          if (neg != 2) SB(Gr + 0x19, B(P + 0x19));
          m.store<uint32_t>(R1 + 0x2c, (B(P + 0x19) & 7u) | (m.load<uint32_t>(R1 + 0x2c) & 0xfffffff8u));
          break;
        case 20:  // the bio (non-members): the global EditBox edits the record's bio until a press or Enter
          if (member) break;
          d.gx("drawPanelS2", {0x1d8, 0x68, 0x13c, 0xd8, 1});
          bio_entry(ctx, R1 + 0x30);   // the EditBox's drawing: bio_entry hands it to the editbox family (roadmap S3.19)
          break;
      }
    } else if (ctl == -1) {
      int32_t yy = y;
      if (abs32(sub32(x, 0x9a)) < 0x50 && abs32(sub32(yy, 0x40)) < 0x28) {
        const int32_t q = div32(sub32(yy, 0x18), neg == 9 ? 0x15 : 0x14);
        switch (u32(q) <= 3 ? q : -1) {
          case 0:  // the name
            m.store<uint8_t>(tb, 0);
            call_peer(ctx, "textInputBox", {pool(ctx, 60), 0xc8, 0x20, 0x10});
            if (B(tb) != 0) cpy(ctx, R1 + 0x10, tb);
            yy = y;
            btn = gs.g_rootInput[kButton];
            break;
          case 1:  // the title
            m.store<uint8_t>(tb, 0);
            call_peer(ctx, "textInputBox", {pool(ctx, 32), 0xc8, 0x20, 0x10});
            if (B(tb) != 0) cpy(ctx, R1, tb);
            yy = y;
            btn = gs.g_rootInput[kButton];
            break;
          case 2: {  // demographics bits 3..6: one bit, stepped up (left) or down
            const uint8_t b = B(R1 + 0x21);
            uint32_t v = b & 0x78u;
            if (btn == 1) {
              v = (v & 0x3c) << 1;
              if (v == 0) v = 8;
            } else {
              v = (v >> 1) & 0x78;
              if (v == 0) v = 0x40;
            }
            v |= b & 0x87u;
            SB(R1 + 0x21, v);
            m.store<uint16_t>(P + 0xb8, static_cast<uint16_t>((m.load<uint16_t>(P + 0xb8) & 0xff87u) | v));
            break;
          }
          case 3: {  // demographics bits 0..2
            const uint8_t b = B(R1 + 0x21);
            uint32_t v = b & 7u;
            if (btn == 1) {
              v = (v & 3) << 1;
              if (v == 0) v = 1;
            } else {
              v = (v >> 1) & 7;
              if (v == 0) v = 4;
            }
            v |= b & 0xf8u;
            SB(R1 + 0x21, v);
            m.store<uint16_t>(P + 0xb8, static_cast<uint16_t>((m.load<uint16_t>(P + 0xb8) & 0xfff8u) | v));
            break;
          }
          default: break;
        }
      }
      // a comment line (no bottom bound: a press below the 20 drawn lines edits a later slot)
      if (abs32(sub32(x, 0x242)) < 0xb2 && yy >= 0x10e) {
        const int32_t row = add32(div32(sub32(yy, 0x10e), 0x10), neg == 5 ? 1 : 0);
        const uint32_t line = GS(g_golferPhrases) + u32(mul32(add32(mul32(character, 25), row), 0x32));
        cpy(ctx, tb, line);
        call_peer(ctx, "textInputBox", {pool(ctx, 48), 0x12c, u32(mul32(add32(row, 0xf), 0x10)), 0x30});
        cpy(ctx, line, tb);
        yy = y;
        btn = gs.g_rootInput[kButton];
      }
      // a member's skill bar
      if (member && character != 0 && abs32(sub32(x, 0x208)) < 0x19 && yy >= 0x3e && yy < 0xde) {
        const uint32_t s = Gr + 0xf0 + u32(div32(sub32(yy, 0x40), 0x10));
        if (btn == 1) {
          if (B(s) < 0xa) SB(s, B(s) + 1);
          else cm::call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});
        } else {
          if (B(s) > 0) SB(s, B(s) - 1);
          else cm::call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});
        }
      }
    }

    // ---- the release wait
    do pump_release(ctx);
    while (gs.g_rootInput[kButton] != 0);
    do pump_release(ctx);
    while (gs.g_rootInput[kButton] != 0);
    if (done) break;
  }

  // ---- exit: a member with a type writes the looks into the pro record of its name index (the original's -1 guard
  // compares a zero-extended byte, so it never skips)
  if ((B(Gr + 0x18) & 0xe0) == 0x20 && m.load<int16_t>(Gr + 0xb6) != 0) {
    const uint32_t pro = GS(g_golfPros) + 0x38 * u32(B(Gr + 0xbb)) + 0x20;
    SB(pro + 2, B(R1 + 0x23) & 0xf);
    SB(pro + 1, B(R1 + 0x27));
    SB(pro + 3, B(R1 + (neg == 8 ? 0x25 : 0x24)));
    SB(pro + 4, B(R1 + 0x25));
  }
  cm::call_optional(ctx, "startSoundSlot", {(B(R1 + 0x21) & 0x80) ? 0x7du : 0x80u});
  cm::call_optional(ctx, "playSound", {0x2d, 0x64, 0, 0, 0});
  d.gx("showCharacterEditor.teardown", {});
  return 0;
}

// ------------------------------------------------------------------------------------------------ test entries
namespace {
// a scripted InputSource (tests): a Blocking pump takes every command up to and including the first press or key
// (QueueInput's rule), except in the release wait, where the oracle's original finds nothing arrived; a cursor
// SEQUENCE, one point per read, the last one repeated
class ScriptInput : public InputSource {
 public:
  ScriptInput(const Ctx *c, std::vector<Command> cmds, std::vector<std::pair<int32_t, int32_t>> cur)
      : ctx_(c), cmds_(std::move(cmds)), cur_(std::move(cur)) {}
  bool batch(PumpKind kind, std::vector<Command> &out) override {
    out.clear();
    if (kind != PumpKind::Blocking) return false;
    const auto it = pump_states().find(ctx_);
    if (it != pump_states().end() && it->second.release) return false;
    while (pos_ < cmds_.size()) {
      out.push_back(cmds_[pos_++]);
      if (is_stop_handler(out.back().handler)) break;
    }
    return !out.empty();
  }
  bool exhausted() const override { return pos_ >= cmds_.size(); }
  bool cursor(int32_t &x, int32_t &y) override {
    if (cur_.empty()) return false;
    const auto &c = cur_[reads_ < cur_.size() ? reads_ : cur_.size() - 1];
    ++reads_;
    x = c.first;
    y = c.second;
    return true;
  }
  size_t consumed() const { return pos_; }
  size_t reads() const { return reads_; }

 private:
  const Ctx *ctx_;
  std::vector<Command> cmds_;
  std::vector<std::pair<int32_t, int32_t>> cur_;
  size_t pos_ = 0, reads_ = 0;
};
std::map<const Ctx *, std::unique_ptr<ScriptInput>> &scripts() {
  static std::map<const Ctx *, std::unique_ptr<ScriptInput>> m;
  return m;
}
// chareditor.setScript(cmds va, n cmds (6 dwords each: handler, a0, a1, async_ctrl, async_shift, t), cursor va, n points)
void a_set_script(Ctx &c, const uint32_t *a, Ret &) {
  std::vector<Command> cmds;
  for (uint32_t k = 0; k < a[1]; ++k) {
    Command cmd;
    const uint32_t b = a[0] + 24 * k;
    cmd.kind = CmdKind::RootHandler;
    cmd.handler = c.mem.load<uint32_t>(b);
    cmd.a0 = c.mem.load<int32_t>(b + 4);
    cmd.a1 = c.mem.load<int32_t>(b + 8);
    cmd.async_ctrl = c.mem.load<uint32_t>(b + 12);
    cmd.async_shift = c.mem.load<uint32_t>(b + 16);
    cmd.t = c.mem.load<uint32_t>(b + 20);
    cmds.push_back(cmd);
  }
  std::vector<std::pair<int32_t, int32_t>> cur;
  for (uint32_t k = 0; k < a[3]; ++k) cur.emplace_back(c.mem.load<int32_t>(a[2] + 8 * k), c.mem.load<int32_t>(a[2] + 8 * k + 4));
  auto s = std::make_unique<ScriptInput>(&c, std::move(cmds), std::move(cur));
  set_input_source(c, s.get());
  scripts()[&c] = std::move(s);
}
// chareditor.scriptStats -> consumed commands | cursor reads << 16
void a_script_stats(Ctx &c, const uint32_t *, Ret &r) {
  const auto it = scripts().find(&c);
  r.set_u32(it == scripts().end() ? 0u : static_cast<uint32_t>(it->second->consumed() | (it->second->reads() << 16)));
}
void a_clear_script(Ctx &c, const uint32_t *, Ret &) {
  set_input_source(c, nullptr);
  scripts().erase(&c);
  pump_states().erase(&c);
}
void a_script_peer(Ctx &c, const uint32_t *a, Ret &) {
  PeerScript &ps = peer_scripts()[&c];
  ps.salt[a[0]] = a[1];
  ps.va = a[2];
  ps.cap = a[3];
}
void a_clear_peers(Ctx &c, const uint32_t *, Ret &) { peer_scripts().erase(&c); }
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }

const FuncDef kFuncs[] = {
    {"chareditor.scriptPeer", "default", "void", 4, 0, a_script_peer, &g_negctl_none},
    {"chareditor.clearPeers", "default", "void", 0, 0, a_clear_peers, &g_negctl_none},
    {"chareditor.setScript", "default", "void", 4, 0, a_set_script, &g_negctl_none},
    {"chareditor.scriptStats", "default", "int", 0, 0, a_script_stats, &g_negctl_none},
    {"chareditor.clearScript", "default", "void", 0, 0, a_clear_script, &g_negctl_none},
    // chareditor.peek(src, dst, n): n bytes copied between two addresses of the context's space (tests read the EditBox
    // arena with it)
    {"chareditor.peek", "default", "void", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &) {
       for (uint32_t i = 0; i < a[2]; ++i) c.mem.store<uint8_t>(a[1] + i, c.mem.load<uint8_t>(a[0] + i));
     },
     &g_negctl_none},
    {"showCharacterEditor", "default", "int", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(show_character_editor(c, S(a, 0), S(a, 1))); }, &g_negctl_editor},
    {"pickCharEditor", "default", "int", 1, 0, [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(pick_char_editor(c, S(a, 0))); },
     &g_negctl_pick},
    {"nearestCharControl", "default", "int", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(nearest_char_control(c, S(a, 0), S(a, 1))); }, &g_negctl_nearest},
    {"charEditorZone", "default", "int", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(char_editor_zone(c, S(a, 0), S(a, 1))); }, &g_negctl_zone},
    // the EditBox family (S1.5): thiscall, a[0] = this
    {"EditBox::setText", "thiscall", "void", 2, 0, [](Ctx &c, const uint32_t *a, Ret &) { editbox_set_text(c, a[0], a[1]); },
     &g_negctl_eb_settext},
    {"EditBox::resizeBuffer", "thiscall", "void", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &) { editbox_resize_buffer(c, a[0], a[1]); }, &g_negctl_eb_resize},
    {"EditBox::insertChar", "thiscall", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(editbox_insert_char(c, a[0], S(a, 1))); }, &g_negctl_eb_insert},
    {"EditBox::onKey", "thiscall", "int", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(editbox_on_key(c, a[0], S(a, 1))); }, &g_negctl_eb_key},
    {"EditBox::resetFields", "thiscall", "int", 1, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(editbox_reset_fields(c, a[0])); }, &g_negctl_eb_reset},
    {"EditBox::releaseState", "thiscall", "int", 1, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(editbox_release_state(c, a[0])); }, &g_negctl_eb_release},
    {"EditBox::create", "thiscall", "int", 7, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(editbox_create(c, a[0], a[5])); }, &g_negctl_eb_create},
};
}  // namespace

extern const ModuleDef kModule_chareditor = {"chareditor", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
