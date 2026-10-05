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

namespace f_0047ba70 {
// MATCH: golf_clean.exe 0x0047ba70 ?FUN_0047ba70@C_FUN_0047ba70@f_0047ba70@@QAEXI@Z
struct C_FUN_0047ba70 { void FUN_0047ba70(undefined4 param_1); };
void C_FUN_0047ba70::FUN_0047ba70(undefined4 param_1)
{
  if (*(int *)((int)this + 0x26c) != 0) {
    *(undefined4 *)(*(int *)((int)this + 0x26c) + 0x5a0) = param_1;
  }
  return;
}
}

namespace f_0047ba90 {
// MATCH: golf_clean.exe 0x0047ba90 ?FUN_0047ba90@C_FUN_0047ba90@f_0047ba90@@QAEXI@Z
struct C_FUN_0047ba90 { void FUN_0047ba90(undefined4 param_1); };
void C_FUN_0047ba90::FUN_0047ba90(undefined4 param_1)
{
  if (*(int *)((int)this + 0x270) != 0) {
    *(undefined4 *)(*(int *)((int)this + 0x270) + 0x5a0) = param_1;
  }
  return;
}
}

namespace f_0047d7c0 {
// MATCH: golf_clean.exe 0x0047d7c0 ?FUN_0047d7c0@f_0047d7c0@@YIXPAI@Z
extern void *PTR_FUN_004baa14;
void __fastcall FUN_00495eb0(int);
void __fastcall FUN_0047d7c0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004baa14);
  FUN_00495eb0((int)param_1);
  return;
}
}

namespace f_0047d7d0 {
// MATCH: golf_clean.exe 0x0047d7d0 ?FUN_0047d7d0@f_0047d7d0@@YIIH@Z
undefined4 __fastcall FUN_0047d7d0(int param_1)
{
    int iVar1;
  if ((*(uint *)(param_1 + 0x9c) & 0x1000) != 0) {
    return 1;
  }
  iVar1 = *(int *)(param_1 + 0x130);
  if (iVar1 != 0) {
    if (!(*(int *)(iVar1 + 0x138) == 0)) {
      iVar1 = *(int *)(*(int *)(iVar1 + 0x13c) + 4);
    } else {
      iVar1 = 0;
    }
    if (iVar1 == param_1) {
      return 1;
    }
  }
  return 0;
}
}

namespace f_0047d840 {
// MATCH: golf_clean.exe 0x0047d840 ?FUN_0047d840@f_0047d840@@YAXI@Z
extern int DAT_0083ab60;
void __cdecl FUN_0047d840(undefined4 param_1)
{
  DAT_0083ab60 = param_1;
  return;
}
}

namespace f_0047f1b0 {
// MATCH: golf_clean.exe 0x0047f1b0 ?FUN_0047f1b0@C_FUN_0047f1b0@f_0047f1b0@@QAEXHIIH@Z
extern int DAT_0083ab40;
extern int DAT_0083ab44;
extern int DAT_0083ab48;
struct T_FUN_0047c010 { void FUN_0047c010(undefined4, undefined4, int); };
struct T_FUN_0047c430 { void FUN_0047c430(undefined4, undefined4, int); };
void __fastcall FUN_0047e120(int);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); virtual int f73(); virtual int f74(); virtual int f75(int); };
struct C_FUN_0047f1b0 { void FUN_0047f1b0(int param_1, undefined4 param_2, undefined4 param_3, int param_4); };
void C_FUN_0047f1b0::FUN_0047f1b0(int param_1, undefined4 param_2, undefined4 param_3, int param_4)
{
  FUN_0047e120((int)this);
  if (*(int **)((int)this + 0x130) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x130)))->f75((int)(this));
  }
  if (!(param_4 == 0)) {
    DAT_0083ab44 = (int)(this);
    DAT_0083ab40 = (int)((void *)0x0);
  } else {
    DAT_0083ab40 = (int)(this);
    DAT_0083ab44 = (int)((void *)0x0);
  }
  DAT_0083ab48 = 0;
  if ((param_1 != 0) && ((*(uint *)((int)this + 0x9c) & 0x200) == 0)) {
    ((T_FUN_0047c010 *)(this))->FUN_0047c010(param_2, param_3, param_4);
  }
  ((T_FUN_0047c430 *)(this))->FUN_0047c430(param_2, param_3, param_4);
  return;
}
}

namespace f_00480610 {
// MATCH: golf_clean.exe 0x00480610 ?FUN_00480610@f_00480610@@YIXPAH@Z
extern int DAT_0083ac18;
void __fastcall FUN_00474cb0(int);
void __fastcall FUN_00479f30(int *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(int); };
void __fastcall FUN_00480610(int *param_1)
{
  FUN_00479f30(param_1);
  FUN_00474cb0((int)(param_1 + 0x9d));
  param_1[0x15c] = 0;
  param_1[0x69] = 0;
  param_1[0x6a] = 0;
  param_1[0x14b] = 0;
  param_1[0x14c] = 0;
  param_1[0x14d] = 0;
  param_1[0x14e] = 0;
  param_1[0x14f] = 0;
  param_1[0x150] = 0;
  param_1[0x151] = 0;
  param_1[0x152] = 0;
  param_1[0x153] = 0;
  param_1[0x154] = 0;
  param_1[0x155] = 0;
  param_1[0x156] = 0;
  param_1[0x157] = 0;
  param_1[0x158] = 0;
  param_1[0x159] = 0;
  param_1[0x15b] = DAT_0083ac18;
  if ((int *)param_1[0x15a] != (int *)0x0) {
    ((VT_1 *)(param_1[0x15a]))->f3((int)(1));
    param_1[0x15a] = 0;
  }
  return;
}
}

namespace f_00480870 {
// MATCH: golf_clean.exe 0x00480870 ?FUN_00480870@C_FUN_00480870@f_00480870@@QAEHPAHPADPAIH00@Z
struct T_FUN_004806c0 { int FUN_004806c0(int, int, int, int, char *, undefined4 *, int, int *, int *); };
struct C_FUN_00480870 { int FUN_00480870(int *param_1, char *param_2, undefined4 *param_3, int param_4, int *param_5, int *param_6); };
int C_FUN_00480870::FUN_00480870(int *param_1, char *param_2, undefined4 *param_3, int param_4, int *param_5, int *param_6)
{
  if (param_1 == (int *)0x0) {
    return 0x10;
  }
  return (((T_FUN_004806c0 *)(this))->FUN_004806c0(*param_1, param_1[1], param_1[2] - *param_1, param_1[3] - param_1[1], param_2, param_3, param_4, param_5, param_6));
}
}

namespace f_00480d70 {
// MATCH: golf_clean.exe 0x00480d70 ?FUN_00480d70@f_00480d70@@YIXPAH@Z
extern int _DAT_0083ab2c;
void __fastcall FUN_00480ce0(void *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); };
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ void __fastcall FUN_00480d70(int *param_1)
{
  if ((param_1[0x88] & 1U) == 0) {
    param_1[0x88] = param_1[0x88] | 1;
    _DAT_0083ab2c = (int)(param_1);
    if ((code *)param_1[0x15c] != (code *)0x0) {
      (*(code *)param_1[0x15c])();
    }
    ((VT_1 *)(param_1))->f22();
    param_1[0x88] = param_1[0x88] & 0xfffffffe;
    FUN_00480ce0(param_1);
  }
  return;
}
}

namespace f_00481b50 {
// MATCH: golf_clean.exe 0x00481b50 ?FUN_00481b50@f_00481b50@@YIPAIPAI@Z
extern void *PTR_LAB_004ba2e0;
undefined4 * __fastcall FUN_00482b60(undefined4 *);
undefined4 * __fastcall FUN_00481b50(undefined4 *param_1)
{
  FUN_00482b60(param_1);
  *param_1 = (unsigned int)(&PTR_LAB_004ba2e0);
  param_1[0x14] = 0;
  param_1[0x15] = 0;
  param_1[0x1b] = 0;
  param_1[0x1c] = 0;
  param_1[0x17] = 0;
  param_1[0x18] = 0;
  param_1[0x19] = 0;
  param_1[0x1a] = 0;
  param_1[0x1d] = 0;
  param_1[0x1e] = 0;
  param_1[0x11] = 0;
  param_1[0x16] = 0;
  param_1[0x1f] = 0;
  param_1[0x12] = 0;
  param_1[0x13] = 0;
  return param_1;
}
}

namespace f_00482e10 {
// MATCH: golf_clean.exe 0x00482e10 ?FUN_00482e10@f_00482e10@@YIHH@Z
int __fastcall FUN_00482e10(int param_1)
{
  if (*(int *)(param_1 + 4) == 0) {
    return 0;
  }
  return param_1 + 8;
}
}

namespace f_00482e80 {
// MATCH: golf_clean.exe 0x00482e80 ?FUN_00482e80@f_00482e80@@YAXXZ
extern int DAT_0083acb0;
void __fastcall FUN_00482fd0(undefined4 *);
void __cdecl FUN_00482e80()
{
  FUN_00482fd0((undefined4 *)((char *)&DAT_0083acb0));
  return;
}
}

namespace f_00482ec0 {
// MATCH: golf_clean.exe 0x00482ec0 ?FUN_00482ec0@f_00482ec0@@YAXXZ
extern int DAT_0083ac30;
void __fastcall FUN_00482fd0(undefined4 *);
void __cdecl FUN_00482ec0()
{
  FUN_00482fd0((undefined4 *)((char *)&DAT_0083ac30));
  return;
}
}

namespace f_00482f00 {
// MATCH: golf_clean.exe 0x00482f00 ?FUN_00482f00@f_00482f00@@YAXXZ
extern int DAT_0083ac88;
struct T_FUN_00482f60 { void FUN_00482f60(int); };
void __cdecl FUN_00482f00()
{
  ((T_FUN_00482f60 *)(((char *)&DAT_0083ac88)))->FUN_00482f60(1);
  return;
}
}

namespace f_00483010 {
// MATCH: golf_clean.exe 0x00483010 ?FUN_00483010@f_00483010@@YIXPAI@Z
extern void *PTR_FUN_004babbc;
void __fastcall FUN_00483070(int);
void __fastcall FUN_00483010(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004babbc);
  FUN_00483070((int)param_1);
  return;
}
}

namespace f_00483030 {
// MATCH: golf_clean.exe 0x00483030 ?FUN_00483030@f_00483030@@YIIH@Z
extern int DAT_0083ad50;
void __fastcall FUN_00483070(int);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(int); };
struct VT_2 { virtual int f0(); virtual int f1(); };
undefined4 __fastcall FUN_00483030(int param_1)
{
    int * piVar1;
  FUN_00483070(param_1);
  piVar1 = (int *)((VT_1 *)(DAT_0083ad50))->f30((int)(param_1));
  *(int **)(param_1 + 4) = piVar1;
  ((VT_2 *)(piVar1))->f1();
  return 0;
}
}

namespace f_00483060 {
// MATCH: golf_clean.exe 0x00483060 ?FUN_00483060@f_00483060@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); };
undefined4 __fastcall FUN_00483060(int param_1)
{
  if (*(int **)(param_1 + 4) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 4)))->f3();
  }
  return 0;
}
}

namespace f_00483320 {
// MATCH: golf_clean.exe 0x00483320 ?FUN_00483320@f_00483320@@YAXXZ
extern int DAT_0083ac30;
extern int DAT_0083acb0;
extern int DAT_0083ad50;
void __stdcall FUN_00483340();
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(int, int); };
void __cdecl FUN_00483320()
{
  FUN_00483340();
  ((VT_1 *)(DAT_0083ad50))->f48((int)(((char *)&DAT_0083acb0)), (int)(((char *)&DAT_0083ac30)));
  return;
}
}

namespace f_004837b0 {
// MATCH: golf_clean.exe 0x004837b0 ?FUN_004837b0@f_004837b0@@YAXXZ
extern int DAT_0083ad20;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_004837b0()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_0083ad20));
  return;
}
}

namespace f_004838b0 {
// MATCH: golf_clean.exe 0x004838b0 ?FUN_004838b0@f_004838b0@@YIXH@Z
extern int DAT_0083ad50;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(int); };
void __fastcall FUN_004838b0(int param_1)
{
  if (*(int *)(param_1 + 4) != 0) {
    ((VT_1 *)(DAT_0083ad50))->f34((int)(*(int *)(param_1 + 4)));
  }
  *(undefined4 *)(param_1 + 0x10) = 0;
  *(undefined4 *)(param_1 + 0xc) = 0;
  *(undefined4 *)(param_1 + 0x18) = 0;
  *(undefined4 *)(param_1 + 0x1c) = 0;
  *(undefined4 *)(param_1 + 8) = 0xffffffff;
  return;
}
}

namespace f_00483b90 {
// MATCH: golf_clean.exe 0x00483b90 ?FUN_00483b90@f_00483b90@@YIXPAI@Z
extern void *PTR_LAB_004ba468;
void __fastcall FUN_004838b0(int);
void __fastcall FUN_00483b90(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_004ba468);
  FUN_004838b0((int)param_1);
  return;
}
}

namespace f_00483ba0 {
// MATCH: golf_clean.exe 0x00483ba0 ?FUN_00483ba0@f_00483ba0@@YAXXZ
extern int DAT_0083ad50;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); virtual int f73(); };
void __cdecl FUN_00483ba0()
{
                    /* WARNING: Could not recover jumptable at 0x00483ba8. Too many branches */ /* WARNING: Treating indirect jump as call */ ((VT_1 *)(DAT_0083ad50))->f73();
  return;
}
}

namespace f_00483cd0 {
// MATCH: golf_clean.exe 0x00483cd0 ?FUN_00483cd0@f_00483cd0@@YAXXZ
extern int DAT_0083ad50;
void __stdcall FUN_00497b20();
void __stdcall FUN_0049c8e0();
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); virtual int f73(); virtual int f74(); virtual int f75(); virtual int f76(); virtual int f77(); virtual int f78(); virtual int f79(); };
void __cdecl FUN_00483cd0()
{
  FUN_0049c8e0();
  FUN_00497b20();
                    /* WARNING: Could not recover jumptable at 0x00483ce2. Too many branches */ /* WARNING: Treating indirect jump as call */ ((VT_1 *)(DAT_0083ad50))->f79();
  return;
}
}

namespace f_00483d30 {
// MATCH: golf_clean.exe 0x00483d30 ?FUN_00483d30@f_00483d30@@YAXXZ
void __stdcall FUN_00483d40();
void __stdcall FUN_00483d60();
void __stdcall FUN_00497b20();
void __cdecl FUN_00483d30()
{
  FUN_00483d40();
  FUN_00483d60();
  FUN_00497b20();
  return;
}
}

namespace f_00483d40 {
// MATCH: golf_clean.exe 0x00483d40 ?FUN_00483d40@f_00483d40@@YAXXZ
extern int DAT_0083ad50;
void __stdcall FUN_00497b20();
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); virtual int f73(); virtual int f74(); virtual int f75(); virtual int f76(); virtual int f77(); virtual int f78(); virtual int f79(); virtual int f80(); virtual int f81(); virtual int f82(); };
void __cdecl FUN_00483d40()
{
  ((VT_1 *)(DAT_0083ad50))->f82();
  FUN_00497b20();
  return;
}
}

namespace f_00483d60 {
// MATCH: golf_clean.exe 0x00483d60 ?FUN_00483d60@f_00483d60@@YAXXZ
extern int DAT_0083ab40;
extern int DAT_0083ab44;
extern int DAT_0083ad50;
void __stdcall FUN_00497b20();
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); virtual int f73(); virtual int f74(); virtual int f75(); virtual int f76(); virtual int f77(); virtual int f78(); virtual int f79(); virtual int f80(); virtual int f81(); virtual int f82(); virtual int f83(); };
void __cdecl FUN_00483d60()
{
  ((VT_1 *)(DAT_0083ad50))->f83();
  DAT_0083ab40 = 0;
  DAT_0083ab44 = 0;
  FUN_00497b20();
  return;
}
}

namespace f_00483d80 {
// MATCH: golf_clean.exe 0x00483d80 ?FUN_00483d80@f_00483d80@@YAXXZ
extern int DAT_0083ad50;
void __stdcall FUN_00497b20();
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
void __cdecl FUN_00483d80()
{
  FUN_00497b20();
                    /* WARNING: Could not recover jumptable at 0x00483d8d. Too many branches */ /* WARNING: Treating indirect jump as call */ ((VT_1 *)(DAT_0083ad50))->f72();
  return;
}
}

namespace f_00483de0 {
// MATCH: golf_clean.exe 0x00483de0 ?FUN_00483de0@f_00483de0@@YAXXZ
extern int DAT_0083ad80;
undefined4 * __fastcall FUN_00487280(undefined4 *);
void __cdecl FUN_00483de0()
{
  FUN_00487280((undefined4 *)((char *)&DAT_0083ad80));
  return;
}
}

namespace f_00483e20 {
// MATCH: golf_clean.exe 0x00483e20 ?FUN_00483e20@f_00483e20@@YAXXZ
extern int DAT_0083ad58;
undefined4 * __fastcall FUN_00487a20(undefined4 *);
void __cdecl FUN_00483e20()
{
  FUN_00487a20((undefined4 *)((char *)&DAT_0083ad58));
  return;
}
}

namespace f_00483e60 {
// MATCH: golf_clean.exe 0x00483e60 ?FUN_00483e60@f_00483e60@@YAXXZ
extern int DAT_0083af98;
undefined4 * __fastcall FUN_00487b40(undefined4 *);
void __cdecl FUN_00483e60()
{
  FUN_00487b40((undefined4 *)((char *)&DAT_0083af98));
  return;
}
}

namespace f_00484130 {
// MATCH: golf_clean.exe 0x00484130 ?FUN_00484130@f_00484130@@YAXXZ
extern int DAT_0083af6c;
void __cdecl FUN_00484130()
{
    int iVar1;
    undefined4 * puVar2;
  puVar2 = (unsigned int *)(((char *)&DAT_0083af6c));
  for (iVar1 = 0xb; iVar1 != 0; iVar1 = iVar1 + -1) {
    *puVar2 = 0;
    puVar2 = puVar2 + 1;
  }
  return;
}
}

namespace f_004843e0 {
// MATCH: golf_clean.exe 0x004843e0 ?FUN_004843e0@f_004843e0@@YAIH@Z
extern int DAT_0083af64;
extern int DAT_0083af68;
undefined4 __cdecl FUN_004843e0(int param_1)
{
  if (param_1 == 0) {
    return 10;
  }
  if ((*(byte *)(param_1 + 0x44) & 2) == 0) {
    return 0;
  }
  if (!(*(int *)(param_1 + 0x48) == 0)) {
    *(undefined4 *)(*(int *)(param_1 + 0x48) + 0x4c) = *(undefined4 *)(param_1 + 0x4c);
  } else {
    DAT_0083af68 = *(undefined4 *)(param_1 + 0x4c);
  }
  if (!(*(int *)(param_1 + 0x4c) == 0)) {
    *(undefined4 *)(*(int *)(param_1 + 0x4c) + 0x48) = *(undefined4 *)(param_1 + 0x48);
  } else {
    DAT_0083af64 = *(undefined4 *)(param_1 + 0x48);
  }
  *(undefined4 *)(param_1 + 0x4c) = 0;
  *(undefined4 *)(param_1 + 0x48) = 0;
  *(uint *)(param_1 + 0x44) = *(uint *)(param_1 + 0x44) & 0xfffffdfd;
  return 0;
}
}

namespace f_004845d0 {
// MATCH: golf_clean.exe 0x004845d0 ?FUN_004845d0@f_004845d0@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); };
undefined4 __fastcall FUN_004845d0(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x40) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at 0x004845d9. Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f23();
    return uVar1;
  }
  return 0;
}
}

namespace f_004846b0 {
// MATCH: golf_clean.exe 0x004846b0 ?FUN_004846b0@C_FUN_004846b0@f_004846b0@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(int); };
struct C_FUN_004846b0 { void FUN_004846b0(undefined4 param_1); };
void C_FUN_004846b0::FUN_004846b0(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x34) = param_1;
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f19((int)(param_1));
  }
  return;
}
}

namespace f_004846d0 {
// MATCH: golf_clean.exe 0x004846d0 ?FUN_004846d0@C_FUN_004846d0@f_004846d0@@QAEIH@Z
struct VT_1 { virtual int f0(); virtual int f1(int); };
struct C_FUN_004846d0 { undefined4 FUN_004846d0(int param_1); };
undefined4 C_FUN_004846d0::FUN_004846d0(int param_1)
{
  if (param_1 == 0) {
    return 10;
  }
  *(int *)((int)this + 0x38) = param_1;
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f1((int)(param_1));
  }
  return 0;
}
}

namespace f_00484750 {
// MATCH: golf_clean.exe 0x00484750 ?FUN_00484750@C_FUN_00484750@f_00484750@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct C_FUN_00484750 { void FUN_00484750(undefined4 param_1); };
void C_FUN_00484750::FUN_00484750(undefined4 param_1)
{
  if ((((VT_1 *)(this))->f1((int)(param_1))) == 0) {
    ((VT_2 *)(this))->f10();
  }
  return;
}
}

namespace f_004847b0 {
// MATCH: golf_clean.exe 0x004847b0 ?FUN_004847b0@C_FUN_004847b0@f_004847b0@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct C_FUN_004847b0 { void FUN_004847b0(undefined4 param_1); };
void C_FUN_004847b0::FUN_004847b0(undefined4 param_1)
{
  if ((((VT_1 *)(this))->f21((int)(param_1))) == 0) {
    ((VT_2 *)(this))->f10();
  }
  return;
}
}

namespace f_004847f0 {
// MATCH: golf_clean.exe 0x004847f0 ?FUN_004847f0@C_FUN_004847f0@f_004847f0@@QAEXH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(int); };
struct C_FUN_004847f0 { void FUN_004847f0(int param_1); };
void C_FUN_004847f0::FUN_004847f0(int param_1)
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

namespace f_00484e30 {
// MATCH: golf_clean.exe 0x00484e30 ?FUN_00484e30@C_FUN_00484e30@f_00484e30@@QAEIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(int, int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); };
struct C_FUN_00484e30 { undefined4 FUN_00484e30(undefined4 param_1, uint param_2); };
undefined4 C_FUN_00484e30::FUN_00484e30(undefined4 param_1, uint param_2)
{
  ((VT_1 *)(this))->f34((int)(param_1), (int)(param_2));
  if ((param_2 & 4) != 0) {
    return 0;
  }
  return (((VT_2 *)(this))->f35());
}
}

namespace f_00484f40 {
// MATCH: golf_clean.exe 0x00484f40 ?FUN_00484f40@C_FUN_00484f40@f_00484f40@@QAEIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(int); };
struct C_FUN_00484f40 { undefined4 FUN_00484f40(int param_1); };
undefined4 C_FUN_00484f40::FUN_00484f40(int param_1)
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
