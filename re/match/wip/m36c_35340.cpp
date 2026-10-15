// m36c WIP (round 36): best 21.9% for sound.dll 0x10035340, from scratch r2c.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10035340 {
struct C {
  char pad0[0x1d8]; int vol; int pad1dc; double dvol; int left; int pad1ec; double dleft; int right; int pad1fc;
  double dright; char pad208[0x224 - 0x208]; int pan;
  void setPanGains(int p);
};
// MATCH: sound.dll 0x10035340 ?setPanGains@C@f_10035340@@QAEXH@Z
void C::setPanGains(int p)
{
  if (p < -64) p = -64;
  else if (p > 63) p = 63;
  int v = vol;
  pan = p;
  if (p == 0) {
    right = v;
    left = v;
    dright = dvol;
    dleft = dvol;
  } else {
    int div = (p < 0) ? 64 : 63;
    int d = v * p / div;
    p = d + v;
    v = v - d;
    if (p > 0x10000) p = 0x10000;
    if (v > 0x10000) v = 0x10000;
    left = v;
    right = p;
    dright = p * (1.0 / 65536.0);
    dleft = v * (1.0 / 65536.0);
  }
}
}

namespace f_10034c80 {
extern uint *g_100b49ec;
struct C {
  char pad0[0x5c]; uint flags5c; char pad60[0x1c4 - 0x60]; int step; double dstep; double dfrom; int vol; int pad1dc;
  double dto;
  void setRateGlide(int from, int to, uint ms);
};
// MATCH: sound.dll 0x10034c80 ?setRateGlide@C@f_10034c80@@QAEXHHI@Z
void C::setRateGlide(int from, int to, uint ms)
{
  if (to == -1) to = vol;
  if (from == -1) from = vol;
  dfrom = from * (1.0 / 65536.0);
  dto = to != 0 ? to * (1.0 / 65536.0) : 0.0;
#define a dfrom
#define b dto
  if (g_100b49ec != 0) {
    if (ms > *g_100b49ec) {
      ms /= *g_100b49ec;
#define n ms
      step = (uint)(to - from) / n;
      dstep = (b - a) / n;
      flags5c |= 0x10;
    } else {
      step = to - from;
      flags5c |= 0x10;
      dstep = to - from;
    }
  }
}
}
