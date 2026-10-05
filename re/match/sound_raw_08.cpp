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

namespace f_1000b110 {
// MATCH: sound.dll 0x1000b110 ?FUN_1000b110@f_1000b110@@YGIPAD@Z
extern int DAT_100b49f4;
struct T_thunk_FUN_100086f0 { undefined4 thunk_FUN_100086f0(LPSTR); };
undefined4 __stdcall FUN_1000b110(LPSTR param_1)
{
  if (DAT_100b49f4 != 0) {
    return (((T_thunk_FUN_100086f0 *)(DAT_100b49f4))->thunk_FUN_100086f0(param_1));
  }
  return 3;
}
}

namespace f_1000e2a0 {
// MATCH: sound.dll 0x1000e2a0 ?FUN_1000e2a0@f_1000e2a0@@YGXI@Z
extern int DAT_100b49f0;
struct T_thunk_FUN_10013560 { int thunk_FUN_10013560(uint); };
void __stdcall FUN_1000e2a0(uint param_1)
{
  if (DAT_100b49f0 != 0) {
    ((T_thunk_FUN_10013560 *)(DAT_100b49f0))->thunk_FUN_10013560(param_1);
  }
  return;
}
}

namespace f_1000ed70 {
// MATCH: sound.dll 0x1000ed70 ?FUN_1000ed70@f_1000ed70@@YGII@Z
extern int DAT_100b4a04;
struct T_thunk_FUN_10039450 { undefined4 thunk_FUN_10039450(undefined4); };
undefined4 __stdcall FUN_1000ed70(undefined4 param_1)
{
  if (DAT_100b4a04 == 0) {
    return 0x13;
  }
  return (((T_thunk_FUN_10039450 *)(DAT_100b4a04))->thunk_FUN_10039450(param_1));
}
}
