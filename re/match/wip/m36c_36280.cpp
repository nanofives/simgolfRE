// m36c WIP (round 36): best 99.6% for sound.dll 0x10036280, from scratch r2m.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
#include <string.h>
typedef unsigned int uint;

namespace f_10036280 {
struct C;
struct Buf { char pad0[0x14]; uint size; char pad18[4]; uint pos; };
struct MixerData { char pad0[0x1a4]; uint flags; };
struct Poster { void post(UINT msg, WPARAM w, LPARAM l); };
extern MixerData *g_100b4a20;
extern Poster g_100b5038;
uint mixA(double *buf, uint *pos, uint n, C *c, int one);
uint mixB(double *buf, uint *pos, uint n, C *c, int one);
uint mixC(double *buf, uint *pos, uint n, C *c, int one);
uint mixD(double *buf, uint *pos, uint n, C *c, int one);
uint mixE(double *buf, uint *pos, uint n, C *c, int one);
uint mixF(double *buf, uint *pos, uint n, C *c, int one);
struct C {
  char pad0[8]; Buf *sb; char pad0c[0x58 - 0xc]; uint flags; uint flags5c; char pad60[0x17c - 0x60]; HWND hwnd;
  short pad180; short channels; char pad184[0x18e - 0x184]; short bits; char pad190[0x1c8 - 0x190];
  double dstep; double dtarget; char pad1d8[0x1e0 - 0x1d8]; double dvol; char pad1e8[0x210 - 0x1e8]; double d210;
  char pad218[0x224 - 0x218]; int pan; uint pos; uint stop; uint inc; char pad234[0x1370 - 0x234];
  int notifies; uint mask; uint base;
  void setPanGains(int p);
  void fade() {
  uint f = flags;
  if ((f & 8) || (flags5c & 0x10)) {
    uint g = flags5c;
    if (g & 2) {
      flags = f | 0x10;
    } else {
      dvol -= dstep;
      if (g & 8) {
        if (dvol >= d210) {
          flags = f & ~8;
          flags5c = g & ~8;
        }
      } else if (g & 0x10) {
        if (dstep > 0.0) {
          if (dvol <= dtarget) flags5c = g & ~0x10;
        } else {
          if (dvol >= dtarget) flags5c = g & ~0x10;
        }
      } else {
        if (dvol == 0.0) flags = f | 0x10;
        if (dvol <= dstep) {
          dstep = dvol;
          flags5c = g | 2;
        }
      }
      setPanGains(pan);
    }
  }
  }
  int renderSingle(double *buf, uint frames, uint fmt, uint *out);
};
// MATCH: sound.dll 0x10036280 ?renderSingle@C@f_10036280@@QAEHPANIIPAI@Z
int C::renderSingle(double *buf, uint frames, uint fmt, uint *out)
{
  if (frames > 2000) return 0x22;
  int st = 0;
  if (fmt >= 12) st = 1;
  pos += inc;
  if (stop != 0 && stop > pos) {
    *out = frames;
    if (st) memset(buf, 0, frames << 4);
    return 0;
  }
  uint cur = sb->pos;
  if (st) {
    if (channels == 2) *out = mixA(buf, &cur, frames, this, 1);
    else if (bits == 8) *out = mixB(buf, &cur, frames, this, 1);
    else *out = mixC(buf, &cur, frames, this, 1);
  } else {
    if (channels == 2) *out = mixD(buf, &cur, frames, this, 1);
    else if (bits == 8) *out = mixE(buf, &cur, frames, this, 1);
    else *out = mixF(buf, &cur, frames, this, 1);
  }
  fade();
  if (*out > frames) return 10;
  sb->pos = cur;
  if (*out != frames) {
    if (st) memset(buf + *out * 2, 0, (frames - *out) << 4);
    flags |= 0x10;
    return 0;
  }
  *out = frames;
  uint f2 = flags;
  if ((f2 & 2) && !(f2 & 0x21) && !(f2 & 0x400) && ((sb->pos - base) & mask) > (sb->size >> 2)) {
    flags = f2 | 0x400;
    notifies++;
    if (g_100b4a20 && (bool)((g_100b4a20->flags >> 5) & 1)) {
      PostMessageA(hwnd, 0x7eb, (WPARAM)this, 0);
      return 0;
    }
    g_100b5038.post(0x7eb, (WPARAM)this, 0);
  }
  return 0;
}
}
