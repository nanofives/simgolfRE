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

namespace f_004852d0 {
// MATCH: golf_clean.exe 0x004852d0 ?FUN_004852d0@f_004852d0@@YIXPAI@Z
extern void *PTR_FUN_004badec;
void __fastcall FUN_004852d0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004badec);
  return;
}
}

namespace f_00485590 {
// MATCH: golf_clean.exe 0x00485590 ?FUN_00485590@f_00485590@@YAXPBD0@Z
void __cdecl FUN_00485590(LPCSTR param_1, LPCSTR param_2)
{
  MessageBoxA((HWND)0x0,param_1,param_2,0);
  return;
}
}

namespace f_00486ce0 {
// MATCH: golf_clean.exe 0x00486ce0 ?FUN_00486ce0@f_00486ce0@@YIXPAI@Z
extern void *PTR_FUN_004baea0;
void __fastcall FUN_00486f10(int);
void __fastcall FUN_00486ce0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004baea0);
  FUN_00486f10((int)param_1);
  return;
}
}

namespace f_00486cf0 {
// MATCH: golf_clean.exe 0x00486cf0 ?FUN_00486cf0@C_FUN_00486cf0@f_00486cf0@@QAEXIIII@Z
void __fastcall FUN_00486f10(int);
struct C_FUN_00486cf0 { void FUN_00486cf0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_00486cf0::FUN_00486cf0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  FUN_00486f10((int)this);
  *(undefined4 *)((int)this + 0x10) = param_1;
  *(undefined4 *)((int)this + 0x1c) = param_2;
  *(undefined4 *)((int)this + 0x14) = 0;
  *(undefined4 *)((int)this + 0x18) = 0;
  *(undefined4 *)((int)this + 0x20) = param_3;
  *(undefined4 *)((int)this + 0x28) = param_4;
  return;
}
}

namespace f_00486fe0 {
// MATCH: golf_clean.exe 0x00486fe0 ?FUN_00486fe0@f_00486fe0@@YAIXZ
extern int DAT_0083afec;
undefined4 __cdecl FUN_00486fe0()
{
  DAT_0083afec = DAT_0083afec + 1;
  return 0;
}
}

namespace f_00486ff0 {
// MATCH: golf_clean.exe 0x00486ff0 ?FUN_00486ff0@f_00486ff0@@YAXXZ
extern int DAT_0083afec;
void __cdecl FUN_00486ff0()
{
  DAT_0083afec = DAT_0083afec + -1;
  return;
}
}

namespace f_00487040 {
// MATCH: golf_clean.exe 0x00487040 ?FUN_00487040@f_00487040@@YIXPAI@Z
extern void *PTR_FUN_004baea8;
void __fastcall FUN_00487060(int);
void __fastcall FUN_00487040(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004baea8);
  FUN_00487060((int)param_1);
  return;
}
}

namespace f_00487050 {
// MATCH: golf_clean.exe 0x00487050 ?FUN_00487050@C_FUN_00487050@f_00487050@@QAEXI@Z
struct C_FUN_00487050 { void FUN_00487050(undefined4 param_1); };
void C_FUN_00487050::FUN_00487050(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0xc) = param_1;
  return;
}
}

namespace f_00487260 {
// MATCH: golf_clean.exe 0x00487260 ?FUN_00487260@f_00487260@@YIXPAI@Z
extern void *PTR_FUN_004baeac;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
void __fastcall FUN_00487260(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004baeac);
  if ((int *)param_1[5] != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at 0x0048726f. Too many branches */ /* WARNING: Treating indirect jump as call */ ((VT_1 *)(param_1[5]))->f5();
    return;
  }
  return;
}
}

namespace f_00487460 {
// MATCH: golf_clean.exe 0x00487460 ?FUN_00487460@f_00487460@@YIIH@Z
undefined4 __fastcall FUN_00487430(int);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
undefined4 __fastcall FUN_00487460(int param_1)
{
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x14)))->f5();
    FUN_00487430(param_1);
    *(undefined4 *)(param_1 + 0x14) = 0;
  }
  return 0;
}
}

namespace f_00487630 {
// MATCH: golf_clean.exe 0x00487630 ?FUN_00487630@f_00487630@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); };
undefined4 __fastcall FUN_00487630(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at 0x00487639. Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x14)))->f29();
    return uVar1;
  }
  return 0;
}
}

namespace f_00487a20 {
// MATCH: golf_clean.exe 0x00487a20 ?FUN_00487a20@f_00487a20@@YIPAIPAI@Z
extern void *PTR_FUN_004bafb0;
void __fastcall FUN_00487210(undefined4 *);
undefined4 * __fastcall FUN_00487a20(undefined4 *param_1)
{
  FUN_00487210(param_1);
  *param_1 = (unsigned int)(&PTR_FUN_004bafb0);
  return param_1;
}
}

namespace f_00487a60 {
// MATCH: golf_clean.exe 0x00487a60 ?FUN_00487a60@f_00487a60@@YIXPAI@Z
extern void *PTR_FUN_004bafb0;
void __fastcall FUN_00487260(undefined4 *);
void __fastcall FUN_00487a60(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004bafb0);
  FUN_00487260(param_1);
  return;
}
}

namespace f_00487ac0 {
// MATCH: golf_clean.exe 0x00487ac0 ?FUN_00487ac0@f_00487ac0@@YIIH@Z
undefined4 __fastcall FUN_004879f0(int);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
undefined4 __fastcall FUN_00487ac0(int param_1)
{
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x14)))->f5();
    FUN_004879f0(param_1);
    *(undefined4 *)(param_1 + 0x14) = 0;
  }
  return 0;
}
}

namespace f_00487b40 {
// MATCH: golf_clean.exe 0x00487b40 ?FUN_00487b40@f_00487b40@@YIPAIPAI@Z
extern void *PTR_FUN_004bb014;
void __fastcall FUN_00487210(undefined4 *);
undefined4 * __fastcall FUN_00487b40(undefined4 *param_1)
{
  FUN_00487210(param_1);
  *param_1 = (unsigned int)(&PTR_FUN_004bb014);
  return param_1;
}
}

namespace f_00487b80 {
// MATCH: golf_clean.exe 0x00487b80 ?FUN_00487b80@f_00487b80@@YIXPAI@Z
extern void *PTR_FUN_004bb014;
void __fastcall FUN_00487260(undefined4 *);
void __fastcall FUN_00487b80(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004bb014);
  FUN_00487260(param_1);
  return;
}
}

namespace f_00487c40 {
// MATCH: golf_clean.exe 0x00487c40 ?FUN_00487c40@f_00487c40@@YIIPAH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); };
undefined4 __fastcall FUN_00487c40(int *param_1)
{
  if ((int *)param_1[5] != (int *)0x0) {
    ((VT_1 *)(param_1[5]))->f5();
    ((VT_2 *)(param_1))->f2();
    param_1[5] = 0;
  }
  return 0;
}
}

namespace f_00487d80 {
// MATCH: golf_clean.exe 0x00487d80 ?FUN_00487d80@f_00487d80@@YAXXZ
extern int DAT_0083b000;
struct T_FUN_00487ea0 { undefined4 * FUN_00487ea0(undefined4); };
void __cdecl FUN_00487d80()
{
  ((T_FUN_00487ea0 *)(((char *)&DAT_0083b000)))->FUN_00487ea0(0x200);
  return;
}
}

namespace f_00487ea0 {
// MATCH: golf_clean.exe 0x00487ea0 ?FUN_00487ea0@C_FUN_00487ea0@f_00487ea0@@QAEPAII@Z
extern void *PTR_FUN_004bb084;
struct T_FUN_00487ee0 { int FUN_00487ee0(size_t); };
struct C_FUN_00487ea0 { undefined4 * FUN_00487ea0(size_t param_1); };
undefined4 * C_FUN_00487ea0::FUN_00487ea0(size_t param_1)
{
  *(undefined1 *)((int)this + 4) = 0;
  *(undefined4 *)((int)this + 0x154) = 0;
  *(undefined4 *)((int)this + 0x158) = 0;
  *(undefined4 *)((int)this + 0x15c) = 0;
  *(undefined4 *)((int)this + 0x160) = 0;
  *(undefined ***)this = (unsigned char ** )(&PTR_FUN_004bb084);
  ((T_FUN_00487ee0 *)(this))->FUN_00487ee0(param_1);
  return (unsigned int *)(this);
}
}

namespace f_004882a0 {
// MATCH: golf_clean.exe 0x004882a0 ?FUN_004882a0@f_004882a0@@YIPAIPAI@Z
extern void *PTR_FUN_004bb088;
undefined4 * __fastcall FUN_004747a0(undefined4 *);
undefined4 * __fastcall FUN_004882a0(undefined4 *param_1)
{
  FUN_004747a0(param_1 + 0x42);
  *param_1 = (unsigned int)(&PTR_FUN_004bb088);
  param_1[0x41] = 0;
  *(undefined1 *)(param_1 + 1) = 0;
  return param_1;
}
}

namespace f_004882f0 {
// MATCH: golf_clean.exe 0x004882f0 ?FUN_004882f0@f_004882f0@@YIXPAI@Z
extern void *PTR_FUN_004bb088;
void __fastcall FUN_00474810(undefined4 *);
void __fastcall FUN_004882f0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004bb088);
  param_1[0x41] = 0;
  *(undefined1 *)(param_1 + 1) = 0;
  FUN_00474810(param_1 + 0x42);
  return;
}
}

namespace f_00488930 {
// MATCH: golf_clean.exe 0x00488930 ?FUN_00488930@C_FUN_00488930@f_00488930@@QAEXIIII@Z
extern int DAT_0083ad10;
struct T_FUN_00476310 { void FUN_00476310(undefined4, undefined4, undefined4, undefined4); };
struct T_FUN_004789f0 { undefined4 FUN_004789f0(int); };
struct C_FUN_00488930 { void FUN_00488930(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_00488930::FUN_00488930(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  if (*(int *)((int)this + 0x130) != 0) {
    ((T_FUN_004789f0 *)((void *)((int)this + 0x274)))->FUN_004789f0(DAT_0083ad10);
    ((T_FUN_00476310 *)((void *)((int)this + 0x274)))->FUN_00476310(param_1, param_2, param_3, param_4);
  }
  return;
}
}

namespace f_00488970 {
// MATCH: golf_clean.exe 0x00488970 ?FUN_00488970@C_FUN_00488970@f_00488970@@QAEXIIII@Z
extern int DAT_0083ad10;
struct T_FUN_00476340 { void FUN_00476340(undefined4, undefined4, undefined4, undefined4); };
struct T_FUN_004789f0 { undefined4 FUN_004789f0(int); };
struct C_FUN_00488970 { void FUN_00488970(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_00488970::FUN_00488970(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  if (*(int *)((int)this + 0x130) != 0) {
    ((T_FUN_004789f0 *)((void *)((int)this + 0x274)))->FUN_004789f0(DAT_0083ad10);
    ((T_FUN_00476340 *)((void *)((int)this + 0x274)))->FUN_00476340(param_1, param_2, param_3, param_4);
  }
  return;
}
}

namespace f_004889b0 {
// MATCH: golf_clean.exe 0x004889b0 ?FUN_004889b0@C_FUN_004889b0@f_004889b0@@QAEXIIII@Z
extern int DAT_0083ad10;
struct T_FUN_00476370 { void FUN_00476370(undefined4, undefined4, undefined4, undefined4); };
struct T_FUN_004789f0 { undefined4 FUN_004789f0(int); };
struct C_FUN_004889b0 { void FUN_004889b0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_004889b0::FUN_004889b0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  if (*(int *)((int)this + 0x130) != 0) {
    ((T_FUN_004789f0 *)((void *)((int)this + 0x274)))->FUN_004789f0(DAT_0083ad10);
    ((T_FUN_00476370 *)((void *)((int)this + 0x274)))->FUN_00476370(param_1, param_2, param_3, param_4);
  }
  return;
}
}

namespace f_00489ab0 {
// MATCH: golf_clean.exe 0x00489ab0 ?FUN_00489ab0@C_FUN_00489ab0@f_00489ab0@@QAEIHII@Z
struct C_FUN_00489ab0 { undefined4 FUN_00489ab0(int param_1, undefined4 param_2, undefined4 param_3); };
undefined4 C_FUN_00489ab0::FUN_00489ab0(int param_1, undefined4 param_2, undefined4 param_3)
{
  if (param_1 == 0) {
    return 3;
  }
  if (*(int *)(param_1 + 4) != 0) {
    *(int *)((int)this + 0x74) = param_1;
  }
  *(undefined4 *)((int)this + 0x78) = param_2;
  *(undefined4 *)((int)this + 0x7c) = param_3;
  return 0;
}
}

namespace f_00489e40 {
// MATCH: golf_clean.exe 0x00489e40 ?FUN_00489e40@f_00489e40@@YIXH@Z
extern int DAT_004e44d0;
extern int DAT_004e44d4;
extern int DAT_004e44d8;
extern int DAT_004e44dc;
extern int DAT_0083b648;
void __fastcall FUN_00480610(int *);
void __fastcall FUN_004894b0(int);
void __fastcall FUN_00489e40(int param_1)
{
  FUN_00480610((int *)(*(int *)(*(int *)(param_1 + 4) + 4) + 4 + param_1));
  FUN_004894b0(*(int *)(*(int *)(param_1 + 4) + 8) + 4 + param_1);
  *(undefined4 *)(param_1 + 0x20) = 0xffffffff;
  *(undefined4 *)(param_1 + 0x24) = 0xffffffff;
  *(undefined4 *)(param_1 + 0xc) = 0;
  *(undefined4 *)(param_1 + 0x10) = 0;
  *(undefined4 *)(param_1 + 0x28) = 0;
  *(undefined4 *)(param_1 + 0x2c) = 0;
  *(undefined4 *)(param_1 + 0x30) = 0;
  *(undefined4 *)(param_1 + 0x34) = DAT_004e44d0;
  *(undefined4 *)(param_1 + 0x38) = DAT_0083b648;
  *(undefined4 *)(param_1 + 0x3c) = DAT_004e44d4;
  *(undefined4 *)(param_1 + 0x18) = DAT_004e44d8;
  *(undefined4 *)(param_1 + 0x1c) = DAT_004e44dc;
  *(undefined4 *)(*(int *)(*(int *)(param_1 + 4) + 8) + 0xbc + param_1) = 1;
  *(undefined4 *)(param_1 + 0x44) = 0;
  *(undefined4 *)(param_1 + 0x48) = 0;
  *(undefined4 *)(param_1 + 0x4c) = 0;
  *(undefined4 *)(param_1 + 0x50) = 0;
  *(undefined4 *)(param_1 + 0x54) = 0;
  *(undefined4 *)(param_1 + 0x14) = 0;
  return;
}
}

namespace f_0048c610 {
// MATCH: golf_clean.exe 0x0048c610 ?FUN_0048c610@C_FUN_0048c610@f_0048c610@@QAEXII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(int, int); };
struct C_FUN_0048c610 { void FUN_0048c610(undefined4 param_1, undefined4 param_2); };
void C_FUN_0048c610::FUN_0048c610(undefined4 param_1, undefined4 param_2)
{
    int * piVar1;
  piVar1 = *(int **)(*(int *)(*(int *)((int)this + -0x58) + 4) + 0xd8 + (int)this);
  if (piVar1 != (int *)0x0) {
    ((VT_1 *)(piVar1))->f29((int)(param_1), (int)(param_2));
  }
  return;
}
}

namespace f_0048e190 {
// MATCH: golf_clean.exe 0x0048e190 ?FUN_0048e190@C_FUN_0048e190@f_0048e190@@QAEXHH@Z
struct C_FUN_0048e190 { void FUN_0048e190(int param_1, int param_2); };
void C_FUN_0048e190::FUN_0048e190(int param_1, int param_2)
{
  if (param_1 != 0x2000) {
    *(int *)((int)this + 0x1fb8) = param_1;
  }
  if (param_2 != 0x2000) {
    *(int *)((int)this + 0x1fbc) = param_2;
  }
  return;
}
}

namespace f_00491490 {
// MATCH: golf_clean.exe 0x00491490 ?FUN_00491490@C_FUN_00491490@f_00491490@@QAEXH@Z
struct C_FUN_00491490 { void FUN_00491490(int param_1); };
void C_FUN_00491490::FUN_00491490(int param_1)
{
  if (param_1 != 0) {
    *(uint *)((int)this + 0x24) = *(uint *)((int)this + 0x24) | 1;
    return;
  }
  *(uint *)((int)this + 0x24) = *(uint *)((int)this + 0x24) & 0xfffffffe;
  return;
}
}

namespace f_004914b0 {
// MATCH: golf_clean.exe 0x004914b0 ?FUN_004914b0@C_FUN_004914b0@f_004914b0@@QAEXH@Z
struct C_FUN_004914b0 { void FUN_004914b0(int param_1); };
void C_FUN_004914b0::FUN_004914b0(int param_1)
{
  if (param_1 != 0) {
    *(uint *)((int)this + 0x24) = *(uint *)((int)this + 0x24) | 2;
    return;
  }
  *(uint *)((int)this + 0x24) = *(uint *)((int)this + 0x24) & 0xfffffffd;
  return;
}
}

namespace f_00492450 {
// MATCH: golf_clean.exe 0x00492450 ?FUN_00492450@f_00492450@@YAPAIXZ
extern int DAT_0083c310;
undefined4 * __cdecl FUN_00492450()
{
  return (unsigned int *)(((char *)&DAT_0083c310));
}
}

namespace f_00492dc0 {
// MATCH: golf_clean.exe 0x00492dc0 ?FUN_00492dc0@f_00492dc0@@YIXPAI@Z
extern void *PTR_FUN_004bba78;
void __fastcall FUN_00492e80(int);
void __fastcall FUN_00492dc0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004bba78);
  FUN_00492e80((int)param_1);
  return;
}
}

namespace f_00493670 {
// MATCH: golf_clean.exe 0x00493670 ?FUN_00493670@C_FUN_00493670@f_00493670@@QAEXPAEHH0I@Z
struct T_FUN_0048e1c0 { int FUN_0048e1c0(undefined *, int, int, undefined *, uint, int); };
struct C_FUN_00493670 { void FUN_00493670(undefined *param_1, int param_2, int param_3, undefined *param_4, uint param_5); };
void C_FUN_00493670::FUN_00493670(undefined *param_1, int param_2, int param_3, undefined *param_4, uint param_5)
{
  ((T_FUN_0048e1c0 *)(this))->FUN_0048e1c0(param_1, param_2, param_3, param_4, param_5, 0);
  return;
}
}

namespace f_00493a30 {
// MATCH: golf_clean.exe 0x00493a30 ?FUN_00493a30@f_00493a30@@YIXH@Z
void __fastcall FUN_00493a60(int);
struct VT_1 { virtual int f0(); virtual int f1(int); };
struct VT_2 { virtual int f0(); virtual int f1(int); };
void __fastcall FUN_00493a30(int param_1)
{
  FUN_00493a60(param_1);
  ((VT_1 *)((param_1 + 0x213c)))->f1((int)(0));
  ((VT_2 *)((param_1 + 0x26c4)))->f1((int)(0));
  return;
}
}

namespace f_00493ed0 {
// MATCH: golf_clean.exe 0x00493ed0 ?FUN_00493ed0@f_00493ed0@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); };
undefined4 __fastcall FUN_00493ed0(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x23b4) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at 0x00493edc. Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x23b4)))->f54();
    return uVar1;
  }
  return 0;
}
}

namespace f_00496690 {
// MATCH: golf_clean.exe 0x00496690 ?FUN_00496690@C_FUN_00496690@f_00496690@@QAEHHHHHH@Z
extern int DAT_0083ff10;
struct T_FUN_00496330 { int FUN_00496330(int, int, int, int, int, int, uint); };
struct C_FUN_00496690 { int FUN_00496690(int param_1, int param_2, int param_3, int param_4, int param_5); };
int C_FUN_00496690::FUN_00496690(int param_1, int param_2, int param_3, int param_4, int param_5)
{
  if ((param_4 != 0) && (param_3 != 0)) {
    return (((T_FUN_00496330 *)(this))->FUN_00496330(param_1, param_2, DAT_0083ff10, param_3, param_4, param_5, 0));
  }
  return 3;
}
}

namespace f_004966d0 {
// MATCH: golf_clean.exe 0x004966d0 ?FUN_004966d0@C_FUN_004966d0@f_004966d0@@QAEHHHHHH@Z
extern int DAT_0083ff10;
struct T_FUN_00496330 { int FUN_00496330(int, int, int, int, int, int, uint); };
struct C_FUN_004966d0 { int FUN_004966d0(int param_1, int param_2, int param_3, int param_4, int param_5); };
int C_FUN_004966d0::FUN_004966d0(int param_1, int param_2, int param_3, int param_4, int param_5)
{
  if ((param_4 != 0) && (param_3 != 0)) {
    return (((T_FUN_00496330 *)(this))->FUN_00496330(param_1, param_2, param_3, DAT_0083ff10, param_4, param_5, 0));
  }
  return 3;
}
}

namespace f_00496710 {
// MATCH: golf_clean.exe 0x00496710 ?FUN_00496710@C_FUN_00496710@f_00496710@@QAEXHHHHH@Z
extern int DAT_0083ff60;
struct T_FUN_00496690 { int FUN_00496690(int, int, int, int, int); };
struct C_FUN_00496710 { void FUN_00496710(int param_1, int param_2, int param_3, int param_4, int param_5); };
void C_FUN_00496710::FUN_00496710(int param_1, int param_2, int param_3, int param_4, int param_5)
{
  DAT_0083ff60 = 1;
  ((T_FUN_00496690 *)(this))->FUN_00496690(param_1, param_2, param_3, param_4, param_5);
  return;
}
}

namespace f_00496740 {
// MATCH: golf_clean.exe 0x00496740 ?FUN_00496740@C_FUN_00496740@f_00496740@@QAEXHHHHH@Z
extern int DAT_0083ff60;
struct T_FUN_004966d0 { int FUN_004966d0(int, int, int, int, int); };
struct C_FUN_00496740 { void FUN_00496740(int param_1, int param_2, int param_3, int param_4, int param_5); };
void C_FUN_00496740::FUN_00496740(int param_1, int param_2, int param_3, int param_4, int param_5)
{
  DAT_0083ff60 = 1;
  ((T_FUN_004966d0 *)(this))->FUN_004966d0(param_1, param_2, param_3, param_4, param_5);
  return;
}
}

namespace f_004967d0 {
// MATCH: golf_clean.exe 0x004967d0 ?FUN_004967d0@C_FUN_004967d0@f_004967d0@@QAEXI@Z
void __fastcall FUN_004979a0(int);
struct C_FUN_004967d0 { void FUN_004967d0(undefined4 param_1); };
void C_FUN_004967d0::FUN_004967d0(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x5c0) = param_1;
  FUN_004979a0((int)this);
  return;
}
}
