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

namespace f_10024630 {
// MATCH: sound.dll 0x10024630 ?FUN_10024630@C_FUN_10024630@f_10024630@@QAEHI@Z
struct T_thunk_FUN_1001d780 { int thunk_FUN_1001d780(int *, uint); };
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); virtual int f3(); virtual int f4(); virtual int f5(); virtual int f6(); virtual int f7(); virtual int f8(); virtual int f9(); virtual int f10(); virtual int f11(); virtual int f12(); virtual int f13(int, int, int); };
struct C_FUN_10024630 { int FUN_10024630(uint param_1); };
int C_FUN_10024630::FUN_10024630(uint param_1)
{
    int iVar1;
  iVar1 = ((T_thunk_FUN_1001d780 *)((void *)((int)this + 0x60)))->thunk_FUN_1001d780((int *)&param_1, param_1);
  if (iVar1 == 0) {
    iVar1 = 0x254;
    *(byte *)(param_1 + 0x38) = *(byte *)(param_1 + 0x38) | 1;
    do {
      if (*(int **)(iVar1 + param_1) != 0) {
        ((VT_1 *)(*(int **)(iVar1 + param_1)))->f13((int)(0), (int)(0x7f), (int)(0x19));
      }
      iVar1 = iVar1 + -4;
    } while (iVar1 >= 0x58);
    iVar1 = 0;
  }
  return iVar1;
}
}

namespace f_10035150 {
// MATCH: sound.dll 0x10035150 ?FUN_10035150@C_FUN_10035150@f_10035150@@QAEXI@Z
struct T_thunk_FUN_1000f1c0 { int thunk_FUN_1000f1c0(uint); };
struct C_FUN_10035150 { void FUN_10035150(uint param_1); };
void C_FUN_10035150::FUN_10035150(uint param_1)
{
    undefined4 * puVar1;
    int iVar2;
  if (((*(uint *)((int)this + 0x58) & 0x10000000) != 0) && (*(void **)((int)this + 0x54) != 0)) {
    ((T_thunk_FUN_1000f1c0 *)(*(void **)((int)this + 0x54)))->thunk_FUN_1000f1c0(param_1);
    puVar1 = (undefined4 *)((int)this + 0x318);
    iVar2 = 0x10;
    do {
      if ((void *)*puVar1 != 0) {
        ((T_thunk_FUN_1000f1c0 *)((void *)*puVar1))->thunk_FUN_1000f1c0(param_1);
      }
      puVar1 = puVar1 + 0x44;
      iVar2 = iVar2 + -1;
    } while (iVar2 != 0);
  }
  return;
}
}
