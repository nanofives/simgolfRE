// shotStyleButtonCode (phase 3.8, stream 38b): the 46-byte leaf mainLoop's H1 calls with a shot-style button number.
// Written from its instructions (re/analysis/ui/0040d760_shotStyleButtonCode.md): the argument minus 3 is compared as an
// unsigned number against 4 and indexes a five-entry jump table; every path ends in `ret` with eax defined.
//   3 -> 0x800043f0   4 -> 0   5 -> 0x8000421f   6 -> 0x80007ff0   7 -> 0x80007fff   any other value -> 0
// What the 0x8000xxxx codes select is not determined (the function is a leaf; the caller stores the result in a frame
// slot). Evidence: the emulator oracle (port/test/test_shotstyle.py) runs the original over every argument.
#include "portsim/sim/shotstyle.h"

#include "portsim/registry.h"

namespace portsim {

namespace {
// Negative controls (port/test/README.md): mode 0 is the correct body.
//   shotStyleButtonCode  1 = buttons 4 and 5 swapped (4 -> 0x8000421f, 5 -> 0)
//                        2 = the range test is `idx > 3` (button 7 maps to 0)
int g_negctl_shot_style = 0;
}  // namespace

uint32_t shot_style_button_code(int32_t button) {
  const int mode = SG_NEGCTL(g_negctl_shot_style);
  // `add eax, -3; cmp eax, 4; ja`: the range test is unsigned, so button < 3 wraps to a huge index
  const uint32_t idx = static_cast<uint32_t>(button) - 3u;
  if (idx > (mode == 2 ? 3u : 4u)) return 0;
  switch (mode == 1 ? (idx == 1 ? 2u : idx == 2 ? 1u : idx) : idx) {
    case 0:
      return 0x800043F0u;
    case 2:
      return 0x8000421Fu;
    case 3:
      return 0x80007FF0u;
    case 4:
      return 0x80007FFFu;
    default:  // 1: the table's second slot is the `xor eax, eax` tail
      return 0;
  }
}

namespace {
void a_shot_style_button_code(Ctx &, const uint32_t *a, Ret &ret) {
  ret.set_u32(shot_style_button_code(static_cast<int32_t>(a[0])));
}

const FuncDef kFuncs[] = {
    {"shotStyleButtonCode", "default", "int", 1, 0, a_shot_style_button_code, &g_negctl_shot_style},
};
}  // namespace

extern const ModuleDef kModule_shotstyle = {"shotstyle", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
