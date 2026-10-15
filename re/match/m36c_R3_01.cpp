// FLAGS sound.dll: /O2
// m36c R3 (round 36): sound.dll release functions 0x10037780..0x1003a900, hand-matched from Ghidra's decompilation
// (raw form: Ghidra's offsets and FUN_ names). Evidence of byte identity, not readable source.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_1003a770 {
extern int DAT_100b4a3c;
// MATCH: sound.dll 0x1003a770 ?FUN_1003a770@f_1003a770@@YAHXZ
int FUN_1003a770(void)
{
  return DAT_100b4a3c;
}
}

namespace f_10039980 {
struct T_FUN_10033990 { void FUN_10033990(void); };
struct C_FUN_10039980 { void FUN_10039980(void); };
// The original's extent includes 5 trailing nops: re/match/boundaries.tsv gives it 8 bytes (verifier, round 36).
// MATCH: sound.dll 0x10039980 ?FUN_10039980@C_FUN_10039980@f_10039980@@QAEXXZ
void C_FUN_10039980::FUN_10039980(void)
{
  ((T_FUN_10033990 *)((int)this + 0x70))->FUN_10033990();
}
}

namespace f_1003a5e0 {
struct T_FUN_10037b40 { void FUN_10037b40(undefined4); };
struct C_FUN_1003a5e0 { void FUN_1003a5e0(undefined4 param_2); };
// MATCH: sound.dll 0x1003a5e0 ?FUN_1003a5e0@C_FUN_1003a5e0@f_1003a5e0@@QAEXI@Z
void C_FUN_1003a5e0::FUN_1003a5e0(undefined4 param_2)
{
  ((T_FUN_10037b40 *)((int)this + 0x70))->FUN_10037b40(param_2);
}
}

namespace f_1003a630 {
struct T_FUN_10037b80 { void FUN_10037b80(undefined4); };
struct C_FUN_1003a630 { void FUN_1003a630(undefined4 param_2); };
// MATCH: sound.dll 0x1003a630 ?FUN_1003a630@C_FUN_1003a630@f_1003a630@@QAEXI@Z
void C_FUN_1003a630::FUN_1003a630(undefined4 param_2)
{
  ((T_FUN_10037b80 *)((int)this + 0x70))->FUN_10037b80(param_2);
}
}

namespace f_1003a650 {
struct T_FUN_10037bc0 { void FUN_10037bc0(undefined4); };
struct C_FUN_1003a650 { void FUN_1003a650(undefined4 param_2); };
// MATCH: sound.dll 0x1003a650 ?FUN_1003a650@C_FUN_1003a650@f_1003a650@@QAEXI@Z
void C_FUN_1003a650::FUN_1003a650(undefined4 param_2)
{
  ((T_FUN_10037bc0 *)((int)this + 0x70))->FUN_10037bc0(param_2);
}
}

namespace f_1003a6e0 {
struct T_FUN_100355d0 { void FUN_100355d0(uint); };
struct C_FUN_1003a6e0 { int FUN_1003a6e0(uint param_2); };
// MATCH: sound.dll 0x1003a6e0 ?FUN_1003a6e0@C_FUN_1003a6e0@f_1003a6e0@@QAEHI@Z
int C_FUN_1003a6e0::FUN_1003a6e0(uint param_2)
{
  ((T_FUN_100355d0 *)((int)this + 0x70))->FUN_100355d0(param_2);
  return 0;
}
}

namespace f_1003a560 {
struct T_FUN_1002c0d0 { byte FUN_1002c0d0(void); };
struct C_FUN_1003a560 { byte FUN_1003a560(void); };
// MATCH: sound.dll 0x1003a560 ?FUN_1003a560@C_FUN_1003a560@f_1003a560@@QAEEXZ
byte C_FUN_1003a560::FUN_1003a560(void)
{
  byte uVar1 = ((T_FUN_1002c0d0 *)this)->FUN_1002c0d0();
  if (*(int *)((int)this + 0x228) != 0) {
    uVar1 = uVar1 | 8;
  }
  return uVar1;
}
}

namespace f_1003a600 {
struct T_FUN_1002c270 { void FUN_1002c270(undefined4); };
struct T_FUN_10037c00 { void FUN_10037c00(undefined4); };
struct C_FUN_1003a600 { void FUN_1003a600(undefined4 param_2); };
// MATCH: sound.dll 0x1003a600 ?FUN_1003a600@C_FUN_1003a600@f_1003a600@@QAEXI@Z
void C_FUN_1003a600::FUN_1003a600(undefined4 param_2)
{
  ((T_FUN_1002c270 *)this)->FUN_1002c270(param_2);
  ((T_FUN_10037c00 *)((int)this + 0x70))->FUN_10037c00(param_2);
}
}
