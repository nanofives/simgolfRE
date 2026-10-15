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

namespace f_1001b510 {
// MATCH: sound.dll 0x1001b510 ?FUN_1001b510@f_1001b510@@YAXXZ
extern int DAT_100b4b50;
void LAB_1001b550();
struct T_Ctrl5b19c_ctor { undefined4 * Ctrl5b19c_ctor(undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined4); };
int __cdecl _atexit(undefined4);
void __cdecl FUN_1001b510()
{
  ((T_Ctrl5b19c_ctor *)(((char *)&DAT_100b4b50)))->Ctrl5b19c_ctor(2, 2, 4, 4, 5, 7, 7, 9, 9, 0xb, 0xb, 0xc, 0);
  _atexit((unsigned int)(&LAB_1001b550));
  return;
}
}

namespace f_1001b6b0 {
// MATCH: sound.dll 0x1001b6b0 ?FUN_1001b6b0@f_1001b6b0@@YAXXZ
extern int DAT_100b4c38;
void LAB_1001b6f0();
struct T_Ctrl5b19c_ctor { undefined4 * Ctrl5b19c_ctor(undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined4); };
int __cdecl _atexit(undefined4);
void __cdecl FUN_1001b6b0()
{
  ((T_Ctrl5b19c_ctor *)(((char *)&DAT_100b4c38)))->Ctrl5b19c_ctor(4, 4, 5, 5, 7, 9, 9, 0xb, 0xb, 0xc, 0xc, 0xe, 1);
  _atexit((unsigned int)(&LAB_1001b6f0));
  return;
}
}

namespace f_1001b710 {
// MATCH: sound.dll 0x1001b710 ?FUN_1001b710@f_1001b710@@YAXXZ
extern int DAT_100b4d20;
void LAB_1001b750();
struct T_Ctrl5b19c_ctor { undefined4 * Ctrl5b19c_ctor(undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined4); };
int __cdecl _atexit(undefined4);
void __cdecl FUN_1001b710()
{
  ((T_Ctrl5b19c_ctor *)(((char *)&DAT_100b4d20)))->Ctrl5b19c_ctor(5, 5, 7, 7, 9, 0xb, 0xb, 0xc, 0xc, 0xe, 0xe, 0x10, 2);
  _atexit((unsigned int)(&LAB_1001b750));
  return;
}
}

namespace f_1001b770 {
// MATCH: sound.dll 0x1001b770 ?FUN_1001b770@f_1001b770@@YAXXZ
extern int DAT_100b4a68;
void LAB_1001b7b0();
struct T_Ctrl5b19c_ctor { undefined4 * Ctrl5b19c_ctor(undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined4); };
int __cdecl _atexit(undefined4);
void __cdecl FUN_1001b770()
{
  ((T_Ctrl5b19c_ctor *)(((char *)&DAT_100b4a68)))->Ctrl5b19c_ctor(7, 7, 9, 9, 0xb, 0xc, 0xc, 0xe, 0xe, 0x10, 0x10, 0x11, 3);
  _atexit((unsigned int)(&LAB_1001b7b0));
  return;
}
}

namespace f_1001b7d0 {
// MATCH: sound.dll 0x1001b7d0 ?FUN_1001b7d0@f_1001b7d0@@YAXXZ
extern int DAT_100b4e08;
void LAB_1001b810();
struct T_Ctrl5b19c_ctor { undefined4 * Ctrl5b19c_ctor(undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined1, undefined4); };
int __cdecl _atexit(undefined4);
void __cdecl FUN_1001b7d0()
{
  ((T_Ctrl5b19c_ctor *)(((char *)&DAT_100b4e08)))->Ctrl5b19c_ctor(9, 9, 0xb, 0xb, 0xc, 0xe, 0xe, 0x10, 0x10, 0x11, 0x11, 0x13, 4);
  _atexit((unsigned int)(&LAB_1001b810));
  return;
}
}
