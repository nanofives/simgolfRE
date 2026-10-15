// FLAGS sound.dll: /O2
// m36c R2: sound.dll Channel / Voice functions 0x10033c00..0x10037780, hand-matched from Ghidra's decompilation.
// C = the channel object (flags at +0x58, 16 voice records of 0x110 bytes from +0x238), V = one voice record.
// Field names are offsets; only the layout and the operations are evidence.
#include <windows.h>
#include <math.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10035f80 {
struct Dev { int f_10010e30(); };
struct C {
  int f_10035fc0();
  int serviceBuffer();
};
// MATCH: sound.dll 0x10035f80 ?serviceBuffer@C@f_10035f80@@QAEHXZ
int C::serviceBuffer()
{
  if (*(Dev **)((int)this + 0x54) != 0 && (*(Dev **)((int)this + 0x54))->f_10010e30() == 0) {
    *(uint *)((int)this + 0x58) |= 0x10;
    return 0;
  }
  if ((*(byte *)((int)this + 0x58) & 8) != 0) {
    return f_10035fc0();
  }
  return 0;
}
}

namespace f_10037760 {
// Two inline setters: the same bit operations written in one statement or one inline merge into a single store.
struct V {
  char pad[0x9c]; uchar b0:1; uchar b1:1; uchar b2:1; uchar b3:5; char pad9d[0x110 - 0x9d];
  void setA() { b2 = 1; }
  void clrB() { b1 = 0; }
};
struct C { char pad[0x238]; V v[16]; void setLoopFlag(int i); };
// MATCH: sound.dll 0x10037760 ?setLoopFlag@C@f_10037760@@QAEXH@Z
void C::setLoopFlag(int i)
{
  v[i].setA();
  v[i].clrB();
}
}

namespace f_10034ac0 {
extern double g_zero;
struct V {
  char pad0[0x58]; double d58; char pad60[0x94 - 0x60];
  uchar a0:1; uchar a1:7; char pad95[0x9c - 0x95];
  uchar b0:1; uchar b1:1; uchar b2:1; uchar b3:1; uchar b4:4;
  char pad9d[0xe4 - 0x9d]; int ie4; char pade8[0xf0 - 0xe8]; double df0; char padf8[0x110 - 0xf8];
};
struct C {
  char pad0[0x58]; uint flags; char pad5c[0x238 - 0x5c]; V v[16];
  int releaseVoices();
};
// MATCH: sound.dll 0x10034ac0 ?releaseVoices@C@f_10034ac0@@QAEHXZ
int C::releaseVoices()
{
  uint f = flags;
  if ((f & 0x40) == 0 || (f & 0x10) != 0) return 0x15;
  if ((f & 1) == 0 || (f & 0x10000000) != 0) {
    flags = f | 8;
    return 0;
  }
  uint mask = 1;
  for (int i = 0; i < 16; i++) {
    if (mask && (flags & 0xffff000)) {
      V *p = &v[i];
      if (p->d58 != g_zero && p->df0 != g_zero) {
        *(uchar *)&((char *)p)[0x9c] |= 8;
      } else {
        p->ie4 = 0;
        p->b2 = 1;
        p->a0 = 0;
      }
    }
    mask <<= 1;
  }
  return 0;
}
}

namespace f_10034b90 {
struct V {
  char pad0[0x58]; double d58; double d60; char pad68[0x94 - 0x68];
  uchar a0:1; uchar a1:7; char pad95[0x9c - 0x95];
  uchar flags;
  char pad9d[0xe4 - 0x9d]; int ie4; char pade8[0xf0 - 0xe8]; double df0; char padf8[0x110 - 0xf8];
};
struct C {
  char pad0[0x58]; uint flags; uint flags5c; char pad60[0x1e0 - 0x60]; double d1e0; char pad1e8[0x210 - 0x1e8];
  double d210; char pad218[0x238 - 0x218]; V v[16];
  int stopVoices();
};
// MATCH: sound.dll 0x10034b90 ?stopVoices@C@f_10034b90@@QAEHXZ
int C::stopVoices()
{
  uint f = flags;
  if ((f & 0x40) == 0 || (f & 0x10) != 0) return 0x15;
  if ((f & 1) != 0) {
    uint mask = 1;
    for (int i = 0; i < 16; i++) {
      if (mask && (flags & 0xffff000)) {
        V *p = &v[i];
        if (p->df0 < 0.0 && (p->flags & 0x28) == 0) {
          p->flags |= 0x28;
          p->d60 = p->d58;
          p->d58 = 0.0;
        }
      }
      mask <<= 1;
    }
    return 0;
  }
  d210 = d1e0;
  flags = f | 8;
  d1e0 = 0.0;
  flags5c |= 8;
  return 0;
}
}

namespace f_100351c0 {
// The mixer object at 0x100b4a20: slot 16 (+0x40) returns its output rate.
struct Mixer {
  virtual void f00(); virtual void f01(); virtual void f02(); virtual void f03();
  virtual void f04(); virtual void f05(); virtual void f06(); virtual void f07();
  virtual void f08(); virtual void f09(); virtual void f0a(); virtual void f0b();
  virtual void f0c(); virtual void f0d(); virtual void f0e(); virtual void f0f();
  virtual uint getRate();
};
extern Mixer *g_100b4a20;
// The voice copies go through inline setters taking a double (fld/fstp); plain double assignments compile to
// integer moves and push an aligned ebp frame.
struct V {
  char pad0[0x50]; int base; int pad54; double dbase; char pad60[0x70 - 0x60]; double d70; double d78; double d80;
  char pad88[0x9c - 0x88]; uchar flags; char pad9d[0xec - 0x9d]; int step; double dstep; double dstep2;
  char pad100[0x110 - 0x100];
  void setA(double a) { dbase = a; }
  void setB(double a) { d80 = a; }
  void setC(double a) { d70 = a; }
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
struct C {
  char pad0[0x58]; uint flags; char pad5c[0x1c0 - 0x5c]; uint rate; char pad1c4[0x1d8 - 0x1c4]; int vol; int pad1dc;
  double dvol; char pad1e8[0x1f0 - 0x1e8]; double dleft; char pad1f8[0x200 - 0x1f8]; double dright;
  int target; int pad20c; double dtarget; char pad218[0x224 - 0x218]; int pan; char pad228[0x238 - 0x228]; V v[16];
  void setPanGains(int p);
  void setRate(int r);
};
// MATCH: sound.dll 0x100351c0 ?setRate@C@f_100351c0@@QAEXH@Z
void C::setRate(int r)
{
  if (r < 0) r = 0;
  else if (r > 0x10000) r = 0x10000;
  target = r;
  dtarget = r * (1.0 / 65536.0);
  if ((flags & 8) == 0) {
    dvol = dtarget;
    vol = r;
  }
  setPanGains(pan);
  if ((flags & 0x10000000) == 0 && (flags & 1) != 0) {
    for (int i = 0; i < 16; i++) {
      if ((v[i].flags & 8) == 0) {
        v[i].setA(dvol);
        v[i].setB(dleft);
        v[i].setC(dright);
        v[i].calc(rate);
      }
    }
  }
}
}

namespace f_100348f0 {
struct Mixer {
  virtual void f00(); virtual void f01(); virtual void f02(); virtual void f03();
  virtual void f04(); virtual void f05(); virtual void f06(); virtual void f07();
  virtual void f08(); virtual void f09(); virtual void f0a(); virtual void f0b();
  virtual void f0c(); virtual void f0d(); virtual void f0e(); virtual void f0f();
  virtual uint getRate();
};
extern Mixer *g_100b4a20;
struct V {
  char pad0[0x18]; int i18; int i1c; char pad20[0x50 - 0x20]; int base; int pad54; double dbase; char pad60[0x70 - 0x60];
  double d70; double d78; double d80; double d88; char pad90[0x94 - 0x90];
  uchar a0:1; uchar a1:7; char pad95[0x9c - 0x95];
  uchar b0:1; uchar b1:1; uchar b2:1; uchar b3:1; uchar b4:4; char pad9d[0xc0 - 0x9d];
  int ic0; int ic4; void *pc8; char padcc[0xec - 0xcc]; int step; double dstep; double dstep2; double d100; double d108;
  void setD108(double a) { d108 = a; }
  void reset() {
    b1 = 0; b2 = 0; b3 = 0;
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
struct C {
  char pad0[0x58];
  uint f0:1; uint f1:1; uint f2:1; uint f3:1; uint f4:1; uint f5:1; uint f6:1; uint f7:5; uint voices:16; uint f28:1;
  uint f29:3;
  char pad5c[0x1c0 - 0x5c]; uint rate; char pad1c4[0x22c - 0x1c4]; int i22c; int i230; char pad234[0x238 - 0x234]; V v[16];
  char pad1338[0x1388 - 0x1338]; double d1388;
  int startVoice(int i);
};
// MATCH: sound.dll 0x100348f0 ?startVoice@C@f_100348f0@@QAEHH@Z
int C::startVoice(int i)
{
  if (!f0) return 0xb;
  if (i > 16) return 10;
  int mask = 1 << i;
  v[i].reset();
  v[i].setD108(d1388);
  v[i].ic4 = i22c;
  v[i].ic0 = 0;
  v[i].pc8 = &i230;
  v[i].b2 = 0;
  v[i].calc(rate);
  voices |= mask;
  f4 = 0;
  v[i].a0 = 1;
  return 0;
}
}

namespace f_10034770 {
struct V {
  char pad0[0x18]; int i18; int i1c; char pad20[0x50 - 0x20]; int base; int pad54; double dbase; char pad60[0x70 - 0x60];
  double d70; double d78; double d80; double d88; char pad90[0x94 - 0x90];
  uchar a0:1; uchar a1:7; char pad95[0x9c - 0x95];
  uchar b0:1; uchar b1:1; uchar b2:1; uchar b3:1; uchar b4:4; char pad9d[0xc0 - 0x9d];
  int ic0; int ic4; void *pc8; char padcc[0xec - 0xcc]; int step; double dstep; double dstep2; double d100; double d108;
  void setA(double a) { dbase = a; }
  void setB(double a) { d80 = a; }
  void setC(double a) { d70 = a; }
  void setD108(double a) { d108 = a; }
  void reset() {
    b1 = 0; b2 = 0; b3 = 0;
    i1c = i18;
    d88 = 0.0;
    ic0 = 0;
    dstep = dstep2;
  }
};
struct C {
  char pad0[0x58];
  uint f0:1; uint f1:1; uint f2:1; uint f3:1; uint f4:1; uint f5:1; uint f6:1; uint f7:5; uint voices:16; uint f28:1;
  uint f29:3;
  char pad5c[0x1e0 - 0x5c]; double dvol; char pad1e8[0x1f0 - 0x1e8]; double dleft; char pad1f8[0x200 - 0x1f8];
  double dright; char pad208[0x22c - 0x208]; int i22c; int i230; char pad234[0x238 - 0x234]; V v[16];
  char pad1338[0x1388 - 0x1338]; double d1388;
  int allocVoice(int *out);
};
// MATCH: sound.dll 0x10034770 ?allocVoice@C@f_10034770@@QAEHPAH@Z
int C::allocVoice(int *out)
{
  *out = -1;
  if (!f0 || f28) return 0x21;
  uint mask = 1;
  int i;
  for (i = 0; i < 16; i++) {
    if ((voices & mask) == 0) break;
    mask <<= 1;
  }
  if (i == 16) return 6;
  v[i].reset();
  v[i].setB(dleft);
  v[i].setC(dright);
  v[i].setD108(d1388);
  v[i].setA(dvol);
  v[i].ic4 = i22c;
  v[i].ic0 = 0;
  v[i].pc8 = &i230;
  v[i].b2 = 0;
  v[i].a0 = 1;
  voices |= mask;
  f4 = 0;
  *out = i;
  return 0;
}
}

namespace f_10034430 {
struct Node { Node *prev; Node *next; void *data; };
struct List { Node *head; Node *tail; Node *cur; int count; };
struct Dev {
  char pad0[0x44]; int i44; char pad48[0xc4 - 0x48]; int ic4; float fc8; float fcc; float fd0;
  void setPlaying(int on);
  int isFree();
  void f0f190(int a);
  void f0f160(int a);
  void f11000(float a, float b, float c);
  void setPos(float a, float b, float c) { f11000(a, b, c); }
};
struct C;
struct Mgr {
  int f12260(C *c, Dev *dev, Dev **out);
  int f12690(C *c, Dev **out);
};
extern Mgr *g_100b49f0;
struct V {
  char pad0[0x18]; int i18; int i1c; char pad20[0x50 - 0x20]; int base; int pad54; double dbase; char pad60[0x70 - 0x60];
  double d70; double d78; double d80; double d88; char pad90[0x94 - 0x90];
  uchar a0:1; uchar a1:7; char pad95[0x98 - 0x95]; uint w98;
  uchar b0:1; uchar b1:1; uchar b2:1; uchar b3:1; uchar b4:4; char pad9d[0xc0 - 0x9d];
  int ic0; int ic4; void *pc8; char padcc[0xec - 0xcc]; int step; double dstep; double dstep2; double d100; double d108;
  void setA(double a) { dbase = a; }
  void setB(double a) { d80 = a; }
  void setC(double a) { d70 = a; }
  void setD108(double a) { d108 = a; }
  void reset2() {
    i1c = i18;
    d88 = 0.0;
    b1 = 0; b2 = 0; b3 = 0;
    ic0 = 0;
    dstep = dstep2;
  }
  void reset() {
    b1 = 0; b2 = 0; b3 = 0;
    i1c = i18;
    d88 = 0.0;
    ic0 = 0;
    dstep = dstep2;
  }
};
struct C {
  char pad0[0x54]; Dev *dev;
  uint f0:1; uint f1:1; uint f2:1; uint f3:1; uint f4:1; uint f5:1; uint f6:1; uint f7:5; uint voices:16; uint f28:1;
  uint f29:3;
  uint g0:1; uint g1:1; uint g2:1; uint g3:1; uint g4:1; uint g5:1; uint g6:1; uint g7:25;
  char pad60[0x1b8 - 0x60]; void *p1b8; char pad1bc[0x1e0 - 0x1bc];
  double dvol; char pad1e8[0x1f0 - 0x1e8]; double dleft; char pad1f8[0x200 - 0x1f8]; double dright;
  char pad208[0x220 - 0x208]; int i220; char pad224[0x22c - 0x224]; int i22c; int i230; char pad234[0x238 - 0x234];
  V v[16];
  List l1338;
  char pad1348[0x1388 - 0x1348]; double d1388;
  int play();
};
// MATCH: sound.dll 0x10034430 ?play@C@f_10034430@@QAEHXZ
int C::play()
{
  Mgr *mgr = g_100b49f0;
  if (mgr == 0) return 3;
  uint g = *(uint *)((char *)this + 0x5c);
  if (g & 4) return 0xb;
  if ((bool)((g >> 6) & 1) && dev) dev->setPlaying(1);
  uint f = *(uint *)((char *)this + 0x58);
  if (f & 1) {
    int i = 0;
    uint mask = 1;
    if (f & 0x10000000) {
      Dev *d = dev;
      if (d->isFree() != 0 || i22c != 0) {
        Node *n;
        for (n = l1338.head; n; n = n->next) {
          d = (Dev *)n->data;
          if (d->isFree() == 0) break;
        }
        if (n == 0) {
          int r = mgr->f12260(this, dev, &d);
          if (r == 0x25 || r == 0x1d) {
            if (mgr->f12690(this, &d) != 0) return 6;
          } else if (r != 0) {
            return 6;
          }
        }
        d->f0f190(dev->ic4);
      }
      d->f0f160(i220);
      if (g5) d->setPos(dev->fc8, dev->fcc, dev->fd0);
      if (i22c == 0) {
        d->setPlaying(0);
        f4 = 0;
        return 0;
      }
      d->i44 = -i22c;
      f4 = 0;
      return 0;
    }
    for (; i < 16; i++) {
      if (((f >> 12) & mask & 0xffff) == 0) break;
      mask <<= 1;
    }
    if (i == 16) return 6;
    v[i].reset2();
    v[i].setB(dleft);
    v[i].setC(dright);
    v[i].setD108(d1388);
    v[i].setA(dvol);
    v[i].ic4 = i22c;
    v[i].ic0 = 0;
    v[i].pc8 = &i230;
    v[i].b2 = 0;
    if (p1b8) v[i].w98 |= 1;
    voices |= mask;
  }
  f4 = 0;
  return 0;
}
}

namespace f_10035fc0 {
struct Dev { void stop(); void setVolume(int v); };
struct C {
  char pad0[0x54]; Dev *dev; uint flags; uint flags5c; char pad60[0x1c8 - 0x60]; double dstep; double dtarget;
  char pad1d8[0x1e0 - 0x1d8]; double dvol; char pad1e8[0x210 - 0x1e8]; double d210; char pad218[0x13a4 - 0x218];
  int i13a4;
  int stepVolumeFade();
};
// MATCH: sound.dll 0x10035fc0 ?stepVolumeFade@C@f_10035fc0@@QAEHXZ
int C::stepVolumeFade()
{
  uint f = flags;
  if ((f & 8) || (flags5c & 0x10)) {
    uint g = flags5c;
    if (g & 2) {
      flags = f | 0x10;
      dev->stop();
      if (i13a4) flags5c |= 0x480;
      return 0;
    }
    if (g & 8) {
      if (dvol >= d210) {
        flags = f & ~8;
        flags5c = g & ~8;
        return 0;
      }
    } else if (g & 0x10) {
      if (dstep > 0.0) {
        if (dvol <= dtarget) goto done;
        if (dvol < dstep) dvol = dtarget;
        else dvol -= dstep;
        dev->setVolume((int)(dvol * 65536.0));
        return 0;
      } else {
        if (dvol < dtarget) {
          if (dtarget - dvol > fabs(dstep)) dvol -= dstep;
          else dvol = dtarget;
        } else {
done:
          dvol = dtarget;
          dev->setVolume((int)(dvol * 65536.0));
          flags5c &= ~0x10;
          return 0;
        }
      }
      dev->setVolume((int)(dvol * 65536.0));
      return 0;
    } else {
      if (dvol == 0.0) {
        flags = f | 0x10;
        dev->stop();
        if (i13a4) flags5c |= 0x480;
        return 0;
      }
      if (dvol <= dstep) {
        flags5c = g | 2;
        dstep = dvol;
      }
      dvol -= dstep;
      dev->setVolume((int)(dvol * 65536.0));
    }
  }
  return 0;
}
}

namespace f_10035770 {
struct Ev { int value; uint time; };
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
  void *first() { cur = head; if (cur == 0) return 0; return cur->data; }
  void *next() { if (cur == 0) return 0; cur = cur->next; if (cur == 0) return 0; return cur->data; }
};
struct C {
  char pad0[0x16c]; uint pos; char pad170[0x1348 - 0x170]; List events;
  int find();
};
// MATCH: sound.dll 0x10035770 ?find@C@f_10035770@@QAEHXZ
int C::find()
{
  int r = -1;
  Ev *e;
  for (e = (Ev *)events.first(); e != 0; e = (Ev *)events.next()) {
    if (pos < e->time) break;
    r = e->value;
  }
  return r;
}
}
