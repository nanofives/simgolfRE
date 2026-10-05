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

namespace f_1002a650 {
// MATCH: sound.dll 0x1002a650 ?FUN_1002a650@f_1002a650@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); };
undefined4 __fastcall FUN_1002a650(int param_1)
{
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x14)))->f23();
  }
  return 0;
}
}

namespace f_1002a670 {
// MATCH: sound.dll 0x1002a670 ?FUN_1002a670@f_1002a670@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); };
undefined4 __fastcall FUN_1002a670(int param_1)
{
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x14)))->f24();
  }
  return 0;
}
}

namespace f_1002a920 {
// MATCH: sound.dll 0x1002a920 ?FUN_1002a920@f_1002a920@@YIXPAE@Z
extern int DAT_10001366;
HANDLE __cdecl __beginthread(int, SIZE_T, int);
void __fastcall FUN_1002a920(byte *param_1)
{
    HANDLE hThread;
  ResetEvent(*(HANDLE *)(param_1 + 0x24));
  if ((*param_1 & 1) == 0) {
    *param_1 = *param_1 | 1;
    hThread = __beginthread((int)(((char *)&DAT_10001366)),0,0);
    *(HANDLE *)(param_1 + 0x20) = hThread;
    SetThreadPriority(hThread,0);
  }
  return;
}
}

namespace f_1002abd0 {
// MATCH: sound.dll 0x1002abd0 ?FUN_1002abd0@C_FUN_1002abd0@f_1002abd0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002ac00(undefined4 *);
struct C_FUN_1002abd0 { undefined4 * FUN_1002abd0(byte param_1); };
undefined4 * C_FUN_1002abd0::FUN_1002abd0(byte param_1)
{
  thunk_FUN_1002ac00((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002ace0 {
// MATCH: sound.dll 0x1002ace0 ?FUN_1002ace0@C_FUN_1002ace0@f_1002ace0@@QAEIH@Z
struct C_FUN_1002ace0 { undefined4 FUN_1002ace0(int param_1); };
undefined4 C_FUN_1002ace0::FUN_1002ace0(int param_1)
{
  if (param_1 == 0) {
    return 10;
  }
  *(int *)((int)this + 0x38) = param_1;
  return 0;
}
}

namespace f_1002b2c0 {
// MATCH: sound.dll 0x1002b2c0 ?FUN_1002b2c0@C_FUN_1002b2c0@f_1002b2c0@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(int); };
struct C_FUN_1002b2c0 { void FUN_1002b2c0(undefined4 param_1); };
void C_FUN_1002b2c0::FUN_1002b2c0(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x34) = param_1;
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f19((int)(param_1));
  }
  return;
}
}

namespace f_1002b330 {
// MATCH: sound.dll 0x1002b330 ?FUN_1002b330@C_FUN_1002b330@f_1002b330@@QAEIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(int); };
struct C_FUN_1002b330 { undefined4 FUN_1002b330(int param_1); };
undefined4 C_FUN_1002b330::FUN_1002b330(int param_1)
{
  if (param_1 == 0) {
    return 10;
  }
  *(int *)((int)this + 0x38) = param_1;
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f21((int)(param_1));
  }
  return 0;
}
}

namespace f_1002b3a0 {
// MATCH: sound.dll 0x1002b3a0 ?FUN_1002b3a0@C_FUN_1002b3a0@f_1002b3a0@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct C_FUN_1002b3a0 { void FUN_1002b3a0(undefined4 param_1); };
void C_FUN_1002b3a0::FUN_1002b3a0(undefined4 param_1)
{
  if ((((VT_1 *)(this))->f1((int)(param_1))) == 0) {
    ((VT_2 *)(this))->f10();
  }
  return;
}
}

namespace f_1002b430 {
// MATCH: sound.dll 0x1002b430 ?FUN_1002b430@C_FUN_1002b430@f_1002b430@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct C_FUN_1002b430 { void FUN_1002b430(undefined4 param_1); };
void C_FUN_1002b430::FUN_1002b430(undefined4 param_1)
{
  if ((((VT_1 *)(this))->f21((int)(param_1))) == 0) {
    ((VT_2 *)(this))->f10();
  }
  return;
}
}

namespace f_1002b4a0 {
// MATCH: sound.dll 0x1002b4a0 ?FUN_1002b4a0@C_FUN_1002b4a0@f_1002b4a0@@QAEXH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(int); };
struct C_FUN_1002b4a0 { void FUN_1002b4a0(int param_1); };
void C_FUN_1002b4a0::FUN_1002b4a0(int param_1)
{
  if (param_1 < -0x40) {
    param_1 = -0x40;
  }
  else if (0x3f < param_1) {
    param_1 = 0x3f;
  }
  *(int *)((int)this + 8) = param_1;
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f17((int)(param_1));
  }
  return;
}
}

namespace f_1002b5f0 {
// MATCH: sound.dll 0x1002b5f0 ?FUN_1002b5f0@C_FUN_1002b5f0@f_1002b5f0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002b620(undefined4 *);
struct C_FUN_1002b5f0 { undefined4 * FUN_1002b5f0(byte param_1); };
undefined4 * C_FUN_1002b5f0::FUN_1002b5f0(byte param_1)
{
  thunk_FUN_1002b620((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002b870 {
// MATCH: sound.dll 0x1002b870 ?FUN_1002b870@C_FUN_1002b870@f_1002b870@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(int); };
struct C_FUN_1002b870 { void FUN_1002b870(undefined4 param_1); };
void C_FUN_1002b870::FUN_1002b870(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f52((int)(param_1));
  }
  return;
}
}

namespace f_1002bff0 {
// MATCH: sound.dll 0x1002bff0 ?FUN_1002bff0@C_FUN_1002bff0@f_1002bff0@@QAEIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(int); };
struct C_FUN_1002bff0 { undefined4 FUN_1002bff0(int param_1); };
undefined4 C_FUN_1002bff0::FUN_1002bff0(int param_1)
{
  if (param_1 < -0x4b0) {
    param_1 = -0x4b0;
  }
  else if (0x4b0 < param_1) {
    param_1 = 0x4b0;
  }
  *(int *)((int)this + 0x5c) = param_1;
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f39((int)(param_1));
  }
  return 0;
}
}

namespace f_1002c1a0 {
// MATCH: sound.dll 0x1002c1a0 ?FUN_1002c1a0@f_1002c1a0@@YIIH@Z
extern int DAT_1002c1a9;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); };
undefined4 __fastcall FUN_1002c1a0(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x40) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_1002c1a9). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f15();
    return uVar1;
  }
  return 0;
}
}

namespace f_1002c1f0 {
// MATCH: sound.dll 0x1002c1f0 ?FUN_1002c1f0@C_FUN_1002c1f0@f_1002c1f0@@QAEIPAEIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(int, int, int); };
struct C_FUN_1002c1f0 { undefined4 FUN_1002c1f0(undefined1 *param_1, undefined4 param_2, int param_3); };
undefined4 C_FUN_1002c1f0::FUN_1002c1f0(undefined1 *param_1, undefined4 param_2, int param_3)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f49((int)(param_1), (int)(param_2), (int)(param_3)));
  }
  if (param_3 != 0) {
    *param_1 = 0;
  }
  return 1;
}
}

namespace f_1002c270 {
// MATCH: sound.dll 0x1002c270 ?FUN_1002c270@C_FUN_1002c270@f_1002c270@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(int); };
struct C_FUN_1002c270 { undefined4 FUN_1002c270(undefined4 param_1); };
undefined4 C_FUN_1002c270::FUN_1002c270(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x60) = param_1;
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f59((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_1002c370 {
// MATCH: sound.dll 0x1002c370 ?FUN_1002c370@C_FUN_1002c370@f_1002c370@@QAEXI@Z
struct C_FUN_1002c370 { void FUN_1002c370(uint param_1); };
void C_FUN_1002c370::FUN_1002c370(uint param_1)
{
  *(uint *)((int)this + 4) = param_1 & 0x7f;
  return;
}
}

namespace f_1002c3d0 {
// MATCH: sound.dll 0x1002c3d0 ?FUN_1002c3d0@C_FUN_1002c3d0@f_1002c3d0@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(int); };
struct C_FUN_1002c3d0 { undefined4 FUN_1002c3d0(undefined4 param_1); };
undefined4 C_FUN_1002c3d0::FUN_1002c3d0(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f55((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_1002c400 {
// MATCH: sound.dll 0x1002c400 ?FUN_1002c400@C_FUN_1002c400@f_1002c400@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(int); };
struct C_FUN_1002c400 { undefined4 FUN_1002c400(undefined4 param_1); };
undefined4 C_FUN_1002c400::FUN_1002c400(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f56((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_1002c430 {
// MATCH: sound.dll 0x1002c430 ?FUN_1002c430@C_FUN_1002c430@f_1002c430@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(int); };
struct C_FUN_1002c430 { undefined4 FUN_1002c430(undefined4 param_1); };
undefined4 C_FUN_1002c430::FUN_1002c430(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f57((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_1002c5b0 {
// MATCH: sound.dll 0x1002c5b0 ?FUN_1002c5b0@C_FUN_1002c5b0@f_1002c5b0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002c5e0(undefined4 *);
struct C_FUN_1002c5b0 { undefined4 * FUN_1002c5b0(byte param_1); };
undefined4 * C_FUN_1002c5b0::FUN_1002c5b0(byte param_1)
{
  thunk_FUN_1002c5e0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002cad0 {
// MATCH: sound.dll 0x1002cad0 ?FUN_1002cad0@C_FUN_1002cad0@f_1002cad0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002cb00(undefined4 *);
struct C_FUN_1002cad0 { undefined4 * FUN_1002cad0(byte param_1); };
undefined4 * C_FUN_1002cad0::FUN_1002cad0(byte param_1)
{
  thunk_FUN_1002cb00((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002ce60 {
// MATCH: sound.dll 0x1002ce60 ?FUN_1002ce60@f_1002ce60@@YIIPAH@Z
int __cdecl FUN_1004249a(undefined *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); };
undefined4 __fastcall FUN_1002ce60(int *param_1)
{
    int * piVar1;
    int iVar2;
  ((VT_1 *)(param_1))->f8();
  piVar1 = param_1 + 0x16;
  iVar2 = 0x28;
  do {
    if ((undefined *)*piVar1 != (undefined *)0x0) {
      FUN_1004249a((undefined *)*piVar1);
      *piVar1 = 0;
    }
    piVar1 = piVar1 + 1;
    iVar2 = iVar2 + -1;
  } while (iVar2 != 0);
  return 0;
}
}

namespace f_1002ceb0 {
// MATCH: sound.dll 0x1002ceb0 ?FUN_1002ceb0@f_1002ceb0@@YIPAIPAI@Z
undefined4 * __fastcall FUN_1002ceb0(undefined4 *param_1)
{
    int iVar1;
    undefined4 * puVar2;
  puVar2 = param_1 + 3;
  for (iVar1 = 0x41; iVar1 != 0; iVar1 = iVar1 + -1) {
    *puVar2 = 0;
    puVar2 = puVar2 + 1;
  }
  param_1[1] = 0;
  *param_1 = 0;
  param_1[2] = 1;
  return param_1;
}
}

namespace f_1002d430 {
// MATCH: sound.dll 0x1002d430 ?FUN_1002d430@f_1002d430@@YIIH@Z
undefined4 __fastcall FUN_1002d430(int param_1)
{
  return *(undefined4 *)(param_1 + 0x204);
}
}

namespace f_1002d530 {
// MATCH: sound.dll 0x1002d530 ?FUN_1002d530@C_FUN_1002d530@f_1002d530@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002d560(undefined4 *);
struct C_FUN_1002d530 { undefined4 * FUN_1002d530(byte param_1); };
undefined4 * C_FUN_1002d530::FUN_1002d530(byte param_1)
{
  thunk_FUN_1002d560((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002d7a0 {
// MATCH: sound.dll 0x1002d7a0 ?FUN_1002d7a0@C_FUN_1002d7a0@f_1002d7a0@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(int); };
struct C_FUN_1002d7a0 { undefined4 FUN_1002d7a0(undefined4 param_1); };
undefined4 C_FUN_1002d7a0::FUN_1002d7a0(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f56((int)(param_1)));
  }
  return 0x13;
}
}

namespace f_1002d8c0 {
// MATCH: sound.dll 0x1002d8c0 ?FUN_1002d8c0@C_FUN_1002d8c0@f_1002d8c0@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(int); };
struct C_FUN_1002d8c0 { void FUN_1002d8c0(undefined4 param_1); };
void C_FUN_1002d8c0::FUN_1002d8c0(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f67((int)(param_1));
  }
  return;
}
}

namespace f_1002d8f0 {
// MATCH: sound.dll 0x1002d8f0 ?FUN_1002d8f0@C_FUN_1002d8f0@f_1002d8f0@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(int); };
struct C_FUN_1002d8f0 { undefined4 FUN_1002d8f0(undefined4 param_1); };
undefined4 C_FUN_1002d8f0::FUN_1002d8f0(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f53((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_1002d920 {
// MATCH: sound.dll 0x1002d920 ?FUN_1002d920@f_1002d920@@YIIH@Z
extern int DAT_1002d929;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); };
undefined4 __fastcall FUN_1002d920(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x40) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_1002d929). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f46();
    return uVar1;
  }
  return 0x14;
}
}

namespace f_1002da30 {
// MATCH: sound.dll 0x1002da30 ?FUN_1002da30@C_FUN_1002da30@f_1002da30@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(int); };
struct C_FUN_1002da30 { undefined4 FUN_1002da30(undefined4 param_1); };
undefined4 C_FUN_1002da30::FUN_1002da30(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f36((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_1002dba0 {
// MATCH: sound.dll 0x1002dba0 ?FUN_1002dba0@C_FUN_1002dba0@f_1002dba0@@QAEIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(int, int); };
struct C_FUN_1002dba0 { undefined4 FUN_1002dba0(undefined4 param_1, undefined4 param_2); };
undefined4 C_FUN_1002dba0::FUN_1002dba0(undefined4 param_1, undefined4 param_2)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f41((int)(param_1), (int)(param_2)));
  }
  return 0x14;
}
}

namespace f_1002dbe0 {
// MATCH: sound.dll 0x1002dbe0 ?FUN_1002dbe0@C_FUN_1002dbe0@f_1002dbe0@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(int); };
struct C_FUN_1002dbe0 { undefined4 FUN_1002dbe0(undefined4 param_1); };
undefined4 C_FUN_1002dbe0::FUN_1002dbe0(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f42((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_1002dc40 {
// MATCH: sound.dll 0x1002dc40 ?FUN_1002dc40@f_1002dc40@@YIIH@Z
extern int DAT_1002dc49;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); };
undefined4 __fastcall FUN_1002dc40(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x40) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_1002dc49). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f45();
    return uVar1;
  }
  return 0;
}
}

namespace f_1002ddb0 {
// MATCH: sound.dll 0x1002ddb0 ?FUN_1002ddb0@C_FUN_1002ddb0@f_1002ddb0@@QAEII@Z
struct T_thunk_FUN_1002ddb0 { undefined4 thunk_FUN_1002ddb0(undefined4); };
struct C_FUN_1002ddb0 { undefined4 FUN_1002ddb0(undefined4 param_1); };
undefined4 C_FUN_1002ddb0::FUN_1002ddb0(undefined4 param_1)
{
  *(byte *)((int)this + 0x58) = (*(byte *)((int)this + 0x58) ^ (byte)param_1) & 1 ^ *(byte *)((int)this + 0x58);
  if (*(void **)((int)this + 0x40) != (void *)0x0) {
    ((T_thunk_FUN_1002ddb0 *)(*(void **)((int)this + 0x40)))->thunk_FUN_1002ddb0(param_1);
  }
  return 0;
}
}

namespace f_1002e130 {
// MATCH: sound.dll 0x1002e130 ?FUN_1002e130@C_FUN_1002e130@f_1002e130@@QAEIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(int, int); };
struct C_FUN_1002e130 { undefined4 FUN_1002e130(undefined4 param_1, undefined4 param_2); };
undefined4 C_FUN_1002e130::FUN_1002e130(undefined4 param_1, undefined4 param_2)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x40)))->f70((int)(param_1), (int)(param_2)));
  }
  return 0;
}
}

namespace f_1002e290 {
// MATCH: sound.dll 0x1002e290 ?FUN_1002e290@C_FUN_1002e290@f_1002e290@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(int); };
struct C_FUN_1002e290 { void FUN_1002e290(undefined4 param_1); };
void C_FUN_1002e290::FUN_1002e290(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f63((int)(param_1));
  }
  return;
}
}

namespace f_1002e430 {
// MATCH: sound.dll 0x1002e430 ?FUN_1002e430@C_FUN_1002e430@f_1002e430@@QAEXI@Z
struct C_FUN_1002e430 { void FUN_1002e430(uint param_1); };
void C_FUN_1002e430::FUN_1002e430(uint param_1)
{
  if (*(uint *)((int)this + 0xc) < param_1) {
    *(uint *)((int)this + 0xc) = param_1;
  }
  *(uint *)((int)this + 8) = param_1;
  return;
}
}

namespace f_1002e530 {
// MATCH: sound.dll 0x1002e530 ?FUN_1002e530@f_1002e530@@YIXPAI@Z
extern void *PTR_LAB_1005bb80;
void __fastcall FUN_1002e530(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005bb80);
  return;
}
}

namespace f_1002e630 {
// MATCH: sound.dll 0x1002e630 ?FUN_1002e630@C_FUN_1002e630@f_1002e630@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002e660(undefined4 *);
struct C_FUN_1002e630 { undefined4 * FUN_1002e630(byte param_1); };
undefined4 * C_FUN_1002e630::FUN_1002e630(byte param_1)
{
  thunk_FUN_1002e660((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}
