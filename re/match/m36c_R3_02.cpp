// FLAGS sound.dll: /O2
// m36c R3 (round 36): sound.dll release functions 0x10037780..0x1003a900, hand-matched from Ghidra's decompilation
// (raw form: Ghidra's offsets and FUN_ names). Evidence of byte identity, not readable source.
#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);
#define V(n) virtual int v##n();
#define V10(p) V(p##0) V(p##1) V(p##2) V(p##3) V(p##4) V(p##5) V(p##6) V(p##7) V(p##8) V(p##9)

namespace f_10039c40 {
extern int DAT_100b4a20;
struct T_FUN_10034ac0 { int FUN_10034ac0(void); };
struct T_FUN_1000c2f0 { void FUN_1000c2f0(int, int); };
struct C_FUN_10039c40 { int FUN_10039c40(void); };
// MATCH: sound.dll 0x10039c40 ?FUN_10039c40@C_FUN_10039c40@f_10039c40@@QAEHXZ
int C_FUN_10039c40::FUN_10039c40(void)
{
  int uVar1 = ((T_FUN_10034ac0 *)((int)this + 0x70))->FUN_10034ac0();
  if ((*(byte *)((int)this + 0xcc) & 4) != 0) {
    ((T_FUN_1000c2f0 *)DAT_100b4a20)->FUN_1000c2f0((int)this, 6);
  }
  return uVar1;
}
}

namespace f_100382c0 {
struct C_FUN_100382c0 { int FUN_100382c0(uint *param_1); };
// MATCH: sound.dll 0x100382c0 ?FUN_100382c0@C_FUN_100382c0@f_100382c0@@QAEHPAI@Z
int C_FUN_100382c0::FUN_100382c0(uint *param_1)
{
  uint bits = (*(uint *)((int)this + 0x58) >> 0xc) & 0xffff;
  *param_1 = 0;
  while ((bits & 1) != 0) {
    *param_1 = *param_1 + 1;
    bits = bits >> 1;
  }
  if (*param_1 >= 0x10) {
    return 0;
  }
  return *param_1 * 0x110 + 0x238 + (int)this;
}
}

namespace f_10038330 {
struct C_FUN_10038330 { int FUN_10038330(int param_1); };
// MATCH: sound.dll 0x10038330 ?FUN_10038330@C_FUN_10038330@f_10038330@@QAEHH@Z
int C_FUN_10038330::FUN_10038330(int param_1)
{
  uint bits = (*(uint *)((int)this + 0x58) >> 0xc) & 0xffff;
  int i;
  byte *p;
  for (i = 0, p = (byte *)((int)this + 0x2d4); i < 0x10; bits = bits >> 1, i++, p = p + 0x110) {
    if (((bits & 1) != 0) && (*(int *)(p + 0x48) == param_1) && ((*p & 8) == 0)) {
      return i * 0x110 + 0x238 + (int)this;
    }
  }
  return 0;
}
}

namespace f_10039620 {
void __cdecl FUN_1004249a(void *);
struct Node { Node *prev; Node *next; int data; };
struct C_FUN_10039620 {
  Node *head; Node *tail; int pad; int count;
  int FUN_10039620(void);
};
// MATCH: sound.dll 0x10039620 ?FUN_10039620@C_FUN_10039620@f_10039620@@QAEHXZ
int C_FUN_10039620::FUN_10039620(void)
{
  Node *n = head;
  if (n == 0) {
    return 0;
  }
  Node *next = n->next;
  if (next != 0) {
    next->prev = 0;
    head = next;
  }
  else {
    tail = 0;
    head = 0;
  }
  int d = n->data;
  FUN_1004249a(n);
  count = count - 1;
  return d;
}
}

namespace f_10039580 {
void __cdecl FUN_1004249a(void *);
struct Node { Node *prev; Node *next; int data; };
struct C_FUN_10039580 {
  Node *head; Node *tail; int pad; int count;
  int popFront(void) {
    Node *n = head;
    if (n == 0) {
      return 0;
    }
    Node *next = n->next;
    if (next != 0) {
      next->prev = 0;
      head = next;
    }
    else {
      tail = 0;
      head = 0;
    }
    int d = n->data;
    FUN_1004249a(n);
    count = count - 1;
    return d;
  }
  void FUN_10039580(void);
};
// MATCH: sound.dll 0x10039580 ?FUN_10039580@C_FUN_10039580@f_10039580@@QAEXXZ
void C_FUN_10039580::FUN_10039580(void)
{
  int d = popFront();
  while (d != 0) {
    d = popFront();
  }
}
}

namespace f_100384b0 {
void __cdecl FUN_1004249a(void *);
struct Node { Node *prev; Node *next; int data; };
struct C_FUN_100384b0 {
  Node *head; Node *tail; int pad; int count;
  int popFront(void) {
    Node *n = head;
    if (n == 0) {
      return 0;
    }
    Node *next = n->next;
    if (next != 0) {
      next->prev = 0;
      head = next;
    }
    else {
      tail = 0;
      head = 0;
    }
    int d = n->data;
    FUN_1004249a(n);
    count = count - 1;
    return d;
  }
  void FUN_100384b0(void);
};
// MATCH: sound.dll 0x100384b0 ?FUN_100384b0@C_FUN_100384b0@f_100384b0@@QAEXXZ
void C_FUN_100384b0::FUN_100384b0(void)
{
  int d = popFront();
  while (d != 0) {
    d = popFront();
  }
}
}

namespace f_10038650 {
struct List { void *head; void *tail; void *cur; int count; List() { head = 0; tail = 0; cur = 0; count = 0; } };
struct Flags { uint b0 : 1; uint open : 1; };
struct Fmt { word tag; word ch; uint rate; uint avg; word align; word bits; word cb; };
class C_FUN_10038650 {
public:
  C_FUN_10038650();
  virtual ~C_FUN_10038650();
  Flags flags;
  WAVEINCAPSA caps;
  UINT numDevs;
  int f3c;
  int f40;
  int h;
  Fmt fmt;
  List list;
};
// The +4 store `lea eax, [esi + 4]; mov [eax], edi` is a memset of the flag word (a plain or member-ctor store
// compiles to `mov [esi + 4], edi`).
// MATCH: sound.dll 0x10038650 ??0C_FUN_10038650@f_10038650@@QAE@XZ
C_FUN_10038650::C_FUN_10038650()
{
  memset(&flags, 0, sizeof(flags));
  numDevs = waveInGetNumDevs();
  f40 = 0;
  waveInGetDevCapsA(0xffffffff, &caps, 0x30);
  fmt.cb = 0;
  fmt.tag = 1;
  fmt.ch = 1;
  h = 0;
  fmt.rate = 8000;
  fmt.avg = 16000;
  fmt.align = 2;
  fmt.bits = 0x10;
}
}

namespace f_10038fd0 {
extern int DAT_100b4a20;
extern HWND DAT_100b49c0;
struct T_FUN_1002aa90 { void FUN_1002aa90(int, WPARAM, LPARAM); };
extern T_FUN_1002aa90 DAT_100b5038;
// The flag test is a bool conversion: `shr eax, 5; test al, 1` (a byte cast of a 1-bit bitfield compiles alike;
// a plain `& 1` or `!= 0` test compiles to `test byte ptr [eax + 0x1a4], 0x20`). Source not determined there.
// MATCH: sound.dll 0x10038fd0 ?FUN_10038fd0@f_10038fd0@@YGXHFJIH@Z
void __stdcall FUN_10038fd0(int param_1, short param_2, LPARAM param_3, WPARAM param_4, int param_5)
{
  if (param_2 == 0x3c0) {
    if ((DAT_100b4a20 != 0) && ((bool)((*(uint *)(DAT_100b4a20 + 0x1a4) >> 5) & 1))) {
      PostMessageA(DAT_100b49c0, 0x7f0, param_4, param_3);
      return;
    }
    DAT_100b5038.FUN_1002aa90(0x7f0, param_4, param_3);
  }
}
}

namespace f_1003a0d0 {
struct T_FUN_1002c370 { void FUN_1002c370(uint); };
struct T_FUN_10035150 { void FUN_10035150(uint); };
struct T_FUN_100351c0 { void FUN_100351c0(uint); };
struct VT { V(0) V(1) V(2) V(3) V(4) V(5) V(6) V(7) V(8) V(9) V10(1) V10(2) V10(3) V10(4) V(50) V(51) };
struct C_FUN_1003a0d0 { void FUN_1003a0d0(uint param_2); };
// MATCH: sound.dll 0x1003a0d0 ?FUN_1003a0d0@C_FUN_1003a0d0@f_1003a0d0@@QAEXI@Z
void C_FUN_1003a0d0::FUN_1003a0d0(uint param_2)
{
  if ((int)param_2 > 0x7f) {
    param_2 = 0x7f;
  }
  else if ((int)param_2 < 0) {
    param_2 = 0;
  }
  ((T_FUN_1002c370 *)this)->FUN_1002c370(param_2);
  if (((VT *)this)->v51() != 0) {
    ((T_FUN_10035150 *)((int)this + 0x70))->FUN_10035150(param_2);
  }
  if (param_2 != 0) {
    param_2 = param_2 + 1;
  }
  ((T_FUN_100351c0 *)((int)this + 0x70))->FUN_100351c0(param_2 << 9);
}
}

namespace f_1003a140 {
struct T_FUN_10034c80 { void FUN_10034c80(int, int, uint); };
struct C_FUN_1003a140 { void FUN_1003a140(int param_2, int param_3, uint param_4); };
// MATCH: sound.dll 0x1003a140 ?FUN_1003a140@C_FUN_1003a140@f_1003a140@@QAEXHHI@Z
void C_FUN_1003a140::FUN_1003a140(int param_2, int param_3, uint param_4)
{
  if ((param_2 != 0) && (param_2 != -1)) {
    if (param_2 > 0x7f) {
      param_2 = 0x7f;
    }
    else if (param_2 < 0) {
      param_2 = 0;
    }
    param_2 = (param_2 + 1) * 0x200;
  }
  if ((param_3 != 0) && (param_3 != -1)) {
    if (param_3 > 0x7f) {
      param_3 = 0x7f;
    }
    else if (param_3 < 0) {
      param_3 = 0;
    }
    param_3 = (param_3 + 1) * 0x200;
  }
  ((T_FUN_10034c80 *)((int)this + 0x70))->FUN_10034c80(param_2, param_3, param_4);
}
}
