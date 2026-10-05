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

namespace f_0049d170 {
// MATCH: golf_clean.exe 0x0049d170 ?FUN_0049d170@f_0049d170@@YAXXZ
void LAB_0049d180();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049d170()
{
  _atexit((unsigned int)(&LAB_0049d180));
  return;
}
}

namespace f_0049d2b0 {
// MATCH: golf_clean.exe 0x0049d2b0 ?FUN_0049d2b0@f_0049d2b0@@YAXXZ
void LAB_0049d2c0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049d2b0()
{
  _atexit((unsigned int)(&LAB_0049d2c0));
  return;
}
}

namespace f_0049d310 {
// MATCH: golf_clean.exe 0x0049d310 ?FUN_0049d310@f_0049d310@@YAXXZ
void LAB_0049d320();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049d310()
{
  _atexit((unsigned int)(&LAB_0049d320));
  return;
}
}

namespace f_0049d370 {
// MATCH: golf_clean.exe 0x0049d370 ?FUN_0049d370@f_0049d370@@YAXXZ
void LAB_0049d380();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049d370()
{
  _atexit((unsigned int)(&LAB_0049d380));
  return;
}
}

namespace f_0049d4a0 {
// MATCH: golf_clean.exe 0x0049d4a0 ?FUN_0049d4a0@f_0049d4a0@@YAXXZ
void LAB_0049d4b0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049d4a0()
{
  _atexit((unsigned int)(&LAB_0049d4b0));
  return;
}
}

namespace f_0049d500 {
// MATCH: golf_clean.exe 0x0049d500 ?FUN_0049d500@f_0049d500@@YAXXZ
void LAB_0049d510();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049d500()
{
  _atexit((unsigned int)(&LAB_0049d510));
  return;
}
}

namespace f_0049d560 {
// MATCH: golf_clean.exe 0x0049d560 ?FUN_0049d560@f_0049d560@@YAXXZ
void LAB_0049d570();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049d560()
{
  _atexit((unsigned int)(&LAB_0049d570));
  return;
}
}

namespace f_0049eb10 {
// MATCH: golf_clean.exe 0x0049eb10 ?FUN_0049eb10@f_0049eb10@@YAXXZ
void LAB_0049eb20();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049eb10()
{
  _atexit((unsigned int)(&LAB_0049eb20));
  return;
}
}

namespace f_0049eb70 {
// MATCH: golf_clean.exe 0x0049eb70 ?FUN_0049eb70@f_0049eb70@@YAXXZ
void LAB_0049eb80();
int __cdecl _atexit(undefined4);
void __cdecl FUN_0049eb70()
{
  _atexit((unsigned int)(&LAB_0049eb80));
  return;
}
}

namespace f_004a00f0 {
// MATCH: golf_clean.exe 0x004a00f0 ?FUN_004a00f0@f_004a00f0@@YAXPADI@Z
undefined1 * __cdecl FUN_00491da0(char *);
void __cdecl FUN_004a614d(undefined4, undefined4);
void __cdecl FUN_004a00f0(char *param_1, undefined4 param_2)
{
    char * pcVar1;
  pcVar1 = (char *)(FUN_00491da0(param_1));
  if (pcVar1 == (char *)0x0) {
    pcVar1 = param_1;
  }
  FUN_004a614d((unsigned int)(pcVar1),param_2);
  return;
}
}

namespace f_004a0140 {
// MATCH: golf_clean.exe 0x004a0140 ?FUN_004a0140@f_004a0140@@YAXXZ
void LAB_004a0150();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004a0140()
{
  _atexit((unsigned int)(&LAB_004a0150));
  return;
}
}

namespace f_004a0160 {
// MATCH: golf_clean.exe 0x004a0160 ?FUN_004a0160@C_FUN_004a0160@f_004a0160@@QAEPAXE@Z
void __fastcall FUN_004a01a0(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_004a0160 { void * FUN_004a0160(byte param_1); };
void * C_FUN_004a0160::FUN_004a0160(byte param_1)
{
  FUN_004a01a0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_004a01a0 {
// MATCH: golf_clean.exe 0x004a01a0 ?FUN_004a01a0@f_004a01a0@@YIXPAI@Z
extern void *PTR_FUN_004bc034;
void __cdecl _free(LPVOID);
void __fastcall FUN_004a01a0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004bc034);
  if ((LPVOID)param_1[1] != (LPVOID)0x0) {
    _free((LPVOID)param_1[1]);
    param_1[1] = 0;
  }
  return;
}
}

