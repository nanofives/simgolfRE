// The context (GameState + 32-bit address space) and the port function registry (phase 2.6, 2026-10-19).
#include "portsim/registry.h"

#include <cstring>

#include "portsim/registry_list.h"

namespace portsim {

namespace {
// [lo, lo + n) and [lo2, lo2 + n2) intersect (64-bit arithmetic: no wrap)
bool overlaps(uint64_t lo, uint64_t n, uint64_t lo2, uint64_t n2) { return lo < lo2 + n2 && lo2 < lo + n; }
}  // namespace

bool Memory::set_data_base(uint32_t base) {
  if (static_cast<uint64_t>(base) + state_size_ > 0x100000000ull || base == 0) return false;
  for (const Mapping &m : maps_)
    if (overlaps(base, state_size_, m.base, m.size)) return false;
  data_base_ = base;
  return true;
}

bool Memory::map(uint32_t base, uint8_t *host, uint32_t size, bool readonly) {
  if (host == nullptr || size == 0 || base == 0 || static_cast<uint64_t>(base) + size > 0x100000000ull) return false;
  if (overlaps(base, size, data_base_, state_size_)) return false;
  for (const Mapping &m : maps_)
    if (overlaps(base, size, m.base, m.size)) return false;
  maps_.push_back(Mapping{base, size, host, readonly});
  return true;
}

uint8_t *Memory::at(uint32_t va, uint32_t len, bool write) const {
  uint32_t rel = va - data_base_;
  if (rel < state_size_ && len <= state_size_ - rel) return state_ + rel;
  for (const Mapping &m : maps_) {
    rel = va - m.base;
    if (rel < m.size && len <= m.size - rel) {
      if (write && m.readonly) break;
      return m.host + rel;
    }
  }
  throw Fault{va, len};
}

uint32_t Memory::va_of(const void *host) const {
  const uint8_t *p = static_cast<const uint8_t *>(host);
  if (p >= state_ && p < state_ + state_size_) return data_base_ + static_cast<uint32_t>(p - state_);
  for (const Mapping &m : maps_)
    if (p >= m.host && p < m.host + m.size) return m.base + static_cast<uint32_t>(p - m.host);
  throw Fault{0, 0};
}

Ctx::Ctx()
    : gs_(std::make_unique<GameState>()),  // value-initialised: every byte zero
      mem(reinterpret_cast<uint8_t *>(gs_.get()), static_cast<uint32_t>(sizeof(GameState))) {}

// ------------------------------------------------------------------------------------------------ registry
// Every module named in port/src/sim/modules.inc contributes its ModuleDef.
#define SG_MODULE(m) extern const ModuleDef kModule_##m;
#include "sim/modules.inc"
#undef SG_MODULE

namespace {
const ModuleDef *const kModules[] = {
#define SG_MODULE(m) &kModule_##m,
#include "sim/modules.inc"
#undef SG_MODULE
};

struct Entry {
  const FuncDef *def;
  const char *module;
};

const std::vector<Entry> &entries() {
  static const std::vector<Entry> all = [] {
    std::vector<Entry> v;
    for (const ModuleDef *m : kModules)
      for (size_t i = 0; i < m->count; ++i) v.push_back(Entry{&m->funcs[i], m->module});
    return v;
  }();
  return all;
}
}  // namespace

int registry_count() { return static_cast<int>(entries().size()); }

const FuncDef *registry_at(int i, const char **module) {
  if (i < 0 || i >= registry_count()) return nullptr;
  if (module) *module = entries()[static_cast<size_t>(i)].module;
  return entries()[static_cast<size_t>(i)].def;
}

int registry_find(const char *name) {
  if (name == nullptr) return -1;
  const std::vector<Entry> &e = entries();
  for (size_t i = 0; i < e.size(); ++i)
    if (std::strcmp(e[i].def->name, name) == 0) return static_cast<int>(i);
  return -1;
}

}  // namespace portsim
