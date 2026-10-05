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

namespace f_00402130 {
// MATCH: golf_clean.exe 0x00402130 ?FUN_00402130@f_00402130@@YIIH@Z
undefined4 __fastcall FUN_00402130(int param_1)
{
    int iVar1;
  if (*(int *)(param_1 + 8) == 0) {
    return 0;
  }
  *(undefined4 *)(param_1 + 0xc) = *(undefined4 *)(*(int *)(param_1 + 0xc) + 0xc);
  iVar1 = *(int *)(param_1 + 0x14) + 1;
  *(int *)(param_1 + 0x14) = iVar1;
  if (iVar1 == *(int *)(param_1 + 0x10)) {
    *(undefined4 *)(param_1 + 0x14) = 0;
  }
  return *(undefined4 *)(*(int *)(param_1 + 0xc) + 8);
}
}

namespace f_00402160 {
// MATCH: golf_clean.exe 0x00402160 ?FUN_00402160@f_00402160@@YIIH@Z
undefined4 __fastcall FUN_00402160(int param_1)
{
  if (*(int *)(param_1 + 8) != 0) {
    return *(undefined4 *)(*(int *)(param_1 + 0xc) + 8);
  }
  return 0;
}
}

namespace f_004021a0 {
// MATCH: golf_clean.exe 0x004021a0 ?FUN_004021a0@f_004021a0@@YIXPAI@Z
extern int DAT_00839650;
extern void *PTR_FUN_004ba278;
void __fastcall FUN_004021a0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_FUN_004ba278);
  DAT_00839650 = param_1[1];
  return;
}
}

namespace f_004041f0 {
// MATCH: golf_clean.exe 0x004041f0 ?FUN_004041f0@f_004041f0@@YIXPAI@Z
extern void *PTR_LAB_004ba2d8;
void __fastcall FUN_00473ae0(int);
void __fastcall FUN_004041f0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_004ba2d8);
  FUN_00473ae0((int)param_1);
  return;
}
}

namespace f_00404320 {
// MATCH: golf_clean.exe 0x00404320 ?FUN_00404320@f_00404320@@YAXXZ
extern int DAT_00519a60;
undefined4 * __fastcall FUN_00404360(undefined4 *);
void __cdecl FUN_00404320()
{
  FUN_00404360((undefined4 *)((char *)&DAT_00519a60));
  return;
}
}

namespace f_00404360 {
// MATCH: golf_clean.exe 0x00404360 ?FUN_00404360@f_00404360@@YIPAIPAI@Z
extern void *PTR_FUN_004ba2fc;
extern void *PTR_LAB_004ba2e8;
undefined4 * __fastcall FUN_004804a0(undefined4 *);
undefined4 * __fastcall FUN_00404360(undefined4 *param_1)
{
  FUN_004804a0(param_1);
  *param_1 = (unsigned int)(&PTR_FUN_004ba2fc);
  param_1[0x9d] = (unsigned int)(&PTR_LAB_004ba2e8);
  return param_1;
}
}

namespace f_004043b0 {
// MATCH: golf_clean.exe 0x004043b0 ?FUN_004043b0@f_004043b0@@YAXXZ
extern int DAT_0051b068;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_004043b0()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_0051b068));
  return;
}
}

namespace f_004043f0 {
// MATCH: golf_clean.exe 0x004043f0 ?FUN_004043f0@f_004043f0@@YAXXZ
extern int DAT_00519928;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_004043f0()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_00519928));
  return;
}
}

namespace f_00404440 {
// MATCH: golf_clean.exe 0x00404440 ?FUN_00404440@f_00404440@@YAXXZ
extern int DAT_00519fd8;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404440()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_00519fd8));
  return;
}
}

namespace f_00404490 {
// MATCH: golf_clean.exe 0x00404490 ?FUN_00404490@f_00404490@@YAXXZ
extern int DAT_0051b320;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404490()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_0051b320));
  return;
}
}

namespace f_004044e0 {
// MATCH: golf_clean.exe 0x004044e0 ?FUN_004044e0@f_004044e0@@YAXXZ
extern int DAT_0051b360;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_004044e0()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_0051b360));
  return;
}
}

namespace f_00404530 {
// MATCH: golf_clean.exe 0x00404530 ?FUN_00404530@f_00404530@@YAXXZ
extern int DAT_00519948;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404530()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_00519948));
  return;
}
}

namespace f_00404580 {
// MATCH: golf_clean.exe 0x00404580 ?FUN_00404580@f_00404580@@YAXXZ
extern int DAT_00519a40;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404580()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_00519a40));
  return;
}
}

namespace f_004045d0 {
// MATCH: golf_clean.exe 0x004045d0 ?FUN_004045d0@f_004045d0@@YAXXZ
extern int DAT_0051a028;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_004045d0()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_0051a028));
  return;
}
}

namespace f_00404620 {
// MATCH: golf_clean.exe 0x00404620 ?FUN_00404620@f_00404620@@YAXXZ
extern int DAT_0051b340;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404620()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_0051b340));
  return;
}
}

namespace f_00404670 {
// MATCH: golf_clean.exe 0x00404670 ?FUN_00404670@f_00404670@@YAXXZ
extern int DAT_00519968;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404670()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_00519968));
  return;
}
}

namespace f_004046c0 {
// MATCH: golf_clean.exe 0x004046c0 ?FUN_004046c0@f_004046c0@@YAXXZ
extern int DAT_005199c8;
void __fastcall FUN_00482fd0(undefined4 *);
void __cdecl FUN_004046c0()
{
  FUN_00482fd0((undefined4 *)((char *)&DAT_005199c8));
  return;
}
}

namespace f_00404700 {
// MATCH: golf_clean.exe 0x00404700 ?FUN_00404700@f_00404700@@YAXXZ
extern int DAT_00519a20;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404700()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_00519a20));
  return;
}
}

namespace f_00404750 {
// MATCH: golf_clean.exe 0x00404750 ?FUN_00404750@f_00404750@@YAXXZ
extern int DAT_0051a008;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404750()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_0051a008));
  return;
}
}

namespace f_004047a0 {
// MATCH: golf_clean.exe 0x004047a0 ?FUN_004047a0@f_004047a0@@YAXXZ
extern int DAT_00519988;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_004047a0()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_00519988));
  return;
}
}

namespace f_004047f0 {
// MATCH: golf_clean.exe 0x004047f0 ?FUN_004047f0@f_004047f0@@YAXXZ
extern int DAT_005199a8;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_004047f0()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_005199a8));
  return;
}
}

namespace f_00404840 {
// MATCH: golf_clean.exe 0x00404840 ?FUN_00404840@f_00404840@@YAXXZ
extern int DAT_0051a048;
void __fastcall FUN_004837f0(undefined4 *);
void __cdecl FUN_00404840()
{
  FUN_004837f0((undefined4 *)((char *)&DAT_0051a048));
  return;
}
}

namespace f_004049a0 {
// MATCH: golf_clean.exe 0x004049a0 ?FUN_004049a0@f_004049a0@@YAXPAXIIIIE@Z
struct T_FUN_00478b80 { void FUN_00478b80(undefined4, undefined4, undefined4, undefined4, undefined4); };
void __cdecl FUN_004049a0(void *param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4, undefined4 param_5, byte param_6)
{
  ((T_FUN_00478b80 *)(param_1))->FUN_00478b80(param_2, param_3, param_4, param_5, (uint)param_6);
  return;
}
}

namespace f_004058f0 {
// MATCH: golf_clean.exe 0x004058f0 ?FUN_004058f0@f_004058f0@@YAXXZ
extern int DAT_005a7148;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_004058f0()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_005a7148));
  return;
}
}

namespace f_004061b0 {
// MATCH: golf_clean.exe 0x004061b0 ?FUN_004061b0@f_004061b0@@YAXXZ
extern int DAT_00568d10;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_004061b0()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_00568d10));
  return;
}
}

namespace f_004061f0 {
// MATCH: golf_clean.exe 0x004061f0 ?FUN_004061f0@f_004061f0@@YAXXZ
extern int DAT_00587ae8;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_004061f0()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_00587ae8));
  return;
}
}

namespace f_00409cb0 {
// MATCH: golf_clean.exe 0x00409cb0 ?FUN_00409cb0@f_00409cb0@@YAXIII@Z
extern int DAT_00586b50;
extern int DAT_00586fa8;
extern int DAT_005a8834;
extern int DAT_005a9cd4;
void __cdecl FUN_00409cb0(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  if (DAT_005a9cd4 < 0x100) {
    *(undefined4 *)(((char *)&DAT_00586b50) + DAT_005a9cd4 * 4) = param_1;
    *(undefined4 *)(((char *)&DAT_00586fa8) + DAT_005a9cd4 * 4) = param_2;
    *(undefined4 *)(((char *)&DAT_005a8834) + DAT_005a9cd4 * 4) = param_3;
    DAT_005a9cd4 = DAT_005a9cd4 + 1;
  }
  return;
}
}

namespace f_004378e0 {
// MATCH: golf_clean.exe 0x004378e0 ?FUN_004378e0@f_004378e0@@YAXXZ
extern int DAT_005aa560;
void __fastcall FUN_00482fd0(undefined4 *);
void __cdecl FUN_004378e0()
{
  FUN_00482fd0((undefined4 *)((char *)&DAT_005aa560));
  return;
}
}

namespace f_004385a0 {
// MATCH: golf_clean.exe 0x004385a0 ?FUN_004385a0@f_004385a0@@YAXXZ
extern int DAT_005a9f78;
undefined4 * __fastcall FUN_00486070(undefined4 *);
void __cdecl FUN_004385a0()
{
  FUN_00486070((undefined4 *)((char *)&DAT_005a9f78));
  return;
}
}

namespace f_0043cc70 {
// MATCH: golf_clean.exe 0x0043cc70 ?FUN_0043cc70@f_0043cc70@@YAXXZ
extern int DAT_005aa6f0;
void __fastcall FUN_00482fd0(undefined4 *);
void __cdecl FUN_0043cc70()
{
  FUN_00482fd0((undefined4 *)((char *)&DAT_005aa6f0));
  return;
}
}

namespace f_0043ccb0 {
// MATCH: golf_clean.exe 0x0043ccb0 ?FUN_0043ccb0@f_0043ccb0@@YAXXZ
extern int DAT_005aa6d0;
void __fastcall FUN_00487000(undefined4 *);
void __cdecl FUN_0043ccb0()
{
  FUN_00487000((undefined4 *)((char *)&DAT_005aa6d0));
  return;
}
}

namespace f_0043d4e0 {
// MATCH: golf_clean.exe 0x0043d4e0 ?FUN_0043d4e0@f_0043d4e0@@YAXXZ
extern int DAT_00820e90;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0043d4e0()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00820e90));
  return;
}
}

namespace f_0043d640 {
// MATCH: golf_clean.exe 0x0043d640 ?FUN_0043d640@f_0043d640@@YAXXZ
extern int DAT_005aa778;
undefined4 * __fastcall FUN_00474ae0(undefined4 *);
void __cdecl FUN_0043d640()
{
  FUN_00474ae0((undefined4 *)((char *)&DAT_005aa778));
  return;
}
}

namespace f_004491c0 {
// MATCH: golf_clean.exe 0x004491c0 ?FUN_004491c0@f_004491c0@@YAXXZ
extern int DAT_00820ed8;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_004491c0()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00820ed8));
  return;
}
}

namespace f_0044ad80 {
// MATCH: golf_clean.exe 0x0044ad80 ?FUN_0044ad80@f_0044ad80@@YAXXZ
extern int DAT_00821040;
void __fastcall FUN_00473ab0(undefined4 *);
void __cdecl FUN_0044ad80()
{
  FUN_00473ab0((undefined4 *)((char *)&DAT_00821040));
  return;
}
}

namespace f_0044add0 {
// MATCH: golf_clean.exe 0x0044add0 ?FUN_0044add0@f_0044add0@@YAXXZ
extern int DAT_008210c8;
undefined4 __fastcall FUN_0044ae60(undefined4);
void __cdecl FUN_0044add0()
{
  FUN_0044ae60((unsigned int)(((char *)&DAT_008210c8)));
  return;
}
}

namespace f_0044aec0 {
// MATCH: golf_clean.exe 0x0044aec0 ?FUN_0044aec0@f_0044aec0@@YAXXZ
extern int DAT_00821c60;
undefined4 __fastcall FUN_0044ae60(undefined4);
void __cdecl FUN_0044aec0()
{
  FUN_0044ae60((unsigned int)(((char *)&DAT_00821c60)));
  return;
}
}

namespace f_0044af00 {
// MATCH: golf_clean.exe 0x0044af00 ?FUN_0044af00@f_0044af00@@YAXXZ
extern int DAT_00821bf8;
undefined4 __fastcall FUN_0044ae60(undefined4);
void __cdecl FUN_0044af00()
{
  FUN_0044ae60((unsigned int)(((char *)&DAT_00821bf8)));
  return;
}
}

namespace f_0044af40 {
// MATCH: golf_clean.exe 0x0044af40 ?FUN_0044af40@f_0044af40@@YAXXZ
extern int DAT_00821070;
undefined4 __fastcall FUN_0044ae60(undefined4);
void __cdecl FUN_0044af40()
{
  FUN_0044ae60((unsigned int)(((char *)&DAT_00821070)));
  return;
}
}

namespace f_0044af80 {
// MATCH: golf_clean.exe 0x0044af80 ?FUN_0044af80@f_0044af80@@YAXXZ
extern int DAT_00821200;
undefined4 __fastcall FUN_0044b060(undefined4);
void __cdecl FUN_0044af80()
{
  FUN_0044b060((unsigned int)(((char *)&DAT_00821200)));
  return;
}
}
