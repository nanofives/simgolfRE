// The original's integer and conversion arithmetic in portable C++ (phase 3.0, 2026-10-19). Conventions: port/CORE.md.
//
// The original is 32-bit x86 code compiled by VC6. Where C++ differs from what its instructions do, port code uses these
// helpers instead of the C++ operator:
//   - signed + - * and unary minus WRAP modulo 2^32 (C++: undefined on overflow)          -> add32 / sub32 / mul32 / neg32
//   - `idiv` truncates toward zero and RAISES #DE on a zero divisor or INT_MIN / -1       -> div32 / mod32 (DivideError)
//   - shift counts are taken mod 32 (`shl eax, cl`); `sar` is arithmetic                    -> shl32 / sar32 / shr32
//   - `<<` of a negative value is undefined in C++17                                       -> shl32
//   - VC6's inline abs (cdq / xor / sub) keeps INT_MIN                                      -> abs32
//   - movsx / movzx of bytes and words                                                     -> sx8 / sx16 / zx8 / zx16
//   - __ftol (64-bit fistp toward zero, low dword kept; NaN or out of range -> 0)          -> ftol_low32
// Division by a power of two written as `x / 8` in the source compiles to cdq / and / add / sar, which is C++'s `/`
// on int32_t (truncation toward zero, never faults): plain `/` is fine there, as long as the dividend did not overflow.
#ifndef PORTSIM_ARITH_H
#define PORTSIM_ARITH_H

#include <cstdint>

namespace portsim {

// An `idiv` / `div` the original executes with a zero divisor or INT_MIN / -1: the CPU raises #DE and the game dies.
// The C ABI reports it as SG_E_DIVIDE.
struct DivideError {
  int32_t dividend, divisor;
};

// A loop the original can never leave from this state (e.g. a decay step that no longer changes its value). The port
// detects the fixed point and throws instead of hanging; the C ABI reports SG_E_NO_RETURN. `where` names the function.
struct NoReturn {
  const char *where;
};

constexpr int32_t i32(uint32_t v) { return static_cast<int32_t>(v); }
constexpr uint32_t u32(int32_t v) { return static_cast<uint32_t>(v); }

constexpr int32_t add32(int32_t a, int32_t b) { return i32(u32(a) + u32(b)); }
constexpr int32_t sub32(int32_t a, int32_t b) { return i32(u32(a) - u32(b)); }
constexpr int32_t mul32(int32_t a, int32_t b) { return i32(u32(a) * u32(b)); }  // imul r32, r/m32 (low dword)
constexpr int32_t neg32(int32_t a) { return i32(0u - u32(a)); }

// shl / sar / shr with the x86 count rule (count & 31)
constexpr int32_t shl32(int32_t a, int32_t n) { return i32(u32(a) << (u32(n) & 31u)); }
constexpr int32_t sar32(int32_t a, int32_t n) {
  // arithmetic shift written without relying on implementation-defined >> of negatives
  return a >= 0 ? i32(u32(a) >> (u32(n) & 31u)) : i32(~(~u32(a) >> (u32(n) & 31u)));
}
constexpr uint32_t shr32(uint32_t a, int32_t n) { return a >> (u32(n) & 31u); }

// idiv: quotient truncated toward zero, remainder with the dividend's sign
inline int32_t div32(int32_t a, int32_t b) {
  if (b == 0 || (a == INT32_MIN && b == -1)) throw DivideError{a, b};
  return a / b;
}
inline int32_t mod32(int32_t a, int32_t b) {
  if (b == 0 || (a == INT32_MIN && b == -1)) throw DivideError{a, b};
  return a % b;
}
// div r32 (unsigned)
inline uint32_t udiv32(uint32_t a, uint32_t b) {
  if (b == 0) throw DivideError{i32(a), 0};
  return a / b;
}

constexpr int32_t abs32(int32_t v) { return v < 0 ? neg32(v) : v; }  // INT_MIN stays INT_MIN

constexpr int32_t sx8(uint32_t v) { return static_cast<int8_t>(static_cast<uint8_t>(v)); }
constexpr int32_t sx16(uint32_t v) { return static_cast<int16_t>(static_cast<uint16_t>(v)); }
constexpr uint32_t zx8(uint32_t v) { return v & 0xFFu; }
constexpr uint32_t zx16(uint32_t v) { return v & 0xFFFFu; }

// The original's double -> int conversion (__ftol: 64-bit fistp toward zero, low dword kept; NaN or out of int64
// range gives the integer indefinite 0x8000000000000000, low dword 0).
inline uint32_t ftol_low32(double v) {
  if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) return 0u;
  return static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(v)));
}

}  // namespace portsim

#endif  // PORTSIM_ARITH_H
