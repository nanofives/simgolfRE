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

namespace f_00480360 {
// MATCH: golf_clean.exe 0x00480360 ?FUN_00480360@f_00480360@@YAXXZ
extern int DAT_0083a2c8;
extern int DAT_0083aac0;
extern int DAT_0083aac4;
void __cdecl FUN_0047cdb0(int);
void __cdecl FUN_0047f8e0(RECT *, int);
void __cdecl FUN_00480360()
{
  if (DAT_0083aac4 != 0) {
    DAT_0083aac0 = 0;
    DAT_0083aac4 = 0;
    FUN_0047f8e0((RECT *)((char *)&DAT_0083a2c8),0);
    FUN_0047cdb0((int)(((char *)&DAT_0083a2c8)));
  }
  return;
}
}

namespace f_00480580 {
// MATCH: golf_clean.exe 0x00480580 ?FUN_00480580@C_FUN_00480580@f_00480580@@QAEPAXE@Z
void __fastcall FUN_004805a0(int *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00480580 { void * FUN_00480580(byte param_1); };
void * C_FUN_00480580::FUN_00480580(byte param_1)
{
  FUN_004805a0((int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00482e90 {
// MATCH: golf_clean.exe 0x00482e90 ?FUN_00482e90@f_00482e90@@YAXXZ
void LAB_00482ea0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00482e90()
{
  _atexit((unsigned int)(&LAB_00482ea0));
  return;
}
}

namespace f_00482ed0 {
// MATCH: golf_clean.exe 0x00482ed0 ?FUN_00482ed0@f_00482ed0@@YAXXZ
void LAB_00482ee0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00482ed0()
{
  _atexit((unsigned int)(&LAB_00482ee0));
  return;
}
}

namespace f_00482f10 {
// MATCH: golf_clean.exe 0x00482f10 ?FUN_00482f10@f_00482f10@@YAXXZ
void LAB_00482f20();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00482f10()
{
  _atexit((unsigned int)(&LAB_00482f20));
  return;
}
}

namespace f_004837c0 {
// MATCH: golf_clean.exe 0x004837c0 ?FUN_004837c0@f_004837c0@@YAXXZ
void LAB_004837d0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004837c0()
{
  _atexit((unsigned int)(&LAB_004837d0));
  return;
}
}

namespace f_00483df0 {
// MATCH: golf_clean.exe 0x00483df0 ?FUN_00483df0@f_00483df0@@YAXXZ
void LAB_00483e00();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00483df0()
{
  _atexit((unsigned int)(&LAB_00483e00));
  return;
}
}

namespace f_00483e30 {
// MATCH: golf_clean.exe 0x00483e30 ?FUN_00483e30@f_00483e30@@YAXXZ
void LAB_00483e40();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00483e30()
{
  _atexit((unsigned int)(&LAB_00483e40));
  return;
}
}

namespace f_00483e70 {
// MATCH: golf_clean.exe 0x00483e70 ?FUN_00483e70@f_00483e70@@YAXXZ
void LAB_00483e80();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00483e70()
{
  _atexit((unsigned int)(&LAB_00483e80));
  return;
}
}

namespace f_004841c0 {
// MATCH: golf_clean.exe 0x004841c0 ?FUN_004841c0@C_FUN_004841c0@f_004841c0@@QAEPAXE@Z
void __fastcall FUN_004841e0(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_004841c0 { void * FUN_004841c0(byte param_1); };
void * C_FUN_004841c0::FUN_004841c0(byte param_1)
{
  FUN_004841e0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00484880 {
// MATCH: golf_clean.exe 0x00484880 ?FUN_00484880@C_FUN_00484880@f_00484880@@QAEPAXE@Z
void __fastcall FUN_004848a0(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00484880 { void * FUN_00484880(byte param_1); };
void * C_FUN_00484880::FUN_00484880(byte param_1)
{
  FUN_004848a0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_004852b0 {
// MATCH: golf_clean.exe 0x004852b0 ?FUN_004852b0@C_FUN_004852b0@f_004852b0@@QAEPAXE@Z
void __fastcall FUN_004852d0(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_004852b0 { void * FUN_004852b0(byte param_1); };
void * C_FUN_004852b0::FUN_004852b0(byte param_1)
{
  FUN_004852d0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_004853b0 {
// MATCH: golf_clean.exe 0x004853b0 ?FUN_004853b0@C_FUN_004853b0@f_004853b0@@QAEPAXE@Z
void __fastcall FUN_004853d0(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_004853b0 { void * FUN_004853b0(byte param_1); };
void * C_FUN_004853b0::FUN_004853b0(byte param_1)
{
  FUN_004853d0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_004854c0 {
// MATCH: golf_clean.exe 0x004854c0 ?FUN_004854c0@f_004854c0@@YIIPAH@Z
undefined4 __cdecl FUN_00484110(undefined4);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); };
struct VT_2 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); };
struct VT_3 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); };
undefined4 __fastcall FUN_004854c0(int *param_1)
{
  if ((int *)param_1[0x10] != (int *)0x0) {
    if ((((VT_1 *)(param_1[0x10]))->f23()) == 0) {
      FUN_00484110((unsigned int)((int *)param_1[0x10]));
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

namespace f_00486250 {
// MATCH: golf_clean.exe 0x00486250 ?FUN_00486250@C_FUN_00486250@f_00486250@@QAEXI@Z
void __cdecl _free(LPVOID);
void * __cdecl _malloc(size_t _Size);
char * __cdecl _strncpy(char * _Dest, char * _Source, size_t _Count);
struct C_FUN_00486250 { void FUN_00486250(size_t param_1); };
void C_FUN_00486250::FUN_00486250(size_t param_1)
{
    char * _Dest;
  if (param_1 != *(size_t *)((int)this + 0x578)) {
    _Dest = (char *)(_malloc(param_1 + 1));
    _Dest[param_1] = '\0';
    *_Dest = '\0';
    if (*(char **)((int)this + 0x574) != (char *)0x0) {
      _strncpy(_Dest,*(char **)((int)this + 0x574),param_1);
      if (*(LPVOID *)((int)this + 0x574) != (LPVOID)0x0) {
        _free(*(LPVOID *)((int)this + 0x574));
      }
    }
    *(char **)((int)this + 0x574) = _Dest;
    *(size_t *)((int)this + 0x578) = param_1;
  }
  return;
}
}

namespace f_00487020 {
// MATCH: golf_clean.exe 0x00487020 ?FUN_00487020@C_FUN_00487020@f_00487020@@QAEPAXE@Z
void __fastcall FUN_00487040(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00487020 { void * FUN_00487020(byte param_1); };
void * C_FUN_00487020::FUN_00487020(byte param_1)
{
  FUN_00487040((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00487240 {
// MATCH: golf_clean.exe 0x00487240 ?FUN_00487240@C_FUN_00487240@f_00487240@@QAEPAXE@Z
void __fastcall FUN_00487260(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00487240 { void * FUN_00487240(byte param_1); };
void * C_FUN_00487240::FUN_00487240(byte param_1)
{
  FUN_00487260((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_004872f0 {
// MATCH: golf_clean.exe 0x004872f0 ?FUN_004872f0@C_FUN_004872f0@f_004872f0@@QAEPAXE@Z
void __fastcall FUN_00487390(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_004872f0 { void * FUN_004872f0(byte param_1); };
void * C_FUN_004872f0::FUN_004872f0(byte param_1)
{
  FUN_00487390((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00487310 {
// MATCH: golf_clean.exe 0x00487310 ?FUN_00487310@f_00487310@@YIXH@Z
extern void *PTR_FUN_004bafac;
void __cdecl FUN_004a4ffc(undefined4);
void __fastcall FUN_00487310(int param_1)
{
    int iVar1;
    undefined4 * puVar2;
    int iVar3;
  iVar1 = *(int *)(param_1 + 0xc);
  *(undefined ***)(param_1 + 8) = (unsigned char ** )(&PTR_FUN_004bafac);
  if (iVar1 != 0) {
    puVar2 = *(undefined4 **)(iVar1 + 4);
    if (!(puVar2 == (undefined4 *)0x0)) {
      *puVar2 = 0;
      *(undefined4 **)(param_1 + 0xc) = puVar2;
    } else {
      *(undefined4 *)(param_1 + 0x10) = 0;
      *(undefined4 *)(param_1 + 0xc) = 0;
    }
    iVar3 = *(int *)(iVar1 + 8);
    FUN_004a4ffc(iVar1);
    *(int *)(param_1 + 0x18) = *(int *)(param_1 + 0x18) + -1;
    while ((iVar3 != 0 && (iVar1 = *(int *)(param_1 + 0xc), iVar1 != 0))) {
      puVar2 = *(undefined4 **)(iVar1 + 4);
      if (!(puVar2 == (undefined4 *)0x0)) {
        *puVar2 = 0;
        *(undefined4 **)(param_1 + 0xc) = puVar2;
      } else {
        *(undefined4 *)(param_1 + 0x10) = 0;
        *(undefined4 *)(param_1 + 0xc) = 0;
      }
      iVar3 = *(int *)(iVar1 + 8);
      FUN_004a4ffc(iVar1);
      *(int *)(param_1 + 0x18) = *(int *)(param_1 + 0x18) + -1;
    }
  }
  return;
}
}

namespace f_00487a40 {
// MATCH: golf_clean.exe 0x00487a40 ?FUN_00487a40@C_FUN_00487a40@f_00487a40@@QAEPAXE@Z
void __fastcall FUN_00487a60(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00487a40 { void * FUN_00487a40(byte param_1); };
void * C_FUN_00487a40::FUN_00487a40(byte param_1)
{
  FUN_00487a60((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00487b60 {
// MATCH: golf_clean.exe 0x00487b60 ?FUN_00487b60@C_FUN_00487b60@f_00487b60@@QAEPAXE@Z
void __fastcall FUN_00487b80(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00487b60 { void * FUN_00487b60(byte param_1); };
void * C_FUN_00487b60::FUN_00487b60(byte param_1)
{
  FUN_00487b80((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00487ce0 {
// MATCH: golf_clean.exe 0x00487ce0 ?FUN_00487ce0@C_FUN_00487ce0@f_00487ce0@@QAEPAIE@Z
extern void *PTR_FUN_004bafac;
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00487ce0 { undefined4 * FUN_00487ce0(byte param_1); };
undefined4 * C_FUN_00487ce0::FUN_00487ce0(byte param_1)
{
    int iVar1;
    undefined4 * puVar2;
    int iVar3;
  iVar1 = *(int *)((int)this + 4);
  *(undefined ***)this = (unsigned char ** )(&PTR_FUN_004bafac);
  if (iVar1 != 0) {
    puVar2 = *(undefined4 **)(iVar1 + 4);
    if (!(puVar2 == (undefined4 *)0x0)) {
      *puVar2 = 0;
      *(undefined4 **)((int)this + 4) = puVar2;
    } else {
      *(undefined4 *)((int)this + 8) = 0;
      *(undefined4 *)((int)this + 4) = 0;
    }
    iVar3 = *(int *)(iVar1 + 8);
    FUN_004a4ffc(iVar1);
    *(int *)((int)this + 0x10) = *(int *)((int)this + 0x10) + -1;
    while ((iVar3 != 0 && (iVar1 = *(int *)((int)this + 4), iVar1 != 0))) {
      puVar2 = *(undefined4 **)(iVar1 + 4);
      if (!(puVar2 == (undefined4 *)0x0)) {
        *puVar2 = 0;
        *(undefined4 **)((int)this + 4) = puVar2;
      } else {
        *(undefined4 *)((int)this + 8) = 0;
        *(undefined4 *)((int)this + 4) = 0;
      }
      iVar3 = *(int *)(iVar1 + 8);
      FUN_004a4ffc(iVar1);
      *(int *)((int)this + 0x10) = *(int *)((int)this + 0x10) + -1;
    }
  }
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_00487d90 {
// MATCH: golf_clean.exe 0x00487d90 ?FUN_00487d90@f_00487d90@@YAXXZ
void LAB_00487da0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00487d90()
{
  _atexit((unsigned int)(&LAB_00487da0));
  return;
}
}

namespace f_00487e30 {
// MATCH: golf_clean.exe 0x00487e30 ?FUN_00487e30@f_00487e30@@YAXXZ
void LAB_00487e40();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00487e30()
{
  _atexit((unsigned int)(&LAB_00487e40));
  return;
}
}

namespace f_004882d0 {
// MATCH: golf_clean.exe 0x004882d0 ?FUN_004882d0@C_FUN_004882d0@f_004882d0@@QAEPAXE@Z
void __fastcall FUN_004882f0(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_004882d0 { void * FUN_004882d0(byte param_1); };
void * C_FUN_004882d0::FUN_004882d0(byte param_1)
{
  FUN_004882f0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00488460 {
// MATCH: golf_clean.exe 0x00488460 ?FUN_00488460@C_FUN_00488460@f_00488460@@QAEPAIE@Z
extern void *PTR_FUN_004bb084;
void __fastcall FUN_00487f30(int);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00488460 { undefined4 * FUN_00488460(byte param_1); };
undefined4 * C_FUN_00488460::FUN_00488460(byte param_1)
{
  *(undefined ***)this = (unsigned char ** )(&PTR_FUN_004bb084);
  FUN_00487f30((int)this);
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_00488630 {
// MATCH: golf_clean.exe 0x00488630 ?FUN_00488630@C_FUN_00488630@f_00488630@@QAEPAXE@Z
void __fastcall FUN_00488650(int *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00488630 { void * FUN_00488630(byte param_1); };
void * C_FUN_00488630::FUN_00488630(byte param_1)
{
  FUN_00488650((int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_004886d0 {
// MATCH: golf_clean.exe 0x004886d0 ?FUN_004886d0@f_004886d0@@YIXPAH@Z
extern int DAT_004e443c;
extern int DAT_004e4440;
extern int DAT_004e4444;
extern int DAT_004e4448;
extern int DAT_004e444c;
extern int DAT_004e4480;
extern int DAT_004e4484;
extern int DAT_0083b604;
extern int DAT_0083b608;
void __fastcall FUN_00480610(int *);
void __cdecl _free(LPVOID);
void __fastcall FUN_004886d0(int *param_1)
{
  FUN_00480610(param_1);
  param_1[0x17b] = DAT_004e4480;
  param_1[0x17c] = DAT_004e4484;
  param_1[0x15d] = 0;
  param_1[0x179] = 0;
  param_1[0x185] = 0;
  param_1[0x17a] = 0;
  param_1[0x16b] = -1;
  param_1[0x16c] = -1;
  param_1[0x189] = 0;
  param_1[0x18a] = 0;
  param_1[0x18b] = 0;
  param_1[0x183] = DAT_0083b604;
  param_1[0x17f] = DAT_004e443c;
  param_1[0x180] = DAT_004e4440;
  param_1[0x181] = DAT_004e4444;
  param_1[0x182] = DAT_004e4448;
  param_1[0x184] = DAT_0083b608;
  param_1[0x187] = 0;
  param_1[0x186] = DAT_004e444c;
  if ((LPVOID)param_1[0x17d] != (LPVOID)0x0) {
    _free((LPVOID)param_1[0x17d]);
    param_1[0x17d] = 0;
  }
  if ((LPVOID)param_1[0x17e] != (LPVOID)0x0) {
    _free((LPVOID)param_1[0x17e]);
    param_1[0x17e] = 0;
  }
  param_1[0x188] = 0;
  return;
}
}

namespace f_00489350 {
// MATCH: golf_clean.exe 0x00489350 ?FUN_00489350@C_FUN_00489350@f_00489350@@QAEPAXE@Z
void __fastcall FUN_00489370(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00489350 { void * FUN_00489350(byte param_1); };
void * C_FUN_00489350::FUN_00489350(byte param_1)
{
  FUN_00489370((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00491470 {
// MATCH: golf_clean.exe 0x00491470 ?FUN_00491470@C_FUN_00491470@f_00491470@@QAEPAHE@Z
void __fastcall FUN_00491410(int *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00491470 { int * FUN_00491470(byte param_1); };
int * C_FUN_00491470::FUN_00491470(byte param_1)
{
  FUN_00491410((int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return (int *)(this);
}
}

namespace f_004916f0 {
// MATCH: golf_clean.exe 0x004916f0 ?FUN_004916f0@C_FUN_004916f0@f_004916f0@@QAEPAXE@Z
void __fastcall FUN_00491710(int *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_004916f0 { void * FUN_004916f0(byte param_1); };
void * C_FUN_004916f0::FUN_004916f0(byte param_1)
{
  FUN_00491710((int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00491d80 {
// MATCH: golf_clean.exe 0x00491d80 ?FUN_00491d80@f_00491d80@@YAXHH@Z
int __cdecl FUN_00491c70(uint, int);
void __cdecl FUN_00491d80(int param_1, int param_2)
{
  FUN_00491c70(param_1 + 0x3fffffff,param_2);
  return;
}
}

namespace f_004928b0 {
// MATCH: golf_clean.exe 0x004928b0 ?FUN_004928b0@C_FUN_004928b0@f_004928b0@@QAEPAXE@Z
void __fastcall FUN_004928d0(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_004928b0 { void * FUN_004928b0(byte param_1); };
void * C_FUN_004928b0::FUN_004928b0(byte param_1)
{
  FUN_004928d0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_00493520 {
// MATCH: golf_clean.exe 0x00493520 ?FUN_00493520@f_00493520@@YAIHHHI@Z
undefined4 __cdecl FUN_00493100(int, int, int, undefined4);
undefined4 __cdecl FUN_004932d0(int, int, int, uint);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); };
undefined4 __cdecl FUN_00493520(int param_1, int param_2, int param_3, uint param_4)
{
    int * piVar1;
    undefined4 uVar2;
  if (!(*(int **)(param_1 + 4) == (int *)0x0)) {
    piVar1 = (int *)((VT_1 *)(*(int **)(param_1 + 4)))->f57();
  } else {
    piVar1 = (int *)0x0;
  }
  if (*piVar1 != 8) {
    if (*piVar1 != 0x10) {
      return 0x17;
    }
    uVar2 = FUN_004932d0(param_1,param_2,param_3,param_4);
    return uVar2;
  }
  uVar2 = FUN_00493100(param_1,param_2,param_3,param_4);
  return uVar2;
}
}

namespace f_00496770 {
// MATCH: golf_clean.exe 0x00496770 ?FUN_00496770@C_FUN_00496770@f_00496770@@QAEXII@Z
void __cdecl FUN_00493580(uint *, uint *);
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(); virtual int f14(); virtual int f15(); virtual int f16(); virtual int f17(); virtual int f18(); virtual int f19(); virtual int f20(); virtual int f21(); virtual int f22(); virtual int f23(); virtual int f24(); virtual int f25(); virtual int f26(); virtual int f27(); virtual int f28(); virtual int f29(); virtual int f30(); virtual int f31(); virtual int f32(); virtual int f33(); virtual int f34(); virtual int f35(); virtual int f36(); virtual int f37(); virtual int f38(); virtual int f39(); virtual int f40(); virtual int f41(); virtual int f42(); virtual int f43(); virtual int f44(); virtual int f45(); virtual int f46(); virtual int f47(); virtual int f48(); virtual int f49(); virtual int f50(); virtual int f51(); virtual int f52(); virtual int f53(); virtual int f54(); virtual int f55(); virtual int f56(); virtual int f57(); virtual int f58(); virtual int f59(); virtual int f60(); virtual int f61(); virtual int f62(); virtual int f63(); virtual int f64(); virtual int f65(); virtual int f66(); virtual int f67(); virtual int f68(); virtual int f69(); virtual int f70(); virtual int f71(); virtual int f72(); };
struct C_FUN_00496770 { void FUN_00496770(uint param_1, uint param_2); };
void C_FUN_00496770::FUN_00496770(uint param_1, uint param_2)
{
  if ((int)param_1 < (int)param_2) {
    *(undefined4 *)((int)this + 0x588) = 0;
  }
  else {
    *(undefined4 *)((int)this + 0x588) = 1;
    FUN_00493580(&param_1,&param_2);
  }
  *(uint *)((int)this + 0x580) = param_1;
  *(uint *)((int)this + 0x58c) = param_1;
  *(uint *)((int)this + 0x584) = param_2;
  ((VT_1 *)(this))->f72();
  return;
}
}

namespace f_00497a00 {
// MATCH: golf_clean.exe 0x00497a00 ?FUN_00497a00@C_FUN_00497a00@f_00497a00@@QAEPAXE@Z
void __fastcall FUN_00497a20(int *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00497a00 { void * FUN_00497a00(byte param_1); };
void * C_FUN_00497a00::FUN_00497a00(byte param_1)
{
  FUN_00497a20((int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_0049c000 {
// MATCH: golf_clean.exe 0x0049c000 ?FUN_0049c000@f_0049c000@@YAXXZ
void LAB_0049c010();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049c000()
{
  _atexit((unsigned int)(&LAB_0049c010));
  return;
}
}

namespace f_0049c800 {
// MATCH: golf_clean.exe 0x0049c800 ?FUN_0049c800@C_FUN_0049c800@f_0049c800@@QAEPAHE@Z
void __fastcall FUN_0049c780(int *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_0049c800 { int * FUN_0049c800(byte param_1); };
int * C_FUN_0049c800::FUN_0049c800(byte param_1)
{
  FUN_0049c780((int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return (int *)(this);
}
}

namespace f_0049d020 {
// MATCH: golf_clean.exe 0x0049d020 ?FUN_0049d020@f_0049d020@@YAXXZ
void LAB_0049d030();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049d020()
{
  _atexit((unsigned int)(&LAB_0049d030));
  return;
}
}

namespace f_0049d070 {
// MATCH: golf_clean.exe 0x0049d070 ?FUN_0049d070@C_FUN_0049d070@f_0049d070@@QAEPAIE@Z
void __cdecl FUN_004a4ffc(undefined4);
void __fastcall thunk_FUN_00474810(undefined4 *);
struct C_FUN_0049d070 { undefined4 * FUN_0049d070(byte param_1); };
undefined4 * C_FUN_0049d070::FUN_0049d070(byte param_1)
{
  thunk_FUN_00474810((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return (unsigned int *)(this);
}
}
