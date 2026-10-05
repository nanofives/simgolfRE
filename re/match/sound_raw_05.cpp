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

namespace f_1002e790 {
// MATCH: sound.dll 0x1002e790 ?FUN_1002e790@C_FUN_1002e790@f_1002e790@@QAEXIE@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(int); };
struct C_FUN_1002e790 { void FUN_1002e790(undefined4 param_1, byte param_2); };
void C_FUN_1002e790::FUN_1002e790(undefined4 param_1, byte param_2)
{
  if ((param_2 & 1) != 0) {
    *(byte *)((int)this + 0x58) = *(byte *)((int)this + 0x58) | 2;
  }
  ((VT_1 *)(this))->f4((int)(param_1));
  return;
}
}

namespace f_1002e8f0 {
// MATCH: sound.dll 0x1002e8f0 ?FUN_1002e8f0@f_1002e8f0@@YIIPAH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); };
undefined4 __fastcall FUN_1002e8f0(int *param_1)
{
  if ((int *)param_1[0x10] != (int *)0x0) {
    ((VT_1 *)(param_1[0x10]))->f5();
  }
  ((VT_2 *)(param_1))->f32();
  param_1[0x10] = 0;
  param_1[0x11] = param_1[0x11] & 0xfffffffe;
  return 0;
}
}

namespace f_1002ea00 {
// MATCH: sound.dll 0x1002ea00 ?FUN_1002ea00@C_FUN_1002ea00@f_1002ea00@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002ea30(undefined4 *);
struct C_FUN_1002ea00 { undefined4 * FUN_1002ea00(byte param_1); };
undefined4 * C_FUN_1002ea00::FUN_1002ea00(byte param_1)
{
  thunk_FUN_1002ea30((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002ebd0 {
// MATCH: sound.dll 0x1002ebd0 ?FUN_1002ebd0@f_1002ebd0@@YIIPAH@Z
undefined4 __cdecl delete_sound(int *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); };
struct VT_3 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); };
undefined4 __fastcall FUN_1002ebd0(int *param_1)
{
  if ((int *)param_1[0x10] != (int *)0x0) {
    if ((((VT_1 *)(param_1[0x10]))->f23()) == 0) {
      delete_sound((int *)param_1[0x10]);
    }
    else {
      ((VT_2 *)(param_1[0x10]))->f14();
    }
    param_1[0x10] = 0;
  }
  ((VT_3 *)(param_1))->f32();
  return 0;
}
}

namespace f_1002ec20 {
// MATCH: sound.dll 0x1002ec20 ?FUN_1002ec20@C_FUN_1002ec20@f_1002ec20@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(int); };
struct C_FUN_1002ec20 { undefined4 FUN_1002ec20(undefined4 param_1); };
undefined4 C_FUN_1002ec20::FUN_1002ec20(undefined4 param_1)
{
  if (*(int **)((int)this + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 0x40)))->f34((int)(param_1));
  }
  return 0;
}
}

namespace f_1002ec50 {
// MATCH: sound.dll 0x1002ec50 ?FUN_1002ec50@f_1002ec50@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); };
undefined4 __fastcall FUN_1002ec50(int param_1)
{
  if (*(int **)(param_1 + 0x40) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0x40)))->f8();
  }
  return 0;
}
}

namespace f_1002eee0 {
// MATCH: sound.dll 0x1002eee0 ?FUN_1002eee0@f_1002eee0@@YIIH@Z
extern int DAT_1002eee9;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); };
undefined4 __fastcall FUN_1002eee0(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x40) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_1002eee9). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f37();
    return uVar1;
  }
  return 0;
}
}

namespace f_1002ef10 {
// MATCH: sound.dll 0x1002ef10 ?FUN_1002ef10@C_FUN_1002ef10@f_1002ef10@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002ef40(undefined4 *);
struct C_FUN_1002ef10 { undefined4 * FUN_1002ef10(byte param_1); };
undefined4 * C_FUN_1002ef10::FUN_1002ef10(byte param_1)
{
  thunk_FUN_1002ef40((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002f0f0 {
// MATCH: sound.dll 0x1002f0f0 ?FUN_1002f0f0@f_1002f0f0@@YIIPAH@Z
undefined4 __cdecl delete_sound(int *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); };
struct VT_3 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); };
undefined4 __fastcall FUN_1002f0f0(int *param_1)
{
  if ((int *)param_1[0x10] != (int *)0x0) {
    if ((((VT_1 *)(param_1[0x10]))->f23()) == 0) {
      delete_sound((int *)param_1[0x10]);
    }
    else {
      ((VT_2 *)(param_1[0x10]))->f14();
    }
    param_1[0x10] = 0;
  }
  ((VT_3 *)(param_1))->f32();
  return 0;
}
}

namespace f_1002f190 {
// MATCH: sound.dll 0x1002f190 ?FUN_1002f190@f_1002f190@@YIIH@Z
extern int DAT_1002f199;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); };
undefined4 __fastcall FUN_1002f190(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x40) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_1002f199). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f5();
    return uVar1;
  }
  return 0x13;
}
}

namespace f_1002f3c0 {
// MATCH: sound.dll 0x1002f3c0 ?FUN_1002f3c0@C_FUN_1002f3c0@f_1002f3c0@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002f3f0(undefined4 *);
struct C_FUN_1002f3c0 { undefined4 * FUN_1002f3c0(byte param_1); };
undefined4 * C_FUN_1002f3c0::FUN_1002f3c0(byte param_1)
{
  thunk_FUN_1002f3f0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002f530 {
// MATCH: sound.dll 0x1002f530 ?FUN_1002f530@f_1002f530@@YIHH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); };
int __fastcall FUN_1002f530(int param_1)
{
    int iVar1;
  if ((*(byte *)(param_1 + 0x58) & 1) == 0) {
    if ((*(int **)(param_1 + 0x40) != (int *)0x0) && (iVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f36(), iVar1 != 0)) {
      return iVar1;
    }
    *(uint *)(param_1 + 0x58) = *(uint *)(param_1 + 0x58) | 1;
  }
  return 0;
}
}

namespace f_1002f570 {
// MATCH: sound.dll 0x1002f570 ?FUN_1002f570@f_1002f570@@YIIH@Z
extern int DAT_1002f57f;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); };
undefined4 __fastcall FUN_1002f570(int param_1)
{
    undefined4 uVar1;
  if (((*(byte *)(param_1 + 0x58) & 1) != 0) && (*(int **)(param_1 + 0x40) != (int *)0x0)) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_1002f57f). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f8();
    return uVar1;
  }
  return 0;
}
}

namespace f_1002f5a0 {
// MATCH: sound.dll 0x1002f5a0 ?FUN_1002f5a0@f_1002f5a0@@YIIPAH@Z
undefined4 __cdecl delete_sound(int *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); };
undefined4 __fastcall FUN_1002f5a0(int *param_1)
{
  ((VT_1 *)(param_1))->f8();
  ((VT_2 *)(param_1))->f14();
  delete_sound((int *)param_1[0x10]);
  param_1[0x10] = 0;
  return 0;
}
}

namespace f_1002f680 {
// MATCH: sound.dll 0x1002f680 ?FUN_1002f680@f_1002f680@@YIIH@Z
extern int DAT_1002f689;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); };
undefined4 __fastcall FUN_1002f680(int param_1)
{
    undefined4 uVar1;
  if (*(int **)(param_1 + 0x40) != (int *)0x0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_1002f689). Too many branches */ /* WARNING: Treating indirect jump as call */ uVar1 = ((VT_1 *)(*(int **)(param_1 + 0x40)))->f37();
    return uVar1;
  }
  return 0;
}
}

namespace f_1002f880 {
// MATCH: sound.dll 0x1002f880 ?FUN_1002f880@C_FUN_1002f880@f_1002f880@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_1002cb00(undefined4 *);
struct C_FUN_1002f880 { undefined4 * FUN_1002f880(byte param_1); };
undefined4 * C_FUN_1002f880::FUN_1002f880(byte param_1)
{
  thunk_FUN_1002cb00((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_1002fb40 {
// MATCH: sound.dll 0x1002fb40 ?FUN_1002fb40@f_1002fb40@@YIIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); };
undefined4 __fastcall FUN_1002fb40(int param_1)
{
  if (*(int **)(param_1 + 0xf8) != (int *)0x0) {
    ((VT_1 *)(*(int **)(param_1 + 0xf8)))->f8();
  }
  return 0xb;
}
}

namespace f_1002ffa0 {
// MATCH: sound.dll 0x1002ffa0 ?FUN_1002ffa0@f_1002ffa0@@YIXH@Z
void __fastcall FUN_1002ffa0(int param_1)
{
  *(DWORD *)(param_1 + 0x100) = (timeGetTime());
  return;
}
}

namespace f_1002ffd0 {
// MATCH: sound.dll 0x1002ffd0 ?FUN_1002ffd0@f_1002ffd0@@YIXH@Z
void __fastcall FUN_1002ffd0(int param_1)
{
  *(DWORD *)(param_1 + 0x104) = (timeGetTime());
  return;
}
}

namespace f_10030150 {
// MATCH: sound.dll 0x10030150 ?FUN_10030150@C_FUN_10030150@f_10030150@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_10030180(undefined4 *);
struct C_FUN_10030150 { undefined4 * FUN_10030150(byte param_1); };
undefined4 * C_FUN_10030150::FUN_10030150(byte param_1)
{
  thunk_FUN_10030180((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_100311b0 {
// MATCH: sound.dll 0x100311b0 ?FUN_100311b0@f_100311b0@@YIIH@Z
undefined4 __fastcall FUN_100311b0(int param_1)
{
  return *(undefined4 *)(param_1 + 0xb4);
}
}

namespace f_10031270 {
// MATCH: sound.dll 0x10031270 ?FUN_10031270@C_FUN_10031270@f_10031270@@QAEPAIE@Z
int __cdecl FUN_1004249a(undefined *);
int __fastcall thunk_FUN_100312a0(undefined4 *);
struct C_FUN_10031270 { undefined4 * FUN_10031270(byte param_1); };
undefined4 * C_FUN_10031270::FUN_10031270(byte param_1)
{
  thunk_FUN_100312a0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_1004249a((unsigned char *)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_10031b60 {
// MATCH: sound.dll 0x10031b60 ?FUN_10031b60@C_FUN_10031b60@f_10031b60@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_10031b60 { void FUN_10031b60(undefined4 param_1); };
void C_FUN_10031b60::FUN_10031b60(undefined4 param_1)
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

namespace f_10032410 {
// MATCH: sound.dll 0x10032410 ?FUN_10032410@C_FUN_10032410@f_10032410@@QAEXH@Z
struct T_thunk_FUN_10038430 { int thunk_FUN_10038430(undefined4); };
struct C_FUN_10032410 { void FUN_10032410(int param_1); };
void C_FUN_10032410::FUN_10032410(int param_1)
{
  if (param_1 != 0) {
    *(void **)(param_1 + 0x20) = this;
    ((T_thunk_FUN_10038430 *)((void *)((int)this + 0x1338)))->thunk_FUN_10038430(param_1);
  }
  return;
}
}

namespace f_10032640 {
// MATCH: sound.dll 0x10032640 ?FUN_10032640@f_10032640@@YIIH@Z
undefined4 __fastcall thunk_FUN_1000f760(int);
undefined4 __fastcall FUN_10032640(int param_1)
{
  if (*(int *)(param_1 + 0x54) != 0) {
    thunk_FUN_1000f760(*(int *)(param_1 + 0x54));
  }
  return 0;
}
}

namespace f_10033990 {
// MATCH: sound.dll 0x10033990 ?FUN_10033990@f_10033990@@YIXPAX@Z
struct T_thunk_FUN_100326a0 { undefined4 * thunk_FUN_100326a0(uint *); };
void __fastcall FUN_10033990(void *param_1)
{
  *(uint *)((int)param_1 + 0x58) = *(uint *)((int)param_1 + 0x58) & 0xfffffffd;
  ((T_thunk_FUN_100326a0 *)(param_1))->thunk_FUN_100326a0((uint *)((int)param_1 + 100));
  return;
}
}

namespace f_10034db0 {
// MATCH: sound.dll 0x10034db0 ?FUN_10034db0@C_FUN_10034db0@f_10034db0@@QAEII@Z
struct T_thunk_FUN_10035460 { int thunk_FUN_10035460(uint); };
struct C_FUN_10034db0 { undefined4 FUN_10034db0(uint param_1); };
undefined4 C_FUN_10034db0::FUN_10034db0(uint param_1)
{
  ((T_thunk_FUN_10035460 *)(this))->thunk_FUN_10035460(param_1);
  *(uint *)((int)this + 0x58) = *(uint *)((int)this + 0x58) | 8;
  return 0;
}
}

namespace f_10034fb0 {
// MATCH: sound.dll 0x10034fb0 ?FUN_10034fb0@f_10034fb0@@YIIPAI@Z
undefined4 __fastcall thunk_FUN_10033300(undefined4 *);
undefined4 __fastcall FUN_10034fb0(undefined4 *param_1)
{
  if (((param_1[0x16] & 2) != 0) && ((param_1[0x16] & 0x21) == 0)) {
    thunk_FUN_10033300(param_1);
  }
  return 0;
}
}

namespace f_10034fe0 {
// MATCH: sound.dll 0x10034fe0 ?FUN_10034fe0@C_FUN_10034fe0@f_10034fe0@@QAEIPAII@Z
undefined4 * __cdecl FID_conflict__memcpy(undefined4 *, undefined4 *, uint);
struct C_FUN_10034fe0 { uint FUN_10034fe0(undefined4 *param_1, uint param_2); };
uint C_FUN_10034fe0::FUN_10034fe0(undefined4 *param_1, uint param_2)
{
    uint uVar1;
    _MMIOINFO local_48;
  mmioGetInfo(*(HMMIO *)this,&local_48,0);
  uVar1 = (int)local_48.pchEndRead - (int)local_48.pchNext;
  if (uVar1 == 0) {
    return 0;
  }
  if (param_2 > uVar1) {
    param_2 = uVar1;
  }
  FID_conflict__memcpy(param_1,(undefined4 *)local_48.pchNext,param_2);
  local_48.pchNext = local_48.pchNext + param_2;
  mmioSetInfo(*(HMMIO *)this,&local_48,0);
  return param_2;
}
}

namespace f_100350e0 {
// MATCH: sound.dll 0x100350e0 ?FUN_100350e0@f_100350e0@@YIIPAI@Z
undefined4 __fastcall FUN_100350e0(undefined4 *param_1)
{
  param_1[0x16] = param_1[0x16] & 0xfffffffd;
  mmioSeek((HMMIO)*param_1,(uint)*(ushort *)(param_1 + 99) * param_1[0x59] + param_1[0x5d],0);
  mmioAdvance((HMMIO)*param_1,(LPMMIOINFO)(param_1 + 3),0);
  return 0;
}
}

namespace f_10035750 {
// MATCH: sound.dll 0x10035750 ?FUN_10035750@C_FUN_10035750@f_10035750@@QAEXI@Z
struct C_FUN_10035750 { void FUN_10035750(undefined4 param_1); };
void C_FUN_10035750::FUN_10035750(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x220) = param_1;
  return;
}
}

namespace f_10037720 {
// MATCH: sound.dll 0x10037720 ?FUN_10037720@C_FUN_10037720@f_10037720@@QAEIII@Z
struct C_FUN_10037720 { undefined4 FUN_10037720(undefined4 param_1, undefined4 param_2); };
undefined4 C_FUN_10037720::FUN_10037720(undefined4 param_1, undefined4 param_2)
{
  *(undefined4 *)((int)this + 0x13a0) = param_1;
  *(undefined4 *)((int)this + 0x13a4) = param_2;
  *(uint *)((int)this + 0x58) = *(uint *)((int)this + 0x58) & 0xfffffffb;
  return 0;
}
}

namespace f_10037b40 {
// MATCH: sound.dll 0x10037b40 ?FUN_10037b40@C_FUN_10037b40@f_10037b40@@QAEII@Z
struct T_thunk_FUN_10011070 { undefined4 thunk_FUN_10011070(undefined4); };
struct C_FUN_10037b40 { undefined4 FUN_10037b40(undefined4 param_1); };
undefined4 C_FUN_10037b40::FUN_10037b40(undefined4 param_1)
{
  if (((*(byte *)((int)this + 0x5c) & 0x20) != 0) && (*(void **)((int)this + 0x54) != (void *)0x0)) {
    return (((T_thunk_FUN_10011070 *)(*(void **)((int)this + 0x54)))->thunk_FUN_10011070(param_1));
  }
  return 0xb;
}
}

namespace f_10037b80 {
// MATCH: sound.dll 0x10037b80 ?FUN_10037b80@C_FUN_10037b80@f_10037b80@@QAEII@Z
struct T_thunk_FUN_100110c0 { undefined4 thunk_FUN_100110c0(undefined4); };
struct C_FUN_10037b80 { undefined4 FUN_10037b80(undefined4 param_1); };
undefined4 C_FUN_10037b80::FUN_10037b80(undefined4 param_1)
{
  if (((*(byte *)((int)this + 0x5c) & 0x20) != 0) && (*(void **)((int)this + 0x54) != (void *)0x0)) {
    return (((T_thunk_FUN_100110c0 *)(*(void **)((int)this + 0x54)))->thunk_FUN_100110c0(param_1));
  }
  return 0xb;
}
}

namespace f_10037bc0 {
// MATCH: sound.dll 0x10037bc0 ?FUN_10037bc0@C_FUN_10037bc0@f_10037bc0@@QAEII@Z
struct T_thunk_FUN_10011110 { undefined4 thunk_FUN_10011110(undefined4); };
struct C_FUN_10037bc0 { undefined4 FUN_10037bc0(undefined4 param_1); };
undefined4 C_FUN_10037bc0::FUN_10037bc0(undefined4 param_1)
{
  if (((*(byte *)((int)this + 0x5c) & 0x20) != 0) && (*(void **)((int)this + 0x54) != (void *)0x0)) {
    return (((T_thunk_FUN_10011110 *)(*(void **)((int)this + 0x54)))->thunk_FUN_10011110(param_1));
  }
  return 0xb;
}
}

namespace f_100381f0 {
// MATCH: sound.dll 0x100381f0 ?FUN_100381f0@f_100381f0@@YIXH@Z
extern double _DAT_1005b498;
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ void __fastcall FUN_100381f0(int param_1)
{
  if ((*(double *)(param_1 + 0x58) != _DAT_1005b498) && (*(double *)(param_1 + 0xf0) != _DAT_1005b498)) {
    *(byte *)(param_1 + 0x9c) = *(byte *)(param_1 + 0x9c) | 8;
    return;
  }
  *(undefined4 *)(param_1 + 0xe4) = 0;
  *(byte *)(param_1 + 0x9c) = *(byte *)(param_1 + 0x9c) | 4;
  *(byte *)(param_1 + 0x94) = *(byte *)(param_1 + 0x94) & 0xfe;
  return;
}
}

namespace f_100383b0 {
// MATCH: sound.dll 0x100383b0 ?FUN_100383b0@C_FUN_100383b0@f_100383b0@@QAEXI@Z
extern double _DAT_1005b488;
struct T_thunk_FUN_10037f30 { int thunk_FUN_10037f30(int); };
struct C_FUN_100383b0 { /* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ void FUN_100383b0(uint param_1); };
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ void C_FUN_100383b0::FUN_100383b0(uint param_1)
{
  *(double *)((int)this + 0x58) = (double)param_1 * _DAT_1005b488;
  if (param_1 != 0) {
    param_1 = param_1 + 1;
  }
  *(uint *)((int)this + 0x50) = param_1 << 9;
  ((T_thunk_FUN_10037f30 *)(this))->thunk_FUN_10037f30(*(int *)((int)this + 0x48));
  return;
}
}

namespace f_10038400 {
// MATCH: sound.dll 0x10038400 ?FUN_10038400@f_10038400@@YIXH@Z
void __fastcall FUN_10038400(int param_1)
{
  if (*(undefined4 **)(param_1 + 0x13a4) != (undefined4 *)0x0) {
    **(undefined4 **)(param_1 + 0x13a4) = 0;
    *(undefined4 *)(param_1 + 0x13a4) = 0;
  }
  return;
}
}

namespace f_10038430 {
// MATCH: sound.dll 0x10038430 ?FUN_10038430@C_FUN_10038430@f_10038430@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_10038430 { void FUN_10038430(undefined4 param_1); };
void C_FUN_10038430::FUN_10038430(undefined4 param_1)
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

namespace f_10038550 {
// MATCH: sound.dll 0x10038550 ?FUN_10038550@C_FUN_10038550@f_10038550@@QAEXI@Z
void * __cdecl operator_new(uint);
struct C_FUN_10038550 { void FUN_10038550(undefined4 param_1); };
void C_FUN_10038550::FUN_10038550(undefined4 param_1)
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
