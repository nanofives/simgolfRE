// m36c WIP (round 36): best 64.6% for sound.dll 0x10035460, from scratch r2d.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10035460 {
struct Mixer {
  virtual void f00(); virtual void f01(); virtual void f02(); virtual void f03();
  virtual void f04(); virtual void f05(); virtual void f06(); virtual void f07();
  virtual void f08(); virtual void f09(); virtual void f0a(); virtual void f0b();
  virtual void f0c(); virtual void f0d(); virtual void f0e(); virtual void f0f();
  virtual uint getRate();
};
extern Mixer *g_100b4a20;
extern uint *g_100b49ec;
struct V {
  char pad0[0x50]; int base; int pad54; double dbase; char pad60[0xec - 0x60]; int step; double dstep; double dstep2;
  char pad100[0x110 - 0x100];
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
  char pad0[0x58]; uint flags; char pad5c[0x1c0 - 0x5c]; uint rate; int istep; double dstep; char pad1d0[0x1e0 - 0x1d0];
  double dvol; char pad1e8[0x208 - 0x1e8]; int ivol; char pad20c[0x238 - 0x20c]; V v[16];
  void recomputeVoiceGains(uint r);
};
// MATCH: sound.dll 0x10035460 ?recomputeVoiceGains@C@f_10035460@@QAEXI@Z
void C::recomputeVoiceGains(uint r)
{
  if (flags & 1) {
    for (int i = 0; i < 16; i++) {
      v[i].calc(r);
    }
  }
  if (g_100b49ec != 0) {
    if (r > *g_100b49ec) {
      uint n = r / *g_100b49ec;
      rate = r;
      dstep = dvol / n;
      istep = ivol / n;
    } else {
      istep = ivol;
      dstep = dvol;
      rate = r;
    }
  }
}
}
namespace f_100355d0 {
inline int neg(int x) { return -x; }
using f_10035460::Mixer;
using f_10035460::g_100b4a20;
using f_10035460::g_100b49ec;
struct V {
  char pad0[0x50]; int base; int pad54; double dbase; double d60; char pad68[0xec - 0x68]; int step; double dstep; double dstep2;
  char pad100[0x110 - 0x100];
  void setD60(double a) { d60 = a; }
  void calc(uint r) {
      if (g_100b4a20 != 0) {
        double dr = g_100b4a20->getRate();
        double x;
        if ((double)r > dr) {
          double ratio = r / dr;
          step = -(int)(base / ratio);
          x = dbase / ratio;
        } else {
          step = -base;
          x = dbase;
        }
        dstep = -x;
        dstep2 = -x;
        d60 = dbase;
      }
  }
};
struct C {
  char pad0[0x58]; uint flags; char pad5c[0x1c0 - 0x5c]; uint rate; int istep; double dstep; char pad1d0[0x1e0 - 0x1d0];
  double dvol; char pad1e8[0x208 - 0x1e8]; int ivol; char pad20c[0x238 - 0x20c]; V v[16];
  void setReverseRate(uint r);
};
// MATCH: sound.dll 0x100355d0 ?setReverseRate@C@f_100355d0@@QAEXI@Z
void C::setReverseRate(uint r)
{
  if (flags & 1) {
    for (int i = 0; i < 16; i++) {
      v[i].calc(r);
    }
  }
  if (g_100b49ec != 0) {
    double x;
    int s;
    if (r > *g_100b49ec) {
      int n = r / *g_100b49ec;
      x = dvol / n;
      istep = s = -(ivol / n);
    } else {
      istep = s = -ivol;
      x = dvol;
    }
    dstep = -x;
    rate = r;
  }
}
}

namespace f_10035460b {
using f_10035460::V;
using f_10035460::g_100b49ec;
struct C {
  char pad0[0x58]; uint f0:1; uint frest:31; char pad5c[0x1c0 - 0x5c]; uint rate; int istep; double dstep; char pad1d0[0x1e0 - 0x1d0];
  double dvol; char pad1e8[0x208 - 0x1e8]; int ivol; char pad20c[0x238 - 0x20c]; V v[16];
  void recomputeVoiceGains(uint r);
  inline void tail(uint r);
};
inline void C::tail(uint r)
{
  if (g_100b49ec != 0) {
    if (r > *g_100b49ec) {
      uint n = r / *g_100b49ec;
      rate = r;
      dstep = dvol / n;
      istep = ivol / n;
    } else {
      istep = ivol;
      dstep = dvol;
      rate = r;
    }
  }
}
// MATCH: sound.dll 0x10035460 ?recomputeVoiceGains@C@f_10035460b@@QAEXI@Z
void C::recomputeVoiceGains(uint r)
{
  if (f0) {
    for (int i = 0; i < 16; i++)
      v[i].calc(r);
  }
  tail(r);
}
}

namespace f_100351c0 {
struct Mixer {
  virtual void f00(); virtual void f01(); virtual void f02(); virtual void f03();
  virtual void f04(); virtual void f05(); virtual void f06(); virtual void f07();
  virtual void f08(); virtual void f09(); virtual void f0a(); virtual void f0b();
  virtual void f0c(); virtual void f0d(); virtual void f0e(); virtual void f0f();
  virtual uint getRate();
};
extern Mixer *g_100b4a20;
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
