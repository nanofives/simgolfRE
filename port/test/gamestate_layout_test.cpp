// The generated GameState layout (include/gamestate.h, gamestate_meta.inc): every object's offset and size are
// asserted at compile time through the meta table, and at run time the table is checked to tile the section exactly
// (objects and gaps in offset order, no hole, no overlap). Specification only: no original data is read.
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "gamestate.h"

#define SG_OBJECT(s, off, size, cls, ptr, hid, saved) \
  static_assert(offsetof(portsim::GameState, s) == (off) && sizeof(portsim::GameState::s) == (size), #s);
#include "gamestate_meta.inc"

namespace {
struct Span { unsigned off, size; };
const Span kSpans[] = {
#define SG_OBJECT(s, off, size, cls, ptr, hid, saved) {off, size},
#define SG_GAP(off, size) {off, size},
#include "gamestate_meta.inc"
};
}  // namespace

extern "C" unsigned portsim_gamestate_size(void);

int main() {
  unsigned pos = 0, objects = 0;
  for (const Span &s : kSpans) {
    if (s.off != pos) {
      std::printf("FAIL: span at 0x%x, expected 0x%x\n", s.off, pos);
      return EXIT_FAILURE;
    }
    pos += s.size;
    ++objects;
  }
  if (pos != sizeof(portsim::GameState) || portsim_gamestate_size() != sizeof(portsim::GameState)) {
    std::printf("FAIL: spans end at 0x%x, GameState is 0x%zx\n", pos, sizeof(portsim::GameState));
    return EXIT_FAILURE;
  }
  auto gs = std::make_unique<portsim::GameState>();   // 3.6 MB: never on the stack
  gs->g_date = 1;                                     // named members, never addresses
  std::printf("OK: %u spans tile 0x%x bytes\n", objects, pos);
  return EXIT_SUCCESS;
}
