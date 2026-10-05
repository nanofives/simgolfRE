// FLAGS golf_clean.exe: /O2
// golf_clean.exe functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (raw form: Ghidra's offsets,
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

namespace f_004011b0 {
// MATCH: golf_clean.exe 0x004011b0 ?FUN_004011b0@f_004011b0@@YAXHH@Z
extern int DAT_0056d1e0;
extern int DAT_0056e950;
void __cdecl FUN_004011b0(int param_1, int param_2)
{
    short * psVar1;
  psVar1 = (short *)(((char *)&DAT_0056d1e0));
  do {
    if ((psVar1[-1] == param_1) && (*psVar1 == param_2)) {
      psVar1[-4] = -1;
    }
    psVar1 = psVar1 + 0x1e;
  } while ((int)((int)psVar1) < (int)(((char *)&DAT_0056e950)));
  return;
}
}

namespace f_004490b0 {
// MATCH: golf_clean.exe 0x004490b0 ?FUN_004490b0@f_004490b0@@YAXXZ
extern int DAT_0080d840;
extern int DAT_008156d0;
undefined4 __stdcall FUN_00483f10();
undefined4 __fastcall FUN_00484f00(int *);
void __cdecl FUN_004490b0()
{
    int * piVar1;
  piVar1 = (int *)((char *)&DAT_0080d840);
  do {
    FUN_00484f00(piVar1);
    piVar1 = piVar1 + 0x1b;
  } while ((int)((int)piVar1) < (int)(((char *)&DAT_008156d0)));
  FUN_00483f10();
  return;
}
}

namespace f_00486f90 {
// MATCH: golf_clean.exe 0x00486f90 ?FUN_00486f90@f_00486f90@@YGXIIIHH@Z
extern int DAT_0083ad50;
extern int DAT_0083aff0;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); };
void __stdcall FUN_00486f90(undefined4 param_1, undefined4 param_2, WPARAM param_3, int pad_4, int pad_5)
{
  if (param_3 != 0) {
    if (*(code **)(param_3 + 4) != 0) {
      (**(code **)(param_3 + 4))();
      return;
    }
    if (((DAT_0083aff0 == 0) || (param_3 == DAT_0083aff0)) && (*(int *)(param_3 + 0x24) == 0)) {
      PostMessageA(((HWND)((VT_1 *)(DAT_0083ad50))->f8()),0x401,param_3,0);
      *(undefined4 *)(param_3 + 0x24) = 1;
    }
  }
  return;
}
}

namespace f_0048cac0 {
// MATCH: golf_clean.exe 0x0048cac0 ?FUN_0048cac0@C_FUN_0048cac0@f_0048cac0@@QAEXHH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct C_FUN_0048cac0 { void FUN_0048cac0(int pad_1, int pad_2); };
void C_FUN_0048cac0::FUN_0048cac0(int pad_1, int pad_2)
{
  *(undefined4 *)(*(int *)(*(int *)(((int)this) + -0x58) + 8) + 0x9c + ((int)this)) = 0xffffffff;
  ((VT_1 *)((*(int *)(*(int *)(((int)this) + -0x58) + 4) + -0x58 + ((int)this))))->f72();
  return;
}
}

namespace f_00496d20 {
// MATCH: golf_clean.exe 0x00496d20 ?FUN_00496d20@C_FUN_00496d20@f_00496d20@@QAEXHH@Z
extern int DAT_0083ab30;
extern int DAT_0083ab34;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); };
struct C_FUN_00496d20 { void FUN_00496d20(int pad_1, int pad_2); };
void C_FUN_00496d20::FUN_00496d20(int pad_1, int pad_2)
{
  ((VT_1 *)(((int *)this)))->f72();
  if ((int)(DAT_0083ab30) == (int)(((int *)this))) {
    DAT_0083ab30 = 0;
    ((VT_2 *)(((int *)this)))->f4();
  }
  if ((int)(DAT_0083ab34) == (int)(((int *)this))) {
    DAT_0083ab34 = 0;
  }
  return;
}
}

namespace f_0049c8b0 {
// MATCH: golf_clean.exe 0x0049c8b0 ?FUN_0049c8b0@f_0049c8b0@@YGXH@Z
void __fastcall FUN_00483010(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
void __stdcall FUN_0049c8b0(int param_1)
{
    undefined4 * puVar1;
  puVar1 = *(undefined4 **)(param_1 + 4);
  if (puVar1 != 0) {
    FUN_00483010(puVar1);
    FUN_004a4ffc((unsigned int)(puVar1));
    *(undefined4 *)(param_1 + 4) = 0;
  }
  return;
}
}

namespace f_0049e640 {
// MATCH: golf_clean.exe 0x0049e640 ?FUN_0049e640@C_FUN_0049e640@f_0049e640@@QAEXHH@Z
extern int _DAT_0083ab2c;
undefined4 __fastcall FUN_00489950(int);
struct C_FUN_0049e640 { /* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ void FUN_0049e640(int pad_1, int pad_2); };
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ void C_FUN_0049e640::FUN_0049e640(int pad_1, int pad_2)
{
  _DAT_0083ab2c = *(undefined4 *)(*(int *)(*(int *)(((int)this) + -0x24) + 4) + 0x10c + ((int)this));
  if (*(int *)(((int)this) + -0x10) != 0) {
    (**(code **)(((int)this) + -0x10))((FUN_00489950(*(int *)(*(int *)(((int)this) + -0x24) + 8) + -0x24 + ((int)this))));
  }
  return;
}
}

namespace f_0049e7b0 {
// MATCH: golf_clean.exe 0x0049e7b0 ?FUN_0049e7b0@C_FUN_0049e7b0@f_0049e7b0@@QAEXH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct C_FUN_0049e7b0 { void FUN_0049e7b0(int pad_1); };
void C_FUN_0049e7b0::FUN_0049e7b0(int pad_1)
{
  ((VT_1 *)((*(int *)(*(int *)(((int)this) + -0x24) + 4) + -0x24 + ((int)this))))->f72();
  return;
}
}

namespace f_0049e990 {
// MATCH: golf_clean.exe 0x0049e990 ?FUN_0049e990@C_FUN_0049e990@f_0049e990@@QAEXHH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct C_FUN_0049e990 { void FUN_0049e990(int pad_1, int pad_2); };
void C_FUN_0049e990::FUN_0049e990(int pad_1, int pad_2)
{
  *(undefined4 *)(*(int *)(*(int *)(((int)this) + -0x24) + 8) + 0xd0 + ((int)this)) = 0xffffffff;
  *(undefined4 *)(*(int *)(*(int *)(((int)this) + -0x24) + 8) + 0xd4 + ((int)this)) = 0xffffffff;
  ((VT_1 *)((*(int *)(*(int *)(((int)this) + -0x24) + 4) + -0x24 + ((int)this))))->f72();
  return;
}
}

namespace f_0049fd80 {
// MATCH: golf_clean.exe 0x0049fd80 ?FUN_0049fd80@C_FUN_0049fd80@f_0049fd80@@QAEXH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct C_FUN_0049fd80 { void FUN_0049fd80(int pad_1); };
void C_FUN_0049fd80::FUN_0049fd80(int pad_1)
{
  ((VT_1 *)((*(int *)(*(int *)(((int)this) + -0x1c) + 4) + -0x1c + ((int)this))))->f72();
  return;
}
}

namespace f_0049fe20 {
// MATCH: golf_clean.exe 0x0049fe20 ?FUN_0049fe20@C_FUN_0049fe20@f_0049fe20@@QAEXHH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct C_FUN_0049fe20 { void FUN_0049fe20(int pad_1, int pad_2); };
void C_FUN_0049fe20::FUN_0049fe20(int pad_1, int pad_2)
{
  *(undefined4 *)(*(int *)(*(int *)(((int)this) + -0x1c) + 8) + 0xd8 + ((int)this)) = 0xffffffff;
  ((VT_1 *)((*(int *)(*(int *)(((int)this) + -0x1c) + 4) + -0x1c + ((int)this))))->f72();
  return;
}
}

namespace f_004a4870 {
// MATCH: golf_clean.exe 0x004a4870 ?FUN_004a4870@C_FUN_004a4870@f_004a4870@@QAEXH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct C_FUN_004a4870 { void FUN_004a4870(int pad_1); };
void C_FUN_004a4870::FUN_004a4870(int pad_1)
{
  ((VT_1 *)((*(int *)(*(int *)(((int)this) + -0xe0) + 4) + -0xe0 + ((int)this))))->f72();
  return;
}
}
