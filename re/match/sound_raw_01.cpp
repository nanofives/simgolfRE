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

namespace f_10003fd0 {
// MATCH: sound.dll 0x10003fd0 ?FUN_10003fd0@C_FUN_10003fd0@f_10003fd0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10004000(undefined4 *);
struct C_FUN_10003fd0 { undefined4 * FUN_10003fd0(byte param_1); };
undefined4 * C_FUN_10003fd0::FUN_10003fd0(byte param_1)
{
  thunk_FUN_10004000((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10005670 {
// MATCH: sound.dll 0x10005670 ?FUN_10005670@C_FUN_10005670@f_10005670@@QAEIH@Z
struct C_FUN_10005670 { undefined4 FUN_10005670(int param_1); };
undefined4 C_FUN_10005670::FUN_10005670(int param_1)
{
  if (param_1 == 0) {
    return 10;
  }
  *(int *)((int)this + 0x38) = param_1;
  return 0;
}
}

namespace f_10005720 {
// MATCH: sound.dll 0x10005720 ?FUN_10005720@f_10005720@@YIXPAI@Z
extern void *PTR_LAB_1005b19c;
void __fastcall FUN_10005720(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1005b19c);
  return;
}
}

namespace f_10005740 {
// MATCH: sound.dll 0x10005740 ?FUN_10005740@C_FUN_10005740@f_10005740@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10005720(undefined4 *);
struct C_FUN_10005740 { undefined4 * FUN_10005740(byte param_1); };
undefined4 * C_FUN_10005740::FUN_10005740(byte param_1)
{
  thunk_FUN_10005720((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10005dd0 {
// MATCH: sound.dll 0x10005dd0 ?FUN_10005dd0@f_10005dd0@@YIIH@Z
undefined4 __fastcall FUN_10005dd0(int param_1)
{
  if ((((*(uint *)(param_1 + 0xac) & 0x20) == 0) || ((*(uint *)(param_1 + 0xac) & 8) != 0)) && ((*(byte *)(param_1 + 0x214) & 8) == 0)) {
    return 0;
  }
  return 1;
}
}

namespace f_10005e00 {
// MATCH: sound.dll 0x10005e00 ?FUN_10005e00@f_10005e00@@YIIH@Z
extern int DAT_100b5000;
struct T_thunk_FUN_10005e30 { int thunk_FUN_10005e30(undefined4); };
undefined4 __fastcall FUN_10005e00(int param_1)
{
  ((T_thunk_FUN_10005e30 *)(((char *)&DAT_100b5000)))->thunk_FUN_10005e30(param_1);
  *(uint *)(param_1 + 0x214) = *(uint *)(param_1 + 0x214) | 2;
  return 0;
}
}

namespace f_10005e30 {
// MATCH: sound.dll 0x10005e30 ?FUN_10005e30@C_FUN_10005e30@f_10005e30@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_10005e30 { void FUN_10005e30(undefined4 param_1); };
void C_FUN_10005e30::FUN_10005e30(undefined4 param_1)
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

namespace f_10005f60 {
// MATCH: sound.dll 0x10005f60 ?FUN_10005f60@C_FUN_10005f60@f_10005f60@@QAEXE@Z
struct C_FUN_10005f60 { void FUN_10005f60(byte param_1); };
void C_FUN_10005f60::FUN_10005f60(byte param_1)
{
  *(uint *)((int)this + 0x214) = (param_1 & 1) << 5 | *(uint *)((int)this + 0x214) & 0xffffffdf;
  return;
}
}

namespace f_10005fb0 {
// MATCH: sound.dll 0x10005fb0 ?FUN_10005fb0@f_10005fb0@@YIIH@Z
undefined4 __fastcall FUN_10005fb0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x73c);
}
}

namespace f_10006130 {
// MATCH: sound.dll 0x10006130 ?FUN_10006130@C_FUN_10006130@f_10006130@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10006160(undefined4 *);
struct C_FUN_10006130 { undefined4 * FUN_10006130(byte param_1); };
undefined4 * C_FUN_10006130::FUN_10006130(byte param_1)
{
  thunk_FUN_10006160((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10006310 {
// MATCH: sound.dll 0x10006310 ?FUN_10006310@f_10006310@@YIIH@Z
undefined4 __fastcall thunk_FUN_1002b090(int);
undefined4 __fastcall FUN_10006310(int param_1)
{
  thunk_FUN_1002b090(param_1);
  return 0;
}
}

namespace f_10006b00 {
// MATCH: sound.dll 0x10006b00 ?FUN_10006b00@C_FUN_10006b00@f_10006b00@@QAEIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); };
struct C_FUN_10006b00 { undefined4 FUN_10006b00(int param_1); };
undefined4 C_FUN_10006b00::FUN_10006b00(int param_1)
{
  if ((*(uint *)((int)this + 0x58) & 1) != 0) {
    return 0xd;
  }
  *(int *)((int)this + 0xcc) = param_1;
  *(uint *)((int)this + 0x58) = *(uint *)((int)this + 0x58) & 0xfffffffb | 3;
  *(undefined4 *)((int)this + 200) = 0;
  ((VT_1 *)(this))->f31();
  return 0;
}
}

namespace f_10006db0 {
// MATCH: sound.dll 0x10006db0 ?FUN_10006db0@f_10006db0@@YIIH@Z
undefined4 __fastcall FUN_10006db0(int param_1)
{
  return *(undefined4 *)(param_1 + 0xb4);
}
}

namespace f_10006ec0 {
// MATCH: sound.dll 0x10006ec0 ?FUN_10006ec0@f_10006ec0@@YIXH@Z
int __cdecl FUN_1004249a(undefined *);
void __fastcall FUN_10006ec0(int param_1)
{
  *(uint *)(param_1 + 8) = *(uint *)(param_1 + 8) & 0xfffffffe;
  if (*(undefined **)(param_1 + 4) != (undefined *)0x0) {
    FUN_1004249a(*(undefined **)(param_1 + 4));
  }
  *(undefined4 *)(param_1 + 4) = 0;
  return;
}
}

namespace f_100086f0 {
// MATCH: sound.dll 0x100086f0 ?FUN_100086f0@C_FUN_100086f0@f_100086f0@@QAEIPAD@Z
extern int DAT_100b4a08;
struct C_FUN_100086f0 { undefined4 FUN_100086f0(LPSTR param_1); };
undefined4 C_FUN_100086f0::FUN_100086f0(LPSTR param_1)
{
    HMMIO pHVar1;
    int iVar2;
    _MMIOINFO * p_Var3;
    _MMIOINFO local_48;
  p_Var3 = &local_48;
  for (iVar2 = 0x12; iVar2 != 0; iVar2 = iVar2 + -1) {
    p_Var3->dwFlags = 0;
    p_Var3 = (_MMIOINFO *)&p_Var3->fccIOProc;
  }
  local_48.cchBuffer = 0x1000;
  local_48.fccIOProc = 0x204d454d;
  pHVar1 = mmioOpenA(param_1,&local_48,0x11001);
  *(HMMIO *)((int)this + 0x30) = pHVar1;
  if (pHVar1 == (HMMIO)0x0) {
    return 8;
  }
  DAT_100b4a08 = DAT_100b4a08 + 1;
  return 0;
}
}

namespace f_10008770 {
// MATCH: sound.dll 0x10008770 ?FUN_10008770@f_10008770@@YIIH@Z
extern int DAT_100b4a08;
undefined4 __fastcall FUN_10008770(int param_1)
{
  if (*(HMMIO *)(param_1 + 0x30) != (HMMIO)0x0) {
    mmioClose(*(HMMIO *)(param_1 + 0x30),0);
  }
  DAT_100b4a08 = DAT_100b4a08 + -1;
  return 0;
}
}

namespace f_10008840 {
// MATCH: sound.dll 0x10008840 ?FUN_10008840@f_10008840@@YIXPAI@Z
void __fastcall FUN_10008840(undefined4 *param_1)
{
  param_1[1] = 0;
  *param_1 = 0;
  return;
}
}

namespace f_10008860 {
// MATCH: sound.dll 0x10008860 ?FUN_10008860@f_10008860@@YIPAIPAI@Z
undefined4 * __fastcall FUN_10008860(undefined4 *param_1)
{
    int iVar1;
    undefined4 * puVar2;
  puVar2 = param_1;
  for (iVar1 = 0x1000; iVar1 != 0; iVar1 = iVar1 + -1) {
    *puVar2 = 0;
    puVar2 = puVar2 + 1;
  }
  param_1[0x1001] = 0;
  param_1[0x1000] = 0;
  return param_1;
}
}

namespace f_10008890 {
// MATCH: sound.dll 0x10008890 ?FUN_10008890@f_10008890@@YIXPAI@Z
void __fastcall FUN_10008890(undefined4 *param_1)
{
    int iVar1;
    undefined4 * puVar2;
  puVar2 = param_1;
  for (iVar1 = 0x1000; iVar1 != 0; iVar1 = iVar1 + -1) {
    *puVar2 = 0;
    puVar2 = puVar2 + 1;
  }
  param_1[0x1001] = 0;
  param_1[0x1000] = 0;
  return;
}
}

namespace f_100088c0 {
// MATCH: sound.dll 0x100088c0 ?FUN_100088c0@C_FUN_100088c0@f_100088c0@@QAEII@Z
struct C_FUN_100088c0 { undefined4 FUN_100088c0(uint param_1); };
undefined4 C_FUN_100088c0::FUN_100088c0(uint param_1)
{
  return *(undefined4 *)((int)this + (param_1 & 0xfff) * 4);
}
}

namespace f_100088e0 {
// MATCH: sound.dll 0x100088e0 ?FUN_100088e0@C_FUN_100088e0@f_100088e0@@QAEXI@Z
extern int DAT_100b4990;
struct T_thunk_FUN_10009040 { int thunk_FUN_10009040(undefined4); };
struct C_FUN_100088e0 { void FUN_100088e0(undefined4 param_1); };
void C_FUN_100088e0::FUN_100088e0(undefined4 param_1)
{
  EnterCriticalSection((LPCRITICAL_SECTION)((char *)&DAT_100b4990));
  ((T_thunk_FUN_10009040 *)((void *)((int)this + 0x4c)))->thunk_FUN_10009040(param_1);
  LeaveCriticalSection((LPCRITICAL_SECTION)((char *)&DAT_100b4990));
  return;
}
}

namespace f_10008bf0 {
// MATCH: sound.dll 0x10008bf0 ?FUN_10008bf0@f_10008bf0@@YIXH@Z
extern int DAT_100b4990;
extern int DAT_100b49a8;
int __cdecl FUN_1004249a(undefined *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); };
void __fastcall FUN_10008bf0(int param_1)
{
    undefined * puVar1;
    undefined4 * puVar2;
    int * piVar3;
  DAT_100b49a8 = DAT_100b49a8 + 1;
  EnterCriticalSection((LPCRITICAL_SECTION)((char *)&DAT_100b4990));
  LeaveCriticalSection((LPCRITICAL_SECTION)((char *)&DAT_100b4990));
  if ((*(byte *)(param_1 + 0x3c) & 1) != 0) {
    DAT_100b49a8 = DAT_100b49a8 + 1;
    *(uint *)(param_1 + 0x3c) = *(uint *)(param_1 + 0x3c) & 0xfffffffb;
    EnterCriticalSection((LPCRITICAL_SECTION)((char *)&DAT_100b4990));
    LeaveCriticalSection((LPCRITICAL_SECTION)((char *)&DAT_100b4990));
    *(undefined4 *)(param_1 + 8) = 0;
    *(undefined4 *)(param_1 + 4) = 0;
    timeKillEvent(*(UINT *)(param_1 + 0x40));
    timeEndPeriod(*(UINT *)(param_1 + 0x18));
    *(uint *)(param_1 + 0x3c) = *(uint *)(param_1 + 0x3c) & 0xfffffffe;
  }
  while( true ) {
    puVar1 = *(undefined **)(param_1 + 0x4c);
    if (puVar1 == (undefined *)0x0) {
      return;
    }
    puVar2 = *(undefined4 **)(puVar1 + 4);
    if (!(puVar2 == (undefined4 *)0x0)) {
      *puVar2 = 0;
      *(undefined4 **)(param_1 + 0x4c) = puVar2;
    } else {
      *(undefined4 *)(param_1 + 0x50) = 0;
      *(undefined4 *)(param_1 + 0x4c) = 0;
    }
    piVar3 = *(int **)(puVar1 + 8);
    FUN_1004249a(puVar1);
    *(int *)(param_1 + 0x58) = *(int *)(param_1 + 0x58) + -1;
    if (piVar3 == (int *)0x0) break;
    ((VT_1 *)(piVar3))->f3();
  }
  return;
}
}

namespace f_10008d70 {
// MATCH: sound.dll 0x10008d70 ?FUN_10008d70@f_10008d70@@YIIH@Z
extern int DAT_100b4990;
extern int DAT_100b49a8;
undefined4 __fastcall FUN_10008d70(int param_1)
{
  if ((*(byte *)(param_1 + 0x3c) & 1) == 0) {
    return 0x15;
  }
  DAT_100b49a8 = DAT_100b49a8 + 1;
  *(uint *)(param_1 + 0x3c) = *(uint *)(param_1 + 0x3c) & 0xfffffffb;
  EnterCriticalSection((LPCRITICAL_SECTION)((char *)&DAT_100b4990));
  LeaveCriticalSection((LPCRITICAL_SECTION)((char *)&DAT_100b4990));
  *(undefined4 *)(param_1 + 8) = 0;
  *(undefined4 *)(param_1 + 4) = 0;
  timeKillEvent(*(UINT *)(param_1 + 0x40));
  timeEndPeriod(*(UINT *)(param_1 + 0x18));
  *(uint *)(param_1 + 0x3c) = *(uint *)(param_1 + 0x3c) & 0xfffffffe;
  return 0;
}
}

namespace f_10009040 {
// MATCH: sound.dll 0x10009040 ?FUN_10009040@C_FUN_10009040@f_10009040@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_10009040 { void FUN_10009040(undefined4 param_1); };
void C_FUN_10009040::FUN_10009040(undefined4 param_1)
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

namespace f_10009c80 {
// MATCH: sound.dll 0x10009c80 ?FUN_10009c80@C_FUN_10009c80@f_10009c80@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10009cb0(undefined4 *);
struct C_FUN_10009c80 { undefined4 * FUN_10009c80(byte param_1); };
undefined4 * C_FUN_10009c80::FUN_10009c80(byte param_1)
{
  thunk_FUN_10009cb0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1000a1b0 {
// MATCH: sound.dll 0x1000a1b0 ?FUN_1000a1b0@f_1000a1b0@@YIIPAH@Z
extern int DAT_100b49b8;
extern char DAT_100b49bc;
undefined4 __fastcall thunk_FUN_10028810(int);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); };
undefined4 __fastcall FUN_1000a1b0(int *param_1)
{
  if ((*(byte *)(param_1 + 9) & 1) == 0) {
    ((VT_1 *)(param_1))->f19();
    thunk_FUN_10028810((int)(((char *)&DAT_100b49b8)));
    DAT_100b49bc = DAT_100b49bc | 2;
    param_1[9] = param_1[9] | 1;
  }
  return 0;
}
}

namespace f_1000a200 {
// MATCH: sound.dll 0x1000a200 ?FUN_1000a200@f_1000a200@@YIIPAH@Z
extern char DAT_100b49bc;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); };
undefined4 __fastcall FUN_1000a200(int *param_1)
{
  if ((*(byte *)(param_1 + 9) & 1) != 0) {
    DAT_100b49bc = DAT_100b49bc & 0xfd;
    ((VT_1 *)(param_1))->f20();
    param_1[9] = param_1[9] & 0xfffffffe;
  }
  return 0;
}
}

namespace f_1000af20 {
// MATCH: sound.dll 0x1000af20 ?FUN_1000af20@C_FUN_1000af20@f_1000af20@@QAEXH@Z
struct C_FUN_1000af20 { void FUN_1000af20(int param_1); };
void C_FUN_1000af20::FUN_1000af20(int param_1)
{
  *(int *)((int)this + 0x34) = *(int *)((int)this + 0x34) + param_1;
  return;
}
}

namespace f_1000afa0 {
// MATCH: sound.dll 0x1000afa0 ?FUN_1000afa0@C_FUN_1000afa0@f_1000afa0@@QAEXI@Z
struct C_FUN_1000afa0 { void FUN_1000afa0(undefined4 param_1); };
void C_FUN_1000afa0::FUN_1000afa0(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x30) = param_1;
  return;
}
}

namespace f_1000b000 {
// MATCH: sound.dll 0x1000b000 ?FUN_1000b000@f_1000b000@@YIPAIPAI@Z
int __fastcall thunk_FUN_1002e3f0(undefined4 *);
undefined4 * __fastcall FUN_1000b000(undefined4 *param_1)
{
  thunk_FUN_1002e3f0(param_1);
  param_1[9] = 0;
  param_1[10] = 0;
  param_1[0xb] = 0;
  param_1[0xc] = 0;
  param_1[0xd] = 0;
  return param_1;
}
}

namespace f_1000b030 {
// MATCH: sound.dll 0x1000b030 ?FUN_1000b030@f_1000b030@@YIHH@Z
int __fastcall FUN_1000b030(int param_1)
{
  return (*(int *)(param_1 + 0xc) - *(int *)(param_1 + 8)) + 1;
}
}

namespace f_1000b2b0 {
// MATCH: sound.dll 0x1000b2b0 ?FUN_1000b2b0@C_FUN_1000b2b0@f_1000b2b0@@QAEPAHE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1000b2e0(int *);
struct C_FUN_1000b2b0 { int * FUN_1000b2b0(byte param_1); };
int * C_FUN_1000b2b0::FUN_1000b2b0(byte param_1)
{
  thunk_FUN_1000b2e0((int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (int *)(this);
}
}

namespace f_1000bef0 {
// MATCH: sound.dll 0x1000bef0 ?FUN_1000bef0@f_1000bef0@@YIXPAH@Z
extern int DAT_100b49ec;
extern int DAT_100b49f0;
extern int DAT_100b49f4;
extern int DAT_100b49f8;
extern int DAT_100b49fc;
extern int DAT_100b4a00;
struct T_thunk_FUN_10008920 { int thunk_FUN_10008920(int *); };
undefined4 __fastcall thunk_FUN_10008d70(int);
undefined4 __fastcall thunk_FUN_1000f760(int);
void __fastcall FUN_1000bef0(int *param_1)
{
  thunk_FUN_10008d70((int)DAT_100b49ec);
  if (param_1[0x6a] != 0) {
    ((T_thunk_FUN_10008920 *)(DAT_100b49ec))->thunk_FUN_10008920(param_1);
  }
  if (DAT_100b49f8 != 0) {
    thunk_FUN_1000f760(DAT_100b49f8);
  }
  if ((*(byte *)(DAT_100b49f0 + 0x6c) & 0x10) != 0) {
    if (DAT_100b49fc != 0) {
      thunk_FUN_1000f760(DAT_100b49fc);
    }
    if (DAT_100b4a00 != 0) {
      thunk_FUN_1000f760(DAT_100b4a00);
    }
  }
  if (DAT_100b49f4 != 0) {
    *(undefined4 *)(DAT_100b49f4 + 0x18) = 0;
  }
  param_1[0x6c] = 0;
  param_1[0x6d] = 0;
  return;
}
}

namespace f_1000c3d0 {
// MATCH: sound.dll 0x1000c3d0 ?FUN_1000c3d0@C_FUN_1000c3d0@f_1000c3d0@@QAEII@Z
struct C_FUN_1000c3d0 { undefined4 FUN_1000c3d0(undefined4 param_1); };
undefined4 C_FUN_1000c3d0::FUN_1000c3d0(undefined4 param_1)
{
    uint uVar1;
  uVar1 = *(int *)((int)this + 0x41dc) + 1U & 0xfff;
  if (uVar1 == *(uint *)((int)this + 0x41d8)) {
    return 0x22;
  }
  *(undefined4 *)((int)this + uVar1 * 4 + 0x1d8) = param_1;
  *(uint *)((int)this + 0x41dc) = uVar1;
  return 0;
}
}

namespace f_1000cc40 {
// MATCH: sound.dll 0x1000cc40 ?FUN_1000cc40@C_FUN_1000cc40@f_1000cc40@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10008840(undefined4 *);
struct C_FUN_1000cc40 { undefined4 * FUN_1000cc40(byte param_1); };
undefined4 * C_FUN_1000cc40::FUN_1000cc40(byte param_1)
{
  thunk_FUN_10008840((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1000e120 {
// MATCH: sound.dll 0x1000e120 ?FUN_1000e120@C_FUN_1000e120@f_1000e120@@QAEXH@Z
struct T_thunk_FUN_1000e680 { int thunk_FUN_1000e680(undefined4); };
struct C_FUN_1000e120 { void FUN_1000e120(int param_1); };
void C_FUN_1000e120::FUN_1000e120(int param_1)
{
  ((T_thunk_FUN_1000e680 *)((void *)((int)this + 0x1c8)))->thunk_FUN_1000e680(param_1);
  *(uint *)(param_1 + 200) = *(uint *)(param_1 + 200) | 0x40;
  return;
}
}

namespace f_1000e1d0 {
// MATCH: sound.dll 0x1000e1d0 ?FUN_1000e1d0@C_FUN_1000e1d0@f_1000e1d0@@QAEXH@Z
struct C_FUN_1000e1d0 { void FUN_1000e1d0(int param_1); };
void C_FUN_1000e1d0::FUN_1000e1d0(int param_1)
{
  *(int *)((int)this + 0x1b4) = param_1;
  *(int *)((int)this + 0x1b0) = *(int *)((int)this + 0x1b0) + param_1;
  return;
}
}

namespace f_1000e680 {
// MATCH: sound.dll 0x1000e680 ?FUN_1000e680@C_FUN_1000e680@f_1000e680@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_1000e680 { void FUN_1000e680(undefined4 param_1); };
void C_FUN_1000e680::FUN_1000e680(undefined4 param_1)
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

namespace f_1000e7d0 {
// MATCH: sound.dll 0x1000e7d0 ?FUN_1000e7d0@f_1000e7d0@@YIIH@Z
uint __fastcall FUN_1000e7d0(int param_1)
{
  return *(uint *)(param_1 + 0x1a4) >> 1 & 1;
}
}
