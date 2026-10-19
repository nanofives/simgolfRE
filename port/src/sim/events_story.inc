// Events, news and stories, part 2: the story file reader, the story reply chapter and the story progress of a golfer pair
// (see portsim/sim/events.h). Phase 3.5, written from the specification; no original address, code or text.
namespace portsim {

using ev::golfer;
using ev::text_va;

namespace {
// Negative controls: mode 0 is the correct body.
//   loadStoryStrings  1 = the cache key test ignores `sel`, 2 = the header counter starts at 0, 3 = the line cap is 201
//   computeStoryReply 1 = the 0xc000 outcome drops v once, 2 = the run of 0x13 words is not counted
//   storyText         1 = the owner's flag test uses bit 0x10000
//   advanceStory      1 = a partner one stage behind plays the other sound, 2 = the counter drop is c / 4
int g_negctl_load_story_strings = 0;
int g_negctl_compute_story_reply = 0;
int g_negctl_story_text = 0;
int g_negctl_advance_story = 0;
//   substituteStoryNames 1 = the second name is the golfer's own, 2 = the two tokens swapped
int g_negctl_substitute_story_names = 0;

constexpr int kCacheSlots = 8;
}  // namespace

namespace ev {

Ptr<Golfer> golfer(Ctx &ctx, int32_t g) { return state_arr(ctx, &GameState::g_golfers) + g; }

// the original's fopen(path, "rt") + fgets(line, 0xfa) over the bytes of a file: text mode turns CR LF into LF and ends at
// a Ctrl-Z; a line holds at most 249 bytes
struct TextStream {
  std::vector<uint8_t> data;
  size_t pos = 0;
  explicit TextStream(const std::vector<uint8_t> &raw) {
    for (size_t i = 0; i < raw.size(); ++i) {
      if (raw[i] == 0x1A) break;
      if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') continue;
      data.push_back(raw[i]);
    }
  }
  // false at end of file with nothing read
  bool gets(std::vector<uint8_t> &line, size_t cap) {
    line.clear();
    if (pos >= data.size()) return false;
    while (line.size() + 1 < cap && pos < data.size()) {
      const uint8_t c = data[pos++];
      line.push_back(c);
      if (c == '\n') break;
    }
    line.push_back(0);
    return true;
  }
};

}  // namespace ev

namespace {
uint32_t cstr_len(const std::vector<uint8_t> &v) {
  uint32_t n = 0;
  while (n < v.size() && v[n] != 0) ++n;
  return n;
}
}  // namespace

// loadStoryStrings(file, key, sel, mode): see events.h
int32_t load_story_strings(Ctx &ctx, int32_t file, int32_t key, int32_t sel, int32_t mode) {
  const Memory &m = ctx.mem;
  const uint32_t buf = text_va(ctx);
  const Ptr<StoryFileRec> cache = state_arr(ctx, &GameState::g_storyFiles);
  const int neg = SG_NEGCTL(g_negctl_load_story_strings);
  for (int i = 0; i < kCacheSlots; ++i) {
    const Ptr<StoryFileRec> e = cache + i;
    if (e.field(&StoryFileRec::f_00).get() != u32(file) || e.field(&StoryFileRec::f_04).get() != u32(key)) continue;
    if (!(neg == 1) && e.field(&StoryFileRec::f_08).get() != u32(sel)) continue;
    const uint32_t text = e.va() + 0xC;
    if (mode == -1) {
      vstrcat(m, buf, text);
      return 0;
    }
    const int32_t len0 = i32(vstrlen(m, buf));
    vstrcat(m, buf, text);
    const int32_t len1 = i32(vstrlen(m, buf));
    if (mode == 1) {
      if (len0 >= len1) return len0;
      for (int32_t k = len0; k < len1; ++k)
        if (m.load<uint8_t>(buf + u32(k)) == '\n') m.store<uint8_t>(buf + u32(k), 0);
      return len1;
    }
    uint32_t eax = u32(mode);
    if (len0 >= len1) return i32(eax);
    for (int32_t k = len0; k < len1; ++k) {
      eax = (eax & 0xFFFFFF00u) | m.load<uint8_t>(buf + u32(k));
      if (static_cast<char>(eax) != '\n') continue;
      const uint32_t src = buf + u32(k) + 1;
      const uint32_t n = vstrlen(m, src) + 1;
      rep_movs(m, buf + u32(len0), src, n);
      eax = n;
    }
    return i32(eax);
  }

  // a miss claims the slot named by the slot word
  const int32_t slot = state_obj(ctx, &GameState::g_story838220).get();
  const Ptr<StoryFileRec> claimed = cache + slot;
  claimed.field(&StoryFileRec::f_00).set(u32(file));
  claimed.field(&StoryFileRec::f_04).set(0xFFFFFFFEu);
  claimed.field(&StoryFileRec::f_08).set(u32(sel));

  // the file: <themes dir>/<theme or the standard theme>/<story file name of slot `file`>
  std::string name;
  auto append = [&](uint32_t va) {
    for (;; ++va) {
      const char c = static_cast<char>(m.load<uint8_t>(va));
      if (c == 0) break;
      name.push_back(c == '\\' ? '/' : c);
    }
  };
  append(state_va(ctx, &GameState::f_strpool_4c84e8));
  append((ctx.gs().g_flags & 0x10000000u) ? state_va(ctx, &GameState::f_strpool_4c6ba4) + 108
                                           : state_va(ctx, &GameState::g_theme));
  append(state_va(ctx, &GameState::f_strpool_4c84e4));
  append((state_arr(ctx, &GameState::g_storyDefs) + file).va());
  if (!ctx.files().exists(name)) return 0;
  ev::TextStream f(ctx.files().read_all(name));

  std::vector<uint8_t> line, saved(1, 0);   // the saved line is uninitialised stack until a space line was read (U-0901)
  int32_t state = key, count = 1, lines = 0, down = key;
  if (neg == 2) count = 0;
  const int32_t cap = neg == 3 ? 201 : 200;
  bool more = f.gets(line, 0xFA);
  bool stopped = false;
  auto cat = [&](const std::vector<uint8_t> &s) {
    uint32_t end = buf + vstrlen(m, buf);
    for (uint32_t k = 0;; ++k) {
      const uint8_t c = k < s.size() ? s[k] : 0;
      m.store<uint8_t>(end++, c);
      if (c == 0) break;
    }
  };
  while (more) {
    if (line.size() > 1 && line[1] != 0) {
      line[cstr_len(line) - 1] = 0;
      if (key == -1) {
        cat(line);
        return 0;
      }
      if (line[0] == ' ') {
        if (state == -1) {
          if (down == sel) {
            if (mode != 1) cat(line);
            sel = -1;
          } else {
            --down;
          }
        }
        saved = line;
      } else if (state == count) {
        cat(line);
        vstrcat(m, buf, state_va(ctx, &GameState::f_strpool_4d3880));
        ++count;
        state = -1;
      } else {
        if (state == -1 && sel != -1) {
          if (mode != 0) cat(saved);
          sel = -1;
        }
        ++count;
      }
      ++lines;
      if (lines > cap) return 0;
    }
    if (state == -1 && sel == -1) {
      stopped = true;
      break;
    }
    more = f.gets(line, 0xFA);
  }
  if (!stopped && state == -1 && sel != -1 && mode != 1) cat(saved);
  if (mode != -1) return mode;
  const int32_t s2 = state_obj(ctx, &GameState::g_story838220).get();
  const Ptr<StoryFileRec> e2 = cache + s2;
  vstrcpy(m, e2.va() + 0xC, buf);
  e2.field(&StoryFileRec::f_04).set(u32(key));
  const int32_t next = static_cast<int16_t>(static_cast<uint16_t>(s2 + 1)) % 8;
  state_obj(ctx, &GameState::g_story838220).set(static_cast<int16_t>(next));
  return next;
}

// computeStoryReply(file, key, hole, g)
int32_t compute_story_reply(Ctx &ctx, int32_t file, int32_t key, int32_t hole, int32_t g) {
  const int neg = SG_NEGCTL(g_negctl_compute_story_reply);
  const Ptr<Golfer> other = golfer(ctx, g ^ 1);
  const Ptr<Golfer> own = golfer(ctx, g);
  int32_t r = clamp(other.field(&Golfer::hole_counter).byte(hole).get() >> 3, 0, key);
  const int32_t pidx = own.field(&Golfer::partner).get();
  Ptr<Golfer> p = golfer(ctx, pidx);
  if (p.field(&Golfer::hole).get() == hole && p.field(&Golfer::hole_counter).byte(hole).get() == 0) {
    const Ptr<uint8_t> thoughts = p.field(&Golfer::thought_kind);
    int32_t n = 0;
    if (neg != 2 && thoughts.get() == 0x13) {
      while (n < 4) {
        ++n;
        if (thoughts.byte(n).get() != 0x13) break;
      }
    }
    const int32_t c = clamp(p.field(&Golfer::mood).get(), p.field(&Golfer::story_counter).get(), 0x63);
    int32_t v = (c - 1) / 2;
    if (key >= 2 && v > key - 1) v = key - 1;
    const Ptr<Golfer> pp = golfer(ctx, own.field(&Golfer::partner).get());
    const uint32_t t = (pp.field(&Golfer::thought_arg) + n).get() & 0xC000u;
    if (t == 0x4000u) {
      ++v;
    } else if (v != 0) {
      if (t != 0) --v;
      if (v != 0 && t == 0xC000u && neg != 1) --v;
    }
    const Ptr<Golfer> p2 = golfer(ctx, own.field(&Golfer::partner).get());
    if (ev::call_peer(ctx, "holeQuadrants", {u32(p2.field(&Golfer::x).get()), u32(p2.field(&Golfer::y).get()), 8}).eax != 0)
      v = key;
    r = clamp(v, 0, key);
    const Ptr<Golfer> q = golfer(ctx, own.field(&Golfer::partner).get());
    q.field(&Golfer::story_stage).set(static_cast<int16_t>(r));
    q.field(&Golfer::hole_counter).byte(hole).set(static_cast<uint8_t>((u32(r) << 3) + 1u));
  } else {
    p.field(&Golfer::story_stage).set(static_cast<int16_t>(r));
    golfer(ctx, own.field(&Golfer::partner).get())
        .field(&Golfer::hole_counter)
        .byte(hole)
        .set(static_cast<uint8_t>((u32(r) << 3) + 1u));
  }
  state_obj(ctx, &GameState::g_storyReplyChapter).set(r);
  return load_story_strings(ctx, file, key, r, -1);
}

// storyText(a, i, g)
int32_t story_text(Ctx &ctx, int32_t a, int32_t i, int32_t g) {
  const Memory &m = ctx.mem;
  const Ptr<Golfer> rec = golfer(ctx, g);
  const uint32_t f = rec.field(&Golfer::flags).get() & (SG_NEGCTL(g_negctl_story_text) == 1 ? 0x10000u : 0x100000u);
  uint32_t key;
  if (f != 0) {
    key = rec.field(&Golfer::hole_chapter).byte(i).get();
  } else {
    const Ptr<Golfer> prec = golfer(ctx, rec.field(&Golfer::partner).get());
    key = prec.field(&Golfer::hole_chapter).byte(i).get();
  }
  if (key == 0) return 0;
  const int32_t gg = f != 0 ? g : (g ^ 1);
  compute_story_reply(ctx, a, i32(key), i, gg);
  const uint32_t buf = text_va(ctx);
  const int32_t len = i32(vstrlen(m, buf));
  if (f != 0) {
    for (int32_t k = 0; k < len; ++k)
      if (m.load<uint8_t>(buf + u32(k)) == '\n') m.store<uint8_t>(buf + u32(k), 0);
    return sx8(m.load<uint8_t>(buf));
  }
  int32_t k = 0;
  while (k < len && m.load<uint8_t>(buf + u32(k)) != '\n') ++k;
  if (k >= len) {
    m.store<uint8_t>(buf, 0);
    return sx8(m.load<uint8_t>(buf));
  }
  uint32_t tail = 0;
  while (m.load<uint8_t>(buf + u32(k) + 1 + tail) != 0) ++tail;
  ++tail;
  for (uint32_t q = 0; q < tail; ++q) m.store<uint8_t>(buf + q, m.load<uint8_t>(buf + u32(k) + 1 + q));
  return sx8(m.load<uint8_t>(buf));
}

// substituteStoryNames(g): the two name tokens of the text in g_textBuffer become the display names of g and of g ^ 1
void substitute_story_names(Ctx &ctx, int32_t g) {
  const uint32_t buf = text_va(ctx);
  const int neg = SG_NEGCTL(g_negctl_substitute_story_names);
  const std::vector<uint8_t> saved = ev::grab(ctx, buf);
  ctx.mem.store<uint8_t>(buf, 0);
  ev::build_golfer_name(ctx, g, 0);
  const std::vector<uint8_t> self = ev::grab(ctx, buf);
  ctx.mem.store<uint8_t>(buf, 0);
  ev::build_golfer_name(ctx, g ^ (neg == 1 ? 0 : 1), 0);
  const std::vector<uint8_t> other = ev::grab(ctx, buf);
  ev::put(ctx, buf, saved);
  const uint32_t token_self = state_va(ctx, &GameState::f_strpool_4d3954) + 8;
  const uint32_t token_other = state_va(ctx, &GameState::f_strpool_4d3954);
  ev::replace_with(ctx, neg == 2 ? token_other : token_self, self);
  ev::replace_with(ctx, neg == 2 ? token_self : token_other, other);
}

// advanceStory(g, force)
int32_t advance_story(Ctx &ctx, int32_t g, int32_t force) {
  const int neg = SG_NEGCTL(g_negctl_advance_story);
  const Ptr<Golfer> r = golfer(ctx, g);
  auto partner = [&]() { return golfer(ctx, r.field(&Golfer::partner).get()); };
  if (r.field(&Golfer::story_id).get() == -1) return 0;
  if (force == 0 && r.field(&Golfer::hole).get() <= 1 && r.field(&Golfer::strokes).get() == 0) return 0;
  int16_t stage = r.field(&Golfer::story_stage).get();
  ctx.mem.store<uint8_t>(text_va(ctx), 0);
  if (stage > 3 && partner().field(&Golfer::story_stage).get() > 3) return 0;
  if (static_cast<int32_t>(r.field(&Golfer::story_counter).get()) <= static_cast<int32_t>(stage) * 4 + 4 && force == 0) return 0;
  stage = static_cast<int16_t>(stage + 1);
  r.field(&Golfer::story_stage).set(stage);
  r.field(&Golfer::hole_chapter).byte(r.field(&Golfer::hole).get()).set(static_cast<uint8_t>(stage));
  r.field(&Golfer::flags).set(r.field(&Golfer::flags).get() | (1u << 23));
  partner().field(&Golfer::flags).set(partner().field(&Golfer::flags).get() | (1u << 23));
  ctx.gs().g_lastStoryAdvanceDate = ctx.gs().g_date;
  const int32_t storyId = r.field(&Golfer::story_id).get();
  if (story_text(ctx, storyId, r.field(&Golfer::hole).get(), g) != 0) {
    emit_golfer_thought(ctx, g, 0x32, add32(mul32(ctx.gs().g_storyReplyChapter, 16), r.field(&Golfer::story_stage).get()));
    const int32_t x = r.field(&Golfer::x).get(), y = r.field(&Golfer::y).get();
    ev::play_sound_at(ctx, add32(ev::call_peer(ctx, "typeBit7Clear", {u32(g)}).eax, 0x58), x, y, 0);
  }
  const int32_t other = g ^ 1;
  if (story_text(ctx, r.field(&Golfer::story_id).get(), r.field(&Golfer::hole).get(), other) != 0) {
    emit_golfer_thought(ctx, other, 0x32, add32(mul32(ctx.gs().g_storyReplyChapter, 16), r.field(&Golfer::story_stage).get()));
    partner().field(&Golfer::thought_timer).set(static_cast<uint8_t>(partner().field(&Golfer::thought_timer).get() + 3));
    const int16_t mine = r.field(&Golfer::story_stage).get();
    const int32_t pi = r.field(&Golfer::partner).get();
    const Ptr<Golfer> pr = golfer(ctx, pi);
    const int16_t theirs = pr.field(&Golfer::story_stage).get();
    if (theirs >= mine) {
      const int32_t x = r.field(&Golfer::x).get(), y = r.field(&Golfer::y).get();
      ev::play_sound_at(ctx, add32(ev::call_peer(ctx, "typeBit7Clear", {u32(pi)}).eax, 0xa6), x, y, 0x5dc);
      if (r.field(&Golfer::story_stage).get() == 1) {
        const int32_t mode = state_obj(ctx, &GameState::g_courseType).get();
        const int32_t odd = r.field(&Golfer::story_id).get() & 1;
        if (u32(mode) <= 3u) {
          int32_t snd;
          switch (mode) {
            case 0: snd = (odd ? 2 : 0) + 0x32; break;
            case 1: snd = (odd | 0x3c) << 1; break;
            case 2: snd = (odd ? 2 : 0) + 0x6e; break;
            default: snd = (odd ? 2 : 0) + 0x73; break;
          }
          ev::play_sound(ctx, snd, 0x64, 0, 0, 0);
        }
        r.field(&Golfer::thought_timer).set(static_cast<uint8_t>(r.field(&Golfer::thought_timer).get() + 1));
        partner().field(&Golfer::thought_timer).set(static_cast<uint8_t>(partner().field(&Golfer::thought_timer).get() + 1));
      }
      r.field(&Golfer::mood).set(static_cast<int16_t>(r.field(&Golfer::mood).get() + 1));
      // the two calls of the empty function (no effect) are the type-flag tests; nothing to run
    } else {
      const int32_t px = pr.field(&Golfer::x).get(), py = pr.field(&Golfer::y).get();
      if (static_cast<int32_t>(theirs) == static_cast<int32_t>(mine) - 1 && neg != 1) {
        ev::play_sound_at(ctx, add32(ev::call_peer(ctx, "typeBit7Clear", {u32(pi)}).eax, 0xa4), px, py, 0x5dc);
      } else {
        ev::play_sound_at(ctx, add32(ev::call_peer(ctx, "typeBit7Clear", {u32(pi)}).eax, 0x5a), px, py, 0x5dc);
        r.field(&Golfer::mood).set(static_cast<int16_t>(r.field(&Golfer::mood).get() - 1));
      }
    }
  }
  int32_t result = 0;
  const int16_t theirs = partner().field(&Golfer::story_stage).get();
  const int16_t mine = r.field(&Golfer::story_stage).get();
  if (theirs < mine) {
    r.field(&Golfer::story_stage).set(static_cast<int16_t>(mine - 1));
    const int16_t c = r.field(&Golfer::story_counter).get();
    r.field(&Golfer::story_counter).set(static_cast<int16_t>(c - (neg == 2 ? c / 4 : c / 3)));
  } else {
    result = 1;
    if (mine == 4 && theirs == mine) {
      const int32_t mode = state_obj(ctx, &GameState::g_courseType).get();
      if (u32(mode) <= 3u) {
        static const int32_t kSnd[4] = {0x33, 0x78, 0x6e, 0x73};
        ev::play_sound(ctx, kSnd[mode], 0x64, 0, 0, 0);
      }
      ctx.gs().g_awardStories = add32(ctx.gs().g_awardStories, 1);
      ev::log_tick(ctx, 0x120, static_cast<int32_t>(r.field(&Golfer::story_id).get()));
      const Ptr<GolferTypeState> ts = state_arr(ctx, &GameState::g_golferTypeState);
      const Ptr<GolferTypeState> t1 = ts + r.field(&Golfer::type).get();
      t1.field(&GolferTypeState::ban).set(static_cast<uint8_t>(r.field(&Golfer::story_id).get()));
      const int32_t hole = r.field(&Golfer::hole).get();
      const Ptr<uint8_t> f1 = t1.field(&GolferTypeState::hole_flags).byte(hole);
      f1.set(f1.get() | 2);
      const Ptr<GolferTypeState> t2 = ts + partner().field(&Golfer::type).get();
      const Ptr<uint8_t> f2 = t2.field(&GolferTypeState::hole_flags).byte(hole);
      f2.set(f2.get() | 2);
      announce_landmark(ctx, g, r.field(&Golfer::story_id).get());
    }
  }
  {
    const Ptr<Golfer> pr = partner();
    pr.field(&Golfer::hole_chapter).byte(r.field(&Golfer::hole).get()).set(static_cast<uint8_t>(pr.field(&Golfer::story_stage).get()));
  }
  partner().field(&Golfer::wait).set(static_cast<int16_t>(static_cast<uint16_t>(0xFFFCu - r.field(&Golfer::story_stage).get()) << 2));
  r.field(&Golfer::wait).set(partner().field(&Golfer::wait).get());
  const int32_t y = r.field(&Golfer::y).get(), x = r.field(&Golfer::x).get();
  const Ptr<Golfer> pr = partner();
  const int32_t a = angle_fixed(sub32(pr.field(&Golfer::x).get(), x), sub32(pr.field(&Golfer::y).get(), y));
  const uint8_t face = static_cast<uint8_t>(((((a >> 28) & 0xF) + 1) >> 1) & 7);
  r.field(&Golfer::facing).set(static_cast<int8_t>(face));
  partner().field(&Golfer::facing).set(static_cast<int8_t>(face ^ 4));
  partner().field(&Golfer::state).set(0xB);
  r.field(&Golfer::state).set(0xB);
  partner().field(&Golfer::step_left).set(0);
  r.field(&Golfer::step_left).set(0);
  return result;
}

}  // namespace portsim
