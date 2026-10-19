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
#include "portsim/arith.h"  // ftol_low32 and the other arithmetic helpers (phase 3.0)

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

class ExeConstants;  // portsim/execonst.h
class GameFiles;     // portsim/gamefiles.h

// The context lacks a resource a port function needs (no ExeConstants or GameFiles attached, or the name is unknown).
// The C ABI reports SG_E_NO_RESOURCE.
struct MissingResource {
  const char *what;
};

// One GameState (zero-filled, on the heap: 3.6 MB) and its address space.
class Ctx {
 public:
  Ctx();
  // Read-only constants of the user's own exe (phase 3.0) and the user's game files: attached by the caller, not
  // owned; the accessors throw MissingResource when nothing is attached.
  const ExeConstants *exe_ptr = nullptr;
  GameFiles *files_ptr = nullptr;
  const ExeConstants &exe() const {
    if (!exe_ptr) throw MissingResource{"exe constants"};
    return *exe_ptr;
  }
  GameFiles &files() const {
    if (!files_ptr) throw MissingResource{"game files"};
    return *files_ptr;
  }
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

// A defect of the original the port reproduces behind a named compatibility switch (phase 3.0; port/CORE.md). `*flag`
// is 1 (default) = behave like the original, 0 = the fixed behaviour. Module code reads it through SG_DEFECT(var).
struct DefectDef {
  const char *name;  // "<function>.<what>", e.g. "sinScaled.readPastTable"
  const char *ref;   // re/analysis/data/model_defects.md entry, e.g. "model_defects#10"
  const char *what;  // one line
  int *flag;
};

struct ModuleDef {
  const char *module;
  const FuncDef *funcs;
  size_t count;
  const DefectDef *defects = nullptr;  // phase 3.0: the module's compatibility switches (may be empty)
  size_t defect_count = 0;
};

#define SG_DEFECT(var) ((var) != 0)

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

}  // namespace portsim

#endif  // PORTSIM_REGISTRY_H
