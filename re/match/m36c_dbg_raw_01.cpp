// FLAGS sound.dll: /Od /ZI /GZ
// sound.dll functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (round 36 batch m36c, raw form:
// Ghidra's offsets, casts and FUN_/DAT_ names, decompiled with re/names reset in memory by
// log/c3/m36c_ghidra/M36cDecompileRaw.java). Names, types and layouts are still to be written: these are evidence of
// byte identity, not readable source. String literals are placeholders named by address (s_<addr>), never the game's text.

#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_1003aa50 {
// MATCH: sound.dll 0x1003aa50 ?FUN_1003aa50@f_1003aa50@@YAIXZ
extern int DAT_100b58a0;
extern int DAT_100b58a4;
extern int DAT_100b58a8;
undefined4 __cdecl FUN_1003aa50()
{
  DAT_100b58a0 = 10;
  DAT_100b58a4 = 0xb4;
  DAT_100b58a8 = 1;
  return 0;
}
}
