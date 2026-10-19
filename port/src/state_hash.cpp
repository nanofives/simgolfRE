// GameState object table and sgh64 hashes (phase 2.6, 2026-10-19). See portsim/state_hash.h.
#include "portsim/state_hash.h"

#include <cstring>
#include <iterator>
#include <map>
#include <string>

namespace portsim {

namespace {
constexpr uint64_t kMul = 0x9E3779B97F4A7C15ull;
constexpr uint64_t kXor = 0x5BD1E995ull;

uint64_t mix(uint64_t h) {
  h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ull;
  h = (h ^ (h >> 27)) * 0x94D049BB133111EBull;
  return h ^ (h >> 31);
}

const ObjectInfo kObjects[] = {
#define SG_OBJECT(s, off, size, cls, ptr, hid, saved) {#s, off, size, #cls},
#include "gamestate_meta.inc"
};

struct PointerWord {
  const char *name;
  uint32_t off;
};
const PointerWord kPointerWords[] = {
#define SG_POINTER_WORD(s, off) {#s, off},
#include "gamestate_meta.inc"
};

// pointer-word offsets by object index
const std::map<const ObjectInfo *, std::vector<uint32_t>> &pointer_words() {
  static const std::map<const ObjectInfo *, std::vector<uint32_t>> words = [] {
    std::map<const ObjectInfo *, std::vector<uint32_t>> w;
    for (const PointerWord &p : kPointerWords)
      for (const ObjectInfo &o : objects())
        if (std::strcmp(o.name, p.name) == 0) w[&o].push_back(p.off);
    return w;
  }();
  return words;
}
}  // namespace

uint64_t sgh64(const uint8_t *data, size_t n) {
  uint64_t s = 0;
  size_t nw = (n + 3) / 4;
  for (size_t i = 0; i < nw; ++i) {
    uint32_t w = 0;
    size_t take = n - i * 4 < 4 ? n - i * 4 : 4;
    std::memcpy(&w, data + i * 4, take);  // little-endian host (x86 / x64)
    s += (static_cast<uint64_t>(w) ^ kXor) * ((2 * static_cast<uint64_t>(i) + 1) * kMul);
  }
  return mix(s ^ static_cast<uint64_t>(n));
}

const std::vector<ObjectInfo> &objects() {
  static const std::vector<ObjectInfo> all(std::begin(kObjects), std::end(kObjects));
  return all;
}

const std::vector<const ObjectInfo *> &objects_of(const char *cls) {
  static const std::map<std::string, std::vector<const ObjectInfo *>> by_class = [] {
    std::map<std::string, std::vector<const ObjectInfo *>> m;
    for (const ObjectInfo &o : objects()) {
      m[o.cls].push_back(&o);
      m[""].push_back(&o);
    }
    return m;
  }();
  static const std::vector<const ObjectInfo *> none;
  auto it = by_class.find(cls ? cls : "");
  return it == by_class.end() ? none : it->second;
}

uint64_t object_hash(const uint8_t *state, const ObjectInfo &o) {
  auto it = pointer_words().find(&o);
  if (it == pointer_words().end()) return sgh64(state + o.off, o.size);
  std::vector<uint8_t> tmp(state + o.off, state + o.off + o.size);
  for (uint32_t w : it->second)
    if (w + 4 <= o.size) std::memset(tmp.data() + w, 0, 4);
  return sgh64(tmp.data(), tmp.size());
}

}  // namespace portsim
