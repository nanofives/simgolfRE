// m36c WIP (round 36): best 58.8% for sound.dll 0x10033300, from scratch n2.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2
// scratch
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10033300 {
struct Dec { char pad[0x14]; unsigned int size; char *base; char *rd; char *wr; char *end; };
struct C_FUN_10033300 {
  HMMIO h; int f4; Dec *dec; MMIOINFO info; int f54; unsigned int b0:1, b1:1, b2:1, b3:1, b4:1, b5:1, b6:1, b7:1, b8:1, b9:1, b10:1, bx:21; char pad1[0x170 - 0x5c];
  unsigned int f170; int f174; unsigned int f178; char pad2[0x1b8 - 0x17c]; int f1b8;
  char pad3[0x1368 - 0x1bc]; unsigned int f1368; unsigned int f136c; int f1370; unsigned int mask; char *wp;
  char pad4[0x13a0 - 0x137c]; int f13a0;
  int readInto(char *dst, unsigned int &n) {
    int err = 0;
    unsigned int avail = info.pchEndRead - info.pchNext;
    if (n > avail) {
      n = avail;
      err = 0x2b;
    }
    memcpy(dst, info.pchNext, n);
    info.pchNext += n;
    mmioSetInfo(h, &info, 0);
    return err;
  }
  int FUN_10033300(); };
// MATCH: sound.dll 0x10033300 ?FUN_10033300@C_FUN_10033300@f_10033300@@QAEHXZ
int C_FUN_10033300::FUN_10033300()
{
  unsigned int n1, n2, rem, space;
  char *p2;
  int err;
  if (b5) {
    return 0x26;
  }
  Dec *d = dec;
  p2 = d->base;
  char *r = d->rd - 4;
  if (r < p2) {
    r += d->size;
  }
  char *w = wp;
  space = (r - w) & mask;
  if (space == 0) {
    space = d->size;
  }
  unsigned int looping = b2;
  if (looping && f1b8 == 0 && f13a0 == 0) {
    rem = f136c + f1368 - f170;
  }
  else if (looping && f1b8 != 0 && !b7) {
    rem = f136c + f1368 - f170;
  }
  else {
    rem = f178 - f170;
  }
  if (rem >= space) {
    if (w + space > d->end) {
      n1 = d->end - w;
      n2 = space - n1;
    }
    else {
      n1 = space;
      n2 = 0;
      p2 = 0;
    }
    err = readInto(w, n1);
    if (p2 != 0 && err == 0) {
      err = readInto(p2, n2);
      w = dec->base + n2;
      n1 += n2;
    }
    else {
      w = wp + n1;
    }
    wp = w;
    f170 += n1;
    dec->wr = dec->end;
    if (err == 0) {
      goto done;
    }
  }
  else if (looping && !b7 && f13a0 == 0) {
    if (w + rem > d->end) {
      n1 = d->end - w;
      n2 = rem - n1;
    }
    else {
      n1 = rem;
      n2 = 0;
      p2 = 0;
    }
    err = readInto(w, n1);
    unsigned int total;
    if (p2 != 0 && err == 0) {
      err = readInto(p2, n2);
      w = dec->base + n2;
      total = n1 + n2;
    }
    else {
      w = wp + n1;
      total = n1;
    }
    wp = w;
    f170 += total;
    if (err == 0) {
      if (n1 + n2 == rem) {
        mmioSeek(h, -(LONG)f1368, SEEK_CUR);
        mmioGetInfo(h, &info, 0);
        mmioAdvance(h, &info, 0);
        f170 = f136c;
        b10 = 0;
        return 0;
      }
      goto done;
    }
    if (err != 0x2b) {
      goto done;
    }
  }
  else {
    if (w + rem > d->end) {
      n1 = d->end - w;
      n2 = rem - n1;
    }
    else {
      n1 = rem;
      n2 = 0;
      p2 = 0;
    }
    err = readInto(w, n1);
    if (p2 != 0 && err == 0) {
      err = readInto(p2, n2);
      w = dec->base + n2;
      n1 += n2;
    }
    else {
      w = wp + n1;
    }
    wp = w;
    f170 += n1;
    if (err == 0x2b) {
      mmioAdvance(h, &info, 0);
    }
    b10 = 0;
    dec->wr = w;
    if (f170 == f178) {
      b5 = 1;
    }
    return 0;
  }
  mmioAdvance(h, &info, 0);
done:
  b10 = 0;
  return 0;
}
}
