// FLAGS sound.dll: /O1
// sound.dll functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (round 36 batch m36c, raw form:
// Ghidra's offsets, casts and FUN_/DAT_ names, decompiled with re/names reset in memory by
// log/c3/m36c_ghidra/M36cDecompileRaw.java). Names, types and layouts are still to be written: these are evidence of
// byte identity, not readable source. String literals are placeholders named by address (s_<addr>), never the game's text.

#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10041aad {
// MATCH: sound.dll 0x10041aad ?FUN_10041aad@f_10041aad@@YIXH@Z
extern void *PTR_LAB_1005ecd4;
void __fastcall FUN_10041aad(int param_1)
{
  *(undefined ***)(*(int *)(*(int *)(param_1 + -8) + 4) + -8 + param_1) = (unsigned char ** )(&PTR_LAB_1005ecd4);
  return;
}
}

namespace f_10041c26 {
// MATCH: sound.dll 0x10041c26 ?FUN_10041c26@f_10041c26@@YIXH@Z
extern void *PTR_FUN_1005ece4;
int __fastcall FUN_10041aad(int);
void __fastcall FUN_10041c26(int param_1)
{
  *(undefined ***)(*(int *)(*(int *)(param_1 + -8) + 4) + -8 + param_1) = (unsigned char ** )(&PTR_FUN_1005ece4);
  FUN_10041aad(param_1);
  return;
}
}

namespace f_10041e43 {
// MATCH: sound.dll 0x10041e43 ?FUN_10041e43@f_10041e43@@YAXPAU_RTL_CRITICAL_SECTION@@@Z
void __cdecl FUN_10041e43(LPCRITICAL_SECTION param_1)
{
  InitializeCriticalSection(param_1);
  return;
}
}

namespace f_10041e4e {
// MATCH: sound.dll 0x10041e4e ?FUN_10041e4e@f_10041e4e@@YAXPAU_RTL_CRITICAL_SECTION@@@Z
void __cdecl FUN_10041e4e(LPCRITICAL_SECTION param_1)
{
  DeleteCriticalSection(param_1);
  return;
}
}

namespace f_10041e59 {
// MATCH: sound.dll 0x10041e59 ?FUN_10041e59@f_10041e59@@YAXPAU_RTL_CRITICAL_SECTION@@@Z
void __cdecl FUN_10041e59(LPCRITICAL_SECTION param_1)
{
  EnterCriticalSection(param_1);
  return;
}
}

namespace f_10041e64 {
// MATCH: sound.dll 0x10041e64 ?FUN_10041e64@f_10041e64@@YAXPAU_RTL_CRITICAL_SECTION@@@Z
void __cdecl FUN_10041e64(LPCRITICAL_SECTION param_1)
{
  LeaveCriticalSection(param_1);
  return;
}
}

namespace f_10045b9b {
// MATCH: sound.dll 0x10045b9b ?FUN_10045b9b@f_10045b9b@@YAXPAI@Z
void __cdecl FUN_10045b9b(undefined4 *param_1)
{
    int iVar1;
    HMODULE pHVar2;
  *param_1 = 0;
  pHVar2 = GetModuleHandleA((LPCSTR)0x0);
  if (((short)pHVar2->unused == 0x5a4d) && (iVar1 = pHVar2[0xf].unused, iVar1 != 0)) {
    *(undefined1 *)param_1 = *(undefined1 *)((int)&pHVar2[6].unused + iVar1 + 2);
    *(undefined1 *)((int)param_1 + 1) = *(undefined1 *)((int)&pHVar2[6].unused + iVar1 + 3);
  }
  return;
}
}

namespace f_100470ac {
// MATCH: sound.dll 0x100470ac ?FUN_100470ac@f_100470ac@@YAHPAEPAI1@Z
extern void *PTR_LOOP_100653d8;
int __cdecl FUN_100470ac(undefined *param_1, undefined4 *param_2, uint *param_3)
{
    undefined ** ppuVar1;
    uint uVar2;
  ppuVar1 = (unsigned char ** )(&PTR_LOOP_100653d8);
  while ((param_1 <= ppuVar1[4] || (param_1 >= ppuVar1[5]))) {
    ppuVar1 = (undefined **)*ppuVar1;
    if ((int)(ppuVar1) == (int)(&PTR_LOOP_100653d8)) {
      return 0;
    }
  }
  if (((uint)param_1 & 0xf) != 0) {
    return 0;
  }
  if (((uint)param_1 & 0xfff) < 0x100) {
    return 0;
  }
  *param_2 = (unsigned int)(ppuVar1);
  uVar2 = (uint)param_1 & 0xfffff000;
  *param_3 = uVar2;
  return ((int)(param_1 + (-0x100 - uVar2)) >> 4) + 8 + uVar2;
}
}

namespace f_10047103 {
// MATCH: sound.dll 0x10047103 ?FUN_10047103@f_10047103@@YAXHHPAE@Z
extern int DAT_100b5590;
int __cdecl FUN_10046fea(int);
void __cdecl FUN_10047103(int param_1, int param_2, byte *param_3)
{
    int * piVar1;
  piVar1 = (int *)(param_1 + 0x18 + (param_2 - *(int *)(param_1 + 0x10) >> 0xc) * 8);
  *piVar1 = *piVar1 + (uint)*param_3;
  *param_3 = 0;
  piVar1[1] = 0xf1;
  if ((*piVar1 == 0xf0) && (DAT_100b5590 = DAT_100b5590 + 1, DAT_100b5590 == 0x20)) {
    FUN_10046fea(0x10);
  }
  return;
}
}

namespace f_1004e4b3 {
// MATCH: sound.dll 0x1004e4b3 ?FUN_1004e4b3@f_1004e4b3@@YAXPAI@Z
int __cdecl FUN_10042c07(undefined *);
/* Library Function - Single Match ___free_lc_time Libraries: VC6 12.00.8168 libc, VC6 12.00.8168 libcmt, Visual Studio 2003 Release, Visual Studio 2005 Release */ void __cdecl FUN_1004e4b3(undefined4 *param_1)
{
  if (param_1 != 0) {
    FUN_10042c07((undefined *)param_1[1]);
    FUN_10042c07((undefined *)param_1[2]);
    FUN_10042c07((undefined *)param_1[3]);
    FUN_10042c07((undefined *)param_1[4]);
    FUN_10042c07((undefined *)param_1[5]);
    FUN_10042c07((undefined *)param_1[6]);
    FUN_10042c07((undefined *)*param_1);
    FUN_10042c07((undefined *)param_1[8]);
    FUN_10042c07((undefined *)param_1[9]);
    FUN_10042c07((undefined *)param_1[10]);
    FUN_10042c07((undefined *)param_1[0xb]);
    FUN_10042c07((undefined *)param_1[0xc]);
    FUN_10042c07((undefined *)param_1[0xd]);
    FUN_10042c07((undefined *)param_1[7]);
    FUN_10042c07((undefined *)param_1[0xe]);
    FUN_10042c07((undefined *)param_1[0xf]);
    FUN_10042c07((undefined *)param_1[0x10]);
    FUN_10042c07((undefined *)param_1[0x11]);
    FUN_10042c07((undefined *)param_1[0x12]);
    FUN_10042c07((undefined *)param_1[0x13]);
    FUN_10042c07((undefined *)param_1[0x14]);
    FUN_10042c07((undefined *)param_1[0x15]);
    FUN_10042c07((undefined *)param_1[0x16]);
    FUN_10042c07((undefined *)param_1[0x17]);
    FUN_10042c07((undefined *)param_1[0x18]);
    FUN_10042c07((undefined *)param_1[0x19]);
    FUN_10042c07((undefined *)param_1[0x1a]);
    FUN_10042c07((undefined *)param_1[0x1b]);
    FUN_10042c07((undefined *)param_1[0x1c]);
    FUN_10042c07((undefined *)param_1[0x1d]);
    FUN_10042c07((undefined *)param_1[0x1e]);
    FUN_10042c07((undefined *)param_1[0x1f]);
    FUN_10042c07((undefined *)param_1[0x20]);
    FUN_10042c07((undefined *)param_1[0x21]);
    FUN_10042c07((undefined *)param_1[0x22]);
    FUN_10042c07((undefined *)param_1[0x23]);
    FUN_10042c07((undefined *)param_1[0x24]);
    FUN_10042c07((undefined *)param_1[0x25]);
    FUN_10042c07((undefined *)param_1[0x26]);
    FUN_10042c07((undefined *)param_1[0x27]);
    FUN_10042c07((undefined *)param_1[0x28]);
    FUN_10042c07((undefined *)param_1[0x29]);
    FUN_10042c07((undefined *)param_1[0x2a]);
  }
  return;
}
}
