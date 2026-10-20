// commands.cpp part: the two text widgets mainLoop and the panels call, showPopup and textInputBox. Their effect depends on
// Text_put widths, which come from the context's TextMetrics (commands.h, TEXT METRICS; U-0944). Specification: the
// C3 hook sources c3em.cpp (showPopup) and c3el.cpp (textInputBox); every address is in those notes and in
// log/p3/p37b_notes.txt, none here.

// Text_put on the draw context the caller reads from f_stateptr_4c1570, with the base font the caller's setDrawTarget
// selected (style fonts 1..3 none); the drawing calls around it are dropped
int32_t text_put(Ctx &ctx, uint32_t font, uint32_t text) {
  TextMetrics *tm = text_metrics(ctx);
  if (!tm) throw MissingResource{"Text_put"};
  return tm->text_put(ctx, ctx.gs().f_stateptr_4c1570[0], font, text);
}

// showPopup (x, y, mask, flag4, disabled): a box over the lines of g_textBuffer (one per 0x0a; a line that starts with a
// space is an option, the others are captions). Returns the chosen option index (flag4 != 0: Enter / Space / a click
// on an enabled option; Esc -1), or for flag4 == 0 (a check list) the toggled mask when the close button is clicked.
// The box width is (widest line - 1) | 0xf plus 0x31, from Text_put; the height (lines * 3 + 3) * 8. Drawing, the
// colour reads and the waitTicks(10, 0) between passes (no pump) are dropped; the pass's pump is Blocking.
// The y of the first option line is written only when an option line exists; the hover and click tests read it either
// way, so with no option line the original reads an uninitialised frame slot: 0 here (the zeroed-stack convention).
int32_t show_popup(Ctx &ctx, int32_t x, int32_t y, int32_t init_mask, int32_t flag4, int32_t disabled) {
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  if (!text_metrics(ctx)) throw MissingResource{"Text_put"};   // every call measures (the tail at least): before any write
  const int mode = SG_NEGCTL(g_negctl_popup);
  const uint32_t buf = GS(g_textBuffer);
  const uint32_t font = GS(g_font519928);
  int32_t mask = flag4 != 0 ? 0 : init_mask;
  // measuring pass: each line is measured with its 0x0a replaced by a 0 (put back after), then the tail
  int32_t widest = 0, lines = 0;
  uint32_t seg = 0;
  for (uint32_t i = 0; i < vstrlen(m, buf); ++i) {
    if (m.load<uint8_t>(buf + i) != 0x0a) continue;
    m.store<uint8_t>(buf + i, 0);
    const int32_t w = text_put(ctx, font, buf + seg);
    m.store<uint8_t>(buf + i, 0x0a);
    seg = i + 1;
    if (w > widest) widest = w;
    ++lines;
  }
  const int32_t tail = text_put(ctx, font, buf + seg);
  if (tail > widest) widest = tail;
  const int32_t inner = sub32(widest, 1) | 0xf;
  const int32_t box_w = add32(inner, 0x31);
  const int32_t box_h = mul32(add32(mul32(lines, 3), 3), 8);
  int32_t sel = -1, result = 0, key_latch = 0, first_y = 0;
  for (;;) {
    const int32_t half = box_w / 2;
    const int32_t left = sub32(x, half);
    // line pass: the option lines' indices, the recorded result (the last option whose bit is in the mask)
    int32_t line_y = add32(y, 0xc), line_idx = 0, first = -1;
    uint32_t seg2 = 0;
    for (uint32_t k = 0; k < vstrlen(m, buf); ++k) {
      if (m.load<uint8_t>(buf + k) != 0x0a) continue;
      if (m.load<uint8_t>(buf + seg2) == ' ') {
        if (first == -1) {
          first = line_idx;
          first_y = line_y;
        }
        const uint32_t bit = 1u << (u32(line_idx - first) & 31);
        if ((u32(disabled) & bit) == 0 && (bit & u32(mask)) != 0) result = line_idx - first;
      }
      seg2 = k + 1;
      line_y = add32(line_y, (first != -1 ? 6 : 0) + 0x12);
      ++line_idx;
    }
    const int32_t count = sub32(line_idx, first);
    pump_messages(ctx, PumpKind::Blocking, "showPopup");
    if (flag4 != 0) {
      if (flush_redraw(ctx) != 0) {
        const uint32_t t = u32(take_code(ctx)) - 0xd;
        if (t <= 0x1b) {
          switch (t + 0xd) {   // the key byte table: Enter / Space return the selection, Esc -1, Up, Down
            case 0x0d:
            case 0x20: return sel;
            case 0x1b: return -1;
            case 0x26:
              key_latch = 1;
              if (sel > 0 || mode == 2) --sel;
              break;
            case 0x28:
              key_latch = 1;
              if (sel < count - 1) ++sel;
              break;
            default: break;
          }
        }
      }
      mask = i32(1u << (u32(sel) & 31));
      if (gs.g_rootInput[kButton] != 0) {
        if ((u32(disabled) & u32(mask)) == 0) return sel;
        call_optional(ctx, "playSound", {0x18, 0x64, 0, 0, 0});   // a disabled option: the refusal sound, a new pass
        continue;
      }
    }
    // the hover: the option under the cursor (root-window coordinates) is selected
    InputSource *src = input_source(ctx);
    int32_t lx = 0, ly = 0;
    if (!src || !src->cursor(lx, ly)) throw NeedInput{{"showPopup"}};
    const int32_t right = add32(sub32(half, 0x30), x);
    if (left < lx && lx < right && first_y <= ly && ly < add32(first_y, mul32(count, 0x18))) {
      key_latch = 0;
      sel = sub32(ly, first_y) / 0x18;
    } else if (key_latch == 0) {
      sel = -1;
    }
    if (gs.g_rootInput[kButton] == 1) {
      const int32_t cx = gs.g_rootInput[kClickX], cy = gs.g_rootInput[kClickY];
      if (left < cx && cx < right && first_y <= cy) {
        const int32_t hit = sub32(cy, first_y) / 0x18;
        if (hit < count) {
          const int32_t b = i32(1u << (u32(hit) & 31));
          mask = flag4 != 0 ? b : (mode == 1 ? mask | b : mask ^ b);
        }
      }
      // the close button: within 0x14 of (x + half - 0x14, y + height - 0x14)
      if (abs32(add32(sub32(sub32(cx, half), x), 0x14)) < 0x14 &&
          abs32(add32(sub32(sub32(cy, box_h), y), 0x14)) < 0x14) {
        if (flag4 == 0) result = mask;
        return result;
      }
    }
  }
}

// _isprint (the static CRT): with __mb_cur_max above 1 the locale's _isctype (not ported: MissingResource); otherwise
// the ctype table word _pctype[c] & (_BLANK | _PUNCT | _ALPHA | _DIGIT). Both CRT globals sit in an unmodeled gap of the
// data section (offsets from the gap member, cited in the notes).
int32_t crt_isprint(Ctx &ctx, int32_t c) {
  const uint32_t gap = GS(gap_023a73);
  if (ctx.mem.load<int32_t>(gap + 0x7a9) > 1) throw MissingResource{"_isctype"};   // __mb_cur_max
  const uint32_t pctype = ctx.mem.load<uint32_t>(gap + 0x59d);                      // _pctype
  return ctx.mem.load<uint16_t>(pctype + 2 * u32(c)) & 0x157;
}

// textInputBox (title, x, y, maxLen): edits the NUL-terminated g_textBuffer in place; returns the last takeCode value.
// width = maxLen * 12; after every edit the text is cut from the end while its Text_put width is above width - 0x10
// (signed), the cursor clamped to the new length, the length variable NOT updated (the original's stale length: Enter
// then writes its 0 at the old length). Keys: Esc empties the text, Enter ends, Backspace deletes before the cursor;
// with no character the codes 0x27 / 0x25 / 0x23 / 0x24 move the cursor and 0x2e deletes at it; a printable character
// is inserted while the cursor is below maxLen - 1. The title and the copies the original makes into its frame,
// the caret measure and all drawing are dropped. An empty text whose width 0 is still above the limit (maxLen <= 1)
// makes the original write a 0 before the buffer forever: NoReturn after that write.
int32_t text_input_box(Ctx &ctx, uint32_t title, int32_t x, int32_t y, int32_t max_len) {
  (void)title;
  (void)x;
  (void)y;
  GameState &gs = ctx.gs();
  const Memory &m = ctx.mem;
  if (!text_metrics(ctx)) throw MissingResource{"Text_put"};   // the first width is measured before any write
  const int mode = SG_NEGCTL(g_negctl_text_input);
  const uint32_t tb = GS(g_textBuffer);
  const uint32_t font = GS(g_font519928);
  int32_t len = i32(vstrlen(m, tb));
  int32_t cur = len;
  const int32_t width = mul32(max_len, 12);
  for (;;) {
    int32_t w = text_put(ctx, font, tb);
    while (w > sub32(width, mode == 1 ? 0x18 : 0x10)) {
      const uint32_t n = vstrlen(m, tb);
      m.store<uint8_t>(tb + n - 1, 0);
      if (n == 0) throw NoReturn{"textInputBox"};
      const uint32_t n2 = vstrlen(m, tb);
      if (u32(cur) > n2) cur = i32(n2);
      w = text_put(ctx, font, tb);
    }
    do {
      pump_messages(ctx, PumpKind::Blocking, "textInputBox");
    } while (gs.g_rootInput[kButton] == 0 && flush_redraw(ctx) == 0);
    const int32_t ch = take_key(ctx);
    const int32_t code = take_code(ctx);
    if (ch == 0x1b) {
      m.store<uint8_t>(tb, 0);
      return code;
    }
    if (ch == 0x0d) {
      m.store<uint8_t>(tb + u32(len), 0);
      return code;
    }
    if (ch == 8) {
      if (cur != 0) {
        for (int32_t i = cur; i <= len; ++i) m.store<uint8_t>(tb + u32(i), m.load<uint8_t>(tb + u32(i) + 1));
        --cur;
        --len;
        m.store<uint8_t>(tb + u32(len), 0);
      }
    } else if (ch == 0) {
      if (code == 0x27) {
        if (cur < len) ++cur;
      } else if (code == 0x25) {
        if (cur != 0) --cur;
      } else if (code == 0x23) {
        cur = len;
      } else if (code == 0x24) {
        cur = 0;
      } else if (code == 0x2e && cur < len) {
        for (int32_t i = cur; i <= len; ++i) m.store<uint8_t>(tb + u32(i), m.load<uint8_t>(tb + u32(i) + 1));
        --len;
        m.store<uint8_t>(tb + u32(len), 0);
      }
    }
    if (cur >= max_len - 1) continue;
    if (crt_isprint(ctx, ch) == 0) continue;
    for (int32_t i = len; i >= cur; --i) m.store<uint8_t>(tb + u32(i) + 1, m.load<uint8_t>(tb + u32(i)));
    m.store<uint8_t>(tb + u32(cur), static_cast<uint8_t>(mode == 2 ? ch + 1 : ch));
    ++cur;
    ++len;
    m.store<uint8_t>(tb + u32(len), 0);
    m.store<uint8_t>(tb + u32(max_len), 0);
  }
}
