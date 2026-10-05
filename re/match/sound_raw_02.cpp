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

namespace f_1000e8c0 {
// MATCH: sound.dll 0x1000e8c0 ?FUN_1000e8c0@C_FUN_1000e8c0@f_1000e8c0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1000e8f0(undefined4 *);
struct C_FUN_1000e8c0 { undefined4 * FUN_1000e8c0(byte param_1); };
undefined4 * C_FUN_1000e8c0::FUN_1000e8c0(byte param_1)
{
  thunk_FUN_1000e8f0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1000e8f0 {
// MATCH: sound.dll 0x1000e8f0 ?FUN_1000e8f0@f_1000e8f0@@YIXPAI@Z
extern void *PTR_LAB_1005b3d4;
int __fastcall thunk_FUN_1002a3b0(undefined4 *);
void __fastcall FUN_1000e8f0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b3d4);
  thunk_FUN_1002a3b0(param_1);
  return;
}
}

namespace f_1000ed00 {
// MATCH: sound.dll 0x1000ed00 ?FUN_1000ed00@C_FUN_1000ed00@f_1000ed00@@QAEXH@Z
struct C_FUN_1000ed00 { void FUN_1000ed00(int param_1); };
void C_FUN_1000ed00::FUN_1000ed00(int param_1)
{
  *(int *)((int)this + 0x3c) = param_1;
  *(int *)((int)this + 0x38) = *(int *)((int)this + 0x38) + param_1;
  return;
}
}

namespace f_1000eda0 {
// MATCH: sound.dll 0x1000eda0 ?FUN_1000eda0@f_1000eda0@@YAIXZ
extern int DAT_100b4a04;
undefined4 __fastcall thunk_FUN_10039470(int);
undefined4 __cdecl FUN_1000eda0()
{
  if (DAT_100b4a04 == 0) {
    return 0x13;
  }
  return (thunk_FUN_10039470(DAT_100b4a04));
}
}

namespace f_1000edf0 {
// MATCH: sound.dll 0x1000edf0 ?FUN_1000edf0@f_1000edf0@@YIXH@Z
void __fastcall FUN_1000edf0(int param_1)
{
  *(undefined4 *)(param_1 + 0x38) = 0;
  return;
}
}

namespace f_1000ee10 {
// MATCH: sound.dll 0x1000ee10 ?FUN_1000ee10@f_1000ee10@@YIXH@Z
void __fastcall FUN_1000ee10(int param_1)
{
  *(undefined4 *)(param_1 + 0x30) = 0;
  return;
}
}

namespace f_1000eef0 {
// MATCH: sound.dll 0x1000eef0 ?FUN_1000eef0@f_1000eef0@@YIPAIPAI@Z
undefined4 __fastcall thunk_FUN_1000f590(uint *);
uint * __fastcall FUN_1000eef0(uint *param_1)
{
  *param_1 = *param_1 & 0xfffffffe;
  *(byte *)(param_1 + 1) = (byte)param_1[1] | 1;
  param_1[0x17] = 0;
  param_1[0x11] = 0;
  thunk_FUN_1000f590(param_1);
  param_1[0x32] = 0;
  param_1[0x33] = 0;
  param_1[0x34] = 0;
  param_1[0x19] = 0;
  param_1[0x1a] = 0;
  param_1[0x13] = 0;
  param_1[0x12] = 0;
  param_1[0x14] = 0;
  param_1[0x15] = 0;
  param_1[0x16] = 0;
  return param_1;
}
}

namespace f_1000f0c0 {
// MATCH: sound.dll 0x1000f0c0 ?FUN_1000f0c0@C_FUN_1000f0c0@f_1000f0c0@@QAEII@Z
struct C_FUN_1000f0c0 { undefined4 FUN_1000f0c0(undefined4 param_1); };
undefined4 C_FUN_1000f0c0::FUN_1000f0c0(undefined4 param_1)
{
  if (*(int *)((int)this + 0x60) != 0) {
    return 0xc;
  }
  *(undefined4 *)((int)this + 0x78) = param_1;
  return 0;
}
}

namespace f_10012b80 {
// MATCH: sound.dll 0x10012b80 ?FUN_10012b80@C_FUN_10012b80@f_10012b80@@QAEPAEE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1000ef60(int);
struct C_FUN_10012b80 { undefined * FUN_10012b80(byte param_1); };
undefined * C_FUN_10012b80::FUN_10012b80(byte param_1)
{
  thunk_FUN_1000ef60((int)this);
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned char *)(this);
}
}

namespace f_10013560 {
// MATCH: sound.dll 0x10013560 ?FUN_10013560@C_FUN_10013560@f_10013560@@QAEXI@Z
extern int DAT_100b4a34;
struct T_thunk_FUN_1000f1c0 { int thunk_FUN_1000f1c0(uint); };
struct C_FUN_10013560 { void FUN_10013560(uint param_1); };
void C_FUN_10013560::FUN_10013560(uint param_1)
{
    void * this_00;
  this_00 = (void *)(DAT_100b4a34);
  *(uint *)((int)this + 0x188) = param_1;
  for (; this_00 != (void *)0x0; this_00 = *(void **)((int)this_00 + 0xc0)) {
    ((T_thunk_FUN_1000f1c0 *)(this_00))->thunk_FUN_1000f1c0(param_1);
  }
  return;
}
}

namespace f_100137f0 {
// MATCH: sound.dll 0x100137f0 ?FUN_100137f0@C_FUN_100137f0@f_100137f0@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_100137f0 { void FUN_100137f0(undefined4 param_1); };
void C_FUN_100137f0::FUN_100137f0(undefined4 param_1)
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

namespace f_1001bb10 {
// MATCH: sound.dll 0x1001bb10 ?FUN_1001bb10@f_1001bb10@@YIXH@Z
int __cdecl FUN_1004249a(undefined *);
struct VT_1 {  virtual int f0(int); };
void __fastcall FUN_1001bb10(int param_1)
{
    undefined * puVar1;
    undefined4 * puVar2;
  while( true ) {
    puVar1 = *(undefined **)(param_1 + 0xc);
    if (puVar1 == (undefined *)0x0) {
      return;
    }
    puVar2 = *(undefined4 **)(puVar1 + 4);
    if (!(puVar2 == (undefined4 *)0x0)) {
      *puVar2 = 0;
      *(undefined4 **)(param_1 + 0xc) = puVar2;
    } else {
      *(undefined4 *)(param_1 + 0x10) = 0;
      *(undefined4 *)(param_1 + 0xc) = 0;
    }
    puVar2 = *(undefined4 **)(puVar1 + 8);
    FUN_1004249a(puVar1);
    *(int *)(param_1 + 0x18) = *(int *)(param_1 + 0x18) + -1;
    if (puVar2 == (undefined4 *)0x0) break;
    ((VT_1 *)(puVar2))->f0((int)(1));
  }
  return;
}
}

namespace f_1001bb80 {
// MATCH: sound.dll 0x1001bb80 ?FUN_1001bb80@f_1001bb80@@YIIH@Z
undefined4 __fastcall FUN_1001bb80(int param_1)
{
    int iVar1;
  iVar1 = *(int *)(param_1 + 0xc);
  *(int *)(param_1 + 0x14) = iVar1;
  if (iVar1 != 0) {
    return *(undefined4 *)(iVar1 + 8);
  }
  return 0;
}
}

namespace f_1001bbb0 {
// MATCH: sound.dll 0x1001bbb0 ?FUN_1001bbb0@f_1001bbb0@@YIXH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
void __fastcall FUN_1001bbb0(int param_1)
{
    int iVar1;
    int * piVar2;
  iVar1 = *(int *)(param_1 + 0xc);
  *(int *)(param_1 + 0x14) = iVar1;
  if ((iVar1 != 0) && (piVar2 = *(int **)(iVar1 + 8), piVar2 != (int *)0x0)) {
    while( true ) {
      ((VT_1 *)(piVar2))->f5();
      if (*(int *)(param_1 + 0x14) == 0) break;
      iVar1 = *(int *)(*(int *)(param_1 + 0x14) + 4);
      *(int *)(param_1 + 0x14) = iVar1;
      if ((iVar1 == 0) || (piVar2 = *(int **)(iVar1 + 8), piVar2 == (int *)0x0)) break;
    }
  }
  *(undefined4 *)(param_1 + 4) = 0x18;
  return;
}
}

namespace f_1001bea0 {
// MATCH: sound.dll 0x1001bea0 ?FUN_1001bea0@C_FUN_1001bea0@f_1001bea0@@QAEHE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001b570(uint);
struct C_FUN_1001bea0 { int FUN_1001bea0(byte param_1); };
int C_FUN_1001bea0::FUN_1001bea0(byte param_1)
{
  thunk_FUN_1001b570((uint)this);
  if ((param_1 & 1) != 0) {
    FUN_1004249a((undefined *)((int)this + -0x2c));
  }
  return (int)this + -0x2c;
}
}

namespace f_1001c060 {
// MATCH: sound.dll 0x1001c060 ?FUN_1001c060@f_1001c060@@YIXPAI@Z
extern void *PTR_LAB_1005b19c;
void __fastcall FUN_1001c060(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b19c);
  return;
}
}

namespace f_1001c2a0 {
// MATCH: sound.dll 0x1001c2a0 ?FUN_1001c2a0@f_1001c2a0@@YIXH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
void __fastcall FUN_1001c2a0(int param_1)
{
    int iVar1;
    int * piVar2;
  iVar1 = *(int *)(param_1 + -0x20);
  *(int *)(param_1 + -0x18) = iVar1;
  if ((iVar1 != 0) && (piVar2 = *(int **)(iVar1 + 8), piVar2 != (int *)0x0)) {
    while( true ) {
      ((VT_1 *)(piVar2))->f5();
      if (*(int *)(param_1 + -0x18) == 0) break;
      iVar1 = *(int *)(*(int *)(param_1 + -0x18) + 4);
      *(int *)(param_1 + -0x18) = iVar1;
      if ((iVar1 == 0) || (piVar2 = *(int **)(iVar1 + 8), piVar2 == (int *)0x0)) break;
    }
  }
  *(undefined4 *)(param_1 + -0x28) = 0x18;
  return;
}
}

namespace f_1001c340 {
// MATCH: sound.dll 0x1001c340 ?FUN_1001c340@C_FUN_1001c340@f_1001c340@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(int); };
struct C_FUN_1001c340 { void FUN_1001c340(undefined4 param_1); };
void C_FUN_1001c340::FUN_1001c340(undefined4 param_1)
{
  ((VT_1 *)(((int)this + -0x2c)))->f1((int)(param_1));
  return;
}
}

namespace f_1001c530 {
// MATCH: sound.dll 0x1001c530 ?FUN_1001c530@C_FUN_1001c530@f_1001c530@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001c560(undefined4 *);
struct C_FUN_1001c530 { undefined4 * FUN_1001c530(byte param_1); };
undefined4 * C_FUN_1001c530::FUN_1001c530(byte param_1)
{
  thunk_FUN_1001c560((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1001c560 {
// MATCH: sound.dll 0x1001c560 ?FUN_1001c560@f_1001c560@@YIXPAI@Z
extern void *PTR_LAB_1005b19c;
void __fastcall FUN_1001c560(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b19c);
  return;
}
}

namespace f_1001c6e0 {
// MATCH: sound.dll 0x1001c6e0 ?FUN_1001c6e0@C_FUN_1001c6e0@f_1001c6e0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001c710(undefined4 *);
struct C_FUN_1001c6e0 { undefined4 * FUN_1001c6e0(byte param_1); };
undefined4 * C_FUN_1001c6e0::FUN_1001c6e0(byte param_1)
{
  thunk_FUN_1001c710((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1001c710 {
// MATCH: sound.dll 0x1001c710 ?FUN_1001c710@f_1001c710@@YIXPAI@Z
extern void *PTR_LAB_1005b19c;
void __fastcall FUN_1001c710(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b19c);
  return;
}
}

namespace f_1001c730 {
// MATCH: sound.dll 0x1001c730 ?FUN_1001c730@C_FUN_1001c730@f_1001c730@@QAEXI@Z
int __cdecl FUN_1004249a(undefined *);
void * __cdecl operator_new(uint);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
struct C_FUN_1001c730 { void FUN_1001c730(uint param_1); };
void C_FUN_1001c730::FUN_1001c730(uint param_1)
{
  *(uint *)((int)this + 0x3c) = param_1;
  if (*(undefined **)((int)this + 0x2c) != (undefined *)0x0) {
    FUN_1004249a(*(undefined **)((int)this + 0x2c));
    *(undefined4 *)((int)this + 0x2c) = 0;
  }
  *(void **)((int)this + 0x2c) = (operator_new(param_1));
  ((VT_1 *)(this))->f5();
  *(undefined4 *)((int)this + 0x30) = 0;
  return;
}
}

namespace f_1001c950 {
// MATCH: sound.dll 0x1001c950 ?FUN_1001c950@C_FUN_1001c950@f_1001c950@@QAEHE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001c990(uint);
struct C_FUN_1001c950 { int FUN_1001c950(byte param_1); };
int C_FUN_1001c950::FUN_1001c950(byte param_1)
{
  thunk_FUN_1001c990((uint)this);
  if ((param_1 & 1) != 0) {
    FUN_1004249a((undefined *)((int)this + -0x2c));
  }
  return (int)this + -0x2c;
}
}

namespace f_1001cb60 {
// MATCH: sound.dll 0x1001cb60 ?FUN_1001cb60@C_FUN_1001cb60@f_1001cb60@@QAEXI@Z
struct C_FUN_1001cb60 { void FUN_1001cb60(undefined4 param_1); };
void C_FUN_1001cb60::FUN_1001cb60(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x1c) = param_1;
  return;
}
}

namespace f_1001cb80 {
// MATCH: sound.dll 0x1001cb80 ?FUN_1001cb80@f_1001cb80@@YIXH@Z
void __fastcall FUN_1001cb80(int param_1)
{
  *(undefined4 *)(param_1 + 8) = 0x38;
  return;
}
}

namespace f_1001cba0 {
// MATCH: sound.dll 0x1001cba0 ?FUN_1001cba0@C_FUN_1001cba0@f_1001cba0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001cbd0(undefined4 *);
struct C_FUN_1001cba0 { undefined4 * FUN_1001cba0(byte param_1); };
undefined4 * C_FUN_1001cba0::FUN_1001cba0(byte param_1)
{
  thunk_FUN_1001cbd0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1001cbd0 {
// MATCH: sound.dll 0x1001cbd0 ?FUN_1001cbd0@f_1001cbd0@@YIXPAI@Z
extern void *PTR_LAB_1005b19c;
void __fastcall FUN_1001cbd0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b19c);
  return;
}
}

namespace f_1001cc60 {
// MATCH: sound.dll 0x1001cc60 ?FUN_1001cc60@C_FUN_1001cc60@f_1001cc60@@QAEXI@Z
struct C_FUN_1001cc60 { void FUN_1001cc60(undefined4 param_1); };
void C_FUN_1001cc60::FUN_1001cc60(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x1c) = param_1;
  return;
}
}

namespace f_1001cc80 {
// MATCH: sound.dll 0x1001cc80 ?FUN_1001cc80@f_1001cc80@@YIXH@Z
void __fastcall FUN_1001cc80(int param_1)
{
  *(undefined4 *)(param_1 + 8) = 0x34;
  return;
}
}

namespace f_1001cca0 {
// MATCH: sound.dll 0x1001cca0 ?FUN_1001cca0@C_FUN_1001cca0@f_1001cca0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001ccd0(undefined4 *);
struct C_FUN_1001cca0 { undefined4 * FUN_1001cca0(byte param_1); };
undefined4 * C_FUN_1001cca0::FUN_1001cca0(byte param_1)
{
  thunk_FUN_1001ccd0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1001ccd0 {
// MATCH: sound.dll 0x1001ccd0 ?FUN_1001ccd0@f_1001ccd0@@YIXPAI@Z
extern void *PTR_LAB_1005b19c;
void __fastcall FUN_1001ccd0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b19c);
  return;
}
}

namespace f_1001cd30 {
// MATCH: sound.dll 0x1001cd30 ?FUN_1001cd30@f_1001cd30@@YIXH@Z
int __cdecl FUN_1004249a(undefined *);
void __fastcall FUN_1001cd30(int param_1)
{
  if (*(undefined **)(param_1 + 0x1c) != (undefined *)0x0) {
    FUN_1004249a(*(undefined **)(param_1 + 0x1c));
  }
  if (*(undefined **)(param_1 + 0x24) != (undefined *)0x0) {
    FUN_1004249a(*(undefined **)(param_1 + 0x24));
  }
  return;
}
}

namespace f_1001d150 {
// MATCH: sound.dll 0x1001d150 ?FUN_1001d150@C_FUN_1001d150@f_1001d150@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_1001d150 { void FUN_1001d150(undefined4 param_1); };
void C_FUN_1001d150::FUN_1001d150(undefined4 param_1)
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

namespace f_1001d1d0 {
// MATCH: sound.dll 0x1001d1d0 ?FUN_1001d1d0@C_FUN_1001d1d0@f_1001d1d0@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_1001d1d0 { void FUN_1001d1d0(undefined4 param_1); };
void C_FUN_1001d1d0::FUN_1001d1d0(undefined4 param_1)
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
  if (*(undefined4 **)this != (undefined4 *)0x0) {
    **(undefined4 **)this = (unsigned int)(puVar1);
    puVar1[1] = *(undefined4 *)this;
    *(undefined4 **)this = puVar1;
    *(int *)((int)this + 0xc) = *(int *)((int)this + 0xc) + 1;
    return;
  }
  *(undefined4 **)this = puVar1;
  *(undefined4 **)((int)this + 4) = puVar1;
  *(int *)((int)this + 0xc) = *(int *)((int)this + 0xc) + 1;
  return;
}
}

namespace f_1001e1c0 {
// MATCH: sound.dll 0x1001e1c0 ?FUN_1001e1c0@f_1001e1c0@@YIIH@Z
int __fastcall thunk_FUN_1001e8e0(int);
undefined4 __fastcall FUN_1001e1c0(int param_1)
{
    int iVar1;
  iVar1 = *(int *)(param_1 + 0x19c);
  *(int *)(param_1 + 0x1a4) = iVar1;
  if ((iVar1 != 0) && (iVar1 = *(int *)(iVar1 + 8), iVar1 != 0)) {
    while( true ) {
      thunk_FUN_1001e8e0(iVar1);
      if (*(int *)(param_1 + 0x1a4) == 0) break;
      iVar1 = *(int *)(*(int *)(param_1 + 0x1a4) + 4);
      *(int *)(param_1 + 0x1a4) = iVar1;
      if ((iVar1 == 0) || (iVar1 = *(int *)(iVar1 + 8), iVar1 == 0)) break;
    }
  }
  *(uint *)(param_1 + 0x4c) = *(uint *)(param_1 + 0x4c) & 0xffffff63;
  return 0;
}
}

namespace f_1001e3d0 {
// MATCH: sound.dll 0x1001e3d0 ?FUN_1001e3d0@f_1001e3d0@@YIIPAI@Z
extern int DAT_100b4a08;
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001e7d0(undefined4 *);
undefined4 __fastcall FUN_1001e3d0(undefined4 *param_1)
{
    undefined * puVar1;
    undefined4 * puVar2;
  if (((*(byte *)(param_1 + 0x13) & 1) != 0) && ((HMMIO)*param_1 != (HMMIO)0x0)) {
    mmioClose((HMMIO)*param_1,0);
    DAT_100b4a08 = DAT_100b4a08 + -1;
  }
  puVar1 = (undefined *)param_1[0x67];
  if (puVar1 != (undefined *)0x0) {
    puVar2 = *(undefined4 **)(puVar1 + 4);
    if (!(puVar2 == (undefined4 *)0x0)) {
      *puVar2 = 0;
      param_1[0x67] = (unsigned int)(puVar2);
    } else {
      param_1[0x68] = 0;
      param_1[0x67] = 0;
    }
    puVar2 = *(undefined4 **)(puVar1 + 8);
    FUN_1004249a(puVar1);
    param_1[0x6a] = param_1[0x6a] + -1;
    while (puVar2 != (undefined4 *)0x0) {
      if ((puVar2[0xf] != 0) && ((*(byte *)(puVar2 + 0xd) & 4) == 0)) {
        puVar2[0xf] = 0;
        puVar2[0xb] = 0;
        puVar2[0x10] = 0;
      }
      if (puVar2 != (undefined4 *)0x0) {
        thunk_FUN_1001e7d0(puVar2);
        FUN_1004249a((undefined *)puVar2);
      }
      puVar1 = (undefined *)param_1[0x67];
      if (puVar1 == (undefined *)0x0) break;
      puVar2 = *(undefined4 **)(puVar1 + 4);
      if (!(puVar2 == (undefined4 *)0x0)) {
        *puVar2 = 0;
        param_1[0x67] = (unsigned int)(puVar2);
      } else {
        param_1[0x68] = 0;
        param_1[0x67] = 0;
      }
      puVar2 = *(undefined4 **)(puVar1 + 8);
      FUN_1004249a(puVar1);
      param_1[0x6a] = param_1[0x6a] + -1;
    }
  }
  *param_1 = 0;
  param_1[0x59] = 0;
  param_1[0x5b] = 0;
  param_1[0x13] = param_1[0x13] & 0xfffffffe;
  return 0;
}
}

namespace f_1001e530 {
// MATCH: sound.dll 0x1001e530 ?FUN_1001e530@f_1001e530@@YIIH@Z
undefined4 __fastcall FUN_1001e530(int param_1)
{
  *(uint *)(param_1 + 0x4c) = *(uint *)(param_1 + 0x4c) & 0xfffffff7;
  return 0;
}
}

namespace f_1001e680 {
// MATCH: sound.dll 0x1001e680 ?FUN_1001e680@C_FUN_1001e680@f_1001e680@@QAEXI@Z
struct C_FUN_1001e680 { void FUN_1001e680(uint param_1); };
void C_FUN_1001e680::FUN_1001e680(uint param_1)
{
  *(uint *)((int)this + 0x4c) = (param_1 & 1) << 1 | *(uint *)((int)this + 0x4c) & 0xfffffffd;
  return;
}
}

namespace f_1001e9a0 {
// MATCH: sound.dll 0x1001e9a0 ?FUN_1001e9a0@C_FUN_1001e9a0@f_1001e9a0@@QAEXH@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001ef80(int);
struct C_FUN_1001e9a0 { void FUN_1001e9a0(int param_1); };
void C_FUN_1001e9a0::FUN_1001e9a0(int param_1)
{
  if ((*(undefined **)((int)this + 0x3c) != (undefined *)0x0) && ((*(byte *)((int)this + 0x34) & 4) != 0)) {
    FUN_1004249a(*(undefined **)((int)this + 0x3c));
    *(undefined4 *)((int)this + 0x3c) = 0;
  }
  *(int *)((int)this + 0x3c) = param_1;
  *(int *)((int)this + 0x2c) = param_1;
  if (param_1 != 0) {
    thunk_FUN_1001ef80((int)this);
  }
  *(undefined4 *)((int)this + 0x40) = 0;
  return;
}
}
