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

namespace f_004021b0 {
// MATCH: golf_clean.exe 0x004021b0 ?FUN_004021b0@f_004021b0@@YAXH@Z
extern int DAT_00839650;
void __cdecl _free(LPVOID);
void __cdecl FUN_004021b0(int param_1)
{
  if (param_1 != 0) {
    if (DAT_00839650 == 0) {
      _free((LPVOID)param_1);
    }
    DAT_00839650 = 0;
  }
  return;
}
}

namespace f_00403f40 {
// MATCH: golf_clean.exe 0x00403f40 ?FUN_00403f40@f_00403f40@@YAXXZ
void LAB_00403f50();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00403f40()
{
  _atexit((unsigned int)(&LAB_00403f50));
  return;
}
}

namespace f_00403fa0 {
// MATCH: golf_clean.exe 0x00403fa0 ?FUN_00403fa0@f_00403fa0@@YAXXZ
void LAB_00403fb0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00403fa0()
{
  _atexit((unsigned int)(&LAB_00403fb0));
  return;
}
}

namespace f_00404000 {
// MATCH: golf_clean.exe 0x00404000 ?FUN_00404000@f_00404000@@YAXXZ
void LAB_00404010();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404000()
{
  _atexit((unsigned int)(&LAB_00404010));
  return;
}
}

namespace f_00404060 {
// MATCH: golf_clean.exe 0x00404060 ?FUN_00404060@f_00404060@@YAXXZ
void LAB_00404070();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404060()
{
  _atexit((unsigned int)(&LAB_00404070));
  return;
}
}

namespace f_00404260 {
// MATCH: golf_clean.exe 0x00404260 ?FUN_00404260@C_FUN_00404260@f_00404260@@QAEPAIE@Z
void __fastcall FUN_00404200(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_00404260 { undefined4 * FUN_00404260(byte param_1); };
undefined4 * C_FUN_00404260::FUN_00404260(byte param_1)
{
  FUN_00404200((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return (unsigned int *)(this);
}
}

namespace f_00404330 {
// MATCH: golf_clean.exe 0x00404330 ?FUN_00404330@f_00404330@@YAXXZ
void LAB_00404340();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404330()
{
  _atexit((unsigned int)(&LAB_00404340));
  return;
}
}

namespace f_00404380 {
// MATCH: golf_clean.exe 0x00404380 ?FUN_00404380@C_FUN_00404380@f_00404380@@QAEPAXE@Z
void __cdecl FUN_004a4ffc(undefined4);
void __fastcall thunk_FUN_004805a0(int *);
struct C_FUN_00404380 { void * FUN_00404380(byte param_1); };
void * C_FUN_00404380::FUN_00404380(byte param_1)
{
  thunk_FUN_004805a0((int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return this;
}
}

namespace f_004043c0 {
// MATCH: golf_clean.exe 0x004043c0 ?FUN_004043c0@f_004043c0@@YAXXZ
void LAB_004043d0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004043c0()
{
  _atexit((unsigned int)(&LAB_004043d0));
  return;
}
}

namespace f_00404400 {
// MATCH: golf_clean.exe 0x00404400 ?FUN_00404400@f_00404400@@YAXXZ
void LAB_00404410();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404400()
{
  _atexit((unsigned int)(&LAB_00404410));
  return;
}
}

namespace f_00404450 {
// MATCH: golf_clean.exe 0x00404450 ?FUN_00404450@f_00404450@@YAXXZ
void LAB_00404460();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404450()
{
  _atexit((unsigned int)(&LAB_00404460));
  return;
}
}

namespace f_004044a0 {
// MATCH: golf_clean.exe 0x004044a0 ?FUN_004044a0@f_004044a0@@YAXXZ
void LAB_004044b0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004044a0()
{
  _atexit((unsigned int)(&LAB_004044b0));
  return;
}
}

namespace f_004044f0 {
// MATCH: golf_clean.exe 0x004044f0 ?FUN_004044f0@f_004044f0@@YAXXZ
void LAB_00404500();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004044f0()
{
  _atexit((unsigned int)(&LAB_00404500));
  return;
}
}

namespace f_00404540 {
// MATCH: golf_clean.exe 0x00404540 ?FUN_00404540@f_00404540@@YAXXZ
void LAB_00404550();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404540()
{
  _atexit((unsigned int)(&LAB_00404550));
  return;
}
}

namespace f_00404590 {
// MATCH: golf_clean.exe 0x00404590 ?FUN_00404590@f_00404590@@YAXXZ
void LAB_004045a0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404590()
{
  _atexit((unsigned int)(&LAB_004045a0));
  return;
}
}

namespace f_004045e0 {
// MATCH: golf_clean.exe 0x004045e0 ?FUN_004045e0@f_004045e0@@YAXXZ
void LAB_004045f0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004045e0()
{
  _atexit((unsigned int)(&LAB_004045f0));
  return;
}
}

namespace f_00404630 {
// MATCH: golf_clean.exe 0x00404630 ?FUN_00404630@f_00404630@@YAXXZ
void LAB_00404640();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404630()
{
  _atexit((unsigned int)(&LAB_00404640));
  return;
}
}

namespace f_00404680 {
// MATCH: golf_clean.exe 0x00404680 ?FUN_00404680@f_00404680@@YAXXZ
void LAB_00404690();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404680()
{
  _atexit((unsigned int)(&LAB_00404690));
  return;
}
}

namespace f_004046d0 {
// MATCH: golf_clean.exe 0x004046d0 ?FUN_004046d0@f_004046d0@@YAXXZ
void LAB_004046e0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004046d0()
{
  _atexit((unsigned int)(&LAB_004046e0));
  return;
}
}

namespace f_00404710 {
// MATCH: golf_clean.exe 0x00404710 ?FUN_00404710@f_00404710@@YAXXZ
void LAB_00404720();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404710()
{
  _atexit((unsigned int)(&LAB_00404720));
  return;
}
}

namespace f_00404760 {
// MATCH: golf_clean.exe 0x00404760 ?FUN_00404760@f_00404760@@YAXXZ
void LAB_00404770();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404760()
{
  _atexit((unsigned int)(&LAB_00404770));
  return;
}
}

namespace f_004047b0 {
// MATCH: golf_clean.exe 0x004047b0 ?FUN_004047b0@f_004047b0@@YAXXZ
void LAB_004047c0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004047b0()
{
  _atexit((unsigned int)(&LAB_004047c0));
  return;
}
}

namespace f_00404800 {
// MATCH: golf_clean.exe 0x00404800 ?FUN_00404800@f_00404800@@YAXXZ
void LAB_00404810();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404800()
{
  _atexit((unsigned int)(&LAB_00404810));
  return;
}
}

namespace f_00404850 {
// MATCH: golf_clean.exe 0x00404850 ?FUN_00404850@f_00404850@@YAXXZ
void LAB_00404860();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404850()
{
  _atexit((unsigned int)(&LAB_00404860));
  return;
}
}

namespace f_00404cf0 {
// MATCH: golf_clean.exe 0x00404cf0 ?FUN_00404cf0@f_00404cf0@@YAXXZ
void LAB_00404d00();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404cf0()
{
  _atexit((unsigned int)(&LAB_00404d00));
  return;
}
}

namespace f_00404e90 {
// MATCH: golf_clean.exe 0x00404e90 ?FUN_00404e90@f_00404e90@@YAXXZ
void LAB_00404ea0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00404e90()
{
  _atexit((unsigned int)(&LAB_00404ea0));
  return;
}
}

namespace f_00405000 {
// MATCH: golf_clean.exe 0x00405000 ?FUN_00405000@f_00405000@@YAXXZ
void LAB_00405010();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00405000()
{
  _atexit((unsigned int)(&LAB_00405010));
  return;
}
}

namespace f_00405150 {
// MATCH: golf_clean.exe 0x00405150 ?FUN_00405150@f_00405150@@YAXXZ
void LAB_00405160();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00405150()
{
  _atexit((unsigned int)(&LAB_00405160));
  return;
}
}

namespace f_004052d0 {
// MATCH: golf_clean.exe 0x004052d0 ?FUN_004052d0@f_004052d0@@YAXXZ
void LAB_004052e0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004052d0()
{
  _atexit((unsigned int)(&LAB_004052e0));
  return;
}
}

namespace f_00405440 {
// MATCH: golf_clean.exe 0x00405440 ?FUN_00405440@f_00405440@@YAXXZ
void LAB_00405450();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00405440()
{
  _atexit((unsigned int)(&LAB_00405450));
  return;
}
}

namespace f_00405580 {
// MATCH: golf_clean.exe 0x00405580 ?FUN_00405580@f_00405580@@YAXXZ
void LAB_00405590();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00405580()
{
  _atexit((unsigned int)(&LAB_00405590));
  return;
}
}

namespace f_004056c0 {
// MATCH: golf_clean.exe 0x004056c0 ?FUN_004056c0@f_004056c0@@YAXXZ
void LAB_004056d0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004056c0()
{
  _atexit((unsigned int)(&LAB_004056d0));
  return;
}
}

namespace f_00405830 {
// MATCH: golf_clean.exe 0x00405830 ?FUN_00405830@f_00405830@@YAXXZ
void LAB_00405840();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00405830()
{
  _atexit((unsigned int)(&LAB_00405840));
  return;
}
}

namespace f_00405900 {
// MATCH: golf_clean.exe 0x00405900 ?FUN_00405900@f_00405900@@YAXXZ
void LAB_00405910();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00405900()
{
  _atexit((unsigned int)(&LAB_00405910));
  return;
}
}

namespace f_00405a00 {
// MATCH: golf_clean.exe 0x00405a00 ?FUN_00405a00@f_00405a00@@YAXHHH@Z
void __cdecl FUN_0040c500(undefined4, int, int, int);
void __cdecl FUN_00405a00(int param_1, int param_2, int param_3)
{
  if (param_1 == 0) {
    FUN_0040c500(0x3b,param_2,param_3,0);
    return;
  }
  if (param_1 == 1) {
    FUN_0040c500(0x3a,param_2,param_3,0);
    return;
  }
  if (param_1 == 3) {
    FUN_0040c500(0xc3,param_2,param_3,0);
    return;
  }
  if (param_1 == 4) {
    FUN_0040c500(0xc4,param_2,param_3,0);
    return;
  }
  if (param_1 == 6) {
    FUN_0040c500(0xc5,param_2,param_3,0);
    return;
  }
  if (param_1 == 5) {
    FUN_0040c500(0xc6,param_2,param_3,0);
  }
  return;
}
}

namespace f_004061c0 {
// MATCH: golf_clean.exe 0x004061c0 ?FUN_004061c0@f_004061c0@@YAXXZ
void LAB_004061d0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004061c0()
{
  _atexit((unsigned int)(&LAB_004061d0));
  return;
}
}

namespace f_00406200 {
// MATCH: golf_clean.exe 0x00406200 ?FUN_00406200@f_00406200@@YAXXZ
void LAB_00406210();
int __cdecl _atexit(undefined4);
void __cdecl FUN_00406200()
{
  _atexit((unsigned int)(&LAB_00406210));
  return;
}
}

namespace f_004378f0 {
// MATCH: golf_clean.exe 0x004378f0 ?FUN_004378f0@f_004378f0@@YAXXZ
void LAB_00437900();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004378f0()
{
  _atexit((unsigned int)(&LAB_00437900));
  return;
}
}

namespace f_004385b0 {
// MATCH: golf_clean.exe 0x004385b0 ?FUN_004385b0@f_004385b0@@YAXXZ
void LAB_004385c0();
int __cdecl _atexit(undefined4);
void __cdecl FUN_004385b0()
{
  _atexit((unsigned int)(&LAB_004385c0));
  return;
}
}

namespace f_0043cbd0 {
// MATCH: golf_clean.exe 0x0043cbd0 ?FUN_0043cbd0@C_FUN_0043cbd0@f_0043cbd0@@QAEPAIE@Z
void __fastcall FUN_0043cb50(undefined4 *);
void __cdecl FUN_004a4ffc(undefined4);
struct C_FUN_0043cbd0 { undefined4 * FUN_0043cbd0(byte param_1); };
undefined4 * C_FUN_0043cbd0::FUN_0043cbd0(byte param_1)
{
  FUN_0043cb50((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    FUN_004a4ffc((unsigned int)(this));
  }
  return (unsigned int *)(this);
}
}
