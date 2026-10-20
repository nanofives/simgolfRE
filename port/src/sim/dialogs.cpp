// Dialogs (see portsim/sim/dialogs.h). Phase 3.8, stream 38d, written from the specification (the C3 hook sources
// c3em.cpp skillPointDialog, c3fa.cpp showCharacterIntro, c3fr.cpp drawGolferInfoPanel, c3ei.cpp drawThoughtBubble, read
// as reference; every address is in log/p38/38d_notes.md, none here). Drawing, sound and the window tree are dropped; the
// text the functions build in g_textBuffer and the itoa scratch g_infoPanelNumBuf are reproduced, because the peers
// they call read and extend the same buffer.
#include "portsim/sim/dialogs.h"

#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "commands_internal.h"
#include "portsim/arith.h"
#include "portsim/ptr.h"
#include "portsim/sim/commands.h"
#include "portsim/sim/util.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   skillPointDialog     1 = the - arrow ignores the row's entry value (any value above 0 goes down), 2 = the + arrow
//                        sets bit r + 1 of the mask, 3 = OK is accepted one point early (used + 1 >= points), 4 = the
//                        + arrow's band ends at rowY + 0xb
//   showCharacterIntro   1 = an event gives 2 points, 2 = the selected golfer is not cleared (storing the result was an
//                        equivalent mutant: skillPointDialog returns the mask word it keeps up to date)
//   drawGolferInfoPanel  1 = the current hole's mood byte goes to +0x5c + hole, 2 = the tooltip after 5 frames, 3 = the
//                        partner's sprite is not composed, 4 = story button 2's radius 0x1a, 5 = the thoughts start at
//                        slot 8 for mode != 0
//   scroll text          (drawScrollText / runScrollText / scrollTextWindow) 1 = the right edge + 9, 2 = a text without a
//                        final newline keeps every entry, 3 = Esc keeps the selection (a Down past the last entry was blind:
//                        the scripts rarely reach the last entry and then press Enter)
//   showBulletinBoard    1 = the events in reverse date order (g_year from the oldest), 2 = the award gives 4 points
int g_negctl_skill_dialog = 0;
int g_negctl_char_intro = 0;
int g_negctl_info_panel = 0;
int g_negctl_scroll_text = 0;
int g_negctl_bulletin = 0;
int g_negctl_none = 0;

using cm::call_peer;
using cm::cat;
using cm::cpy;
using cm::G;

// g_rootInput slots (int32[16]) the dialog reads
constexpr int kClickX = 2, kClickY = 3, kButton = 4;

// golfer record g (0x100 bytes; g may be out of range: the address wraps like the original's shl 8)
inline uint32_t rec(Ctx &ctx, int32_t g) { return GS(g_golfers) + (u32(g) << 8); }

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
// strcat(g_textBuffer, itoa(v, g_infoPanelNumBuf, 10)): the itoa buffer both functions use
void cat_itoa(Ctx &ctx, int32_t v) { cat(ctx, GS(g_textBuffer), itoa10(ctx, v, GS(g_infoPanelNumBuf))); }

// Test seam (dialogs.scriptPeer): per context, peers whose calls are logged instead of run and answered with
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
// storyText's scripted eax: FNV-1a over the argument dwords from the salt, bit 7; the other scripted peers return 0
uint32_t script_value(uint32_t name, uint32_t salt, std::initializer_list<uint32_t> args) {
  if (name != fnv1a("storyText")) return 0;
  uint32_t v = salt;
  for (uint32_t a : args) v = (v ^ a) * 16777619u;
  return (v >> 7) & 1u;
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

// drawThoughtBubble (g, x, y, mode) without its drawing: composeGolferSprite(g) once, twice with mode 1 (its two sprite
// layers each compose the golfer); its other callees (typeBit7Clear, thoughtFlag) only read
void thought_bubble(Ctx &ctx, int32_t g, int32_t mode) {
  peer(ctx, "composeGolferSprite", {u32(g)});
  if (mode == 1) peer(ctx, "composeGolferSprite", {u32(g)});
}

// the cursor a View_toLocal read returns (the context's InputSource; none = the dialog waits for input it cannot have)
void read_cursor(Ctx &ctx, int32_t &x, int32_t &y, const char *where) {
  InputSource *src = input_source(ctx);
  if (!src || !src->cursor(x, y)) throw NeedInput{{where}};
}

}  // namespace

// ------------------------------------------------------------------------------------------------ skillPointDialog
// One pass per loop iteration: the frame and title (dropped); with xbase >= 0 the golfer's bubble (golfer -1 = the
// preview record 0x99, whose type word is set to 0 and status word to the signed byte +0x21 of golfer type 0 first);
// the cursor read (points > 0); the ten rows (value = the golfer's own skill byte when golfer != -1 and its type word is
// set, else g_playerSkills; the arrows under the cursor give the hover index, + = row, - = row + 10, later tests win; the
// row's value text); the points-left text (points > 0). points == -1 returns -1 here. Then the pump (Blocking) and the
// exit tests: no button and no key starts a new pass. A button within 0x14 of the OK button (xbase + 0x15e, 0x14e)
// returns the mask word when every point is used, otherwise asks showPopup (a zero answer returns the mask word, any
// other a new pass). points == 0 ignores every other click. A button on an arrow: + raises the row while points are left
// and the value is below 10 (bit r of the mask set); - lowers it while it is above 0 and above its value on entry (bit r
// cleared at 0). Every refused arrow click (and a button with no hover while more points are used than given) plays the
// refusal sound (the audio backend's).
int32_t skill_point_dialog(Ctx &ctx, uint32_t title, int32_t mask, int32_t points, int32_t golfer, int32_t xbase) {
  (void)title;  // drawn only
  (void)mask;   // its OR into a frame copy is never read
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int mode = SG_NEGCTL(g_negctl_skill_dialog);
  const uint32_t skills = GS(g_playerSkills);
  const uint32_t tb = GS(g_textBuffer);
  const uint32_t mword = GS(g_skillMaskBlock) + 8;
  int8_t entry[32];
  for (uint32_t i = 0; i < 32; ++i) entry[i] = m.load<int8_t>(skills + i);
  int32_t used = 0, left = points;
  int32_t lx = 0, ly = 0;  // frame slots only the cursor read writes (zeroed-stack convention before it)
  for (;;) {
    if (xbase >= 0) {
      int32_t who = golfer;
      if (golfer == -1) {
        const uint32_t r = rec(ctx, 0x99);
        m.store<int16_t>(r + 0xb6, 0);
        m.store<int16_t>(r + 0xb8, static_cast<int16_t>(m.load<int8_t>(GS(g_golferTypes) + 0x21)));
        who = 0x99;
      }
      thought_bubble(ctx, who, 1);
    }
    int32_t hover = -1;
    if (points > 0) read_cursor(ctx, lx, ly, "skillPointDialog");
    const int32_t colX = add32(xbase, 0x52);
    const int32_t arrowX = add32(xbase, 0x32);
    for (int32_t i = 0; i < 10; ++i) {
      const int32_t rowY = 0x5a + 0x18 * i;
      int32_t value;
      if (golfer != -1 && m.load<int16_t>(rec(ctx, golfer) + 0xb6) != 0)
        value = m.load<uint8_t>(rec(ctx, golfer) + 0xf0 + u32(i));
      else
        value = m.load<uint8_t>(skills + u32(i));
      if (lx < colX) {
        if (lx >= arrowX && ly > rowY && ly <= rowY + (mode == 4 ? 0xb : 0xc)) hover = i;
        if (lx < colX && lx >= arrowX && ly > rowY + 0xc && ly <= rowY + 0x18) hover = i + 10;
      }
      if (value != 0) {
        cpy(ctx, tb, value < 10 ? GS(f_strpool_4c5340) : GS(g_str4e9a84));
        cat(ctx, tb, itoa10(ctx, value * 10, GS(g_infoPanelNumBuf)));
        cat(ctx, tb, GS(f_strpool_4c60c4));
      }
    }
    if (points > 0) {
      cpy(ctx, tb, GS(f_strpool_4d3c3c) + 0x10);
      cat(ctx, tb, itoa10(ctx, left, GS(g_infoPanelNumBuf)));
      cat(ctx, tb, GS(f_strpool_4d3c3c));
    }
    if (points == -1) return -1;
    cm::pump_messages(ctx, PumpKind::Blocking, "skillPointDialog");
    if (gs.g_rootInput[kButton] == 0 && cm::flush_redraw(ctx) == 0) continue;
    const int32_t button = gs.g_rootInput[kButton];
    if (button != 0) {
      const int32_t dx = abs32(sub32(sub32(gs.g_rootInput[kClickX], xbase), 0x15e));
      const int32_t dy = abs32(sub32(gs.g_rootInput[kClickY], 0x14e));
      if (dx < 0x14 && dy < 0x14) {
        if (points <= (mode == 3 ? used + 1 : used)) return m.load<int16_t>(mword);
        cpy(ctx, tb, GS(f_strpool_4d3b5c) + 0x98);   // the Image the original saves the screen in is drawing
        cat(ctx, tb, GS(f_strpool_4d3b5c) + 0x58);
        const int32_t answer = i32(call_peer(ctx, "showPopup", {0x190, 0xc8, 1, 1, 0}).eax);
        if (answer == 0) return m.load<int16_t>(mword);
        continue;
      }
    }
    if (points == 0) continue;
    if (button != 0 && hover != -1) {
      const int32_t r = hover % 10;
      const int8_t v = m.load<int8_t>(skills + u32(r));
      if (hover < 10) {
        if (used < points && v < 10) {
          ++used;
          --left;
          m.store<int8_t>(skills + u32(r), static_cast<int8_t>(v + 1));
          m.store<uint16_t>(mword, static_cast<uint16_t>(m.load<uint16_t>(mword) | (1u << ((mode == 2 ? r + 1 : r) & 31))));
          continue;
        }
      } else if (v > 0 && (mode == 1 || v > entry[r])) {
        --used;
        ++left;
        const int8_t nv = static_cast<int8_t>(v - 1);
        m.store<int8_t>(skills + u32(r), nv);
        if (nv == 0) m.store<uint16_t>(mword, static_cast<uint16_t>(m.load<uint16_t>(mword) & ~(1u << (r & 31))));
        continue;
      }
    } else if (points <= 0 || used <= points) {
      continue;
    }
    cm::call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});
  }
}

// ------------------------------------------------------------------------------------------------ showCharacterIntro
// The help text (two strings into g_textBuffer), points = 10 + 3 per non-zero date of the 22 bulletin event records
// (g_bulletinEvents + 0x28, 0x30 each), no golfer selected (f_w_4c2e00[2] = -1), drawWindow (its effect: none with no
// golfer and mode -2), skillPointDialog(golfer type 0's name, 0, points, -1, 0xc8); the result goes to the mask word
// (int16) and is returned.
int32_t show_character_intro(Ctx &ctx) {
  const Memory &m = ctx.mem;
  const int mode = SG_NEGCTL(g_negctl_char_intro);
  const uint32_t tb = GS(g_textBuffer);
  cpy(ctx, tb, GS(f_strpool_4c4714));
  cat(ctx, tb, GS(f_strpool_4c44ac) + 0x210);
  int32_t points = 10;
  for (uint32_t k = 0; k < 22; ++k)
    if (m.load<int32_t>(GS(g_bulletinEvents) + 0x28 + 0x30 * k) != 0) points += mode == 1 ? 2 : 3;
  if (mode != 2) ctx.gs().f_w_4c2e00[2] = -1;
  cm::draw_window_effect(ctx, -2);
  const int32_t r = skill_point_dialog(ctx, GS(g_golferTypes) + 0x10, 0, points, -1, 0xc8);
  m.store<int16_t>(GS(g_skillMaskBlock) + 8, static_cast<int16_t>(r));
  return r;
}

// ------------------------------------------------------------------------------------------------ drawGolferInfoPanel
namespace {
// the pointer-table string i of f_ptrtab_4c28b8 (hole and lie names: the index is not bounded, as in the original)
uint32_t place_name(Ctx &ctx, int32_t i) { return ctx.mem.load<uint32_t>(GS(f_ptrtab_4c28b8) + 4 * u32(i)); }

// the tooltip text of a hovered button (id 0..5; the table entry 4 has no text, ids above 5 or -1 none): the button
// texts are strings of f_strpool_4d3968 (offsets per page); drawPanelBackdrop, which draws it, is dropped
void tooltip(Ctx &ctx, int32_t id, const uint32_t *offs) {
  const uint32_t tb = GS(g_textBuffer);
  ctx.mem.store<uint8_t>(tb, 0);
  if (u32(id) <= 5 && offs[id] != 0) cpy(ctx, tb, GS(f_strpool_4d3968) + offs[id]);
}
}  // namespace

// The panel's simulation effects, in the original's order: the skill dialog's drawing pass when the record's byte +0x18
// is set (skillPointDialog with points -1: text only); the bubble (composeGolferSprite on g); the text lines (name and
// type, status bits and age, the type's traits, the opinion, likes and kind, the fourteen flags); with a current hole
// (byte +0x21 != -1) the hole line, the record's per-hole mood byte (+0x5d + hole = low byte of +0xa4 while hole < 0x13),
// the scorecard; then the hovered button g_infoPanelTab (from one cursor read) and the hover frame counter
// g_infoPanelHoverFrames, on one of two pages: the STORY page (story set at +0xb0 and g_infoPanelToggle set: the story
// lines through loadStoryStrings / storyText / substituteStoryNames, repeated with a growing limit until they fit, five
// buttons at y 0x11f, button 5 off when both partners are at story stage 4, the partner's bubble) or the THOUGHTS page
// (five buttons at y 0x106, the newest thoughts through buildGolferComment, at most five shown, mode != 0 = all ten
// slots but only the red ones).
void draw_golfer_info_panel(Ctx &ctx, int32_t g, int32_t mode) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int neg = SG_NEGCTL(g_negctl_info_panel);
  const uint32_t R = rec(ctx, g);
  const uint32_t tb = GS(g_textBuffer);
  auto clear = [&] { m.store<uint8_t>(tb, 0); };
  if (m.load<uint8_t>(R + 0x18) != 0) {
    clear();
    call_peer(ctx, "buildGolferName", {u32(g), 0});
    skill_point_dialog(ctx, tb, m.load<int16_t>(R + 0x16), -1, g, -0x32);
  }
  const bool story = !(m.load<int16_t>(R + 0xb0) == -1 || gs.g_infoPanelToggle == 0);
  thought_bubble(ctx, g, -1);
  // name line
  const int32_t type = m.load<int16_t>(R + 0xb6);
  clear();
  call_peer(ctx, "buildGolferName", {u32(g), 0});
  cat(ctx, tb, GS(f_strpool_4c4970) + 4);
  cat(ctx, tb, GS(g_golferTypes) + u32(mul32(0x230, type)));
  cat(ctx, tb, GS(f_strpool_4c59e0));
  // status line (the last of four bits wins) and age
  const uint8_t st = m.load<uint8_t>(R + 0xb8);
  clear();
  if (st & 8) cpy(ctx, tb, GS(f_strpool_4c84e8) + 0x1ec);
  if (st & 0x10) cpy(ctx, tb, GS(f_strpool_4c84e8) + 0x1e4);
  if (st & 0x20) cpy(ctx, tb, GS(f_strpool_4c84e8) + 0x1d8);
  if (st & 0x40) cpy(ctx, tb, GS(f_strpool_4c84e8) + 0x1d0);
  cat(ctx, tb, GS(f_strpool_4d3968) + 0xa4);
  cat_itoa(ctx, i32(call_peer(ctx, "golferAge", {u32(g)}).eax));
  // traits: bits 0..4 of the type's signed byte +0x20, joined by a comma, the last by "and"
  clear();
  {
    int32_t bits = m.load<int8_t>(GS(g_golferTypes) + u32(mul32(0x230, type)) + 0x20);
    int32_t count = 0;
    for (int32_t i = 0; i < 5; ++i) {
      const int32_t b = 1 << i;
      if ((bits & b) == 0) continue;
      bits -= b;
      if (count != 0) cat(ctx, tb, bits != 0 ? GS(f_strpool_4c528c) + 0x2c : GS(f_strpool_4c6980) + 0x174);
      ++count;
      cat(ctx, tb, m.load<uint32_t>(GS(f_ptrtab_4c2858) + 0xc + 4 * u32(i)));
    }
  }
  // opinion
  cpy(ctx, tb, GS(f_strpool_4c4e54));
  call_peer(ctx, "appendOpinion", {u32(m.load<int16_t>(R + 0xa4)), 0});
  cat(ctx, tb, GS(f_strpool_4c4e54));
  // likes (bits 0..2 of byte +0x19) and the kind byte +0xba
  clear();
  {
    const uint32_t likes = m.load<uint8_t>(R + 0x19);
    for (uint32_t i = 0; i < 3; ++i) {
      if ((likes & (1u << i)) == 0) continue;
      if (m.load<uint8_t>(tb) != 0) cat(ctx, tb, GS(f_strpool_4c528c) + 0x2c);
      cat(ctx, tb, m.load<uint32_t>(GS(f_ptrtab_4c2858) + 4 * i));
    }
  }
  if (m.load<uint8_t>(R + 0xba) == 1) cat(ctx, tb, GS(f_strpool_4c5340));
  if (m.load<uint8_t>(R + 0xba) == 2) cat(ctx, tb, GS(f_strpool_4d3968) + 0xa0);
  // the meters: only thoughtBalance is a call (the rest is drawing)
  call_peer(ctx, "thoughtBalance", {u32(g)});
  // the fourteen flags of the word +0xa0
  clear();
  for (uint32_t i = 0; i < 14; ++i)
    cat(ctx, tb, (m.load<int16_t>(R + 0xa0) & (1 << i)) == 0 ? GS(f_strpool_4c4944)
                                                               : m.load<uint32_t>(GS(f_ptrtab_4c2960) + 4 * i));
  const int32_t hole = m.load<int8_t>(R + 0x21);
  if (hole == -1) return;
  cpy(ctx, tb, place_name(ctx, hole));
  cat(ctx, tb, GS(f_strpool_4d3968) + 0x74);
  cat(ctx, tb, place_name(ctx, m.load<int8_t>(R + 0x22)));
  cat(ctx, tb, GS(f_strpool_4c720c) + 0x44);
  if (hole < 0x13) m.store<uint8_t>(R + u32(add32(neg == 1 ? 0x5c : 0x5d, hole)), m.load<uint8_t>(R + 0xa4));
  // the scorecard: hole numbers, the played holes' strokes, the current hole's strokes, the total (text only)
  int32_t total = 0;
  for (int32_t h = 1, x = 0x10a; x < 0x24e; ++h, x += 0x12) {
    clear();
    cat_itoa(ctx, h);
    const int32_t sc = m.load<int8_t>(R + 0x23 + u32(h));
    if (sc != 0) {
      clear();
      cat_itoa(ctx, sc);
      total = add32(total, m.load<int8_t>(R + 0x23 + u32(h)));
    }
    if (hole == h) {
      clear();
      cat_itoa(ctx, m.load<int8_t>(R + 0x22));
      total = add32(total, m.load<int8_t>(R + 0x22));
    }
  }
  clear();
  cat_itoa(ctx, total);
  const int32_t prev = gs.g_infoPanelTab;
  gs.g_infoPanelTab = -1;
  int32_t cx = 0, cy = 0;
  auto near = [&](int32_t x, int32_t y, int32_t radius) { return approx_distance(sub32(x, cx), sub32(y, cy)) < radius; };

  if (story) {
    const int32_t partner = g ^ 1;
    int32_t limit = 0x118;
    for (;;) {
      clear();
      peer(ctx, "loadStoryStrings", {u32(m.load<int16_t>(R + 0xb0)), u32(-1), 0, 0});
      m.store<uint8_t>(tb, 0x27);
      cat(ctx, tb, GS(f_strpool_4d3968) + 0x6c);
      call_peer(ctx, "buildGolferName", {u32(g), 0});
      cat(ctx, tb, GS(f_strpool_4c6980) + 0x174);
      call_peer(ctx, "buildGolferName", {u32(partner), 0});
      int32_t y = 0xb3;
      if (m.load<int8_t>(R + 0x21) >= 1) {
        int32_t k = 1;
        do {
          if (m.load<uint8_t>(R + 0x37 + u32(k)) != 0) {
            const int32_t who = (m.load<uint32_t>(R + 0x10) & (1u << 20)) != 0 ? partner : g;
            const int32_t other = who ^ 1;
            clear();
            if (peer(ctx, "storyText", {u32(m.load<int16_t>(R + 0xb0)), u32(k), u32(other)}).eax != 0) {
              peer(ctx, "substituteStoryNames", {u32(other)});
              y += 9;
            }
            clear();
            if (peer(ctx, "storyText", {u32(m.load<int16_t>(R + 0xb0)), u32(k), u32(who)}).eax != 0) {
              peer(ctx, "substituteStoryNames", {u32(who)});
              y += 10;
            }
          }
          ++k;
        } while (k <= m.load<int8_t>(R + 0x21));
      }
      if (y <= limit) break;
      limit += 0x13;
    }
    read_cursor(ctx, cx, cy, "drawGolferInfoPanel");
    if (near(0x126, 0x11f, 0x19)) gs.g_infoPanelTab = 0;
    if (near(0x167, 0x11f, 0x19)) gs.g_infoPanelTab = 5;
    if (near(0x1a8, 0x11f, neg == 4 ? 0x1a : 0x19)) gs.g_infoPanelTab = 2;
    if (near(0x1e9, 0x11f, 0x19)) gs.g_infoPanelTab = 3;
    if (near(0x221, 0x11f, 0x19)) gs.g_infoPanelTab = 4;
    if (m.load<int16_t>(R + 0xb2) == 4 && m.load<int16_t>(rec(ctx, m.load<int16_t>(R + 0xa2)) + 0xb2) == 4 &&
        gs.g_infoPanelTab == 5)
      gs.g_infoPanelTab = -1;
    if (neg != 3) thought_bubble(ctx, partner, -1);
    if (gs.g_infoPanelTab != prev) {
      gs.g_infoPanelHoverFrames = 0;
      return;
    }
    clear();
    gs.g_infoPanelHoverFrames = add32(gs.g_infoPanelHoverFrames, 1);
    if (gs.g_infoPanelHoverFrames <= (neg == 2 ? 5 : 6)) return;
    static const uint32_t kStoryTips[6] = {0x60, 0x4c, 0x3c, 0x2c, 0, 0x1c};
    tooltip(ctx, gs.g_infoPanelTab, kStoryTips);
    return;
  }

  read_cursor(ctx, cx, cy, "drawGolferInfoPanel");
  if (near(0x13e, 0x106, 0x19)) gs.g_infoPanelTab = 0;
  if (near(0x17f, 0x106, 0x19)) gs.g_infoPanelTab = 1;
  if (near(0x1c0, 0x106, 0x19)) gs.g_infoPanelTab = 2;
  if (near(0x201, 0x106, 0x19)) gs.g_infoPanelTab = 3;
  if (near(0x253, 0x106, 0x19)) gs.g_infoPanelTab = 4;
  if (gs.g_infoPanelTab != prev) {
    gs.g_infoPanelHoverFrames = 0;
  } else {
    gs.g_infoPanelHoverFrames = add32(gs.g_infoPanelHoverFrames, 1);
    if (gs.g_infoPanelHoverFrames > (neg == 2 ? 5 : 6) && gs.g_infoPanelTab != -1) {
      static const uint32_t kTips[6] = {0x60, 0x4c, 0x3c, 0x10, 0, 0x1c};
      tooltip(ctx, gs.g_infoPanelTab, kTips);
    }
  }
  // the thoughts, newest first from slot 9 (mode != 0) or 4, at most five shown
  int32_t drawn = 0;
  for (int32_t k = mode != 0 ? (neg == 5 ? 8 : 9) : 4; k >= 0; --k) {
    const uint32_t kind = m.load<uint8_t>(R + 0x70 + u32(k));
    if (kind == 0) continue;
    const uint32_t p = m.load<uint8_t>(R + 0x7a + u32(k));
    cpy(ctx, tb, place_name(ctx, i32(p / 11)));
    cat(ctx, tb, GS(f_strpool_4d3968) + 8);
    if (kind != 0x13) {
      cat(ctx, tb, place_name(ctx, i32(p % 11)));
      cat(ctx, tb, GS(f_strpool_4d3968));
    }
    const uint32_t len = vstrlen(m, tb);
    const uint32_t word = R + 0x88 + 2 * u32(k);
    peer(ctx, "buildGolferComment", {kind, m.load<uint16_t>(word) & 0x3fffu, p, u32(g)});
    if (gs.g_thoughtOnPartner != 0) {
      m.store<uint8_t>(tb + len, 0);
      cat(ctx, tb, GS(f_strpool_4d3964));
      peer(ctx, "buildGolferComment", {m.load<uint8_t>(R + 0x70 + u32(k)), m.load<uint16_t>(word) & 0x3fffu,
                                            m.load<uint8_t>(R + 0x7a + u32(k)), u32(g)});
      cat(ctx, tb, GS(f_strpool_4d3964));
    }
    // the colour: red for top bits 0xc000 (green 0x4000, purple kind 0x32); mode != 0 shows only the red ones. The
    // width test (Text_put <= 0x180) only chooses how the line is drawn.
    const uint32_t top = m.load<uint16_t>(word) & 0xc000u;
    bool red = top == 0xc000u;
    if (m.load<uint8_t>(R + 0x70 + u32(k)) == 0x32) red = false;
    if (mode != 0 && !red) continue;
    if (++drawn >= 5) return;
  }
}

// ------------------------------------------------------------------------------------------------ scroll text
// The scroll-text box (U-0950): drawScrollText lays the text out in the scroll-window globals (line starts, the hotkey
// byte table, the widest line, the line and entry counts, the box's right and bottom edges) from Text_put widths, which
// come from the context's TextMetrics (commands.h; MissingResource{"Text_put"} without one, at the first width), then
// draws (dropped). runScrollText is the menu loop over it, scrollTextWindow its entry with a position.
namespace {
// the original's frame buffers (runScrollText's text copy, drawScrollText's one-character string, showBulletinBoard's
// worldToScreen out-cells) live in a zeroed block of the context's address space above the GameState, like tick's frame
// block; slots are handed out and taken back in stack order
constexpr uint32_t kScratchSize = 0x2000;
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
  uint32_t base = (ctx.mem.data_base() + static_cast<uint32_t>(sizeof(GameState)) + 0x01000000u) & ~0xFFFFu;
  for (int tries = 0; tries < 64; ++tries, base += 0x00100000u)
    if (ctx.mem.map(base, b.host.data(), kScratchSize, false)) {
      b.va = base;
      return base;
    }
  throw MissingResource{"dialogs scratch block (no free address range)"};
}
// n zeroed bytes of the scratch block for the lifetime of the object
struct Scratch {
  Ctx &ctx;
  uint32_t n, va;
  Scratch(Ctx &c, uint32_t bytes) : ctx(c), n((bytes + 3u) & ~3u), va(0) {
    const uint32_t base = scratch_base(c);
    ScratchBlock &b = scratch_blocks()[&c];
    if (b.sp + n > kScratchSize) throw MissingResource{"dialogs scratch block overflow"};
    va = base + b.sp;
    b.sp += n;
    std::memset(c.mem.at(va, n, true), 0, n);
  }
  ~Scratch() { scratch_blocks()[&ctx].sp -= n; }
};
// a write past one of the original's frame buffers (it would overwrite its own locals or return address)
struct FrameOverrun {
  const char *what;
};

int32_t scale_x(Ctx &ctx, int32_t v) { return div32(add32(mul32(ctx.gs().g_screenWidth, v), 160), 320); }

// Text_put on the text object with the base font runScrollText's setDrawTarget selects (g_font519928)
int32_t text_width(Ctx &ctx, uint32_t text) {
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) throw MissingResource{"Text_put"};
  return tm->text_put(ctx, ctx.gs().f_stateptr_4c1570[0], GS(g_font519928), text);
}
}  // namespace

// drawScrollText (text, sel): returns sel. The hotkey table (32 bytes of g_scrollTextLine) is set to 0xff; pass 1 walks
// the text: a newline ends a line (the widest line's width kept, the next line's start offset stored, the line count
// raised); a ' ' or '_' at the start of a line is a menu entry: the character after it goes to the hotkey table (first
// 32 entries) and the first entry's line is kept when none is; every other character adds its own Text_put width (one
// character at a time; a '|' also adds the width of the column separator string). Then the line count is clamped to the
// box ((0xe8 - top) / line height), a box x of -1 is centred on the widest line, the right edge (2 * widest /
// scaleX(2) + x + 8) and the bottom edge (line height * lines + top + 6, + 2 with the footer flag) are stored, a text
// that does not end in a newline counts one entry less, and an odd top marks the plain-rectangle style. Drawing the
// lines (pass 3) changes nothing it does not put back.
int32_t draw_scroll_text(Ctx &ctx, uint32_t text, int32_t sel) {
  const Memory &m = ctx.mem;
  const uint32_t hot = GS(g_scrollTextLine);
  const uint32_t starts = GS(g_scrollTextState) + 0xc;
  const uint32_t vars = GS(g_scrollTextVars);
  const uint32_t win = GS(g_scrollTextWin);
  const int neg = SG_NEGCTL(g_negctl_scroll_text);
  for (uint32_t i = 0; i < 32; ++i) m.store<uint8_t>(hot + i, 0xff);
  int32_t width = 0, line = 0, marks = 0;
  m.store<int32_t>(starts, 0);
  m.store<int32_t>(vars + 8, 0);
  m.store<int32_t>(vars + 0x24, 0);
  Scratch pair(ctx, 2);
  for (uint32_t idx = 0; idx < vstrlen(m, text); ++idx) {
    const uint8_t c = m.load<uint8_t>(text + idx);
    if (c == '\n') {
      if (m.load<int32_t>(vars + 0x24) < width) m.store<int32_t>(vars + 0x24, width);
      width = 0;
      ++line;
      m.store<int32_t>(vars + 8, line);
      m.store<int32_t>(starts + 4 * u32(line), i32(idx) + 1);
      continue;
    }
    if (width == 0 && (c == ' ' || c == '_')) {
      if (marks < 0x20) m.store<uint8_t>(hot + u32(marks), m.load<uint8_t>(text + idx + 1));
      if (m.load<int32_t>(win + 0x2c) == -1) m.store<int32_t>(win + 0x2c, line);
      ++marks;
    }
    m.store<uint8_t>(pair.va, c);
    width = add32(width, text_width(ctx, pair.va));
    if (c == '|') width = add32(width, text_width(ctx, GS(f_str_4e3db4)));
    line = m.load<int32_t>(vars + 8);
  }
  const int32_t lh = m.load<int32_t>(GS(f_w_4e3da4) + 8);
  m.store<int32_t>(vars + 8, clamp(line, 0, div32(sub32(0xe8, m.load<int32_t>(win + 0x14)), lh)));
  if (m.load<int32_t>(win + 0x10) == -1) {
    int32_t t = add32(m.load<int32_t>(vars + 0x24), 8);
    t = sub32(t, t >> 31);
    m.store<int32_t>(win + 0x10, sub32(0xa0, t >> 1));
  }
  const int32_t sx2 = scale_x(ctx, 2);
  const int32_t x0 = m.load<int32_t>(win + 0x10), y0 = m.load<int32_t>(win + 0x14);
  const int32_t right = add32(add32(div32(mul32(m.load<int32_t>(vars + 0x24), 2), sx2), x0), neg == 1 ? 9 : 8);
  m.store<int32_t>(vars + 0x14, right);
  const int32_t bottom = add32(add32(mul32(lh, m.load<int32_t>(vars + 8)), y0), 6);
  m.store<int32_t>(vars + 0x18, bottom);
  if (m.load<int32_t>(win + 0x40) != 0) m.store<int32_t>(vars + 0x18, add32(bottom, 2));
  const uint32_t len = vstrlen(m, text);
  if (m.load<uint8_t>(text + len - 1) != '\n' && neg != 2) --marks;
  m.store<int32_t>(vars + 0xc, marks);
  if ((y0 & 1) != 0) m.store<int32_t>(vars + 0x10, 1);
  // pass 3 (the drawing): each line's closing newline and its column bar are NUL while the line is drawn, then put back
  // as '\n' and '|'. The line-start table has no bound (a 33rd line overwrites the no-wait flag, a 35th the line count),
  // so with such a text the starts are text offsets no longer and these writes land where the original's do.
  for (int32_t j = 0; j < m.load<int32_t>(vars + 8); ++j) {
    const uint32_t end = text + m.load<uint32_t>(starts + 4 * u32(j) + 4) - 1;
    m.store<uint8_t>(end, 0);
    uint32_t bar = 0;
    for (uint32_t p = text + m.load<uint32_t>(starts + 4 * u32(j));; ++p) {   // strchr(line, '|')
      const uint8_t ch = m.load<uint8_t>(p);
      if (ch == '|') {
        bar = p;
        break;
      }
      if (ch == 0) break;
    }
    if (bar != 0) {
      m.store<uint8_t>(bar, 0);
      text_width(ctx, bar + 1);   // the right column's width only places it (drawing), measured as the original does
      m.store<uint8_t>(bar, '|');
    }
    m.store<uint8_t>(end, '\n');
  }
  return sel;
}

// runScrollText (text, unused): the menu loop; returns the chosen entry or -1. The text is copied into a 0x400-byte
// frame buffer (a longer text overruns the original's frame: FrameOverrun); the selection starts at the remembered one
// (f_w_4e3da4[0], -1 = 0); the loop state, line height 7 and the colour set (by bit 0 of the box top) are set; the box
// is laid out; the no-wait flag (g_scrollTextVars[0]) set means lay out only: cleared, -1. The loop: pumpMessages
// (Blocking); with no button and no hover only the key test runs; otherwise the point is the cursor (scaled by 4 /
// scaleX(4)) or, after a click, the click's scaled point (the clicked flag set, a right button sets the right flag), the
// entry under it (a disabled entry keeps the last drawn one, outside the box -1), and a click with the button already
// released ends the loop; a key: Enter / Space choose an enabled entry, Esc -1, Up / Down move, any other key jumps to
// the next entry whose hotkey's low 5 bits match. An entry out of range or a disabled box gives -1; a changed selection
// is laid out again. The exit clears the remembered selection, the box flags and the input words (resetScrollText).
int32_t run_scroll_text(Ctx &ctx, uint32_t text) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int neg = SG_NEGCTL(g_negctl_scroll_text);
  const uint32_t vars = GS(g_scrollTextVars);
  const uint32_t win = GS(g_scrollTextWin);
  const uint32_t state = GS(g_scrollTextState);
  const uint32_t remember = GS(f_w_4e3da4);
  const uint32_t n = vstrlen(m, text);
  if (n + 1 > 0x400) throw FrameOverrun{"runScrollText: a text longer than its 0x400-byte frame buffer"};
  Scratch frame(ctx, 0x404);
  const uint32_t buf = frame.va + 4;   // the byte before the copy reads as 0 (an empty text's last-character test)
  for (uint32_t i = 0; i <= n; ++i) m.store<uint8_t>(buf + i, m.load<uint8_t>(text + i));
  int32_t sel = m.load<int32_t>(remember);
  if (sel == -1) sel = 0;
  int32_t last = -1;
  m.store<int32_t>(vars + 0x10, 0);
  m.store<int32_t>(win + 0x44, 0);
  m.store<int32_t>(win + 0x3c, 0);
  m.store<int32_t>(win + 0x2c, -1);
  m.store<int32_t>(state + 8, 1);
  const uint8_t ylo = m.load<uint8_t>(win + 0x14);
  m.store<int32_t>(remember + 8, 7);
  m.store<uint32_t>(win + 0xc, 0x80000000u);
  m.store<uint32_t>(win, 0x80004210u);
  m.store<uint32_t>(GS(scroll_text_style), 0x800003ffu);
  m.store<uint32_t>(vars + 4, 0x80000018u);
  if (ylo & 1) {
    m.store<uint32_t>(win + 0xc, 0x80007fffu);
    m.store<uint32_t>(vars + 4, 0x800003ffu);
    m.store<uint32_t>(GS(scroll_text_style), 0x800003ffu);
    m.store<uint32_t>(win, 0x80006318u);
  }
  int32_t done = 0, hover = 1;
  draw_scroll_text(ctx, buf, m.load<int32_t>(remember));
  if (m.load<int32_t>(vars) != 0) {
    m.store<int32_t>(vars, 0);
    return -1;
  }
  gs.g_rootInput[kButton] = 0;
  for (;;) {
    m.store<int32_t>(state + 8, 0);
    cm::pump_messages(ctx, PumpKind::Blocking, "runScrollText");
    int32_t click = gs.g_rootInput[kButton];
    bool key_branch;
    if (click == 0 && hover == 0) {
      key_branch = cm::flush_redraw(ctx) != 0;
    } else {
      hover = 1;
      int32_t px, py;
      if (click == 0) {
        read_cursor(ctx, px, py, "runScrollText");
        px = div32(i32(u32(px) * 4u), scale_x(ctx, 4));
        py = div32(i32(u32(py) * 4u), scale_x(ctx, 4));
        click = gs.g_rootInput[kButton];
      } else {
        px = gs.g_rootInput[0];
        py = gs.g_rootInput[1];
        m.store<int32_t>(win + 0x44, 1);
      }
      if (click == 2) m.store<int32_t>(win + 0x3c, 1);
      const int32_t top = m.load<int32_t>(win + 0x14);
      sel = sub32(div32(sub32(sub32(py, top), 4), m.load<int32_t>(remember + 8)), m.load<int32_t>(win + 0x2c));
      if (sel < 0) sel = -1;
      if (((1u << (u32(sel) & 31)) & m.load<uint32_t>(win + 0x30)) != 0) sel = last;
      if (px > m.load<int32_t>(vars + 0x14) || px < m.load<int32_t>(win + 0x10)) sel = -1;
      if (py > m.load<int32_t>(vars + 0x18) || py < top) sel = -1;
      if (m.load<int32_t>(win + 0x44) != 0 && gs.g_rootInput[kButton] == 0) done = 1;
      key_branch = cm::flush_redraw(ctx) != 0;
    }
    bool none = false;
    if (key_branch) {
      hover = 0;
      const int32_t key = cm::take_code(ctx);
      switch (key) {
        case 0x0d:
        case 0x20:
          if (((1u << (u32(sel) & 31)) & m.load<uint32_t>(win + 0x30)) == 0) done = 1;
          break;
        case 0x1b:
          done = 1;
          none = neg != 3;
          break;
        case 0x26:
          if (sel > 0) --sel;
          break;
        case 0x28:
          if (sel < sub32(m.load<int32_t>(vars + 0xc), 1)) ++sel;
          break;
        default:
          for (int32_t i = sel + 1; i < 0x20; ++i) {
            const uint8_t h = m.load<uint8_t>(GS(g_scrollTextLine) + u32(i));
            if (h != 0xff && ((h ^ u32(key)) & 0x1f) == 0) {
              sel = i;
              break;
            }
          }
          break;
      }
    }
    if (none || sel < 0 || sel >= m.load<int32_t>(vars + 0xc) || m.load<int32_t>(win + 0x24) != 0 ||
        m.load<int32_t>(state + 4) != 0)
      sel = -1;
    if (sel != last && m.load<int32_t>(win + 0x28) != 0) {
      draw_scroll_text(ctx, buf, sel);
      last = sel;
    }
    if (done != 0 || gs.g_flag_83afcc != 0) break;
  }
  if (sel == -1) m.store<int32_t>(win + 0x3c, 0);
  m.store<int32_t>(remember + 4, -1);
  m.store<int32_t>(remember, -1);
  m.store<int32_t>(win + 0x40, 0);
  m.store<int32_t>(state + 4, 0);
  m.store<int32_t>(win + 0x30, 0);
  m.store<int32_t>(state, 0);
  gs.g_rootInput[kButton] = 0;   // resetScrollText: the button and both key words
  gs.gDirtyFlagB = 0;
  gs.gDirtyFlagA = 0;
  return sel;
}

// scrollTextWindow (text, x, y): the box position and the redraw flag, then runScrollText(text, 1)
int32_t scroll_text_window(Ctx &ctx, uint32_t text, int32_t x, int32_t y) {
  const uint32_t win = GS(g_scrollTextWin);
  ctx.mem.store<int32_t>(win + 0x10, x);
  ctx.mem.store<int32_t>(win + 0x14, y);
  ctx.mem.store<int32_t>(win + 0x28, 1);
  return run_scroll_text(ctx, text);
}

// ------------------------------------------------------------------------------------------------ showBulletinBoard
namespace {
// pumpInput483bd0 as a modal wait: the display's showScreen loop dispatches what has arrived (Blocking: the board waits
// for a click or a key); its input-phase word ends 0
void pump_input_blocking(Ctx &ctx, const char *where) {
  ctx.gs().g_inputPhase = 0x3f;
  InputSource *src = input_source(ctx);
  if (!src || src->exhausted()) throw NeedInput{{where}};
  std::vector<Command> batch;
  if (src->batch(PumpKind::Blocking, batch))
    for (const Command &c : batch) apply_command(ctx, c);
  ctx.gs().g_inputPhase = 0;
}
}  // namespace

// showBulletinBoard (): the end-of-season board (P5 calls it every unpaused frame). No pending photo (f_w_4e3db8[0] ==
// -1): nothing. A photo position (f_w_4e3db8[1] != -1): off screen the camera jumps to it, on screen the photo's file
// name is built (__itoa of the photo index into g_scoreSummaryBuf; saveBmp writes the file: output, dropped) and the
// position cleared. The board waits until g_bulletinAnimTick reaches 0x14. Then, for the dated event records in date
// order, the line text (appendCourseTitle and appendDate with g_date / g_courseIndex set from the record and put back
// after it: appendDate leaves g_year at the event's year; the month through __itoa into g_scoreSummaryBuf) and the photo
// file name (__itoa of the record index; the photo is drawing); the board waits for a click or a key (pumpInput483bd0);
// then the button word is cleared and, with a skill award (mask word != 0) and a real photo (index != 0x63), the award
// text is laid out (scrollTextWindow with the no-wait flag set), the scratch golfer record 156's type and status words
// are set and skillPointDialog(golfer type 0's name, mask, 3, 156, 0xc8) stores its result in the mask word. The photo
// index becomes -1 and the button word 0.
void show_bulletin_board(Ctx &ctx) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  const int neg = SG_NEGCTL(g_negctl_bulletin);
  const uint32_t photo = GS(f_w_4e3db8);
  const uint32_t tb = GS(g_textBuffer);
  const uint32_t numbuf = GS(g_scoreSummaryBuf);
  if (m.load<int32_t>(photo) == -1) return;
  if (m.load<int32_t>(photo + 4) != -1) {
    Scratch out(ctx, 8);
    const uint32_t wy = GS(g_scrollTextWin) + 0x54;
    if (call_peer(ctx, "worldToScreen", {m.load<uint32_t>(photo + 4), m.load<uint32_t>(wy), out.va, out.va + 4, 0}).eax ==
        0) {
      gs.gCameraTileX = sar32(m.load<int32_t>(photo + 4), 10);
      gs.gCameraTileY = sar32(m.load<int32_t>(wy), 10);
    } else {
      itoa10(ctx, m.load<int32_t>(photo), numbuf);   // the file name; clamp and the rectangle only feed saveBmp
      m.store<int32_t>(photo + 4, -1);
    }
  }
  const int32_t seen = add32(gs.g_bulletinAnimTick, 1);
  gs.g_bulletinAnimTick = seen;
  if (seen < 0x14) return;
  if (m.load<int32_t>(photo) != 0x63) cm::call_optional(ctx, "playSound", {0x38, 0x64, 0, 0, 0});
  const uint32_t ev = GS(g_bulletinEvents) + 0x28;   // 22 records of 0x30: date, course index, ...
  int32_t order[22];
  bool used[22] = {};
  int32_t count = 0;
  for (;;) {
    int32_t best = -1, best_date = 0x7ffffff;
    for (int32_t i = 0; i < 22; ++i) {
      const int32_t date = m.load<int32_t>(ev + 0x30 * u32(i));
      if (date != 0 && !used[i] && date < best_date) {
        best_date = date;
        best = i;
      }
    }
    if (best == -1) break;
    order[count++] = best;
    used[best] = true;
  }
  for (int32_t k = 0; k < count; ++k) {
    const int32_t e = order[neg == 1 ? count - 1 - k : k];
    const int32_t saved_date = gs.g_date;
    const int32_t saved_course = gs.g_courseIndex;
    m.store<uint8_t>(tb, 0);
    gs.g_date = m.load<int32_t>(ev + 0x30 * u32(e));
    gs.g_courseIndex = m.load<int32_t>(ev + 0x30 * u32(e) + 4);
    call_peer(ctx, "appendCourseTitle", {0});
    cat(ctx, tb, GS(f_strpool_4e3e28) + 0x2c);
    uint32_t mo = u32(gs.g_date) & 0x800003ffu;   // the date's day part, made positive the way the original does
    if (i32(mo) < 0) mo = ((mo - 1) | 0xfffffc00u) + 1;
    const int32_t scaled = i32(mo * 0x1eu);
    const int32_t month = add32(sar32(add32(scaled, sar32(scaled, 31) & 0x3ff), 10), 1);
    cat(ctx, tb, itoa10(ctx, month, numbuf));
    cat(ctx, tb, GS(f_strpool_4c3f70));
    call_peer(ctx, "appendDate", {u32(gs.g_date)});
    gs.g_courseIndex = saved_course;
    gs.g_date = saved_date;
    cpy(ctx, tb, GS(f_strpool_4e3e7c));   // the photo's file name (loadBmpSurface: drawing)
    cat(ctx, tb, itoa10(ctx, e, numbuf));
    cat(ctx, tb, GS(f_strpool_4c4970) + 0x4d0);
  }
  for (;;) {
    pump_input_blocking(ctx, "showBulletinBoard");
    if (gs.g_rootInput[kButton] != 0) break;
    if (cm::flush_redraw(ctx) != 0) break;
  }
  const int16_t award = m.load<int16_t>(GS(g_skillMaskBlock) + 8);
  gs.g_rootInput[kButton] = 0;
  if (award != 0 && m.load<int32_t>(photo) != 0x63) {
    cpy(ctx, tb, GS(f_strpool_4e3e28));
    m.store<int32_t>(GS(g_scrollTextVars), 1);
    scroll_text_window(ctx, tb, 0x50, 5);
    const int16_t w = m.load<int16_t>(GS(g_skillMaskBlock) + 8);
    const uint32_t r156 = rec(ctx, 0x9c);   // one record past the golfer table (the model's g_tileBuildCode)
    m.store<int16_t>(r156 + 0xb6, 0);
    m.store<int16_t>(r156 + 0xb8, static_cast<int16_t>(m.load<int8_t>(GS(g_golferTypes) + 0x21)));
    const int32_t r = skill_point_dialog(ctx, GS(g_golferTypes) + 0x10, w, w != 0 ? (neg == 2 ? 4 : 3) : 0xd, 0x9c, 0xc8);
    m.store<int16_t>(GS(g_skillMaskBlock) + 8, static_cast<int16_t>(r));
  }
  m.store<int32_t>(photo, -1);
  gs.g_rootInput[kButton] = 0;
}

// ------------------------------------------------------------------------------------------------ test entries
namespace {
// a scripted InputSource (tests): the QueueInput rule for batches (a Blocking pump takes every command up to and
// including the first press or key) and a cursor SEQUENCE, one point per read, the last one repeated
class ScriptInput : public InputSource {
 public:
  ScriptInput(std::vector<Command> cmds, std::vector<std::pair<int32_t, int32_t>> cur)
      : cmds_(std::move(cmds)), cur_(std::move(cur)) {}
  bool batch(PumpKind kind, std::vector<Command> &out) override {
    out.clear();
    if (kind != PumpKind::Blocking) return false;
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
  std::vector<Command> cmds_;
  std::vector<std::pair<int32_t, int32_t>> cur_;
  size_t pos_ = 0, reads_ = 0;
};
std::map<const Ctx *, std::unique_ptr<ScriptInput>> &scripts() {
  static std::map<const Ctx *, std::unique_ptr<ScriptInput>> m;
  return m;
}
// dialogs.setScript(cmds va, n cmds (6 dwords each: handler, a0, a1, async_ctrl, async_shift, t), cursor va, n points
// (x, y dwords))
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
  auto s = std::make_unique<ScriptInput>(std::move(cmds), std::move(cur));
  set_input_source(c, s.get());
  scripts()[&c] = std::move(s);
}
// dialogs.scriptStats -> consumed commands | cursor reads << 16
void a_script_stats(Ctx &c, const uint32_t *, Ret &r) {
  const auto it = scripts().find(&c);
  r.set_u32(it == scripts().end() ? 0u : static_cast<uint32_t>(it->second->consumed() | (it->second->reads() << 16)));
}
void a_clear_script(Ctx &c, const uint32_t *, Ret &) {
  set_input_source(c, nullptr);
  scripts().erase(&c);
}
// dialogs.scriptPeer(name hash, salt, log va, cap) / dialogs.clearPeers: see peer()
void a_script_peer(Ctx &c, const uint32_t *a, Ret &) {
  PeerScript &ps = peer_scripts()[&c];
  ps.salt[a[0]] = a[1];
  ps.va = a[2];
  ps.cap = a[3];
}
void a_clear_peers(Ctx &c, const uint32_t *, Ret &) { peer_scripts().erase(&c); }
inline int32_t S(const uint32_t *a, int i) { return static_cast<int32_t>(a[i]); }

const FuncDef kFuncs[] = {
    {"dialogs.scriptPeer", "default", "void", 4, 0, a_script_peer, &g_negctl_none},
    {"dialogs.clearPeers", "default", "void", 0, 0, a_clear_peers, &g_negctl_none},
    {"dialogs.setScript", "default", "void", 4, 0, a_set_script, &g_negctl_none},
    {"dialogs.scriptStats", "default", "int", 0, 0, a_script_stats, &g_negctl_none},
    {"dialogs.clearScript", "default", "void", 0, 0, a_clear_script, &g_negctl_none},
    {"skillPointDialog", "default", "int", 5, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(skill_point_dialog(c, a[0], S(a, 1), S(a, 2), S(a, 3), S(a, 4))); },
     &g_negctl_skill_dialog},
    {"showCharacterIntro", "default", "int", 0, 0,
     [](Ctx &c, const uint32_t *, Ret &r) { r.set_i32(show_character_intro(c)); }, &g_negctl_char_intro},
    {"drawGolferInfoPanel", "default", "void", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &) { draw_golfer_info_panel(c, S(a, 0), S(a, 1)); }, &g_negctl_info_panel},
    {"drawScrollText", "default", "int", 2, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(draw_scroll_text(c, a[0], S(a, 1))); }, &g_negctl_scroll_text},
    {"runScrollText", "default", "int", 2, 0, [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(run_scroll_text(c, a[0])); },
     &g_negctl_scroll_text},
    {"scrollTextWindow", "default", "int", 3, 0,
     [](Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(scroll_text_window(c, a[0], S(a, 1), S(a, 2))); },
     &g_negctl_scroll_text},
    {"showBulletinBoard", "default", "void", 0, 0, [](Ctx &c, const uint32_t *, Ret &) { show_bulletin_board(c); },
     &g_negctl_bulletin},
};
}  // namespace

extern const ModuleDef kModule_dialogs = {"dialogs", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
