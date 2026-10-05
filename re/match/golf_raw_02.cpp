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

namespace f_0044b100 {
// MATCH: golf_clean.exe 0x0044b100 ?FUN_0044b100@f_0044b100@@YAXXZ
extern int DAT_008223c0;
undefined4 __fastcall FUN_0044b060(undefined4);
void __cdecl FUN_0044b100()
{
  FUN_0044b060((unsigned int)(((char *)&DAT_008223c0)));
  return;
}
}

namespace f_0044b140 {
// MATCH: golf_clean.exe 0x0044b140 ?FUN_0044b140@f_0044b140@@YAXXZ
extern int DAT_008222b8;
undefined4 __fastcall FUN_0044b060(undefined4);
void __cdecl FUN_0044b140()
{
  FUN_0044b060((unsigned int)(((char *)&DAT_008222b8)));
  return;
}
}

namespace f_0044b760 {
// MATCH: golf_clean.exe 0x0044b760 ?FUN_0044b760@f_0044b760@@YAXXZ
extern int DAT_00821cb8;
undefined4 __fastcall FUN_0044b8d0(undefined4);
void __cdecl FUN_0044b760()
{
  FUN_0044b8d0((unsigned int)(((char *)&DAT_00821cb8)));
  return;
}
}

namespace f_0045aed0 {
// MATCH: golf_clean.exe 0x0045aed0 ?FUN_0045aed0@f_0045aed0@@YAXXZ
extern int DAT_00822b98;
extern int DAT_00822b9c;
extern int DAT_00822d68;
undefined4 __stdcall FUN_0045af00();
void __stdcall FUN_00483cf0();
void __cdecl FUN_0045aed0()
{
    int iVar1;
  iVar1 = FUN_0045af00();
  while (iVar1 == 0) {
    DAT_00822b9c = 0;
    DAT_00822b98 = 0;
    iVar1 = FUN_0045af00();
  }
  FUN_00483cf0();
  DAT_00822d68 = 0;
  return;
}
}

namespace f_0045af00 {
// MATCH: golf_clean.exe 0x0045af00 ?FUN_0045af00@f_0045af00@@YAIXZ
extern int DAT_00822b98;
extern int DAT_00822b9c;
void __stdcall FUN_00483c30();
void __stdcall FUN_00483c70();
undefined4 __cdecl FUN_0045af00()
{
  FUN_00483c70();
  FUN_00483c30();
  if ((DAT_00822b9c == 0) && (DAT_00822b98 == 0)) {
    return 1;
  }
  return 0;
}
}

namespace f_0045ba60 {
// MATCH: golf_clean.exe 0x0045ba60 ?FUN_0045ba60@f_0045ba60@@YAXXZ
extern int DAT_00822cc8;
void __fastcall FUN_00488490(undefined4 *);
void __cdecl FUN_0045ba60()
{
  FUN_00488490((undefined4 *)((char *)&DAT_00822cc8));
  return;
}
}

namespace f_0045bab0 {
// MATCH: golf_clean.exe 0x0045bab0 ?FUN_0045bab0@f_0045bab0@@YAXXZ
extern int DAT_00822c98;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0045bab0()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00822c98));
  return;
}
}

namespace f_00463170 {
// MATCH: golf_clean.exe 0x00463170 ?FUN_00463170@f_00463170@@YAHHH@Z
int __cdecl FUN_00463170(int param_1, int param_2)
{
  if (param_1 >= param_2) {
    param_1 = -1;
  }
  return param_1;
}
}

namespace f_00465560 {
// MATCH: golf_clean.exe 0x00465560 ?FUN_00465560@f_00465560@@YAXXZ
extern int DAT_00822b98;
extern int DAT_00822b9c;
extern int DAT_00822d68;
void __stdcall FUN_00483d30();
void __cdecl FUN_00465560()
{
  FUN_00483d30();
  DAT_00822d68 = 0;
  DAT_00822b9c = 0;
  DAT_00822b98 = 0;
  return;
}
}

namespace f_0046de40 {
// MATCH: golf_clean.exe 0x0046de40 ?FUN_0046de40@f_0046de40@@YAXXZ
extern int DAT_00838f98;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_0046de40()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_00838f98));
  return;
}
}

namespace f_00473a70 {
// MATCH: golf_clean.exe 0x00473a70 ?FUN_00473a70@f_00473a70@@YAXXZ
extern int DAT_00839348;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_00473a70()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_00839348));
  return;
}
}

namespace f_004744c0 {
// MATCH: golf_clean.exe 0x004744c0 ?FUN_004744c0@C_FUN_004744c0@f_004744c0@@QAEHH@Z
extern int DAT_0083ad50;
void __fastcall FUN_00473ae0(int);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(int, int); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(int); };
struct VT_3 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(int); };
struct C_FUN_004744c0 { int FUN_004744c0(int param_1); };
int C_FUN_004744c0::FUN_004744c0(int param_1)
{
    undefined4 uVar1;
    int * piVar2;
    int iVar3;
  if (*(int *)(param_1 + 4) == 0) {
    return (int)this;
  }
  FUN_00473ae0((int)this);
  *(undefined4 *)((int)this + 8) = *(undefined4 *)(param_1 + 8);
  *(undefined4 *)((int)this + 0x10) = *(undefined4 *)(param_1 + 0x10);
  *(undefined4 *)((int)this + 0x14) = *(undefined4 *)(param_1 + 0x14);
  *(undefined4 *)((int)this + 0x18) = *(undefined4 *)(param_1 + 0x18);
  *(undefined4 *)((int)this + 0x1c) = *(undefined4 *)(param_1 + 0x1c);
  *(undefined4 *)((int)this + 0x20) = *(undefined4 *)(param_1 + 0x20);
  *(undefined4 *)((int)this + 0x24) = *(undefined4 *)(param_1 + 0x24);
  uVar1 = *(undefined4 *)(param_1 + 0x28);
  *(undefined4 *)((int)this + 0x28) = uVar1;
  if (!(this == (void *)0x0)) {
    iVar3 = (int)this + 4;
  } else {
    iVar3 = 0;
  }
  piVar2 = (int *)((VT_1 *)(DAT_0083ad50))->f32((int)(iVar3), (int)(uVar1));
  *(int **)((int)this + 4) = piVar2;
  ((VT_2 *)(piVar2))->f6((int)(0xff));
  ((VT_3 *)(*(int **)((int)this + 4)))->f3((int)(*(undefined4 *)(param_1 + 4)));
  return (int)this;
}
}

namespace f_00474780 {
// MATCH: golf_clean.exe 0x00474780 ?FUN_00474780@f_00474780@@YIXH@Z
void __fastcall FUN_00474780(int param_1)
{
  *(undefined1 *)(param_1 + 4) = 0;
  *(undefined4 *)(param_1 + 0xc) = 0;
  *(undefined4 *)(param_1 + 8) = 0;
  *(undefined4 *)(param_1 + 0x14) = 0;
  *(undefined4 *)(param_1 + 0x10) = 0;
  return;
}
}

namespace f_004747a0 {
// MATCH: golf_clean.exe 0x004747a0 ?FUN_004747a0@f_004747a0@@YIPAIPAI@Z
extern void *PTR_FUN_004ba84c;
void __fastcall FUN_00474780(int);
undefined4 * __fastcall FUN_004747a0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004ba84c);
  FUN_00474780((int)param_1);
  return param_1;
}
}

namespace f_00474810 {
// MATCH: golf_clean.exe 0x00474810 ?FUN_00474810@f_00474810@@YIXPAI@Z
extern void *PTR_FUN_004ba84c;
void __fastcall FUN_004747e0(int);
void __fastcall FUN_00474810(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004ba84c);
  FUN_004747e0((int)param_1);
  return;
}
}

namespace f_00474a90 {
// MATCH: golf_clean.exe 0x00474a90 ?FUN_00474a90@f_00474a90@@YAXXZ
extern int DAT_00839a68;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_00474a90()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00839a68));
  return;
}
}

namespace f_00475b00 {
// MATCH: golf_clean.exe 0x00475b00 ?FUN_00475b00@C_FUN_00475b00@f_00475b00@@QAEII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(int); };
struct C_FUN_00475b00 { undefined4 FUN_00475b00(undefined4 param_1); };
undefined4 C_FUN_00475b00::FUN_00475b00(undefined4 param_1)
{
  if (*(int **)((int)this + 4) != (int *)0x0) {
    return (((VT_1 *)(*(int **)((int)this + 4)))->f13((int)(param_1)));
  }
  return 0x18;
}
}

namespace f_00476340 {
// MATCH: golf_clean.exe 0x00476340 ?FUN_00476340@C_FUN_00476340@f_00476340@@QAEXIIII@Z
struct C_FUN_00476340 { void FUN_00476340(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_00476340::FUN_00476340(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  *(undefined4 *)((int)this + 0x70) = param_1;
  *(undefined4 *)((int)this + 0x80) = param_2;
  *(undefined4 *)((int)this + 0x90) = param_3;
  *(undefined4 *)((int)this + 0xa0) = param_4;
  return;
}
}

namespace f_00476370 {
// MATCH: golf_clean.exe 0x00476370 ?FUN_00476370@C_FUN_00476370@f_00476370@@QAEXIIII@Z
struct C_FUN_00476370 { void FUN_00476370(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_00476370::FUN_00476370(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  *(undefined4 *)((int)this + 0x74) = param_1;
  *(undefined4 *)((int)this + 0x84) = param_2;
  *(undefined4 *)((int)this + 0x94) = param_3;
  *(undefined4 *)((int)this + 0xa4) = param_4;
  return;
}
}

namespace f_004763a0 {
// MATCH: golf_clean.exe 0x004763a0 ?FUN_004763a0@C_FUN_004763a0@f_004763a0@@QAEXIIII@Z
struct C_FUN_004763a0 { void FUN_004763a0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_004763a0::FUN_004763a0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  *(undefined4 *)((int)this + 0x78) = param_1;
  *(undefined4 *)((int)this + 0x88) = param_2;
  *(undefined4 *)((int)this + 0x98) = param_3;
  *(undefined4 *)((int)this + 0xa8) = param_4;
  return;
}
}

namespace f_004785e0 {
// MATCH: golf_clean.exe 0x004785e0 ?FUN_004785e0@C_FUN_004785e0@f_004785e0@@QAEXPADI@Z
extern int DAT_00839aa0;
extern int DAT_00839aa4;
extern int DAT_00839aa8;
struct T_FUN_00478610 { int FUN_00478610(char *); };
struct C_FUN_004785e0 { void FUN_004785e0(char *param_1, undefined4 param_2); };
void C_FUN_004785e0::FUN_004785e0(char *param_1, undefined4 param_2)
{
  *(undefined4 *)((int)this + 0x1c) = 0;
  *(undefined4 *)((int)this + 0x18) = *(undefined4 *)((int)this + 0x14);
  *(undefined4 *)((int)this + 0x3c) = 0;
  DAT_00839aa0 = 0;
  DAT_00839aa4 = param_2;
  DAT_00839aa8 = 0;
  ((T_FUN_00478610 *)(this))->FUN_00478610(param_1);
  return;
}
}

namespace f_00478700 {
// MATCH: golf_clean.exe 0x00478700 ?FUN_00478700@f_00478700@@YIHH@Z
extern int DAT_00839aa0;
extern int DAT_00839aa4;
extern int DAT_00839aa8;
int __fastcall FUN_00477580(int);
int __fastcall FUN_00478700(int param_1)
{
  if (DAT_00839aa8 != 0) {
    DAT_00839aa0 = DAT_00839aa0 + (FUN_00477580(param_1));
    *(int *)(param_1 + 0x1c) = *(int *)(param_1 + 0x1c) + 1;
  }
  DAT_00839aa4 = 0;
  *(undefined4 *)(param_1 + 0x30) = 0;
  *(undefined4 *)(param_1 + 0x38) = 0;
  *(undefined4 *)(param_1 + 0x34) = 0;
  *(undefined4 *)(param_1 + 0x3c) = 0;
  return DAT_00839aa0;
}
}

namespace f_004789f0 {
// MATCH: golf_clean.exe 0x004789f0 ?FUN_004789f0@C_FUN_004789f0@f_004789f0@@QAEIH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(int); };
struct C_FUN_004789f0 { undefined4 FUN_004789f0(int param_1); };
undefined4 C_FUN_004789f0::FUN_004789f0(int param_1)
{
  if (*(int **)((int)this + 4) == (int *)0x0) {
    return 7;
  }
  if (param_1 == 0) {
    return 3;
  }
  ((VT_1 *)(*(int **)((int)this + 4)))->f59((int)(*(undefined4 *)(param_1 + 4)));
  return 0;
}
}

namespace f_00478b30 {
// MATCH: golf_clean.exe 0x00478b30 ?FUN_00478b30@C_FUN_00478b30@f_00478b30@@QAEXI@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(int, int); };
struct C_FUN_00478b30 { void FUN_00478b30(undefined4 param_1); };
void C_FUN_00478b30::FUN_00478b30(undefined4 param_1)
{
  if (*(int **)((int)this + 4) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 4)))->f17((int)(0), (int)(param_1));
  }
  return;
}
}

namespace f_00478b50 {
// MATCH: golf_clean.exe 0x00478b50 ?FUN_00478b50@C_FUN_00478b50@f_00478b50@@QAEIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(int, int); };
struct C_FUN_00478b50 { undefined4 FUN_00478b50(undefined4 param_1, undefined4 param_2); };
undefined4 C_FUN_00478b50::FUN_00478b50(undefined4 param_1, undefined4 param_2)
{
  if (*(int **)((int)this + 4) == (int *)0x0) {
    return 7;
  }
  return (((VT_1 *)(*(int **)((int)this + 4)))->f17((int)(param_1), (int)(param_2)));
}
}

namespace f_00478b80 {
// MATCH: golf_clean.exe 0x00478b80 ?FUN_00478b80@C_FUN_00478b80@f_00478b80@@QAEXIIIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(int, int, int, int, int, int); };
struct C_FUN_00478b80 { void FUN_00478b80(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4, undefined4 param_5); };
void C_FUN_00478b80::FUN_00478b80(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4, undefined4 param_5)
{
  if (*(int **)((int)this + 4) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 4)))->f25((int)(param_1), (int)(param_2), (int)(param_3), (int)(param_4), (int)(param_5), (int)(1));
  }
  return;
}
}

namespace f_00478bb0 {
// MATCH: golf_clean.exe 0x00478bb0 ?FUN_00478bb0@C_FUN_00478bb0@f_00478bb0@@QAEXIIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(int, int, int, int, int, int); };
struct C_FUN_00478bb0 { void FUN_00478bb0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_00478bb0::FUN_00478bb0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  if (*(int **)((int)this + 4) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 4)))->f25((int)(param_1), (int)(param_3), (int)(param_2), (int)(param_3), (int)(param_4), (int)(1));
  }
  return;
}
}

namespace f_00478be0 {
// MATCH: golf_clean.exe 0x00478be0 ?FUN_00478be0@C_FUN_00478be0@f_00478be0@@QAEXIIII@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(int, int, int, int, int, int); };
struct C_FUN_00478be0 { void FUN_00478be0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4); };
void C_FUN_00478be0::FUN_00478be0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  if (*(int **)((int)this + 4) != (int *)0x0) {
    ((VT_1 *)(*(int **)((int)this + 4)))->f25((int)(param_1), (int)(param_2), (int)(param_1), (int)(param_3), (int)(param_4), (int)(1));
  }
  return;
}
}

namespace f_00479560 {
// MATCH: golf_clean.exe 0x00479560 ?FUN_00479560@C_FUN_00479560@f_00479560@@QAEXHHHHII@Z
struct T_FUN_00478bb0 { void FUN_00478bb0(undefined4, undefined4, undefined4, undefined4); };
struct T_FUN_00478be0 { void FUN_00478be0(undefined4, undefined4, undefined4, undefined4); };
struct C_FUN_00479560 { void FUN_00479560(int param_1, int param_2, int param_3, int param_4, undefined4 param_5, undefined4 param_6); };
void C_FUN_00479560::FUN_00479560(int param_1, int param_2, int param_3, int param_4, undefined4 param_5, undefined4 param_6)
{
  ((T_FUN_00478bb0 *)(this))->FUN_00478bb0(param_1 + 1, param_3, param_2, param_5);
  ((T_FUN_00478bb0 *)(this))->FUN_00478bb0(param_1, param_3 + -1, param_4, param_6);
  ((T_FUN_00478be0 *)(this))->FUN_00478be0(param_1, param_2, param_4 + -1, param_5);
  ((T_FUN_00478be0 *)(this))->FUN_00478be0(param_3, param_2 + 1, param_4, param_6);
  return;
}
}

namespace f_004795d0 {
// MATCH: golf_clean.exe 0x004795d0 ?FUN_004795d0@C_FUN_004795d0@f_004795d0@@QAEIPAHII@Z
struct T_FUN_00479560 { void FUN_00479560(int, int, int, int, undefined4, undefined4); };
struct C_FUN_004795d0 { undefined4 FUN_004795d0(int *param_1, undefined4 param_2, undefined4 param_3); };
undefined4 C_FUN_004795d0::FUN_004795d0(int *param_1, undefined4 param_2, undefined4 param_3)
{
  if (param_1 == (int *)0x0) {
    return 3;
  }
  ((T_FUN_00479560 *)(this))->FUN_00479560(*param_1, param_1[1], param_1[2] + -1, param_1[3] + -1, param_2, param_3);
  return 0;
}
}

namespace f_00479610 {
// MATCH: golf_clean.exe 0x00479610 ?FUN_00479610@C_FUN_00479610@f_00479610@@QAEXHHHII@Z
struct T_FUN_00478c10 { void FUN_00478c10(undefined4, undefined4, undefined4, undefined4); };
struct T_FUN_00478c70 { void FUN_00478c70(undefined4, undefined4, undefined4, undefined4); };
struct C_FUN_00479610 { void FUN_00479610(int param_1, int param_2, int param_3, undefined4 param_4, undefined4 param_5); };
void C_FUN_00479610::FUN_00479610(int param_1, int param_2, int param_3, undefined4 param_4, undefined4 param_5)
{
  ((T_FUN_00478c10 *)(this))->FUN_00478c10(param_1 + 1, param_3, param_2, param_5);
  ((T_FUN_00478c10 *)(this))->FUN_00478c10(param_1, param_3 + -1, param_4, param_5);
  ((T_FUN_00478c70 *)(this))->FUN_00478c70(param_1, param_2, param_4, param_5);
  ((T_FUN_00478c70 *)(this))->FUN_00478c70(param_3, param_2 + 1, param_4, param_5);
  return;
}
}

namespace f_00479670 {
// MATCH: golf_clean.exe 0x00479670 ?FUN_00479670@C_FUN_00479670@f_00479670@@QAEIPAHI@Z
struct T_FUN_00479610 { void FUN_00479610(int, int, int, undefined4, undefined4); };
struct C_FUN_00479670 { undefined4 FUN_00479670(int *param_1, undefined4 param_2); };
undefined4 C_FUN_00479670::FUN_00479670(int *param_1, undefined4 param_2)
{
  if (param_1 == (int *)0x0) {
    return 0x10;
  }
  ((T_FUN_00479610 *)(this))->FUN_00479610(*param_1, param_1[1], param_1[2] + -1, param_1[3] + -1, param_2);
  return 0;
}
}

namespace f_004799f0 {
// MATCH: golf_clean.exe 0x004799f0 ?FUN_004799f0@f_004799f0@@YAXXZ
extern int DAT_0083a500;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_004799f0()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_0083a500));
  return;
}
}

namespace f_00479a40 {
// MATCH: golf_clean.exe 0x00479a40 ?FUN_00479a40@f_00479a40@@YAXXZ
extern int DAT_0083a7b8;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_00479a40()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_0083a7b8));
  return;
}
}

namespace f_00479b20 {
// MATCH: golf_clean.exe 0x00479b20 ?FUN_00479b20@f_00479b20@@YIXH@Z
extern int DAT_0083aa98;
void __fastcall FUN_00479b20(int param_1)
{
  if (DAT_0083aa98 == param_1) {
    ClipCursor((RECT *)0x0);
    DAT_0083aa98 = 0;
  }
  return;
}
}

namespace f_0047abc0 {
// MATCH: golf_clean.exe 0x0047abc0 ?FUN_0047abc0@f_0047abc0@@YAIXZ
uint __cdecl FUN_0047abc0()
{
  return (GetSystemMetrics(0x17)) ^ (int)(GetAsyncKeyState(1)) >> 0xf;
}
}

namespace f_0047b9f0 {
// MATCH: golf_clean.exe 0x0047b9f0 ?FUN_0047b9f0@C_FUN_0047b9f0@f_0047b9f0@@QAEXH@Z
struct T_FUN_004967f0 { void FUN_004967f0(int); };
struct C_FUN_0047b9f0 { void FUN_0047b9f0(int param_1); };
void C_FUN_0047b9f0::FUN_0047b9f0(int param_1)
{
  if (*(void **)((int)this + 0x26c) != (void *)0x0) {
    ((T_FUN_004967f0 *)(*(void **)((int)this + 0x26c)))->FUN_004967f0(param_1);
  }
  return;
}
}

namespace f_0047ba10 {
// MATCH: golf_clean.exe 0x0047ba10 ?FUN_0047ba10@C_FUN_0047ba10@f_0047ba10@@QAEXH@Z
struct T_FUN_004967f0 { void FUN_004967f0(int); };
struct C_FUN_0047ba10 { void FUN_0047ba10(int param_1); };
void C_FUN_0047ba10::FUN_0047ba10(int param_1)
{
  if (*(void **)((int)this + 0x270) != (void *)0x0) {
    ((T_FUN_004967f0 *)(*(void **)((int)this + 0x270)))->FUN_004967f0(param_1);
  }
  return;
}
}

namespace f_0047ba30 {
// MATCH: golf_clean.exe 0x0047ba30 ?FUN_0047ba30@C_FUN_0047ba30@f_0047ba30@@QAEXII@Z
struct T_FUN_00496770 { void FUN_00496770(uint, uint); };
struct C_FUN_0047ba30 { void FUN_0047ba30(uint param_1, uint param_2); };
void C_FUN_0047ba30::FUN_0047ba30(uint param_1, uint param_2)
{
  if (*(void **)((int)this + 0x26c) != (void *)0x0) {
    ((T_FUN_00496770 *)(*(void **)((int)this + 0x26c)))->FUN_00496770(param_1, param_2);
  }
  return;
}
}

namespace f_0047ba50 {
// MATCH: golf_clean.exe 0x0047ba50 ?FUN_0047ba50@C_FUN_0047ba50@f_0047ba50@@QAEXII@Z
struct T_FUN_00496770 { void FUN_00496770(uint, uint); };
struct C_FUN_0047ba50 { void FUN_0047ba50(uint param_1, uint param_2); };
void C_FUN_0047ba50::FUN_0047ba50(uint param_1, uint param_2)
{
  if (*(void **)((int)this + 0x270) != (void *)0x0) {
    ((T_FUN_00496770 *)(*(void **)((int)this + 0x270)))->FUN_00496770(param_1, param_2);
  }
  return;
}
}
