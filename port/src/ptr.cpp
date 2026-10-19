// The original's string and copy idioms over the virtual address space (portsim/ptr.h), phase 3.0.
#include "portsim/ptr.h"

namespace portsim {

void rep_movs(const Memory &m, uint32_t dst, uint32_t src, uint32_t n) {
  uint32_t d = n / 4;
  for (; d; --d, dst += 4, src += 4) m.store<uint32_t>(dst, m.load<uint32_t>(src));
  for (uint32_t b = n & 3u; b; --b, ++dst, ++src) m.store<uint8_t>(dst, m.load<uint8_t>(src));
}

uint32_t vstrlen(const Memory &m, uint32_t s) {
  uint32_t n = 0;
  while (m.load<uint8_t>(s + n) != 0) ++n;
  return n;
}

uint32_t vstrcat(const Memory &m, uint32_t dst, uint32_t src) {
  const uint32_t n = vstrlen(m, src) + 1;  // the copy includes the NUL
  rep_movs(m, dst + vstrlen(m, dst), src, n);
  return dst;
}

uint32_t vstrcpy(const Memory &m, uint32_t dst, uint32_t src) {
  rep_movs(m, dst, src, vstrlen(m, src) + 1);
  return dst;
}

uint32_t vstrpbrk(const Memory &m, uint32_t s, uint32_t set) {
  uint32_t bits[8] = {};
  for (uint8_t c; (c = m.load<uint8_t>(set)) != 0; ++set) bits[c >> 5] |= 1u << (c & 31u);
  for (uint8_t c; (c = m.load<uint8_t>(s)) != 0; ++s)
    if (bits[c >> 5] & (1u << (c & 31u))) return s;
  return 0;
}

}  // namespace portsim
