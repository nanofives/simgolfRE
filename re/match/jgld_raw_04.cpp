// FLAGS jgld.dll: /Od /ZI /GZ
// jgld.dll functions matched from Ghidra's decompilation by re/tools/ghidra2src.py (raw form: Ghidra's offsets,
// casts and FUN_/DAT_ names; in a /Od build this compiles to the same instructions as the original member
// accesses, so each // MATCH: below is a 100% instruction match). Names, types and layouts are still to be
// written: these are evidence of byte identity, not readable source. String literals are placeholders named by
// address (s_<addr>), never the game's text.

#include <windows.h>
typedef unsigned char undefined; typedef unsigned char undefined1; typedef unsigned short undefined2;
typedef unsigned int undefined4; typedef unsigned __int64 undefined8; typedef unsigned int uint;
typedef unsigned short ushort; typedef unsigned char uchar; typedef unsigned char byte; typedef unsigned long ulong;
typedef __int64 longlong; typedef unsigned __int64 ulonglong; typedef signed char sbyte; typedef unsigned short word;
typedef unsigned int dword; typedef long double float10; typedef int code(...);

namespace f_10067560 {
// MATCH: jgld.dll 0x10067560 ?FUN_10067560@f_10067560@@YIIH@Z
undefined4 __fastcall FUN_10067560(int param_1)
{
  return *(undefined4 *)(param_1 + 0x144);
}
}

namespace f_100675a0 {
// MATCH: jgld.dll 0x100675a0 ?FUN_100675a0@f_100675a0@@YIIH@Z
undefined4 __fastcall FUN_100675a0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x140);
}
}

namespace f_100675e0 {
// MATCH: jgld.dll 0x100675e0 ?FUN_100675e0@f_100675e0@@YIXH@Z
void __fastcall FUN_100675e0(int param_1)
{
  ShowWindow(*(HWND *)(param_1 + 0x144),1);
  return;
}
}

namespace f_10067640 {
// MATCH: jgld.dll 0x10067640 ?FUN_10067640@f_10067640@@YIXH@Z
void __fastcall FUN_10067640(int param_1)
{
  ShowWindow(*(HWND *)(param_1 + 0x144),0);
  return;
}
}

namespace f_100676a0 {
// MATCH: jgld.dll 0x100676a0 ?FUN_100676a0@f_100676a0@@YIXH@Z
void __fastcall FUN_100676a0(int param_1)
{
  ShowWindow(*(HWND *)(param_1 + 0x144),6);
  return;
}
}

namespace f_10067700 {
// MATCH: jgld.dll 0x10067700 ?FUN_10067700@f_10067700@@YIXH@Z
void __fastcall FUN_10067700(int param_1)
{
  ShowWindow(*(HWND *)(param_1 + 0x144),3);
  return;
}
}

namespace f_10067760 {
// MATCH: jgld.dll 0x10067760 ?FUN_10067760@f_10067760@@YIIH@Z
undefined4 __fastcall FUN_10067760(int param_1)
{
  return *(undefined4 *)(param_1 + 300);
}
}

namespace f_100677a0 {
// MATCH: jgld.dll 0x100677a0 ?FUN_100677a0@f_100677a0@@YIIH@Z
undefined4 __fastcall FUN_100677a0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x130);
}
}

namespace f_100677e0 {
// MATCH: jgld.dll 0x100677e0 ?FUN_100677e0@f_100677e0@@YIIH@Z
undefined4 __fastcall FUN_100677e0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x134);
}
}

namespace f_10067fa0 {
// MATCH: jgld.dll 0x10067fa0 ?FUN_10067fa0@f_10067fa0@@YIPAXPAI@Z
extern void *PTR_LAB_1011da58;
int __fastcall thunk_FUN_10068570(int);
void * __fastcall FUN_10067fa0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011da58);
  param_1[1] = 0;
  thunk_FUN_10068570((int)param_1);
  return param_1;
}
}

namespace f_10068000 {
// MATCH: jgld.dll 0x10068000 ?FUN_10068000@f_10068000@@YIXH@Z
int __cdecl operator_delete(void *);
void __fastcall FUN_10068000(int param_1)
{
    void * local_c;
  if (*(int *)(param_1 + 4) != 0) {
    local_c = *(void **)(param_1 + 4);
    operator_delete(local_c);
  }
  *(undefined4 *)(param_1 + 4) = 0;
  *(undefined4 *)(param_1 + 8) = 0;
  *(undefined4 *)(param_1 + 0xc) = 0;
  *(undefined4 *)(param_1 + 0x10) = 1;
  return;
}
}

namespace f_100680a0 {
// MATCH: jgld.dll 0x100680a0 ?FUN_100680a0@f_100680a0@@YIPAXPAI@Z
extern void *PTR_LAB_1011da5c;
int __fastcall thunk_FUN_100685c0(int);
void * __fastcall FUN_100680a0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011da5c);
  param_1[1] = 0;
  thunk_FUN_100685c0((int)param_1);
  return param_1;
}
}

namespace f_10068100 {
// MATCH: jgld.dll 0x10068100 ?FUN_10068100@f_10068100@@YIXH@Z
int __cdecl operator_delete(void *);
void __fastcall FUN_10068100(int param_1)
{
    void * local_c;
  if (*(int *)(param_1 + 4) != 0) {
    local_c = *(void **)(param_1 + 4);
    operator_delete(local_c);
  }
  *(undefined4 *)(param_1 + 4) = 0;
  *(undefined4 *)(param_1 + 8) = 0;
  *(undefined4 *)(param_1 + 0xc) = 0;
  *(undefined4 *)(param_1 + 0x10) = 1;
  return;
}
}

namespace f_10068300 {
// MATCH: jgld.dll 0x10068300 ?FUN_10068300@f_10068300@@YIPAXPAI@Z
extern void *PTR_LAB_1011da60;
int __fastcall thunk_FUN_10068610(int);
void * __fastcall FUN_10068300(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011da60);
  param_1[1] = 0;
  thunk_FUN_10068610((int)param_1);
  return param_1;
}
}

namespace f_10068360 {
// MATCH: jgld.dll 0x10068360 ?FUN_10068360@f_10068360@@YIXH@Z
int __cdecl operator_delete(void *);
void __fastcall FUN_10068360(int param_1)
{
    void * local_c;
  if (*(int *)(param_1 + 4) != 0) {
    local_c = *(void **)(param_1 + 4);
    operator_delete(local_c);
  }
  *(undefined4 *)(param_1 + 4) = 0;
  *(undefined4 *)(param_1 + 8) = 0;
  *(undefined4 *)(param_1 + 0xc) = 0;
  *(undefined4 *)(param_1 + 0x10) = 1;
  return;
}
}

namespace f_10068570 {
// MATCH: jgld.dll 0x10068570 ?FUN_10068570@f_10068570@@YIHH@Z
int __fastcall thunk_FUN_10068000(int);
int __fastcall FUN_10068570(int param_1)
{
  thunk_FUN_10068000(param_1);
  return 0;
}
}

namespace f_100685c0 {
// MATCH: jgld.dll 0x100685c0 ?FUN_100685c0@f_100685c0@@YIHH@Z
int __fastcall thunk_FUN_10068100(int);
int __fastcall FUN_100685c0(int param_1)
{
  thunk_FUN_10068100(param_1);
  return 0;
}
}

namespace f_10068610 {
// MATCH: jgld.dll 0x10068610 ?FUN_10068610@f_10068610@@YIHH@Z
int __fastcall thunk_FUN_10068360(int);
int __fastcall FUN_10068610(int param_1)
{
  thunk_FUN_10068360(param_1);
  return 0;
}
}

namespace f_10069140 {
// MATCH: jgld.dll 0x10069140 ?FUN_10069140@C_FUN_10069140@f_10069140@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_10069010(undefined4 *);
struct C_FUN_10069140 { void * FUN_10069140(uint param_1); };
void * C_FUN_10069140::FUN_10069140(uint param_1)
{
  thunk_FUN_10069010((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_100691b0 {
// MATCH: jgld.dll 0x100691b0 ?FUN_100691b0@C_FUN_100691b0@f_100691b0@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_10068760(undefined4 *);
struct C_FUN_100691b0 { void * FUN_100691b0(uint param_1); };
void * C_FUN_100691b0::FUN_100691b0(uint param_1)
{
  thunk_FUN_10068760((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10069270 {
// MATCH: jgld.dll 0x10069270 ?FUN_10069270@f_10069270@@YIII@Z
extern int DAT_10128728;
undefined4 __fastcall FUN_10069270(undefined4 param_1)
{
  DAT_10128728 = 0;
  return param_1;
}
}

namespace f_1006a150 {
// MATCH: jgld.dll 0x1006a150 ?FUN_1006a150@C_FUN_1006a150@f_1006a150@@QAEPAXI@Z
extern void *PTR_LAB_1011db10;
int __fastcall thunk_FUN_1006a260(undefined4 *);
struct C_FUN_1006a150 { void * FUN_1006a150(undefined4 param_1); };
void * C_FUN_1006a150::FUN_1006a150(undefined4 param_1)
{
  thunk_FUN_1006a260((unsigned int *)(this));
  *((undefined4 *)this) = (unsigned int)(&PTR_LAB_1011db10);
  ((undefined4 *)this)[0x203] = param_1;
  ((undefined4 *)this)[2] = 0;
  ((undefined4 *)this)[0x204] = 0;
  ((undefined4 *)this)[0x205] = 0;
  return this;
}
}

namespace f_1006a1f0 {
// MATCH: jgld.dll 0x1006a1f0 ?FUN_1006a1f0@C_FUN_1006a1f0@f_1006a1f0@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_1006a2c0(undefined4 *);
struct C_FUN_1006a1f0 { void * FUN_1006a1f0(uint param_1); };
void * C_FUN_1006a1f0::FUN_1006a1f0(uint param_1)
{
  thunk_FUN_1006a2c0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_1006a260 {
// MATCH: jgld.dll 0x1006a260 ?FUN_1006a260@f_1006a260@@YIPAXPAI@Z
extern void *PTR_LAB_1011db3c;
int __fastcall thunk_FUN_10006ab0(undefined4 *);
void * __fastcall FUN_1006a260(undefined4 *param_1)
{
  thunk_FUN_10006ab0(param_1);
  *param_1 = (unsigned int)(&PTR_LAB_1011db3c);
  return param_1;
}
}

namespace f_1006a380 {
// MATCH: jgld.dll 0x1006a380 ?FUN_1006a380@f_1006a380@@YIXH@Z
void __fastcall FUN_1006a380(int param_1)
{
  *(undefined4 *)(param_1 + 0x810) = 0;
  *(undefined4 *)(param_1 + 0x814) = 0;
  return;
}
}

namespace f_1006a8e0 {
// MATCH: jgld.dll 0x1006a8e0 ?FUN_1006a8e0@C_FUN_1006a8e0@f_1006a8e0@@QAEIHHH@Z
struct C_FUN_1006a8e0 { undefined4 FUN_1006a8e0(int param_1, int param_2, int param_3); };
undefined4 C_FUN_1006a8e0::FUN_1006a8e0(int param_1, int param_2, int param_3)
{
    int local_c;
  for (local_c = 0; local_c < param_3; local_c = local_c + 1) {
    *(undefined1 *)(param_1 + local_c * 3) = *(undefined1 *)((int)this + (local_c + param_2) * 4 + 0xc);
    *(undefined1 *)(param_1 + 1 + local_c * 3) = *(undefined1 *)((int)this + (local_c + param_2) * 4 + 0xd);
    *(undefined1 *)(param_1 + 2 + local_c * 3) = *(undefined1 *)((int)this + (local_c + param_2) * 4 + 0xe);
  }
  return 0;
}
}

namespace f_1006add0 {
// MATCH: jgld.dll 0x1006add0 ?FUN_1006add0@C_FUN_1006add0@f_1006add0@@QAEPAEH@Z
extern char DAT_10128734;
extern char DAT_10128735;
extern char DAT_10128736;
struct C_FUN_1006add0 { undefined * FUN_1006add0(int param_1); };
undefined * C_FUN_1006add0::FUN_1006add0(int param_1)
{
  DAT_10128734 = *(undefined1 *)((int)this + param_1 * 4 + 0xc);
  DAT_10128735 = *(undefined1 *)((int)this + param_1 * 4 + 0xd);
  DAT_10128736 = *(undefined1 *)((int)this + param_1 * 4 + 0xe);
  return (unsigned char *)(((char *)&DAT_10128734));
}
}

namespace f_1006ae50 {
// MATCH: jgld.dll 0x1006ae50 ?FUN_1006ae50@C_FUN_1006ae50@f_1006ae50@@QAEXH@Z
struct C_FUN_1006ae50 { void FUN_1006ae50(int param_1); };
void C_FUN_1006ae50::FUN_1006ae50(int param_1)
{
    int local_c;
  for (local_c = 0; local_c < 0x100; local_c = local_c + 1) {
    *(undefined1 *)(param_1 + 2 + local_c * 4) = *(undefined1 *)((int)this + local_c * 4 + 0xc);
    *(undefined1 *)(param_1 + 1 + local_c * 4) = *(undefined1 *)((int)this + local_c * 4 + 0xd);
    *(undefined1 *)(param_1 + local_c * 4) = *(undefined1 *)((int)this + local_c * 4 + 0xe);
    *(undefined1 *)(param_1 + 3 + local_c * 4) = 0;
  }
  return;
}
}

namespace f_1006af00 {
// MATCH: jgld.dll 0x1006af00 ?FUN_1006af00@f_1006af00@@YIXPAI@Z
extern void *PTR_LAB_1011db3c;
int __fastcall thunk_FUN_10006b80(undefined4 *);
void __fastcall FUN_1006af00(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011db3c);
  thunk_FUN_10006b80(param_1);
  return;
}
}

namespace f_1006af50 {
// MATCH: jgld.dll 0x1006af50 ?FUN_1006af50@C_FUN_1006af50@f_1006af50@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_1006af00(undefined4 *);
struct C_FUN_1006af50 { void * FUN_1006af50(uint param_1); };
void * C_FUN_1006af50::FUN_1006af50(uint param_1)
{
  thunk_FUN_1006af00((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_1006afc0 {
// MATCH: jgld.dll 0x1006afc0 ?FUN_1006afc0@C_FUN_1006afc0@f_1006afc0@@QAEXI@Z
struct C_FUN_1006afc0 { void FUN_1006afc0(undefined4 param_1); };
void C_FUN_1006afc0::FUN_1006afc0(undefined4 param_1)
{
  *(undefined4 *)this = param_1;
  return;
}
}

namespace f_1006b000 {
// MATCH: jgld.dll 0x1006b000 ?FUN_1006b000@f_1006b000@@YIHH@Z
int __fastcall FUN_1006b000(int param_1)
{
  return param_1 + 0x40c;
}
}

namespace f_1006b040 {
// MATCH: jgld.dll 0x1006b040 ?FUN_1006b040@f_1006b040@@YIHH@Z
int __fastcall FUN_1006b040(int param_1)
{
  return param_1 + 0x60c;
}
}

