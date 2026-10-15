// FLAGS sound.dll: /O2
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

namespace f_100316c0 {
// MATCH: sound.dll 0x100316c0 ?FUN_100316c0@C_FUN_100316c0@f_100316c0@@QAEIPAI@Z
extern int DAT_100b4a08;
extern int DAT_100b5084;
extern int DAT_100b5158;
extern int DAT_100b515c;
extern int DAT_100b542c;
extern float _DAT_1005c5e0;
int __cdecl FUN_1003b150(int, int, float *);
void * __cdecl operator_new(uint);
struct T_thunk_FUN_10031b60 { int thunk_FUN_10031b60(undefined4); };
struct C_FUN_100316c0 { /* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ undefined4 FUN_100316c0(undefined4 *param_1); };
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ undefined4 C_FUN_100316c0::FUN_100316c0(undefined4 *param_1)
{
    byte bVar2;
    undefined4 * puVar3;
    HMMIO pHVar4;
    float * pfVar5;
    float * pfVar6;
    int iVar7;
    short * psVar8;
    byte * pbVar9;
    byte bVar10;
    int * piVar11;
    _MMIOINFO * p_Var12;
    _MMIOINFO local_48;
  if (param_1 == 0) {
    return 10;
  }
  if (100 < *(uint *)((int)this + 0x68)) {
    return 0x10;
  }
  p_Var12 = &local_48;
  for (iVar7 = 0x12; iVar7 != 0; iVar7 = iVar7 + -1) {
    p_Var12->dwFlags = 0;
    p_Var12 = (_MMIOINFO *)&p_Var12->fccIOProc;
  }
  local_48.fccIOProc = 0x204d454d;
  local_48.cchBuffer = 0x36;
  local_48.adwInfo[0] = 0;
  local_48.pchBuffer = (HPSTR)0x0;
  puVar3 = (unsigned int *)(operator_new(4));
  pHVar4 = mmioOpenA((LPSTR)0x0,&local_48,0x11002);
  *puVar3 = (unsigned int)(pHVar4);
  if (pHVar4 == (HMMIO)0x0) {
    return 6;
  }
  DAT_100b4a08 = DAT_100b4a08 + 1;
  mmioGetInfo((HMMIO)*puVar3,&local_48,0);
  psVar8 = (short *)*param_1;
  pfVar5 = (float *)((char *)&DAT_100b515c);
  do {
    *pfVar5 = (float)(int)(*psVar8) * _DAT_1005c5e0;
    pfVar6 = pfVar5 + 1;
    psVar8 = psVar8 + 1;
    pfVar5 = pfVar6;
  } while ((int)((int)pfVar6) < (int)(((char *)&DAT_100b542c)));
  FUN_1003b150((int)(((char *)&DAT_100b515c)),(int)(((char *)&DAT_100b5084)),*(float **)((int)this + 0x90));
  bVar10 = 0x80;
  bVar2 = 0;
  piVar11 = (int *)(((char *)&DAT_100b5084));
  pbVar9 = (byte *)local_48.pchBuffer;
  do {
    if (*piVar11 != 0) {
      bVar2 = bVar2 | bVar10;
    }
    bVar10 = bVar10 >> 1;
    if ((bVar10 == 0) || (piVar11 == (int *)((char *)&DAT_100b5158))) {
      *pbVar9 = bVar2;
      pbVar9 = pbVar9 + 1;
      bVar2 = 0;
      bVar10 = 0x80;
    }
    piVar11 = piVar11 + 1;
  } while ((int)((int)piVar11) < (int)(((char *)&DAT_100b515c)));
  *(int *)((int)this + 0x80) = *(int *)((int)this + 0x80) + 1;
  local_48.pchEndRead = local_48.pchBuffer + 7;
  mmioSetInfo((HMMIO)*puVar3,&local_48,0);
  ((T_thunk_FUN_10031b60 *)((void *)((int)this + 0x5c)))->thunk_FUN_10031b60((unsigned int)(puVar3));
  return 0;
}
}
