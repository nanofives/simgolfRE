// FLAGS sound.dll: /O2
// m36c R3 (round 36): sound.dll release functions 0x10037780..0x1003a900 (wave-in device), hand-matched from Ghidra's
// decompilation (raw form: Ghidra's offsets and FUN_ names). Evidence of byte identity, not readable source.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

// Shared layout (offsets from the disassembly): +4 flag word (bit 1 written as a 1-bit field: `and al, 0xfd` /
// `or al, 2`), +0x44 HWAVEIN, +0x48 WAVEFORMATEX, +0x5c list {head, tail, cur, count} of nodes {prev, next, data}.
#define WAVEIN_CLASS(NAME, METHOD) \
  struct Node { Node *prev; Node *next; WAVEHDR *data; }; \
  struct List { \
    Node *head; Node *tail; Node *cur; int count; \
    WAVEHDR *first() { cur = head; if (cur == 0) return 0; return cur->data; } \
    WAVEHDR *next() { if (cur != 0) { cur = cur->next; if (cur != 0) return cur->data; } return 0; } \
  }; \
  struct Flags { uint b0 : 1; uint open : 1; }; \
  inline int mapErr(MMRESULT r) { \
    switch (r) { \
    case 2: case 5: return 10; \
    case 6: return 1; \
    case 0x21: return 0xe; \
    case 4: return 0xc; \
    case 7: return 0x11; \
    case 0x20: return 0x12; \
    default: return 0xb; \
    } \
  } \
  struct NAME { \
    void *vtbl; Flags flags; char pad8[0x3c]; HWAVEIN h; WAVEFORMATEX fmt; char pad5a[2]; List list; \
    METHOD; \
  };

namespace f_10038890 {
void *__cdecl operator_new(uint);
struct Hdr {
  char *lpData; DWORD len; DWORD rec; DWORD user; DWORD flags; DWORD loops; Hdr *next; DWORD reserved;
  Hdr() { lpData = 0; len = 0; rec = 0; user = (DWORD)this; flags = 0; loops = 0; next = 0; reserved = 0; }
};
struct Node { Node *prev; Node *next; Hdr *data; Node(Hdr *d) { prev = 0; next = 0; data = d; } };
struct List {
  Node *head; Node *tail; Node *cur; int count;
  Hdr *first() { cur = head; if (cur != 0) return cur->data; return 0; }
  Hdr *last() { cur = tail; if (cur != 0) return cur->data; return 0; }
  void append(Hdr *d) {
    Node *n = new Node(d);
    if (tail != 0) {
      tail->next = n;
      n->prev = tail;
    }
    else {
      head = n;
    }
    tail = n;
    count++;
  }
};
struct C_FUN_10038890 {
  void *vtbl; int flags; char pad8[0x3c]; HWAVEIN h; WAVEFORMATEX fmt; char pad5a[2]; List list;
  int FUN_10038890(uint param_1, uint param_2);
};
// MATCH: sound.dll 0x10038890 ?FUN_10038890@C_FUN_10038890@f_10038890@@QAEHII@Z
int C_FUN_10038890::FUN_10038890(uint param_1, uint param_2)
{
  Hdr *prev = 0;
  if (list.first() != 0) {
    return 0x10;
  }
  // The loop ends `jae exit; jmp top` (a break at the bottom of an endless loop); a for/while/do loop ends `jb top`.
  uint i = 0;
  if (i < param_1) {
    for (;;) {
      Hdr *hdr = new Hdr;
      if (hdr->lpData != 0) {
        return 0x10;
      }
      if (param_2 == 0) {
        return 10;
      }
      hdr->lpData = (char *)operator new(param_2);
      hdr->len = param_2;
      if (prev != 0) {
        prev->next = hdr;
      }
      prev = hdr;
      list.append(hdr);
      i++;
      if (i >= param_1) {
        break;
      }
    }
  }
  Hdr *l = list.last();
  l->next = list.first();
  return 0;
}
}

namespace f_10038b40 {
WAVEIN_CLASS(C_FUN_10038b40, int FUN_10038b40(void))
// MATCH: sound.dll 0x10038b40 ?FUN_10038b40@C_FUN_10038b40@f_10038b40@@QAEHXZ
int C_FUN_10038b40::FUN_10038b40(void)
{
  WAVEHDR *hdr = list.first();
  if (hdr == 0) {
    return 0x13;
  }
  if (h == 0) {
    return 0x13;
  }
  for (; hdr != 0; hdr = list.next()) {
    MMRESULT r = waveInPrepareHeader(h, hdr, 0x20);
    if (r != 0) {
      return mapErr(r);
    }
  }
  return 0;
}
}

namespace f_10038da0 {
WAVEIN_CLASS(C_FUN_10038da0, int FUN_10038da0(void))
// MATCH: sound.dll 0x10038da0 ?FUN_10038da0@C_FUN_10038da0@f_10038da0@@QAEHXZ
int C_FUN_10038da0::FUN_10038da0(void)
{
  WAVEHDR *hdr = list.first();
  if (hdr == 0) {
    return 0x13;
  }
  if (h == 0) {
    return 0x13;
  }
  for (; hdr != 0; hdr = list.next()) {
    MMRESULT r = waveInAddBuffer(h, hdr, 0x20);
    if (r != 0) {
      return mapErr(r);
    }
  }
  return 0;
}
}

namespace f_10039050 {
WAVEIN_CLASS(C_FUN_10039050, int FUN_10039050(void))
void CALLBACK FUN_1000179e(HWAVEIN, UINT, DWORD, DWORD, DWORD);
// MATCH: sound.dll 0x10039050 ?FUN_10039050@C_FUN_10039050@f_10039050@@QAEHXZ
int C_FUN_10039050::FUN_10039050(void)
{
  if (h != 0) {
    return 0xc;
  }
  MMRESULT r = waveInOpen(&h, 0xffffffff, &fmt, (DWORD)FUN_1000179e, (DWORD)this, 0x30000);
  if (r != 0) {
    return mapErr(r);
  }
  flags.open = 1;
  return 0;
}
}

namespace f_10039140 {
WAVEIN_CLASS(C_FUN_10039140, int FUN_10039140(void))
extern CRITICAL_SECTION DAT_100b503c;
// MATCH: sound.dll 0x10039140 ?FUN_10039140@C_FUN_10039140@f_10039140@@QAEHXZ
int C_FUN_10039140::FUN_10039140(void)
{
  if (h == 0) {
    return 0x13;
  }
  EnterCriticalSection(&DAT_100b503c);
  flags.open = 0;
  waveInStop(h);
  waveInReset(h);
  WAVEHDR *hdr = list.first();
  if ((hdr != 0) && (h != 0)) {
    for (; hdr != 0; hdr = list.next()) {
      if (waveInUnprepareHeader(h, hdr, 0x20) != 0) {
        break;
      }
    }
  }
  MMRESULT r = waveInClose(h);
  if (r != 0) {
    return mapErr(r);
  }
  LeaveCriticalSection(&DAT_100b503c);
  h = 0;
  return 0;
}
}
