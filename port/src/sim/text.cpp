// Text leaves (see portsim/sim/text.h). Phase 3.0, written from the specification; no original address, code or text.
// P24D: this file also holds the language packs (portsim/sim/lang.h), at its end: every src/sim file is a listed module.
#define _CRT_SECURE_NO_WARNINGS  // P24D (lang): std::fopen, std::getenv (MSVC /W4 /WX)
#include "portsim/sim/text.h"

#include "portsim/ptr.h"

namespace portsim {

namespace {
// Negative controls: mode 0 is the correct body.
//   appendClubName    1 = clubs 11 and 12 swapped, 2 = strcpy instead of strcat
//   sanitizeFileName  1 = keeps one trailing space, 2 = an empty result returns 1
int g_negctl_append_club_name = 0;
int g_negctl_sanitize_file_name = 0;

// byte offsets of the club names in f_strpool_4c4f98, club 0..13 (the switch's case strings)
constexpr uint32_t kClubName[14] = {0x288, 0x280, 0x278, 0x270, 0x268, 0x260, 0x258,
                                    0x250, 0x248, 0x240, 0x238, 0x22C, 0x220, 0x218};
}  // namespace

void append_club_name(Ctx &ctx, int32_t club) {
  if (u32(club) > 13u) return;
  uint32_t k = u32(club);
  if (SG_NEGCTL(g_negctl_append_club_name) == 1 && (k == 11 || k == 12)) k = 23 - k;
  const uint32_t src = state_va(ctx, &GameState::f_strpool_4c4f98) + kClubName[k];
  const uint32_t dst = state_va(ctx, &GameState::g_textBuffer);
  if (SG_NEGCTL(g_negctl_append_club_name) == 2)
    vstrcpy(ctx.mem, dst, src);
  else
    vstrcat(ctx.mem, dst, src);
}

int32_t sanitize_file_name(Ctx &ctx, uint32_t s) {
  if (vstrpbrk(ctx.mem, s, state_va(ctx, &GameState::f_strpool_4c3ee0)) != 0) return 0;
  const int mode = SG_NEGCTL(g_negctl_sanitize_file_name);
  int32_t len;
  for (;;) {
    len = i32(vstrlen(ctx.mem, s));
    if (len == 0) break;
    if (ctx.mem.load<uint8_t>(s + u32(len) - 1) != ' ') break;
    if (mode == 1 && (len < 2 || ctx.mem.load<uint8_t>(s + u32(len) - 2) != ' ')) break;
    ctx.mem.store<uint8_t>(s + u32(len) - 1, 0);
    if (!(len > 0)) break;
  }
  if (mode == 2 && len == 0) return 1;
  return len > 0 ? 1 : 0;
}

// ------------------------------------------------------------------------------------------------ adapters
namespace {
void a_append_club_name(Ctx &c, const uint32_t *a, Ret &) { append_club_name(c, static_cast<int32_t>(a[0])); }
void a_sanitize_file_name(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(sanitize_file_name(c, a[0])); }

const FuncDef kFuncs[] = {
    {"appendClubName", "default", "void", 1, 0, a_append_club_name, &g_negctl_append_club_name},
    {"sanitizeFileName", "default", "bool", 1, 0, a_sanitize_file_name, &g_negctl_sanitize_file_name},
};
}  // namespace

extern const ModuleDef kModule_text = {"text", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim

// ======================================================================================================================
// P24D: language packs, drawing only (portsim/sim/lang.h). No game text lives here: the packs are the user's files.
#include "portsim/sim/lang.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "portsim/ptr.h"
#include "portsim/registry.h"

namespace portsim {
namespace lang {

namespace {

// code page 1252's 0x80..0x9f (0 = undefined there: kept as the C1 control of the same value)
const uint16_t kHigh[32] = {0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
                            0x2039, 0x0152, 0,      0x017D, 0,      0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
                            0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178};

struct Piece {
  bool conv = false;
  char cls = 0;      // 's' text, 'c' one byte, 'n' a number
  std::string text;  // the literal, or the conversion's spec ("%5d")
};
struct Pattern {
  std::vector<Piece> key, val;
  size_t lit = 0;  // literal bytes of the key
};
struct Pack {
  std::unordered_map<std::string, std::string> exact;
  std::unordered_map<std::string, std::string> exact_flat;  // the exact keys flattened (flat): a wrapped text's lookup
  std::vector<Pattern> pats;                                    // most literal bytes first
  std::unordered_map<unsigned char, std::vector<size_t>> first;  // patterns by their first literal byte
  std::vector<size_t> open;                                      // patterns that start with a conversion
};

std::mutex g_mu;
std::unique_ptr<Pack> g_pack[kCount];
std::atomic<int> g_lang{kEnglish};
// tr's results per language: hit flag + text (cleared at a pack load or past kCacheMax)
std::unordered_map<std::string, std::pair<bool, std::string>> g_cache[kCount];
constexpr size_t kCacheMax = 200000;

// the redirected texts of one context: a host block mapped at kLangVa
struct Arena {
  std::vector<uint8_t> buf;
  uint32_t used = 0;
  int lang = -1;
  std::unordered_map<std::string, uint32_t> at;  // translation -> its address
};
std::unordered_map<const Ctx *, std::unique_ptr<Arena>> g_arena;

bool is_conv_type(char c) { return std::strchr("diouxXcsfeEgG", c) != nullptr; }

// split a printf format into literals and conversions; false on a lone '%' that starts no conversion
bool split(const std::string &f, std::vector<Piece> &out) {
  out.clear();
  std::string lit;
  for (size_t i = 0; i < f.size();) {
    if (f[i] != '%') {
      lit += f[i++];
      continue;
    }
    if (i + 1 < f.size() && f[i + 1] == '%') {
      lit += '%';
      i += 2;
      continue;
    }
    size_t j = i + 1;
    while (j < f.size() && std::strchr("-+ #0", f[j])) ++j;
    while (j < f.size() && f[j] >= '0' && f[j] <= '9') ++j;
    if (j < f.size() && f[j] == '.') {
      ++j;
      while (j < f.size() && f[j] >= '0' && f[j] <= '9') ++j;
    }
    while (j < f.size() && (f[j] == 'l' || f[j] == 'h')) ++j;
    if (j >= f.size() || !is_conv_type(f[j])) return false;
    if (!lit.empty()) out.push_back({false, 0, lit});
    lit.clear();
    const char t = f[j];
    out.push_back({true, t == 's' ? 's' : t == 'c' ? 'c' : 'n', f.substr(i, j + 1 - i)});
    i = j + 1;
  }
  if (!lit.empty()) out.push_back({false, 0, lit});
  return true;
}

bool digit(char c) { return c >= '0' && c <= '9'; }

// P24L: a %s part may be empty (set_empty_captures): the sim leaves optional clauses out (hole opened: 'a 285 yard par 4' has
// no slope word between 'yard ' and 'par', no dogleg clause before ', is now'), so a key written from the code with %s in
// their place must match the text without them
bool g_empty_caps = true;

bool match(const std::vector<Piece> &k, size_t pi, const std::string &s, size_t si, std::vector<std::string> &caps) {
  if (pi == k.size()) return si == s.size();
  const Piece &p = k[pi];
  if (!p.conv) {
    if (s.compare(si, p.text.size(), p.text) != 0) return false;
    return match(k, pi + 1, s, si + p.text.size(), caps);
  }
  if (p.cls == 'c') {
    if (si >= s.size()) return false;
    caps.push_back(s.substr(si, 1));
    if (match(k, pi + 1, s, si + 1, caps)) return true;
    caps.pop_back();
    return false;
  }
  if (p.cls == 'n') {
    size_t j = si;
    if (j < s.size() && s[j] == '-') ++j;
    if (j >= s.size() || !digit(s[j])) return false;
    std::vector<size_t> ends;
    while (j < s.size()) {
      if (digit(s[j])) {
        ++j;
        ends.push_back(j);
      } else if ((s[j] == ',' || s[j] == '.') && j + 1 < s.size() && digit(s[j + 1])) {
        ++j;
      } else {
        break;
      }
    }
    for (size_t e = ends.size(); e-- > 0;) {  // longest first
      caps.push_back(s.substr(si, ends[e] - si));
      if (match(k, pi + 1, s, ends[e], caps)) return true;
      caps.pop_back();
    }
    return false;
  }
  // 's': shortest first (empty allowed unless set_empty_captures(false)); a literal next is searched for directly
  const Piece *next = pi + 1 < k.size() ? &k[pi + 1] : nullptr;
  if (!next) {
    if (si >= s.size() && !g_empty_caps) return false;
    caps.push_back(s.substr(si));
    if (match(k, pi + 1, s, s.size(), caps)) return true;
    caps.pop_back();
    return false;
  }
  for (size_t e = g_empty_caps ? si : si + 1; e <= s.size(); ++e) {
    if (!next->conv) {
      const size_t f = s.find(next->text, e);
      if (f == std::string::npos) return false;
      e = f;
    }
    caps.push_back(s.substr(si, e - si));
    if (match(k, pi + 1, s, e, caps)) return true;
    caps.pop_back();
  }
  return false;
}

std::string unescape(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\' && i + 1 < s.size()) {
      const char c = s[++i];
      o += c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : c;
    } else {
      o += s[i];
    }
  }
  return o;
}
std::string escape(const std::string &s) {
  std::string o;
  for (char c : s) {
    if (c == '\n') o += "\\n";
    else if (c == '\t') o += "\\t";
    else if (c == '\r') o += "\\r";
    else if (c == '\\') o += "\\\\";
    else o += c;
  }
  return o;
}

// trailing spaces / tabs / line ends: not part of a key (the drawers add some: a theme text's tip has one more than its file line)
size_t core_len(const std::string &s) {
  size_t n = s.size();
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) --n;
  return n;
}

// how many levels of %s parts are translated inside a pattern's match (lookup_core; set_capture_depth, g_mu held to read)
int g_cap_depth = kCaptureDepth;
bool g_lead_blanks = true;  // P24K (set_leading_blanks)
bool lookup_core(const Pack &p, const std::string &s, std::string &out, int depth = 0);
// a text with its line breaks and '^' marks as spaces, trailing blanks dropped: wrapMessageBuffer (commands.cpp) turns the space
// at each break into LF, and a '^' into a space with LF after it, in g_textBuffer before the drawers read it
std::string flat(const std::string &s) {
  std::string f = s;
  for (char &c : f)
    if (c == '\n' || c == '^') c = ' ';
  f.resize(core_len(f));
  return f;
}
// the lookup of one text (g_mu held; lookup below adds the leading blanks): the text without its trailing blanks, which are
// put back after the translation; a text with a line break and no line of its own is looked up flattened (the translation then
// has no breaks: the drawer wraps it again)
bool lookup_text(const Pack &p, const std::string &s, std::string &out);
// P24K: leading blanks too. showPopup (commands_popup.inc draw_popup_pass) draws each option line by itself, and an option line
// starts with a space (that space is what marks it as an option), so " <option>" reaches the drawer. A key written with its
// leading blanks wins (the story replies of batch 8b); otherwise the text without them is looked up and they are put back.
bool lookup(const Pack &p, const std::string &s, std::string &out) {
  if (lookup_text(p, s, out)) return true;
  if (!g_lead_blanks) return false;
  size_t lead = 0;
  while (lead < s.size() && (s[lead] == ' ' || s[lead] == '\t')) ++lead;
  if (lead == 0 || lead >= core_len(s) || !lookup_text(p, s.substr(lead), out)) return false;
  out = s.substr(0, lead) + out;
  return true;
}
bool lookup_text(const Pack &p, const std::string &s, std::string &out) {
  const size_t n = core_len(s);
  const std::string core = s.substr(0, n);
  if (s.find('\n') != std::string::npos || s.find('^') != std::string::npos) {
    // a wrapped text: its own line first (a key that holds the breaks), then flattened (exact, then patterns), and only then
    // the patterns over the text as it is (a pattern ending in "%s looks on in amazement." would otherwise take the first lines
    // of a wrapped narration as its %s)
    auto x = p.exact.find(core);
    if (x != p.exact.end()) {
      out = x->second + s.substr(n);
      return true;
    }
    const std::string f = flat(s);
    auto e = p.exact_flat.find(f);
    if (e != p.exact_flat.end()) {
      out = e->second;
      return true;
    }
    if (lookup_core(p, f, out)) return true;
  }
  if (!lookup_core(p, core, out)) return false;
  out += s.substr(n);
  return true;
}
bool lookup_core(const Pack &p, const std::string &s, std::string &out, int depth) {
  auto e = p.exact.find(s);
  if (e != p.exact.end()) {
    out = e->second;
    return true;
  }
  if (p.pats.empty() || s.empty()) return false;
  const Pattern *best = nullptr;
  std::vector<std::string> caps, best_caps;
  auto f = p.first.find(static_cast<unsigned char>(s[0]));
  auto consider = [&](size_t i) {
    const Pattern &pt = p.pats[i];
    if (best && pt.lit <= best->lit) return;
    caps.clear();
    if (match(pt.key, 0, s, 0, caps)) {
      best = &pt;
      best_caps = caps;
    }
  };
  if (f != p.first.end())
    for (size_t i : f->second) consider(i);
  for (size_t i : p.open) consider(i);
  if (!best) return false;
  out.clear();
  size_t c = 0;
  for (const Piece &v : best->val) {
    if (!v.conv) {
      out += v.text;
    } else if (c < best_caps.size()) {
      // a %s part that has a translation of its own is translated too ("Club: %s" over "Club: Driver", "... at %s!" over
      // "Hole 5"), three levels deep at most (P24J: the tile inside a quoted thought inside a comment tip is the third); a part
      // is looked up only when it is shorter than the text holding it, so a level can never see the same text again
      // P24K: a part with blanks around it (a word with its trailing space before a number, a clause with its leading space
      // after one) is looked up as it is first, then without them (keys never end in a blank: load_pack trims them), and the
      // blanks are put back
      const std::string &cap = best_caps[c];
      std::string sub;
      bool done = false;
      if (v.cls == 's' && depth < g_cap_depth && cap.size() < s.size()) {
        done = lookup_core(p, cap, sub, depth + 1);
        if (!done && g_lead_blanks) {
          size_t a = 0;
          while (a < cap.size() && (cap[a] == ' ' || cap[a] == '\t')) ++a;
          const size_t b = core_len(cap);
          if (b > a && (a > 0 || b < cap.size()) && lookup_core(p, cap.substr(a, b - a), sub, depth + 1)) {
            sub = cap.substr(0, a) + sub + cap.substr(b);
            done = true;
          }
        }
      }
      out += done ? sub : cap;
      ++c;
    }
  }
  return true;
}

FILE *dump_file() {
  static FILE *f = nullptr;
  static bool tried = false;
  if (!tried) {
    tried = true;
    const char *p = std::getenv("SG_LANG_DUMP");
    if (p && *p) f = std::fopen(p, "ab");
  }
  return f;
}

}  // namespace

std::string utf8_to_cp1252(const std::string &u, bool *lossy) {
  std::string o;
  if (lossy) *lossy = false;
  for (size_t i = 0; i < u.size();) {
    const unsigned char c = static_cast<unsigned char>(u[i]);
    uint32_t cp = c;
    size_t n = 1;
    if (c >= 0xF0 && i + 3 < u.size()) {
      cp = ((c & 7u) << 18) | ((u[i + 1] & 0x3Fu) << 12) | ((u[i + 2] & 0x3Fu) << 6) | (u[i + 3] & 0x3Fu);
      n = 4;
    } else if (c >= 0xE0 && i + 2 < u.size()) {
      cp = ((c & 0xFu) << 12) | ((u[i + 1] & 0x3Fu) << 6) | (u[i + 2] & 0x3Fu);
      n = 3;
    } else if (c >= 0xC0 && i + 1 < u.size()) {
      cp = ((c & 0x1Fu) << 6) | (u[i + 1] & 0x3Fu);
      n = 2;
    }
    i += n;
    if (cp < 0x80 || (cp >= 0xA0 && cp < 0x100)) {
      o += static_cast<char>(cp);
      continue;
    }
    int b = -1;
    for (int k = 0; k < 32; ++k)
      if (kHigh[k] == cp || (kHigh[k] == 0 && cp == 0x80u + static_cast<uint32_t>(k))) b = 0x80 + k;
    if (b < 0) {
      if (lossy) *lossy = true;
      o += '?';
    } else {
      o += static_cast<char>(b);
    }
  }
  return o;
}

std::string cp1252_to_utf8(const std::string &s) {
  std::string o;
  for (char ch : s) {
    const unsigned char c = static_cast<unsigned char>(ch);
    uint32_t cp = c;
    if (c >= 0x80 && c < 0xA0 && kHigh[c - 0x80]) cp = kHigh[c - 0x80];
    if (cp < 0x80) {
      o += static_cast<char>(cp);
    } else if (cp < 0x800) {
      o += static_cast<char>(0xC0 | (cp >> 6));
      o += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      o += static_cast<char>(0xE0 | (cp >> 12));
      o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      o += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }
  return o;
}

std::vector<std::string> conversions(const std::string &fmt) {
  std::vector<Piece> p;
  std::vector<std::string> out;
  if (!split(fmt, p)) return out;
  for (const Piece &x : p)
    if (x.conv) out.push_back(x.text);
  return out;
}

int load_pack(int id, const std::string &utf8, std::vector<std::string> *errors) {
  if (id <= kEnglish || id >= kCount) return 0;
  auto pack = std::make_unique<Pack>();
  size_t line_no = 0;
  for (size_t pos = 0; pos < utf8.size();) {
    size_t eol = utf8.find('\n', pos);
    if (eol == std::string::npos) eol = utf8.size();
    std::string line = utf8.substr(pos, eol - pos);
    pos = eol + 1;
    ++line_no;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line_no == 1 && line.size() >= 3 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);  // a UTF-8 BOM
    if (line.empty() || line[0] == '#') continue;
    const size_t tab = line.find('\t');
    auto refuse = [&](const char *why) {
      if (errors) errors->push_back("line " + std::to_string(line_no) + ": " + why);
    };
    if (tab == std::string::npos || tab == 0 || tab + 1 >= line.size()) {
      refuse("not `english<TAB>translation`");
      continue;
    }
    bool lossy = false;
    std::string en = utf8_to_cp1252(unescape(line.substr(0, tab)), &lossy);
    std::string tx = utf8_to_cp1252(unescape(line.substr(tab + 1)), &lossy);
    en.resize(core_len(en));  // trailing blanks are not part of a key (lookup puts the drawn text's own back)
    tx.resize(core_len(tx));
    if (lossy) {
      refuse("a character code page 1252 has not");
      continue;
    }
    Pattern pt;
    if (!split(en, pt.key) || !split(tx, pt.val)) {
      refuse("a '%' that starts no conversion (write %% for a percent sign)");
      continue;
    }
    std::vector<std::string> ck, cv;
    for (const Piece &x : pt.key)
      if (x.conv) ck.push_back(x.text);
    for (const Piece &x : pt.val)
      if (x.conv) cv.push_back(x.text);
    if (ck != cv) {
      refuse("the translation's conversions differ from the English ones (same ones, same order)");
      continue;
    }
    if (ck.empty()) {
      // no conversion: the bytes as written, with %% read as one percent sign
      std::string k, v;
      for (const Piece &x : pt.key) k += x.text;
      for (const Piece &x : pt.val) v += x.text;
      pack->exact[k] = v;
      pack->exact_flat[flat(k)] = v;
      continue;
    }
    for (const Piece &x : pt.key)
      if (!x.conv) pt.lit += x.text.size();
    pack->pats.push_back(std::move(pt));
  }
  std::stable_sort(pack->pats.begin(), pack->pats.end(), [](const Pattern &a, const Pattern &b) { return a.lit > b.lit; });
  for (size_t i = 0; i < pack->pats.size(); ++i) {
    const Piece &p0 = pack->pats[i].key.front();
    if (p0.conv) pack->open.push_back(i);
    else pack->first[static_cast<unsigned char>(p0.text[0])].push_back(i);
  }
  const int n = static_cast<int>(pack->exact.size() + pack->pats.size());
  std::lock_guard<std::mutex> lk(g_mu);
  g_pack[id] = std::move(pack);
  g_cache[id].clear();
  for (auto &a : g_arena) a.second->lang = -1;  // its texts are the old pack's
  return n;
}

bool has_pack(int id) {
  std::lock_guard<std::mutex> lk(g_mu);
  return id > kEnglish && id < kCount && g_pack[id] != nullptr;
}

void set_capture_depth(int levels) {
  std::lock_guard<std::mutex> lk(g_mu);
  g_cap_depth = levels < 0 ? 0 : levels;
  for (auto &c : g_cache) c.clear();
  for (auto &a : g_arena) a.second->lang = -1;
}

void set_leading_blanks(bool on) {
  std::lock_guard<std::mutex> lk(g_mu);
  g_lead_blanks = on;
  for (auto &c : g_cache) c.clear();
  for (auto &a : g_arena) a.second->lang = -1;
}

void set_empty_captures(bool on) {
  std::lock_guard<std::mutex> lk(g_mu);
  g_empty_caps = on;
  for (auto &c : g_cache) c.clear();
  for (auto &a : g_arena) a.second->lang = -1;
}

void set_language(int id) {
  if (id == kEnglish || has_pack(id)) g_lang.store(id);
}
int language() { return g_lang.load(); }

void note(const char *where, const std::string &s, bool hit) {
  FILE *f = dump_file();
  if (!f || s.empty()) return;
  bool letter = false;
  for (char c : s)
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) letter = true;
  if (!letter) return;
  static std::unordered_set<std::string> seen;
  std::string key = std::string(where) + '\t' + s;
  if (!seen.insert(key).second) return;
  const std::string line = std::string(hit ? "hit" : "miss") + '\t' + where + '\t' + escape(cp1252_to_utf8(s)) + '\n';
  std::fwrite(line.data(), 1, line.size(), f);
  std::fflush(f);
}

namespace {
// tr with g_mu held; `hit` tells whether a translation was found
std::string tr_locked(int id, const std::string &s, bool &hit) {
  hit = false;
  if (id == kEnglish || !g_pack[id]) return s;
  auto &cache = g_cache[id];
  auto c = cache.find(s);
  if (c != cache.end()) {
    hit = c->second.first;
    return hit ? c->second.second : s;
  }
  std::string out;
  hit = lookup(*g_pack[id], s, out);
  if (cache.size() >= kCacheMax) cache.clear();
  cache.emplace(s, std::make_pair(hit, hit ? out : std::string()));
  return hit ? out : s;
}
}  // namespace

std::string tr(const std::string &s) {
  const int id = g_lang.load();
  if (id == kEnglish && !dump_file()) return s;
  std::lock_guard<std::mutex> lk(g_mu);
  bool hit = false;
  std::string out = tr_locked(id, s, hit);
  note("tr", s, hit);
  return out;
}

uint32_t redirect(Ctx &ctx, uint32_t va) {
  const int id = g_lang.load();
  if (va == 0 || (id == kEnglish && !dump_file())) return va;
  if (va >= kLangVa && va < kLangVa + kLangSize) return va;  // already a translation
  std::string s;
  try {
    const uint32_t n = vstrlen(ctx.mem, va);
    if (n) s.assign(reinterpret_cast<const char *>(ctx.mem.at(va, n)), n);
  } catch (...) {
    return va;
  }
  std::lock_guard<std::mutex> lk(g_mu);
  bool hit = false;
  std::string out = tr_locked(id, s, hit);
  note("va", s, hit);
  if (!hit || out == s) return va;
  std::unique_ptr<Arena> &ap = g_arena[&ctx];
  if (!ap) ap = std::make_unique<Arena>();
  Arena &a = *ap;
  if (a.buf.empty()) a.buf.assign(kLangSize, 0);
  bool mapped = true;
  try {
    mapped = ctx.mem.at(kLangVa, 1) == a.buf.data();
  } catch (...) {
    mapped = false;
  }
  if (!mapped && !ctx.mem.map(kLangVa, a.buf.data(), kLangSize, false)) return va;  // the range is taken: English
  if (a.lang != id) {
    a.lang = id;
    a.at.clear();
    a.used = 0;
  }
  auto f = a.at.find(out);
  if (f != a.at.end()) return f->second;
  const uint32_t need = static_cast<uint32_t>(out.size()) + 1;
  if (need > kLangSize) return va;
  if (a.used + need > kLangSize) {  // full: start over (a fetch uses its text before the next 4 MB of new texts)
    a.used = 0;
    a.at.clear();
  }
  std::memcpy(a.buf.data() + a.used, out.c_str(), need);
  const uint32_t at = kLangVa + a.used;
  a.used += need;
  a.at.emplace(out, at);
  return at;
}

}  // namespace lang
}  // namespace portsim
