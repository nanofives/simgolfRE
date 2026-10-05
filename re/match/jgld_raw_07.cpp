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

namespace f_10009740 {
// MATCH: jgld.dll 0x10009740 ?FUN_10009740@C_FUN_10009740@f_10009740@@QAEIXZ
struct C_FUN_10009740 { undefined4 FUN_10009740(); };
undefined4 C_FUN_10009740::FUN_10009740()
{
  return 0;
}
}

namespace f_10009810 {
// MATCH: jgld.dll 0x10009810 ?FUN_10009810@C_FUN_10009810@f_10009810@@QAEXH@Z
struct C_FUN_10009810 { void FUN_10009810(int param_1); };
void C_FUN_10009810::FUN_10009810(int param_1)
{
  return;
}
}

namespace f_10009840 {
// MATCH: jgld.dll 0x10009840 ?FUN_10009840@C_FUN_10009840@f_10009840@@QAEXH@Z
struct C_FUN_10009840 { void FUN_10009840(int param_1); };
void C_FUN_10009840::FUN_10009840(int param_1)
{
  return;
}
}

namespace f_10009870 {
// MATCH: jgld.dll 0x10009870 ?FUN_10009870@C_FUN_10009870@f_10009870@@QAEIHHHHHHH@Z
struct C_FUN_10009870 { undefined4 FUN_10009870(int param_1, int param_2, int param_3, int param_4, int param_5, int param_6, int param_7); };
undefined4 C_FUN_10009870::FUN_10009870(int param_1, int param_2, int param_3, int param_4, int param_5, int param_6, int param_7)
{
  return 0x18;
}
}

namespace f_100098b0 {
// MATCH: jgld.dll 0x100098b0 ?FUN_100098b0@C_FUN_100098b0@f_100098b0@@QAEIHHH@Z
struct C_FUN_100098b0 { undefined4 FUN_100098b0(int param_1, int param_2, int param_3); };
undefined4 C_FUN_100098b0::FUN_100098b0(int param_1, int param_2, int param_3)
{
  return 0x18;
}
}

namespace f_100099c0 {
// MATCH: jgld.dll 0x100099c0 ?FUN_100099c0@C_FUN_100099c0@f_100099c0@@QAEIHH@Z
struct C_FUN_100099c0 { undefined4 FUN_100099c0(int param_1, int param_2); };
undefined4 C_FUN_100099c0::FUN_100099c0(int param_1, int param_2)
{
  return 0x18;
}
}

namespace f_10009a00 {
// MATCH: jgld.dll 0x10009a00 ?FUN_10009a00@C_FUN_10009a00@f_10009a00@@QAEIHHHHHH@Z
struct C_FUN_10009a00 { undefined4 FUN_10009a00(int param_1, int param_2, int param_3, int param_4, int param_5, int param_6); };
undefined4 C_FUN_10009a00::FUN_10009a00(int param_1, int param_2, int param_3, int param_4, int param_5, int param_6)
{
  return 0x18;
}
}

namespace f_10009a40 {
// MATCH: jgld.dll 0x10009a40 ?FUN_10009a40@C_FUN_10009a40@f_10009a40@@QAEIHHHHHHHHH@Z
struct C_FUN_10009a40 { undefined4 FUN_10009a40(int param_1, int param_2, int param_3, int param_4, int param_5, int param_6, int param_7, int param_8, int param_9); };
undefined4 C_FUN_10009a40::FUN_10009a40(int param_1, int param_2, int param_3, int param_4, int param_5, int param_6, int param_7, int param_8, int param_9)
{
  return 0x18;
}
}

namespace f_10009a80 {
// MATCH: jgld.dll 0x10009a80 ?FUN_10009a80@C_FUN_10009a80@f_10009a80@@QAEIHHHH@Z
struct C_FUN_10009a80 { undefined4 FUN_10009a80(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009a80::FUN_10009a80(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009b50 {
// MATCH: jgld.dll 0x10009b50 ?FUN_10009b50@C_FUN_10009b50@f_10009b50@@QAEIHHHH@Z
struct C_FUN_10009b50 { undefined4 FUN_10009b50(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009b50::FUN_10009b50(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009b90 {
// MATCH: jgld.dll 0x10009b90 ?FUN_10009b90@C_FUN_10009b90@f_10009b90@@QAEIH@Z
struct C_FUN_10009b90 { undefined4 FUN_10009b90(int param_1); };
undefined4 C_FUN_10009b90::FUN_10009b90(int param_1)
{
  return 0x18;
}
}

namespace f_10009bd0 {
// MATCH: jgld.dll 0x10009bd0 ?FUN_10009bd0@C_FUN_10009bd0@f_10009bd0@@QAEIHHHH@Z
struct C_FUN_10009bd0 { undefined4 FUN_10009bd0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009bd0::FUN_10009bd0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009c10 {
// MATCH: jgld.dll 0x10009c10 ?FUN_10009c10@C_FUN_10009c10@f_10009c10@@QAEIH@Z
struct C_FUN_10009c10 { undefined4 FUN_10009c10(int param_1); };
undefined4 C_FUN_10009c10::FUN_10009c10(int param_1)
{
  return 0x18;
}
}

namespace f_10009d00 {
// MATCH: jgld.dll 0x10009d00 ?FUN_10009d00@C_FUN_10009d00@f_10009d00@@QAEIH@Z
struct C_FUN_10009d00 { undefined4 FUN_10009d00(int param_1); };
undefined4 C_FUN_10009d00::FUN_10009d00(int param_1)
{
  return 0x18;
}
}

namespace f_10009d40 {
// MATCH: jgld.dll 0x10009d40 ?FUN_10009d40@C_FUN_10009d40@f_10009d40@@QAEIHHH@Z
struct C_FUN_10009d40 { undefined4 FUN_10009d40(int param_1, int param_2, int param_3); };
undefined4 C_FUN_10009d40::FUN_10009d40(int param_1, int param_2, int param_3)
{
  return 0x18;
}
}

namespace f_10009d80 {
// MATCH: jgld.dll 0x10009d80 ?FUN_10009d80@C_FUN_10009d80@f_10009d80@@QAEIHHHH@Z
struct C_FUN_10009d80 { undefined4 FUN_10009d80(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009d80::FUN_10009d80(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009dc0 {
// MATCH: jgld.dll 0x10009dc0 ?FUN_10009dc0@C_FUN_10009dc0@f_10009dc0@@QAEIHHHH@Z
struct C_FUN_10009dc0 { undefined4 FUN_10009dc0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009dc0::FUN_10009dc0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009e00 {
// MATCH: jgld.dll 0x10009e00 ?FUN_10009e00@C_FUN_10009e00@f_10009e00@@QAEIHHHH@Z
struct C_FUN_10009e00 { undefined4 FUN_10009e00(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009e00::FUN_10009e00(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009ed0 {
// MATCH: jgld.dll 0x10009ed0 ?FUN_10009ed0@C_FUN_10009ed0@f_10009ed0@@QAEIHHHH@Z
struct C_FUN_10009ed0 { undefined4 FUN_10009ed0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009ed0::FUN_10009ed0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009f10 {
// MATCH: jgld.dll 0x10009f10 ?FUN_10009f10@C_FUN_10009f10@f_10009f10@@QAEIHHHH@Z
struct C_FUN_10009f10 { undefined4 FUN_10009f10(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009f10::FUN_10009f10(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009f50 {
// MATCH: jgld.dll 0x10009f50 ?FUN_10009f50@C_FUN_10009f50@f_10009f50@@QAEIHHHH@Z
struct C_FUN_10009f50 { undefined4 FUN_10009f50(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009f50::FUN_10009f50(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_10009f90 {
// MATCH: jgld.dll 0x10009f90 ?FUN_10009f90@C_FUN_10009f90@f_10009f90@@QAEIHHHH@Z
struct C_FUN_10009f90 { undefined4 FUN_10009f90(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_10009f90::FUN_10009f90(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000a170 {
// MATCH: jgld.dll 0x1000a170 ?FUN_1000a170@C_FUN_1000a170@f_1000a170@@QAEIHHHH@Z
struct C_FUN_1000a170 { undefined4 FUN_1000a170(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000a170::FUN_1000a170(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000a1b0 {
// MATCH: jgld.dll 0x1000a1b0 ?FUN_1000a1b0@C_FUN_1000a1b0@f_1000a1b0@@QAEIHHHH@Z
struct C_FUN_1000a1b0 { undefined4 FUN_1000a1b0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000a1b0::FUN_1000a1b0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000a1f0 {
// MATCH: jgld.dll 0x1000a1f0 ?FUN_1000a1f0@C_FUN_1000a1f0@f_1000a1f0@@QAEIH@Z
struct C_FUN_1000a1f0 { undefined4 FUN_1000a1f0(int param_1); };
undefined4 C_FUN_1000a1f0::FUN_1000a1f0(int param_1)
{
  return 0x18;
}
}

namespace f_1000a230 {
// MATCH: jgld.dll 0x1000a230 ?FUN_1000a230@C_FUN_1000a230@f_1000a230@@QAEIXZ
struct C_FUN_1000a230 { undefined4 FUN_1000a230(); };
undefined4 C_FUN_1000a230::FUN_1000a230()
{
  return 0x18;
}
}

namespace f_1000a270 {
// MATCH: jgld.dll 0x1000a270 ?FUN_1000a270@C_FUN_1000a270@f_1000a270@@QAEIHHH@Z
struct C_FUN_1000a270 { undefined4 FUN_1000a270(int param_1, int param_2, int param_3); };
undefined4 C_FUN_1000a270::FUN_1000a270(int param_1, int param_2, int param_3)
{
  return 0x18;
}
}

namespace f_1000a2b0 {
// MATCH: jgld.dll 0x1000a2b0 ?FUN_1000a2b0@C_FUN_1000a2b0@f_1000a2b0@@QAEIH@Z
struct C_FUN_1000a2b0 { undefined4 FUN_1000a2b0(int param_1); };
undefined4 C_FUN_1000a2b0::FUN_1000a2b0(int param_1)
{
  return 0x18;
}
}

namespace f_1000a2f0 {
// MATCH: jgld.dll 0x1000a2f0 ?FUN_1000a2f0@C_FUN_1000a2f0@f_1000a2f0@@QAEIHHHH@Z
struct C_FUN_1000a2f0 { undefined4 FUN_1000a2f0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000a2f0::FUN_1000a2f0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000abb0 {
// MATCH: jgld.dll 0x1000abb0 ?FUN_1000abb0@C_FUN_1000abb0@f_1000abb0@@QAEIHHHH@Z
struct C_FUN_1000abb0 { undefined4 FUN_1000abb0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000abb0::FUN_1000abb0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000abf0 {
// MATCH: jgld.dll 0x1000abf0 ?FUN_1000abf0@C_FUN_1000abf0@f_1000abf0@@QAEIHHHH@Z
struct C_FUN_1000abf0 { undefined4 FUN_1000abf0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000abf0::FUN_1000abf0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000ac30 {
// MATCH: jgld.dll 0x1000ac30 ?FUN_1000ac30@C_FUN_1000ac30@f_1000ac30@@QAEIH@Z
struct C_FUN_1000ac30 { undefined4 FUN_1000ac30(int param_1); };
undefined4 C_FUN_1000ac30::FUN_1000ac30(int param_1)
{
  return 0x18;
}
}

namespace f_1000ac70 {
// MATCH: jgld.dll 0x1000ac70 ?FUN_1000ac70@C_FUN_1000ac70@f_1000ac70@@QAEIH@Z
struct C_FUN_1000ac70 { undefined4 FUN_1000ac70(int param_1); };
undefined4 C_FUN_1000ac70::FUN_1000ac70(int param_1)
{
  return 0x18;
}
}

namespace f_1000acb0 {
// MATCH: jgld.dll 0x1000acb0 ?FUN_1000acb0@C_FUN_1000acb0@f_1000acb0@@QAEIHHHH@Z
struct C_FUN_1000acb0 { undefined4 FUN_1000acb0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000acb0::FUN_1000acb0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000acf0 {
// MATCH: jgld.dll 0x1000acf0 ?FUN_1000acf0@C_FUN_1000acf0@f_1000acf0@@QAEIHHHH@Z
struct C_FUN_1000acf0 { undefined4 FUN_1000acf0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000acf0::FUN_1000acf0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000ad30 {
// MATCH: jgld.dll 0x1000ad30 ?FUN_1000ad30@C_FUN_1000ad30@f_1000ad30@@QAEIHHHH@Z
struct C_FUN_1000ad30 { undefined4 FUN_1000ad30(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000ad30::FUN_1000ad30(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000ad70 {
// MATCH: jgld.dll 0x1000ad70 ?FUN_1000ad70@C_FUN_1000ad70@f_1000ad70@@QAEIHHHH@Z
struct C_FUN_1000ad70 { undefined4 FUN_1000ad70(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000ad70::FUN_1000ad70(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000adb0 {
// MATCH: jgld.dll 0x1000adb0 ?FUN_1000adb0@C_FUN_1000adb0@f_1000adb0@@QAEIHHHH@Z
struct C_FUN_1000adb0 { undefined4 FUN_1000adb0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000adb0::FUN_1000adb0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000adf0 {
// MATCH: jgld.dll 0x1000adf0 ?FUN_1000adf0@C_FUN_1000adf0@f_1000adf0@@QAEIHHHH@Z
struct C_FUN_1000adf0 { undefined4 FUN_1000adf0(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000adf0::FUN_1000adf0(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}

namespace f_1000ae30 {
// MATCH: jgld.dll 0x1000ae30 ?FUN_1000ae30@C_FUN_1000ae30@f_1000ae30@@QAEIHHHH@Z
struct C_FUN_1000ae30 { undefined4 FUN_1000ae30(int param_1, int param_2, int param_3, int param_4); };
undefined4 C_FUN_1000ae30::FUN_1000ae30(int param_1, int param_2, int param_3, int param_4)
{
  return 0x18;
}
}
