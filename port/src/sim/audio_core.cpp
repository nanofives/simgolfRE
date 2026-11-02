// The audio core (step S4.2, log/roadmap/S4/2_notes.md; API in portsim/sim/audio.h, audio_sink.h, soundstate.h):
// sound.dll's image in the context's address space, the call dispatcher, the hosts (C callback, recorded script), the
// AudioSink route, and the first family of sound.dll functions (lists, nodes, small helpers; part 2 of this file).
//
// Negative-control modes (sg_negctl_set(<name>, mode)); every function reads its own switch:
//   1  a wrong store or a wrong return in the body (each function's comment says which)
//   (the modes are listed per family in part 2)
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "portsim/gamefiles.h"
#include "portsim/ptr.h"
#include "portsim/registry.h"
#include "portsim/registry_list.h"
#include "portsim/sim/audio.h"
#include "portsim/sim/audio_exe.h"
#include "portsim/sim/audio_sink.h"
#include "portsim/sim/soundstate.h"
#include "portsim/state_hash.h"

namespace portsim {

const char *audio_crt_name(uint32_t rva);  // part 2
const char *audio_crt_function_name(uint32_t rva);  // S4.11: audio_crt.cpp
const char *audio_exe_sound_name(uint32_t rva);  // audio_exe.cpp (S4.10)

// ================================================================================================ part 0: the x87 unit
// (moved here from S4.4's audio_mixer.cpp by S4.2b: the core FPU model every module computes on; API in audio.h)
namespace x87 {

#include "audio_x80.inc"

namespace {
bool g_soft = false;

#if defined(_M_IX86)
// One x87 instruction on tbyte operands under the unit's word (exceptions kept masked; the sticky flags it raises are
// cleared again, and the thread's own word is put back).
struct T10 {
  unsigned char b[10];
};
T10 t10(X80 x) {
  T10 t;
  std::memcpy(t.b, &x.m, 8);
  std::memcpy(t.b + 8, &x.se, 2);
  return t;
}
X80 x80(const T10 &t) {
  X80 x;
  std::memcpy(&x.m, t.b, 8);
  std::memcpy(&x.se, t.b + 8, 2);
  return x;
}
uint16_t masked(uint16_t cw) {
  if ((cw & 0x3f) != 0x3f) throw X87Unmodelled{"x87: an unmasked exception in the control word"};
  return cw;
}
X80 hw_add(X80 a, X80 b, uint16_t cw) {
  T10 ta = t10(a), tb = t10(b), tr;
  uint16_t w = masked(cw), save;
  __asm {
    fnstcw save
    fldcw w
    fld tbyte ptr tb
    fld tbyte ptr ta
    fadd st(0), st(1)
    fstp tbyte ptr tr
    fstp st(0)
    fnclex
    fldcw save
  }
  return x80(tr);
}
X80 hw_sub(X80 a, X80 b, uint16_t cw) {
  T10 ta = t10(a), tb = t10(b), tr;
  uint16_t w = masked(cw), save;
  __asm {
    fnstcw save
    fldcw w
    fld tbyte ptr tb
    fld tbyte ptr ta
    fsub st(0), st(1)
    fstp tbyte ptr tr
    fstp st(0)
    fnclex
    fldcw save
  }
  return x80(tr);
}
X80 hw_mul(X80 a, X80 b, uint16_t cw) {
  T10 ta = t10(a), tb = t10(b), tr;
  uint16_t w = masked(cw), save;
  __asm {
    fnstcw save
    fldcw w
    fld tbyte ptr tb
    fld tbyte ptr ta
    fmul st(0), st(1)
    fstp tbyte ptr tr
    fstp st(0)
    fnclex
    fldcw save
  }
  return x80(tr);
}
X80 hw_div(X80 a, X80 b, uint16_t cw) {
  T10 ta = t10(a), tb = t10(b), tr;
  uint16_t w = masked(cw), save;
  __asm {
    fnstcw save
    fldcw w
    fld tbyte ptr tb
    fld tbyte ptr ta
    fdiv st(0), st(1)
    fstp tbyte ptr tr
    fstp st(0)
    fnclex
    fldcw save
  }
  return x80(tr);
}
uint64_t hw_fst64(X80 a, uint16_t cw) {
  T10 ta = t10(a);
  uint64_t r;
  uint16_t w = masked(cw), save;
  __asm {
    fnstcw save
    fldcw w
    fld tbyte ptr ta
    fstp qword ptr r
    fnclex
    fldcw save
  }
  return r;
}
int64_t hw_fist(X80 a, int bits, uint16_t cw) {
  T10 ta = t10(a);
  int64_t q = 0;
  int32_t d = 0;
  int16_t h = 0;
  uint16_t w = masked(cw), save;
  if (bits == 64) {
    __asm {
      fnstcw save
      fldcw w
      fld tbyte ptr ta
      fistp qword ptr q
      fnclex
      fldcw save
    }
    return q;
  }
  if (bits == 32) {
    __asm {
      fnstcw save
      fldcw w
      fld tbyte ptr ta
      fistp dword ptr d
      fnclex
      fldcw save
    }
    return d;
  }
  __asm {
    fnstcw save
    fldcw w
    fld tbyte ptr ta
    fistp word ptr h
    fnclex
    fldcw save
  }
  return h;
}
uint16_t hw_fcom(X80 a, X80 b) {
  T10 ta = t10(a), tb = t10(b);
  uint16_t s;
  __asm {
    fld tbyte ptr tb
    fld tbyte ptr ta
    fcompp
    fnstsw s
    fnclex
  }
  return static_cast<uint16_t>(s & (kC0 | kC1 | kC2 | kC3));
}
#endif
}  // namespace

void set_soft(bool soft) { g_soft = soft; }
bool soft() {
#if defined(_M_IX86)
  return g_soft;
#else
  return true;
#endif
}

#if defined(_M_IX86)
#define SG_X87_OP(hw, sw_) return g_soft ? sw_ : hw
#else
#define SG_X87_OP(hw, sw_) return sw_
#endif
X80 add(X80 a, X80 b, uint16_t cw) { SG_X87_OP(hw_add(a, b, cw), sw::add(a, b, cw)); }
X80 sub(X80 a, X80 b, uint16_t cw) { SG_X87_OP(hw_sub(a, b, cw), sw::sub(a, b, cw)); }
X80 mul(X80 a, X80 b, uint16_t cw) { SG_X87_OP(hw_mul(a, b, cw), sw::mul(a, b, cw)); }
X80 div(X80 a, X80 b, uint16_t cw) { SG_X87_OP(hw_div(a, b, cw), sw::div(a, b, cw)); }
uint64_t fst64(X80 a, uint16_t cw) { SG_X87_OP(hw_fst64(a, cw), sw::fst64(a, cw)); }
int64_t fist(X80 a, int bits, uint16_t cw) { SG_X87_OP(hw_fist(a, bits, cw), sw::fist(a, bits, cw)); }
uint16_t fcom(X80 a, X80 b) { SG_X87_OP(hw_fcom(a, b), sw::fcom(a, b)); }
#undef SG_X87_OP

// A single as the double it widens to exactly (sign, exponent rebiased, significand shifted; a subnormal normalised; NaN
// payloads kept with the quiet bit where it was): what fld dword / fop dword read.
namespace {
uint64_t f32_to_f64_bits(uint32_t b) {
  const uint64_t s = static_cast<uint64_t>(b >> 31) << 63;
  const uint32_t e = (b >> 23) & 0xffu, f = b & ((1u << 23) - 1u);
  if (e == 0xffu) return s | (0x7ffull << 52) | (static_cast<uint64_t>(f) << 29);
  if (e == 0) {
    if (f == 0) return s;
    int sh = 0;
    uint32_t g = f;
    while (!(g & (1u << 23))) {
      g <<= 1;
      ++sh;
    }
    return s | (static_cast<uint64_t>(1023 - 126 - sh) << 52) | (static_cast<uint64_t>(g & ((1u << 23) - 1u)) << 29);
  }
  return s | (static_cast<uint64_t>(e - 127 + 1023) << 52) | (static_cast<uint64_t>(f) << 29);
}
}  // namespace
X80 from_f32(uint32_t bits) { return from_f64(f32_to_f64_bits(bits)); }
X80 fld32(uint32_t bits) { return fld64(f32_to_f64_bits(bits)); }
X80 ln2() {
  X80 r;
  r.m = 0xb17217f7d1cf79acull;
  r.se = 0x3ffe;
  return r;
}
double to_double(X80 a, uint16_t cw) {
  const uint64_t b = sw::fst64(a, cw);
  double d;
  std::memcpy(&d, &b, 8);
  return d;
}
X80 fyl2x(X80 st0, X80 st1, uint16_t cw) {
  const double x = to_double(st0, cw);
  if (!(x > 0.0)) throw X87Unmodelled{"x87: fyl2x of an operand <= 0 (the emulator's other arm is not modelled)"};
  double t = std::log(x) / std::log(2.0);
  t *= to_double(st1, cw);
  uint64_t b;
  std::memcpy(&b, &t, 8);
  return from_f64(b);
}

// ---- the unit
int Unit::depth() const {
  int n = 0;
  for (int i = 0; i < 8; ++i) n += (used >> i) & 1;
  return n;
}
X80 &Unit::st(int i) {
  const int p = (top() + i) & 7;
  if (!((used >> p) & 1)) throw X87Unmodelled{"x87: st(" + std::to_string(i) + ") is empty (stack underflow)"};
  return reg[p];
}
void Unit::push(X80 v) {
  const int p = (top() - 1) & 7;
  if ((used >> p) & 1) throw X87Unmodelled{"x87: push onto a full register stack (stack overflow)"};
  set_top(p);
  reg[p] = v;
  used = static_cast<uint8_t>(used | (1u << p));
}
X80 Unit::pop() {
  const int p = top();
  if (!((used >> p) & 1)) throw X87Unmodelled{"x87: pop of an empty st(0) (stack underflow)"};
  const X80 v = reg[p];
  used = static_cast<uint8_t>(used & ~(1u << p));
  set_top(p + 1);
  return v;
}
void Unit::fxch(int i) {
  X80 &a = st(0), &b = st(i);
  std::swap(a, b);
}
void Unit::fninit() {
  cw = 0x037f;
  sw = 0;
  used = 0;
  cc_unknown = false;
}
void Unit::mmx_enter() {
  set_top(0);
  used = 0xff;
}
void Unit::emms() { used = 0; }

uint16_t Unit::tag_word() const {
  uint16_t t = 0;
  for (int i = 7; i >= 0; --i) {
    t = static_cast<uint16_t>(t << 2);
    if (!((used >> i) & 1)) {
      t |= 3;
      continue;
    }
    const X80 &x = reg[i];
    const int ex = x.se & 0x7fff;
    if (ex == 0 && x.m == 0) t |= 1;
    else if (ex == 0 || ex == 0x7fff || !(x.m >> 63)) t |= 2;
  }
  return t;
}

void Unit::load_image(const uint8_t *p) {
  if (std::memcmp(p, "SX87", 4) != 0) throw MissingResource{"x87 image: bad header"};
  std::memcpy(&cw, p + 4, 2);
  std::memcpy(&sw, p + 6, 2);
  uint16_t tag, flags;
  std::memcpy(&tag, p + 8, 2);
  std::memcpy(&flags, p + 10, 2);
  cc_unknown = (flags & 1u) != 0;
  sw = static_cast<uint16_t>(sw & (0x3800u | kC0 | kC1 | kC2 | kC3));
  used = 0;
  for (int i = 0; i < 8; ++i) {
    if (((tag >> (2 * i)) & 3) != 3) used = static_cast<uint8_t>(used | (1u << i));
    std::memcpy(&reg[i].m, p + 12 + 10 * i, 8);
    std::memcpy(&reg[i].se, p + 12 + 10 * i + 8, 2);
  }
}
void Unit::save_image(uint8_t *p) const {
  std::memcpy(p, "SX87", 4);
  std::memcpy(p + 4, &cw, 2);
  std::memcpy(p + 6, &sw, 2);
  const uint16_t tag = tag_word(), flags = cc_unknown ? 1 : 0;
  std::memcpy(p + 8, &tag, 2);
  std::memcpy(p + 10, &flags, 2);
  for (int i = 0; i < 8; ++i) {
    std::memcpy(p + 12 + 10 * i, &reg[i].m, 8);
    std::memcpy(p + 12 + 10 * i + 8, &reg[i].se, 2);
  }
}

namespace {
struct FpuCtx {
  Unit unit;
  std::string detail;  // why the last body stopped (X87Unmodelled)
};
std::mutex g_fpu_mu;
std::map<const Ctx *, std::unique_ptr<FpuCtx>> &fpu_table() {
  static std::map<const Ctx *, std::unique_ptr<FpuCtx>> t;
  return t;
}
FpuCtx &fctx(const Ctx &c) {
  std::lock_guard<std::mutex> lk(g_fpu_mu);
  auto &p = fpu_table()[&c];
  if (!p) p = std::make_unique<FpuCtx>();
  return *p;
}
}  // namespace

Unit &unit(Ctx &ctx) { return fctx(ctx).unit; }
void set_detail(Ctx &ctx, const std::string &what) { fctx(ctx).detail = what; }
const std::string &detail(Ctx &ctx) { return fctx(ctx).detail; }
void unit_release_(const Ctx &ctx) {
  std::lock_guard<std::mutex> lk(g_fpu_mu);
  fpu_table().erase(&ctx);
}

}  // namespace x87

// ================================================================================================ part 1: the core
namespace {

// Interface objects of a sink: a block mapped per context, and the range of addresses its vtables point into.
constexpr uint32_t kSinkObjBase = 0xFEC00000u, kSinkObjSize = 0x10000u;
constexpr uint32_t kSinkTrapBase = 0xFED00000u, kSinkTrapSize = 0x100000u;  // iface * 0x1000 + slot * 4

class CallbackHost : public SoundHost {
 public:
  SndHostFn fn = nullptr;
  void *user = nullptr;
  void call(Ctx &, SndCallC &c) override {
    c.status = 0;
    const int r = fn(user, &c);
    if (r != 0 || c.status != 0)
      throw SoundHostError{"host callback refused the call to va " + std::to_string(c.va) + " (status " +
                           std::to_string(r != 0 ? r : c.status) + ")"};
  }
};

// A recorded transcript of host calls ($audio/script): each entry is the request the port must make next and the reply
// with the memory the host wrote (and the blocks it mapped) while it served it.
struct ScriptEntry {
  SndCallC req{};
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> writes;
  std::vector<std::pair<uint32_t, std::vector<uint8_t>>> maps;
};

class ScriptHost : public SoundHost {
 public:
  std::vector<ScriptEntry> entries;
  size_t pos = 0;
  std::vector<std::unique_ptr<std::vector<uint8_t>>> *owned = nullptr;
  void call(Ctx &ctx, SndCallC &c) override {
    if (pos >= entries.size())
      throw SoundHostError{"script exhausted: an extra call to va " + std::to_string(c.va) + " kind " +
                           std::to_string(c.kind)};
    const ScriptEntry &e = entries[pos];
    const SndCallC &q = e.req;
    bool same = q.va == c.va && q.kind == c.kind && q.abi == c.abi && q.nargs == c.nargs && q.want_st0 == c.want_st0;
    for (int i = 0; same && i < c.nargs; ++i) same = q.args[i] == c.args[i];
    if (!same) {
      std::string got, want;
      for (int i = 0; i < c.nargs; ++i) got += " " + std::to_string(c.args[i]);
      for (int i = 0; i < q.nargs; ++i) want += " " + std::to_string(q.args[i]);
      throw SoundHostError{"script entry " + std::to_string(pos) + ": port called va " + std::to_string(c.va) +
                           " kind " + std::to_string(c.kind) + " abi " + std::to_string(c.abi) + " args" + got +
                           "; recorded va " + std::to_string(q.va) + " kind " + std::to_string(q.kind) + " abi " +
                           std::to_string(q.abi) + " args" + want};
    }
    for (const auto &m : e.maps) {
      owned->push_back(std::make_unique<std::vector<uint8_t>>(m.second));
      if (!ctx.mem.map(m.first, owned->back()->data(), static_cast<uint32_t>(m.second.size()), false))
        throw SoundHostError{"script entry " + std::to_string(pos) + ": cannot map the block at " + std::to_string(m.first)};
    }
    for (const auto &w : e.writes) std::memcpy(ctx.mem.at(w.first, static_cast<uint32_t>(w.second.size()), true),
                                               w.second.data(), w.second.size());
    c.eax = q.eax;
    c.edx = q.edx;
    c.st0 = q.st0;
    c.has_st0 = q.has_st0;
    ++pos;
  }
};

struct AudioCtx {
  bool attached = false;
  uint32_t base = 0;
  bool own_image = false;
  std::vector<uint8_t> image;  // the image mapped from GameFiles (map_image)
  SoundHost *host = nullptr;
  CallbackHost callback;
  ScriptHost script;
  std::vector<std::unique_ptr<std::vector<uint8_t>>> script_blocks;
  std::set<uint32_t> claims;
  AudioSink *sink = nullptr;
  std::unique_ptr<RecordingSink> owned_sink;
  std::vector<uint8_t> sink_block;  // interface objects (audio_sink_object)
  uint32_t sink_used = 0;
  std::multimap<uint64_t, uint32_t> sink_free;  // P3: returned objects, keyed by iface << 32 | nslots
  AudioCounters counters;
  std::string detail;
  std::map<uint32_t, const FuncDef *> port_cache;  // rva -> registered port function (nullptr = none)
  // the stack (audio.h): the running port function's entry esp and esp (0 = none), the lowest address a frame may use
  uint32_t entry = 0, esp = 0, esp_limit = 0;
  std::vector<uint8_t> frame_block;  // the frame block, when the core mapped it
  bool residue = false;              // a SndResidue scope is open for the next port call (audio.h)
};

std::mutex g_mu;
std::map<const Ctx *, std::unique_ptr<AudioCtx>> &table() {
  static std::map<const Ctx *, std::unique_ptr<AudioCtx>> t;
  return t;
}
AudioCtx &actx(const Ctx &c) {
  std::lock_guard<std::mutex> lk(g_mu);
  auto &p = table()[&c];
  if (!p) p = std::make_unique<AudioCtx>();
  return *p;
}
AudioCtx &attached(const Ctx &c) {
  AudioCtx &a = actx(c);
  if (!a.attached) throw MissingResource{"sound.dll image (audio_attach)"};
  return a;
}

uint32_t rd32(const std::vector<uint8_t> &b, size_t o) {
  if (o + 4 > b.size()) throw MissingResource{"sound.dll: truncated file"};
  uint32_t v;
  std::memcpy(&v, b.data() + o, 4);
  return v;
}
uint16_t rd16(const std::vector<uint8_t> &b, size_t o) {
  if (o + 2 > b.size()) throw MissingResource{"sound.dll: truncated file"};
  uint16_t v;
  std::memcpy(&v, b.data() + o, 2);
  return v;
}

// Map the user's sound.dll as the loader maps it, relocated to `base`, after checking it is the build soundstate.h was
// generated from (.data size, .rdata hash).
void map_from_files(Ctx &ctx, AudioCtx &a, uint32_t base) {
  const std::vector<uint8_t> f = ctx.files().read_all("sound.dll");
  const uint32_t pe = rd32(f, 0x3c);
  if (rd32(f, pe) != 0x4550u) throw MissingResource{"sound.dll: not a PE file"};
  const uint16_t nsec = rd16(f, pe + 6), optsz = rd16(f, pe + 0x14);
  const size_t opt = pe + 0x18;
  const uint32_t ibase = rd32(f, opt + 0x1c), isize = rd32(f, opt + 0x38), hsize = rd32(f, opt + 0x3c);
  const uint32_t reloc_rva = rd32(f, opt + 0x60 + 5 * 8), reloc_size = rd32(f, opt + 0x60 + 5 * 8 + 4);
  if (isize != kSoundImageSize) throw MissingResource{"sound.dll: SizeOfImage differs from soundstate.h"};
  std::vector<uint8_t> img(isize, 0);
  std::memcpy(img.data(), f.data(), std::min<size_t>(hsize, f.size()));
  bool data_ok = false, rdata_ok = false;
  for (uint16_t i = 0; i < nsec; ++i) {
    const size_t s = opt + optsz + 40u * i;
    char name[9] = {};
    std::memcpy(name, f.data() + s, 8);
    const uint32_t vsize = rd32(f, s + 8), rva = rd32(f, s + 12), rawsz = rd32(f, s + 16), rawoff = rd32(f, s + 20);
    const uint32_t n = std::min(rawsz, vsize ? vsize : rawsz);
    if (static_cast<uint64_t>(rawoff) + n > f.size() || static_cast<uint64_t>(rva) + n > isize)
      throw MissingResource{"sound.dll: section outside the file"};
    std::memcpy(img.data() + rva, f.data() + rawoff, n);
    if (std::strcmp(name, ".data") == 0) data_ok = rva == kSoundDataRva && vsize == kSoundDataSize;
    if (std::strcmp(name, ".rdata") == 0)
      rdata_ok = rva == kSoundRdataRva && vsize == kSoundRdataSize && sgh64(f.data() + rawoff, vsize) == kSoundRdataHash;
  }
  if (!data_ok) throw MissingResource{"sound.dll: .data differs from soundstate.h"};
  if (!rdata_ok) throw MissingResource{"sound.dll: .rdata differs from the anchored build"};
  const uint32_t delta = base - ibase;
  for (uint32_t o = reloc_rva; o + 8 <= reloc_rva + reloc_size;) {
    uint32_t page, blk;
    std::memcpy(&page, img.data() + o, 4);
    std::memcpy(&blk, img.data() + o + 4, 4);
    if (blk < 8) break;
    for (uint32_t k = 8; k + 2 <= blk; k += 2) {
      uint16_t e;
      std::memcpy(&e, img.data() + o + k, 2);
      if ((e >> 12) == 3) {
        const uint32_t at = page + (e & 0xfffu);
        uint32_t v;
        std::memcpy(&v, img.data() + at, 4);
        v += delta;
        std::memcpy(img.data() + at, &v, 4);
      }
    }
    o += blk;
  }
  a.image = std::move(img);
  // the read-only part (headers, .text, .rdata) and the writable rest (.data, .idata, .reloc)
  if (!ctx.mem.map(base, a.image.data(), kSoundDataRva, true) ||
      !ctx.mem.map(base + kSoundDataRva, a.image.data() + kSoundDataRva, isize - kSoundDataRva, false))
    throw MissingResource{"sound.dll: the image overlaps another mapping"};
  a.own_image = true;
}

const FuncDef *lookup_port(AudioCtx &a, uint32_t rva) {
  auto it = a.port_cache.find(rva);
  if (it != a.port_cache.end()) return it->second;
  const FuncDef *f = nullptr;
  for (size_t i = 0; i < kSoundFunctionCount; ++i) {
    if (kSoundFunctions[i].rva != rva || kSoundFunctions[i].collides) continue;
    // every sound.dll function is registered as "snd.<hooks.csv name>" (log/roadmap/merges/S4-3.md)
    const int k = registry_find((std::string("snd.") + kSoundFunctions[i].name).c_str());
    if (k >= 0) f = registry_at(k, nullptr);
    break;
  }
  if (f == nullptr) {
    if (const char *crt = audio_crt_name(rva)) {
      const int k = registry_find(crt);
      if (k >= 0) f = registry_at(k, nullptr);
    }
  }
  a.port_cache[rva] = f;
  return f;
}

bool in_image(const AudioCtx &a, uint32_t va) { return va - a.base < kSoundImageSize; }

// the harness's box for the host's view of the x87 unit, when it is mapped
uint8_t *fpu_box(Ctx &ctx) {
  try {
    return ctx.mem.at(kSndFpuBox, static_cast<uint32_t>(x87::Unit::kImageSize), true);
  } catch (const Fault &) {
    return nullptr;
  }
}

Ret host_call(Ctx &ctx, AudioCtx &a, SndKind kind, uint32_t va, SndAbi abi, const uint32_t *args, int n, bool want) {
  SndCallC c{};
  c.va = va;
  c.rva = in_image(a, va) ? va - a.base : 0;
  c.kind = static_cast<int32_t>(kind);
  c.abi = static_cast<int32_t>(abi);
  c.nargs = n;
  c.want_st0 = want ? 1 : 0;
  for (int i = 0; i < n; ++i) c.args[i] = args[i];
  switch (kind) {
    case SndKind::External: ++a.counters.external; break;
    case SndKind::Claimed: ++a.counters.claimed; break;
    case SndKind::Fallback:
      ++a.counters.fallback;
      a.counters.fallback_rvas.push_back(c.rva);
      break;
  }
  if (!a.host) {
    a.detail = "no audio host for a call to va " + std::to_string(va) + " (kind " + std::to_string(c.kind) + ")";
    throw MissingResource{"audio host"};
  }
  // the host's view of the x87 unit (audio.h): the unit's image in the box before the call, the box's image after it
  if (uint8_t *box = fpu_box(ctx)) x87::unit(ctx).save_image(box);
  try {
    a.host->call(ctx, c);
  } catch (const SoundHostError &e) {
    a.detail = e.what;
    throw;
  }
  if (uint8_t *box = fpu_box(ctx)) x87::unit(ctx).load_image(box);
  Ret r;
  r.eax = c.eax;
  r.edx = c.edx;
  if (c.has_st0) r.set_double(c.st0);
  return r;
}

// the import an IAT slot belongs to, from the mapped image's import directory: "DLL!name" (or "DLL!#ordinal")
bool import_name(Ctx &ctx, const AudioCtx &a, uint32_t iat_rva, std::string &dll, std::string &name) {
  const uint32_t pe = ld<uint32_t>(ctx, a.base + 0x3c);
  const uint32_t imp = ld<uint32_t>(ctx, a.base + pe + 0x18 + 0x60 + 8);
  if (imp == 0) return false;
  for (uint32_t d = a.base + imp;; d += 20) {
    const uint32_t oft = ld<uint32_t>(ctx, d), nm = ld<uint32_t>(ctx, d + 12), ft = ld<uint32_t>(ctx, d + 16);
    if (nm == 0 && ft == 0) return false;
    uint32_t i = 0;
    for (;; ++i) {
      const uint32_t th = ld<uint32_t>(ctx, a.base + (oft ? oft : ft) + 4 * i);
      if (th == 0) break;
      if (ft + 4 * i == iat_rva) {
        auto cstr = [&](uint32_t va) {
          std::string s;
          for (char ch; (ch = static_cast<char>(ld<uint8_t>(ctx, va))) != 0; ++va) s += ch;
          return s;
        };
        dll = cstr(a.base + nm);
        name = (th & 0x80000000u) ? "#" + std::to_string(th & 0xffffu) : cstr(a.base + th + 2);
        return true;
      }
    }
  }
}

Ret sink_reply(const AudioReply &r) {
  Ret out;
  out.eax = r.eax;
  out.edx = r.edx;
  if (r.has_st0) out.set_double(r.st0);
  return out;
}

// an import through its IAT slot: the AudioSink by name with no host, else the slot's value through the dispatcher
Ret import_call(Ctx &ctx, AudioCtx &a, uint32_t iat_rva, SndAbi abi, const uint32_t *args, int nargs, bool want_st0);

}  // namespace

namespace detail {
const SoundState &snd_probe() {
  static const SoundState probe{};
  return probe;
}
uint32_t snd_member_offset_bytes(const void *member_in_probe) {
  return static_cast<uint32_t>(static_cast<const uint8_t *>(member_in_probe) -
                               reinterpret_cast<const uint8_t *>(&snd_probe()));
}
}  // namespace detail

void audio_attach(Ctx &ctx, uint32_t image_base, bool map_image) {
  audio_detach(ctx);
  AudioCtx &a = actx(ctx);
  if ((image_base & 0xffffu) != 0 || static_cast<uint64_t>(image_base) + kSoundImageSize > 0x100000000ull)
    throw MissingResource{"sound.dll image base (64 KB aligned, inside the 32-bit space)"};
  if (map_image) map_from_files(ctx, a, image_base);
  a.base = image_base;
  a.attached = true;
}

namespace {
// images the core mapped (map_image) for a context that detached since: Memory has no single-mapping unmap, so the bytes
// stay alive until audio_release (sg_state_free) or the caller's sg_unmap_all no longer needs them
std::map<const Ctx *, std::vector<std::vector<uint8_t>>> &retired() {
  static std::map<const Ctx *, std::vector<std::vector<uint8_t>>> r;
  return r;
}
}  // namespace

void audio_detach(Ctx &ctx) {
  std::lock_guard<std::mutex> lk(g_mu);
  auto it = table().find(&ctx);
  if (it == table().end()) return;
  if (it->second->own_image) retired()[&ctx].push_back(std::move(it->second->image));
  if (!it->second->sink_block.empty()) retired()[&ctx].push_back(std::move(it->second->sink_block));
  if (!it->second->frame_block.empty()) retired()[&ctx].push_back(std::move(it->second->frame_block));
  for (auto &b : it->second->script_blocks) retired()[&ctx].push_back(std::move(*b));
  table().erase(it);
}

void audio_release(Ctx &ctx) {
  audio_detach(ctx);
  audio_exe_release(ctx);
  x87::unit_release_(ctx);
  std::lock_guard<std::mutex> lk(g_mu);
  retired().erase(&ctx);
}

bool audio_attached(const Ctx &ctx) {
  std::lock_guard<std::mutex> lk(g_mu);
  auto it = table().find(&ctx);
  return it != table().end() && it->second->attached;
}

uint32_t snd_base(const Ctx &ctx) { return attached(ctx).base; }
uint32_t snd_data_va(const Ctx &ctx) { return attached(ctx).base + kSoundDataRva; }

void audio_set_host(Ctx &ctx, SoundHost *host) { actx(ctx).host = host; }
void audio_set_callback(Ctx &ctx, SndHostFn fn, void *user) {
  AudioCtx &a = actx(ctx);
  a.callback.fn = fn;
  a.callback.user = user;
  a.host = fn ? &a.callback : nullptr;
}
void audio_claim(Ctx &ctx, uint32_t va) { actx(ctx).claims.insert(va); }
void audio_clear_claims(Ctx &ctx) { actx(ctx).claims.clear(); }
void audio_set_sink(Ctx &ctx, AudioSink *sink) { actx(ctx).sink = sink; }
const AudioCounters &audio_counters(const Ctx &ctx) { return actx(ctx).counters; }
void audio_reset_counters(Ctx &ctx) {
  AudioCtx &a = actx(ctx);
  a.counters = AudioCounters{};
  a.detail.clear();
}
const std::string &audio_detail(const Ctx &ctx) { return actx(ctx).detail; }

const FuncDef *snd_port_function(Ctx &ctx, uint32_t rva) { return lookup_port(attached(ctx), rva); }

SndResidue::SndResidue(Ctx &ctx) : ctx_(ctx), prev_(actx(ctx).residue) { actx(ctx).residue = true; }
SndResidue::~SndResidue() { actx(ctx_).residue = prev_; }

Ret snd_call_va(Ctx &ctx, uint32_t va, SndAbi abi, std::initializer_list<uint32_t> args, bool want_st0) {
  return snd_call_va(ctx, va, abi, args.begin(), static_cast<int>(args.size()), want_st0);
}

Ret snd_call_va(Ctx &ctx, uint32_t va, SndAbi abi, const uint32_t *args, int nargs, bool want_st0) {
  AudioCtx &a = attached(ctx);
  if (nargs < 0 || nargs > kSndMaxArgs) throw SoundHostError{"too many argument slots"};
  uint32_t target = va;
  for (int hop = 0;; ++hop) {
    if (a.claims.count(target)) return host_call(ctx, a, SndKind::Claimed, target, abi, args, nargs, want_st0);
    if (!in_image(a, target)) break;
    if (hop < 8 && ld<uint8_t>(ctx, target) == 0xE9) {  // an incremental-link thunk: jmp rel32
      target = target + 5 + ld<uint32_t>(ctx, target + 1);
      continue;
    }
    const FuncDef *f = lookup_port(a, target - a.base);
    if (f == nullptr) {
      // an import stub `jmp [iat]` (DirectSoundEnumerateA's at rva 0x3a978): the import itself (U-1472)
      if (ld<uint8_t>(ctx, target) == 0xFF && ld<uint8_t>(ctx, target + 1) == 0x25) {
        const uint32_t slot = ld<uint32_t>(ctx, target + 2);
        if (in_image(a, slot)) return import_call(ctx, a, slot - a.base, abi, args, nargs, want_st0);
      }
      return host_call(ctx, a, SndKind::Fallback, target, abi, args, nargs, want_st0);
    }
    // inside a SndResidue scope extra slots are the original's stack residue (audio.h); fewer slots are always an error
    const bool residue = a.residue;
    if (f->arity != nargs && !(residue && nargs > f->arity)) {
      a.detail = std::string("arity of ") + f->name + ": port call with " + std::to_string(nargs) + " slots";
      throw SoundHostError{a.detail};
    }
    ++a.counters.dispatched;
    // the callee enters at the esp the original's `call` leaves (audio.h "the stack"); residue words count, as pushed
    const uint32_t entry0 = a.entry, esp0 = a.esp;
    if (a.esp != 0) {
      const int nreg = abi == SndAbi::Thiscall ? 1 : (abi == SndAbi::Fastcall ? 2 : 0);
      a.entry = a.esp = a.esp - 4u * static_cast<uint32_t>(std::max(0, nargs - nreg)) - 4u;
    }
    a.residue = false;  // the callee's own calls are checked again
    Ret r;
    try {
      f->fn(ctx, args, r);
    } catch (...) {
      a.entry = entry0;
      a.esp = esp0;
      a.residue = residue;
      throw;
    }
    a.entry = entry0;
    a.esp = esp0;
    a.residue = residue;
    return r;
  }
  if (target - kSinkTrapBase < kSinkTrapSize && !a.host && a.sink) {
    AudioCommand cmd;
    cmd.iface = (target - kSinkTrapBase) >> 12;
    cmd.slot = ((target - kSinkTrapBase) & 0xfffu) >> 2;
    cmd.args.assign(args, args + nargs);
    cmd.want_st0 = want_st0;
    return sink_reply(a.sink->com(ctx, cmd));
  }
  return host_call(ctx, a, SndKind::External, target, abi, args, nargs, want_st0);
}

Ret snd_vcall(Ctx &ctx, uint32_t obj, uint32_t slot_off, SndAbi abi, std::initializer_list<uint32_t> args, bool want_st0) {
  uint32_t a[kSndMaxArgs];
  int n = 0;
  a[n++] = obj;
  for (uint32_t v : args) {
    if (n >= kSndMaxArgs) throw SoundHostError{"too many argument slots"};
    a[n++] = v;
  }
  const uint32_t fn = ld<uint32_t>(ctx, ld<uint32_t>(ctx, obj) + slot_off);
  return snd_call_va(ctx, fn, abi, a, n, want_st0);
}

Ret snd_import(Ctx &ctx, uint32_t iat_rva, SndAbi abi, std::initializer_list<uint32_t> args, bool want_st0) {
  AudioCtx &a = attached(ctx);
  return import_call(ctx, a, iat_rva, abi, args.begin(), static_cast<int>(args.size()), want_st0);
}

namespace {
Ret import_call(Ctx &ctx, AudioCtx &a, uint32_t iat_rva, SndAbi abi, const uint32_t *args, int nargs, bool want_st0) {
  if (a.host == nullptr && a.sink != nullptr) {
    AudioCommand cmd;
    if (!import_name(ctx, a, iat_rva, cmd.dll, cmd.name)) throw MissingResource{"sound.dll import slot"};
    cmd.args.assign(args, args + nargs);
    cmd.want_st0 = want_st0;
    return sink_reply(a.sink->import(ctx, cmd));
  }
  return snd_call_va(ctx, ld<uint32_t>(ctx, a.base + iat_rva), abi, args, nargs, want_st0);
}
}  // namespace

// ------------------------------------------------------------------------------------------------ the stack
SndFrame::SndFrame(Ctx &ctx, uint32_t bytes) : ctx_(ctx) {
  AudioCtx &a = attached(ctx);
  prev_esp_ = a.esp;
  prev_entry_ = a.entry;
  if (a.entry == 0) {  // no caller-given esp: the frame block, its top the entry esp
    if (a.frame_block.empty()) {
      bool mapped = true;
      try {
        ctx.mem.at(kSndFrameBase, kSndFrameSize, true);
      } catch (const Fault &) {
        mapped = false;
      }
      if (!mapped) {
        a.frame_block.assign(kSndFrameSize, 0);
        if (!ctx.mem.map(kSndFrameBase, a.frame_block.data(), kSndFrameSize, false))
          throw MissingResource{"audio_core frame block"};
      }
    }
    a.entry = a.esp = kSndFrameBase + kSndFrameSize - 0x44u;
    lazy_ = true;
  } else if (a.esp != a.entry) {
    // a frame of the running function is live: this one belongs to a body that function calls directly (a C call, not the
    // dispatcher), which enters below it as a `call` would
    a.entry = a.esp - 4u;
  }
  entry_ = a.entry;
  block_ = entry_ - kSndFrameBase < kSndFrameSize;
  set_esp(entry_ - bytes);
}
void SndFrame::set_esp(uint32_t esp) {
  AudioCtx &a = actx(ctx_);
  if (esp > entry_ || (block_ && esp < kSndFrameBase)) throw MissingResource{"audio_core stack: frame block full"};
  if (!block_ && a.esp_limit != 0 && esp < a.esp_limit)
    throw MissingResource{"audio_core stack: a frame below the harness's limit (host frames live there)"};
  const uint32_t bytes = entry_ - esp;
  if (bytes) {
    uint8_t *p = ctx_.mem.at(esp, bytes, true);  // the stack must be mapped (Fault otherwise)
    if (block_) std::memset(p, 0, bytes);
  }
  a.esp = esp;
}
SndFrame::~SndFrame() {
  AudioCtx &a = actx(ctx_);
  a.esp = lazy_ ? 0 : prev_esp_;
  a.entry = lazy_ ? 0 : prev_entry_;
}
void snd_set_entry_esp(Ctx &ctx, uint32_t esp, uint32_t limit) {
  AudioCtx &a = actx(ctx);
  a.entry = a.esp = esp;
  a.esp_limit = esp ? limit : 0;
}
uint32_t snd_entry_esp(Ctx &ctx) { return actx(ctx).entry; }
uint32_t snd_esp(Ctx &ctx) { return actx(ctx).esp; }

uint32_t audio_sink_object(Ctx &ctx, uint32_t iface, uint32_t nslots) {
  AudioCtx &a = actx(ctx);
  if (a.sink_block.empty()) {
    a.sink_block.assign(kSinkObjSize, 0);
    if (!ctx.mem.map(kSinkObjBase, a.sink_block.data(), kSinkObjSize, false)) throw MissingResource{"sink object block"};
  }
  const uint32_t need = 4 + 4 * nslots;
  if (iface >= (kSinkTrapSize >> 12) || nslots > 0x400) throw MissingResource{"sink object block full"};
  if (auto it = a.sink_free.find((uint64_t{iface} << 32) | nslots); it != a.sink_free.end()) {  // P3: a returned object
    const uint32_t obj = it->second;
    a.sink_free.erase(it);
    st<uint32_t>(ctx, obj, obj + 4);
    for (uint32_t s = 0; s < nslots; ++s) st<uint32_t>(ctx, obj + 4 + 4 * s, kSinkTrapBase + (iface << 12) + 4 * s);
    return obj;
  }
  if (a.sink_used + need > kSinkObjSize) throw MissingResource{"sink object block full"};
  const uint32_t obj = kSinkObjBase + a.sink_used;
  st<uint32_t>(ctx, obj, obj + 4);
  for (uint32_t s = 0; s < nslots; ++s) st<uint32_t>(ctx, obj + 4 + 4 * s, kSinkTrapBase + (iface << 12) + 4 * s);
  a.sink_used += (need + 15u) & ~15u;
  return obj;
}

void audio_sink_object_free(Ctx &ctx, uint32_t obj, uint32_t iface, uint32_t nslots) {
  AudioCtx &a = actx(ctx);
  if (obj < kSinkObjBase || obj >= kSinkObjBase + a.sink_used) return;  // not one of this block's objects
  a.sink_free.emplace((uint64_t{iface} << 32) | nslots, obj);
}

AudioReply RecordingSink::import(Ctx &, const AudioCommand &cmd) {
  std::string l = "import " + cmd.dll + "!" + cmd.name;
  for (uint32_t v : cmd.args) l += " " + std::to_string(v);
  lines.push_back(l);
  AudioReply r;
  r.eax = answer;
  return r;
}
AudioReply RecordingSink::com(Ctx &, const AudioCommand &cmd) {
  std::string l = "com " + std::to_string(cmd.iface) + "." + std::to_string(cmd.slot);
  for (uint32_t v : cmd.args) l += " " + std::to_string(v);
  lines.push_back(l);
  AudioReply r;
  r.eax = answer;
  return r;
}

// ------------------------------------------------------------------------------------------------ configuration entries
// Test and harness entries reached through sg_call, so the Win32 runner needs no new op. Blobs travel as context files:
//   "$audio/claims"  u32 VAs the host claims          "$audio/script"  the recorded transcript (format below)
//   "$audio/out"     written by audio_core.report: one `key=value` line per counter, the script position and the detail
// Script: "SGAS", u32 version 1, u32 count, then per entry u32 kind, va, abi, nargs, want_st0, args[nargs], eax, edx,
// has_st0, u64 st0 bits, u32 nwrites { u32 va, u32 len, bytes }, u32 nmaps { u32 base, u32 size, bytes }.
namespace {

struct Rd {
  const std::vector<uint8_t> &b;
  size_t p = 0;
  uint32_t u32() {
    if (p + 4 > b.size()) throw MissingResource{"$audio/script truncated"};
    uint32_t v;
    std::memcpy(&v, b.data() + p, 4);
    p += 4;
    return v;
  }
  std::vector<uint8_t> bytes(uint32_t n) {
    if (p + n > b.size()) throw MissingResource{"$audio/script truncated"};
    std::vector<uint8_t> v(b.begin() + static_cast<std::ptrdiff_t>(p), b.begin() + static_cast<std::ptrdiff_t>(p + n));
    p += n;
    return v;
  }
};

void a_attach(Ctx &c, const uint32_t *a, Ret &r) {
  audio_attach(c, a[0], a[1] != 0);
  r.set_u32(snd_base(c));
}
void a_detach(Ctx &c, const uint32_t *, Ret &) { audio_detach(c); }
void a_claims(Ctx &c, const uint32_t *, Ret &r) {
  audio_clear_claims(c);
  const std::vector<uint8_t> b = c.files().exists("$audio/claims") ? c.files().read_all("$audio/claims")
                                                                    : std::vector<uint8_t>{};
  for (size_t i = 0; i + 4 <= b.size(); i += 4) {
    uint32_t v;
    std::memcpy(&v, b.data() + i, 4);
    audio_claim(c, v);
  }
  r.set_u32(static_cast<uint32_t>(b.size() / 4));
}
void a_use_script(Ctx &c, const uint32_t *, Ret &r) {
  AudioCtx &a = actx(c);
  const std::vector<uint8_t> b = c.files().read_all("$audio/script");
  Rd rd{b};
  if (rd.u32() != 0x53414753u || rd.u32() != 1u) throw MissingResource{"$audio/script: bad header"};
  const uint32_t n = rd.u32();
  a.script.entries.clear();
  a.script.pos = 0;
  a.script.owned = &a.script_blocks;
  for (uint32_t i = 0; i < n; ++i) {
    ScriptEntry e;
    e.req.kind = static_cast<int32_t>(rd.u32());
    e.req.va = rd.u32();
    e.req.abi = static_cast<int32_t>(rd.u32());
    e.req.nargs = static_cast<int32_t>(rd.u32());
    e.req.want_st0 = static_cast<int32_t>(rd.u32());
    if (e.req.nargs < 0 || e.req.nargs > kSndMaxArgs) throw MissingResource{"$audio/script: bad entry"};
    for (int k = 0; k < e.req.nargs; ++k) e.req.args[k] = rd.u32();
    e.req.eax = rd.u32();
    e.req.edx = rd.u32();
    e.req.has_st0 = static_cast<int32_t>(rd.u32());
    const uint32_t lo = rd.u32(), hi = rd.u32();
    const uint64_t bits = (static_cast<uint64_t>(hi) << 32) | lo;
    std::memcpy(&e.req.st0, &bits, 8);
    for (uint32_t w = rd.u32(); w; --w) {
      const uint32_t va = rd.u32(), len = rd.u32();
      e.writes.emplace_back(va, rd.bytes(len));
    }
    for (uint32_t m = rd.u32(); m; --m) {
      const uint32_t base = rd.u32(), size = rd.u32();
      e.maps.emplace_back(base, rd.bytes(size));
    }
    a.script.entries.push_back(std::move(e));
  }
  a.host = &a.script;
  r.set_u32(n);
}
// one call for a vector's setup (the Win32 runner copies every writable block per call): attach at a[0] (no mapping), then
// a[1] bit 0 = load $audio/claims, bit 1 = the script host from $audio/script, bit 2 = the C callback host, bit 3 = reset
// the counters
void a_setup(Ctx &c, const uint32_t *a, Ret &r) {
  AudioCtx &x0 = actx(c);
  const SndHostFn fn = x0.callback.fn;
  void *user = x0.callback.user;
  audio_attach(c, a[0], false);
  audio_set_callback(c, fn, user);  // attach starts a fresh state: keep the callback the C ABI installed
  if (!(a[1] & 4u)) actx(c).host = nullptr;
  Ret t;
  if (a[1] & 1u) a_claims(c, a, t);
  if (a[1] & 2u) a_use_script(c, a, t);
  if (a[1] & 8u) audio_reset_counters(c);
  r.set_u32(snd_base(c));
}
void a_use_callback(Ctx &c, const uint32_t *, Ret &r) {
  AudioCtx &a = actx(c);
  a.host = a.callback.fn ? &a.callback : nullptr;
  r.set_u32(a.host ? 1 : 0);
}
void a_no_host(Ctx &c, const uint32_t *, Ret &) { actx(c).host = nullptr; }
void a_reset(Ctx &c, const uint32_t *, Ret &) { audio_reset_counters(c); }
void a_report(Ctx &c, const uint32_t *, Ret &r) {
  AudioCtx &a = actx(c);
  std::string s = "external=" + std::to_string(a.counters.external) + "\nclaimed=" + std::to_string(a.counters.claimed) +
                  "\nfallback=" + std::to_string(a.counters.fallback) + "\ndispatched=" +
                  std::to_string(a.counters.dispatched) + "\nscript_pos=" + std::to_string(a.script.pos) +
                  "\nscript_len=" + std::to_string(a.script.entries.size()) + "\nfallback_rvas=";
  for (size_t i = 0; i < a.counters.fallback_rvas.size(); ++i)
    s += (i ? "," : "") + std::to_string(a.counters.fallback_rvas[i]);
  s += "\ndetail=" + a.detail + "\nx87_detail=" + x87::detail(c) + "\nsoft=" + std::to_string(x87::soft() ? 1 : 0) + "\n";
  c.files().write("$audio/out", std::vector<uint8_t>(s.begin(), s.end()));
  r.set_u32(static_cast<uint32_t>(s.size()));
}
// ---- the stack and the x87 unit (S4.2b). Files: "$audio/x87" the unit's image to load (x87::Unit::load_image), "$audio/x87out"
// the image the unit holds after the call; "$audio/x80in" / "$audio/x80out" audio_core.x80batch's operations: per record
// u32 op, u32 cw, a (u64 m, u16 se), b (u64 m, u16 se), u32 0 (32 bytes); op 0 add, 1 sub, 2 mul, 3 div, 4 fst64 a,
// 5 / 6 / 7 fist word / dword / qword a, 8 fcom a b, 9 fld64 of a.m's bits, 10 from_f64 of a.m's bits, 11 fild of a.m as
// int64, 12 fyl2x (st0 = a, st1 = b); out per record u64 (the significand, the stored bits or the integer), u16 (sign|exponent
// or the condition codes), 6 bytes 0.
std::vector<uint8_t> file_or_empty(Ctx &c, const char *name) {
  return c.files().exists(name) ? c.files().read_all(name) : std::vector<uint8_t>{};
}
void a_esp(Ctx &c, const uint32_t *a, Ret &r) {
  snd_set_entry_esp(c, a[0], a[1]);
  r.set_u32(a[0]);
}
void x87load(Ctx &c, Ret &r) {
  const std::vector<uint8_t> b = file_or_empty(c, "$audio/x87");
  x87::set_detail(c, "");
  if (b.empty()) {
    x87::unit(c) = x87::Unit{};
    r.set_u32(0);
    return;
  }
  if (b.size() < x87::Unit::kImageSize) throw MissingResource{"$audio/x87 truncated"};
  x87::unit(c).load_image(b.data());
  r.set_u32(1);
}
void a_x87load(Ctx &c, const uint32_t *, Ret &r) { x87load(c, r); }
void a_x87save(Ctx &c, const uint32_t *, Ret &r) {
  std::vector<uint8_t> b(x87::Unit::kImageSize);
  x87::unit(c).save_image(b.data());
  c.files().write("$audio/x87out", b);
  r.set_u32(static_cast<uint32_t>(b.size()));
}
// one call before a vector's port call: the entry esp and limit (audio_core.esp) and the unit from "$audio/x87"
void a_vector(Ctx &c, const uint32_t *a, Ret &r) {
  snd_set_entry_esp(c, a[0], a[1]);
  x87load(c, r);
}
// one call after it: "$audio/x87out" and "$audio/out" (audio_core.x87save + audio_core.report)
void a_finish(Ctx &c, const uint32_t *a, Ret &r) {
  a_x87save(c, a, r);
  a_report(c, a, r);
}
void a_x80soft(Ctx &, const uint32_t *a, Ret &r) {
  x87::set_soft(a[0] != 0);
  r.set_u32(x87::soft() ? 1 : 0);
}
void a_x80batch(Ctx &c, const uint32_t *a, Ret &r) {
  const std::vector<uint8_t> in = c.files().read_all("$audio/x80in");
  const bool was = x87::soft();
  if (a[0]) x87::set_soft(true);
  std::vector<uint8_t> out((in.size() / 32) * 16, 0);
  uint32_t n = 0;
  try {
    for (size_t k = 0; k + 32 <= in.size(); k += 32, ++n) {
      uint32_t op, cw;
      x87::X80 x, y;
      std::memcpy(&op, in.data() + k, 4);
      std::memcpy(&cw, in.data() + k + 4, 4);
      std::memcpy(&x.m, in.data() + k + 8, 8);
      std::memcpy(&x.se, in.data() + k + 16, 2);
      std::memcpy(&y.m, in.data() + k + 18, 8);
      std::memcpy(&y.se, in.data() + k + 26, 2);
      const uint16_t w = static_cast<uint16_t>(cw);
      x87::X80 res;
      uint64_t v = 0;
      uint16_t s = 0;
      bool is_x = true;
      switch (op) {
        case 0: res = x87::add(x, y, w); break;
        case 1: res = x87::sub(x, y, w); break;
        case 2: res = x87::mul(x, y, w); break;
        case 3: res = x87::div(x, y, w); break;
        case 4: v = x87::fst64(x, w); is_x = false; break;
        case 5: v = static_cast<uint64_t>(x87::fist(x, 16, w)); is_x = false; break;
        case 6: v = static_cast<uint64_t>(x87::fist(x, 32, w)); is_x = false; break;
        case 7: v = static_cast<uint64_t>(x87::fist(x, 64, w)); is_x = false; break;
        case 8: s = x87::fcom(x, y); is_x = false; break;
        case 9: res = x87::fld64(x.m); break;
        case 10: res = x87::from_f64(x.m); break;
        case 11: res = x87::fild(static_cast<int64_t>(x.m)); break;
        case 12: res = x87::fyl2x(x, y, w); break;
        default: throw MissingResource{"audio_core.x80batch: bad op"};
      }
      if (is_x) {
        v = res.m;
        s = res.se;
      }
      std::memcpy(out.data() + (k / 32) * 16, &v, 8);
      std::memcpy(out.data() + (k / 32) * 16 + 8, &s, 2);
    }
  } catch (const x87::X87Unmodelled &e) {
    x87::set_soft(was);
    x87::set_detail(c, "record " + std::to_string(n) + ": " + e.what);
    throw;
  }
  x87::set_soft(was);
  c.files().write("$audio/x80out", out);
  r.set_u32(n);
}
// production route test: a RecordingSink owned by the context, no host; audio_core.sinkLog writes its lines to $audio/sink
void a_use_sink(Ctx &c, const uint32_t *a, Ret &) {
  AudioCtx &x = actx(c);
  x.owned_sink = std::make_unique<RecordingSink>();
  x.owned_sink->answer = a[0];
  x.sink = x.owned_sink.get();
  x.host = nullptr;
}
void a_sink_log(Ctx &c, const uint32_t *, Ret &r) {
  AudioCtx &x = actx(c);
  std::string s;
  if (x.owned_sink)
    for (const std::string &l : x.owned_sink->lines) s += l + "\n";
  c.files().write("$audio/sink", std::vector<uint8_t>(s.begin(), s.end()));
  r.set_u32(static_cast<uint32_t>(s.size()));
}
// the dispatcher from outside: call `va` with the slots stored in $audio/args (u32 abi, u32 n, slots); eax / edx back
void a_dispatch(Ctx &c, const uint32_t *a, Ret &r) {
  const std::vector<uint8_t> b = c.files().read_all("$audio/args");
  Rd rd{b};
  const SndAbi abi = static_cast<SndAbi>(rd.u32());
  const uint32_t n = rd.u32();
  uint32_t args[kSndMaxArgs] = {};
  for (uint32_t i = 0; i < n && i < kSndMaxArgs; ++i) args[i] = rd.u32();
  r = snd_call_va(c, a[0], abi, args, static_cast<int>(n));
}
// a sink interface object (iface, nslots) -> its address
void a_sink_object(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(audio_sink_object(c, a[0], a[1])); }
// snd_import from outside: a[0] = IAT slot rva, slots in $audio/args as for audio_core.dispatch
void a_import(Ctx &c, const uint32_t *a, Ret &r) {
  const std::vector<uint8_t> b = c.files().read_all("$audio/args");
  Rd rd{b};
  const SndAbi abi = static_cast<SndAbi>(rd.u32());
  const uint32_t n = rd.u32();
  std::vector<uint32_t> v;
  for (uint32_t i = 0; i < n && i < kSndMaxArgs; ++i) v.push_back(rd.u32());
  AudioCtx &x = attached(c);
  if (x.host == nullptr && x.sink != nullptr) {
    AudioCommand cmd;
    if (!import_name(c, x, a[0], cmd.dll, cmd.name)) throw MissingResource{"sound.dll import slot"};
    cmd.args = v;
    r = sink_reply(x.sink->import(c, cmd));
    return;
  }
  r = snd_call_va(c, ld<uint32_t>(c, x.base + a[0]), abi, v.data(), static_cast<int>(v.size()));
}

}  // namespace

// ================================================================================================ part 2: lists, nodes, helpers
// The first family of sound.dll functions (step S4.2 rows of re/roadmap/S4_functions.tsv), written from the C3 notes in the
// hook sources (shim/src/re/c3cn.cpp part 4, c3dj.cpp, c3do.cpp, c3fl.cpp, c3ar.cpp, c3fk.cpp, c3an.cpp, c3au.cpp, c3cx.cpp,
// c3dt.cpp, c3dh.cpp, c3ex.cpp) and judged by the replay (port/test/sound_replay.py). Instruction addresses in comments are
// RVAs. Lists: a header {head +0, back +4, cursor +8, count +0xc} and nodes {prev +0, next +4, value +8} (c3cn part 4).
//
// Negative controls: mode 1 of every function is a wrong result or a wrong store the comment names ("neg 1"). The twins
// share one body and each reads its own switch.
namespace {
using u32 = uint32_t;

// ---- shared bodies

// push_back (c3cn, reference appendNodeB 0x5e30; nine instruction-identical members): a 0xc-byte node from operator new
// (0x5e35); a non-NULL node gets +0 = 0, +4 = 0, +8 = value (0x5e45-0x5e52), a NULL one is used as it is (the stores
// through it fault); with a back node (+4, 0x5e5e) back->next = node, node->prev = the back re-read, back = node
// (0x5e60-0x5e68), else head = back = node (0x5e76); count + 1 stored and returned (0x5e6b / 0x5e7b). neg 1: returns
// the count + 1.
int32_t push_back(Ctx &c, u32 self, u32 value, int neg) {
  const u32 n = snd_new(c, 0xc);
  if (n != 0) {
    st<u32>(c, n, 0);
    st<u32>(c, n + 4, 0);
    st<u32>(c, n + 8, value);
  }
  const u32 back = ld<u32>(c, self + 4);
  if (back != 0) {
    st<u32>(c, back + 4, n);
    st<u32>(c, n, ld<u32>(c, self + 4));
    st<u32>(c, self + 4, n);
  } else {
    st<u32>(c, self, n);
    st<u32>(c, self + 4, n);
  }
  const int32_t cnt = add32(ld<int32_t>(c, self + 0xc), 1);
  st<int32_t>(c, self + 0xc, cnt);
  return neg == 1 ? add32(cnt, 1) : cnt;
}

// pushFront (c3cn, NodeList_pushFront 0x24f90 and its twin PtrList_pushFront 0x1d1d0): push_back anchored on the head:
// head->prev = node (0x24fbf), node->next = the head re-read (0x24fc1-0x24fc3), head = node (0x24fc6); the empty arm is
// push_back's (0x24fd3). neg 1: node->next takes the node itself.
int32_t push_front(Ctx &c, u32 self, u32 value, int neg) {
  const u32 n = snd_new(c, 0xc);
  if (n != 0) {
    st<u32>(c, n, 0);
    st<u32>(c, n + 4, 0);
    st<u32>(c, n + 8, value);
  }
  const u32 head = ld<u32>(c, self);
  if (head != 0) {
    st<u32>(c, head, n);
    st<u32>(c, n + 4, neg == 1 ? n : ld<u32>(c, self));
    st<u32>(c, self, n);
  } else {
    st<u32>(c, self, n);
    st<u32>(c, self + 4, n);
  }
  const int32_t cnt = add32(ld<int32_t>(c, self + 0xc), 1);
  st<int32_t>(c, self + 0xc, cnt);
  return cnt;
}

// pop every node from the front (c3cn clearNodeList, reference clearNodeListD 0x5a60; c3dj popFrontAll, PtrList_freeNodesB
// 0x31ac0: the same instructions). The header's head / back / count sit at fo / bo / co. Per node: next = node+4; with a
// next node its prev = 0 and head = next (0x31aca-0x31ad7), else back = head = 0 (0x31adb-0x31ae2); value = node+8
// (0x31ae8); operator delete(node) (0x31aec); count - 1 (0x31af1-0x31afa); a NULL value ends (0x31afd), else the next
// head (0x31aff, NULL ends). neg 1: the count drops by 2 per node.
void pop_all(Ctx &c, u32 hdr, u32 fo, u32 bo, u32 co, int neg) {
  u32 node = ld<u32>(c, hdr + fo);
  while (node != 0) {
    const u32 next = ld<u32>(c, node + 4);
    if (next != 0) {
      st<u32>(c, next, 0);
      st<u32>(c, hdr + fo, next);
    } else {
      st<u32>(c, hdr + bo, 0);
      st<u32>(c, hdr + fo, 0);
    }
    const u32 value = ld<u32>(c, node + 8);
    snd_delete(c, node);
    st<int32_t>(c, hdr + co, sub32(ld<int32_t>(c, hdr + co), neg == 1 ? 2 : 1));
    if (value == 0) return;
    node = ld<u32>(c, hdr + fo);
  }
}

// popFront (c3cn, listPopFront 0x5c00 / nodeListPopFront 0x39620): one pop; an empty list returns the NULL head (0x5c03);
// otherwise the unlink as in pop_all, the node's value saved (0x5c2a), operator delete (0x5c2e), count - 1 (0x5c39), the
// value returned (0x5c3d). neg 1: returns the node instead of its value.
u32 pop_front(Ctx &c, u32 self, int neg) {
  const u32 node = ld<u32>(c, self);
  if (node == 0) return 0;
  const u32 next = ld<u32>(c, node + 4);
  if (next != 0) {
    st<u32>(c, next, 0);
    st<u32>(c, self, next);
  } else {
    st<u32>(c, self + 4, 0);
    st<u32>(c, self, 0);
  }
  const u32 value = ld<u32>(c, node + 8);
  snd_delete(c, node);
  st<int32_t>(c, self + 0xc, sub32(ld<int32_t>(c, self + 0xc), 1));
  return neg == 1 ? node : value;
}

// unlink a found node (c3do listUnlink, c3dj findUnlink): prev -> its next takes node->next, else the head does; next -> its
// prev takes node->prev and the cursor (+8) becomes next, else cursor = 0 and back = node->prev; operator delete(node);
// count - 1. neg 1: the cursor is not cleared on the last node.
void unlink_node(Ctx &c, u32 hdr, u32 node, int neg) {
  const u32 prev = ld<u32>(c, node);
  if (prev != 0) st<u32>(c, prev + 4, ld<u32>(c, node + 4));
  else st<u32>(c, hdr, ld<u32>(c, node + 4));
  const u32 next = ld<u32>(c, node + 4);
  if (next != 0) {
    st<u32>(c, next, ld<u32>(c, node));
    st<u32>(c, hdr + 8, next);
  } else {
    const u32 back = ld<u32>(c, node);
    if (neg != 1) st<u32>(c, hdr + 8, 0);
    st<u32>(c, hdr + 4, back);
  }
  snd_delete(c, node);
  st<int32_t>(c, hdr + 0xc, sub32(ld<int32_t>(c, hdr + 0xc), 1));
}

// the first node whose value equals `value`, following next (+4); 0 when none
u32 find_node(Ctx &c, u32 hdr, u32 value) {
  u32 node = ld<u32>(c, hdr);
  while (node != 0 && ld<u32>(c, node + 8) != value) node = ld<u32>(c, node + 4);
  return node;
}

// findAndUnlinkNode 0x136c0 / findAndUnlinkNodeB 0xe700 (c3dj): front NULL or no node with the value returns 0
// (0x136c7 / 0x136d7); the found node is unlinked (0x136e3-0x1370f), its value kept (0x13713), the node deleted (0x13717), the
// count decremented (0x1371c-0x13723), the value returned (0x13726). neg 1: returns the node.
u32 find_unlink(Ctx &c, u32 hdr, u32 value, int neg) {
  const u32 node = find_node(c, hdr, value);
  if (node == 0) return 0;
  const u32 prev = ld<u32>(c, node);
  if (prev != 0) st<u32>(c, prev + 4, ld<u32>(c, node + 4));
  else st<u32>(c, hdr, ld<u32>(c, node + 4));
  const u32 next = ld<u32>(c, node + 4);
  if (next != 0) {
    st<u32>(c, next, ld<u32>(c, node));
    st<u32>(c, hdr + 8, next);
  } else {
    const u32 back = ld<u32>(c, node);
    st<u32>(c, hdr + 8, 0);
    st<u32>(c, hdr + 4, back);
  }
  const u32 v = ld<u32>(c, node + 8);
  snd_delete(c, node);
  st<int32_t>(c, hdr + 0xc, sub32(ld<int32_t>(c, hdr + 0xc), 1));
  return neg == 1 ? node : v;
}

// NodeList_insert 0x24e60 and its twin PtrList_insert 0x1d250 (c3dh): front == 0 -> append (0x24e6b-0x24eb7); cursor == 0 ->
// insert at the front (0x24ec1-0x24ef3); cursor->next == 0 -> append (0x24f02); else between cursor and next
// (0x24f24-0x24f32). Each case takes a {0, 0, value} node from operator new (left as it is when NULL) and returns the
// incremented count. neg 1: the between-case links node->prev to next.
u32 new_node(Ctx &c, u32 value) {
  const u32 n = snd_new(c, 0xc);
  if (n != 0) {
    st<u32>(c, n, 0);
    st<u32>(c, n + 4, 0);
    st<u32>(c, n + 8, value);
  }
  return n;
}
u32 list_insert(Ctx &c, u32 self, u32 value, int neg) {
  auto append = [&]() {
    const u32 n = new_node(c, value);
    const u32 back = ld<u32>(c, self + 4);
    if (back != 0) {
      st<u32>(c, back + 4, n);
      st<u32>(c, n, ld<u32>(c, self + 4));
      st<u32>(c, self + 4, n);
    } else {
      st<u32>(c, self, n);
      st<u32>(c, self + 4, n);
    }
    const u32 k = ld<u32>(c, self + 0xc) + 1;
    st<u32>(c, self + 0xc, k);
    return k;
  };
  if (ld<u32>(c, self) == 0) return append();
  const u32 cursor = ld<u32>(c, self + 8);
  if (cursor == 0) {
    const u32 n = new_node(c, value);
    const u32 front = ld<u32>(c, self);
    if (front == 0) {  // 0x24ee3: never taken (the front was tested non-zero), kept as the original has it
      st<u32>(c, self, n);
      st<u32>(c, self + 4, n);
    } else {
      st<u32>(c, front, n);
      st<u32>(c, n + 4, ld<u32>(c, self));
      st<u32>(c, self, n);
    }
    const u32 k = ld<u32>(c, self + 0xc) + 1;
    st<u32>(c, self + 0xc, k);
    return k;
  }
  const u32 next = ld<u32>(c, cursor + 4);
  if (next == 0) return append();
  const u32 n = new_node(c, value);
  st<u32>(c, ld<u32>(c, self + 8) + 4, n);
  st<u32>(c, next, n);
  st<u32>(c, n + 4, next);
  st<u32>(c, n, neg == 1 ? next : ld<u32>(c, self + 8));
  const u32 k = ld<u32>(c, self + 0xc) + 1;
  st<u32>(c, self + 0xc, k);
  return k;
}

// a string copied into a fresh operator-new block of strlen + 1 bytes (`repne scasb`, `rep movsd` + `rep movsb`); the
// block's address is stored at `field` before the copy (c3dj dupInto, c3fk StrPtr_assign); returns strlen + 1
u32 dup_into(Ctx &c, u32 field, u32 s) {
  const u32 len = vstrlen(c.mem, s) + 1;
  const u32 p = snd_new(c, len);
  st<u32>(c, field, p);
  rep_movs(c.mem, p, s, len);
  return len;
}

// the static CRT's strncpy (getNodeByIndex 0x11790 calls it at 0x117e0): up to count bytes of src up to its NUL, then NUL
// bytes up to count; returns dst
u32 crt_strncpy(Ctx &c, u32 dst, u32 src, u32 count) {
  u32 i = 0;
  for (; i < count; ++i) {
    const uint8_t b = ld<uint8_t>(c, src + i);
    st<uint8_t>(c, dst + i, b);
    if (b == 0) break;
  }
  for (; i < count; ++i) st<uint8_t>(c, dst + i, 0);
  return dst;
}

// .data offset of the active list's tail (the head is g_activeListHead; c3dj unlink block: tail at rva 0xb506c)
constexpr u32 kActiveListTailOff = 0x5206c;
// flushSoundList's IAT slot of timeGetTime (c3fl kIatTimeGetTime)
constexpr u32 kIatTimeGetTime = 0xb8654;

// ---- negative-control switches (one per function)
#define SND_NEG(v) int g_negctl_##v = 0;
SND_NEG(appendNodeB) SND_NEG(appendNodeC) SND_NEG(appendNode) SND_NEG(PtrList_pushBack) SND_NEG(NodeList_pushBack)
SND_NEG(ResourceList_pushBack) SND_NEG(nodeListAppendValue) SND_NEG(listAppendNodeA) SND_NEG(listAppendNodeB)
SND_NEG(NodeList_pushFront) SND_NEG(PtrList_pushFront)
SND_NEG(clearNodeListD) SND_NEG(clearNodeListE) SND_NEG(clearNodeListF) SND_NEG(clearNodeListG) SND_NEG(clearNodeListC)
SND_NEG(clearNodeListA) SND_NEG(clearNodeListB) SND_NEG(PtrList_freeNodesA) SND_NEG(NodeList_clear)
SND_NEG(PtrList_freeNodesB) SND_NEG(nodeListClear2) SND_NEG(nodeListClear)
SND_NEG(listPopFront) SND_NEG(nodeListPopFront) SND_NEG(freeDynArray) SND_NEG(resizeDynArray) SND_NEG(flushSoundList)
SND_NEG(listNode3Init) SND_NEG(pairStore) SND_NEG(pairClear) SND_NEG(NodeB000_ctor) SND_NEG(getSpanLength)
SND_NEG(cmdNode_scalarDeletingDtor) SND_NEG(removeSoundNode) SND_NEG(addToSoundList) SND_NEG(stubReturnBB)
SND_NEG(findAndUnlinkNodeB) SND_NEG(findAndUnlinkNode) SND_NEG(seekListToIndex) SND_NEG(getNodeByIndex)
SND_NEG(StrPtr_assign) SND_NEG(ChainObject_dtor) SND_NEG(ListValues_callSlot18) SND_NEG(ChainObject_scalarDtor)
SND_NEG(PtrList_init) SND_NEG(byteSwap16) SND_NEG(NodeList_insert) SND_NEG(PtrList_insert) SND_NEG(NodeList_initNode)
SND_NEG(randRange) SND_NEG(nextRandomFloat) SND_NEG(unlinkFromActiveList) SND_NEG(Object_setName) SND_NEG(getField204)
SND_NEG(NodeB000_initBase) SND_NEG(voiceListRemove) SND_NEG(MemBlock_deleteDtor) SND_NEG(setField6cReturn0)
SND_NEG(clearField6cReturn0) SND_NEG(snd_strncpy) SND_NEG(crt_ftol)
#undef SND_NEG
#define NEG(v) SG_NEGCTL(g_negctl_##v)

// ---- the family members
#define SND_PUSH(v) \
  void a_##v(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(push_back(c, a[0], a[1], NEG(v))); }
SND_PUSH(appendNodeB) SND_PUSH(appendNodeC) SND_PUSH(appendNode) SND_PUSH(PtrList_pushBack) SND_PUSH(NodeList_pushBack)
SND_PUSH(ResourceList_pushBack) SND_PUSH(nodeListAppendValue) SND_PUSH(listAppendNodeA) SND_PUSH(listAppendNodeB)
#undef SND_PUSH
void a_NodeList_pushFront(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(push_front(c, a[0], a[1], NEG(NodeList_pushFront))); }
void a_PtrList_pushFront(Ctx &c, const uint32_t *a, Ret &r) { r.set_i32(push_front(c, a[0], a[1], NEG(PtrList_pushFront))); }
#define SND_CLEAR(v) \
  void a_##v(Ctx &c, const uint32_t *a, Ret &) { pop_all(c, a[0], 0, 4, 0xc, NEG(v)); }
SND_CLEAR(clearNodeListD) SND_CLEAR(clearNodeListE) SND_CLEAR(clearNodeListF) SND_CLEAR(clearNodeListG)
SND_CLEAR(clearNodeListC) SND_CLEAR(clearNodeListA) SND_CLEAR(clearNodeListB) SND_CLEAR(PtrList_freeNodesA)
SND_CLEAR(NodeList_clear) SND_CLEAR(PtrList_freeNodesB) SND_CLEAR(nodeListClear2) SND_CLEAR(nodeListClear)
#undef SND_CLEAR
void a_listPopFront(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(pop_front(c, a[0], NEG(listPopFront))); }
void a_nodeListPopFront(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(pop_front(c, a[0], NEG(nodeListPopFront))); }
void a_findAndUnlinkNode(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(find_unlink(c, a[0], a[1], NEG(findAndUnlinkNode))); }
void a_findAndUnlinkNodeB(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32(find_unlink(c, a[0], a[1], NEG(findAndUnlinkNodeB)));
}
void a_NodeList_insert(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(list_insert(c, a[0], a[1], NEG(NodeList_insert))); }
void a_PtrList_insert(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(list_insert(c, a[0], a[1], NEG(PtrList_insert))); }

// freeDynArray 0x6ec0 (c3cn): +8 &= ~1 stored before the branch (0x6ec9 / 0x6ece); a non-NULL buffer at +4 goes to operator
// delete (0x6ed1 / 0x6ed4); +4 = 0 (0x6edc). neg 1: bit 0 of +8 kept.
void a_freeDynArray(Ctx &c, const uint32_t *a, Ret &) {
  const u32 self = a[0];
  const u32 flags = ld<u32>(c, self + 8) & (NEG(freeDynArray) == 1 ? 0xffffffffu : ~1u);
  const u32 buf = ld<u32>(c, self + 4);
  st<u32>(c, self + 8, flags);
  if (buf != 0) snd_delete(c, buf);
  st<u32>(c, self + 4, 0);
}

// resizeDynArray 0x6f00 (c3do): n == count (+0xc) returns 0 (0x6f08-0x6f11); edi = bit 0 of +8 as 0 / -1 (0x6f1a / 0x6f1d;
// the `cmp edi, 1` store at 0x6f25 can never run); a non-NULL +4 is deleted (0x6f2a-0x6f32); +4 = 0 (0x6f41); operator
// new(n * 8) (0x6f49) becomes +4 (0x6f4e); +0xc = n (0x6f5a); n zero pairs written, +4 re-read per pair (0x6f5f-0x6f70);
// +8 gets bit 0 of edi, i.e. keeps its value (0x6f72-0x6f81); returns 0. neg 1: the count is stored as n + 1.
void a_resizeDynArray(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], n = a[1];
  if (n == ld<u32>(c, self + 0xc)) {
    r.set_u32(0);
    return;
  }
  const u32 eax = ld<u32>(c, self + 8);
  const u32 edi = (eax & 1u) ? 0xffffffffu : 0u;
  const u32 old = ld<u32>(c, self + 4);
  if (old != 0) snd_delete(c, old);
  st<u32>(c, self + 4, 0);
  const u32 data = snd_new(c, n * 8u);
  st<u32>(c, self + 4, data);
  st<u32>(c, self + 0xc, NEG(resizeDynArray) == 1 ? n + 1 : n);
  if (n > 0) {
    u32 i = 0;
    do {
      const u32 d = ld<u32>(c, self + 4);
      ++i;
      st<u32>(c, d + i * 8 - 8, 0);
      st<u32>(c, d + i * 8 - 4, 0);
    } while (i < ld<u32>(c, self + 0xc));
  }
  const u32 cur = ld<u32>(c, self + 8);
  st<u32>(c, self + 8, ((cur ^ edi) & 1u) ^ cur);
  r.set_u32(0);
}

// flushSoundList 0x8470 (c3fl): timeGetTime through its IAT slot, result dropped (0x8473); the engine (g_soundEngine) list at
// +0x1c8: cursor = head (0x8484-0x848c); an empty list or a NULL first object returns 0. Each object o by its slot +0x78:
//   1: slot +0x5c; non-zero advances the cursor (0x864c-0x865a; a NULL cursor or next ends). Zero: the next object from
//      cursor->next first (0x8544-0x8557), o unlinked (0x8559-0x85af), bit 10 of +0xcc -> Channel_clearBackref (thunk 0x1398)
//      on o+0x70 (0x85b2-0x85c0), bit 6 of +0xc8 cleared (0x85c5-0x85ce), then bit 1 of +0x58, or bit 4 of +0x58 with bit 0
//      of byte +0x1424 clear -> postNotification (thunk 0x1b3b) on the engine with (o, 3) (0x85d4-0x85e9 -> 0x863c); bit 0
//      of +0xc8 and bit 7 of +0xcc both clear -> (o, 5) (0x85eb-0x860d); bit 0 of +0xc8 clear and bit 7 of +0xcc set -> o's
//      slot 0 with 1 (0x8618-0x862f); bit 0 of +0xc8 set -> SoundGroup_rewind (thunk 0x2568) on o (0x8633).
//   4: slot +0x5c; next object = o's slot +0x64; with +0x5c zero and bit 3 of byte +0x58, o's slots +0x80 and +0x38.
//   6: slot +0x5c; next object = o's slot +0x64; with +0x5c zero, o's slot +0x80.
//   other: next object = o's slot +0x64. A NULL next object ends (0x865d). Returns 0 (0x8667).
// neg 1: kind 6 skips its slot +0x80 call.
void flush_unlink(Ctx &c, u32 list, u32 obj) {
  u32 n = ld<u32>(c, list);
  while (n != 0 && ld<u32>(c, n + 8) != obj) n = ld<u32>(c, n + 4);
  if (n == 0) return;
  const u32 prev = ld<u32>(c, n);
  if (prev != 0) st<u32>(c, prev + 4, ld<u32>(c, n + 4));
  else st<u32>(c, list, ld<u32>(c, n + 4));
  const u32 next = ld<u32>(c, n + 4);
  if (next != 0) {
    st<u32>(c, next, ld<u32>(c, n));
    st<u32>(c, list + 8, next);
  } else {
    st<u32>(c, list + 8, 0);
    st<u32>(c, list + 4, ld<u32>(c, n));
  }
  snd_delete(c, n);
  st<int32_t>(c, list + 0xc, sub32(ld<int32_t>(c, list + 0xc), 1));
}
void a_flushSoundList(Ctx &c, const uint32_t *, Ret &r) {
  r.set_u32(0);
  snd_import(c, kIatTimeGetTime, SndAbi::Stdcall, {});
  const u32 eng = snd_obj(c, &SoundState::g_soundEngine).get();
  const u32 list = eng + 0x1c8;
  u32 cur = ld<u32>(c, list);
  st<u32>(c, list + 8, cur);
  if (cur == 0) return;
  u32 o = ld<u32>(c, cur + 8);
  while (o != 0) {
    const u32 kind = snd_vcall(c, o, 0x78, SndAbi::Thiscall, {}).eax;
    const u32 obj = o;
    if (kind == 1) {
      const u32 busy = snd_vcall(c, obj, 0x5c, SndAbi::Thiscall, {}).eax;
      cur = ld<u32>(c, list + 8);
      if (busy != 0) {
        if (cur == 0) return;
        cur = ld<u32>(c, cur + 4);
        st<u32>(c, list + 8, cur);
        if (cur == 0) return;
        o = ld<u32>(c, cur + 8);
        continue;
      }
      o = 0;
      if (cur != 0) {
        cur = ld<u32>(c, cur + 4);
        st<u32>(c, list + 8, cur);
        if (cur != 0) o = ld<u32>(c, cur + 8);
      }
      flush_unlink(c, list, obj);
      if (ld<u32>(c, obj + 0xcc) & 0x400u) snd_call(c, 0x1398, SndAbi::Thiscall, {obj + 0x70});
      st<u32>(c, obj + 0xc8, ld<u32>(c, obj + 0xc8) & 0xffffffbfu);
      const u32 f58 = ld<u32>(c, obj + 0x58);
      if ((f58 & 2u) || (((f58 >> 4) & 1u) && !(ld<uint8_t>(c, obj + 0x1424) & 1u))) {
        snd_call(c, 0x1b3b, SndAbi::Thiscall, {snd_obj(c, &SoundState::g_soundEngine).get(), obj, 3});
        continue;
      }
      const u32 b0 = ld<u32>(c, obj + 0xc8) & 1u;
      const u32 b7 = (ld<u32>(c, obj + 0xcc) >> 7) & 1u;
      if (!b0 && !b7) {
        snd_call(c, 0x1b3b, SndAbi::Thiscall, {snd_obj(c, &SoundState::g_soundEngine).get(), obj, 5});
        continue;
      }
      if (!b0 && ((ld<u32>(c, obj + 0xcc) >> 7) & 1u)) {
        if (obj != 0) snd_vcall(c, obj, 0, SndAbi::Thiscall, {1});
        continue;
      }
      snd_call(c, 0x2568, SndAbi::Thiscall, {obj});
      continue;
    }
    if (kind == 4) {
      const u32 busy = snd_vcall(c, obj, 0x5c, SndAbi::Thiscall, {}).eax;
      o = snd_vcall(c, obj, 0x64, SndAbi::Thiscall, {}).eax;
      if (!busy && (ld<uint8_t>(c, obj + 0x58) & 8u)) {
        snd_vcall(c, obj, 0x80, SndAbi::Thiscall, {});
        snd_vcall(c, obj, 0x38, SndAbi::Thiscall, {});
      }
      continue;
    }
    if (kind == 6) {
      const u32 busy = snd_vcall(c, obj, 0x5c, SndAbi::Thiscall, {}).eax;
      o = snd_vcall(c, obj, 0x64, SndAbi::Thiscall, {}).eax;
      if (!busy && NEG(flushSoundList) != 1) snd_vcall(c, obj, 0x80, SndAbi::Thiscall, {});
      continue;
    }
    o = snd_vcall(c, obj, 0x64, SndAbi::Thiscall, {}).eax;
  }
}

// listNode3Init 0x87f0 (c3dj): +0, +4, +8 = 0 (0x87f4-0x87f9), returns this. neg 1: +8 untouched.
void a_listNode3Init(Ctx &c, const uint32_t *a, Ret &r) {
  st<u32>(c, a[0], 0);
  st<u32>(c, a[0] + 4, 0);
  if (NEG(listNode3Init) != 1) st<u32>(c, a[0] + 8, 0);
  r.set_u32(a[0]);
}
// pairStore 0x8810 (c3ar): +0 = first argument (0x881a), +4 = second (0x881c), returns this. neg 1: the arguments swapped.
void a_pairStore(Ctx &c, const uint32_t *a, Ret &r) {
  const bool sw = NEG(pairStore) == 1;
  st<u32>(c, a[0], sw ? a[2] : a[1]);
  st<u32>(c, a[0] + 4, sw ? a[1] : a[2]);
  r.set_u32(a[0]);
}
// pairClear 0x8840 (c3dj): +4 = 0 (0x8840), +0 = 0 (0x8847); eax as the caller left it. neg 1: +0 untouched.
void a_pairClear(Ctx &c, const uint32_t *a, Ret &) {
  st<u32>(c, a[0] + 4, 0);
  if (NEG(pairClear) != 1) st<u32>(c, a[0], 0);
}
// NodeB000_initBase 0x2e3f0 (c3fk): +4, +8, +0xc, +0x10, +0, +0x14, +0x18, +0x1c = 0, +0x20 = -1 (0x2e3f4-0x2e40b); returns
// this. neg 1: +0x20 = 0.
void a_NodeB000_initBase(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = a[0];
  for (u32 o : {4u, 8u, 0xcu, 0x10u, 0u, 0x14u, 0x18u, 0x1cu}) st<u32>(c, s + o, 0);
  st<u32>(c, s + 0x20, NEG(NodeB000_initBase) == 1 ? 0u : 0xffffffffu);
  r.set_u32(s);
}
// NodeB000_ctor 0xb000 (c3fk): NodeB000_initBase through the thunk 0x12a3 (0xb003, eax unused), +0x24 .. +0x34 = 0
// (0xb00a-0xb016), returns this. neg 1: +0x34 untouched.
void a_NodeB000_ctor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = a[0];
  snd_call(c, 0x12a3, SndAbi::Thiscall, {s});
  for (u32 o : {0x24u, 0x28u, 0x2cu, 0x30u}) st<u32>(c, s + o, 0);
  if (NEG(NodeB000_ctor) != 1) st<u32>(c, s + 0x34, 0);
  r.set_u32(s);
}
// getSpanLength 0xb030 (c3an): [+0xc] - [+8] + 1, wrapping. neg 1: without the + 1.
void a_getSpanLength(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32(ld<u32>(c, a[0] + 0xc) - ld<u32>(c, a[0] + 8) + (NEG(getSpanLength) == 1 ? 0u : 1u));
}
// cmdNode::scalarDeletingDtor 0xcc40 (c3dj): pairClear through the thunk 0x1816 (0xcc43); bit 0 of the flags -> operator delete
// of this; returns this. neg 1: deletes on bit 1 instead.
void a_cmdNode_scalarDeletingDtor(Ctx &c, const uint32_t *a, Ret &r) {
  snd_call(c, 0x1816, SndAbi::Thiscall, {a[0]});
  if (a[1] & (NEG(cmdNode_scalarDeletingDtor) == 1 ? 2u : 1u)) snd_delete(c, a[0]);
  r.set_u32(a[0]);
}
// removeSoundNode 0xe050 (c3do): the list at this+0x1c8; empty or value not found returns 0x24 (0xe05c / 0xe06c / 0xe06f);
// the found node's value kept (0xe0b9), the node unlinked (0xe07c-0xe0cb); a NULL value returns 0x24 (0xe0d5), else bit 6 of
// value+0xc8 is cleared (0xe0d7-0xe0e0) and 0 returned. neg 1: unlink_node's cursor mistake.
void a_removeSoundNode(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 hdr = a[0] + 0x1c8, arg = a[1];
  r.set_u32(0x24);
  if (ld<u32>(c, hdr) == 0) return;
  const u32 node = find_node(c, hdr, arg);
  if (node == 0) return;
  const u32 value = ld<u32>(c, node + 8);
  unlink_node(c, hdr, node, NEG(removeSoundNode));
  if (value == 0) return;
  st<u32>(c, arg + 0xc8, ld<u32>(c, arg + 0xc8) & 0xffffffbfu);
  r.set_u32(0);
}
// voiceListRemove 0x32440 (c3do): a NULL argument returns at once (0x3244c, eax as the caller left it); arg+0x20 = 0 first
// (0x3244e); the list at this+0x1338: empty or not found ends; the found node is unlinked (0x3246c-0x324b7). neg 1: arg+0x20
// not cleared.
void a_voiceListRemove(Ctx &c, const uint32_t *a, Ret &) {
  const u32 arg = a[1];
  if (arg == 0) return;
  if (NEG(voiceListRemove) != 1) st<u32>(c, arg + 0x20, 0);
  const u32 hdr = a[0] + 0x1338;
  if (ld<u32>(c, hdr) == 0) return;
  const u32 node = find_node(c, hdr, arg);
  if (node != 0) unlink_node(c, hdr, node, 0);
}
// addToSoundList 0xe120 (c3dj): appendNodeC (thunk 0x2351, 0xe12c) on the list at this+0x1c8 with the argument; then bit 6 of
// the low byte of arg+0xc8 set (0xe131-0xe139), that dword returned. neg 1: bit 5 instead.
void a_addToSoundList(Ctx &c, const uint32_t *a, Ret &r) {
  snd_call(c, 0x2351, SndAbi::Thiscall, {a[0] + 0x1c8, a[1]});
  u32 f = ld<u32>(c, a[1] + 0xc8);
  f = (f & ~0xffu) | ((f | (NEG(addToSoundList) == 1 ? 0x20u : 0x40u)) & 0xffu);
  st<u32>(c, a[1] + 0xc8, f);
  r.set_u32(f);
}
// stubReturnBB 0xe570 (C2 note re/analysis/audio/0000e570_sound_stubReturnBB.md): `mov eax, 0xb; ret 4`. neg 1: returns 0xc.
void a_stubReturnBB(Ctx &, const uint32_t *, Ret &r) { r.set_u32(NEG(stubReturnBB) == 1 ? 0xcu : 0xbu); }
// seekListToIndex 0x116a0 (c3au): count (+0x148) 0 returns 3 (0x116ad); index >= count (unsigned) returns 0xa (0x116bf); a
// parked cursor (+8) returns 6 (0x116d0); the walk from the head (+4) stores each node at +8 before the exit test
// (0x116e4), leaves on index (0x116e7), follows +0x10 (0x116e9), at most count steps (0x116ed); returns 0. neg 1: the walk
// stops one node early.
void a_seekListToIndex(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], index = a[1];
  const u32 count = ld<u32>(c, self + 0x148);
  if (count == 0) return r.set_i32(3);
  if (index >= count) return r.set_i32(0xa);
  if (ld<u32>(c, self + 8) != 0) return r.set_i32(6);
  const u32 target = (NEG(seekListToIndex) == 1 && index > 0) ? index - 1 : index;
  u32 node = ld<u32>(c, self + 4);
  for (u32 i = 0;;) {
    const bool last = i == target;
    st<u32>(c, self + 8, node);
    if (last) break;
    node = ld<u32>(c, node + 0x10);
    if (++i >= count) break;
  }
  r.set_i32(0);
}
// getNodeByIndex 0x11790 (c3cx): index >= count (+0x148, unsigned) returns 0xa (0x1179e); head (+4) NULL returns 3 (0x117ad);
// index steps along +0x10, a NULL node returns 3 (0x117bd / 0x117c9); _strncpy(dst, node->+0, count) (0x117e0); returns 0.
// neg 1: copies count - 1 bytes.
void a_getNodeByIndex(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], index = a[1], dst = a[2], count = a[3];
  if (index >= ld<u32>(c, self + 0x148)) return r.set_i32(0xa);
  u32 node = ld<u32>(c, self + 4);
  if (node == 0) return r.set_i32(3);
  for (u32 i = 0; i < index; ++i) {
    if (node == 0) return r.set_i32(3);
    node = ld<u32>(c, node + 0x10);
  }
  if (node == 0) return r.set_i32(3);
  snd_call(c, snd_crt::strncpy_, SndAbi::Cdecl,
           {dst, ld<u32>(c, node), NEG(getNodeByIndex) == 1 ? count - 1 : count});
  r.set_i32(0);
}
// StrPtr_assign 0x1b160 (c3fk): NULL s returns 0xa (0x1b16d); a non-NULL *this goes to operator delete (0x1b176-0x1b17d);
// operator new(strlen(s) + 1) (0x1b192, no NULL test) stored at *this (0x1b1a3), s copied with its NUL (0x1b1a5-0x1b1bd);
// returns 0. neg 1: returns 1.
void a_StrPtr_assign(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], s = a[1];
  if (s == 0) return r.set_u32(0xa);
  const u32 old = ld<u32>(c, self);
  if (old != 0) snd_delete(c, old);
  dup_into(c, self, s);
  r.set_u32(NEG(StrPtr_assign) == 1 ? 1u : 0u);
}
// ChainObject_dtor 0x1b570 (c3do): ecx is the sub-object at +0x2c; the Ctrl vtable (vt_CtrlObject) stored at the sub-object
// (0x1b599), the object's own vtable (vt_Ctrl5b58c) at its start (0x1b59f); then two loops over the list in the object (head
// +0xc, back +0x10, count +0x18): loop 1 pops while the head is non-NULL, a NULL value ends it, a non-NULL value gets its
// slot 0 with 1 (0x1b5ab-0x1b5e5); loop 2 pops without the call (0x1b5e7-0x1b651). neg 1: loop 1 passes 0 to slot 0.
void drain_list(Ctx &c, u32 self, int neg) {
  for (int loop = 1; loop <= 2; ++loop) {
    for (;;) {
      const u32 node = ld<u32>(c, self + 0xc);
      if (node == 0) break;
      const u32 next = ld<u32>(c, node + 4);
      if (next != 0) {
        st<u32>(c, next, 0);
        st<u32>(c, self + 0xc, next);
      } else {
        st<u32>(c, self + 0x10, 0);
        st<u32>(c, self + 0xc, 0);
      }
      const u32 value = ld<u32>(c, node + 8);
      snd_delete(c, node);
      st<int32_t>(c, self + 0x18, sub32(ld<int32_t>(c, self + 0x18), 1));
      if (value == 0) break;
      if (loop == 1) snd_vcall(c, value, 0, SndAbi::Thiscall, {neg == 1 ? 0u : 1u});
    }
  }
}
void a_ChainObject_dtor(Ctx &c, const uint32_t *a, Ret &) {
  const u32 sub = a[0], self = sub - 0x2c;
  st<u32>(c, sub, snd_va(c, snd_rva::vt_CtrlObject));
  st<u32>(c, self, snd_va(c, snd_rva::vt_Ctrl5b58c));
  drain_list(c, self, NEG(ChainObject_dtor));
}
// ChainObject_scalarDtor 0x1bea0 (c3do): ChainObject_dtor through the thunk 0x156e on the sub-object (0x1bea3); bit 0 of the
// flags -> operator delete of the object start (sub - 0x2c, 0x1beb3); returns the object start (0x1bebb). neg 1: returns
// the sub-object.
void a_ChainObject_scalarDtor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 sub = a[0];
  snd_call(c, 0x156e, SndAbi::Thiscall, {sub});
  const u32 self = sub - 0x2c;
  if (a[1] & 1u) snd_delete(c, self);
  r.set_u32(NEG(ChainObject_scalarDtor) == 1 ? sub : self);
}
// ListValues_callSlot18 0x1bc00 (c3dt): head (+0xc) copied to the cursor (+0x14) (0x1bc08); NULL returns 0; a node with a NULL
// value returns the node (0x1bc12); else the value's slot +0x18 (0x1bc16), the cursor re-read (0x1bc19, NULL returns 0), its
// next (+4) becomes the cursor (0x1bc25, NULL returns 0), a NULL value there returns the node (0x1bc2f), else repeat.
// neg 1: the cursor is not advanced in memory (the walk uses a local copy).
void a_ListValues_callSlot18(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  u32 node = ld<u32>(c, self + 0xc);
  st<u32>(c, self + 0x14, node);
  if (node == 0) return r.set_u32(0);
  u32 obj = ld<u32>(c, node + 8);
  if (obj == 0) return r.set_u32(node);
  u32 local = node;
  for (;;) {
    snd_vcall(c, obj, 0x18, SndAbi::Thiscall, {});
    u32 cur = NEG(ListValues_callSlot18) == 1 ? local : ld<u32>(c, self + 0x14);
    if (cur == 0) return r.set_u32(0);
    cur = ld<u32>(c, cur + 4);
    local = cur;
    if (NEG(ListValues_callSlot18) != 1) st<u32>(c, self + 0x14, cur);
    if (cur == 0) return r.set_u32(0);
    obj = ld<u32>(c, cur + 8);
    if (obj == 0) return r.set_u32(cur);
  }
}
// PtrList_init 0x1d130 (c3dj): +0, +4, +8, +0xc = 0 (0x1d134-0x1d13c), returns this. neg 1: +0xc untouched.
void a_PtrList_init(Ctx &c, const uint32_t *a, Ret &r) {
  for (u32 o : {0u, 4u, 8u}) st<u32>(c, a[0] + o, 0);
  if (NEG(PtrList_init) != 1) st<u32>(c, a[0] + 0xc, 0);
  r.set_u32(a[0]);
}
// byteSwap16 0x1e6e0 (c3ar): the low 16 bits of the argument with their bytes exchanged; the high half of eax is 0 (the loop
// counter 2 when ax is written). neg 1: the bytes not exchanged.
void a_byteSwap16(Ctx &, const uint32_t *a, Ret &r) {
  const u32 v = a[0];
  r.set_u32(NEG(byteSwap16) == 1 ? (v & 0xffffu) : (((v & 0xffu) << 8) | ((v >> 8) & 0xffu)));
}
// NodeList_initNode 0x25010 (c3ar): +0 = 0 (0x25016), +4 = 0 (0x2501c), +8 = the argument (0x25023), returns this.
// neg 1: +8 = argument + 1.
void a_NodeList_initNode(Ctx &c, const uint32_t *a, Ret &r) {
  st<u32>(c, a[0], 0);
  st<u32>(c, a[0] + 4, 0);
  st<u32>(c, a[0] + 8, a[1] + (NEG(NodeList_initNode) == 1 ? 1u : 0u));
  r.set_u32(a[0]);
}
// randRange 0x28560 (c3an): state = state * 0x19660d + 0x3c6ef35f stored back (0x28562-0x2856d); returns
// ((state >> 16) * (n & 0xffff)) >> 16 with a 32-bit multiply (0x28573-0x2857f). neg 1: state >> 15.
void a_randRange(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = ld<u32>(c, a[0]) * 0x19660du + 0x3c6ef35fu;
  st<u32>(c, a[0], s);
  r.set_u32(((s >> (NEG(randRange) == 1 ? 15 : 16)) * (a[1] & 0xffffu)) >> 16);
}
// nextRandomFloat 0x285a0 (c3ar): the same LCG step stored back (0x285a3-0x285ae); the low 23 bits with the exponent of 1.0
// (0x285b0 / 0x285b5) loaded as a single (0x285be) minus the single g_float1 of .rdata (0x285c2); the difference of two
// values in [1, 2) is exact at any precision, returned in st(0). neg 1: the subtrahend is not loaded (returns [1, 2)).
// On the x87 unit (S4.2b): fld dword (0x285be) pushes the value, fsub dword (0x285c2) leaves the difference in st(0) for the
// caller; Ret carries it as the caller's fstp stores it.
void a_nextRandomFloat(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 s = ld<u32>(c, a[0]) * 0x19660du + 0x3c6ef35fu;
  st<u32>(c, a[0], s);
  const u32 bits = (s & ((1u << 23) - 1u)) | 0x3f800000u;
  SG_X87_GUARD(c, {
    x87::Unit &u = x87::unit(c);
    u.push(x87::fld32(bits));
    if (NEG(nextRandomFloat) != 1)
      u.st(0) = x87::sub(u.st(0), x87::from_f32(ld<u32>(c, snd_va(c, snd_rva::g_float1))), u.cw);
    r.set_double(x87::to_double(u.st(0), u.cw));
  })
}
// sound.dll's __ftol 0x42550 (static CRT; moved here from S4.4's audio_mixer by S4.2b): fnstcw [ebp-2]; the word with
// `or ah, 0xc` (rounding control 11, chop) loaded; fistp qword [ebp-0xc]; the caller's word back; eax:edx = the qword
// (__ftol+0x7..+0x22). neg 1: the rounding control of the caller's word is kept (no chop).
void a_crt_ftol(Ctx &c, const uint32_t *, Ret &r) {
  SG_X87_GUARD(c, {
    x87::Unit &u = x87::unit(c);
    const uint16_t w = NEG(crt_ftol) == 1 ? u.cw : static_cast<uint16_t>(u.cw | 0x0c00u);
    const int64_t q = x87::fist(u.st(0), 64, w);
    u.pop();
    r.eax = static_cast<uint32_t>(static_cast<uint64_t>(q));
    r.edx = static_cast<uint32_t>(static_cast<uint64_t>(q) >> 32);
  })
}
// unlinkFromActiveList 0x2b090 (c3dj): bit 1 of byte +0x44 clear returns 0 (0x2b090-0x2b096); otherwise the unlink block:
// prev (+0x48) -> its +0x4c takes this->+0x4c, else g_activeListHead does; next (+0x4c) -> its +0x48 takes this->+0x48, else
// the tail (.data 0x5206c) does; the flag dword re-read, bit 1 of its low byte cleared; +0x4c = 0, +0x48 = 0, +0x44 = flags
// (0x2b099-0x2b0e6); returns 0. neg 1: the tail is not updated.
void a_unlinkFromActiveList(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0];
  r.set_u32(0);
  if ((ld<uint8_t>(c, self + 0x44) & 2u) == 0) return;
  const u32 head = snd_obj(c, &SoundState::g_activeListHead).va(), tail = snd_data_va(c) + kActiveListTailOff;
  const u32 prev = ld<u32>(c, self + 0x48);
  if (prev != 0) st<u32>(c, prev + 0x4c, ld<u32>(c, self + 0x4c));
  else st<u32>(c, head, ld<u32>(c, self + 0x4c));
  const u32 next = ld<u32>(c, self + 0x4c);
  if (next != 0) st<u32>(c, next + 0x48, ld<u32>(c, self + 0x48));
  else if (NEG(unlinkFromActiveList) != 1) st<u32>(c, tail, ld<u32>(c, self + 0x48));
  u32 flags = ld<u32>(c, self + 0x44);
  flags = (flags & ~0xffu) | (flags & 0xfdu);
  st<u32>(c, self + 0x4c, 0);
  st<u32>(c, self + 0x48, 0);
  st<u32>(c, self + 0x44, flags);
}
// Object_setName 0x2bb10 (c3dj): NULL returns 0xa (0x2bb6e); a non-NULL +0x50 goes to operator delete (0x2bb24); +0x50 takes a
// fresh copy (0x2bb39 operator new, 0x2bb4a store, copy to 0x2bb65); returns 0. neg 1: returns the copied length.
void a_Object_setName(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 self = a[0], name = a[1];
  if (name == 0) return r.set_u32(0xa);
  const u32 old = ld<u32>(c, self + 0x50);
  if (old != 0) snd_delete(c, old);
  const u32 len = dup_into(c, self + 0x50, name);
  r.set_u32(NEG(Object_setName) == 1 ? len : 0u);
}
// getField204 0x2d430 (c3fk): `mov eax, [ecx+0x204]`. neg 1: reads +0x200.
void a_getField204(Ctx &c, const uint32_t *a, Ret &r) {
  r.set_u32(ld<u32>(c, a[0] + (NEG(getField204) == 1 ? 0x200u : 0x204u)));
}
// MemBlock_deleteDtor 0x38ae0 (c3dj): a non-NULL +0 goes to operator delete (0x38aea); +0 = 0 (0x38af6); bit 0 of the flags
// -> operator delete of this (0x38b01); returns this. neg 1: +0 kept.
void a_MemBlock_deleteDtor(Ctx &c, const uint32_t *a, Ret &r) {
  const u32 data = ld<u32>(c, a[0]);
  if (data != 0) snd_delete(c, data);
  if (NEG(MemBlock_deleteDtor) != 1) st<u32>(c, a[0], 0);
  if (a[1] & 1u) snd_delete(c, a[0]);
  r.set_u32(a[0]);
}
// setField6cReturn0 0x39450 / clearField6cReturn0 0x39470 (c3ex): +0x6c = the argument / 0; returns 0. neg 1: returns 1.
void a_setField6cReturn0(Ctx &c, const uint32_t *a, Ret &r) {
  st<u32>(c, a[0] + 0x6c, a[1]);
  r.set_u32(NEG(setField6cReturn0) == 1 ? 1u : 0u);
}
void a_clearField6cReturn0(Ctx &c, const uint32_t *a, Ret &r) {
  st<u32>(c, a[0] + 0x6c, 0);
  r.set_u32(NEG(clearField6cReturn0) == 1 ? 1u : 0u);
}
// the static CRT's strncpy, reached by rva (snd_crt::strncpy_). neg 1: no NUL padding.
void a_snd_strncpy(Ctx &c, const uint32_t *a, Ret &r) {
  if (NEG(snd_strncpy) == 1) {
    u32 i = 0;
    for (; i < a[2]; ++i) {
      const uint8_t b = ld<uint8_t>(c, a[1] + i);
      st<uint8_t>(c, a[0] + i, b);
      if (b == 0) break;
    }
    return r.set_u32(a[0]);
  }
  r.set_u32(crt_strncpy(c, a[0], a[1], a[2]));
}

int g_negctl_none = 0;

#define F(name, abi, ret, n, fn) {name, abi, ret, n, 0, fn, &g_negctl_##fn},
const FuncDef kFuncs[] = {
    // configuration entries (audio_core.*): not sound.dll functions
    {"audio_core.attach", "default", "uint32", 2, 0, a_attach, &g_negctl_none},
    {"audio_core.detach", "default", "void", 0, 0, a_detach, &g_negctl_none},
    {"audio_core.claims", "default", "uint32", 0, 0, a_claims, &g_negctl_none},
    {"audio_core.useScript", "default", "uint32", 0, 0, a_use_script, &g_negctl_none},
    {"audio_core.useCallback", "default", "uint32", 0, 0, a_use_callback, &g_negctl_none},
    {"audio_core.setup", "default", "uint32", 2, 0, a_setup, &g_negctl_none},
    {"audio_core.noHost", "default", "void", 0, 0, a_no_host, &g_negctl_none},
    {"audio_core.reset", "default", "void", 0, 0, a_reset, &g_negctl_none},
    {"audio_core.report", "default", "uint32", 0, 0, a_report, &g_negctl_none},
    {"audio_core.useSink", "default", "void", 1, 0, a_use_sink, &g_negctl_none},
    {"audio_core.sinkLog", "default", "uint32", 0, 0, a_sink_log, &g_negctl_none},
    {"audio_core.sinkObject", "default", "uint32", 2, 0, a_sink_object, &g_negctl_none},
    {"audio_core.dispatch", "default", "uint32", 1, 0, a_dispatch, &g_negctl_none},
    {"audio_core.import", "default", "uint32", 1, 0, a_import, &g_negctl_none},
    {"audio_core.esp", "default", "uint32", 2, 0, a_esp, &g_negctl_none},
    {"audio_core.vector", "default", "uint32", 2, 0, a_vector, &g_negctl_none},
    {"audio_core.finish", "default", "uint32", 0, 0, a_finish, &g_negctl_none},
    {"audio_core.x87load", "default", "uint32", 0, 0, a_x87load, &g_negctl_none},
    {"audio_core.x87save", "default", "uint32", 0, 0, a_x87save, &g_negctl_none},
    {"audio_core.x80soft", "default", "uint32", 1, 0, a_x80soft, &g_negctl_none},
    {"audio_core.x80batch", "default", "uint32", 1, 0, a_x80batch, &g_negctl_none},
#define G(fn) &g_negctl_##fn
    {"snd.appendNodeB", "thiscall", "int", 2, 0, a_appendNodeB, G(appendNodeB)},
    {"snd.appendNodeC", "thiscall", "int", 2, 0, a_appendNodeC, G(appendNodeC)},
    {"snd.appendNode", "thiscall", "int", 2, 0, a_appendNode, G(appendNode)},
    {"snd.PtrList_pushBack", "thiscall", "int", 2, 0, a_PtrList_pushBack, G(PtrList_pushBack)},
    {"snd.NodeList_pushBack", "thiscall", "int", 2, 0, a_NodeList_pushBack, G(NodeList_pushBack)},
    {"snd.ResourceList_pushBack", "thiscall", "int", 2, 0, a_ResourceList_pushBack, G(ResourceList_pushBack)},
    {"snd.nodeListAppendValue", "thiscall", "int", 2, 0, a_nodeListAppendValue, G(nodeListAppendValue)},
    {"snd.listAppendNodeA", "thiscall", "int", 2, 0, a_listAppendNodeA, G(listAppendNodeA)},
    {"snd.listAppendNodeB", "thiscall", "int", 2, 0, a_listAppendNodeB, G(listAppendNodeB)},
    {"snd.NodeList_pushFront", "thiscall", "int", 2, 0, a_NodeList_pushFront, G(NodeList_pushFront)},
    {"snd.PtrList_pushFront", "thiscall", "int", 2, 0, a_PtrList_pushFront, G(PtrList_pushFront)},
    {"snd.clearNodeListD", "thiscall", "void", 1, 0, a_clearNodeListD, G(clearNodeListD)},
    {"snd.clearNodeListE", "thiscall", "void", 1, 0, a_clearNodeListE, G(clearNodeListE)},
    {"snd.clearNodeListF", "thiscall", "void", 1, 0, a_clearNodeListF, G(clearNodeListF)},
    {"snd.clearNodeListG", "thiscall", "void", 1, 0, a_clearNodeListG, G(clearNodeListG)},
    {"snd.clearNodeListC", "thiscall", "void", 1, 0, a_clearNodeListC, G(clearNodeListC)},
    {"snd.clearNodeListA", "thiscall", "void", 1, 0, a_clearNodeListA, G(clearNodeListA)},
    {"snd.clearNodeListB", "thiscall", "void", 1, 0, a_clearNodeListB, G(clearNodeListB)},
    {"snd.PtrList_freeNodesA", "thiscall", "void", 1, 0, a_PtrList_freeNodesA, G(PtrList_freeNodesA)},
    {"snd.NodeList_clear", "thiscall", "void", 1, 0, a_NodeList_clear, G(NodeList_clear)},
    {"snd.PtrList_freeNodesB", "thiscall", "void", 1, 0, a_PtrList_freeNodesB, G(PtrList_freeNodesB)},
    {"snd.nodeListClear2", "thiscall", "void", 1, 0, a_nodeListClear2, G(nodeListClear2)},
    {"snd.nodeListClear", "thiscall", "void", 1, 0, a_nodeListClear, G(nodeListClear)},
    {"snd.listPopFront", "thiscall", "pointer", 1, 0, a_listPopFront, G(listPopFront)},
    {"snd.nodeListPopFront", "thiscall", "pointer", 1, 0, a_nodeListPopFront, G(nodeListPopFront)},
    {"snd.freeDynArray", "thiscall", "void", 1, 0, a_freeDynArray, G(freeDynArray)},
    {"snd.resizeDynArray", "thiscall", "uint32", 2, 0, a_resizeDynArray, G(resizeDynArray)},
    {"snd.flushSoundList", "default", "uint32", 0, 0, a_flushSoundList, G(flushSoundList)},
    {"snd.listNode3Init", "thiscall", "pointer", 1, 0, a_listNode3Init, G(listNode3Init)},
    {"snd.pairStore", "thiscall", "pointer", 3, 0, a_pairStore, G(pairStore)},
    {"snd.pairClear", "thiscall", "void", 1, 0, a_pairClear, G(pairClear)},
    {"snd.NodeB000_ctor", "thiscall", "pointer", 1, 0, a_NodeB000_ctor, G(NodeB000_ctor)},
    {"snd.getSpanLength", "thiscall", "int", 1, 0, a_getSpanLength, G(getSpanLength)},
    {"snd.cmdNode::scalarDeletingDtor", "thiscall", "pointer", 2, 0, a_cmdNode_scalarDeletingDtor,
     G(cmdNode_scalarDeletingDtor)},
    {"snd.removeSoundNode", "thiscall", "uint32", 2, 0, a_removeSoundNode, G(removeSoundNode)},
    {"snd.addToSoundList", "thiscall", "uint32", 2, 0, a_addToSoundList, G(addToSoundList)},
    {"snd.stubReturnBB", "thiscall", "uint32", 2, 0, a_stubReturnBB, G(stubReturnBB)},
    {"snd.findAndUnlinkNodeB", "thiscall", "uint32", 2, 0, a_findAndUnlinkNodeB, G(findAndUnlinkNodeB)},
    {"snd.findAndUnlinkNode", "thiscall", "uint32", 2, 0, a_findAndUnlinkNode, G(findAndUnlinkNode)},
    {"snd.seekListToIndex", "thiscall", "int", 2, 0, a_seekListToIndex, G(seekListToIndex)},
    {"snd.getNodeByIndex", "thiscall", "int", 4, 0, a_getNodeByIndex, G(getNodeByIndex)},
    {"snd.StrPtr_assign", "thiscall", "uint32", 2, 0, a_StrPtr_assign, G(StrPtr_assign)},
    {"snd.ChainObject_dtor", "thiscall", "void", 1, 0, a_ChainObject_dtor, G(ChainObject_dtor)},
    {"snd.ListValues_callSlot18", "thiscall", "uint32", 1, 0, a_ListValues_callSlot18, G(ListValues_callSlot18)},
    {"snd.ChainObject_scalarDtor", "thiscall", "pointer", 2, 0, a_ChainObject_scalarDtor, G(ChainObject_scalarDtor)},
    {"snd.PtrList_init", "thiscall", "pointer", 1, 0, a_PtrList_init, G(PtrList_init)},
    {"snd.byteSwap16", "default", "uint16", 1, 0, a_byteSwap16, G(byteSwap16)},
    {"snd.NodeList_insert", "thiscall", "uint32", 2, 0, a_NodeList_insert, G(NodeList_insert)},
    {"snd.PtrList_insert", "thiscall", "uint32", 2, 0, a_PtrList_insert, G(PtrList_insert)},
    {"snd.NodeList_initNode", "thiscall", "pointer", 2, 0, a_NodeList_initNode, G(NodeList_initNode)},
    {"snd.randRange", "thiscall", "uint32", 2, 0, a_randRange, G(randRange)},
    {"snd.nextRandomFloat", "thiscall", "float", 1, 0, a_nextRandomFloat, G(nextRandomFloat)},
    {"snd.unlinkFromActiveList", "thiscall", "uint32", 1, 0, a_unlinkFromActiveList, G(unlinkFromActiveList)},
    {"snd.Object_setName", "thiscall", "uint32", 2, 0, a_Object_setName, G(Object_setName)},
    {"snd.getField204", "thiscall", "uint32", 1, 0, a_getField204, G(getField204)},
    {"snd.NodeB000_initBase", "thiscall", "pointer", 1, 0, a_NodeB000_initBase, G(NodeB000_initBase)},
    {"snd.voiceListRemove", "thiscall", "void", 2, 0, a_voiceListRemove, G(voiceListRemove)},
    {"snd.MemBlock_deleteDtor", "thiscall", "pointer", 2, 0, a_MemBlock_deleteDtor, G(MemBlock_deleteDtor)},
    {"snd.setField6cReturn0", "thiscall", "uint32", 2, 0, a_setField6cReturn0, G(setField6cReturn0)},
    {"snd.clearField6cReturn0", "thiscall", "uint32", 1, 0, a_clearField6cReturn0, G(clearField6cReturn0)},
    // static CRT functions sound.dll links that hooks.csv has no row for: reached by rva (kCrtRvas)
    {"audio_core.crt.strncpy", "default", "pointer", 3, 0, a_snd_strncpy, G(snd_strncpy)},
    {"audio_core.crt.ftol", "default", "int64", 0, 0, a_crt_ftol, G(crt_ftol)},
#undef G
};
#undef F
#undef NEG

}  // namespace

// static CRT functions without a hooks.csv row: rva -> registered name (lookup_port consults it after kSoundFunctions)
const char *audio_crt_name(uint32_t rva) {
  if (rva == snd_crt::strncpy_) return "audio_core.crt.strncpy";
  if (rva == snd_crt::ftol_) return "audio_core.crt.ftol";
  if (const char *n = audio_crt_function_name(rva)) return n;  // S4.11: audio_crt.cpp
  if (const char *n = audio_exe_sound_name(rva)) return n;  // S4.10
  return nullptr;
}

extern const ModuleDef kModule_audio_core = {"audio_core", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
