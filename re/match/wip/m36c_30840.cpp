// m36c WIP (round 36): best 55.0% for sound.dll 0x10030840, from scratch l.cpp; what differs: log/c3/m36c_*_notes.txt
// FLAGS sound.dll: /O2 /GX
// scratch
#include <windows.h>
#include <mmreg.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10030840 {
extern int DAT_100b4a08;
extern "C" void __cdecl FUN_1003a980(int *bits, float *speech, void *state);
struct Elem { MMIOINFO info; char p0[0x90 - 0x48]; unsigned int f90; char p1[0x98 - 0x94]; unsigned int f98;
  unsigned char f9c; char p2[0xa0 - 0x9d]; WAVEFORMATEX wfx; char p3[0xb4 - 0xb2]; Elem *fb4; char p4[0xcc - 0xb8];
  int fcc; char p5[0xd4 - 0xd0]; HPSTR fd4; HPSTR fd8; char p6[0x110 - 0xdc];
  Elem(); ~Elem(); int init(MMIOINFO *mi, WAVEFORMATEX *fmt); };
struct Node { Node *prev; Node *next; void *data; Node(void *d) { prev = 0; next = 0; data = d; } };
struct List {
  Node *head; Node *tail; Node *cur; int count;
  void *last() { cur = tail; return cur ? cur->data : 0; }
  void append(void *d) {
    Node *n = new Node(d);
    if (tail != 0) { tail->next = n; n->prev = tail; }
    else head = n;
    tail = n;
    count++;
  }
};
struct C_FUN_10030840 { char pad[0x5c]; List list; char pad1[0xbc - 0x6c]; WAVEFORMATEX wfx; char pad2[0xd4 - 0xce];
  void *codec; char pad3[0xe0 - 0xd8]; int fe0;
  int FUN_10030840(Elem *src); };
// MATCH: sound.dll 0x10030840 ?FUN_10030840@C_FUN_10030840@f_10030840@@QAEHPAUElem@2@@Z
int C_FUN_10030840::FUN_10030840(Elem *src)
{
  float v;
  HMMIO h;
  MMIOINFO info;
  int bits[54];
  float speech[180];
  if (src == 0) {
    return 10;
  }
  if (codec == 0) {
    return 0x13;
  }
  memset(&info, 0, sizeof(info));
  info.fccIOProc = 0x204d454d;
  info.cchBuffer = 0x168;
  info.adwInfo[0] = 100;
  h = mmioOpenA(0, &info, 0x1000);
  if (h == 0) {
    return 8;
  }
  DAT_100b4a08++;
  mmioGetInfo(h, &info, 0);
  char *p = src->info.pchBuffer;
  int c = (int)h;
  for (int i = 0; i < 54; i++) {
    if (i % 8 == 0) {
      c = *p++;
    }
    bits[i] = ((0x80 >> (i & 7)) & c) != 0;
  }
  FUN_1003a980(bits, speech, codec);
  short *out = (short *)info.pchBuffer;
  for (int k = 0; k < 180; k++) {
    v = speech[k] * 32768.0f;
    if (v < -32768.0) {
      *out = -32768;
    }
    else if (v > 32767.0) {
      *out = 32767;
    }
    else {
      *out = (short)v;
    }
    out++;
  }
  info.pchNext = info.pchBuffer;
  info.pchEndRead = info.pchBuffer + 0x168;
  mmioSetInfo(h, &info, 0);
  Elem *item = new Elem;
  item->init(&info, &wfx);
  Elem *prev = (Elem *)list.last();
  list.append(item);
  item->f90 = (unsigned int)(src->info.pchEndRead - src->info.pchBuffer) / item->wfx.nBlockAlign;
  item->fd4 = item->info.pchEndRead;
  item->fd8 = item->info.pchBuffer;
  if (prev != 0) {
    prev->f98 |= 1;
    prev->f9c |= 1;
    prev->fb4 = item;
    prev->fcc = 0;
  }
  fe0++;
  return 0;
}
}
