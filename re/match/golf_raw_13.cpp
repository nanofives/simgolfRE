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

namespace f_00436b00 {
// MATCH: golf_clean.exe 0x00436b00 ?FUN_00436b00@f_00436b00@@YAHHH@Z
extern int DAT_004c7af0;
extern int DAT_004c7af8;
extern int DAT_004c7afc;
int __cdecl FUN_00467170(int, int);
int __cdecl FUN_00436b00(int param_1, int param_2)
{
    int iVar1;
    int iVar2;
    short * psVar3;
    int local_4;
  local_4 = -1;
  iVar1 = FUN_00467170(param_1 + -0x11e,param_2 + -0x1ec);
  if (iVar1 < 0x10) {
    local_4 = -2;
  }
  iVar1 = FUN_00467170(param_1 + -0x100,param_2 + -0x1fe);
  if (iVar1 < 0x10) {
    local_4 = -3;
  }
  iVar1 = 0;
  psVar3 = (short *)(((char *)&DAT_004c7af0));
  do {
    if (((int)(psVar3) == (int)(((char *)&DAT_004c7af8))) || ((int)(psVar3) == (int)(((char *)&DAT_004c7afc)))) {
      iVar2 = FUN_00467170((param_1 - *psVar3) * 4 + -0x20,(param_2 - psVar3[1]) + -0x1e);
      if (iVar2 < 0x1e) {
        return iVar1;
      }
    }
    else {
      iVar2 = FUN_00467170((param_1 - (iVar1 < 8 ? 0x10 : 0x20)) - (int)*psVar3, (param_2 - psVar3[1]) + -0x10);
      if (iVar2 < 0x10) {
        return iVar1;
      }
    }
    psVar3 = psVar3 + 2;
    iVar1 = iVar1 + 1;
  } while (*psVar3 != -1);
  if (local_4 == 1) {
    local_4 = 0;
  }
  return local_4;
}
}

namespace f_0046de70 {
// MATCH: golf_clean.exe 0x0046de70 ?FUN_0046de70@f_0046de70@@YAXPADII@Z
extern int DAT_00839260;
extern int DAT_00839264;
extern int DAT_00839278;
int __cdecl FUN_0046dea0(char *, int);
void __cdecl FUN_0046de70(char *param_1, undefined4 param_2, undefined4 param_3)
{
  DAT_00839260 = param_2;
  DAT_00839264 = param_3;
  DAT_00839278 = 1;
  FUN_0046dea0(param_1, 1);
  return;
}
}

namespace f_00478a90 {
// MATCH: golf_clean.exe 0x00478a90 ?FUN_00478a90@C_FUN_00478a90@f_00478a90@@QAEXIIHHHH@Z
extern char s_004e428c[];
extern char s_004e4280[];
struct T_FUN_004929b0 { int FUN_004929b0(undefined4, undefined4, int, int, int, int, char *); };
struct C_FUN_00478a90 { void FUN_00478a90(undefined4 param_1, undefined4 param_2, int param_3, int param_4, int param_5, int param_6); };
void C_FUN_00478a90::FUN_00478a90(undefined4 param_1, undefined4 param_2, int param_3, int param_4, int param_5, int param_6)
{
    int iVar1;
  if (++*(int *)((int)this + 0x2b4) > 0x14) {
    MessageBoxA((HWND)0x0,s_004e428c,s_004e4280,0);
    return;
  }
  ((T_FUN_004929b0 *)((void *)((int)this + 0x254)))->FUN_004929b0(param_1, param_2, param_3, param_4, param_5, param_6, 0);
  return;
}
}

namespace f_00478c10 {
// MATCH: golf_clean.exe 0x00478c10 ?FUN_00478c10@C_FUN_00478c10@f_00478c10@@QAEXHHHI@Z
struct T_FUN_00485d40 { void FUN_00485d40(int, int, int, undefined1); };
struct T_FUN_00494d30 { void FUN_00494d30(int, int, int, uint); };
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); };
struct C_FUN_00478c10 { void FUN_00478c10(int param_1, int param_2, int param_3, uint param_4); };
void C_FUN_00478c10::FUN_00478c10(int param_1, int param_2, int param_3, uint param_4)
{
    int * piVar1;
  if (!(*(int **)((int)this + 4) == 0)) {
    piVar1 = (int *)((VT_1 *)(*(int **)((int)this + 4)))->f57();
  } else {
    piVar1 = 0;
  }
  switch (*piVar1) {
  case 8:
    ((T_FUN_00485d40 *)(this))->FUN_00485d40(param_1, param_2, param_3, (char)param_4);
    break;
  case 0x10:
    ((T_FUN_00494d30 *)(this))->FUN_00494d30(param_1, param_2, param_3, param_4);
    break;
  }
  return;
}
}

namespace f_00478c70 {
// MATCH: golf_clean.exe 0x00478c70 ?FUN_00478c70@C_FUN_00478c70@f_00478c70@@QAEXHHHI@Z
struct T_FUN_00485e80 { void FUN_00485e80(int, int, int, undefined1); };
struct T_FUN_00494f00 { void FUN_00494f00(int, int, int, uint); };
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); };
struct C_FUN_00478c70 { void FUN_00478c70(int param_1, int param_2, int param_3, uint param_4); };
void C_FUN_00478c70::FUN_00478c70(int param_1, int param_2, int param_3, uint param_4)
{
    int * piVar1;
  if (!(*(int **)((int)this + 4) == 0)) {
    piVar1 = (int *)((VT_1 *)(*(int **)((int)this + 4)))->f57();
  } else {
    piVar1 = 0;
  }
  switch (*piVar1) {
  case 8:
    ((T_FUN_00485e80 *)(this))->FUN_00485e80(param_1, param_2, param_3, (char)param_4);
    break;
  case 0x10:
    ((T_FUN_00494f00 *)(this))->FUN_00494f00(param_1, param_2, param_3, param_4);
    break;
  }
  return;
}
}

namespace f_0047ab00 {
// MATCH: golf_clean.exe 0x0047ab00 ?FUN_0047ab00@C_FUN_0047ab00@f_0047ab00@@QAEIH@Z
void __cdecl FUN_0047d130(int *, int, int, int);
struct C_FUN_0047ab00 { undefined4 FUN_0047ab00(int param_1); };
undefined4 C_FUN_0047ab00::FUN_0047ab00(int param_1)
{
  if ((param_1 >= 0x7f00) && (param_1 <= 0x7f8a)) {
    *(undefined4 *)((int)this + 0x208) = 0;
    *(int *)((int)this + 0x218) = param_1;
    *(undefined4 *)((int)this + 0x214) = 0;
    FUN_0047d130(0,1,-1,-1);
    return 0;
  }
  return 3;
}
}

namespace f_0047ab50 {
// MATCH: golf_clean.exe 0x0047ab50 ?FUN_0047ab50@C_FUN_0047ab50@f_0047ab50@@QAEXPAH0@Z
extern int DAT_0083aa9c;
extern int DAT_0083ad50;
struct T_FUN_0047b200 { void FUN_0047b200(int *, int *); };
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(int, int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(int); };
struct C_FUN_0047ab50 { void FUN_0047ab50(int *param_1, int *param_2); };
void C_FUN_0047ab50::FUN_0047ab50(int *param_1, int *param_2)
{
    RECT rc;
  if ((param_1 != 0) && (param_2 != 0)) {
    if (DAT_0083ad50 != 0) {
      ((VT_1 *)(DAT_0083ad50))->f11((int)(param_1), (int)(param_2));
    }
    ((T_FUN_0047b200 *)(this))->FUN_0047b200(param_1, param_2);
    if ((int)(this) == (int)(DAT_0083aa9c)) {
      if (DAT_0083ad50 != 0) {
        ((VT_2 *)(DAT_0083ad50))->f14((int)(&rc));
      }
      *param_1 = *param_1 - rc.left;
      *param_2 = *param_2 - rc.top;
    }
  }
  return;
}
}

namespace f_0047e5f0 {
// MATCH: golf_clean.exe 0x0047e5f0 ?FUN_0047e5f0@C_FUN_0047e5f0@f_0047e5f0@@QAEXH@Z
void __fastcall FUN_00479b40(int);
struct C_FUN_0047e5f0 { void FUN_0047e5f0(int param_1); };
void C_FUN_0047e5f0::FUN_0047e5f0(int param_1)
{
    int iVar1;
  if (param_1 != 0) {
    if (*(int *)((int)this + 0x22c) + 1 > *(int *)((int)this + 0x228)) {
      FUN_00479b40((int)this);
    }
    if ((*(uint *)(param_1 + 0x9c) & 0x2000000) != 0) {
      *(int *)(*(int *)((int)this + 0x224) + *(int *)((int)this + 0x22c) * 4) = param_1;
      *(int *)((int)this + 0x22c) = *(int *)((int)this + 0x22c) + 1;
      return;
    }
    for (iVar1 = *(int *)((int)this + 0x22c); 0 < iVar1; iVar1 = iVar1 + -1) {
      *(undefined4 *)(*(int *)((int)this + 0x224) + iVar1 * 4) = *(undefined4 *)(*(int *)((int)this + 0x224) + -4 + iVar1 * 4);
    }
    **(int **)((int)this + 0x224) = param_1;
    *(int *)((int)this + 0x22c) = *(int *)((int)this + 0x22c) + 1;
  }
  return;
}
}

namespace f_00480c80 {
// MATCH: golf_clean.exe 0x00480c80 ?FUN_00480c80@C_FUN_00480c80@f_00480c80@@QAEXH@Z
struct T_FUN_00480c20 { void FUN_00480c20(int, int, int, int, int); };
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); };
struct C_FUN_00480c80 { void FUN_00480c80(int pad_1); };
void C_FUN_00480c80::FUN_00480c80(int pad_1)
{
    int iVar1;
    int * piVar2;
  iVar1 = (*(int **)((int)((void *)this) + 0x278) != 0) ? ((VT_1 *)(*(int **)((int)((void *)this) + 0x278)))->f53() : 0;
  piVar2 = (*(int **)((int)((void *)this) + 0x278) != 0) ? (int *)((VT_2 *)(*(int **)((int)((void *)this) + 0x278)))->f53() : 0;
  ((T_FUN_00480c20 *)(((void *)this)))->FUN_00480c20(0, 0, piVar2[2] - *piVar2, *(int *)(iVar1 + 0xc) - *(int *)(iVar1 + 4), 0);
  return;
}
}

namespace f_00483800 {
// MATCH: golf_clean.exe 0x00483800 ?FUN_00483800@C_FUN_00483800@f_00483800@@QAEIHIH@Z
extern int DAT_0083ad50;
void __fastcall FUN_004838b0(int);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(int, int, int); };
struct C_FUN_00483800 { undefined4 FUN_00483800(int param_1, undefined4 param_2, int pad_3); };
undefined4 C_FUN_00483800::FUN_00483800(int param_1, undefined4 param_2, int pad_3)
{
    int iVar1;
    int * piVar2;
  if (param_1 == 0) {
    return 3;
  }
  FUN_004838b0((int)this);
  if (!(this == 0)) {
    iVar1 = (int)this + 4;
  } else {
    iVar1 = 0;
  }
  piVar2 = (int *)((VT_1 *)(DAT_0083ad50))->f29((int)(iVar1));
  *(int **)((int)this + 4) = piVar2;
  return (((VT_2 *)(piVar2))->f2((int)(param_1), (int)(param_2), (int)(pad_3)));
}
}

namespace f_00483850 {
// MATCH: golf_clean.exe 0x00483850 ?FUN_00483850@C_FUN_00483850@f_00483850@@QAEIHHIH@Z
extern int DAT_0083ad50;
void __fastcall FUN_004838b0(int);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(int); };
struct VT_2 { virtual int f0(); virtual int f1(int, int, int, int); };
struct C_FUN_00483850 { undefined4 FUN_00483850(int param_1, int param_2, undefined4 param_3, int pad_4); };
undefined4 C_FUN_00483850::FUN_00483850(int param_1, int param_2, undefined4 param_3, int pad_4)
{
    int iVar1;
    int * piVar2;
  if ((param_1 != 0) && (param_2 != 0)) {
    FUN_004838b0((int)this);
    if (!(this == 0)) {
      iVar1 = (int)this + 4;
    } else {
      iVar1 = 0;
    }
    piVar2 = (int *)((VT_1 *)(DAT_0083ad50))->f29((int)(iVar1));
    *(int **)((int)this + 4) = piVar2;
    return (((VT_2 *)(piVar2))->f1((int)(param_1), (int)(param_2), (int)(param_3), (int)(pad_4)));
  }
  return 0x10;
}
}

namespace f_00483ac0 {
// MATCH: golf_clean.exe 0x00483ac0 ?FUN_00483ac0@f_00483ac0@@YAHPAX@Z
extern int DAT_004e43ac;
extern int DAT_0083ad44;
extern int DAT_0083ad50;
struct T_FUN_00483800 { undefined4 FUN_00483800(int, undefined4, int); };
void __stdcall FUN_00483b10();
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); };
int __cdecl FUN_00483ac0(void *param_1)
{
    int iVar1;
  if (param_1 == 0) {
    return 3;
  }
  FUN_00483b10();
  iVar1 = ((VT_1 *)(DAT_0083ad50))->f46();
  if (iVar1 == 0) {
    DAT_0083ad44 = (int)(param_1);
    if ((*(int *)((int)param_1 + 4) == 0) && (iVar1 = ((T_FUN_00483800 *)(param_1))->FUN_00483800((int)(((char *)&DAT_004e43ac)), 0xc, 0), iVar1 != 0)) {
      return iVar1;
    }
    iVar1 = 0;
  }
  return iVar1;
}
}

namespace f_00486360 {
// MATCH: golf_clean.exe 0x00486360 ?FUN_00486360@C_FUN_00486360@f_00486360@@QAEXPAIHHPAH1@Z
struct T_FUN_00477280 { int FUN_00477280(char *, char *); };
int __fastcall FUN_00477580(int);
struct T_FUN_00486330 { int FUN_00486330(int, uint *, int); };
struct C_FUN_00486360 { void FUN_00486360(uint *param_1, int param_2, int param_3, int *param_4, int *param_5); };
void C_FUN_00486360::FUN_00486360(uint *param_1, int param_2, int param_3, int *param_4, int *param_5)
{
    int iVar1;
    int iVar2;
  iVar1 = ((T_FUN_00486330 *)(this))->FUN_00486330(param_3, param_1, param_2);
  iVar2 = ((T_FUN_00477280 *)((void *)((int)this + 0x274)))->FUN_00477280((char *)param_1[iVar1], (char *)((*(int *)((int)this + 0x574) - (int)param_1[iVar1]) + param_3));
  *param_4 = iVar2;
  *param_5 = FUN_00477580((int)this + 0x274) * (iVar1 - *(int *)((int)this + 0x57c));
  iVar1 = *(int *)((int)this + 0x59c) + *(int *)((int)this + 0x598);
  *param_4 = *param_4 + iVar1;
  *param_5 = *param_5 + iVar1;
  return;
}
}

namespace f_0048e0b0 {
// MATCH: golf_clean.exe 0x0048e0b0 ?FUN_0048e0b0@C_FUN_0048e0b0@f_0048e0b0@@QAEXH@Z
extern int DAT_0083ad50;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); };
struct C_FUN_0048e0b0 { void FUN_0048e0b0(int param_1); };
void C_FUN_0048e0b0::FUN_0048e0b0(int param_1)
{
  if (((*(int *)((int)this + 0x590) == 0) && ((*(uint *)((int)this + 0x1f68) & 0x400) == 0)) && (DAT_0083ad50 != 0)) {
    if ((((VT_1 *)(DAT_0083ad50))->f42()) >= 0x400) {
      *(int *)(*(int *)(*(int *)((int)this + 0x1464) + 8) + 0x1498 + (int)this) = (param_1 * 3) / 2;
      return;
    }
  }
  *(int *)(*(int *)(*(int *)((int)this + 0x1464) + 8) + 0x1498 + (int)this) = param_1;
  return;
}
}

namespace f_0048e120 {
// MATCH: golf_clean.exe 0x0048e120 ?FUN_0048e120@C_FUN_0048e120@f_0048e120@@QAEXH@Z
extern int DAT_0083ad50;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); };
struct C_FUN_0048e120 { void FUN_0048e120(int param_1); };
void C_FUN_0048e120::FUN_0048e120(int param_1)
{
  if (((*(int *)((int)this + 0x590) == 0) && ((*(uint *)((int)this + 0x1f68) & 0x400) == 0)) && (DAT_0083ad50 != 0)) {
    if ((((VT_1 *)(DAT_0083ad50))->f42()) >= 0x400) {
      *(int *)(*(int *)(*(int *)((int)this + 0x1464) + 8) + 0x1494 + (int)this) = (param_1 * 3) / 2;
      return;
    }
  }
  *(int *)(*(int *)(*(int *)((int)this + 0x1464) + 8) + 0x1494 + (int)this) = param_1;
  return;
}
}

namespace f_004967f0 {
// MATCH: golf_clean.exe 0x004967f0 ?FUN_004967f0@C_FUN_004967f0@f_004967f0@@QAEXH@Z
extern int _DAT_0083ab2c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct C_FUN_004967f0 { /* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ void FUN_004967f0(int param_1); };
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ void C_FUN_004967f0::FUN_004967f0(int param_1)
{
    int iVar1;
  _DAT_0083ab2c = *(undefined4 *)((int)this + 0x130);
  iVar1 = *(int *)((int)this + 0x580);
  if (param_1 < iVar1) {
    *(int *)((int)this + 0x58c) = iVar1;
  }
  else if (param_1 > *(int *)((int)this + 0x584)) {
    *(int *)((int)this + 0x58c) = *(int *)((int)this + 0x584);
  }
  else {
    *(int *)((int)this + 0x58c) = param_1;
  }
  if (*(int *)((int)this + 0x588) != 0) {
    *(int *)((int)this + 0x58c) = (*(int *)((int)this + 0x584) - *(int *)((int)this + 0x58c)) + iVar1;
  }
  ((VT_1 *)(this))->f72();
  return;
}
}

namespace f_004a4ad0 {
// MATCH: golf_clean.exe 0x004a4ad0 ?FUN_004a4ad0@C_FUN_004a4ad0@f_004a4ad0@@QAEHH@Z
struct C_FUN_004a4ad0 { int FUN_004a4ad0(int param_1); };
int C_FUN_004a4ad0::FUN_004a4ad0(int param_1)
{
    int iVar1;
    int * piVar2;
  iVar1 = 0;
  piVar2 = (int *)((int)this + 0x580);
  while( true ) {
    if (*piVar2 == -1) {
      return -1;
    }
    if (*piVar2 == param_1) break;
    iVar1 = iVar1 + 1;
    piVar2 = piVar2 + 6;
    if (iVar1 >= 0x100) {
      return -1;
    }
  }
  return iVar1;
}
}
