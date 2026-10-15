// m36c WIP (round 36): best 98.3% for sound.dll 0x100339c0, from scratch j4.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_100339c0 {
struct Dec { char pad[0x18]; int f18; int f1c; int f20; };
struct Rec { int f0; int f4; char p1[0x70 - 8]; int f70; int f74; char p2[0x84 - 0x78]; unsigned char b0 : 1; unsigned char b1 : 1; unsigned char b2 : 1; unsigned char b3 : 1; unsigned char b4 : 4;
  char p3[0xa8 - 0x85]; int fa8; char p4[0xd8 - 0xac]; double fd8; double fe0; char p5[0x110 - 0xe8]; };
struct C_FUN_100339c0 {
  HMMIO h; int f4; Dec *dec; MMIOINFO info; int f54; unsigned int f58; unsigned int f5c;
  char pad1[0x164 - 0x60]; int f164; int f168; int f16c; int f170; int f174; int f178; int f17c;
  int f180, f184, f188; unsigned short f18c; char pad2[0x1c0 - 0x18e]; int f1c0;
  char pad3[0x1d8 - 0x1c4]; int f1d8, f1dc; double f1e0; char pad4[0x208 - 0x1e8]; int f208, f20c; double f210;
  char pad5[0x224 - 0x218]; int f224; int f228; char pad6[0x250 - 0x22c]; Rec rec[16];
  char pad7[0x1378 - 0x250 - 16 * 0x110]; int f1378;
  void fill(); void m1(int); void m2(int);
  int FUN_100339c0(); };
// MATCH: sound.dll 0x100339c0 ?FUN_100339c0@C_FUN_100339c0@f_100339c0@@QAEHXZ
int C_FUN_100339c0::FUN_100339c0()
{
  if (h == 0 && (f5c & 4) == 0) {
    return 0x14;
  }
  f58 &= 0xfffffedf;
  f16c = f164;
  if (f58 & 1) {
    info.pchNext = info.pchBuffer;
    info.pchEndRead = info.pchBuffer + f178;
  }
  else if ((f5c & 4) == 0) {
    mmioSeek(h, f18c * f164 + f174, 0);
    mmioGetInfo(h, &info, 0);
    mmioAdvance(h, &info, 0);
    dec->f20 = dec->f18;
    dec->f1c = dec->f20;
    f170 = 0;
    f1378 = dec->f18;
    fill();
    mmioAdvance(h, &info, 0);
  }
  f1d8 = f208;
  f5c &= ~3;
  f228 = 0;
  f58 &= 0xf0000f67;
  f1e0 = f210;
  m1(f1c0);
  m2(f224);
  if (f58 & 1) {
    for (int i = 0; i < 16; i++) {
      rec[i].b1 = 0;
      rec[i].b2 = 0;
      rec[i].b3 = 0;
      rec[i].f4 = rec[i].f0;
      rec[i].fd8 = rec[i].fe0;
      rec[i].f70 = 0;
      rec[i].f74 = 0;
      rec[i].fa8 = 0;
      rec[i].b1 = 0;
    }
  }
  return 0;
}
}
