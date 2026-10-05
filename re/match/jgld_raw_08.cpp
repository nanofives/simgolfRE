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

namespace f_1000ae70 {
// MATCH: jgld.dll 0x1000ae70 ?FUN_1000ae70@C_FUN_1000ae70@f_1000ae70@@QAEIHHHH@Z
struct C_FUN_1000ae70 { undefined4 FUN_1000ae70(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000ae70::FUN_1000ae70(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10017fb0 {
// MATCH: jgld.dll 0x10017fb0 ?FUN_10017fb0@C_FUN_10017fb0@f_10017fb0@@QAEXH@Z
struct C_FUN_10017fb0 { void FUN_10017fb0(int param_1); };
void C_FUN_10017fb0::FUN_10017fb0(int param_1)
{
  return;
}
}

namespace f_1001b590 {
// MATCH: jgld.dll 0x1001b590 ?FUN_1001b590@C_FUN_1001b590@f_1001b590@@QAEIHHHH@Z
struct C_FUN_1001b590 { undefined4 FUN_1001b590(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1001b590::FUN_1001b590(int param_1, int param_2, int param_3, int param_4)
{
  return 0;
}
}

namespace f_1001b5d0 {
// MATCH: jgld.dll 0x1001b5d0 ?FUN_1001b5d0@C_FUN_1001b5d0@f_1001b5d0@@QAEIHHHH@Z
struct C_FUN_1001b5d0 { undefined4 FUN_1001b5d0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1001b5d0::FUN_1001b5d0(int param_1, int param_2, int param_3, int param_4)
{
  return 0;
}
}

namespace f_1001b610 {
// MATCH: jgld.dll 0x1001b610 ?FUN_1001b610@C_FUN_1001b610@f_1001b610@@QAEIHHH@Z
struct C_FUN_1001b610 { undefined4 FUN_1001b610(int param_1, int param_2, int param_3); };
undefined4 C_FUN_1001b610::FUN_1001b610(int param_1, int param_2, int param_3)
{
  return 0;
}
}

namespace f_1001b650 {
// MATCH: jgld.dll 0x1001b650 ?FUN_1001b650@C_FUN_1001b650@f_1001b650@@QAEIHHH@Z
struct C_FUN_1001b650 { undefined4 FUN_1001b650(int param_1, int param_2, int param_3); };
undefined4 C_FUN_1001b650::FUN_1001b650(int param_1, int param_2, int param_3)
{
  return 0;
}
}

namespace f_10066d60 {
// MATCH: jgld.dll 0x10066d60 ?FUN_10066d60@C_FUN_10066d60@f_10066d60@@QAEIXZ
struct C_FUN_10066d60 { undefined4 FUN_10066d60(); };
undefined4 C_FUN_10066d60::FUN_10066d60()
{
  return 0;
}
}

namespace f_10066d90 {
// MATCH: jgld.dll 0x10066d90 ?FUN_10066d90@C_FUN_10066d90@f_10066d90@@QAEIXZ
struct C_FUN_10066d90 { undefined4 FUN_10066d90(); };
undefined4 C_FUN_10066d90::FUN_10066d90()
{
  return 0;
}
}

namespace f_10066dc0 {
// MATCH: jgld.dll 0x10066dc0 ?FUN_10066dc0@C_FUN_10066dc0@f_10066dc0@@QAEXHHHH@Z
struct C_FUN_10066dc0 { void FUN_10066dc0(int param_1, int param_2, int param_3, int param_4); };
void C_FUN_10066dc0::FUN_10066dc0(int param_1, int param_2, int param_3, int param_4)
{
  return;
}
}

namespace f_10066df0 {
// MATCH: jgld.dll 0x10066df0 ?FUN_10066df0@C_FUN_10066df0@f_10066df0@@QAEIH@Z
struct C_FUN_10066df0 { undefined4 FUN_10066df0(int param_1); };
undefined4 C_FUN_10066df0::FUN_10066df0(int param_1)
{
  return 0x18;
}
}

namespace f_10066e30 {
// MATCH: jgld.dll 0x10066e30 ?FUN_10066e30@C_FUN_10066e30@f_10066e30@@QAEIH@Z
struct C_FUN_10066e30 { undefined4 FUN_10066e30(int param_1); };
undefined4 C_FUN_10066e30::FUN_10066e30(int param_1)
{
  return 0x18;
}
}

namespace f_10066e70 {
// MATCH: jgld.dll 0x10066e70 ?FUN_10066e70@C_FUN_10066e70@f_10066e70@@QAEIXZ
struct C_FUN_10066e70 { undefined4 FUN_10066e70(); };
undefined4 C_FUN_10066e70::FUN_10066e70()
{
  return 0;
}
}

namespace f_10066ea0 {
// MATCH: jgld.dll 0x10066ea0 ?FUN_10066ea0@C_FUN_10066ea0@f_10066ea0@@QAEXXZ
struct C_FUN_10066ea0 { void FUN_10066ea0(); };
void C_FUN_10066ea0::FUN_10066ea0()
{
  return;
}
}

namespace f_10066ed0 {
// MATCH: jgld.dll 0x10066ed0 ?FUN_10066ed0@C_FUN_10066ed0@f_10066ed0@@QAEIH@Z
struct C_FUN_10066ed0 { undefined4 FUN_10066ed0(int param_1); };
undefined4 C_FUN_10066ed0::FUN_10066ed0(int param_1)
{
  return 0;
}
}

namespace f_10066f10 {
// MATCH: jgld.dll 0x10066f10 ?FUN_10066f10@C_FUN_10066f10@f_10066f10@@QAEXH@Z
struct C_FUN_10066f10 { void FUN_10066f10(int param_1); };
void C_FUN_10066f10::FUN_10066f10(int param_1)
{
  return;
}
}

namespace f_10066f40 {
// MATCH: jgld.dll 0x10066f40 ?FUN_10066f40@C_FUN_10066f40@f_10066f40@@QAEIXZ
struct C_FUN_10066f40 { undefined4 FUN_10066f40(); };
undefined4 C_FUN_10066f40::FUN_10066f40()
{
  return 0x18;
}
}

namespace f_10066f80 {
// MATCH: jgld.dll 0x10066f80 ?FUN_10066f80@C_FUN_10066f80@f_10066f80@@QAEIXZ
struct C_FUN_10066f80 { undefined4 FUN_10066f80(); };
undefined4 C_FUN_10066f80::FUN_10066f80()
{
  return 0x18;
}
}

namespace f_10066fc0 {
// MATCH: jgld.dll 0x10066fc0 ?FUN_10066fc0@C_FUN_10066fc0@f_10066fc0@@QAEIXZ
struct C_FUN_10066fc0 { undefined4 FUN_10066fc0(); };
undefined4 C_FUN_10066fc0::FUN_10066fc0()
{
  return 0x18;
}
}

namespace f_10067000 {
// MATCH: jgld.dll 0x10067000 ?FUN_10067000@C_FUN_10067000@f_10067000@@QAEIHH@Z
struct C_FUN_10067000 { undefined4 FUN_10067000(int param_1, int param_2); };
undefined4 C_FUN_10067000::FUN_10067000(int param_1, int param_2)
{
  return 0x18;
}
}

namespace f_10067040 {
// MATCH: jgld.dll 0x10067040 ?FUN_10067040@C_FUN_10067040@f_10067040@@QAEIH@Z
struct C_FUN_10067040 { undefined4 FUN_10067040(int param_1); };
undefined4 C_FUN_10067040::FUN_10067040(int param_1)
{
  return 0x18;
}
}

namespace f_10067080 {
// MATCH: jgld.dll 0x10067080 ?FUN_10067080@C_FUN_10067080@f_10067080@@QAEIH@Z
struct C_FUN_10067080 { undefined4 FUN_10067080(int param_1); };
undefined4 C_FUN_10067080::FUN_10067080(int param_1)
{
  return 0x18;
}
}

namespace f_100670c0 {
// MATCH: jgld.dll 0x100670c0 ?FUN_100670c0@C_FUN_100670c0@f_100670c0@@QAEIH@Z
struct C_FUN_100670c0 { undefined4 FUN_100670c0(int param_1); };
undefined4 C_FUN_100670c0::FUN_100670c0(int param_1)
{
  return 0x18;
}
}

namespace f_10067100 {
// MATCH: jgld.dll 0x10067100 ?FUN_10067100@C_FUN_10067100@f_10067100@@QAEIHHH@Z
struct C_FUN_10067100 { undefined4 FUN_10067100(int param_1, int param_2, int param_3); };
undefined4 C_FUN_10067100::FUN_10067100(int param_1, int param_2, int param_3)
{
  return 0x18;
}
}

namespace f_10067140 {
// MATCH: jgld.dll 0x10067140 ?FUN_10067140@C_FUN_10067140@f_10067140@@QAEIH@Z
struct C_FUN_10067140 { undefined4 FUN_10067140(int param_1); };
undefined4 C_FUN_10067140::FUN_10067140(int param_1)
{
  return 0x18;
}
}

namespace f_10067180 {
// MATCH: jgld.dll 0x10067180 ?FUN_10067180@C_FUN_10067180@f_10067180@@QAEIHH@Z
struct C_FUN_10067180 { undefined4 FUN_10067180(int param_1, int param_2); };
undefined4 C_FUN_10067180::FUN_10067180(int param_1, int param_2)
{
  return 0x18;
}
}

namespace f_100671c0 {
// MATCH: jgld.dll 0x100671c0 ?FUN_100671c0@C_FUN_100671c0@f_100671c0@@QAEIH@Z
struct C_FUN_100671c0 { undefined4 FUN_100671c0(int param_1); };
undefined4 C_FUN_100671c0::FUN_100671c0(int param_1)
{
  return 0x18;
}
}

namespace f_10067200 {
// MATCH: jgld.dll 0x10067200 ?FUN_10067200@C_FUN_10067200@f_10067200@@QAEIH@Z
struct C_FUN_10067200 { undefined4 FUN_10067200(int param_1); };
undefined4 C_FUN_10067200::FUN_10067200(int param_1)
{
  return 0x18;
}
}

namespace f_10067240 {
// MATCH: jgld.dll 0x10067240 ?FUN_10067240@C_FUN_10067240@f_10067240@@QAEIH@Z
struct C_FUN_10067240 { undefined4 FUN_10067240(int param_1); };
undefined4 C_FUN_10067240::FUN_10067240(int param_1)
{
  return 0x18;
}
}

namespace f_10067280 {
// MATCH: jgld.dll 0x10067280 ?FUN_10067280@C_FUN_10067280@f_10067280@@QAEIH@Z
struct C_FUN_10067280 { undefined4 FUN_10067280(int param_1); };
undefined4 C_FUN_10067280::FUN_10067280(int param_1)
{
  return 0x18;
}
}

namespace f_100672c0 {
// MATCH: jgld.dll 0x100672c0 ?FUN_100672c0@C_FUN_100672c0@f_100672c0@@QAEIHHHHHH@Z
struct C_FUN_100672c0 { undefined4 FUN_100672c0(int param_1, int param_2, int param_3, int param_4, int param_5, int param_6); };
undefined4 C_FUN_100672c0::FUN_100672c0(int param_1, int param_2, int param_3, int param_4, int param_5, int param_6)
{
  return 0x18;
}
}

namespace f_10067300 {
// MATCH: jgld.dll 0x10067300 ?FUN_10067300@C_FUN_10067300@f_10067300@@QAEIHHHH@Z
struct C_FUN_10067300 { undefined4 FUN_10067300(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10067300::FUN_10067300(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10067340 {
// MATCH: jgld.dll 0x10067340 ?FUN_10067340@C_FUN_10067340@f_10067340@@QAEIHH@Z
struct C_FUN_10067340 { undefined4 FUN_10067340(int param_1, int param_2); };
undefined4 C_FUN_10067340::FUN_10067340(int param_1, int param_2)
{
  return 0x18;
}
}

namespace f_10067380 {
// MATCH: jgld.dll 0x10067380 ?FUN_10067380@C_FUN_10067380@f_10067380@@QAEIH@Z
struct C_FUN_10067380 { undefined4 FUN_10067380(int param_1); };
undefined4 C_FUN_10067380::FUN_10067380(int param_1)
{
  return 0x18;
}
}

namespace f_100673c0 {
// MATCH: jgld.dll 0x100673c0 ?FUN_100673c0@C_FUN_100673c0@f_100673c0@@QAEIH@Z
struct C_FUN_100673c0 { undefined4 FUN_100673c0(int param_1); };
undefined4 C_FUN_100673c0::FUN_100673c0(int param_1)
{
  return 0x18;
}
}

namespace f_10067400 {
// MATCH: jgld.dll 0x10067400 ?FUN_10067400@C_FUN_10067400@f_10067400@@QAEIH@Z
struct C_FUN_10067400 { undefined4 FUN_10067400(int param_1); };
undefined4 C_FUN_10067400::FUN_10067400(int param_1)
{
  return 0x18;
}
}

namespace f_10067440 {
// MATCH: jgld.dll 0x10067440 ?FUN_10067440@C_FUN_10067440@f_10067440@@QAEIH@Z
struct C_FUN_10067440 { undefined4 FUN_10067440(int param_1); };
undefined4 C_FUN_10067440::FUN_10067440(int param_1)
{
  return 0x18;
}
}

namespace f_10067480 {
// MATCH: jgld.dll 0x10067480 ?FUN_10067480@C_FUN_10067480@f_10067480@@QAEIH@Z
struct C_FUN_10067480 { undefined4 FUN_10067480(int param_1); };
undefined4 C_FUN_10067480::FUN_10067480(int param_1)
{
  return 0x18;
}
}

namespace f_100674c0 {
// MATCH: jgld.dll 0x100674c0 ?FUN_100674c0@C_FUN_100674c0@f_100674c0@@QAEXH@Z
struct C_FUN_100674c0 { void FUN_100674c0(int param_1); };
void C_FUN_100674c0::FUN_100674c0(int param_1)
{
  return;
}
}

namespace f_10067820 {
// MATCH: jgld.dll 0x10067820 ?FUN_10067820@C_FUN_10067820@f_10067820@@QAEIXZ
struct C_FUN_10067820 { undefined4 FUN_10067820(); };
undefined4 C_FUN_10067820::FUN_10067820()
{
  return 0;
}
}

namespace f_10067d50 {
// MATCH: jgld.dll 0x10067d50 ?FUN_10067d50@C_FUN_10067d50@f_10067d50@@QAEXIII@Z
extern int DAT_10122dc0;
extern int DAT_10122dc4;
extern int DAT_10122dc8;
struct C_FUN_10067d50 { void FUN_10067d50(undefined4 param_1, undefined4 param_2, undefined4 param_3); };
void C_FUN_10067d50::FUN_10067d50(undefined4 param_1, undefined4 param_2, undefined4 param_3)
{
  DAT_10122dc0 = param_1;
  DAT_10122dc4 = param_2;
  DAT_10122dc8 = param_3;
  return;
}
}
