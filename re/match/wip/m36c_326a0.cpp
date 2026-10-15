// m36c WIP (round 36): best 99.4% for sound.dll 0x100326a0, from scratch o.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
#include <mmreg.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_100326a0 {
extern "C" void __cdecl free(void *);
extern "C" void *__cdecl malloc(size_t);
extern "C" char *__cdecl strstr(const char *, const char *);
extern int DAT_100b4a08;
struct Node { Node *prev; Node *next; void *data; };
struct List {
  Node *head; Node *tail; Node *cur; int count;
  void remove(Node *n) {
    if (n == 0) return;
    if (n->prev != 0) n->prev->next = n->next; else head = n->next;
    Node *nx = n->next;
    if (nx != 0) { nx->prev = n->prev; cur = nx; }
    else { tail = n->prev; cur = 0; }
    free(n);
    count--;
  }
  void append(void *d);
};
extern List DAT_100b49c8;
struct Buf { int setRate(unsigned int rate, int bits); };
struct Dev {
  virtual void v00(); virtual void v01(); virtual void v02(); virtual void v03();
  virtual void v04(); virtual void v05(); virtual void v06(); virtual void v07();
  virtual void v08(); virtual void v09(); virtual void v10(); virtual void v11();
  virtual void v12(); virtual void v13(); virtual void v14(); virtual void v15();
  virtual unsigned int rate();
};
extern Dev *DAT_100b4a20;
struct Out { int create(Buf **out, WAVEFORMATEX *fmt, int flags, int size); };
extern Out *DAT_100b49f0;
struct Loop { int id; int type; int start; int end; int frac; int count; };
struct Cue { int d[6]; };
struct Elem { MMIOINFO info; char p0[0x88 - 0x48]; int f88; int f8c; int f90; char p1[0x9c - 0x94];
  unsigned char b0 : 1; unsigned char b1 : 1; unsigned char b2 : 1; unsigned char b3 : 5; char p2[0xa0 - 0x9d];
  WAVEFORMATEX wfx; char p3[0xd4 - 0xb2]; HPSTR fd4; HPSTR fd8; int fdc; char p4[0x110 - 0xe0]; };
struct C_FUN_100326a0 {
  HMMIO h; HMMIO h2; MMIOINFO *dec; MMIOINFO info; Buf *buf; unsigned int f58; unsigned int f5c; int f60;
  char name[0x100]; int f164, f168, f16c, f170, f174; unsigned int f178; int f17c; WAVEFORMATEX wfx; short f192;
  char smpl[0x24]; void *f1b8; int f1bc; char pad1[0x228 - 0x1c0]; int f228; char pad2[0x238 - 0x22c];
  Elem elem[16]; List l1338; List cues; unsigned int f1358, f135c, f1360, f1364, f1368, f136c, f1370, mask;
  char *wp; char pad3[0x139c - 0x137c]; unsigned int f139c;
  int cleanup(); void resetBuffers(); int beginA(Out *o); int beginB(Out *o); void fill();
  int FUN_100326a0(const char *fname); };
// MATCH: sound.dll 0x100326a0 ?FUN_100326a0@C_FUN_100326a0@f_100326a0@@QAEHPBD@Z
int C_FUN_100326a0::FUN_100326a0(const char *fname)
{
  if (strstr(fname, ".mid") != 0 || strstr(fname, ".rmi") != 0) {
    return 0xb;
  }

  MMCKINFO riff;
  MMCKINFO data;
  MMCKINFO fmt;
  MMCKINFO cue;
  memset(&riff, 0, sizeof(riff));
  memset(&fmt, 0, sizeof(fmt));
  memset(&data, 0, sizeof(data));
  memset(&cue, 0, sizeof(cue));
  MMCKINFO smp;
  DWORD res;
  int r;
  int r2;
  if (f58 & 2) {
    int rc = cleanup();
    if (rc != 0) {
      return rc;
    }
  }
  h = 0;
  MMIOINFO *pi = &info;
  memset(pi, 0, sizeof(MMIOINFO));
  memset(smpl, 0, 0x2c);
  f164 = 0;
  f168 = 0;
  f16c = 0;
  f170 = 0;
  f174 = 0;
  info.cchBuffer = 0x20000;
  info.fccIOProc = 0x204d454d;
  strcpy(name, fname);
  h = mmioOpenA(name, 0, 0);
  if (h == 0) {
    memset(pi, 0, sizeof(MMIOINFO));
    return 8;
  }
  DAT_100b4a08++;
  f58 |= 2;
  DAT_100b49c8.append(&info);
  riff.ckid = mmioStringToFOURCCA("RIFF", 0);
  riff.fccType = mmioStringToFOURCCA("WAVE", 0);
  if (mmioDescend(h, &riff, 0, MMIO_FINDRIFF) != 0) {
    resetBuffers();
    return 0x12;
  }
  fmt.ckid = mmioStringToFOURCCA("fmt ", 0);
  if (mmioDescend(h, &fmt, &riff, MMIO_FINDCHUNK) != 0) {
    resetBuffers();
    return 0x12;
  }
  mmioRead(h, (HPSTR)&wfx, 0x10);
  wfx.cbSize = 0;
  mmioAscend(h, &fmt, 0);
  smp.ckid = mmioStringToFOURCCA("smpl", 0);
  if (mmioDescend(h, &smp, &riff, MMIO_FINDCHUNK) == 0) {
    mmioRead(h, smpl, 0x24);
    if (*(int *)(smpl + 0x1c) != 0) {
      f1b8 = new Loop[*(int *)(smpl + 0x1c)];
      mmioRead(h, (HPSTR)f1b8, *(int *)(smpl + 0x1c) * sizeof(Loop));
      Loop *lp = (Loop *)f1b8;
      f1368 = (lp->end - lp->start + 1) * wfx.nBlockAlign;
      f136c = wfx.nBlockAlign * lp->start;
    }
    mmioAscend(h, &smp, 0);
  }
  else {
    *(int *)(smpl + 0xc) = 0x3c;
  }
  riff.ckid = mmioStringToFOURCCA("RIFF", 0);
  mmioSeek(h, 0, SEEK_SET);
  riff.fccType = mmioStringToFOURCCA("WAVE", 0);
  if (mmioDescend(h, &riff, 0, MMIO_FINDRIFF) != 0) {
    resetBuffers();
    return 0x12;
  }
  cue.ckid = mmioStringToFOURCCA("cue ", 0);
  if (mmioDescend(h, &cue, &riff, MMIO_FINDCHUNK) == 0) {
    res = 0;
    mmioRead(h, (HPSTR)&res, 4);
    for (unsigned int i = 0; i < res; i++) {
      Cue *c = new Cue;
      mmioRead(h, (HPSTR)c, sizeof(Cue));
      cues.append(c);
    }
    mmioAscend(h, &cue, 0);
  }
  mmioAscend(h, &riff, 0);
  riff.ckid = mmioStringToFOURCCA("RIFF", 0);
  mmioSeek(h, 0, SEEK_SET);
  riff.fccType = mmioStringToFOURCCA("WAVE", 0);
  if (mmioDescend(h, &riff, 0, MMIO_FINDRIFF) != 0) {
    resetBuffers();
    return 0x12;
  }
  data.ckid = mmioStringToFOURCCA("data", 0);
  if (mmioDescend(h, &data, &riff, MMIO_FINDCHUNK) != 0) {
    resetBuffers();
    return 0x12;
  }
  f174 = data.dwDataOffset;
  f178 = data.cksize;
  f168 = data.cksize / wfx.nBlockAlign;
  f139c = f168 * 1000 / wfx.nSamplesPerSec;
  if (f1b8 == 0) {
    f1368 = data.cksize;
    f136c = 0;
  }
  if (f58 & 0x20000000) {
    if (DAT_100b4a20 != 0) {
      r2 = beginA(DAT_100b49f0);
    }
    else {
      r2 = res;
    }
    if (r2 == 0 || r2 == 0x23) {
      mmioClose(h, 0);
      for (Node *n = DAT_100b49c8.head; n != 0; n = n->next) {
        if (n->data == &info) {
          DAT_100b49c8.remove(n);
          break;
        }
      }
      DAT_100b4a08--;
      h = 0;
      if (r2 == 0) {
        f58 |= 1;
      }
      return r2;
    }
  }
  if (f5c & 0x20) {
    if (DAT_100b4a20 != 0) {
      r2 = beginB(DAT_100b49f0);
    }
    else {
      r2 = res;
    }
    if (r2 == 0 || r2 == 0x23) {
      mmioClose(h, 0);
      for (Node *n = DAT_100b49c8.head; n != 0; n = n->next) {
        if (n->data == &info) {
          DAT_100b49c8.remove(n);
          break;
        }
      }
      DAT_100b4a08--;
      h = 0;
      return r2;
    }
  }
  unsigned int size = 0x20000;
  if ((f58 & 1) || f178 < size) {
    size = f178;
    f58 |= 1;
  }
  if (!(f58 & 1)) {
    mask = 0x1ffff;
  }
  mmioSetBuffer(h, 0, size, 0);
  mmioGetInfo(h, &info, 0);
  mmioAdvance(h, &info, 0);
  if ((unsigned int)((f168 - f164) * wfx.nBlockAlign) <= (unsigned int)info.cchBuffer) {
    mmioAdvance(h, &info, 0);
    f58 |= 1;
    dec = &info;
    info.pchEndRead = (f168 - f164) * wfx.nBlockAlign + info.pchBuffer;
    for (int k = 0; k < 16; k++) {
      if (dec != 0) {
        elem[k].info = *dec;
      }
      elem[k].f88 = 0;
      elem[k].f8c = 0;
      elem[k].f90 = 0;
      elem[k].wfx = wfx;
      elem[k].f8c = f164;
      elem[k].f90 = f168;
      elem[k].f88 = 0;
      elem[k].b1 = 0;
      elem[k].b2 = 1;
      if (f1b8 != 0) {
        elem[k].fd8 = ((Loop *)f1b8)->start * wfx.nBlockAlign + dec->pchBuffer;
        elem[k].fd4 = (((Loop *)f1b8)->end + 1) * wfx.nBlockAlign + dec->pchBuffer;
        elem[k].fdc = (((Loop *)f1b8)->end - ((Loop *)f1b8)->start + 1) * wfx.nBlockAlign;
      }
      else {
        elem[k].fd8 = dec->pchBuffer;
        elem[k].fd4 = dec->pchEndRead;
        elem[k].fdc = dec->pchEndRead - dec->pchBuffer + 1;
      }
    }
  }
  else {
  dec = (MMIOINFO *)operator new(sizeof(MMIOINFO));
  memset(dec, 0, sizeof(MMIOINFO));
  dec->cchBuffer = size;
  dec->fccIOProc = 0x204d454d;
  dec->adwInfo[0] = 0;
  dec->pchBuffer = (HPSTR)malloc(size);
  h2 = mmioOpenA(0, dec, 0x1002);
  if (h2 == 0) {
    cleanup();
    return 8;
  }
  DAT_100b4a08++;
  mmioGetInfo(h2, dec, 0);
  DAT_100b49c8.append(dec);
  wp = dec->pchBuffer;
  if (f1b8 != 0) {
    f58 |= 4;
    for (int j = 0; j < 16; j++) {
      *(unsigned int *)((char *)&elem[0] + 0x98) |= 1;
    }
  }
  fill();
  mmioGetInfo(h, &info, 0);
  mmioAdvance(h, &info, 0);
  unsigned int fl = f5c;
  if ((fl & 0x40) && !(fl & 0x80)) {
    if (DAT_100b49f0 != 0) {
      if (DAT_100b49f0->create(&buf, &wfx, 0x100e0, 0x10000) == 0) {
        buf->setRate(DAT_100b4a20->rate(), 8);
        f58 |= 0x10000000;
        goto done;
      }
      fl = f5c;
    }
    f5c = fl & ~0x40;
  }
  }
done:
  f228 = 0;
  return 0;
}
}
