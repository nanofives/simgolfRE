// portsim internal API for port function authors (phase 2.6, 2026-10-19). How to add a function: port/test/README.md.
//
// A port function is plain C++ over portsim::GameState (named members only, never addresses). To make it testable
// against the vector corpus, its module file also defines an ADAPTER with the original's interface:
//
//   void adapter(portsim::Ctx &ctx, const uint32_t *args, portsim::Ret &ret);
//
// `args` are the original's 32-bit argument slots (thiscall `this` first); pointers are 32-bit virtual addresses of
// the context's address space, resolved with ctx.mem (a miss throws portsim::Fault, reported as SG_E_FAULT). The
// adapter stores what the original leaves in eax / edx / st(0) in `ret`. Each module lists its adapters in one
// ModuleDef and is named once in port/src/sim/modules.inc.
#ifndef PORTSIM_REGISTRY_H
#define PORTSIM_REGISTRY_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "gamestate.h"

namespace portsim {

// An access no mapping covers (the original would have faulted or read foreign memory).
struct Fault {
  uint32_t va, len;
};

// The context's 32-bit virtual address space: the GameState at `data_base`, plus the caller's mappings.
class Memory {
 public:
  struct Mapping {
    uint32_t base, size;
    uint8_t *host;
    bool readonly;
  };

  Memory(uint8_t *state, uint32_t state_size) : state_(state), state_size_(state_size) {}

  uint32_t data_base() const { return data_base_; }
  bool set_data_base(uint32_t base);
  bool map(uint32_t base, uint8_t *host, uint32_t size, bool readonly);
  void unmap_all() { maps_.clear(); }

  // host pointer to [va, va + len), or throws Fault
  uint8_t *at(uint32_t va, uint32_t len, bool write = false) const;
  // virtual address of host memory inside a mapping (or the state), or throws Fault{0, 0}
  uint32_t va_of(const void *host) const;

  template <class T> T load(uint32_t va) const {
    T v;
    std::memcpy(&v, at(va, sizeof(T)), sizeof(T));
    return v;
  }
  template <class T> void store(uint32_t va, T v) const { std::memcpy(at(va, sizeof(T), true), &v, sizeof(T)); }
  // a reference to an object at `va` (GameState types are pack(1); x86 / x64 allow unaligned access)
  template <class T> T &ref(uint32_t va, bool write = true) const {
    return *reinterpret_cast<T *>(at(va, sizeof(T), write));
  }

 private:
  uint8_t *state_;
  uint32_t state_size_;
  uint32_t data_base_ = 0x20000000u;
  std::vector<Mapping> maps_;
};

// One GameState (zero-filled, on the heap: 3.6 MB) and its address space.
class Ctx {
 public:
  Ctx();
  GameState &gs() { return *gs_; }
  uint8_t *bytes() { return reinterpret_cast<uint8_t *>(gs_.get()); }

 private:
  std::unique_ptr<GameState> gs_;  // declared before `mem`, which is built from it

 public:
  Memory mem;
};

// The original's register results.
struct Ret {
  uint32_t eax = 0, edx = 0;
  double st0 = 0.0;
  bool has_st0 = false;
  void set_u32(uint32_t v) { eax = v; }
  void set_i32(int32_t v) { eax = static_cast<uint32_t>(v); }
  void set_double(double v) {
    st0 = v;
    has_st0 = true;
  }
};

using Adapter = void (*)(Ctx &ctx, const uint32_t *args, Ret &ret);

struct FuncDef {
  const char *name;  // hooks.csv name of the original function
  const char *abi;   // vector corpus key abi: default / thiscall / stdcall / fastcall / mscdecl
  const char *ret;   // vector corpus key ret type
  int arity;         // argument slots
  uint32_t fpcw;     // x87 control word the result depends on (0x027f), 0 = none
  Adapter fn;
  int *negctl;       // the function's negative-control mode (0 = correct body), or nullptr
};

struct ModuleDef {
  const char *module;
  const FuncDef *funcs;
  size_t count;
};

// Negative controls: a module declares `int g_negctl_<x> = 0;` and its body reads it through SG_NEGCTL(g_negctl_<x>).
// With -DPORTSIM_NEGCTL=0 every wrong body compiles away.
#ifndef PORTSIM_NEGCTL
#define PORTSIM_NEGCTL 1
#endif
#if PORTSIM_NEGCTL
#define SG_NEGCTL(var) (var)
#else
#define SG_NEGCTL(var) 0
#endif

// The original's double -> int conversion (__ftol: 64-bit fistp toward zero, low dword kept; NaN or out of int64
// range gives the integer indefinite 0x8000000000000000, low dword 0).
inline uint32_t ftol_low32(double v) {
  if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) return 0u;
  return static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(v)));
}

}  // namespace portsim

#endif  // PORTSIM_REGISTRY_H
