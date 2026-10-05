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

namespace f_1000aa70 {
// MATCH: jgld.dll 0x1000aa70 ?FUN_1000aa70@f_1000aa70@@YIHH@Z
int __fastcall FUN_1000aa70(int param_1)
{
  return *(int *)(param_1 + 0x5c) - *(int *)(param_1 + 0x54);
}
}

namespace f_1000aab0 {
// MATCH: jgld.dll 0x1000aab0 ?FUN_1000aab0@f_1000aab0@@YIHH@Z
int __fastcall FUN_1000aab0(int param_1)
{
  return *(int *)(param_1 + 0x60) - *(int *)(param_1 + 0x58);
}
}

namespace f_1000aaf0 {
// MATCH: jgld.dll 0x1000aaf0 ?FUN_1000aaf0@f_1000aaf0@@YIIH@Z
undefined4 __fastcall FUN_1000aaf0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x40);
}
}

namespace f_1000ab30 {
// MATCH: jgld.dll 0x1000ab30 ?FUN_1000ab30@f_1000ab30@@YIHH@Z
int __fastcall FUN_1000ab30(int param_1)
{
  return param_1 + 0x24;
}
}

namespace f_1000ab70 {
// MATCH: jgld.dll 0x1000ab70 ?FUN_1000ab70@f_1000ab70@@YIIH@Z
undefined4 __fastcall FUN_1000ab70(int param_1)
{
  return *(undefined4 *)(param_1 + 0x7c);
}
}

namespace f_1000aeb0 {
// MATCH: jgld.dll 0x1000aeb0 ?FUN_1000aeb0@C_FUN_1000aeb0@f_1000aeb0@@QAEXI@Z
struct C_FUN_1000aeb0 { void FUN_1000aeb0(undefined4 param_1); };
void C_FUN_1000aeb0::FUN_1000aeb0(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x28) = param_1;
  return;
}
}

namespace f_1000aef0 {
// MATCH: jgld.dll 0x1000aef0 ?FUN_1000aef0@f_1000aef0@@YIIH@Z
undefined4 __fastcall FUN_1000aef0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x810);
}
}

namespace f_10010390 {
// MATCH: jgld.dll 0x10010390 ?FUN_10010390@f_10010390@@YAXPAIIIII@Z
void __cdecl FUN_10010390(undefined4 *param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4, undefined4 param_5)
{
  *param_1 = param_2;
  param_1[1] = param_3;
  param_1[2] = param_4;
  param_1[3] = param_5;
  return;
}
}

namespace f_10014af0 {
// MATCH: jgld.dll 0x10014af0 ?FUN_10014af0@f_10014af0@@YIPAXPAI@Z
extern void *PTR_LAB_1011d380;
int __fastcall thunk_FUN_10014c70(undefined4 *);
void * __fastcall FUN_10014af0(undefined4 *param_1)
{
  thunk_FUN_10014c70(param_1);
  *param_1 = (unsigned int)(&PTR_LAB_1011d380);
  param_1[3] = 0;
  param_1[2] = 0;
  param_1[4] = 0;
  param_1[5] = 0;
  param_1[6] = 0;
  param_1[7] = 0;
  param_1[8] = 0;
  param_1[9] = 0;
  param_1[10] = 0xff;
  param_1[0xb] = 0;
  param_1[0xc] = 0;
  param_1[0xd] = 0;
  InitializeCriticalSection((LPCRITICAL_SECTION)(param_1 + 0xe));
  return param_1;
}
}

namespace f_10014c00 {
// MATCH: jgld.dll 0x10014c00 ?FUN_10014c00@C_FUN_10014c00@f_10014c00@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_10014de0(undefined4 *);
struct C_FUN_10014c00 { void * FUN_10014c00(uint param_1); };
void * C_FUN_10014c00::FUN_10014c00(uint param_1)
{
  thunk_FUN_10014de0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10014c70 {
// MATCH: jgld.dll 0x10014c70 ?FUN_10014c70@f_10014c70@@YIPAXPAI@Z
extern void *PTR_LAB_1011d444;
int __fastcall thunk_FUN_10006ab0(undefined4 *);
void * __fastcall FUN_10014c70(undefined4 *param_1)
{
  thunk_FUN_10006ab0(param_1);
  *param_1 = (unsigned int)(&PTR_LAB_1011d444);
  return param_1;
}
}

namespace f_10014cd0 {
// MATCH: jgld.dll 0x10014cd0 ?FUN_10014cd0@C_FUN_10014cd0@f_10014cd0@@QAEPAXI@Z
extern void *PTR_LAB_1011d380;
int __fastcall thunk_FUN_10014c70(undefined4 *);
struct C_FUN_10014cd0 { void * FUN_10014cd0(undefined4 param_1); };
void * C_FUN_10014cd0::FUN_10014cd0(undefined4 param_1)
{
  thunk_FUN_10014c70((unsigned int *)(this));
  *((undefined4 *)this) = (unsigned int)(&PTR_LAB_1011d380);
  ((undefined4 *)this)[3] = param_1;
  ((undefined4 *)this)[2] = 0;
  ((undefined4 *)this)[4] = 0;
  ((undefined4 *)this)[5] = 0;
  ((undefined4 *)this)[6] = 0;
  ((undefined4 *)this)[7] = 0;
  ((undefined4 *)this)[8] = 0;
  ((undefined4 *)this)[9] = 0;
  ((undefined4 *)this)[10] = 0xff;
  ((undefined4 *)this)[0xb] = 0;
  ((undefined4 *)this)[0xc] = 0;
  ((undefined4 *)this)[0xd] = 0;
  InitializeCriticalSection((LPCRITICAL_SECTION)(((undefined4 *)this) + 0xe));
  return this;
}
}

namespace f_10016930 {
// MATCH: jgld.dll 0x10016930 ?FUN_10016930@C_FUN_10016930@f_10016930@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_100168e0(undefined4 *);
struct C_FUN_10016930 { void * FUN_10016930(uint param_1); };
void * C_FUN_10016930::FUN_10016930(uint param_1)
{
  thunk_FUN_100168e0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10017eb0 {
// MATCH: jgld.dll 0x10017eb0 ?FUN_10017eb0@C_FUN_10017eb0@f_10017eb0@@QAEXI@Z
struct C_FUN_10017eb0 { void FUN_10017eb0(undefined4 param_1); };
void C_FUN_10017eb0::FUN_10017eb0(undefined4 param_1)
{
  *(undefined4 *)((int)this + 0x10) = param_1;
  return;
}
}

namespace f_10017ef0 {
// MATCH: jgld.dll 0x10017ef0 ?FUN_10017ef0@f_10017ef0@@YIIH@Z
undefined4 __fastcall FUN_10017ef0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x10);
}
}

namespace f_10018060 {
// MATCH: jgld.dll 0x10018060 ?FUN_10018060@f_10018060@@YIIH@Z
undefined4 __fastcall FUN_10018060(int param_1)
{
  return *(undefined4 *)(param_1 + 0x30);
}
}

namespace f_100180a0 {
// MATCH: jgld.dll 0x100180a0 ?FUN_100180a0@f_100180a0@@YIIH@Z
undefined4 __fastcall FUN_100180a0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x34);
}
}

namespace f_100180e0 {
// MATCH: jgld.dll 0x100180e0 ?FUN_100180e0@f_100180e0@@YIIH@Z
uint __fastcall FUN_100180e0(int param_1)
{
  return *(uint *)(param_1 + 0x18) & 1;
}
}

namespace f_100655c0 {
// MATCH: jgld.dll 0x100655c0 ?FUN_100655c0@C_FUN_100655c0@f_100655c0@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_10065670(undefined4 *);
struct C_FUN_100655c0 { void * FUN_100655c0(uint param_1); };
void * C_FUN_100655c0::FUN_100655c0(uint param_1)
{
  thunk_FUN_10065670((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10065630 {
// MATCH: jgld.dll 0x10065630 ?FUN_10065630@f_10065630@@YIPAIPAI@Z
extern void *PTR_LAB_1011d7dc;
undefined4 * __fastcall FUN_10065630(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011d7dc);
  return param_1;
}
}

namespace f_10065670 {
// MATCH: jgld.dll 0x10065670 ?FUN_10065670@f_10065670@@YIXPAI@Z
extern void *PTR_LAB_1011d640;
int __fastcall thunk_FUN_10066d20(undefined4 *);
void __fastcall FUN_10065670(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011d640);
  thunk_FUN_10066d20(param_1);
  return;
}
}

namespace f_100656c0 {
// MATCH: jgld.dll 0x100656c0 ?FUN_100656c0@f_100656c0@@YIXH@Z
struct VT_1 { virtual int f0(); virtual int f1(); virtual int f2(); };
void __fastcall FUN_100656c0(int param_1)
{
  ChangeDisplaySettingsA((DEVMODEA *)(param_1 + 4),0);
  if (*(int *)(param_1 + 0x144) != 0) {
    DestroyWindow(*(HWND *)(param_1 + 0x144));
    *(undefined4 *)(param_1 + 0x144) = 0;
  }
  if (*(int *)(param_1 + 0x148) != 0) {
    ((VT_1 *)(*(int **)(param_1 + 0x148)))->f2();
    *(undefined4 *)(param_1 + 0x148) = 0;
  }
  UnregisterClassA("JackalClass",*(HINSTANCE *)(param_1 + 0x140));
  *(undefined4 *)(param_1 + 0x140) = 0;
  return;
}
}

namespace f_10065bf0 {
// MATCH: jgld.dll 0x10065bf0 ?FUN_10065bf0@f_10065bf0@@YIIH@Z
undefined4 __fastcall FUN_10065bf0(int param_1)
{
  return *(undefined4 *)(param_1 + 0x138);
}
}

namespace f_10066650 {
// MATCH: jgld.dll 0x10066650 ?FUN_10066650@C_FUN_10066650@f_10066650@@QAEXPAUtagRECT@@@Z
struct C_FUN_10066650 { void FUN_10066650(LPRECT param_1); };
void C_FUN_10066650::FUN_10066650(LPRECT param_1)
{
  GetWindowRect(*(HWND *)((int)this + 0x144),param_1);
  return;
}
}

namespace f_100666b0 {
// MATCH: jgld.dll 0x100666b0 ?FUN_100666b0@C_FUN_100666b0@f_100666b0@@QAEXPAI0@Z
struct C_FUN_100666b0 { void FUN_100666b0(undefined4 *param_1, undefined4 *param_2); };
void C_FUN_100666b0::FUN_100666b0(undefined4 *param_1, undefined4 *param_2)
{
  *param_1 = *(undefined4 *)((int)this + 300);
  *param_2 = *(undefined4 *)((int)this + 0x130);
  return;
}
}

namespace f_10066710 {
// MATCH: jgld.dll 0x10066710 ?FUN_10066710@C_FUN_10066710@f_10066710@@QAEXPAUtagRECT@@@Z
struct C_FUN_10066710 { void FUN_10066710(RECT *param_1); };
void C_FUN_10066710::FUN_10066710(RECT *param_1)
{
  InvalidateRect(*(HWND *)((int)this + 0x144),param_1,0);
  return;
}
}

namespace f_10066810 {
// MATCH: jgld.dll 0x10066810 ?FUN_10066810@C_FUN_10066810@f_10066810@@QAEXHHHH@Z
struct C_FUN_10066810 { void FUN_10066810(int param_1, int param_2, int param_3, int param_4); };
void C_FUN_10066810::FUN_10066810(int param_1, int param_2, int param_3, int param_4)
{
  SetWindowPos(*(HWND *)((int)this + 0x144),(HWND)0x0,param_1,param_2,param_3,param_4,0);
  return;
}
}

namespace f_100669e0 {
// MATCH: jgld.dll 0x100669e0 ?FUN_100669e0@f_100669e0@@YIXPAI@Z
extern void *PTR_LAB_1011da58;
int __fastcall thunk_FUN_10068000(int);
void __fastcall FUN_100669e0(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011da58);
  thunk_FUN_10068000((int)param_1);
  return;
}
}

namespace f_10066a30 {
// MATCH: jgld.dll 0x10066a30 ?FUN_10066a30@f_10066a30@@YIXPAI@Z
extern void *PTR_LAB_1011da5c;
int __fastcall thunk_FUN_10068100(int);
void __fastcall FUN_10066a30(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011da5c);
  thunk_FUN_10068100((int)param_1);
  return;
}
}

namespace f_10066a80 {
// MATCH: jgld.dll 0x10066a80 ?FUN_10066a80@f_10066a80@@YIIH@Z
undefined4 __fastcall FUN_10066a80(int param_1)
{
  return *(undefined4 *)(param_1 + 0xc);
}
}

namespace f_10066ac0 {
// MATCH: jgld.dll 0x10066ac0 ?FUN_10066ac0@f_10066ac0@@YIXH@Z
void __fastcall FUN_10066ac0(int param_1)
{
  *(undefined4 *)(param_1 + 0xc) = 0;
  return;
}
}

namespace f_10066b00 {
// MATCH: jgld.dll 0x10066b00 ?FUN_10066b00@f_10066b00@@YIXPAI@Z
extern void *PTR_LAB_1011da60;
int __fastcall thunk_FUN_10068360(int);
void __fastcall FUN_10066b00(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011da60);
  thunk_FUN_10068360((int)param_1);
  return;
}
}

namespace f_10066b50 {
// MATCH: jgld.dll 0x10066b50 ?FUN_10066b50@f_10066b50@@YIIH@Z
undefined4 __fastcall FUN_10066b50(int param_1)
{
  return *(undefined4 *)(param_1 + 0xc);
}
}

namespace f_10066b90 {
// MATCH: jgld.dll 0x10066b90 ?FUN_10066b90@f_10066b90@@YIXH@Z
void __fastcall FUN_10066b90(int param_1)
{
  *(undefined4 *)(param_1 + 0xc) = 0;
  return;
}
}

namespace f_10066bd0 {
// MATCH: jgld.dll 0x10066bd0 ?FUN_10066bd0@C_FUN_10066bd0@f_10066bd0@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_100669e0(undefined4 *);
struct C_FUN_10066bd0 { void * FUN_10066bd0(uint param_1); };
void * C_FUN_10066bd0::FUN_10066bd0(uint param_1)
{
  thunk_FUN_100669e0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10066c40 {
// MATCH: jgld.dll 0x10066c40 ?FUN_10066c40@C_FUN_10066c40@f_10066c40@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_10066a30(undefined4 *);
struct C_FUN_10066c40 { void * FUN_10066c40(uint param_1); };
void * C_FUN_10066c40::FUN_10066c40(uint param_1)
{
  thunk_FUN_10066a30((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10066cb0 {
// MATCH: jgld.dll 0x10066cb0 ?FUN_10066cb0@C_FUN_10066cb0@f_10066cb0@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_10066b00(undefined4 *);
struct C_FUN_10066cb0 { void * FUN_10066cb0(uint param_1); };
void * C_FUN_10066cb0::FUN_10066cb0(uint param_1)
{
  thunk_FUN_10066b00((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10066d20 {
// MATCH: jgld.dll 0x10066d20 ?FUN_10066d20@f_10066d20@@YIXPAI@Z
extern void *PTR_LAB_1011d7dc;
void __fastcall FUN_10066d20(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011d7dc);
  return;
}
}

namespace f_100674f0 {
// MATCH: jgld.dll 0x100674f0 ?FUN_100674f0@C_FUN_100674f0@f_100674f0@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_10066d20(undefined4 *);
struct C_FUN_100674f0 { void * FUN_100674f0(uint param_1); };
void * C_FUN_100674f0::FUN_100674f0(uint param_1)
{
  thunk_FUN_10066d20((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}
