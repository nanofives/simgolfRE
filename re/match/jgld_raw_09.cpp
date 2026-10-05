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

namespace f_10067da0 {
// MATCH: jgld.dll 0x10067da0 ?FUN_10067da0@C_FUN_10067da0@f_10067da0@@QAEXPAI00@Z
extern int DAT_10122dc0;
extern int DAT_10122dc4;
extern int DAT_10122dc8;
struct C_FUN_10067da0 { void FUN_10067da0(undefined4 *param_1, undefined4 *param_2, undefined4 *param_3); };
void C_FUN_10067da0::FUN_10067da0(undefined4 *param_1, undefined4 *param_2, undefined4 *param_3)
{
  *param_1 = DAT_10122dc0;
  *param_2 = DAT_10122dc4;
  *param_3 = DAT_10122dc8;
  return;
}
}

namespace f_10067f60 {
// MATCH: jgld.dll 0x10067f60 ?FUN_10067f60@C_FUN_10067f60@f_10067f60@@QAEXI@Z
extern int DAT_10128728;
struct C_FUN_10067f60 { void FUN_10067f60(undefined4 param_1); };
void C_FUN_10067f60::FUN_10067f60(undefined4 param_1)
{
  DAT_10128728 = param_1;
  return;
}
}

namespace f_10069050 {
// MATCH: jgld.dll 0x10069050 ?FUN_10069050@C_FUN_10069050@f_10069050@@QAEIHHH@Z
struct C_FUN_10069050 { undefined4 FUN_10069050(int param_1, int param_2, int param_3); };
undefined4 C_FUN_10069050::FUN_10069050(int param_1, int param_2, int param_3)
{
  return 0x18;
}
}

namespace f_10069090 {
// MATCH: jgld.dll 0x10069090 ?FUN_10069090@C_FUN_10069090@f_10069090@@QAEIHHHH@Z
struct C_FUN_10069090 { undefined4 FUN_10069090(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10069090::FUN_10069090(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_100690d0 {
// MATCH: jgld.dll 0x100690d0 ?FUN_100690d0@C_FUN_100690d0@f_100690d0@@QAEXXZ
struct C_FUN_100690d0 { void FUN_100690d0(); };
void C_FUN_100690d0::FUN_100690d0()
{
  return;
}
}

namespace f_10069100 {
// MATCH: jgld.dll 0x10069100 ?FUN_10069100@C_FUN_10069100@f_10069100@@QAEIHH@Z
struct C_FUN_10069100 { undefined4 FUN_10069100(int param_1, int param_2); };
undefined4 C_FUN_10069100::FUN_10069100(int param_1, int param_2)
{
  return 0;
}
}
