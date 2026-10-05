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

namespace f_100385d0 {
// MATCH: sound.dll 0x100385d0 ?FUN_100385d0@C_FUN_100385d0@f_100385d0@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_100385d0 { void FUN_100385d0(undefined4 param_1); };
void C_FUN_100385d0::FUN_100385d0(undefined4 param_1)
{
    undefined4 * puVar1;
  puVar1 = (unsigned int *)(operator_new(0xc));
  if (!(puVar1 == (undefined4 *)0x0)) {
    *puVar1 = 0;
    puVar1[1] = 0;
    puVar1[2] = param_1;
  } else {
    puVar1 = (undefined4 *)0x0;
  }
  if (*(int *)((int)this + 4) != 0) {
    *(undefined4 **)(*(int *)((int)this + 4) + 4) = puVar1;
    *puVar1 = *(undefined4 *)((int)this + 4);
    *(undefined4 **)((int)this + 4) = puVar1;
    *(int *)((int)this + 0xc) = *(int *)((int)this + 0xc) + 1;
    return;
  }
  *(undefined4 **)this = puVar1;
  *(undefined4 **)((int)this + 4) = puVar1;
  *(int *)((int)this + 0xc) = *(int *)((int)this + 0xc) + 1;
  return;
}
}

namespace f_100386e0 {
// MATCH: sound.dll 0x100386e0 ?FUN_100386e0@C_FUN_100386e0@f_100386e0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10038710(undefined4 *);
struct C_FUN_100386e0 { undefined4 * FUN_100386e0(byte param_1); };
undefined4 * C_FUN_100386e0::FUN_100386e0(byte param_1)
{
  thunk_FUN_10038710((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10038a00 {
// MATCH: sound.dll 0x10038a00 ?FUN_10038a00@f_10038a00@@YIIH@Z
int __cdecl FUN_1004249a(undefined *);
undefined4 __fastcall FUN_10038a00(int param_1)
{
    undefined * puVar1;
    undefined4 * puVar2;
  if ((*(byte *)(param_1 + 4) & 1) != 0) {
    waveInStop(*(HWAVEIN *)(param_1 + 0x44));
    waveInReset(*(HWAVEIN *)(param_1 + 0x44));
  }
  puVar1 = *(undefined **)(param_1 + 0x5c);
  if (puVar1 != (undefined *)0x0) {
    puVar2 = *(undefined4 **)(puVar1 + 4);
    if (!(puVar2 == (undefined4 *)0x0)) {
      *puVar2 = 0;
      *(undefined4 **)(param_1 + 0x5c) = puVar2;
    } else {
      *(undefined4 *)(param_1 + 0x60) = 0;
      *(undefined4 *)(param_1 + 0x5c) = 0;
    }
    puVar2 = *(undefined4 **)(puVar1 + 8);
    FUN_1004249a(puVar1);
    *(int *)(param_1 + 0x68) = *(int *)(param_1 + 0x68) + -1;
    while (puVar2 != (undefined4 *)0x0) {
      if ((undefined *)*puVar2 != (undefined *)0x0) {
        FUN_1004249a((undefined *)*puVar2);
      }
      *puVar2 = 0;
      FUN_1004249a((undefined *)puVar2);
      puVar1 = *(undefined **)(param_1 + 0x5c);
      if (puVar1 == (undefined *)0x0) {
        return 0;
      }
      puVar2 = *(undefined4 **)(puVar1 + 4);
      if (!(puVar2 == (undefined4 *)0x0)) {
        *puVar2 = 0;
        *(undefined4 **)(param_1 + 0x5c) = puVar2;
      } else {
        *(undefined4 *)(param_1 + 0x60) = 0;
        *(undefined4 *)(param_1 + 0x5c) = 0;
      }
      puVar2 = *(undefined4 **)(puVar1 + 8);
      FUN_1004249a(puVar1);
      *(int *)(param_1 + 0x68) = *(int *)(param_1 + 0x68) + -1;
    }
  }
  return 0;
}
}

namespace f_10038ae0 {
// MATCH: sound.dll 0x10038ae0 ?FUN_10038ae0@C_FUN_10038ae0@f_10038ae0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
struct C_FUN_10038ae0 { undefined4 * FUN_10038ae0(byte param_1); };
undefined4 * C_FUN_10038ae0::FUN_10038ae0(byte param_1)
{
  if (*(undefined **)this != (undefined *)0x0) {
    FUN_1004249a(*(undefined **)this);
  }
  *(undefined4 *)this = 0;
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10038b20 {
// MATCH: sound.dll 0x10038b20 ?FUN_10038b20@f_10038b20@@YIIH@Z
undefined4 __fastcall FUN_10038b20(int param_1)
{
  waveInStart(*(HWAVEIN *)(param_1 + 0x44));
  return 0;
}
}

namespace f_10038f10 {
// MATCH: sound.dll 0x10038f10 ?FUN_10038f10@f_10038f10@@YIIH@Z
undefined4 __fastcall FUN_10038f10(int param_1)
{
  waveInStop(*(HWAVEIN *)(param_1 + 0x44));
  waveInReset(*(HWAVEIN *)(param_1 + 0x44));
  return 0;
}
}

namespace f_100393d0 {
// MATCH: sound.dll 0x100393d0 ?FUN_100393d0@C_FUN_100393d0@f_100393d0@@QAEIPAUwavehdr_tag@@@Z
struct T_thunk_FUN_100316c0 { undefined4 thunk_FUN_100316c0(undefined4 *); };
struct C_FUN_100393d0 { undefined4 FUN_100393d0(LPWAVEHDR param_1); };
undefined4 C_FUN_100393d0::FUN_100393d0(LPWAVEHDR param_1)
{
  if (param_1 == (LPWAVEHDR)0x0) {
    return 10;
  }
  waveInUnprepareHeader(*(HWAVEIN *)((int)this + 0x44),param_1,0x20);
  param_1->dwFlags = 0;
  if (*(void **)((int)this + 0x6c) != (void *)0x0) {
    ((T_thunk_FUN_100316c0 *)(*(void **)((int)this + 0x6c)))->thunk_FUN_100316c0((unsigned int *)(&param_1->lpData));
  }
  waveInPrepareHeader(*(HWAVEIN *)((int)this + 0x44),param_1,0x20);
  waveInAddBuffer(*(HWAVEIN *)((int)this + 0x44),param_1,0x20);
  return 0;
}
}

namespace f_10039450 {
// MATCH: sound.dll 0x10039450 ?FUN_10039450@C_FUN_10039450@f_10039450@@QAEII@Z
struct C_FUN_10039450 { undefined4 FUN_10039450(undefined4 param_1); };
undefined4 C_FUN_10039450::FUN_10039450(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x6c) = param_1;
  return 0;
}
}

namespace f_10039470 {
// MATCH: sound.dll 0x10039470 ?FUN_10039470@f_10039470@@YIIH@Z
undefined4 __fastcall FUN_10039470(int param_1)
{
  *(undefined4 *)(param_1 + 0x6c) = 0;
  return 0;
}
}

namespace f_100394c0 {
// MATCH: sound.dll 0x100394c0 ?FUN_100394c0@f_100394c0@@YIXPAI@Z
int __cdecl FUN_1004249a(undefined *);
void __fastcall FUN_100394c0(undefined4 *param_1)
{
  if ((undefined *)*param_1 != (undefined *)0x0) {
    FUN_1004249a((undefined *)*param_1);
  }
  *param_1 = 0;
  return;
}
}

namespace f_10039760 {
// MATCH: sound.dll 0x10039760 ?FUN_10039760@C_FUN_10039760@f_10039760@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10039870(undefined4 *);
struct C_FUN_10039760 { undefined4 * FUN_10039760(byte param_1); };
undefined4 * C_FUN_10039760::FUN_10039760(byte param_1)
{
  thunk_FUN_10039870((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10039c10 {
// MATCH: sound.dll 0x10039c10 ?FUN_10039c10@f_10039c10@@YIIPAH@Z
undefined4 __fastcall thunk_FUN_100339c0(undefined4 *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(int); };
undefined4 __fastcall FUN_10039c10(int *param_1)
{
  ((VT_1 *)(param_1))->f16((int)(param_1[1]));
  thunk_FUN_100339c0((unsigned int *)(param_1 + 0x1c));
  return 0;
}
}

namespace f_10039c80 {
// MATCH: sound.dll 0x10039c80 ?FUN_10039c80@f_10039c80@@YIXH@Z
undefined4 __fastcall thunk_FUN_10034b90(int);
void __fastcall FUN_10039c80(int param_1)
{
  thunk_FUN_10034b90(param_1 + 0x70);
  return;
}
}

namespace f_1003a080 {
// MATCH: sound.dll 0x1003a080 ?FUN_1003a080@f_1003a080@@YIIH@Z
struct T_thunk_FUN_100377f0 { int thunk_FUN_100377f0(byte); };
undefined4 __fastcall FUN_1003a080(int param_1)
{
  if ((*(byte *)(param_1 + 200) & 2) == 0) {
    return 0x14;
  }
  ((T_thunk_FUN_100377f0 *)((void *)(param_1 + 0x70)))->thunk_FUN_100377f0(1);
  return 0;
}
}

namespace f_1003a5b0 {
// MATCH: sound.dll 0x1003a5b0 ?FUN_1003a5b0@C_FUN_1003a5b0@f_1003a5b0@@QAEXIII@Z
struct T_thunk_FUN_10037b00 { byte thunk_FUN_10037b00(undefined4, undefined4, undefined4); };
struct C_FUN_1003a5b0 { void FUN_1003a5b0(undefined4 param_1, undefined4 param_2, undefined4 param_3); };
void C_FUN_1003a5b0::FUN_1003a5b0(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  ((T_thunk_FUN_10037b00 *)((void *)((int)this + 0x70)))->thunk_FUN_10037b00(param_1, param_2, param_3);
  return;
}
}

namespace f_1003a710 {
// MATCH: sound.dll 0x1003a710 ?FUN_1003a710@f_1003a710@@YIIH@Z
undefined4 __fastcall FUN_1003a710(int param_1)
{
  return *(undefined4 *)(param_1 + 0x298);
}
}

namespace f_1003a730 {
// MATCH: sound.dll 0x1003a730 ?FUN_1003a730@f_1003a730@@YIXH@Z
undefined4 __fastcall thunk_FUN_10035770(int);
void __fastcall FUN_1003a730(int param_1)
{
  thunk_FUN_10035770(param_1 + 0x70);
  return;
}
}

namespace f_1003a800 {
// MATCH: sound.dll 0x1003a800 ?FUN_1003a800@f_1003a800@@YIIH@Z
undefined4 __fastcall FUN_1003a800(int param_1)
{
  if (((*(byte *)(param_1 + 0xcc) & 0x20) != 0) && ((*(byte *)(param_1 + 200) & 2) != 0)) {
    return 1;
  }
  return 0;
}
}

namespace f_10042b72 {
// MATCH: sound.dll 0x10042b72 ?FUN_10042b72@f_10042b72@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_10042b72()
{
  FUN_10047771(9);
  return;
}
}

namespace f_10042bd1 {
// MATCH: sound.dll 0x10042bd1 ?FUN_10042bd1@f_10042bd1@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_10042bd1()
{
  FUN_10047771(9);
  return;
}
}

namespace f_10042c71 {
// MATCH: sound.dll 0x10042c71 ?FUN_10042c71@f_10042c71@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_10042c71()
{
  FUN_10047771(9);
  return;
}
}

namespace f_10042cc9 {
// MATCH: sound.dll 0x10042cc9 ?FUN_10042cc9@f_10042cc9@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_10042cc9()
{
  FUN_10047771(9);
  return;
}
}

namespace f_100453dd {
// MATCH: sound.dll 0x100453dd ?FUN_100453dd@f_100453dd@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_100453dd()
{
  FUN_10047771(9);
  return;
}
}

namespace f_100455eb {
// MATCH: sound.dll 0x100455eb ?FUN_100455eb@f_100455eb@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_100455eb()
{
  FUN_10047771(9);
  return;
}
}

namespace f_10045666 {
// MATCH: sound.dll 0x10045666 ?FUN_10045666@f_10045666@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_10045666()
{
  FUN_10047771(9);
  return;
}
}

namespace f_1004588f {
// MATCH: sound.dll 0x1004588f ?FUN_1004588f@f_1004588f@@YAPAKXZ
DWORD * __stdcall __getptd();
DWORD * __cdecl FUN_1004588f()
{
  return (__getptd()) + 2;
}
}

namespace f_10045898 {
// MATCH: sound.dll 0x10045898 ?FUN_10045898@f_10045898@@YAPAKXZ
DWORD * __stdcall __getptd();
DWORD * __cdecl FUN_10045898()
{
  return (__getptd()) + 3;
}
}

namespace f_1004593a {
// MATCH: sound.dll 0x1004593a ?FUN_1004593a@f_1004593a@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_1004593a()
{
  FUN_10047771(9);
  return;
}
}

namespace f_100459c3 {
// MATCH: sound.dll 0x100459c3 ?FUN_100459c3@f_100459c3@@YAXXZ
int __cdecl FUN_10047771(int);
void __cdecl FUN_100459c3()
{
  FUN_10047771(9);
  return;
}
}

namespace f_1004a158 {
// MATCH: sound.dll 0x1004a158 ?FUN_1004a158@f_1004a158@@YAXXZ
void __cdecl __amsg_exit(int);
/* Library Function - Single Match __fptrap Library: Visual Studio 2003 Release */ void __cdecl FUN_1004a158()
{
  __amsg_exit(2);
  return;
}
}
