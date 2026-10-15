// m36c WIP (round 36): best 78.0% for sound.dll 0x10035990, from scratch r2f.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10035990 {
struct V {
  char pad0[0x9c]; uchar b0:1; uchar b1:1; uchar b2:1; uchar b3:5; char pad9d[0x110 - 0x9d];
};
struct C {
  char pad0[0x58];
  uint f0:1; uint f1:1; uint f2:1; uint f3:1; uint f4:1; uint f5:1; uint f6:1; uint f7:5; uint voices:16; uint f28:1;
  uint f29:3;
  uint g0:1; uint g1:1; uint g2:1; uint g3:1; uint g4:1; uint g5:1; uint g6:1; uint g7:25; char pad60[0x1c4 - 0x60];
  int step; char pad1c8[0x1d8 - 0x1c8]; int vol; char pad1dc[0x238 - 0x1dc]; V v[16];
  void renderSingle(double *buf, uint frames, uint fmt, uint *out);
  void renderStreamed(uint frames, uint fmt, uint *out);
  void renderVoice(V *p, double *buf, uint frames, uint fmt, uint *out);
  int render(double *buf, uint frames, uint fmt, uint *out);
  uint f5c() { return *(uint *)((char *)this + 0x5c); }
};
// MATCH: sound.dll 0x10035990 ?render@C@f_10035990@@QAEHPANIIPAI@Z
int C::render(double *buf, uint frames, uint fmt, uint *out)
{
  uint f;
  uint mask = 1;
  int first = 0;
  uint maxOut = 0;
  if (fmt > 0x18) return 10;
  f = *(uint *)((char *)this + 0x58);
  if ((f & 8) || g4) {
    int st = step;
    if (st < 0) {
      vol -= st;
      if ((uint)vol > 0x10000) vol = 0x10000;
    } else if (st < vol) {
      vol -= st;
    } else {
      vol = 0;
    }
  }
  if (!(f & 1) && !g2 && !g6) {
    renderSingle(buf, frames, fmt, out);
    return 0;
  }
  uint h = f5c();
  if (h & 0x40) {
    renderStreamed(frames, fmt, out);
    return 0;
  }
  if (f & 0xffff000) {
    if (fmt >= 12) first = 1;
    for (int i = 0; i < 16; i++) {
      if (voices & mask) {
        timeGetTime();
        renderVoice(&v[i], buf, frames, fmt, out);
        if (*out != frames || v[i].b2) voices &= ~mask;
        if (*out > maxOut) maxOut = *out;
        if (first) {
          first = 0;
          fmt -= 12;
        }
      }
      mask <<= 1;
    }
    *out = maxOut;
    if (voices == 0) f4 = 1;
    return 0;
  }
  if (h & 4) {
    bool s = false;
    if (fmt >= 12) s = true;
    if ((bool)((h >> 6) & 1) && (f & 0x10000000) && s) memset(buf, 0, frames << 4);
  }
  return 0;
}
}
