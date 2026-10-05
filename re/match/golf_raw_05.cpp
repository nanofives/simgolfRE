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

namespace f_00497c20 {
// MATCH: golf_clean.exe 0x00497c20 ?FUN_00497c20@C_FUN_00497c20@f_00497c20@@QAEII@Z
extern int DAT_008400b0;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); };
struct C_FUN_00497c20 { undefined4 FUN_00497c20(undefined4 param_1); };
undefined4 C_FUN_00497c20::FUN_00497c20(undefined4 param_1)
{
  if (DAT_008400b0 == 0) {
    return 7;
  }
  if ((*(uint *)((int)this + 0xe8) & 0x60000000) == 0) {
    *(undefined4 *)((int)this + 0x60) = 0;
    *(undefined4 *)((int)this + 0xe4) = param_1;
    ((VT_1 *)(((int)this + 100)))->f36();
    *(uint *)((int)this + 0xe8) = *(uint *)((int)this + 0xe8) | 0x20000000;
  }
  return 0;
}
}

namespace f_00497c70 {
// MATCH: golf_clean.exe 0x00497c70 ?FUN_00497c70@C_FUN_00497c70@f_00497c70@@QAEII@Z
extern int DAT_008400b0;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); };
struct C_FUN_00497c70 { undefined4 FUN_00497c70(undefined4 param_1); };
undefined4 C_FUN_00497c70::FUN_00497c70(undefined4 param_1)
{
  if (DAT_008400b0 == 0) {
    return 7;
  }
  if ((*(uint *)((int)this + 0xe8) & 0x60000000) == 0) {
    *(undefined4 *)((int)this + 0x60) = 1;
    *(undefined4 *)((int)this + 0xe4) = param_1;
    ((VT_1 *)(((int)this + 100)))->f36();
    *(uint *)((int)this + 0xe8) = *(uint *)((int)this + 0xe8) | 0x20000000;
  }
  return 0;
}
}

namespace f_0049bff0 {
// MATCH: golf_clean.exe 0x0049bff0 ?FUN_0049bff0@f_0049bff0@@YAXXZ
extern int DAT_008400c8;
undefined4 * __fastcall FUN_0049c020(undefined4 *);
void __cdecl FUN_0049bff0()
{
  FUN_0049c020((undefined4 *)((char *)&DAT_008400c8));
  return;
}
}

namespace f_0049c0a0 {
// MATCH: golf_clean.exe 0x0049c0a0 ?FUN_0049c0a0@f_0049c0a0@@YIXH@Z
void __fastcall FUN_0049c0a0(int param_1)
{
  *(undefined4 *)(param_1 + 0x594) = 0;
  return;
}
}

namespace f_0049c910 {
// MATCH: golf_clean.exe 0x0049c910 ?FUN_0049c910@f_0049c910@@YIXH@Z
undefined4 __fastcall FUN_0049cae0(int);
void __fastcall FUN_0049cb20(int);
undefined4 __fastcall FUN_0049ccc0(HPSTR);
void __fastcall FUN_0049c910(int param_1)
{
    int iVar1;
  if ((*(uint *)(param_1 + 0xb8) & 0x8000) == 0) {
    return;
  }
  if ((*(uint *)(param_1 + 0xb8) & 0x10) == 0) {
    iVar1 = FUN_0049ccc0((HPSTR)param_1);
    if (iVar1 != 0) {
      return;
    }
    iVar1 = FUN_0049cae0(param_1);
    if (iVar1 != 0) {
      return;
    }
  }
  FUN_0049cb20(param_1);
  return;
}
}

namespace f_0049d010 {
// MATCH: golf_clean.exe 0x0049d010 ?FUN_0049d010@f_0049d010@@YAXXZ
extern int DAT_008406e8;
undefined4 * __fastcall FUN_0049d050(undefined4 *);
void __cdecl FUN_0049d010()
{
  FUN_0049d050((undefined4 *)((char *)&DAT_008406e8));
  return;
}
}

namespace f_0049d050 {
// MATCH: golf_clean.exe 0x0049d050 ?FUN_0049d050@f_0049d050@@YIPAIPAI@Z
extern void *PTR_FUN_004bbe6c;
undefined4 * __fastcall FUN_004747a0(undefined4 *);
undefined4 * __fastcall FUN_0049d050(undefined4 *param_1)
{
  FUN_004747a0(param_1);
  *param_1 = (unsigned int)(&PTR_FUN_004bbe6c);
  param_1[6] = 0;
  return param_1;
}
}

namespace f_0049d0e0 {
// MATCH: golf_clean.exe 0x0049d0e0 ?FUN_0049d0e0@f_0049d0e0@@YIIH@Z
void __fastcall FUN_004747e0(int);
undefined4 __fastcall FUN_0049d0e0(int param_1)
{
  FUN_004747e0(param_1);
  *(undefined4 *)(param_1 + 0x18) = 0;
  return 0;
}
}

namespace f_0049d160 {
// MATCH: golf_clean.exe 0x0049d160 ?FUN_0049d160@f_0049d160@@YAXXZ
extern int DAT_00840710;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049d160()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00840710));
  return;
}
}

namespace f_0049d2a0 {
// MATCH: golf_clean.exe 0x0049d2a0 ?FUN_0049d2a0@f_0049d2a0@@YAXXZ
extern int DAT_008407c0;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049d2a0()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_008407c0));
  return;
}
}

namespace f_0049d300 {
// MATCH: golf_clean.exe 0x0049d300 ?FUN_0049d300@f_0049d300@@YAXXZ
extern int DAT_00840790;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049d300()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00840790));
  return;
}
}

namespace f_0049d360 {
// MATCH: golf_clean.exe 0x0049d360 ?FUN_0049d360@f_0049d360@@YAXXZ
extern int DAT_008407f0;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049d360()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_008407f0));
  return;
}
}

namespace f_0049d490 {
// MATCH: golf_clean.exe 0x0049d490 ?FUN_0049d490@f_0049d490@@YAXXZ
extern int DAT_00840830;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049d490()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00840830));
  return;
}
}

namespace f_0049d4f0 {
// MATCH: golf_clean.exe 0x0049d4f0 ?FUN_0049d4f0@f_0049d4f0@@YAXXZ
extern int DAT_00840890;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049d4f0()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00840890));
  return;
}
}

namespace f_0049d550 {
// MATCH: golf_clean.exe 0x0049d550 ?FUN_0049d550@f_0049d550@@YAXXZ
extern int DAT_00840860;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049d550()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00840860));
  return;
}
}

namespace f_0049eb00 {
// MATCH: golf_clean.exe 0x0049eb00 ?FUN_0049eb00@f_0049eb00@@YAXXZ
extern int DAT_008408c8;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049eb00()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_008408c8));
  return;
}
}

namespace f_0049eb60 {
// MATCH: golf_clean.exe 0x0049eb60 ?FUN_0049eb60@f_0049eb60@@YAXXZ
extern int DAT_008408f8;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0049eb60()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_008408f8));
  return;
}
}

namespace f_004a0130 {
// MATCH: golf_clean.exe 0x004a0130 ?FUN_004a0130@f_004a0130@@YAXXZ
extern int DAT_00840930;
extern char s_004e4a44[];
struct T_FUN_004a0180 { undefined4 * FUN_004a0180(undefined4); };
void __cdecl FUN_004a0130()
{
  ((T_FUN_004a0180 *)(((char *)&DAT_00840930)))->FUN_004a0180((unsigned int)(s_004e4a44));
  return;
}
}

namespace f_004a0180 {
// MATCH: golf_clean.exe 0x004a0180 ?FUN_004a0180@C_FUN_004a0180@f_004a0180@@QAEPAIPAD@Z
extern void *PTR_FUN_004bc034;
struct T_FUN_004a01d0 { undefined4 FUN_004a01d0(char *); };
struct C_FUN_004a0180 { undefined4 * FUN_004a0180(char *param_1); };
undefined4 * C_FUN_004a0180::FUN_004a0180(char *param_1)
{
  *(undefined ***)this = (unsigned char ** )(&PTR_FUN_004bc034);
  *(undefined4 *)((int)this + 4) = 0;
  ((T_FUN_004a01d0 *)(this))->FUN_004a01d0(param_1);
  return (unsigned int *)(this);
}
}

namespace f_004a0320 {
// MATCH: golf_clean.exe 0x004a0320 ?FUN_004a0320@f_004a0320@@YAXIHIII@Z
extern int DAT_00840930;
struct T_FUN_004a0280 { void FUN_004a0280(undefined4, int, undefined4, undefined4, undefined4); };
void __cdecl FUN_004a0320(undefined4 param_1, int param_2, undefined4 param_3, undefined4 param_4, undefined4 param_5)
{
  ((T_FUN_004a0280 *)(((char *)&DAT_00840930)))->FUN_004a0280(param_1, param_2, param_3, param_4, param_5);
  return;
}
}

namespace f_004a1370 {
// MATCH: golf_clean.exe 0x004a1370 ?FUN_004a1370@f_004a1370@@YIIH@Z
undefined4 __fastcall FUN_004a1370(int param_1)
{
  switch(*(undefined4 *)(param_1 + 500)) {
  case 1:
  case 2:
  case 4:
  case 0x10:
    return *(undefined4 *)(*(int *)(*(int *)(param_1 + 4) + 8) + 0xd4 + param_1);
  default:
    return 0;
  case 8:
    return *(undefined4 *)(param_1 + 0x118);
  }
}
}

namespace f_004a2980 {
// MATCH: golf_clean.exe 0x004a2980 ?FUN_004a2980@C_FUN_004a2980@f_004a2980@@QAEIH@Z
struct C_FUN_004a2980 { undefined4 FUN_004a2980(int param_1); };
undefined4 C_FUN_004a2980::FUN_004a2980(int param_1)
{
    int iVar1;
  iVar1 = *(int *)((int)this + param_1 * 4 + 4);
  if (iVar1 != 0) {
    return *(undefined4 *)(iVar1 + 0x574);
  }
  return 0;
}
}

namespace f_004a29a0 {
// MATCH: golf_clean.exe 0x004a29a0 ?FUN_004a29a0@C_FUN_004a29a0@f_004a29a0@@QAEXI@Z
struct C_FUN_004a29a0 { void FUN_004a29a0(undefined4 param_1); };
void C_FUN_004a29a0::FUN_004a29a0(undefined4 param_1)
{
    int iVar1;
    undefined4 * puVar2;
  puVar2 = (undefined4 *)((int)this + 0x54);
  for (iVar1 = 10; iVar1 != 0; iVar1 = iVar1 + -1) {
    *puVar2 = param_1;
    puVar2 = puVar2 + 1;
  }
  return;
}
}

namespace f_004a3790 {
// MATCH: golf_clean.exe 0x004a3790 ?FUN_004a3790@C_FUN_004a3790@f_004a3790@@QAEXI@Z
extern int DAT_004e449c;
extern int DAT_004e44a0;
extern int DAT_004e44a4;
extern int DAT_004e44a8;
extern int DAT_004e44ac;
extern int DAT_004e44b0;
extern int DAT_004e44b4;
extern int DAT_004e44b8;
extern int DAT_004e44bc;
extern int DAT_004e44c0;
extern int DAT_004e44c4;
extern int DAT_004e44c8;
extern int DAT_0083b628;
extern int DAT_0083b62c;
extern int DAT_0083b630;
struct T_FUN_004762d0 { undefined4 FUN_004762d0(int, undefined4, undefined4, undefined4); };
struct T_FUN_00476310 { void FUN_00476310(undefined4, undefined4, undefined4, undefined4); };
struct T_FUN_00476340 { void FUN_00476340(undefined4, undefined4, undefined4, undefined4); };
struct T_FUN_00476370 { void FUN_00476370(undefined4, undefined4, undefined4, undefined4); };
struct T_FUN_004896b0 { undefined4 FUN_004896b0(size_t); };
void __fastcall FUN_004a35e0(int *);
struct C_FUN_004a3790 { void FUN_004a3790(size_t param_1); };
void C_FUN_004a3790::FUN_004a3790(size_t param_1)
{
  FUN_004a35e0((int *)(this));
  ((T_FUN_00476310 *)((void *)(*(int *)(*(int *)this + 4) + 0x274 + (int)this)))->FUN_00476310(DAT_004e449c, DAT_004e44a8, DAT_004e44b4, DAT_004e44c0);
  ((T_FUN_00476340 *)((void *)(*(int *)(*(int *)this + 4) + 0x274 + (int)this)))->FUN_00476340(DAT_004e44a0, DAT_004e44ac, DAT_004e44b8, DAT_004e44c4);
  ((T_FUN_00476370 *)((void *)(*(int *)(*(int *)this + 4) + 0x274 + (int)this)))->FUN_00476370(DAT_004e44a4, DAT_004e44b0, DAT_004e44bc, DAT_004e44c8);
  ((T_FUN_004762d0 *)((void *)(*(int *)(*(int *)this + 4) + 0x274 + (int)this)))->FUN_004762d0(DAT_0083b628, DAT_0083b62c, DAT_0083b630, 0);
  ((T_FUN_004896b0 *)((void *)(*(int *)(*(int *)this + 8) + (int)this)))->FUN_004896b0(param_1);
  return;
}
}

namespace f_004a3860 {
// MATCH: golf_clean.exe 0x004a3860 ?FUN_004a3860@C_FUN_004a3860@f_004a3860@@QAEXI@Z
struct T_FUN_004897f0 { undefined4 FUN_004897f0(undefined4); };
void __fastcall FUN_004a35e0(int *);
struct C_FUN_004a3860 { void FUN_004a3860(undefined4 param_1); };
void C_FUN_004a3860::FUN_004a3860(undefined4 param_1)
{
  FUN_004a35e0((int *)(this));
  ((T_FUN_004897f0 *)((void *)(*(int *)(*(int *)this + 8) + (int)this)))->FUN_004897f0(param_1);
  return;
}
}

namespace f_004a4d30 {
// MATCH: golf_clean.exe 0x004a4d30 ?FUN_004a4d30@C_FUN_004a4d30@f_004a4d30@@QAEXI@Z
void __fastcall FUN_004a4d50(int);
struct C_FUN_004a4d30 { void FUN_004a4d30(undefined4 param_1); };
void C_FUN_004a4d30::FUN_004a4d30(undefined4 param_1)
{
  FUN_004a4d50((int)this);
  *(undefined4 *)((int)this + 0x10) = param_1;
  return;
}
}

namespace f_004a4ea0 {
// MATCH: golf_clean.exe 0x004a4ea0 ?FUN_004a4ea0@C_FUN_004a4ea0@f_004a4ea0@@QAEIPAI0@Z
struct C_FUN_004a4ea0 { undefined4 FUN_004a4ea0(undefined4 *param_1, undefined4 *param_2); };
undefined4 C_FUN_004a4ea0::FUN_004a4ea0(undefined4 *param_1, undefined4 *param_2)
{
  if (*(int *)((int)this + 4) == 0) {
    return 0;
  }
  if (param_2 != (undefined4 *)0x0) {
    *param_2 = *(undefined4 *)(*(int *)((int)this + 4) + 4);
  }
  if (param_1 != (undefined4 *)0x0) {
    *param_1 = **(undefined4 **)((int)this + 4);
  }
  return *(undefined4 *)(*(int *)((int)this + 4) + 8);
}
}

namespace f_004a910c {
// MATCH: golf_clean.exe 0x004a910c ?FUN_004a910c@f_004a910c@@YAPAKXZ
DWORD * __stdcall __getptd();
DWORD * __cdecl FUN_004a910c()
{
  return (__getptd()) + 2;
}
}

namespace f_004a9115 {
// MATCH: golf_clean.exe 0x004a9115 ?FUN_004a9115@f_004a9115@@YAPAKXZ
DWORD * __stdcall __getptd();
DWORD * __cdecl FUN_004a9115()
{
  return (__getptd()) + 3;
}
}

namespace f_004abe6b {
// MATCH: golf_clean.exe 0x004abe6b ?FUN_004abe6b@f_004abe6b@@YAXXZ
void __cdecl __amsg_exit(int);
void __cdecl FUN_004abe6b()
{
  __amsg_exit(2);
  return;
}
}
