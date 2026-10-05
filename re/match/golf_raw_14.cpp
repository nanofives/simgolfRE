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

namespace f_00401c30 {
// MATCH: golf_clean.exe 0x00401c30 ?FUN_00401c30@C_FUN_00401c30@f_00401c30@@QAEPAIE@Z
extern int DAT_00839650;
extern void *PTR_FUN_004ba278;
void __cdecl _free(LPVOID);
struct C_FUN_00401c30 { undefined4 * FUN_00401c30(byte param_1); };
undefined4 * C_FUN_00401c30::FUN_00401c30(byte param_1)
{
  *(undefined ***)this = (unsigned char ** )(&PTR_FUN_004ba278);
  DAT_00839650 = *(int *)((int)this + 4);
  if (((param_1 & 1) != 0) && (this != 0)) {
    if (DAT_00839650 == 0) {
      _free(this);
    }
    DAT_00839650 = 0;
  }
  return (unsigned int *)(this);
}
}

namespace f_0047ada0 {
// MATCH: golf_clean.exe 0x0047ada0 ?FUN_0047ada0@f_0047ada0@@YIXH@Z
void __cdecl _free(LPVOID);
void __fastcall FUN_0047ada0(int param_1)
{
    int iVar1;
    LPVOID pvVar2;
    int iVar3;
  iVar1 = *(int *)(param_1 + 0x130);
  if ((iVar1 != 0) && (*(int *)(iVar1 + 0x138) != 0)) {
    for (iVar3 = 0; iVar3 < *(int *)(iVar1 + 0x140); iVar3++) {
      if (*(int *)(*(int *)(iVar1 + 0x13c) + 4) == param_1) {
        *(undefined4 *)(*(int *)(*(int *)(iVar1 + 0x13c) + 0xc) + 0x10) = *(undefined4 *)(*(int *)(iVar1 + 0x13c) + 0x10);
        *(undefined4 *)(*(int *)(*(int *)(iVar1 + 0x13c) + 0x10) + 0xc) = *(undefined4 *)(*(int *)(iVar1 + 0x13c) + 0xc);
        pvVar2 = *(LPVOID *)(iVar1 + 0x13c);
        if (pvVar2 == *(LPVOID *)(iVar1 + 0x138)) {
          *(undefined4 *)(iVar1 + 0x138) = *(undefined4 *)((int)pvVar2 + 0xc);
        }
        *(undefined4 *)(iVar1 + 0x13c) = *(undefined4 *)((int)pvVar2 + 0xc);
        if (*(int *)(iVar1 + 0x148) == 0) {
          if (*(LPVOID *)((int)pvVar2 + 8) != (LPVOID)0x0) {
            _free(*(LPVOID *)((int)pvVar2 + 8));
          }
          *(undefined4 *)((int)pvVar2 + 8) = 0;
          if (pvVar2 != (LPVOID)0x0) {
            _free(pvVar2);
          }
        }
        *(int *)(iVar1 + 0x140) = *(int *)(iVar1 + 0x140) + -1;
        break;
      }
      *(undefined4 *)(iVar1 + 0x13c) = *(undefined4 *)(*(int *)(iVar1 + 0x13c) + 0xc);
    }
    if (*(int *)(iVar1 + 0x140) == 0) {
      *(undefined4 *)(iVar1 + 0x138) = 0;
    }
    *(int *)(iVar1 + 0x144) = *(int *)(iVar1 + 0x140) + -1;
  }
  return;
}
}

namespace f_0049bf70 {
// MATCH: golf_clean.exe 0x0049bf70 ?FUN_0049bf70@C_FUN_0049bf70@f_0049bf70@@QAEPAIE@Z
extern int DAT_00839650;
extern void *PTR_FUN_004ba278;
void __cdecl _free(LPVOID);
void __fastcall thunk_FUN_004853d0(undefined4 *);
struct C_FUN_0049bf70 { undefined4 * FUN_0049bf70(byte param_1); };
undefined4 * C_FUN_0049bf70::FUN_0049bf70(byte param_1)
{
  thunk_FUN_004853d0((unsigned int *)(this));
  *(undefined ***)((int)this + 0x5c) = (unsigned char ** )(&PTR_FUN_004ba278);
  DAT_00839650 = *(int *)((int)this + 0x60);
  if (((param_1 & 1) != 0) && (this != 0)) {
    if (DAT_00839650 == 0) {
      _free(this);
    }
    DAT_00839650 = 0;
  }
  return (unsigned int *)(this);
}
}
