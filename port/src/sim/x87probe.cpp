// TEST-ONLY module (phase 3.R, 2026-10-19): functions whose result depends on the x87 control word and on 80-bit
// intermediates, so the harness can prove that a backend really runs x87 code. They model nothing of the game.
//
//   x87Probe::mulDiv(a, b, c)  st(0) = (a * b) / c with the product kept in an x87 register (80 bits wide, or 53 / 64 as
//                              the precision control says) and the quotient rounded by the rounding control when stored
//   x87Probe::controlWord()    the x87 control word the call runs under (fnstcw), 0x027f on a build without x87
//
// On Win32 the bodies are x87 inline assembly. On x64 there is no x87 code: they compute with SSE2 doubles, which equals the
// Win32 result only under the control word 0x027f (53-bit, nearest). Negative control mode 1 of mulDiv is the classic
// wrong port: plain C doubles on both builds, which ignores the control word and the wide intermediate.
#include <cstring>

#include "portsim/registry.h"

namespace portsim {

namespace {
int g_negctl_x87_mul_div = 0;  // 1 = plain double arithmetic (ignores the control word and the 80-bit product)

double slots_double(const uint32_t *a) {
  double d;
  std::memcpy(&d, a, sizeof d);
  return d;
}

double mul_div(double a, double b, double c) {
  if (SG_NEGCTL(g_negctl_x87_mul_div) == 1) {
    volatile double p = a * b;  // rounded to 53 bits whatever the x87 precision control says
    return p / c;
  }
#if defined(_M_IX86) && defined(_MSC_VER)
  double r;
  __asm {
    fld qword ptr [a]
    fmul qword ptr [b]
    fdiv qword ptr [c]
    fstp qword ptr [r]
  }
  return r;
#else
  return a * b / c;
#endif
}

uint32_t control_word() {
#if defined(_M_IX86) && defined(_MSC_VER)
  unsigned short w;
  __asm fnstcw w
  return w;
#else
  return 0x027Fu;
#endif
}

void a_mul_div(Ctx &, const uint32_t *args, Ret &ret) {
  ret.set_double(mul_div(slots_double(args), slots_double(args + 2), slots_double(args + 4)));
}

void a_control_word(Ctx &, const uint32_t *, Ret &ret) { ret.set_u32(control_word()); }

const FuncDef kFuncs[] = {
    {"x87Probe::mulDiv", "default", "double", 6, 0x027f, a_mul_div, &g_negctl_x87_mul_div},
    {"x87Probe::controlWord", "default", "uint32", 0, 0x027f, a_control_word, nullptr},
};
}  // namespace

extern const ModuleDef kModule_x87probe = {"x87probe", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0])};

}  // namespace portsim
