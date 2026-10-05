// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (raw form: Ghidra's offsets,
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

namespace f_10005330 {
// MATCH: jgld.dll 0x10005330 ?FUN_10005330@f_10005330@@YIII@Z
undefined4 __fastcall FUN_10005330(undefined4 param_1)
{
  return param_1;
}
}

namespace f_10006270 {
// MATCH: jgld.dll 0x10006270 ?FUN_10006270@C_FUN_10006270@f_10006270@@QAEPAXM@Z
struct T_thunk_FUN_10003f10 { int thunk_FUN_10003f10(float); };
struct C_FUN_10006270 { void * FUN_10006270(float param_1); };
void * C_FUN_10006270::FUN_10006270(float param_1)
{
  if ((*(uint *)((int)this + 0x20) & 2) != 0) {
    ((T_thunk_FUN_10003f10 *)((void *)((int)this + 0x14)))->thunk_FUN_10003f10(param_1);
  }
  return this;
}
}

namespace f_100062e0 {
// MATCH: jgld.dll 0x100062e0 ?FUN_100062e0@f_100062e0@@YIXH@Z
int __fastcall thunk_FUN_10003df0(float *);
int __fastcall thunk_FUN_10004530(float *);
void __fastcall FUN_100062e0(int param_1)
{
  thunk_FUN_10003df0((float *)(param_1 + 0x14));
  thunk_FUN_10004530((float *)(param_1 + 4));
  return;
}
}

namespace f_10006790 {
// MATCH: jgld.dll 0x10006790 ?FUN_10006790@C_FUN_10006790@f_10006790@@QAEHH@Z
struct C_FUN_10006790 { int FUN_10006790(int param_1); };
int C_FUN_10006790::FUN_10006790(int param_1)
{
    uint local_c;
    uint local_10;
  for (local_c = 0; local_c < 0x10; local_c = local_c + 1) {
    *(undefined4 *)((int)this + local_c * 4 + 4) = *(undefined4 *)(param_1 + 4 + local_c * 4);
  }
  for (local_10 = 0; local_10 < 0x10; local_10 = local_10 + 1) {
    *(undefined4 *)((int)this + local_10 * 4 + 4) = *(undefined4 *)(param_1 + 4 + local_10 * 4);
  }
  return (int)this;
}
}

namespace f_10008360 {
// MATCH: jgld.dll 0x10008360 ?FUN_10008360@f_10008360@@YAXPAHHHHH@Z
void __cdecl FUN_10008360(int *param_1, int param_2, int param_3, int param_4, int param_5)
{
  *param_1 = param_2;
  param_1[1] = param_3;
  param_1[2] = param_2 + param_4;
  param_1[3] = param_3 + param_5;
  return;
}
}

namespace f_10008640 {
// MATCH: jgld.dll 0x10008640 ?FUN_10008640@f_10008640@@YIIH@Z
undefined4 __fastcall FUN_10008640(int param_1)
{
  *(undefined4 *)(param_1 + 0x4b8) = *(undefined4 *)(param_1 + 0x4bc);
  *(int *)(param_1 + 0x4c4) = *(int *)(param_1 + 0x4c4) + 1;
  return *(undefined4 *)(param_1 + 0x4b8);
}
}

namespace f_100086b0 {
// MATCH: jgld.dll 0x100086b0 ?FUN_100086b0@C_FUN_100086b0@f_100086b0@@QAEXH@Z
struct C_FUN_100086b0 { void FUN_100086b0(int param_1); };
void C_FUN_100086b0::FUN_100086b0(int param_1)
{
  *(int *)((int)this + 0x4c4) = *(int *)((int)this + 0x4c4) - param_1;
  if (*(int *)((int)this + 0x4c4) <= 0) {
    *(undefined4 *)((int)this + 0x4b8) = 0;
    *(undefined4 *)((int)this + 0x4c4) = 0;
  }
  return;
}
}

namespace f_10008730 {
// MATCH: jgld.dll 0x10008730 ?FUN_10008730@f_10008730@@YIIH@Z
undefined4 __fastcall FUN_10008730(int param_1)
{
  *(undefined4 *)(param_1 + 0x4cc) = *(undefined4 *)(param_1 + 0x4c0);
  if (*(int *)(param_1 + 0x4cc) != 0) {
    *(int *)(param_1 + 0x4c8) = *(int *)(param_1 + 0x4c8) + 1;
  }
  return *(undefined4 *)(param_1 + 0x4cc);
}
}

namespace f_100087b0 {
// MATCH: jgld.dll 0x100087b0 ?FUN_100087b0@C_FUN_100087b0@f_100087b0@@QAEXH@Z
struct C_FUN_100087b0 { void FUN_100087b0(int param_1); };
void C_FUN_100087b0::FUN_100087b0(int param_1)
{
  *(int *)((int)this + 0x4c8) = *(int *)((int)this + 0x4c8) - param_1;
  if (*(int *)((int)this + 0x4c8) <= 0) {
    *(undefined4 *)((int)this + 0x4cc) = 0;
    *(undefined4 *)((int)this + 0x4c8) = 0;
  }
  return;
}
}

namespace f_10008ae0 {
// MATCH: jgld.dll 0x10008ae0 ?FUN_10008ae0@C_FUN_10008ae0@f_10008ae0@@QAEHPAH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(int); };
struct C_FUN_10008ae0 { int FUN_10008ae0(int *param_1); };
int C_FUN_10008ae0::FUN_10008ae0(int *param_1)
{
    int local_c;
    HDC local_10;
  local_10 = (HDC)((VT_1 *)(this))->f10();
  local_c = *param_1;
  SelectObject(local_10,*(HGDIOBJ *)(local_c + 0x18));
  ((VT_2 *)(((int *)this)))->f11((int)(1));
  return 0;
}
}

namespace f_10008b90 {
// MATCH: jgld.dll 0x10008b90 ?FUN_10008b90@f_10008b90@@YIHPAH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(int); };
int __fastcall FUN_10008b90(int *param_1)
{
    HDC local_c;
  local_c = (HDC)((VT_1 *)(param_1))->f10();
  SelectObject(local_c,((HGDIOBJ)GetStockObject(0xd)));
  ((VT_2 *)(param_1))->f11((int)(1));
  return 0;
}
}

namespace f_10008c40 {
// MATCH: jgld.dll 0x10008c40 ?FUN_10008c40@C_FUN_10008c40@f_10008c40@@QAEHIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(int); };
struct C_FUN_10008c40 { int FUN_10008c40(uint param_1, uint param_2, uint param_3); };
int C_FUN_10008c40::FUN_10008c40(uint param_1, uint param_2, uint param_3)
{
    HDC local_c;
  local_c = (HDC)((VT_1 *)(this))->f10();
  SetTextColor(local_c,param_1 & 0xff | (param_2 & 0xff) << 8 | (param_3 & 0xff) << 0x10);
  ((VT_2 *)(((int *)this)))->f11((int)(1));
  return 0;
}
}

namespace f_10008d00 {
// MATCH: jgld.dll 0x10008d00 ?FUN_10008d00@C_FUN_10008d00@f_10008d00@@QAEHI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(int); };
struct C_FUN_10008d00 { int FUN_10008d00(uint param_1); };
int C_FUN_10008d00::FUN_10008d00(uint param_1)
{
    HDC local_c;
  local_c = (HDC)((VT_1 *)(this))->f10();
  SetTextColor(local_c,param_1 & 0xffff | 0x10ff0000);
  ((VT_2 *)(((int *)this)))->f11((int)(1));
  return 0;
}
}

namespace f_10008da0 {
// MATCH: jgld.dll 0x10008da0 ?FUN_10008da0@C_FUN_10008da0@f_10008da0@@QAEHHHPBDH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(int); };
struct C_FUN_10008da0 { int FUN_10008da0(int param_1, int param_2, LPCSTR param_3, int param_4); };
int C_FUN_10008da0::FUN_10008da0(int param_1, int param_2, LPCSTR param_3, int param_4)
{
    HDC local_c;
  local_c = (HDC)((VT_1 *)(this))->f10();
  TextOutA(local_c,param_1,param_2,param_3,param_4);
  ((VT_2 *)(((int *)this)))->f11((int)(1));
  return 0;
}
}

namespace f_10009120 {
// MATCH: jgld.dll 0x10009120 ?FUN_10009120@f_10009120@@YAHPAH@Z
int __cdecl FUN_10009120(int *param_1)
{
  return param_1[2] - *param_1;
}
}

namespace f_10009160 {
// MATCH: jgld.dll 0x10009160 ?FUN_10009160@f_10009160@@YAHH@Z
int __cdecl FUN_10009160(int param_1)
{
  return *(int *)(param_1 + 0xc) - *(int *)(param_1 + 4);
}
}

namespace f_100092e0 {
// MATCH: jgld.dll 0x100092e0 ?FUN_100092e0@f_100092e0@@YIHH@Z
int __fastcall FUN_100092e0(int param_1)
{
  *(undefined4 *)(param_1 + 4) = 0;
  return param_1;
}
}

namespace f_10009690 {
// MATCH: jgld.dll 0x10009690 ?FUN_10009690@f_10009690@@YIPAXPAI@Z
extern void *PTR_LAB_1011d220;
int __fastcall thunk_FUN_10006ab0(undefined4 *);
void * __fastcall FUN_10009690(undefined4 *param_1)
{
  thunk_FUN_10006ab0(param_1);
  *param_1 = (unsigned int)(&PTR_LAB_1011d220);
  return param_1;
}
}

namespace f_100096f0 {
// MATCH: jgld.dll 0x100096f0 ?FUN_100096f0@f_100096f0@@YIXPAI@Z
extern void *PTR_LAB_1011d220;
int __fastcall thunk_FUN_10006b80(undefined4 *);
void __fastcall FUN_100096f0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011d220);
  thunk_FUN_10006b80(param_1);
  return;
}
}

namespace f_1000a4e0 {
// MATCH: jgld.dll 0x1000a4e0 ?FUN_1000a4e0@C_FUN_1000a4e0@f_1000a4e0@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_100096f0(undefined4 *);
struct C_FUN_1000a4e0 { void * FUN_1000a4e0(uint param_1); };
void * C_FUN_1000a4e0::FUN_1000a4e0(uint param_1)
{
  thunk_FUN_100096f0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_1000a5d0 {
// MATCH: jgld.dll 0x1000a5d0 ?FUN_1000a5d0@C_FUN_1000a5d0@f_1000a5d0@@QAEII@Z
struct C_FUN_1000a5d0 { undefined4 FUN_1000a5d0(undefined4 param_1); };
undefined4 C_FUN_1000a5d0::FUN_1000a5d0(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x4d0) = param_1;
  return 0;
}
}

namespace f_1000a620 {
// MATCH: jgld.dll 0x1000a620 ?FUN_1000a620@f_1000a620@@YIIH@Z
undefined4 __fastcall FUN_1000a620(int param_1)
{
  return *(undefined4 *)(param_1 + 0x4d0);
}
}

namespace f_1000a750 {
// MATCH: jgld.dll 0x1000a750 ?FUN_1000a750@f_1000a750@@YIXPAH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); };
void __fastcall FUN_1000a750(int *param_1)
{
  ((VT_1 *)(param_1))->f4();
  return;
}
}

namespace f_1000a7b0 {
// MATCH: jgld.dll 0x1000a7b0 ?FUN_1000a7b0@f_1000a7b0@@YIXPAH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); };
void __fastcall FUN_1000a7b0(int *param_1)
{
  ((VT_1 *)(param_1))->f4();
  return;
}
}

namespace f_1000a990 {
// MATCH: jgld.dll 0x1000a990 ?FUN_1000a990@f_1000a990@@YIHH@Z
int __fastcall FUN_1000a990(int param_1)
{
  return param_1 + 0x44;
}
}

namespace f_1000aa30 {
// MATCH: jgld.dll 0x1000aa30 ?FUN_1000aa30@f_1000aa30@@YIHH@Z
int __fastcall FUN_1000aa30(int param_1)
{
  return param_1 + 0x54;
}
}
