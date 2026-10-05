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

namespace f_1001ef10 {
// MATCH: sound.dll 0x1001ef10 ?FUN_1001ef10@f_1001ef10@@YIXH@Z
extern int DAT_100b4a1c;
void __fastcall FUN_1001ef10(int param_1)
{
    DWORD * pDVar1;
    int iVar2;
  if (*(int *)(DAT_100b4a1c + 0x3c) != 0) {
    pDVar1 = (DWORD *)(param_1 + 600);
    iVar2 = 0x80;
    do {
      if ((char)*pDVar1 != '\0') {
        *(char *)((int)pDVar1 + 2) = '\0';
        midiOutShortMsg(*(HMIDIOUT *)(DAT_100b4a1c + 0x3c),*pDVar1);
        *pDVar1 = 0;
      }
      pDVar1 = pDVar1 + 1;
      iVar2 = iVar2 + -1;
    } while (iVar2 != 0);
  }
  return;
}
}

namespace f_1001f150 {
// MATCH: sound.dll 0x1001f150 ?FUN_1001f150@f_1001f150@@YIXH@Z
void __fastcall FUN_1001f150(int param_1)
{
    int iVar1;
  iVar1 = *(int *)(param_1 + 0x19c);
  *(int *)(param_1 + 0x1a4) = iVar1;
  if ((iVar1 != 0) && (iVar1 = *(int *)(iVar1 + 8), iVar1 != 0)) {
    while( true ) {
      *(byte *)(iVar1 + 0x38) = *(byte *)(iVar1 + 0x38) & 0xf7;
      if (*(int *)(param_1 + 0x1a4) == 0) break;
      iVar1 = *(int *)(*(int *)(param_1 + 0x1a4) + 4);
      *(int *)(param_1 + 0x1a4) = iVar1;
      if (iVar1 == 0) {
        return;
      }
      iVar1 = *(int *)(iVar1 + 8);
      if (iVar1 == 0) {
        return;
      }
    }
  }
  return;
}
}

namespace f_1001f6c0 {
// MATCH: sound.dll 0x1001f6c0 ?FUN_1001f6c0@C_FUN_1001f6c0@f_1001f6c0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1001ff50(undefined4 *);
struct C_FUN_1001f6c0 { undefined4 * FUN_1001f6c0(byte param_1); };
undefined4 * C_FUN_1001f6c0::FUN_1001f6c0(byte param_1)
{
  thunk_FUN_1001ff50((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10021370 {
// MATCH: sound.dll 0x10021370 ?FUN_10021370@f_10021370@@YIIH@Z
undefined4 __fastcall FUN_10021370(int param_1)
{
  return *(undefined4 *)(param_1 + 0x71c);
}
}

namespace f_10022240 {
// MATCH: sound.dll 0x10022240 ?FUN_10022240@f_10022240@@YIIH@Z
undefined4 __fastcall thunk_FUN_1001e1c0(int);
undefined4 __fastcall FUN_10022240(int param_1)
{
  thunk_FUN_1001e1c0(param_1 + 0x60);
  *(undefined4 *)(param_1 + 0x740) = 0;
  *(uint *)(param_1 + 0x214) = *(uint *)(param_1 + 0x214) & 0xffffffe7;
  return 0;
}
}

namespace f_10022750 {
// MATCH: sound.dll 0x10022750 ?FUN_10022750@C_FUN_10022750@f_10022750@@QAEII@Z
struct T_thunk_FUN_10023d00 { int thunk_FUN_10023d00(uint *); };
struct C_FUN_10022750 { undefined4 FUN_10022750(uint param_1); };
undefined4 C_FUN_10022750::FUN_10022750(uint param_1)
{
  if (2 < param_1) {
    return 10;
  }
  *(uint *)((int)this + 0x214) = (param_1 & 3) << 7 | *(uint *)((int)this + 0x214) & 0xfffffe7f;
  ((T_thunk_FUN_10023d00 *)(this))->thunk_FUN_10023d00(*(uint **)((int)this + 0x238));
  return 0;
}
}

namespace f_100227a0 {
// MATCH: sound.dll 0x100227a0 ?FUN_100227a0@C_FUN_100227a0@f_100227a0@@QAEII@Z
struct C_FUN_100227a0 { undefined4 FUN_100227a0(undefined4 param_1); };
undefined4 C_FUN_100227a0::FUN_100227a0(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x240) = param_1;
  return 0;
}
}

namespace f_10024230 {
// MATCH: sound.dll 0x10024230 ?FUN_10024230@f_10024230@@YIPAIPAI@Z
int __fastcall thunk_FUN_1001ccf0(undefined4 *);
undefined4 * __fastcall FUN_10024230(undefined4 *param_1)
{
  thunk_FUN_1001ccf0(param_1);
  param_1[10] = 0;
  param_1[0xb] = 0;
  param_1[0xc] = 0;
  return param_1;
}
}

namespace f_10024260 {
// MATCH: sound.dll 0x10024260 ?FUN_10024260@f_10024260@@YIXH@Z
int __fastcall thunk_FUN_1001cd30(int);
void __fastcall FUN_10024260(int param_1)
{
  *(undefined4 *)(param_1 + 0x30) = 0;
  thunk_FUN_1001cd30(param_1);
  return;
}
}

namespace f_10024460 {
// MATCH: sound.dll 0x10024460 ?FUN_10024460@f_10024460@@YIPAIPAI@Z
int __fastcall thunk_FUN_1001caf0(undefined4 *);
undefined4 * __fastcall FUN_10024460(undefined4 *param_1)
{
  thunk_FUN_1001caf0(param_1 + 5);
  param_1[1] = 0x7f;
  *param_1 = 0;
  param_1[2] = 0;
  param_1[3] = 0;
  param_1[4] = 0x40;
  return param_1;
}
}

namespace f_100244a0 {
// MATCH: sound.dll 0x100244a0 ?FUN_100244a0@f_100244a0@@YIXH@Z
int __fastcall thunk_FUN_1001cbd0(undefined4 *);
void __fastcall FUN_100244a0(int param_1)
{
  thunk_FUN_1001cbd0((undefined4 *)(param_1 + 0x14));
  return;
}
}

namespace f_10024500 {
// MATCH: sound.dll 0x10024500 ?FUN_10024500@C_FUN_10024500@f_10024500@@QAEHHI@Z
struct T_thunk_FUN_1001d780 { int thunk_FUN_1001d780(int *, uint); };
struct T_thunk_FUN_1001e9a0 { int thunk_FUN_1001e9a0(int); };
struct T_thunk_FUN_100205f0 { undefined4 thunk_FUN_100205f0(uint); };
struct C_FUN_10024500 { int FUN_10024500(int param_1, uint param_2); };
int C_FUN_10024500::FUN_10024500(int param_1, uint param_2)
{
    int iVar1;
    int local_4;
    void * local_8;
  iVar1 = ((T_thunk_FUN_1001d780 *)((void *)((int)this + 0x60)))->thunk_FUN_1001d780(&local_4, *(uint *)((int)this + 0x20c));
  if (iVar1 != 0) {
    local_4 = 0;
  }
  iVar1 = ((T_thunk_FUN_1001d780 *)((void *)((int)this + 0x60)))->thunk_FUN_1001d780((int *)&local_8, param_2);
  if (iVar1 == 0) {
    ((T_thunk_FUN_1001e9a0 *)(local_8))->thunk_FUN_1001e9a0(param_1);
    *(uint *)((int)local_8 + 0x34) = *(uint *)((int)local_8 + 0x34) & 0xfffffffb;
    if ((local_4 == 0) || (*(uint *)(local_4 + 0x54) < *(uint *)((int)local_8 + 0x54))) {
      *(uint *)((int)this + 0x20c) = param_2;
    }
    ((T_thunk_FUN_100205f0 *)(this))->thunk_FUN_100205f0(param_2);
    iVar1 = 0;
  }
  return iVar1;
}
}

namespace f_10024c50 {
// MATCH: sound.dll 0x10024c50 ?FUN_10024c50@C_FUN_10024c50@f_10024c50@@QAEXE@Z
struct C_FUN_10024c50 { void FUN_10024c50(byte param_1); };
void C_FUN_10024c50::FUN_10024c50(byte param_1)
{
  if ((param_1 & 1) != 0) {
    *(uint *)((int)this + 0x214) = *(uint *)((int)this + 0x214) & 0xfffff3ff | 0x200;
    return;
  }
  if ((param_1 & 2) != 0) {
    *(uint *)((int)this + 0x214) = *(uint *)((int)this + 0x214) & 0xfffff5ff | 0x400;
    return;
  }
  if ((param_1 & 4) != 0) {
    *(uint *)((int)this + 0x214) = *(uint *)((int)this + 0x214) & 0xfffff9ff | 0x800;
  }
  return;
}
}

namespace f_10024de0 {
// MATCH: sound.dll 0x10024de0 ?FUN_10024de0@C_FUN_10024de0@f_10024de0@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_10024de0 { void FUN_10024de0(undefined4 param_1); };
void C_FUN_10024de0::FUN_10024de0(undefined4 param_1)
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

namespace f_10024f90 {
// MATCH: sound.dll 0x10024f90 ?FUN_10024f90@C_FUN_10024f90@f_10024f90@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_10024f90 { void FUN_10024f90(undefined4 param_1); };
void C_FUN_10024f90::FUN_10024f90(undefined4 param_1)
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

namespace f_10028560 {
// MATCH: sound.dll 0x10028560 ?FUN_10028560@C_FUN_10028560@f_10028560@@QAEII@Z
struct C_FUN_10028560 { uint FUN_10028560(uint param_1); };
uint C_FUN_10028560::FUN_10028560(uint param_1)
{
    uint uVar1;
  uVar1 = *(int *)this * 0x19660d + 0x3c6ef35f;
  *(uint *)this = uVar1;
  return (uVar1 >> 0x10) * (param_1 & 0xffff) >> 0x10;
}
}

namespace f_10028630 {
// MATCH: sound.dll 0x10028630 ?FUN_10028630@C_FUN_10028630@f_10028630@@QAEPAIE@Z
extern void *PTR_LAB_1005b8ac;
int __cdecl FUN_1004249a(undefined *);
struct C_FUN_10028630 { undefined4 * FUN_10028630(byte param_1); };
undefined4 * C_FUN_10028630::FUN_10028630(byte param_1)
{
  *(undefined ***)this = (unsigned char ** )(&PTR_LAB_1005b8ac);
  if ((*(HWND *)((int)this + 8) != (HWND)0x0) && ((*(byte *)((int)this + 4) & 4) != 0)) {
    SetWindowLongA(*(HWND *)((int)this + 8),-4,*(LONG *)((int)this + 0xc));
    *(undefined4 *)((int)this + 0xc) = 0;
    *(undefined4 *)((int)this + 8) = 0;
    *(byte *)((int)this + 4) = *(byte *)((int)this + 4) & 0xfb;
  }
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_100286a0 {
// MATCH: sound.dll 0x100286a0 ?FUN_100286a0@f_100286a0@@YIXPAI@Z
extern void *PTR_LAB_1005b8ac;
void __fastcall FUN_100286a0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b8ac);
  if (((HWND)param_1[2] != (HWND)0x0) && ((*(byte *)(param_1 + 1) & 4) != 0)) {
    SetWindowLongA((HWND)param_1[2],-4,param_1[3]);
    param_1[3] = 0;
    param_1[2] = 0;
    *(byte *)(param_1 + 1) = *(byte *)(param_1 + 1) & 0xfb;
  }
  return;
}
}

namespace f_100286f0 {
// MATCH: sound.dll 0x100286f0 ?FUN_100286f0@C_FUN_100286f0@f_100286f0@@QAEIPAUHWND__@@@Z
extern int DAT_100025ef;
struct C_FUN_100286f0 { undefined4 FUN_100286f0(HWND param_1); };
undefined4 C_FUN_100286f0::FUN_100286f0(HWND param_1)
{
    LONG LVar1;
  if (*(int *)((int)this + 8) == 0) {
    LVar1 = SetWindowLongA(param_1,-4,(long)(((char *)&DAT_100025ef)));
    *(LONG *)((int)this + 0xc) = LVar1;
    if (LVar1 == 0) {
      return 0x18;
    }
    *(HWND *)((int)this + 8) = param_1;
    *(byte *)((int)this + 4) = *(byte *)((int)this + 4) | 4;
  }
  return 0;
}
}

namespace f_100287c0 {
// MATCH: sound.dll 0x100287c0 ?FUN_100287c0@f_100287c0@@YIIH@Z
undefined4 __fastcall FUN_100287c0(int param_1)
{
  if ((*(HWND *)(param_1 + 8) != (HWND)0x0) && ((*(byte *)(param_1 + 4) & 4) != 0)) {
    SetWindowLongA(*(HWND *)(param_1 + 8),-4,*(LONG *)(param_1 + 0xc));
    *(undefined4 *)(param_1 + 0xc) = 0;
    *(undefined4 *)(param_1 + 8) = 0;
    *(byte *)(param_1 + 4) = *(byte *)(param_1 + 4) & 0xfb;
  }
  return 0;
}
}

namespace f_10028fb0 {
// MATCH: sound.dll 0x10028fb0 ?FUN_10028fb0@C_FUN_10028fb0@f_10028fb0@@QAEPAIE@Z
extern void *PTR_LAB_1005b8b0;
int __cdecl FUN_1004249a(undefined *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
struct C_FUN_10028fb0 { undefined4 * FUN_10028fb0(byte param_1); };
undefined4 * C_FUN_10028fb0::FUN_10028fb0(byte param_1)
{
  *(undefined ***)this = (unsigned char ** )(&PTR_LAB_1005b8b0);
  if (*(int **)((int)this + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x14)))->f5();
  }
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10028ff0 {
// MATCH: sound.dll 0x10028ff0 ?FUN_10028ff0@f_10028ff0@@YIXPAI@Z
extern int DAT_10028fff;
extern void *PTR_LAB_1005b8b0;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
void __fastcall FUN_10028ff0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b8b0);
  if ((int *)param_1[5] != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_10028fff). Too many branches */ /* WARNING: Treating indirect jump as call */ ((VT_1 *)(param_1[5]))->f5();
    return;
  }
  return;
}
}

namespace f_10029140 {
// MATCH: sound.dll 0x10029140 ?FUN_10029140@C_FUN_10029140@f_10029140@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10029200(undefined4 *);
struct C_FUN_10029140 { undefined4 * FUN_10029140(byte param_1); };
undefined4 * C_FUN_10029140::FUN_10029140(byte param_1)
{
  thunk_FUN_10029200((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10029170 {
// MATCH: sound.dll 0x10029170 ?FUN_10029170@f_10029170@@YIXH@Z
int __cdecl FUN_1004249a(undefined *);
void __fastcall FUN_10029170(int param_1)
{
    undefined * puVar1;
    undefined4 * puVar2;
    int iVar3;
  puVar1 = *(undefined **)(param_1 + 8);
  if (puVar1 != (undefined *)0x0) {
    puVar2 = *(undefined4 **)(puVar1 + 4);
    if (!(puVar2 == (undefined4 *)0x0)) {
      *puVar2 = 0;
      *(undefined4 **)(param_1 + 8) = puVar2;
    } else {
      *(undefined4 *)(param_1 + 0xc) = 0;
      *(undefined4 *)(param_1 + 8) = 0;
    }
    iVar3 = *(int *)(puVar1 + 8);
    FUN_1004249a(puVar1);
    *(int *)(param_1 + 0x14) = *(int *)(param_1 + 0x14) + -1;
    while ((iVar3 != 0 && (puVar1 = *(undefined **)(param_1 + 8), puVar1 != (undefined *)0x0))) {
      puVar2 = *(undefined4 **)(puVar1 + 4);
      if (!(puVar2 == (undefined4 *)0x0)) {
        *puVar2 = 0;
        *(undefined4 **)(param_1 + 8) = puVar2;
      } else {
        *(undefined4 *)(param_1 + 0xc) = 0;
        *(undefined4 *)(param_1 + 8) = 0;
      }
      iVar3 = *(int *)(puVar1 + 8);
      FUN_1004249a(puVar1);
      *(int *)(param_1 + 0x14) = *(int *)(param_1 + 0x14) + -1;
    }
  }
  return;
}
}

namespace f_10029320 {
// MATCH: sound.dll 0x10029320 ?FUN_10029320@f_10029320@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
undefined4 __fastcall FUN_10029320(int param_1)
{
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x14)))->f5();
    *(undefined4 *)(param_1 + 0x14) = 0;
  }
  return 0;
}
}

namespace f_10029600 {
// MATCH: sound.dll 0x10029600 ?FUN_10029600@f_10029600@@YIIH@Z
extern int DAT_10029609;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); };
undefined4 __fastcall FUN_10029600(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_10029609). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x14)))->f29();
    return uVar1;
  }
  return 0;
}
}

namespace f_100296d0 {
// MATCH: sound.dll 0x100296d0 ?FUN_100296d0@C_FUN_100296d0@f_100296d0@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(int); };
struct C_FUN_100296d0 { undefined4 FUN_100296d0(undefined4 param_1); };
undefined4 C_FUN_100296d0::FUN_100296d0(undefined4 param_1)
{
  if (*(int **)((int)this + 0x14) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x14)))->f21((int)(param_1)));
  }
  return 3;
}
}

namespace f_10029700 {
// MATCH: sound.dll 0x10029700 ?FUN_10029700@f_10029700@@YIIH@Z
extern int DAT_10029709;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); };
undefined4 __fastcall FUN_10029700(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_10029709). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x14)))->f22();
    return uVar1;
  }
  return 3;
}
}

namespace f_10029c30 {
// MATCH: sound.dll 0x10029c30 ?FUN_10029c30@C_FUN_10029c30@f_10029c30@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(int); };
struct C_FUN_10029c30 { undefined4 FUN_10029c30(undefined4 param_1); };
undefined4 C_FUN_10029c30::FUN_10029c30(undefined4 param_1)
{
  if (*(int **)((int)this + 0x14) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x14)))->f33((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_10029cd0 {
// MATCH: sound.dll 0x10029cd0 ?FUN_10029cd0@C_FUN_10029cd0@f_10029cd0@@QAEIIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(int, int, int); };
struct C_FUN_10029cd0 { undefined4 FUN_10029cd0(undefined4 param_1, undefined4 param_2, undefined4 param_3); };
undefined4 C_FUN_10029cd0::FUN_10029cd0(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  if (*(int **)((int)this + 0x14) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x14)))->f36((int)(param_1), (int)(param_2), (int)(param_3)));
  }
  return 0x14;
}
}

namespace f_10029d40 {
// MATCH: sound.dll 0x10029d40 ?FUN_10029d40@C_FUN_10029d40@f_10029d40@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(int); };
struct C_FUN_10029d40 { undefined4 FUN_10029d40(undefined4 param_1); };
undefined4 C_FUN_10029d40::FUN_10029d40(undefined4 param_1)
{
  if (*(int **)((int)this + 0x14) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x14)))->f38((int)(param_1)));
  }
  return 0x14;
}
}

namespace f_10029f30 {
// MATCH: sound.dll 0x10029f30 ?FUN_10029f30@C_FUN_10029f30@f_10029f30@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10029f60(undefined4 *);
struct C_FUN_10029f30 { undefined4 * FUN_10029f30(byte param_1); };
undefined4 * C_FUN_10029f30::FUN_10029f30(byte param_1)
{
  thunk_FUN_10029f60((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10029f60 {
// MATCH: sound.dll 0x10029f60 ?FUN_10029f60@f_10029f60@@YIXPAI@Z
extern int DAT_10029f6f;
extern void *PTR_LAB_1005b8b0;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
void __fastcall FUN_10029f60(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b8b0);
  if ((int *)param_1[5] != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_10029f6f). Too many branches */ /* WARNING: Treating indirect jump as call */ ((VT_1 *)(param_1[5]))->f5();
    return;
  }
  return;
}
}

namespace f_1002a000 {
// MATCH: sound.dll 0x1002a000 ?FUN_1002a000@f_1002a000@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
undefined4 __fastcall FUN_1002a000(int param_1)
{
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x14)))->f5();
    *(undefined4 *)(param_1 + 0x14) = 0;
  }
  return 0;
}
}

namespace f_1002a1d0 {
// MATCH: sound.dll 0x1002a1d0 ?FUN_1002a1d0@f_1002a1d0@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); };
undefined4 __fastcall FUN_1002a1d0(int param_1)
{
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x14)))->f22();
  }
  return 0;
}
}

namespace f_1002a1f0 {
// MATCH: sound.dll 0x1002a1f0 ?FUN_1002a1f0@f_1002a1f0@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); };
undefined4 __fastcall FUN_1002a1f0(int param_1)
{
  if (*(int **)(param_1 + 0x14) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x14)))->f23();
  }
  return 0;
}
}

namespace f_1002a380 {
// MATCH: sound.dll 0x1002a380 ?FUN_1002a380@C_FUN_1002a380@f_1002a380@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002a3b0(undefined4 *);
struct C_FUN_1002a380 { undefined4 * FUN_1002a380(byte param_1); };
undefined4 * C_FUN_1002a380::FUN_1002a380(byte param_1)
{
  thunk_FUN_1002a3b0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002a3b0 {
// MATCH: sound.dll 0x1002a3b0 ?FUN_1002a3b0@f_1002a3b0@@YIXPAI@Z
extern int DAT_1002a3bf;
extern void *PTR_LAB_1005b8b0;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
void __fastcall FUN_1002a3b0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b8b0);
  if ((int *)param_1[5] != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_1002a3bf). Too many branches */ /* WARNING: Treating indirect jump as call */ ((VT_1 *)(param_1[5]))->f5();
    return;
  }
  return;
}
}

namespace f_1002a470 {
// MATCH: sound.dll 0x1002a470 ?FUN_1002a470@f_1002a470@@YIIPAH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); };
undefined4 __fastcall FUN_1002a470(int *param_1)
{
  if ((int *)param_1[5] != (int *)0x0) {
    ((VT_1 *)(param_1[5]))->f5();
    ((VT_2 *)(param_1))->f2();
    param_1[5] = 0;
  }
  return 0;
}
}

namespace f_1002a4e0 {
// MATCH: sound.dll 0x1002a4e0 ?FUN_1002a4e0@C_FUN_1002a4e0@f_1002a4e0@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(int); };
struct C_FUN_1002a4e0 { undefined4 FUN_1002a4e0(undefined4 param_1); };
undefined4 C_FUN_1002a4e0::FUN_1002a4e0(undefined4 param_1)
{
  if (*(int **)((int)this + 0x14) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 0x14)))->f26((int)(param_1)));
  }
  return 0x13;
}
}
