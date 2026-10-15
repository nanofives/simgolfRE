// FLAGS sound.dll: /O2
// m36c R3 (round 36): sound.dll release functions 0x10037780..0x1003a900 with x87 code (Voice / MultiSound),
// hand-matched from Ghidra's decompilation (raw form: Ghidra's offsets and FUN_ names). Evidence of byte identity,
// not readable source. Double constants read from the anchored sound.dll: 0x1005c5f8 = 1.0, 0x1005b280 = 1000.0,
// 0x1005c910 = 0.01.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_1003a210 {
#define V(n) virtual int v##n();
#define V10(p) V(p##0) V(p##1) V(p##2) V(p##3) V(p##4) V(p##5) V(p##6) V(p##7) V(p##8) V(p##9)
extern int DAT_1005c608[];
extern int DAT_1005c66c[];
extern double DAT_1005c6d0[];
struct Ch { void FUN_10035750(int); };
struct C_FUN_1003a210 {
  virtual ~C_FUN_1003a210(); V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V10(1) V10(2) V10(3) V10(4) V(50) V(51)
  char pad8[0x68]; Ch ch; char pad71[0x13f8 - 0x71]; double x13f8;
  void FUN_1002bff0(int);
  int FUN_1003a210(int param_1);
};
// MATCH: sound.dll 0x1003a210 ?FUN_1003a210@C_FUN_1003a210@f_1003a210@@QAEHH@Z
int C_FUN_1003a210::FUN_1003a210(int param_1)
{
  if ((param_1 < -0x4b0) || (param_1 > 0x4b0)) {
    param_1 = param_1 % 0x4b0;
  }
  FUN_1002bff0(param_1);
  int semis;
  int frac;
  __asm {
    push eax
    push ecx
    push edx
    mov eax, param_1
    cdq
    mov ecx, 0x64
    idiv ecx
    cmp edx, 0
    jge La256
    neg edx
  La256:
    mov semis, eax
    mov frac, edx
    pop edx
    pop ecx
    pop eax
  }
  semis += 12;
  double base = 0.0;
  int rate;
  if (v51() != 0) {
    rate = DAT_1005c66c[semis];
  }
  else {
    rate = DAT_1005c608[semis];
    base = DAT_1005c6d0[semis];
  }
  int idx2;
  if (param_1 < 0) {
    idx2 = semis - 1;
  }
  else {
    idx2 = semis + 1;
  }
  double d = (DAT_1005c6d0[idx2] - DAT_1005c6d0[semis]) * frac * 0.01 + base;
  int diff = DAT_1005c608[idx2] - DAT_1005c608[semis];
  ch.FUN_10035750(rate - (int)((float)diff * frac * -0.01f));
  x13f8 = d;
  return 0;
}
}

namespace f_10037c40 {
#define V(n) virtual int v##n();
#define V10(p) V(p##0) V(p##1) V(p##2) V(p##3) V(p##4) V(p##5) V(p##6) V(p##7) V(p##8) V(p##9)
struct Engine { V(0) V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V(10) V(11) V(12) V(13) V(14) V(15)
  virtual uint v16(); };
extern Engine *DAT_100b4a20;
struct C_FUN_10037c40 {
  char pad[0x48]; int x48; int x4c; int x50; int x54; double d58; double d60; int x68; int x6c; double d70; int x78;
  int x7c; double d80; int x88; int x8c; int x90; int x94[3]; char pada0[0xb4 - 0xa0]; int xb4; int xb8; int xbc;
  int xc0; int xc4; int xc8; int xcc; int xd0; int xd4; int xd8; int xdc; int xe0; int xe4; int xe8; int xec;
  double df0; double df8; int x100; int x104; double d108;
  C_FUN_10037c40 *FUN_10037c40(void);
};
// MATCH: sound.dll 0x10037c40 ?FUN_10037c40@C_FUN_10037c40@f_10037c40@@QAEPAU12@XZ
C_FUN_10037c40 *C_FUN_10037c40::FUN_10037c40(void)
{
  memset(x94, 0, sizeof(x94));
  xb8 = 0;
  x50 = 0x10000;
  d58 = 1.0;
  d60 = 1.0;
  x78 = 0x10000;
  d80 = 1.0;
  x68 = 0x10000;
  d70 = 1.0;
  x88 = 0;
  x8c = 0;
  x90 = 0;
  xbc = 0;
  x4c = 0x10000;
  xc0 = 0;
  xc4 = 0;
  if (DAT_100b4a20 != 0) {
    double rate = DAT_100b4a20->v16();
    double d;
    if (1000.0 > rate) {
      double ratio = 1000.0 / rate;
      xec = (int)(x50 / ratio);
      d = d58 / ratio;
    }
    else {
      xec = x50;
      d = d58;
    }
    df0 = d;
    df8 = d;
  }
  x100 = 0;
  xc8 = 0;
  xcc = 0;
  xe0 = 0;
  xb4 = 0;
  x104 = 0;
  d108 = 1.0;
  xec = 0;
  xd4 = 0;
  xd8 = 0;
  xdc = 0;
  xe4 = 0;
  x48 = 0;
  xe8 = 0x80;
  return this;
}
}

namespace f_10037f30 {
struct C_FUN_10037f30 {
  char pad[0x48]; int pan; int x4c; int vol; int x54; double base; char pad60[8]; int gainA; int x6c; double dA;
  int gainB; int x7c; double dB;
  void FUN_10037f30(int param_1);
};
// MATCH: sound.dll 0x10037f30 ?FUN_10037f30@C_FUN_10037f30@f_10037f30@@QAEXH@Z
void C_FUN_10037f30::FUN_10037f30(int param_1)
{
  if (param_1 < -0x40) {
    param_1 = -0x40;
  }
  else if (param_1 > 0x3f) {
    param_1 = 0x3f;
  }
  int v = vol;
  pan = param_1;
  if (param_1 == 0) {
    gainA = v;
    gainB = v;
    dA = base;
    dB = base;
    return;
  }
  int div = (param_1 < 0) + 0x3f;
  double d = (param_1 * base) / div;
  int iv = (v * param_1) / div;
  double a = d + base;
  double b = base - d;
  int ga = iv + v;
  int gb = v - iv;
  if (ga > 0x10000) {
    ga = 0x10000;
  }
  if (a > 1.0) {
    a = 1.0;
  }
  if (gb > 0x10000) {
    gb = 0x10000;
  }
  if (b > 1.0) {
    b = 1.0;
  }
  gainA = ga;
  gainB = gb;
  dB = b;
  dA = a;
}
}
