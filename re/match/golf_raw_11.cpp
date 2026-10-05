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

namespace f_0040bbf0 {
// MATCH: golf_clean.exe 0x0040bbf0 ?FUN_0040bbf0@f_0040bbf0@@YAXI@Z
extern int DAT_004c284c;
extern int DAT_004c2c94;
extern int DAT_0053a450;
extern int DAT_0053bbac;
extern int DAT_0053caf0;
extern int DAT_0053e638;
extern int DAT_005419d0;
extern int DAT_00542414;
extern int DAT_00543cfc;
extern int DAT_00561250;
extern int DAT_00561258;
extern int DAT_005685f8;
extern int DAT_005689e8;
extern int DAT_0056d1b8;
extern int DAT_0056fcb0;
extern int DAT_00571d38;
extern int DAT_00571fd8;
extern int DAT_00572cac;
extern int DAT_00572cb0;
extern int DAT_00575ab0;
extern int DAT_005787cc;
extern int DAT_00585850;
extern int DAT_0059aaf8;
extern int DAT_0059ae7c;
extern int DAT_0059ae80;
extern int DAT_0059b730;
extern int DAT_0059bf90;
extern int DAT_0059c08c;
extern int DAT_0059d81c;
extern int DAT_0059dea0;
extern int DAT_005a46b8;
extern int DAT_005a4998;
extern int DAT_005a5a24;
extern int DAT_005a6364;
extern int DAT_005a9cac;
extern int DAT_005a9cb0;
extern int DAT_00822c70;
extern int DAT_00822c78;
extern int DAT_00822c88;
extern int DAT_00838c1c;
extern int g_accuracy_skill_x100;
extern int g_cash_div100;
extern int g_course_type;
extern int g_date;
extern int g_flags;
extern int g_fun_rating;
extern int g_golfer_types;
extern int g_holes_plus1;
extern int g_imagination_x100;
extern int g_length_skill_x100;
extern int g_par;
extern int g_placed_objects;
extern int g_records;
extern int g_skill_rating_x100;
extern int g_theme;
extern int g_tile_byte;
extern int g_tile_type;
extern int g_yards;
void __cdecl FUN_0040af70(undefined4, undefined4);
void __cdecl FUN_0040bbf0(undefined4 param_1)
{
  DAT_0053e638 = param_1;
  FUN_0040af70((unsigned int)(((char *)&DAT_00822c78)),4);
  FUN_0040af70((unsigned int)(((char *)&g_holes_plus1)),4);
  FUN_0040af70((unsigned int)(((char *)&g_yards)),4);
  FUN_0040af70((unsigned int)(((char *)&g_par)),4);
  FUN_0040af70((unsigned int)(((char *)&g_course_type)),1);
  FUN_0040af70((unsigned int)(((char *)&g_fun_rating)),4);
  FUN_0040af70((unsigned int)(((char *)&g_skill_rating_x100)),4);
  FUN_0040af70((unsigned int)(((char *)&g_length_skill_x100)),4);
  FUN_0040af70((unsigned int)(((char *)&g_accuracy_skill_x100)),4);
  FUN_0040af70((unsigned int)(((char *)&g_imagination_x100)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_005a9cac)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_005a9cb0)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_005787cc)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_00561258)),4);
  FUN_0040af70((unsigned int)(((char *)&g_cash_div100)),4);
  FUN_0040af70((unsigned int)(((char *)&g_records)),0x28);
  FUN_0040af70((unsigned int)(((char *)&DAT_0059bf90)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_00575ab0)),0x28a0);
  FUN_0040af70((unsigned int)(((char *)&g_tile_type)),0x9c4);
  FUN_0040af70((unsigned int)(((char *)&g_tile_byte)),0x9c4);
  FUN_0040af70((unsigned int)(((char *)&DAT_0053caf0)),5000);
  FUN_0040af70((unsigned int)(((char *)&DAT_0053bbac)),0x9c4);
  FUN_0040af70((unsigned int)(((char *)&DAT_005a4998)),0xa29);
  FUN_0040af70((unsigned int)(((char *)&DAT_00838c1c)),0x169);
  FUN_0040af70((unsigned int)(((char *)&g_placed_objects)),0x1000);
  FUN_0040af70((unsigned int)(((char *)&DAT_004c284c)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_00561250)),4);
  FUN_0040af70((unsigned int)(((char *)&g_date)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_005a6364)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_0053a450)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_00822c88)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_0059b730)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_0059ae7c)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_0059dea0)),0x80);
  FUN_0040af70((unsigned int)(((char *)&DAT_0059c08c)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_00822c70)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_005685f8)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_005a5a24)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_00543cfc)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_00572cac)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_0059aaf8)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_0059ae80)),0x1c8);
  FUN_0040af70((unsigned int)(((char *)&DAT_0056d1b8)),6000);
  FUN_0040af70((unsigned int)(((char *)&DAT_004c2c94)),0x12);
  FUN_0040af70((unsigned int)(((char *)&DAT_005689e8)),800);
  FUN_0040af70((unsigned int)(((char *)&DAT_005419d0)),0x40);
  FUN_0040af70((unsigned int)(((char *)&DAT_00572cb0)),0xa00);
  FUN_0040af70((unsigned int)(((char *)&g_golfer_types)),0xa640);
  FUN_0040af70((unsigned int)(((char *)&DAT_00542414)),0x9c4);
  FUN_0040af70((unsigned int)(((char *)&g_flags)),4);
  FUN_0040af70((unsigned int)(((char *)&g_theme)),100);
  FUN_0040af70((unsigned int)(((char *)&DAT_00571d38)),4);
  FUN_0040af70((unsigned int)(((char *)&DAT_0056fcb0)),0x1002);
  FUN_0040af70((unsigned int)(((char *)&DAT_0059d81c)),0x100);
  FUN_0040af70((unsigned int)(((char *)&DAT_005a46b8)),0x100);
  FUN_0040af70((unsigned int)(((char *)&DAT_00585850)),0x1300);
  FUN_0040af70((unsigned int)(((char *)&DAT_00571fd8)),0x30e);
  return;
}
}

namespace f_0040cdd0 {
// MATCH: golf_clean.exe 0x0040cdd0 ?FUN_0040cdd0@f_0040cdd0@@YAXHHIIH@Z
extern int DAT_0058b890;
extern int DAT_0058b8bc;
extern int DAT_0058b8e8;
extern int DAT_005a55d8;
extern int DAT_005a5604;
extern int DAT_005a5630;
extern int DAT_0083ad50;
extern void *PTR_DAT_004c1570;
void __cdecl FUN_0040ca10(int, int, int, int, int);
struct T_FUN_00473f60 { undefined4 FUN_00473f60(int, int, undefined4, undefined4, int); };
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(int, int, int); };
void __cdecl FUN_0040cdd0(int param_1, int param_2, uint param_3, uint param_4, int param_5)
{
    uint uVar1;
    int iVar2;
  if (DAT_0083ad50 != 0) {
    ((VT_1 *)(DAT_0083ad50))->f39((int)(1), (int)(1), (int)(1));
  }
  uVar1 = param_3 & 0xf;
  if (uVar1 != 0) {
    param_3 = param_3 + (0xf - uVar1);
    param_1 = param_1 - (int)(0xf - uVar1) / 2;
  }
  uVar1 = param_4 & 0xf;
  if (uVar1 != 0) {
    param_4 = param_4 + (0xf - uVar1);
    param_2 = param_2 - (int)(0xf - uVar1) / 2;
  }
  if (param_5 != 0) {
    FUN_0040ca10(param_1,param_2 + -1,param_3,param_4,0);
  }
  iVar2 = 0xc;
  if (0xc < (int)(param_3 - 0x10)) {
    do {
      ((T_FUN_00473f60 *)(((char *)&DAT_005a5604)))->FUN_00473f60((int)(((char *)&DAT_0058b8bc)), (int)PTR_DAT_004c1570, iVar2 + param_1, param_2 + -0x10 + param_4, 0);
      iVar2 = iVar2 + 0x10;
    } while (iVar2 < (int)(param_3 - 0x10));
  }
  iVar2 = param_2 + -0x10 + param_4;
  ((T_FUN_00473f60 *)(((char *)&DAT_005a5630)))->FUN_00473f60((int)(((char *)&DAT_0058b8e8)), (int)PTR_DAT_004c1570, param_1 + -0xc + param_3, iVar2, 0);
  ((T_FUN_00473f60 *)(((char *)&DAT_005a55d8)))->FUN_00473f60((int)(((char *)&DAT_0058b890)), (int)PTR_DAT_004c1570, param_1 + -4, iVar2, 0);
  return;
}
}

namespace f_00456bb0 {
// MATCH: golf_clean.exe 0x00456bb0 ?FUN_00456bb0@f_00456bb0@@YADHH@Z
extern int DAT_00578372;
extern int g_tile_type;
char __cdecl FUN_00456bb0(int param_1, int param_2)
{
  return ('\0' < (char)(((char *)&DAT_00578372))[(char)(((char *)&g_tile_type))[param_1 * 0x32 + param_2] * 0x30]) + '\x02';
}
}

namespace f_00473ae0 {
// MATCH: golf_clean.exe 0x00473ae0 ?FUN_00473ae0@f_00473ae0@@YIXH@Z
extern int DAT_0083ad50;
void __fastcall FUN_00483010(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
void __cdecl _free(LPVOID);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(int); };
void __fastcall FUN_00473ae0(int param_1)
{
    undefined4 * puVar1;
  if (DAT_0083ad50 != 0) {
    if (*(int *)(param_1 + 4) != 0) {
      ((VT_1 *)(DAT_0083ad50))->f37((int)(*(int *)(param_1 + 4)));
    }
    if (*(LPVOID *)(param_1 + 8) != (LPVOID)0x0) {
      _free(*(LPVOID *)(param_1 + 8));
      *(undefined4 *)(param_1 + 8) = 0;
    }
    puVar1 = *(undefined4 **)(param_1 + 0xc);
    if (puVar1 != 0) {
      FUN_00483010(puVar1);
      FUN_004a4ffc((unsigned int)(puVar1));
      *(undefined4 *)(param_1 + 0xc) = 0;
    }
    *(undefined4 *)(param_1 + 0x10) = 0;
    *(undefined4 *)(param_1 + 0x14) = 0;
    *(undefined4 *)(param_1 + 0x18) = 0;
    *(undefined4 *)(param_1 + 0x1c) = 0;
    *(undefined4 *)(param_1 + 0x20) = 0;
    *(undefined4 *)(param_1 + 0x24) = 0;
  }
  return;
}
}

namespace f_00478750 {
// MATCH: golf_clean.exe 0x00478750 ?FUN_00478750@C_FUN_00478750@f_00478750@@QAEXPADIII@Z
extern char DAT_00839658;
extern int DAT_00839a9c;
extern int DAT_00839aa0;
extern int DAT_00839aa4;
extern int DAT_00839aa8;
struct T_FUN_004787a0 { int FUN_004787a0(char *); };
struct C_FUN_00478750 { void FUN_00478750(char *param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_00478750::FUN_00478750(char *param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  *(undefined4 *)((int)this + 0x1c) = 0;
  *(undefined4 *)((int)this + 0x18) = *(undefined4 *)((int)this + 0x14);
  DAT_00839a9c = param_2;
  DAT_00839aa8 = 0;
  DAT_00839aa0 = param_3;
  DAT_00839aa4 = param_4;
  *(undefined4 *)((int)this + 0x3c) = 0;
  (*(unsigned char *)((char *)((char *)&DAT_00839658) + 0)) = 0;
  ((T_FUN_004787a0 *)(this))->FUN_004787a0(param_1);
  return;
}
}

namespace f_0047b820 {
// MATCH: golf_clean.exe 0x0047b820 ?FUN_0047b820@f_0047b820@@YIXPAH@Z
extern int DAT_0083aa98;
extern int DAT_0083aac0;
extern int DAT_0083ab30;
extern int DAT_0083ab34;
extern int DAT_0083ab40;
extern int DAT_0083ab44;
extern int DAT_0083ab54;
extern int DAT_0083ad50;
void LAB_0047b87f();
void __fastcall FUN_00479b20(int);
struct T_FUN_0047b080 { undefined4 FUN_0047b080(int); };
void __stdcall FUN_0047e450();
void __stdcall FUN_00480360();
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); };
struct VT_3 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); };
struct VT_4 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(int); };
struct VT_5 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); };
void __fastcall FUN_0047b820(int *param_1)
{
    int * piVar1;
  ((VT_1 *)(param_1))->f62();
  if ((int)(DAT_0083aa98) == (int)(param_1)) {
    FUN_00479b20((int)param_1);
  }
  if ((int)(DAT_0083aac0) == (int)(param_1)) {
    FUN_00480360();
  }
  piVar1 = (int *)(DAT_0083ab40);
  if (DAT_0083ab40 == 0) {
    piVar1 = (int *)(DAT_0083ab44);
  }
  if (piVar1 != param_1) {
    piVar1 = (int *)(DAT_0083ab40);
    if (DAT_0083ab40 == 0) {
      piVar1 = (int *)(DAT_0083ab44);
    }
    if ((((T_FUN_0047b080 *)(param_1))->FUN_0047b080((int)piVar1)) == 0) goto LAB_0047b87f;
  }
  DAT_0083ab40 = 0;
  DAT_0083ab44 = 0;
LAB_0047b87f:
  if ((int)(DAT_0083ab30) == (int)(param_1)) {
    DAT_0083ab30 = 0;
    ((VT_2 *)(param_1))->f4();
  }
  if ((int)(DAT_0083ab34) == (int)(param_1)) {
    DAT_0083ab34 = 0;
  }
  if ((param_1[0x28] & 1U) != 0) {
    param_1[0x28] = param_1[0x28] & 0xfffffffe;
    if ((int)(DAT_0083ab54) == (int)(param_1)) {
      ((VT_3 *)(param_1))->f68();
    }
    FUN_0047e450();
    if (DAT_0083ad50 != 0) {
      ((VT_4 *)(DAT_0083ad50))->f12((int)(0));
    }
    if ((int *)param_1[4] != 0) {
      ((VT_5 *)(param_1[4]))->f7();
    }
  }
  return;
}
}

namespace f_0047d010 {
// MATCH: golf_clean.exe 0x0047d010 ?FUN_0047d010@f_0047d010@@YAXXZ
extern int DAT_0047d01c;
extern int DAT_0083ad50;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); };
void __cdecl FUN_0047d010()
{
  if (DAT_0083ad50 != 0) {
                    /* WARNING: Could not recover jumptable at ((char *)&DAT_0047d01c). Too many branches */ /* WARNING: Treating indirect jump as call */ ((VT_1 *)(DAT_0083ad50))->f2();
    return;
  }
  return;
}
}

namespace f_0047d510 {
// MATCH: golf_clean.exe 0x0047d510 ?FUN_0047d510@f_0047d510@@YIXH@Z
struct VT_1 {  virtual int f0(int); };
struct VT_2 {  virtual int f0(int); };
struct VT_3 {  virtual int f0(int); };
void __fastcall FUN_0047d510(int param_1)
{
  if (*(undefined4 **)(param_1 + 0x150) != 0) {
    ((VT_1 *)(*(undefined4 **)(param_1 + 0x150)))->f0((int)(1));
    *(undefined4 *)(param_1 + 0x150) = 0;
  }
  if (*(undefined4 **)(param_1 + 0x154) != 0) {
    ((VT_2 *)(*(undefined4 **)(param_1 + 0x154)))->f0((int)(1));
    *(undefined4 *)(param_1 + 0x154) = 0;
  }
  if (*(undefined4 **)(param_1 + 0x158) != 0) {
    ((VT_3 *)(*(undefined4 **)(param_1 + 0x158)))->f0((int)(1));
    *(undefined4 *)(param_1 + 0x158) = 0;
  }
  return;
}
}

namespace f_0047e2d0 {
// MATCH: golf_clean.exe 0x0047e2d0 ?FUN_0047e2d0@f_0047e2d0@@YAXXZ
extern int DAT_0083a2d8;
extern int DAT_0083a7bc;
extern int DAT_0083ab94;
void __cdecl FUN_0047e140(int, RECT *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(int); };
void __cdecl FUN_0047e2d0()
{
    int iVar1;
    int * piVar2;
    RECT local_10;
  if (DAT_0083a7bc != 0) {
    ((VT_1 *)(DAT_0083a7bc))->f52((int)(&local_10));
  }
  iVar1 = 0;
  if (0 < DAT_0083ab94) {
    piVar2 = (int *)(((char *)&DAT_0083a2d8));
    do {
      if ((*(byte *)(*piVar2 + 0xa0) & 1) != 0) {
        FUN_0047e140(*piVar2,&local_10);
      }
      iVar1 = iVar1 + 1;
      piVar2 = piVar2 + 1;
    } while (iVar1 < DAT_0083ab94);
  }
  return;
}
}

namespace f_00481b20 {
// MATCH: golf_clean.exe 0x00481b20 ?FUN_00481b20@f_00481b20@@YAXXZ
extern int DAT_0083ac10;
void __cdecl _free(LPVOID);
void __cdecl FUN_00481b20()
{
  if ((int)(DAT_0083ac10) != (int)((LPVOID)0x0)) {
    _free((void *)(DAT_0083ac10));
    DAT_0083ac10 = (int)((LPVOID)0x0);
  }
  return;
}
}

namespace f_00481ba0 {
// MATCH: golf_clean.exe 0x00481ba0 ?FUN_00481ba0@f_00481ba0@@YIXH@Z
void __fastcall FUN_00483010(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct VT_1 {  virtual int f0(int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(int); };
struct VT_3 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(int); };
struct VT_4 {  virtual int f0(int); };
struct VT_5 {  virtual int f0(int); };
void __fastcall FUN_00481ba0(int param_1)
{
    undefined4 * puVar1;
  *(undefined4 *)(param_1 + 0x74) = 0;
  if (*(undefined4 **)(param_1 + 0x54) != 0) {
    ((VT_1 *)(*(undefined4 **)(param_1 + 0x54)))->f0((int)(1));
    *(undefined4 *)(param_1 + 0x54) = 0;
    *(undefined4 *)(param_1 + 0x44) = 0;
  }
  if (*(int **)(param_1 + 0x6c) != 0) {
    ((VT_2 *)(*(int **)(param_1 + 0x6c)))->f3((int)(1));
    *(undefined4 *)(param_1 + 0x6c) = 0;
  }
  if (*(int **)(param_1 + 0x70) != 0) {
    ((VT_3 *)(*(int **)(param_1 + 0x70)))->f3((int)(1));
    *(undefined4 *)(param_1 + 0x70) = 0;
  }
  puVar1 = *(undefined4 **)(param_1 + 100);
  if (puVar1 != 0) {
    FUN_00483010(puVar1);
    FUN_004a4ffc((unsigned int)(puVar1));
    *(undefined4 *)(param_1 + 100) = 0;
  }
  puVar1 = *(undefined4 **)(param_1 + 0x68);
  if (puVar1 != 0) {
    FUN_00483010(puVar1);
    FUN_004a4ffc((unsigned int)(puVar1));
    *(undefined4 *)(param_1 + 0x68) = 0;
  }
  if (*(undefined4 **)(param_1 + 0x5c) != 0) {
    ((VT_4 *)(*(undefined4 **)(param_1 + 0x5c)))->f0((int)(1));
    *(undefined4 *)(param_1 + 0x5c) = 0;
  }
  if (*(undefined4 **)(param_1 + 0x60) != 0) {
    ((VT_5 *)(*(undefined4 **)(param_1 + 0x60)))->f0((int)(1));
    *(undefined4 *)(param_1 + 0x60) = 0;
  }
  puVar1 = *(undefined4 **)(param_1 + 0x78);
  if (puVar1 != 0) {
    FUN_00483010(puVar1);
    FUN_004a4ffc((unsigned int)(puVar1));
    *(undefined4 *)(param_1 + 0x78) = 0;
  }
  if (*(int *)(param_1 + 0x44) != 0) {
    FUN_004a4ffc(*(int *)(param_1 + 0x44));
    *(undefined4 *)(param_1 + 0x44) = 0;
  }
  if (*(int *)(param_1 + 0x48) != 0) {
    FUN_004a4ffc(*(int *)(param_1 + 0x48));
    *(undefined4 *)(param_1 + 0x48) = 0;
  }
  if (*(int *)(param_1 + 0x4c) != 0) {
    FUN_004a4ffc(*(int *)(param_1 + 0x4c));
    *(undefined4 *)(param_1 + 0x4c) = 0;
  }
  *(undefined4 *)(param_1 + 0x50) = 0;
  *(undefined4 *)(param_1 + 0x58) = 0;
  *(undefined4 *)(param_1 + 0x7c) = 0;
  return;
}
}

namespace f_00483b10 {
// MATCH: golf_clean.exe 0x00483b10 ?FUN_00483b10@f_00483b10@@YAXXZ
extern int DAT_0083ad44;
extern int DAT_0083ad50;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); };
void __cdecl FUN_00483b10()
{
  if (DAT_0083ad50 != 0) {
    ((VT_1 *)(DAT_0083ad50))->f47();
  }
  DAT_0083ad44 = 0;
  return;
}
}

namespace f_00484060 {
// MATCH: golf_clean.exe 0x00484060 ?FUN_00484060@f_00484060@@YAIXZ
extern int DAT_0083afc0;
extern int DAT_0083afc4;
void __stdcall FUN_00484130();
undefined4 __cdecl FUN_00484060()
{
  DAT_0083afc4 = 0;
  if ((int)(DAT_0083afc0) != (int)((HMODULE)0x0)) {
    FreeLibrary((struct HINSTANCE__ *)(DAT_0083afc0));
    DAT_0083afc0 = (int)((HMODULE)0x0);
  }
  FUN_00484130();
  return 0;
}
}

namespace f_004884b0 {
// MATCH: golf_clean.exe 0x004884b0 ?FUN_004884b0@f_004884b0@@YIXH@Z
struct VT_1 {  virtual int f0(int); };
void __fastcall FUN_004884b0(int param_1)
{
  if (*(HCURSOR *)(param_1 + 4) != (HCURSOR)0x0) {
    DestroyCursor(*(HCURSOR *)(param_1 + 4));
    *(undefined4 *)(param_1 + 4) = 0;
  }
  if (*(undefined4 **)(param_1 + 8) != 0) {
    ((VT_1 *)(*(undefined4 **)(param_1 + 8)))->f0((int)(1));
    *(undefined4 *)(param_1 + 8) = 0;
  }
  *(undefined4 *)(param_1 + 0xc) = 0;
  *(undefined4 *)(param_1 + 0x10) = 0;
  return;
}
}

namespace f_00490c30 {
// MATCH: golf_clean.exe 0x00490c30 ?FUN_00490c30@f_00490c30@@YAXXZ
extern int DAT_0083b994;
extern int DAT_0083b998;
extern int DAT_0083b9b4;
extern int DAT_0083b9c4;
void __cdecl _free(LPVOID);
void __cdecl FUN_00490c30()
{
  DAT_0083b9b4 = 0;
  DAT_0083b9c4 = 0;
  if ((int)(DAT_0083b994) != (int)((LPVOID)0x0)) {
    _free((void *)(DAT_0083b994));
    DAT_0083b994 = (int)((LPVOID)0x0);
  }
  if ((int)(DAT_0083b998) != (int)((LPVOID)0x0)) {
    _free((void *)(DAT_0083b998));
    DAT_0083b998 = (int)((LPVOID)0x0);
  }
  return;
}
}

namespace f_004a0060 {
// MATCH: golf_clean.exe 0x004a0060 ?FUN_004a0060@f_004a0060@@YAXXZ
extern int DAT_0083ad50;
extern int DAT_0084092c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); };
void __cdecl FUN_004a0060()
{
  if (DAT_0083ad50 != 0) {
    ((VT_1 *)(DAT_0083ad50))->f3();
  }
  if ((int)(DAT_0084092c) != (int)((HMODULE)0x0)) {
    FreeLibrary((struct HINSTANCE__ *)(DAT_0084092c));
    DAT_0084092c = (int)((HMODULE)0x0);
  }
  DAT_0083ad50 = 0;
  return;
}
}

namespace f_004a00a0 {
// MATCH: golf_clean.exe 0x004a00a0 ?FUN_004a00a0@f_004a00a0@@YAHPBD@Z
extern int DAT_0083ad50;
extern int DAT_0084092c;
extern char s_004e4a2c[];
void __stdcall FUN_004a0060();
int __cdecl FUN_004a00a0(LPCSTR param_1)
{
    FARPROC pFVar1;
    int iVar2;
  if (param_1 == (LPCSTR)0x0) {
    return 0;
  }
  DAT_0084092c = (int)(LoadLibraryA(param_1));
  if ((int)(DAT_0084092c) == (int)((HMODULE)0x0)) {
    return 0;
  }
  pFVar1 = GetProcAddress((struct HINSTANCE__ *)(DAT_0084092c),s_004e4a2c);
  if (pFVar1 == (FARPROC)0x0) {
    FUN_004a0060();
    return 0;
  }
  iVar2 = (*pFVar1)();
  if (iVar2 == 0) {
    FUN_004a0060();
  }
  DAT_0083ad50 = iVar2;
  return iVar2;
}
}

namespace f_004a43f0 {
// MATCH: golf_clean.exe 0x004a43f0 ?FUN_004a43f0@C_FUN_004a43f0@f_004a43f0@@QAEXHH@Z
struct T_FUN_00492a90 { int FUN_00492a90(int, int, undefined4 *, undefined4 *); };
struct C_FUN_004a43f0 { void FUN_004a43f0(int param_1, int param_2); };
void C_FUN_004a43f0::FUN_004a43f0(int param_1, int param_2)
{
  if (((((T_FUN_00492a90 *)((void *)((int)this + -0xd0)))->FUN_00492a90(param_1, param_2,(unsigned int *)(&param_2), 0)) != -1) && (*(code **)((int)this + -0x20) != 0)) {
    (**(code **)((int)this + -0x20))(param_2);
  }
  return;
}
}

namespace f_004a4430 {
// MATCH: golf_clean.exe 0x004a4430 ?FUN_004a4430@C_FUN_004a4430@f_004a4430@@QAEXHH@Z
struct T_FUN_00492a90 { int FUN_00492a90(int, int, undefined4 *, undefined4 *); };
struct C_FUN_004a4430 { void FUN_004a4430(int param_1, int param_2); };
void C_FUN_004a4430::FUN_004a4430(int param_1, int param_2)
{
  if (((((T_FUN_00492a90 *)((void *)((int)this + -0xd0)))->FUN_00492a90(param_1, param_2,(unsigned int *)(&param_2), 0)) != -1) && (*(code **)((int)this + -0x1c) != 0)) {
    (**(code **)((int)this + -0x1c))(param_2);
  }
  return;
}
}

namespace f_004a4470 {
// MATCH: golf_clean.exe 0x004a4470 ?FUN_004a4470@C_FUN_004a4470@f_004a4470@@QAEXHH@Z
struct T_FUN_00492a90 { int FUN_00492a90(int, int, undefined4 *, undefined4 *); };
struct C_FUN_004a4470 { void FUN_004a4470(int param_1, int param_2); };
void C_FUN_004a4470::FUN_004a4470(int param_1, int param_2)
{
  if (((((T_FUN_00492a90 *)((void *)((int)this + -0xd0)))->FUN_00492a90(param_1, param_2,(unsigned int *)(&param_2), 0)) != -1) && (*(code **)((int)this + -0x18) != 0)) {
    (**(code **)((int)this + -0x18))(param_2);
  }
  return;
}
}

namespace f_004a44b0 {
// MATCH: golf_clean.exe 0x004a44b0 ?FUN_004a44b0@C_FUN_004a44b0@f_004a44b0@@QAEXHH@Z
struct T_FUN_00492a90 { int FUN_00492a90(int, int, undefined4 *, undefined4 *); };
struct C_FUN_004a44b0 { void FUN_004a44b0(int param_1, int param_2); };
void C_FUN_004a44b0::FUN_004a44b0(int param_1, int param_2)
{
  if (((((T_FUN_00492a90 *)((void *)((int)this + -0xd0)))->FUN_00492a90(param_1, param_2,(unsigned int *)(&param_2), 0)) != -1) && (*(code **)((int)this + -0x14) != 0)) {
    (**(code **)((int)this + -0x14))(param_2);
  }
  return;
}
}

namespace f_004a44f0 {
// MATCH: golf_clean.exe 0x004a44f0 ?FUN_004a44f0@C_FUN_004a44f0@f_004a44f0@@QAEXHH@Z
struct T_FUN_00492a90 { int FUN_00492a90(int, int, undefined4 *, undefined4 *); };
struct C_FUN_004a44f0 { void FUN_004a44f0(int param_1, int param_2); };
void C_FUN_004a44f0::FUN_004a44f0(int param_1, int param_2)
{
  if (((((T_FUN_00492a90 *)((void *)((int)this + -0xd0)))->FUN_00492a90(param_1, param_2,(unsigned int *)(&param_2), 0)) != -1) && (*(code **)((int)this + -0x10) != 0)) {
    (**(code **)((int)this + -0x10))(param_2);
  }
  return;
}
}

namespace f_004a4530 {
// MATCH: golf_clean.exe 0x004a4530 ?FUN_004a4530@C_FUN_004a4530@f_004a4530@@QAEXHH@Z
struct T_FUN_00492a90 { int FUN_00492a90(int, int, undefined4 *, undefined4 *); };
struct C_FUN_004a4530 { void FUN_004a4530(int param_1, int param_2); };
void C_FUN_004a4530::FUN_004a4530(int param_1, int param_2)
{
  if (((((T_FUN_00492a90 *)((void *)((int)this + -0xd0)))->FUN_00492a90(param_1, param_2,(unsigned int *)(&param_2), 0)) != -1) && (*(code **)((int)this + -0xc) != 0)) {
    (**(code **)((int)this + -0xc))(param_2);
  }
  return;
}
}

namespace f_004a4570 {
// MATCH: golf_clean.exe 0x004a4570 ?FUN_004a4570@C_FUN_004a4570@f_004a4570@@QAEXHH@Z
struct T_FUN_00492a90 { int FUN_00492a90(int, int, undefined4 *, undefined4 *); };
struct C_FUN_004a4570 { void FUN_004a4570(int param_1, int param_2); };
void C_FUN_004a4570::FUN_004a4570(int param_1, int param_2)
{
  if (((((T_FUN_00492a90 *)((void *)((int)this + -0xd0)))->FUN_00492a90(param_1, param_2,(unsigned int *)(&param_2), 0)) != -1) && (*(code **)((int)this + -8) != 0)) {
    (**(code **)((int)this + -8))(param_2);
  }
  return;
}
}
