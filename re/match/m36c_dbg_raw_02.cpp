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

namespace f_1003c180 {
// MATCH: sound.dll 0x1003c180 ?FUN_1003c180@f_1003c180@@YAXPAHPAI1HHH@Z
undefined4 __cdecl FUN_1003bf30(int, int *, undefined4 *, undefined4 *, int, int, int);
void __cdecl FUN_1003c180(int *param_1, undefined4 *param_2, undefined4 *param_3, int param_4, int param_5, int param_6)
{
  FUN_1003bf30(0,param_1,param_2,param_3,param_4,param_5,param_6);
  return;
}
}

namespace f_1003c1d0 {
// MATCH: sound.dll 0x1003c1d0 ?FUN_1003c1d0@f_1003c1d0@@YAXPAHPAI1HH@Z
undefined4 __cdecl FUN_1003bf30(int, int *, undefined4 *, undefined4 *, int, int, int);
void __cdecl FUN_1003c1d0(int *param_1, undefined4 *param_2, undefined4 *param_3, int param_4, int param_5)
{
  FUN_1003bf30(1,param_1,param_2,param_3,param_4,param_5,0);
  return;
}
}

namespace f_1003d040 {
// MATCH: sound.dll 0x1003d040 ?FUN_1003d040@f_1003d040@@YAHHPAHPAM@Z
extern int DAT_10064a18;
undefined4 __cdecl FUN_10040e30(int, int *, int *, float *);
int __cdecl FUN_1003d040(int param_1, int *param_2, float *param_3)
{
  if (param_1 != 0) {
    param_1 = param_1 + -4;
  }
  FUN_10040e30(param_1 + 4,(int *)((char *)&DAT_10064a18),param_2,param_3);
  return 0;
}
}
