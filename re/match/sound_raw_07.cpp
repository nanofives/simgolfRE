// FLAGS sound.dll: /O2
// sound.dll functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (raw form: Ghidra's offsets,
// casts and FUN_/DAT_ names; in a /Od build this compiles to the same instructions as the original member
// accesses, so each // MATCH: below is a 100% instruction match). Names, types and layouts are still to be
// written: these are evidence of byte identity, not readable source. String literals are placeholders named by
// address (s_<addr>), never the game's text.

#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10009bb0 {
// MATCH: sound.dll 0x10009bb0 ?FUN_10009bb0@f_10009bb0@@YAIXZ
extern int DAT_100b4a1c;
struct VT_1 {  virtual int f0(int); };
undefined4 __cdecl FUN_10009bb0()
{
  if (DAT_100b4a1c != 0) {
    ((VT_1 *)(DAT_100b4a1c))->f0((int)(1));
  }
  DAT_100b4a1c = 0;
  return 0;
}
}

namespace f_10009da0 {
// MATCH: sound.dll 0x10009da0 ?FUN_10009da0@C_FUN_10009da0@f_10009da0@@QAEIIE@Z
extern int DAT_100b49ec;
struct T_thunk_FUN_100088e0 { int thunk_FUN_100088e0(undefined4); };
struct C_FUN_10009da0 { undefined4 FUN_10009da0(undefined4 param_1, byte param_2); };
undefined4 C_FUN_10009da0::FUN_10009da0(undefined4 param_1, byte param_2)
{
  if ((param_2 & 2) != 0) {
    if ((midiOutOpen((LPHMIDIOUT)((int)this + 0x3c),0xffffffff,0,0,0)) != 0) {
      return 7;
    }
  }
  if (DAT_100b49ec != 0) {
    *(void **)((int)this + 0x2c) = (void *)(DAT_100b49ec);
    ((T_thunk_FUN_100088e0 *)(DAT_100b49ec))->thunk_FUN_100088e0((unsigned int)(this));
  }
  return 0;
}
}

namespace f_1000a170 {
// MATCH: sound.dll 0x1000a170 ?FUN_1000a170@f_1000a170@@YIXH@Z
extern int DAT_100b49ec;
struct T_thunk_FUN_100088e0 { int thunk_FUN_100088e0(undefined4); };
undefined4 __fastcall thunk_FUN_10008ce0(UINT *);
void __fastcall FUN_1000a170(int param_1)
{
  if (DAT_100b49ec != 0) {
    *(UINT **)(param_1 + 0x2c) = (unsigned int *)(DAT_100b49ec);
    ((T_thunk_FUN_100088e0 *)(DAT_100b49ec))->thunk_FUN_100088e0(param_1);
    thunk_FUN_10008ce0((unsigned int *)(DAT_100b49ec));
    return;
  }
  return;
}
}

namespace f_1000afe0 {
// MATCH: sound.dll 0x1000afe0 ?FUN_1000afe0@f_1000afe0@@YIHH@Z
int __fastcall FUN_1000afe0(int param_1)
{
  return (*(int *)(param_1 + 0x24) << 0x1f) >> 0x1f;
}
}

namespace f_1000b1a0 {
// MATCH: sound.dll 0x1000b1a0 ?FUN_1000b1a0@f_1000b1a0@@YAIXZ
extern int DAT_100b4a20;
struct VT_1 {  virtual int f0(int); };
undefined4 __cdecl FUN_1000b1a0()
{
  if (DAT_100b4a20 != 0) {
    ((VT_1 *)(DAT_100b4a20))->f0((int)(1));
  }
  DAT_100b4a20 = 0;
  return 0;
}
}

namespace f_1000bf90 {
// MATCH: sound.dll 0x1000bf90 ?FUN_1000bf90@f_1000bf90@@YIXH@Z
extern int DAT_100b49ec;
extern int DAT_100b49f0;
extern int DAT_100b49f8;
extern int DAT_100b49fc;
extern int DAT_100b4a00;
struct T_thunk_FUN_100088e0 { int thunk_FUN_100088e0(undefined4); };
undefined4 __fastcall thunk_FUN_10008ce0(UINT *);
int __fastcall thunk_FUN_1000f020(int);
struct T_thunk_FUN_10010db0 { undefined4 thunk_FUN_10010db0(undefined4); };
void __fastcall FUN_1000bf90(int param_1)
{
  if (DAT_100b49f8 != 0) {
    thunk_FUN_1000f020(DAT_100b49f8);
  }
  if ((*(byte *)(DAT_100b49f0 + 0x6c) & 0x10) != 0) {
    if (DAT_100b49fc != 0) {
      ((T_thunk_FUN_10010db0 *)(DAT_100b49fc))->thunk_FUN_10010db0(1);
    }
    if (DAT_100b4a00 != 0) {
      ((T_thunk_FUN_10010db0 *)(DAT_100b4a00))->thunk_FUN_10010db0(1);
    }
  }
  if (DAT_100b49ec != 0) {
    *(UINT **)(param_1 + 0x1a8) = (unsigned int *)(DAT_100b49ec);
    ((T_thunk_FUN_100088e0 *)(DAT_100b49ec))->thunk_FUN_100088e0(param_1);
    thunk_FUN_10008ce0((unsigned int *)(DAT_100b49ec));
  }
  return;
}
}

namespace f_1000ea00 {
// MATCH: sound.dll 0x1000ea00 ?FUN_1000ea00@f_1000ea00@@YAIXZ
extern int DAT_100b4a24;
struct VT_1 {  virtual int f0(int); };
undefined4 __cdecl FUN_1000ea00()
{
  if (DAT_100b4a24 != 0) {
    ((VT_1 *)(DAT_100b4a24))->f0((int)(1));
  }
  DAT_100b4a24 = 0;
  return 0;
}
}

namespace f_1000ec00 {
// MATCH: sound.dll 0x1000ec00 ?FUN_1000ec00@f_1000ec00@@YIIPAH@Z
extern int DAT_100b49ec;
extern int DAT_100b4a04;
struct T_thunk_FUN_10008920 { int thunk_FUN_10008920(int *); };
undefined4 __fastcall thunk_FUN_10008d70(int);
struct VT_1 {  virtual int f0(int); };
undefined4 __fastcall FUN_1000ec00(int *param_1)
{
  if (param_1[0xc] != 0) {
    thunk_FUN_10008d70(param_1[0xc]);
    ((T_thunk_FUN_10008920 *)(DAT_100b49ec))->thunk_FUN_10008920(param_1);
  }
  if (DAT_100b4a04 != 0) {
    ((VT_1 *)(DAT_100b4a04))->f0((int)(1));
    DAT_100b4a04 = 0;
  }
  return 0;
}
}

namespace f_10020ea0 {
// MATCH: sound.dll 0x10020ea0 ?FUN_10020ea0@f_10020ea0@@YIIH@Z
extern int DAT_100b4a1c;
struct T_thunk_FUN_1000a240 { undefined4 thunk_FUN_1000a240(undefined4, undefined4, undefined4, undefined4); };
undefined4 __fastcall FUN_10020ea0(int param_1)
{
  if ((*(byte *)(param_1 + 0xac) & 1) == 0) {
    return 0x14;
  }
  if (DAT_100b4a1c != 0) {
    ((T_thunk_FUN_1000a240 *)(DAT_100b4a1c))->thunk_FUN_1000a240(param_1, 2, 0, 0);
  }
  return 0;
}
}

namespace f_10031080 {
// MATCH: sound.dll 0x10031080 ?FUN_10031080@f_10031080@@YIIH@Z
extern int DAT_100b4a20;
struct T_thunk_FUN_1000c2f0 { undefined4 thunk_FUN_1000c2f0(undefined4, undefined4); };
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); };
undefined4 __fastcall FUN_10031080(int param_1)
{
  if ((*(byte *)(param_1 + 0x58) & 1) != 0) {
    return 0xd;
  }
  if (DAT_100b4a20 != 0) {
    if ((((VT_1 *)(DAT_100b4a20))->f27()) == 0) {
      *(uint *)(param_1 + 0x58) = *(uint *)(param_1 + 0x58) & 0xffffffdb | 1;
      ((T_thunk_FUN_1000c2f0 *)(DAT_100b4a20))->thunk_FUN_1000c2f0(param_1, 1);
      return 0;
    }
  }
  return 1;
}
}

namespace f_10039bd0 {
// MATCH: sound.dll 0x10039bd0 ?FUN_10039bd0@f_10039bd0@@YIIPAH@Z
extern int DAT_100b4a20;
struct T_thunk_FUN_1000c2f0 { undefined4 thunk_FUN_1000c2f0(undefined4, undefined4); };
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); };
undefined4 __fastcall FUN_10039bd0(int *param_1)
{
  if ((((VT_1 *)(param_1))->f24()) == 0) {
    return 0;
  }
  if (DAT_100b4a20 != 0) {
    return (((T_thunk_FUN_1000c2f0 *)(DAT_100b4a20))->thunk_FUN_1000c2f0((unsigned int)(param_1), 3));
  }
  return 0x13;
}
}
