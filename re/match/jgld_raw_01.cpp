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

namespace f_10002650 {
// MATCH: jgld.dll 0x10002650 ?FUN_10002650@f_10002650@@YIPAIPAI@Z
extern void *PTR_LAB_1011d01c;
undefined4 * __fastcall FUN_10002650(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011d01c);
  param_1[1] = 0;
  param_1[2] = 0xffffffff;
  param_1[3] = 0;
  return param_1;
}
}

namespace f_100026c0 {
// MATCH: jgld.dll 0x100026c0 ?FUN_100026c0@C_FUN_100026c0@f_100026c0@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_10002820(undefined4 *);
struct C_FUN_100026c0 { void * FUN_100026c0(uint param_1); };
void * C_FUN_100026c0::FUN_100026c0(uint param_1)
{
  thunk_FUN_10002820((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10002730 {
// MATCH: jgld.dll 0x10002730 ?FUN_10002730@C_FUN_10002730@f_10002730@@QAEPAXPBDH@Z
extern void *PTR_LAB_1011d01c;
undefined4 * __fastcall thunk_FUN_10002650(undefined4 *);
int __fastcall thunk_FUN_10002820(undefined4 *);
struct T_thunk_FUN_10002a20 { int thunk_FUN_10002a20(LPCSTR, int); };
struct C_FUN_10002730 { void * FUN_10002730(LPCSTR param_1, int param_2); };
void * C_FUN_10002730::FUN_10002730(LPCSTR param_1, int param_2)
{
    undefined4 local_1c[5];
  *(undefined ***)this = (unsigned char ** )(&PTR_LAB_1011d01c);
  thunk_FUN_10002650(local_1c);
  thunk_FUN_10002820(local_1c);
  ((T_thunk_FUN_10002a20 *)(((void *)this)))->thunk_FUN_10002a20(param_1, param_2);
  return this;
}
}

namespace f_100027b0 {
// MATCH: jgld.dll 0x100027b0 ?FUN_100027b0@C_FUN_100027b0@f_100027b0@@QAEPAXPBD@Z
extern void *PTR_LAB_1011d01c;
undefined4 * __fastcall thunk_FUN_10002650(undefined4 *);
int __fastcall thunk_FUN_10002820(undefined4 *);
struct T_thunk_FUN_10002a20 { int thunk_FUN_10002a20(LPCSTR, int); };
struct C_FUN_100027b0 { void * FUN_100027b0(LPCSTR param_1); };
void * C_FUN_100027b0::FUN_100027b0(LPCSTR param_1)
{
    undefined4 local_1c[5];
  *(undefined ***)this = (unsigned char ** )(&PTR_LAB_1011d01c);
  thunk_FUN_10002650(local_1c);
  thunk_FUN_10002820(local_1c);
  ((T_thunk_FUN_10002a20 *)(((void *)this)))->thunk_FUN_10002a20(param_1, 0);
  return this;
}
}

namespace f_10002820 {
// MATCH: jgld.dll 0x10002820 ?FUN_10002820@f_10002820@@YIXPAI@Z
extern void *PTR_LAB_1011d01c;
int __fastcall thunk_FUN_10002e10(int);
void __fastcall FUN_10002820(undefined4 *param_1)
{
  *param_1 = (unsigned int)(&PTR_LAB_1011d01c);
  thunk_FUN_10002e10((int)param_1);
  return;
}
}

namespace f_10002e10 {
// MATCH: jgld.dll 0x10002e10 ?FUN_10002e10@f_10002e10@@YIXH@Z
void __fastcall FUN_10002e10(int param_1)
{
  if (*(int *)(param_1 + 4) != 0) {
    UnmapViewOfFile(*(LPCVOID *)(param_1 + 4));
    *(undefined4 *)(param_1 + 4) = 0;
  }
  if (*(int *)(param_1 + 0xc) != 0) {
    CloseHandle(*(HANDLE *)(param_1 + 0xc));
    *(undefined4 *)(param_1 + 0xc) = 0;
  }
  if (*(int *)(param_1 + 8) != -1) {
    CloseHandle(*(HANDLE *)(param_1 + 8));
    *(undefined4 *)(param_1 + 8) = 0xffffffff;
  }
  return;
}
}

namespace f_10002ef0 {
// MATCH: jgld.dll 0x10002ef0 ?FUN_10002ef0@C_FUN_10002ef0@f_10002ef0@@QAEXI@Z
int __fastcall thunk_FUN_10002e10(int);
struct C_FUN_10002ef0 { void FUN_10002ef0(uint param_1); };
void C_FUN_10002ef0::FUN_10002ef0(uint param_1)
{
    int local_c;
  if (param_1 < *(uint *)((int)this + 4)) {
    thunk_FUN_10002e10((int)this);
  }
  else {
    local_c = param_1 - *(int *)((int)this + 4);
    if (*(int *)((int)this + 4) != 0) {
      UnmapViewOfFile(*(LPCVOID *)((int)this + 4));
      *(undefined4 *)((int)((void *)this) + 4) = 0;
    }
    if (*(int *)((int)((void *)this) + 0xc) != 0) {
      CloseHandle(*(HANDLE *)((int)((void *)this) + 0xc));
      *(undefined4 *)((int)((void *)this) + 0xc) = 0;
    }
    if ((SetFilePointer(*(HANDLE *)((int)((void *)this) + 8),local_c,(PLONG)0x0,0)) == -1) {
      thunk_FUN_10002e10((int)((void *)this));
    }
    else {
      SetEndOfFile(*(HANDLE *)((int)((void *)this) + 8));
      if (*(int *)((int)((void *)this) + 8) != 0) {
        CloseHandle(*(HANDLE *)((int)((void *)this) + 8));
        *(undefined4 *)((int)((void *)this) + 8) = 0;
      }
    }
  }
  return;
}
}

namespace f_10003050 {
// MATCH: jgld.dll 0x10003050 ?FUN_10003050@f_10003050@@YIPAXPAX@Z
struct T_thunk_FUN_100036e0 { int thunk_FUN_100036e0(undefined4, undefined4, undefined4); };
void * __fastcall FUN_10003050(void *param_1)
{
  ((T_thunk_FUN_100036e0 *)(param_1))->thunk_FUN_100036e0(0, 0, 0);
  *(undefined4 *)((int)param_1 + 0xc) = 0x3f800000;
  return param_1;
}
}

namespace f_100030b0 {
// MATCH: jgld.dll 0x100030b0 ?FUN_100030b0@C_FUN_100030b0@f_100030b0@@QAEPAIPAI@Z
struct S_4 { int a[4]; };
struct C_FUN_100030b0 { undefined4 * FUN_100030b0(undefined4 *param_1); };
undefined4 * C_FUN_100030b0::FUN_100030b0(undefined4 *param_1)
{
  if (param_1 != (undefined4 *)0x0) {
    *(S_4 *)(this) = *(S_4 *)(param_1);
  }
  return (unsigned int *)(this);
}
}

namespace f_10003110 {
// MATCH: jgld.dll 0x10003110 ?FUN_10003110@C_FUN_10003110@f_10003110@@QAEPAXMPAI@Z
struct T_thunk_FUN_10004230 { int thunk_FUN_10004230(float, undefined4 *); };
struct C_FUN_10003110 { void * FUN_10003110(float param_1, undefined4 *param_2); };
void * C_FUN_10003110::FUN_10003110(float param_1, undefined4 *param_2)
{
  ((T_thunk_FUN_10004230 *)(this))->thunk_FUN_10004230(param_1, param_2);
  return this;
}
}

namespace f_10003170 {
// MATCH: jgld.dll 0x10003170 ?FUN_10003170@f_10003170@@YIXPAX@Z
struct T_thunk_FUN_100036e0 { int thunk_FUN_100036e0(undefined4, undefined4, undefined4); };
void __fastcall FUN_10003170(void *param_1)
{
  ((T_thunk_FUN_100036e0 *)(param_1))->thunk_FUN_100036e0(0, 0, 0);
  *(undefined4 *)((int)param_1 + 0xc) = 0x3f800000;
  return;
}
}

namespace f_100031d0 {
// MATCH: jgld.dll 0x100031d0 ?FUN_100031d0@C_FUN_100031d0@f_100031d0@@QAEPAXPAMPAI@Z
extern void *PTR_LAB_1011d024;
struct T_thunk_FUN_10004690 { int thunk_FUN_10004690(float *, undefined4 *); };
struct C_FUN_100031d0 { void * FUN_100031d0(float *param_1, undefined4 *param_2); };
void * C_FUN_100031d0::FUN_100031d0(float *param_1, undefined4 *param_2)
{
  *(undefined ***)this = (unsigned char ** )(&PTR_LAB_1011d024);
  ((T_thunk_FUN_10004690 *)(this))->thunk_FUN_10004690(param_1, param_2);
  return this;
}
}

namespace f_10003230 {
// MATCH: jgld.dll 0x10003230 ?FUN_10003230@C_FUN_10003230@f_10003230@@QAEPAXPAM@Z
extern void *PTR_LAB_1011d024;
struct T_thunk_FUN_10004750 { int thunk_FUN_10004750(float *); };
struct C_FUN_10003230 { void * FUN_10003230(float *param_1); };
void * C_FUN_10003230::FUN_10003230(float *param_1)
{
  *(undefined ***)this = (unsigned char ** )(&PTR_LAB_1011d024);
  ((T_thunk_FUN_10004750 *)(this))->thunk_FUN_10004750(param_1);
  return this;
}
}

namespace f_10003290 {
// MATCH: jgld.dll 0x10003290 ?FUN_10003290@C_FUN_10003290@f_10003290@@QAEPAXPAI@Z
extern void *PTR_LAB_1011d024;
struct T_thunk_FUN_100046f0 { int thunk_FUN_100046f0(undefined4 *); };
struct C_FUN_10003290 { void * FUN_10003290(undefined4 *param_1); };
void * C_FUN_10003290::FUN_10003290(undefined4 *param_1)
{
  *(undefined ***)this = (unsigned char ** )(&PTR_LAB_1011d024);
  ((T_thunk_FUN_100046f0 *)(this))->thunk_FUN_100046f0(param_1);
  return this;
}
}

namespace f_10003340 {
// MATCH: jgld.dll 0x10003340 ?FUN_10003340@f_10003340@@YIXH@Z
int __fastcall thunk_FUN_100045f0(int);
void __fastcall FUN_10003340(int param_1)
{
  thunk_FUN_100045f0(param_1);
  return;
}
}

namespace f_10003390 {
// MATCH: jgld.dll 0x10003390 ?FUN_10003390@C_FUN_10003390@f_10003390@@QAEPAXPAI0@Z
extern void *PTR_LAB_1011d028;
int __fastcall thunk_FUN_10003050(void *);
struct T_thunk_FUN_100055c0 { int thunk_FUN_100055c0(undefined4 *, undefined4 *); };
struct C_FUN_10003390 { void * FUN_10003390(undefined4 *param_1, undefined4 *param_2); };
void * C_FUN_10003390::FUN_10003390(undefined4 *param_1, undefined4 *param_2)
{
  thunk_FUN_10003050((void *)((int)this + 4));
  *((undefined4 *)this) = (unsigned int)(&PTR_LAB_1011d028);
  ((T_thunk_FUN_100055c0 *)(((undefined4 *)this)))->thunk_FUN_100055c0(param_1, param_2);
  return this;
}
}

namespace f_10003400 {
// MATCH: jgld.dll 0x10003400 ?FUN_10003400@C_FUN_10003400@f_10003400@@QAEPAXPAI@Z
extern void *PTR_LAB_1011d028;
int __fastcall thunk_FUN_10003050(void *);
struct T_thunk_FUN_10005530 { int thunk_FUN_10005530(undefined4 *); };
struct C_FUN_10003400 { void * FUN_10003400(undefined4 *param_1); };
void * C_FUN_10003400::FUN_10003400(undefined4 *param_1)
{
  thunk_FUN_10003050((void *)((int)this + 4));
  *((undefined4 *)this) = (unsigned int)(&PTR_LAB_1011d028);
  ((T_thunk_FUN_10005530 *)(((undefined4 *)this)))->thunk_FUN_10005530(param_1);
  return this;
}
}

namespace f_10003470 {
// MATCH: jgld.dll 0x10003470 ?FUN_10003470@C_FUN_10003470@f_10003470@@QAEPAXPAI@Z
extern void *PTR_LAB_1011d028;
int __fastcall thunk_FUN_10003050(void *);
struct T_thunk_FUN_100054b0 { int thunk_FUN_100054b0(undefined4 *); };
struct C_FUN_10003470 { void * FUN_10003470(undefined4 *param_1); };
void * C_FUN_10003470::FUN_10003470(undefined4 *param_1)
{
  thunk_FUN_10003050((void *)((int)this + 4));
  *((undefined4 *)this) = (unsigned int)(&PTR_LAB_1011d028);
  ((T_thunk_FUN_100054b0 *)(((undefined4 *)this)))->thunk_FUN_100054b0(param_1);
  return this;
}
}

namespace f_100035b0 {
// MATCH: jgld.dll 0x100035b0 ?FUN_100035b0@f_100035b0@@YIXH@Z
int __fastcall thunk_FUN_10005650(int);
void __fastcall FUN_100035b0(int param_1)
{
  thunk_FUN_10005650(param_1);
  return;
}
}

namespace f_10003600 {
// MATCH: jgld.dll 0x10003600 ?FUN_10003600@C_FUN_10003600@f_10003600@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_100032f0(undefined4 *);
struct C_FUN_10003600 { void * FUN_10003600(uint param_1); };
void * C_FUN_10003600::FUN_10003600(uint param_1)
{
  thunk_FUN_100032f0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_10003670 {
// MATCH: jgld.dll 0x10003670 ?FUN_10003670@C_FUN_10003670@f_10003670@@QAEPAXI@Z
int __cdecl operator_delete(void *);
int __fastcall thunk_FUN_100034e0(undefined4 *);
struct C_FUN_10003670 { void * FUN_10003670(uint param_1); };
void * C_FUN_10003670::FUN_10003670(uint param_1)
{
  thunk_FUN_100034e0((unsigned int *)(this));
  if ((param_1 & 1) != 0) {
    operator_delete(((void *)this));
  }
  return this;
}
}

namespace f_100036e0 {
// MATCH: jgld.dll 0x100036e0 ?FUN_100036e0@C_FUN_100036e0@f_100036e0@@QAEXIII@Z
struct C_FUN_100036e0 { void FUN_100036e0(undefined4 param_1, undefined4 param_2, undefined4 param_3); };
void C_FUN_100036e0::FUN_100036e0(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  *(undefined4 *)this = param_1;
  *(undefined4 *)((int)this + 4) = param_2;
  *(undefined4 *)((int)this + 8) = param_3;
  return;
}
}

namespace f_10003730 {
// MATCH: jgld.dll 0x10003730 ?FUN_10003730@f_10003730@@YIXPAI@Z
void __fastcall FUN_10003730(undefined4 *param_1)
{
  *param_1 = 0;
  param_1[1] = 0;
  param_1[2] = 0;
  return;
}
}

namespace f_10003790 {
// MATCH: jgld.dll 0x10003790 ?FUN_10003790@C_FUN_10003790@f_10003790@@QAEPAMPAMH@Z
struct S_3 { int a[3]; };
struct C_FUN_10003790 { float * FUN_10003790(float *param_1, int param_2); };
float * C_FUN_10003790::FUN_10003790(float *param_1, int param_2)
{
    float local_14[3];
    int local_18;
  for (local_18 = 0; local_18 < 3; local_18 = local_18 + 1) {
    local_14[local_18] = *(float *)((int)this + local_18 * 4) + *(float *)(param_2 + local_18 * 4);
  }
  *(S_3 *)(param_1) = *(S_3 *)(local_14);
  return param_1;
}
}

namespace f_10003820 {
// MATCH: jgld.dll 0x10003820 ?FUN_10003820@C_FUN_10003820@f_10003820@@QAEPAMPAMH@Z
struct S_3 { int a[3]; };
struct C_FUN_10003820 { float * FUN_10003820(float *param_1, int param_2); };
float * C_FUN_10003820::FUN_10003820(float *param_1, int param_2)
{
    float local_14[3];
    int local_18;
  for (local_18 = 0; local_18 < 3; local_18 = local_18 + 1) {
    local_14[local_18] = *(float *)((int)this + local_18 * 4) - *(float *)(param_2 + local_18 * 4);
  }
  *(S_3 *)(param_1) = *(S_3 *)(local_14);
  return param_1;
}
}

namespace f_100038b0 {
// MATCH: jgld.dll 0x100038b0 ?FUN_100038b0@C_FUN_100038b0@f_100038b0@@QAEHH@Z
struct C_FUN_100038b0 { int FUN_100038b0(int param_1); };
int C_FUN_100038b0::FUN_100038b0(int param_1)
{
    int local_c;
  for (local_c = 0; local_c < 3; local_c = local_c + 1) {
    *(float *)((int)this + local_c * 4) = *(float *)((int)this + local_c * 4) + *(float *)(param_1 + local_c * 4);
  }
  return (int)this;
}
}

namespace f_10003930 {
// MATCH: jgld.dll 0x10003930 ?FUN_10003930@C_FUN_10003930@f_10003930@@QAEHH@Z
struct C_FUN_10003930 { int FUN_10003930(int param_1); };
int C_FUN_10003930::FUN_10003930(int param_1)
{
    int local_c;
  for (local_c = 0; local_c < 3; local_c = local_c + 1) {
    *(float *)((int)this + local_c * 4) = *(float *)((int)this + local_c * 4) - *(float *)(param_1 + local_c * 4);
  }
  return (int)this;
}
}

namespace f_10003df0 {
// MATCH: jgld.dll 0x10003df0 ?FUN_10003df0@f_10003df0@@YIXPAM@Z
void __fastcall FUN_10003df0(float *param_1)
{
  *param_1 = -*param_1;
  param_1[1] = -param_1[1];
  param_1[2] = -param_1[2];
  return;
}
}

namespace f_10003f10 {
// MATCH: jgld.dll 0x10003f10 ?FUN_10003f10@C_FUN_10003f10@f_10003f10@@QAEXM@Z
struct C_FUN_10003f10 { void FUN_10003f10(float param_1); };
void C_FUN_10003f10::FUN_10003f10(float param_1)
{
    int local_c;
  for (local_c = 0; local_c < 3; local_c = local_c + 1) {
    *(float *)((int)this + local_c * 4) = param_1 * *(float *)((int)this + local_c * 4);
  }
  return;
}
}

namespace f_10003f80 {
// MATCH: jgld.dll 0x10003f80 ?FUN_10003f80@C_FUN_10003f80@f_10003f80@@QAEPAMPAMH@Z
struct S_3 { int a[3]; };
extern float _DAT_1011d04c;
struct C_FUN_10003f80 { /* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ float * FUN_10003f80(float *param_1, int param_2); };
/* WARNING: Globals starting with '_' overlap smaller symbols at the same address */ float * C_FUN_10003f80::FUN_10003f80(float *param_1, int param_2)
{
    float local_14[3];
    int local_18;
  for (local_18 = 0; local_18 < 3; local_18 = local_18 + 1) {
    local_14[local_18] = (*(float *)(param_2 + local_18 * 4) + *(float *)((int)this + local_18 * 4)) * _DAT_1011d04c * local_14[local_18];
  }
  *(S_3 *)(param_1) = *(S_3 *)(local_14);
  return param_1;
}
}

namespace f_10004020 {
// MATCH: jgld.dll 0x10004020 ?FUN_10004020@C_FUN_10004020@f_10004020@@QAEXMMM@Z
struct C_FUN_10004020 { void FUN_10004020(float param_1, float param_2, float param_3); };
void C_FUN_10004020::FUN_10004020(float param_1, float param_2, float param_3)
{
  *(float *)this = param_1 + *(float *)this;
  *(float *)((int)this + 4) = param_2 + *(float *)((int)this + 4);
  *(float *)((int)this + 8) = param_3 + *(float *)((int)this + 8);
  return;
}
}

namespace f_10004090 {
// MATCH: jgld.dll 0x10004090 ?FUN_10004090@C_FUN_10004090@f_10004090@@QAEXPAM@Z
struct C_FUN_10004090 { void FUN_10004090(float *param_1); };
void C_FUN_10004090::FUN_10004090(float *param_1)
{
  *(float *)this = *(float *)this + *param_1;
  *(float *)((int)this + 4) = *(float *)((int)this + 4) + param_1[1];
  *(float *)((int)this + 8) = *(float *)((int)this + 8) + param_1[2];
  return;
}
}

namespace f_100045f0 {
// MATCH: jgld.dll 0x100045f0 ?FUN_100045f0@f_100045f0@@YIXH@Z
void __fastcall FUN_100045f0(int param_1)
{
    int local_c;
  for (local_c = 0; local_c < 0x10; local_c = local_c + 1) {
    *(undefined4 *)(param_1 + 4 + local_c * 4) = 0;
  }
  *(undefined4 *)(param_1 + 4) = 0x3f800000;
  *(undefined4 *)(param_1 + 0x18) = 0x3f800000;
  *(undefined4 *)(param_1 + 0x2c) = 0x3f800000;
  *(undefined4 *)(param_1 + 0x40) = 0x3f800000;
  return;
}
}

namespace f_10004690 {
// MATCH: jgld.dll 0x10004690 ?FUN_10004690@C_FUN_10004690@f_10004690@@QAEXPAMPAI@Z
struct T_thunk_FUN_100046f0 { int thunk_FUN_100046f0(undefined4 *); };
struct T_thunk_FUN_10004750 { int thunk_FUN_10004750(float *); };
struct C_FUN_10004690 { void FUN_10004690(float *param_1, undefined4 *param_2); };
void C_FUN_10004690::FUN_10004690(float *param_1, undefined4 *param_2)
{
  ((T_thunk_FUN_10004750 *)(this))->thunk_FUN_10004750(param_1);
  ((T_thunk_FUN_100046f0 *)(((void *)this)))->thunk_FUN_100046f0(param_2);
  return;
}
}

namespace f_100046f0 {
// MATCH: jgld.dll 0x100046f0 ?FUN_100046f0@C_FUN_100046f0@f_100046f0@@QAEXPAI@Z
struct C_FUN_100046f0 { void FUN_100046f0(undefined4 *param_1); };
void C_FUN_100046f0::FUN_100046f0(undefined4 *param_1)
{
  *(undefined4 *)((int)this + 0x34) = *param_1;
  *(undefined4 *)((int)this + 0x38) = param_1[1];
  *(undefined4 *)((int)this + 0x3c) = param_1[2];
  return;
}
}

namespace f_100049e0 {
// MATCH: jgld.dll 0x100049e0 ?FUN_100049e0@C_FUN_100049e0@f_100049e0@@QAEHM@Z
struct C_FUN_100049e0 { int FUN_100049e0(float param_1); };
int C_FUN_100049e0::FUN_100049e0(float param_1)
{
    int local_c;
  for (local_c = 0; local_c < 0xf; local_c = local_c + 5) {
    *(float *)((int)this + local_c * 4 + 4) = param_1 * *(float *)((int)this + local_c * 4 + 4);
  }
  return (int)this;
}
}

namespace f_10004cc0 {
// MATCH: jgld.dll 0x10004cc0 ?FUN_10004cc0@C_FUN_10004cc0@f_10004cc0@@QAEHH@Z
struct C_FUN_10004cc0 { int FUN_10004cc0(int param_1); };
int C_FUN_10004cc0::FUN_10004cc0(int param_1)
{
    int local_c;
  for (local_c = 0; local_c < 0x10; local_c = local_c + 1) {
    *(float *)((int)this + local_c * 4 + 4) = *(float *)((int)this + local_c * 4 + 4) + *(float *)(param_1 + 4 + local_c * 4);
  }
  return (int)this;
}
}

namespace f_10004d40 {
// MATCH: jgld.dll 0x10004d40 ?FUN_10004d40@C_FUN_10004d40@f_10004d40@@QAEHH@Z
struct C_FUN_10004d40 { int FUN_10004d40(int param_1); };
int C_FUN_10004d40::FUN_10004d40(int param_1)
{
    int local_c;
  for (local_c = 0; local_c < 0x10; local_c = local_c + 1) {
    *(float *)((int)this + local_c * 4 + 4) = *(float *)((int)this + local_c * 4 + 4) - *(float *)(param_1 + 4 + local_c * 4);
  }
  return (int)this;
}
}

namespace f_100052b0 {
// MATCH: jgld.dll 0x100052b0 ?FUN_100052b0@f_100052b0@@YIPAXH@Z
int __fastcall thunk_FUN_100045f0(int);
void * __fastcall FUN_100052b0(int param_1)
{
  thunk_FUN_100045f0(param_1);
  return (void *)(param_1);
}
}

namespace f_10005300 {
// MATCH: jgld.dll 0x10005300 ?FUN_10005300@f_10005300@@YIII@Z
undefined4 __fastcall FUN_10005300(undefined4 param_1)
{
  return param_1;
}
}
