// Typed pointers over the context's 32-bit virtual address space, GameState accessors and the original's string and
// copy idioms (phase 3.0, 2026-10-19; decision re/model/DECISIONS.md "pointer representation for phase 3", U-0724).
//
// A Ptr<T> is a 32-bit virtual address plus the Memory that resolves it. Every read and write goes through
// Memory::at, so an access outside every mapping throws portsim::Fault (SG_E_FAULT), exactly where the original would
// have read foreign memory or faulted. Pointer arithmetic wraps modulo 2^32 like the original's `lea`, so an index the
// original never bounds-checks (a negative tile type, a direction past 7) reads the same neighbouring object of the
// GameState image as the original did. Port code never holds a native pointer into state across calls and never
// writes an address literal: it gets addresses from arguments, from state, or from the accessors below.
//
//   Ptr<int8_t> types = state_arr(ctx, &GameState::g_tileType);     // element 0 of an array member
//   int8_t t = state_grid(ctx, &GameState::g_tileType, 50)(x, y);    // x * 50 + y, wrapping, unchecked
//   Ptr<Golfer> g = state_arr(ctx, &GameState::g_golfers) + i;
//   int16_t ty = g.field(&Golfer::type).get();                        // a member of a model struct (pack(1))
//   Ptr<Random> rng = state_obj(ctx, &GameState::g_rng);              // a scalar or struct member
#ifndef PORTSIM_PTR_H
#define PORTSIM_PTR_H

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "portsim/arith.h"
#include "portsim/registry.h"

namespace portsim {

namespace detail {
// offset of a data member of a small model struct (pack(1), standard layout), computed on a zeroed probe object
template <class S, class M> uint32_t member_offset(M S::*m) {
  static_assert(sizeof(S) <= 0x10000, "member_offset is for model record types; use state_obj for GameState");
  alignas(S) static const unsigned char probe[sizeof(S)] = {};
  const S *s = reinterpret_cast<const S *>(probe);
  return static_cast<uint32_t>(reinterpret_cast<const unsigned char *>(&(s->*m)) - probe);
}
}  // namespace detail

template <class T> class Ptr {
 public:
  Ptr() = default;
  Ptr(const Memory &m, uint32_t va) : mem_(&m), va_(va) {}

  uint32_t va() const { return va_; }
  bool null() const { return va_ == 0; }
  explicit operator bool() const { return va_ != 0; }
  const Memory &mem() const { return *mem_; }

  // element access; arithmetic wraps like the original's address computation
  Ptr operator+(int32_t n) const { return Ptr(*mem_, va_ + u32(n) * static_cast<uint32_t>(sizeof(T))); }
  Ptr operator-(int32_t n) const { return Ptr(*mem_, va_ - u32(n) * static_cast<uint32_t>(sizeof(T))); }
  Ptr at(int32_t i) const { return *this + i; }
  T get() const { return mem_->load<T>(va_); }
  void set(const T &v) const { mem_->store<T>(va_, v); }
  T operator[](int32_t i) const { return at(i).get(); }
  T operator*() const { return get(); }
  // in-place access (bypasses the copy; the reference is valid until the next mapping change)
  T &ref() const { return mem_->ref<T>(va_, true); }
  const T &cref() const { return mem_->ref<T>(va_, false); }

  // a member of a model struct, or element 0 of an array member
  // (S = T delays the signature to the call, so Ptr<int> etc. still instantiate)
  template <class M, class S = T> Ptr<M> field(M S::*m) const {
    static_assert(std::is_same<S, T>::value, "a member of another struct");
    return Ptr<M>(*mem_, va_ + detail::member_offset(m));
  }
  template <class M, size_t N, class S = T> Ptr<M> field(M (S::*m)[N]) const {
    static_assert(std::is_same<S, T>::value, "a member of another struct");
    return Ptr<M>(*mem_, va_ + detail::member_offset(m));
  }
  // the same address seen as another type (the original's casts), and byte offsets
  template <class U> Ptr<U> as() const { return Ptr<U>(*mem_, va_); }
  Ptr<uint8_t> byte(int32_t off) const { return Ptr<uint8_t>(*mem_, va_ + u32(off)); }

  bool operator==(const Ptr &o) const { return va_ == o.va_; }
  bool operator!=(const Ptr &o) const { return va_ != o.va_; }

 private:
  const Memory *mem_ = nullptr;
  uint32_t va_ = 0;
};

// ---------------------------------------------------------------------------------------- GameState accessors
// The address of a GameState member (a scalar, a struct, or a whole array as one object).
template <class M> Ptr<M> state_obj(Ctx &c, M GameState::*m) { return Ptr<M>(c.mem, c.mem.va_of(&(c.gs().*m))); }
// Element 0 of an array member, typed as the element.
template <class M, size_t N> Ptr<M> state_arr(Ctx &c, M (GameState::*m)[N]) {
  return Ptr<M>(c.mem, c.mem.va_of(&(c.gs().*m)[0]));
}
// The virtual address of a GameState member.
template <class M> uint32_t state_va(Ctx &c, M GameState::*m) { return c.mem.va_of(&(c.gs().*m)); }

// A row-major grid over an array member: cell(x, y) = element x * width + y, computed with the original's wrapping
// arithmetic and NO bound check (the original's tile accessors have none: model_defects.md section 1).
template <class T> struct Grid {
  Ptr<T> base;
  int32_t width;
  Ptr<T> cell(int32_t x, int32_t y) const { return base + add32(mul32(x, width), y); }
  T operator()(int32_t x, int32_t y) const { return cell(x, y).get(); }
};
template <class M, size_t N> Grid<M> state_grid(Ctx &c, M (GameState::*m)[N], int32_t width) {
  return Grid<M>{state_arr(c, m), width};
}

// ---------------------------------------------------------------------------------------- string and copy idioms
// VC6's inline `rep movsd` + `rep movsb` copy: n / 4 dwords then n % 4 bytes, forward, each unit read then written
// (equal to memmove only when the ranges do not overlap within 4 bytes).
void rep_movs(const Memory &m, uint32_t dst, uint32_t src, uint32_t n);
// strlen by `repne scasb` (faults when no NUL is mapped)
uint32_t vstrlen(const Memory &m, uint32_t s);
// the inline strcat idiom: length of src including its NUL, end of dst, rep_movs; returns dst
uint32_t vstrcat(const Memory &m, uint32_t dst, uint32_t src);
// the inline strcpy idiom; returns dst
uint32_t vstrcpy(const Memory &m, uint32_t dst, uint32_t src);
// the static CRT's strpbrk (bit set of `set` up to its NUL; first byte of s in the set, or 0 at s's NUL)
uint32_t vstrpbrk(const Memory &m, uint32_t s, uint32_t set);

}  // namespace portsim

#endif  // PORTSIM_PTR_H
