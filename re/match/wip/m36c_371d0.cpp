// m36c WIP (round 36): best 85.2% for sound.dll 0x100371d0, from scratch r2n.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
#include <string.h>
typedef unsigned int uint;
typedef unsigned char uchar;

namespace f_100371d0 {
struct Mixer {
  virtual void f00(); virtual void f01(); virtual void f02(); virtual void f03();
  virtual void f04(); virtual void f05(); virtual void f06(); virtual void f07();
  virtual void f08(); virtual void f09(); virtual void f0a(); virtual void f0b();
  virtual void f0c(); virtual void f0d(); virtual void f0e(); virtual void f0f();
  virtual uint getRate();
};
extern Mixer *g_100b4a20;
struct C;
struct V {
  char pad0[0x18]; int i18; int i1c; char pad20[0x50 - 0x20]; int base; int pad54; double dbase; double d60;
  char pad68[0x88 - 0x68]; double d88; char pad90[0x94 - 0x90];
  uchar a0:1; uchar a1:7; char pad95[0x9c - 0x95];
  union { uchar flags; struct { uchar b0:1; uchar b1:1; uchar b2:1; uchar b3:1; uchar b4:1; uchar b5:1; uchar b6:2; }; };
  char pad9d[0xc0 - 0x9d];
  uint ic0; uint ic4; char padc8[0xe4 - 0xc8]; int ie4; char pade8[0xec - 0xe8]; int step; double dstep; double dstep2;
  char pad100[0x110 - 0x100];
  void setPan(int p);
  void setA(double a) { dbase = a; }
  void setLoop() { b2 = 1; }
  V *self() { return this; }
  void reset() {
    flags &= 0xf1;
    i1c = i18;
    d88 = 0.0;
    ic0 = 0;
    dstep = dstep2;
  }
  void rewind() {
    flags &= 0xe1;
    i1c = i18;
    d88 = 0.0;
    ic0 = 0;
    dstep = dstep2;
  }
  void calc(uint r) {
      if (g_100b4a20 != 0) {
        double dr = g_100b4a20->getRate();
        double x;
        if ((double)r > dr) {
          double ratio = r / dr;
          step = (int)(base / ratio);
          x = dbase / ratio;
        } else {
          step = base;
          x = dbase;
        }
        dstep = x;
        dstep2 = x;
      }
  }
};
uint mixA(double *buf, uint *pos, uint n, C *c, int one);
uint mixB(double *buf, uint *pos, uint n, C *c, int one);
uint mixC(double *buf, uint *pos, uint n, V *v, int zero);
uint mixB2(double *buf, uint *pos, uint n, C *c, int one);
uint mixD(double *buf, uint *pos, uint n, C *c, int one);
uint mixF(double *buf, uint *pos, uint n, V *v, int zero);
struct C {
  char pad0[0x5c]; uint flags5c; char pad60[0x182 - 0x60]; short channels; char pad184[0x18e - 0x184]; short bits;
  char pad190[0x1c0 - 0x190]; uint rate; char pad1c4[0x1e0 - 0x1c4]; double dvol; char pad1e8[0x224 - 0x1e8];
  int pan; char pad228[0x230 - 0x228]; uint inc;
  int renderVoice(V *v, double *buf, uint frames, uint fmt, uint *out);
};
// MATCH: sound.dll 0x100371d0 ?renderVoice@C@f_100371d0@@QAEHPAUV@2@PANIIPAI@Z
int C::renderVoice(V *v, double *buf, uint frames, uint fmt, uint *out)
{
  uint cur = v->i1c;
  uint res = 0;
  int st = 0;
  if (fmt >= 12) st = 1;
  if (!v->b2) {
    v->ic0 += inc;
    if (v->ic4 > v->ic0) {
      *out = frames;
      if (st) memset(buf, 0, frames << 4);
      return 0;
    }
    if (st) {
      if (channels == 2) *out = mixA(buf, &cur, frames, this, 1);
      else if (bits == 8) *out = mixB(buf, &cur, frames, this, 1);
      else res = mixC(buf, &cur, frames, v, 0);
    } else {
      if (channels == 2) *out = mixD(buf, &cur, frames, this, 1);
      else if (bits == 8) *out = mixB2(buf, &cur, frames, this, 1);
      else res = mixF(buf, &cur, frames, v, 0);
    }
    *out = res;
    v->i1c = cur;
  }
  if (v->b3) {
    if (v->b4) {
      v->rewind();
      v->setLoop();
      v->setA(dvol);
      v->setPan(pan);
      v->calc(rate);
      v->ie4 = 0;
      v->a0 = 0;
    } else {
      double d = v->dbase -= v->dstep;
      if (v->b5) {
        if (d >= v->d60) {
          v->b3 = 0;
          v->b5 = 0;
        }
      } else if (d <= 0.0) {
        v->rewind();
        v->setLoop();
        v->setA(dvol);
        v->calc(rate);
        v->ie4 = 0;
        v->b3 = 0;
      } else if (d < v->dstep) {
        v->dstep = d;
        v->b4 = 1;
      }
      v->setPan(pan);
    }
  }
  if (res != frames) {
    if (st && !(flags5c & 4)) memset(buf + res * 2, 0, (frames - res) << 4);
    v->reset();
    v->setLoop();
  }
  return 0;
}
}
