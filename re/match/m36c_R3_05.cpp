// FLAGS sound.dll: /O2
// m36c R3 (round 36): sound.dll release functions 0x10037780..0x1003a900 (MultiSound methods), hand-matched from
// Ghidra's decompilation (raw form: Ghidra's offsets and FUN_ names). Evidence of byte identity, not readable source.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);
#define V(n) virtual int v##n();
#define V10(p) V(p##0) V(p##1) V(p##2) V(p##3) V(p##4) V(p##5) V(p##6) V(p##7) V(p##8) V(p##9)
// Synthetic vtable of the sound object: slot 4 (+0x10) and slot 18 (+0x48) take one argument.
#define SOUND_VT(NAME) struct NAME { virtual ~NAME(); V(1) V(2) V(3) virtual int v4(int); V(5) V(6) V(7) V(8) V(9) \
  V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) virtual int v18(int); V(19) V10(2) V10(3) V10(4) V(50) V(51) };
// Engine vtable (DAT_100b4a20): slot 27 (+0x6c).
#define ENGINE_VT(NAME) struct NAME { V(0) V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V10(1) V(20) V(21) V(22) V(23) \
  V(24) V(25) V(26) V(27) };
#define FLAG(o) (*(uint *)((int)this + (o)))
// MultiSound with its channel at +0x70 (channel flags at +0x58 = MultiSound +0xc8, channel +0x5c = +0xcc). The field
// +0x50 is read through an inline accessor: a direct member read compiles with other registers (measured on 0x10039e70).
#define MS_CLASS(NAME, METHOD) \
  struct Ch { \
    char pad[0x58]; uint flags; uint f5c; \
    void FUN_10033ba0(void); void FUN_100348f0(int); int FUN_10034770(int *); void FUN_10034430(void); \
    void FUN_10035150(int); \
    int hasFlag(uint f) { return flags & f; } void setFlag(uint f) { flags |= f; } \
  }; \
  struct NAME { \
    virtual ~NAME(); V(1) V(2) V(3) virtual int v4(int); V(5) V(6) V(7) V(8) V(9) \
    V(10) V(11) V(12) V(13) V(14) V(15) V(16) V(17) virtual int v18(int); V(19) V10(2) V10(3) V10(4) V(50) V(51) \
    int x4; char pad8[0x50 - 8]; int x50v; int x54; uint x58; char pad5c[0x70 - 0x5c]; Ch ch; \
    char padd0[0x1428 - 0xd0]; uint x1428; \
    int x50() { return x50v; } \
    METHOD; \
  };

namespace f_10039b30 {
SOUND_VT(VT)
struct T_FUN_100340b0 { int FUN_100340b0(void); };
struct C_FUN_10039b30 { int FUN_10039b30(void); };
// MATCH: sound.dll 0x10039b30 ?FUN_10039b30@C_FUN_10039b30@f_10039b30@@QAEHXZ
int C_FUN_10039b30::FUN_10039b30(void)
{
  ((VT *)this)->v23();
  if (*(int *)((int)this + 0x1438) > 1) {
    int i = 1;
    VT **child = (VT **)((int)this + 0x1440);
    for (; i < *(int *)((int)this + 0x1438); i++, child++) {
      if (*child != 0) {
        ((C_FUN_10039b30 *)*child)->FUN_10039b30();
        delete *child;
        *child = 0;
      }
    }
    *(int *)((int)this + 0x1438) = 1;
  }
  if ((FLAG(0xc8) & 2) != 0) {
    int r = ((T_FUN_100340b0 *)((int)this + 0x70))->FUN_100340b0();
    FLAG(0x44) = FLAG(0x44) & 0xfffffffe;
    return r;
  }
  return 0x14;
}
}

namespace f_1003a370 {
SOUND_VT(VT)
struct T_FUN_1002c040 { void FUN_1002c040(uint); };
struct T_FUN_10037920 { void FUN_10037920(void); };
struct BF { uint b0 : 5; uint b5 : 1; uint b6 : 1; uint b7 : 1; uint b8 : 1; uint b9 : 1; };
struct C_FUN_1003a370 { int FUN_1003a370(uint param_2); };
// MATCH: sound.dll 0x1003a370 ?FUN_1003a370@C_FUN_1003a370@f_1003a370@@QAEHI@Z
int C_FUN_1003a370::FUN_1003a370(uint param_2)
{
  ((T_FUN_1002c040 *)this)->FUN_1002c040(param_2);
  if ((param_2 & 1) != 0) {
    ((T_FUN_10037920 *)((int)this + 0x70))->FUN_10037920();
  }
  if ((param_2 & 0x10) != 0) {
    FLAG(0xc8) |= 0x20000000;
  }
  if ((param_2 & 0x20) != 0) {
    FLAG(0xc8) |= 0x80000000;
  }
  if ((param_2 & 0x40) != 0) {
    ((BF *)((int)this + 0xcc))->b5 = 1;
  }
  if ((param_2 & 0x100) != 0) {
    ((BF *)((int)this + 0xcc))->b6 = 1;
  }
  if ((param_2 & 0x200) != 0) {
    ((BF *)((int)this + 0xcc))->b7 = 1;
  }
  if ((param_2 & 2) != 0) {
    ((VT *)this)->v18(1);
  }
  if ((param_2 & 0x400) != 0) {
    ((BF *)((int)this + 0xcc))->b9 = 1;
  }
  return 0;
}
}

namespace f_10039e70 {
extern UINT *DAT_100b49ec;
struct T_FUN_10008ce0 { void FUN_10008ce0(void); };
MS_CLASS(C_FUN_10039e70, int FUN_10039e70(int param_1))
// MATCH: sound.dll 0x10039e70 ?FUN_10039e70@C_FUN_10039e70@f_10039e70@@QAEHH@Z
int C_FUN_10039e70::FUN_10039e70(int param_1)
{
  if ((ch.flags & 1) == 0) {
    return 0xb;
  }
  if ((ch.flags & 2) == 0) {
    ch.FUN_10033ba0();
    if ((x58 & 2) != 0) {
      if (v4(x50()) != 0) {
        return 8;
      }
    }
    else {
      return 0x14;
    }
  }
  if (DAT_100b49ec != 0) {
    ((T_FUN_10008ce0 *)DAT_100b49ec)->FUN_10008ce0();
  }
  ch.FUN_100348f0(param_1);
  if (!ch.hasFlag(0x40)) {
    ch.setFlag(0x40);
    v31();
  }
  return 0;
}
}

namespace f_10039f30 {
extern int DAT_100b4a20;
struct T_FUN_1000c2f0 { void FUN_1000c2f0(int, int); };
MS_CLASS(C_FUN_10039f30, int FUN_10039f30(int *param_2))
// MATCH: sound.dll 0x10039f30 ?FUN_10039f30@C_FUN_10039f30@f_10039f30@@QAEHPAH@Z
int C_FUN_10039f30::FUN_10039f30(int *param_2)
{
  *param_2 = -1;
  if ((ch.flags & 1) == 0) {
    return 0x21;
  }
  if ((ch.flags & 2) == 0) {
    ch.FUN_10033ba0();
    if ((x58 & 2) != 0) {
      if (v4(x50()) != 0) {
        return 8;
      }
    }
    else {
      return 0x14;
    }
  }
  int iVar1 = ch.FUN_10034770(param_2);
  if (iVar1 == 0) {
    if ((ch.flags & 0x40) != 0) {
      return 0;
    }
    ((T_FUN_1000c2f0 *)DAT_100b4a20)->FUN_1000c2f0((int)this, 1);
  }
  if ((ch.flags & 0x40) != 0) {
    return 0;
  }
  if ((v51() == 0) || ((bool)((x58 >> 4) & 1)) || ((bool)((ch.f5c >> 6) & 1))) {
    ((T_FUN_1000c2f0 *)DAT_100b4a20)->FUN_1000c2f0((int)this, 1);
  }
  return iVar1;
}
}

namespace f_10039ce0 {
ENGINE_VT(EVT)
extern EVT *DAT_100b4a20;
struct T_FUN_1000c2f0 { void FUN_1000c2f0(int, int); };
MS_CLASS(C_FUN_10039ce0, int FUN_10039ce0(void))
// MATCH: sound.dll 0x10039ce0 ?FUN_10039ce0@C_FUN_10039ce0@f_10039ce0@@QAEHXZ
int C_FUN_10039ce0::FUN_10039ce0(void)
{
  if (((ch.flags & 0x40) != 0) && ((ch.flags & 1) == 0)) {
    return 0xe;
  }
  if ((DAT_100b4a20 == 0) || (DAT_100b4a20->v27() != 0)) {
    return 1;
  }
  if ((x1428 & 1) != 0) {
    return 0x28;
  }
  if ((ch.flags & 2) == 0) {
    ch.FUN_10033ba0();
    if ((x58 & 2) != 0) {
      if (v4(x50()) != 0) {
        return 8;
      }
    }
    else {
      return 0x14;
    }
  }
  if (((ch.flags & 1) != 0) && (v51() == 0)) {
    ch.FUN_10034430();
  }
  if ((ch.flags & 0x40) != 0) {
    return 0;
  }
  if (((ch.f5c & 4) != 0) || ((bool)((ch.f5c >> 6) & 1))) {
    ch.FUN_10035150(x4);
    ch.FUN_10034430();
  }
  if (((ch.flags & 1) != 0) && (v51() != 0) && ((x58 & 2) != 0)) {
    ((T_FUN_1000c2f0 *)DAT_100b4a20)->FUN_1000c2f0((int)this, 1);
  }
  if ((v51() == 0) || ((bool)((x58 >> 4) & 1)) || ((bool)((ch.f5c >> 6) & 1))) {
    ((T_FUN_1000c2f0 *)DAT_100b4a20)->FUN_1000c2f0((int)this, 1);
  }
  return 0;
}
}

