// FLAGS sound.dll: /O2
// m36c (round 36): sound.dll release functions hand-fixed from ghidra2src output (raw form: Ghidra's offsets and
// FUN_ names). Evidence of byte identity, not readable source.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10037b00 {
// Ghidra typed the result byte; the original returns `mov eax, 0xb`, so int (and the callee's eax passes through).
struct T_thunk_FUN_10011000 { int thunk_FUN_10011000(undefined4, undefined4, undefined4); };
struct C_FUN_10037b00 { int FUN_10037b00(undefined4 param_1, undefined4 param_2, undefined4 param_3); };

// MATCH: sound.dll 0x10037b00 ?FUN_10037b00@C_FUN_10037b00@f_10037b00@@QAEHIII@Z
int C_FUN_10037b00::FUN_10037b00(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  if (((*(byte *)((int)this + 0x5c) & 0x20) != 0) && (*(void **)((int)this + 0x54) != 0)) {
    return (((T_thunk_FUN_10011000 *)(*(void **)((int)this + 0x54)))->thunk_FUN_10011000(param_1, param_2, param_3));
  }
  return 0xb;
}
}
