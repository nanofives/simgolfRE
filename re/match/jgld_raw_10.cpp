// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (raw form: Ghidra's offsets,
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

namespace f_10064fd0 {
// MATCH: jgld.dll 0x10064fd0 ?FUN_10064fd0@f_10064fd0@@YAXXZ
int __stdcall FUN_10065010();
int __stdcall FUN_10065050();
void __cdecl FUN_10064fd0()
{
  FUN_10065010();
  FUN_10065050();
  return;
}
}

namespace f_10065010 {
// MATCH: jgld.dll 0x10065010 ?FUN_10065010@f_10065010@@YAXXZ
extern int DAT_101286e0;
int __fastcall thunk_FUN_10067fa0(undefined4 *);
void __cdecl FUN_10065010()
{
  thunk_FUN_10067fa0((undefined4 *)((char *)&DAT_101286e0));
  return;
}
}

namespace f_10065050 {
// MATCH: jgld.dll 0x10065050 ?FUN_10065050@f_10065050@@YAXXZ
extern int DAT_100650a0;
int __cdecl _atexit(int);
void __cdecl FUN_10065050()
{
  _atexit((int)(((char *)&DAT_100650a0)));
  return;
}
}

namespace f_100650a0 {
// MATCH: jgld.dll 0x100650a0 ?FUN_100650a0@f_100650a0@@YAXXZ
extern int DAT_101286e0;
int __fastcall thunk_FUN_100669e0(undefined4 *);
void __cdecl FUN_100650a0()
{
  thunk_FUN_100669e0((undefined4 *)((char *)&DAT_101286e0));
  return;
}
}

namespace f_100650e0 {
// MATCH: jgld.dll 0x100650e0 ?FUN_100650e0@f_100650e0@@YAXXZ
int __stdcall FUN_10065120();
int __stdcall FUN_10065160();
void __cdecl FUN_100650e0()
{
  FUN_10065120();
  FUN_10065160();
  return;
}
}

namespace f_10065120 {
// MATCH: jgld.dll 0x10065120 ?FUN_10065120@f_10065120@@YAXXZ
extern int DAT_101286c8;
int __fastcall thunk_FUN_100680a0(undefined4 *);
void __cdecl FUN_10065120()
{
  thunk_FUN_100680a0((undefined4 *)((char *)&DAT_101286c8));
  return;
}
}

namespace f_10065160 {
// MATCH: jgld.dll 0x10065160 ?FUN_10065160@f_10065160@@YAXXZ
extern int DAT_100651b0;
int __cdecl _atexit(int);
void __cdecl FUN_10065160()
{
  _atexit((int)(((char *)&DAT_100651b0)));
  return;
}
}

namespace f_100651b0 {
// MATCH: jgld.dll 0x100651b0 ?FUN_100651b0@f_100651b0@@YAXXZ
extern int DAT_101286c8;
int __fastcall thunk_FUN_10066a30(undefined4 *);
void __cdecl FUN_100651b0()
{
  thunk_FUN_10066a30((undefined4 *)((char *)&DAT_101286c8));
  return;
}
}

namespace f_100651f0 {
// MATCH: jgld.dll 0x100651f0 ?FUN_100651f0@f_100651f0@@YAXXZ
int __stdcall FUN_10065230();
int __stdcall FUN_10065270();
void __cdecl FUN_100651f0()
{
  FUN_10065230();
  FUN_10065270();
  return;
}
}

namespace f_10065230 {
// MATCH: jgld.dll 0x10065230 ?FUN_10065230@f_10065230@@YAXXZ
extern int DAT_101286f8;
int __fastcall thunk_FUN_10068300(undefined4 *);
void __cdecl FUN_10065230()
{
  thunk_FUN_10068300((undefined4 *)((char *)&DAT_101286f8));
  return;
}
}

namespace f_10065270 {
// MATCH: jgld.dll 0x10065270 ?FUN_10065270@f_10065270@@YAXXZ
extern int DAT_100652c0;
int __cdecl _atexit(int);
void __cdecl FUN_10065270()
{
  _atexit((int)(((char *)&DAT_100652c0)));
  return;
}
}

namespace f_100652c0 {
// MATCH: jgld.dll 0x100652c0 ?FUN_100652c0@f_100652c0@@YAXXZ
extern int DAT_101286f8;
int __fastcall thunk_FUN_10066b00(undefined4 *);
void __cdecl FUN_100652c0()
{
  thunk_FUN_10066b00((undefined4 *)((char *)&DAT_101286f8));
  return;
}
}

namespace f_10067e00 {
// MATCH: jgld.dll 0x10067e00 ?FUN_10067e00@C_FUN_10067e00@f_10067e00@@QAEXXZ
int __stdcall thunk_FUN_10068f30();
struct C_FUN_10067e00 { void FUN_10067e00(); };
void C_FUN_10067e00::FUN_10067e00()
{
  thunk_FUN_10068f30();
  return;
}
}

namespace f_10067e40 {
// MATCH: jgld.dll 0x10067e40 ?FUN_10067e40@C_FUN_10067e40@f_10067e40@@QAEXXZ
int __stdcall thunk_FUN_10068fa0();
struct C_FUN_10067e40 { void FUN_10067e40(); };
void C_FUN_10067e40::FUN_10067e40()
{
  thunk_FUN_10068fa0();
  return;
}
}

namespace f_10067e80 {
// MATCH: jgld.dll 0x10067e80 ?FUN_10067e80@C_FUN_10067e80@f_10067e80@@QAEXPAI0@Z
int __cdecl thunk_FUN_1006aa70(undefined4 *, undefined4 *);
struct C_FUN_10067e80 { void FUN_10067e80(undefined4 *param_1, undefined4 *param_2); };
void C_FUN_10067e80::FUN_10067e80(undefined4 *param_1, undefined4 *param_2)
{
  thunk_FUN_1006aa70(param_1,param_2);
  return;
}
}

namespace f_10067ed0 {
// MATCH: jgld.dll 0x10067ed0 ?FUN_10067ed0@C_FUN_10067ed0@f_10067ed0@@QAEXXZ
int __stdcall thunk_FUN_1006ad60();
struct C_FUN_10067ed0 { void FUN_10067ed0(); };
void C_FUN_10067ed0::FUN_10067ed0()
{
  thunk_FUN_1006ad60();
  return;
}
}

namespace f_10069a70 {
// MATCH: jgld.dll 0x10069a70 ?FUN_10069a70@f_10069a70@@YAXPAUHWND__@@0@Z
int __cdecl thunk_FUN_100699c0(HWND);
void __cdecl FUN_10069a70(HWND param_1, HWND param_2)
{
  if (param_1 != param_2) {
    thunk_FUN_100699c0(param_1);
  }
  return;
}
}

namespace f_10069db0 {
// MATCH: jgld.dll 0x10069db0 ?FUN_10069db0@f_10069db0@@YAXIIII@Z
extern int DAT_1012872c;
struct VT_1 {  virtual int f0(int, int); };
void __cdecl FUN_10069db0(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  ((VT_1 *)(DAT_1012872c))->f0((int)(param_3), (int)(param_4));
  return;
}
}

namespace f_10069e10 {
// MATCH: jgld.dll 0x10069e10 ?FUN_10069e10@f_10069e10@@YAXIII@Z
extern int DAT_1012872c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(int, int); };
void __cdecl FUN_10069e10(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  ((VT_1 *)(DAT_1012872c))->f2((int)(param_2), (int)(param_3));
  return;
}
}

namespace f_10069e70 {
// MATCH: jgld.dll 0x10069e70 ?FUN_10069e70@f_10069e70@@YAXIIII@Z
extern int DAT_1012872c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(int, int, int); };
void __cdecl FUN_10069e70(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  ((VT_1 *)(DAT_1012872c))->f6((int)(param_2), (int)(param_3), (int)(param_4));
  return;
}
}

namespace f_10069ee0 {
// MATCH: jgld.dll 0x10069ee0 ?FUN_10069ee0@f_10069ee0@@YAXIII@Z
extern int DAT_1012872c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(int, int); };
void __cdecl FUN_10069ee0(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  ((VT_1 *)(DAT_1012872c))->f5((int)(param_2), (int)(param_3));
  return;
}
}

namespace f_10069f40 {
// MATCH: jgld.dll 0x10069f40 ?FUN_10069f40@f_10069f40@@YAXIIII@Z
extern int DAT_1012872c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(int, int, int); };
void __cdecl FUN_10069f40(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  ((VT_1 *)(DAT_1012872c))->f7((int)(param_2), (int)(param_3), (int)(param_4));
  return;
}
}

namespace f_10069fb0 {
// MATCH: jgld.dll 0x10069fb0 ?FUN_10069fb0@f_10069fb0@@YAXIII@Z
extern int DAT_1012872c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(int, int); };
void __cdecl FUN_10069fb0(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  ((VT_1 *)(DAT_1012872c))->f8((int)(param_2), (int)(param_3));
  return;
}
}

namespace f_1006a010 {
// MATCH: jgld.dll 0x1006a010 ?FUN_1006a010@f_1006a010@@YAXIIII@Z
extern int DAT_1012872c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(int, int, int); };
void __cdecl FUN_1006a010(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4)
{
  ((VT_1 *)(DAT_1012872c))->f9((int)(param_2), (int)(param_3), (int)(param_4));
  return;
}
}

namespace f_1006a080 {
// MATCH: jgld.dll 0x1006a080 ?FUN_1006a080@f_1006a080@@YAXIII@Z
extern int DAT_1012872c;
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(int, int); };
void __cdecl FUN_1006a080(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  ((VT_1 *)(DAT_1012872c))->f10((int)(param_2), (int)(param_3));
  return;
}
}

namespace f_1006ad60 {
// MATCH: jgld.dll 0x1006ad60 ?FUN_1006ad60@f_1006ad60@@YAXXZ
extern int DAT_10128738;
void __cdecl FUN_1006ad60()
{
  if ((int)(DAT_10128738) != (int)((HGDIOBJ)0x0)) {
    DeleteObject((void *)(DAT_10128738));
    DAT_10128738 = (int)((HGDIOBJ)0x0);
  }
  return;
}
}

