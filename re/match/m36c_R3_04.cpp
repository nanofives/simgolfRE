// FLAGS sound.dll: /O2
// m36c R3 (round 36): sound.dll release functions 0x10037780..0x1003a900 (Channel / Voice / MMIO buffer), hand-matched
// from Ghidra's decompilation (raw form: Ghidra's offsets and FUN_ names). Evidence of byte identity, not readable source.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_100377f0 {
struct VFlags { byte b0 : 1; byte b1 : 1; byte b2 : 1; byte b3 : 1; };
struct Voice { char pad[0x9c]; VFlags f; char pad2[0x110 - 0x9d]; };
struct CFlags { uint b0 : 1; uint b1 : 6; uint b7 : 1; };
struct C_FUN_100377f0 {
  char pad[0x58]; CFlags flags; char pad2[0x238 - 0x5c]; Voice voices[16];
  void FUN_100377f0(bool param_1);
};
// MATCH: sound.dll 0x100377f0 ?FUN_100377f0@C_FUN_100377f0@f_100377f0@@QAEX_N@Z
void C_FUN_100377f0::FUN_100377f0(bool param_1)
{
  if (flags.b0) {
    for (int i = 0; i < 16; i++) {
      voices[i].f.b1 = param_1;
    }
    return;
  }
  flags.b7 = param_1;
}
}

namespace f_10037920 {
void __cdecl FUN_1004249a(void *);
struct VFlags { byte b0 : 1; byte b1 : 1; byte b2 : 1; byte b3 : 1; };
struct Voice {
  MMIOINFO info; char pad48[0x88 - sizeof(MMIOINFO)]; int x88; int x8c; int x90; char pad94[0x9c - 0x94]; VFlags vf;
  char pad9d[0xa0 - 0x9d]; WAVEFORMATEX fmt; char padb2[0x110 - 0xa0 - sizeof(WAVEFORMATEX)];
  // MmioBuffer_init 0x10037e10, inlined here
  void init(MMIOINFO *i, WAVEFORMATEX *f) {
    if (i != 0) {
      info = *i;
    }
    x88 = 0;
    x8c = 0;
    x90 = 0;
    fmt = *f;
  }
};
struct C_FUN_10037920 {
  HMMIO hmmio; int x4; MMIOINFO *pinfo; MMIOINFO info; int x54; uint flags; char pad5c[0x164 - 0x5c]; int x164;
  int x168; char pad16c[0x174 - 0x16c]; int x174; int x178; char pad17c[4]; WAVEFORMATEX fmt;
  char pad192[0x238 - 0x180 - sizeof(WAVEFORMATEX)]; Voice voices[16];
  int FUN_10037920(void);
};
// MATCH: sound.dll 0x10037920 ?FUN_10037920@C_FUN_10037920@f_10037920@@QAEHXZ
int C_FUN_10037920::FUN_10037920(void)
{
  int timeout = 0x10000;
  int i;
  if ((flags & 2) == 0) {
    flags |= 1;
    return 0;
  }
  if (((flags & 0x40) != 0) && ((flags & 0x10) == 0)) {
    flags |= 0x10;
    for (i = 0; i < 16; i++) {
      voices[i].vf.b1 = 0;
      voices[i].vf.b2 = 1;
    }
  }
  // The wait loop is not rotated in the original (test at the top, `jmp` back): a local copy of the flags and one
  // break per test give that; `while (a && b && --t)` and `for (;;) { if (a || b || t) break; }` are rotated.
  while (1) {
    uint f = flags;
    if ((f & 0x40) == 0) {
      break;
    }
    if ((f & 0x10) != 0) {
      break;
    }
    if (--timeout == 0) {
      break;
    }
    Sleep(1);
  }
  if (((flags & 0x40) != 0) && ((flags & 0x10) == 0)) {
    return 6;
  }
  mmioGetInfo(hmmio, &info, 0);
  mmioSetBuffer(hmmio, 0, x178, 0);
  mmioGetInfo(hmmio, &info, 0);
  mmioSeek(hmmio, x174, 0);
  mmioGetInfo(hmmio, &info, 0);
  mmioAdvance(hmmio, &info, 0);
  if ((pinfo != 0) && (pinfo != &info)) {
    FUN_1004249a(pinfo);
    pinfo = &info;
  }
  flags |= 1;
  for (i = 0; i < 16; i++) {
    voices[i].init(pinfo, &fmt);
    voices[i].x90 = x168;
    voices[i].x8c = x164;
    voices[i].x88 = 0;
    voices[i].vf.b1 = 0;
    voices[i].vf.b2 = 1;
  }
  return 0;
}
}

namespace f_10037e10 {
struct C_FUN_10037e10 {
  MMIOINFO info; char pad[0x88 - sizeof(MMIOINFO)]; int f88; int f8c; int f90; char pad2[0xa0 - 0x94]; WAVEFORMATEX fmt;
  void FUN_10037e10(MMIOINFO *param_1, WAVEFORMATEX *param_2);
};
// MATCH: sound.dll 0x10037e10 ?FUN_10037e10@C_FUN_10037e10@f_10037e10@@QAEXPAU_MMIOINFO@@PAUtWAVEFORMATEX@@@Z
void C_FUN_10037e10::FUN_10037e10(MMIOINFO *param_1, WAVEFORMATEX *param_2)
{
  if (param_1 != 0) {
    info = *param_1;
  }
  f88 = 0;
  f8c = 0;
  f90 = 0;
  fmt = *param_2;
}
}
