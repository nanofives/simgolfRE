// Stateful leaves of golf_clean.exe (they write memory), A/B'd with diff_hook's state snapshot/compare/restore
// (`state` regions in re/frida/hooks_registry.py). Reimplemented from re/analysis/<subsystem>/<addr>_<name>.md.
#include "hooks.h"

namespace {

// __thiscall emulated with __fastcall: ecx = this, edx unused.
typedef double(__fastcall* RandomNext_t)(void* self, void* edx);
typedef void(__cdecl* ClearTile_t)(int, int);

// 0x0045c1a0  ?next@Random@@QAENXZ
// seed = seed * 0x41c64e6d + 0x3039 (32-bit wrap) stored back to [this] (0x0045c1b8); returns bits 16..30 of the
// new seed as a double times the constant at 0x004ba800 (1/32768), i.e. a value in [0, 1).
RandomNext_t RandomNext_orig;
double __fastcall RandomNext_re(void* self, void*) {
    unsigned int* seed = static_cast<unsigned int*>(self);
    *seed = *seed * 0x41c64e6du + 0x3039u;
    return static_cast<double>((*seed >> 16) & 0x7fff) * *reinterpret_cast<const double*>(0x004ba800);
}

// 0x00470a10  clearTile(x, y): k = the kind byte of the current course (index dword at 0x0059bf90, 0x2e-byte
// records, byte at 0x00571ff7 + index*0x2e); writes tile type 0x11 when k == 2, else 0x14, to 0x005722e8 + x*50 + y
// (0x00470a45); when k == 2 also ANDs the flags word at 0x0053caf0 + (x*50 + y)*2 with 0xfcdf (0x00470a50).
ClearTile_t ClearTile_orig;
void __cdecl ClearTile_re(int x, int y) {
    const int course = *reinterpret_cast<const int*>(0x0059bf90);
    const signed char k = *reinterpret_cast<const signed char*>(0x00571ff7 + course * 0x2e);
    const int cell = x * 50 + y;
    reinterpret_cast<signed char*>(0x005722e8)[cell] = k == 2 ? 0x11 : 0x14;
    if (k == 2) reinterpret_cast<unsigned short*>(0x0053caf0)[cell] &= 0xfcdf;
}

}  // namespace

SG_HOOK("golf_clean.exe", 0x0045c1a0, Random_next, RandomNext_re, RandomNext_orig);
SG_HOOK("golf_clean.exe", 0x00470a10, clearTile, ClearTile_re, ClearTile_orig);
