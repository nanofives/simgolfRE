"""C3 batch c3co (round 22, 2026-10-09): golf_clean.exe static-initialization families, low half.

Reimplementations: shim/src/re/c3co.cpp. Fixtures: re/frida/js/fixtures.d/c3co.js. Purposes: log/c3/c3co_purpose.md.
All functions are argument-free, so each registry entry is one call; variants are fixtures:
  *_fail (atexitRegister_*, dynInit_*): the _onexit 0x004a5654 stand-in returns NULL, so _atexit returns -1.
  base key: the stand-in returns the pointer, so _atexit returns 0.
The stand-in records every pointer handed to _onexit (count at $log+0, pointers from $log+4): the state "$log".
initGlobalArray_* and dynInit_* write live game globals: their fixtures suspend the game's first thread, fill the
regions below with a pattern, and put the real bytes back before resuming it (the regions are the compared state).
dynInit_staticInit_00404320 / _004043b0 also reach 0x00483da0, which calls [0x0083ad50] vtable +0x150: the fixture
swaps a recording display in for the key ("$disp", counter at +4) and 0x0083ad4c is state.
"""

_LOG = [("$log", 0, 0x40)]
_DISP = [("$disp", 0, 8), (0x0083AD4C, 0, 4)]


def _r(regions):
    return [(a, 0, n) for a, n in regions]


HOOKS = {}

_e = dict(module="golf_clean.exe", addr=0x00403F40, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00403f40"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00403f40_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00403FA0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00403fa0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00403fa0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404000, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404000"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404000_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404060, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404060"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404060_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404330, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404330"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404330_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004043C0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_004043c0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_004043c0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404400, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404400"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404400_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404450, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404450"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404450_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004044A0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_004044a0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_004044a0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004044F0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_004044f0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_004044f0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404540, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404540"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404540_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404590, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404590"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404590_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004045E0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_004045e0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_004045e0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404630, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404630"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404630_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404680, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404680"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404680_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004046D0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_004046d0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_004046d0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404710, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404710"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404710_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404760, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404760"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404760_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004047B0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_004047b0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_004047b0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404800, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404800"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404800_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404850, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404850"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404850_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404CF0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404cf0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404cf0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404E90, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00404e90"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00404e90_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00405000, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00405000"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00405000_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00405150, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00405150"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00405150_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004052D0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_004052d0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_004052d0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00405440, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00405440"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00405440_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00405580, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00405580"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00405580_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004056C0, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_004056c0"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_004056c0_fail"] = dict(fixture="c3co_onexit_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00405830, abi="default", ret="int", args=[], vectors=[()], state=_LOG)
HOOKS["atexitRegister_00405830"] = dict(fixture="c3co_onexit_ok", **_e)
HOOKS["atexitRegister_00405830_fail"] = dict(fixture="c3co_onexit_fail", **_e)
HOOKS["initGlobalArray_00403f20"] = dict(module="golf_clean.exe", addr=0x00403F20, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_00403f20", state=_r([(0x004f65b8, 0x130dc)]))
HOOKS["initGlobalArray_00403f80"] = dict(module="golf_clean.exe", addr=0x00403F80, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_00403f80", state=_r([(0x0050fc10, 0x9d1c)]))
HOOKS["initGlobalArray_00403fe0"] = dict(module="golf_clean.exe", addr=0x00403FE0, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_00403fe0", state=_r([(0x00509678, 0x65b4)]))
HOOKS["initGlobalArray_00404040"] = dict(module="golf_clean.exe", addr=0x00404040, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_00404040", state=_r([(0x004e9a90, 0xcb48)]))
HOOKS["initGlobalArray_00404c20"] = dict(module="golf_clean.exe", addr=0x00404C20, abi="default", ret="pointer", args=[], vectors=[()],
    fixture="c3co_iga_00404c20", state=_r([(0x00540940, 0x78), (0x005423d8, 0x4c), (0x005439d0, 0x2e0), (0x0055e5a0, 0x390), (0x00567af8, 0x4c), (0x0056a240, 0x2e0), (0x00578790, 0x7c), (0x005791b8, 0x4c), (0x0058b640, 0x4c), (0x0059b730, 0x4c), (0x0059ca48, 0xde0), (0x005a40f0, 0x4c)]))
HOOKS["initGlobalArray_00404e00"] = dict(module="golf_clean.exe", addr=0x00404E00, abi="default", ret="pointer", args=[], vectors=[()],
    fixture="c3co_iga_00404e00", state=_r([(0x00562358, 0x5a0), (0x0056a750, 0x4c), (0x0056c7a8, 0x4c), (0x0058b348, 0x4c), (0x0059aba8, 0x2e0), (0x0059bf88, 0x4c), (0x0059d910, 0x5a0), (0x005a48d8, 0xd0)]))
HOOKS["initGlobalArray_00404f50"] = dict(module="golf_clean.exe", addr=0x00404F50, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_00404f50", state=_r([(0x0053ba38, 0x180), (0x00540d88, 0x5a0), (0x005628d8, 0x4a60), (0x0056a540, 0x230), (0x0058a528, 0xc80), (0x0059b040, 0x700), (0x0059e7b0, 0x14c0)]))
HOOKS["initGlobalArray_004050b0"] = dict(module="golf_clean.exe", addr=0x004050B0, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_004050b0", state=_r([(0x00542e40, 0xd0), (0x0055e920, 0x230), (0x005678a8, 0x180), (0x00568248, 0x390), (0x0056a7a8, 0x180), (0x00587e98, 0x26a0)]))
HOOKS["initGlobalArray_004051f0"] = dict(module="golf_clean.exe", addr=0x004051F0, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_004051f0", state=_r([(0x00541a08, 0x2e0), (0x00568fb8, 0x4f0), (0x00570ca8, 0x10a0), (0x00583a70, 0x7b0), (0x00586f40, 0x78), (0x00587398, 0x758), (0x0058b380, 0x2e0), (0x0059ba58, 0x548), (0x005a7bf8, 0x440)]))
HOOKS["initGlobalArray_004053a0"] = dict(module="golf_clean.exe", addr=0x004053A0, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_004053a0", state=_r([(0x0053fd38, 0xc28), (0x00571d30, 0x2b4), (0x0058b190, 0x1d8), (0x0059b768, 0x30c), (0x005a34e8, 0xc28), (0x005a8c80, 0x700)]))
HOOKS["initGlobalArray_004054e0"] = dict(module="golf_clean.exe", addr=0x004054E0, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_004054e0", state=_r([(0x00520580, 0xd0), (0x0053c560, 0x5a0), (0x00542f18, 0xd0), (0x0059aaf0, 0xd0), (0x0059df10, 0x8b8), (0x005a4128, 0x5a0)]))
HOOKS["initGlobalArray_00405620"] = dict(module="golf_clean.exe", addr=0x00405620, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_00405620", state=_r([(0x005612d8, 0x6d4), (0x00567380, 0x548), (0x0056a938, 0x548), (0x0058b670, 0x650), (0x0058cca8, 0x6d4), (0x005a53b8, 0x650)]))
HOOKS["initGlobalArray_00405760"] = dict(module="golf_clean.exe", addr=0x00405760, abi="default", ret="void", args=[], vectors=[()],
    fixture="c3co_iga_00405760", state=_r([(0x0053b7d0, 0x288), (0x0053de68, 0x7e0), (0x00541570, 0x470), (0x00561250, 0xa4), (0x00567a10, 0xfc), (0x005791e8, 0x2e0)]))
_e = dict(module="golf_clean.exe", addr=0x00403F10, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x004f65b8, 0x130dc)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00403f20"] = dict(fixture="c3co_dyn_00403f10_ok", **_e)
HOOKS["dynInit_initGlobalArray_00403f20_fail"] = dict(fixture="c3co_dyn_00403f10_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00403F70, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0050fc10, 0x9d1c)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00403f80"] = dict(fixture="c3co_dyn_00403f70_ok", **_e)
HOOKS["dynInit_initGlobalArray_00403f80_fail"] = dict(fixture="c3co_dyn_00403f70_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00403FD0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00509678, 0x65b4)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00403fe0"] = dict(fixture="c3co_dyn_00403fd0_ok", **_e)
HOOKS["dynInit_initGlobalArray_00403fe0_fail"] = dict(fixture="c3co_dyn_00403fd0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404030, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x004e9a90, 0xcb48)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00404040"] = dict(fixture="c3co_dyn_00404030_ok", **_e)
HOOKS["dynInit_initGlobalArray_00404040_fail"] = dict(fixture="c3co_dyn_00404030_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404310, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519a50, 0x598)]) + _LOG + _DISP)
HOOKS["dynInit_staticInit_00404320"] = dict(fixture="c3co_dyn_00404310_ok", **_e)
HOOKS["dynInit_staticInit_00404320_fail"] = dict(fixture="c3co_dyn_00404310_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004043A0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0051b058, 0x2d8)]) + _LOG + _DISP)
HOOKS["dynInit_staticInit_004043b0"] = dict(fixture="c3co_dyn_004043a0_ok", **_e)
HOOKS["dynInit_staticInit_004043b0_fail"] = dict(fixture="c3co_dyn_004043a0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004043E0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519918, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_004043f0"] = dict(fixture="c3co_dyn_004043e0_ok", **_e)
HOOKS["dynInit_staticInit_004043f0_fail"] = dict(fixture="c3co_dyn_004043e0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404430, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519fc8, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404440"] = dict(fixture="c3co_dyn_00404430_ok", **_e)
HOOKS["dynInit_staticInit_00404440_fail"] = dict(fixture="c3co_dyn_00404430_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404480, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0051b310, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404490"] = dict(fixture="c3co_dyn_00404480_ok", **_e)
HOOKS["dynInit_staticInit_00404490_fail"] = dict(fixture="c3co_dyn_00404480_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004044D0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0051b350, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_004044e0"] = dict(fixture="c3co_dyn_004044d0_ok", **_e)
HOOKS["dynInit_staticInit_004044e0_fail"] = dict(fixture="c3co_dyn_004044d0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404520, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519938, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404530"] = dict(fixture="c3co_dyn_00404520_ok", **_e)
HOOKS["dynInit_staticInit_00404530_fail"] = dict(fixture="c3co_dyn_00404520_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404570, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519a30, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404580"] = dict(fixture="c3co_dyn_00404570_ok", **_e)
HOOKS["dynInit_staticInit_00404580_fail"] = dict(fixture="c3co_dyn_00404570_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004045C0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0051a018, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_004045d0"] = dict(fixture="c3co_dyn_004045c0_ok", **_e)
HOOKS["dynInit_staticInit_004045d0_fail"] = dict(fixture="c3co_dyn_004045c0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404610, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0051b330, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404620"] = dict(fixture="c3co_dyn_00404610_ok", **_e)
HOOKS["dynInit_staticInit_00404620_fail"] = dict(fixture="c3co_dyn_00404610_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404660, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519958, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404670"] = dict(fixture="c3co_dyn_00404660_ok", **_e)
HOOKS["dynInit_staticInit_00404670_fail"] = dict(fixture="c3co_dyn_00404660_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004046B0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x005199b8, 0x78)]) + _LOG)
HOOKS["dynInit_staticInit_004046c0"] = dict(fixture="c3co_dyn_004046b0_ok", **_e)
HOOKS["dynInit_staticInit_004046c0_fail"] = dict(fixture="c3co_dyn_004046b0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004046F0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519a10, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404700"] = dict(fixture="c3co_dyn_004046f0_ok", **_e)
HOOKS["dynInit_staticInit_00404700_fail"] = dict(fixture="c3co_dyn_004046f0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404740, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519ff8, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404750"] = dict(fixture="c3co_dyn_00404740_ok", **_e)
HOOKS["dynInit_staticInit_00404750_fail"] = dict(fixture="c3co_dyn_00404740_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404790, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519978, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_004047a0"] = dict(fixture="c3co_dyn_00404790_ok", **_e)
HOOKS["dynInit_staticInit_004047a0_fail"] = dict(fixture="c3co_dyn_00404790_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004047E0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00519998, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_004047f0"] = dict(fixture="c3co_dyn_004047e0_ok", **_e)
HOOKS["dynInit_staticInit_004047f0_fail"] = dict(fixture="c3co_dyn_004047e0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404830, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0051a038, 0x28)]) + _LOG)
HOOKS["dynInit_staticInit_00404840"] = dict(fixture="c3co_dyn_00404830_ok", **_e)
HOOKS["dynInit_staticInit_00404840_fail"] = dict(fixture="c3co_dyn_00404830_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404C10, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00540940, 0x78), (0x005423d8, 0x4c), (0x005439d0, 0x2e0), (0x0055e5a0, 0x390), (0x00567af8, 0x4c), (0x0056a240, 0x2e0), (0x00578790, 0x7c), (0x005791b8, 0x4c), (0x0058b640, 0x4c), (0x0059b730, 0x4c), (0x0059ca48, 0xde0), (0x005a40f0, 0x4c)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00404c20"] = dict(fixture="c3co_dyn_00404c10_ok", **_e)
HOOKS["dynInit_initGlobalArray_00404c20_fail"] = dict(fixture="c3co_dyn_00404c10_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404DF0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00562358, 0x5a0), (0x0056a750, 0x4c), (0x0056c7a8, 0x4c), (0x0058b348, 0x4c), (0x0059aba8, 0x2e0), (0x0059bf88, 0x4c), (0x0059d910, 0x5a0), (0x005a48d8, 0xd0)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00404e00"] = dict(fixture="c3co_dyn_00404df0_ok", **_e)
HOOKS["dynInit_initGlobalArray_00404e00_fail"] = dict(fixture="c3co_dyn_00404df0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00404F40, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0053ba38, 0x180), (0x00540d88, 0x5a0), (0x005628d8, 0x4a60), (0x0056a540, 0x230), (0x0058a528, 0xc80), (0x0059b040, 0x700), (0x0059e7b0, 0x14c0)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00404f50"] = dict(fixture="c3co_dyn_00404f40_ok", **_e)
HOOKS["dynInit_initGlobalArray_00404f50_fail"] = dict(fixture="c3co_dyn_00404f40_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004050A0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00542e40, 0xd0), (0x0055e920, 0x230), (0x005678a8, 0x180), (0x00568248, 0x390), (0x0056a7a8, 0x180), (0x00587e98, 0x26a0)]) + _LOG)
HOOKS["dynInit_initGlobalArray_004050b0"] = dict(fixture="c3co_dyn_004050a0_ok", **_e)
HOOKS["dynInit_initGlobalArray_004050b0_fail"] = dict(fixture="c3co_dyn_004050a0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004051E0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00541a08, 0x2e0), (0x00568fb8, 0x4f0), (0x00570ca8, 0x10a0), (0x00583a70, 0x7b0), (0x00586f40, 0x78), (0x00587398, 0x758), (0x0058b380, 0x2e0), (0x0059ba58, 0x548), (0x005a7bf8, 0x440)]) + _LOG)
HOOKS["dynInit_initGlobalArray_004051f0"] = dict(fixture="c3co_dyn_004051e0_ok", **_e)
HOOKS["dynInit_initGlobalArray_004051f0_fail"] = dict(fixture="c3co_dyn_004051e0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00405390, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0053fd38, 0xc28), (0x00571d30, 0x2b4), (0x0058b190, 0x1d8), (0x0059b768, 0x30c), (0x005a34e8, 0xc28), (0x005a8c80, 0x700)]) + _LOG)
HOOKS["dynInit_initGlobalArray_004053a0"] = dict(fixture="c3co_dyn_00405390_ok", **_e)
HOOKS["dynInit_initGlobalArray_004053a0_fail"] = dict(fixture="c3co_dyn_00405390_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x004054D0, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x00520580, 0xd0), (0x0053c560, 0x5a0), (0x00542f18, 0xd0), (0x0059aaf0, 0xd0), (0x0059df10, 0x8b8), (0x005a4128, 0x5a0)]) + _LOG)
HOOKS["dynInit_initGlobalArray_004054e0"] = dict(fixture="c3co_dyn_004054d0_ok", **_e)
HOOKS["dynInit_initGlobalArray_004054e0_fail"] = dict(fixture="c3co_dyn_004054d0_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00405610, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x005612d8, 0x6d4), (0x00567380, 0x548), (0x0056a938, 0x548), (0x0058b670, 0x650), (0x0058cca8, 0x6d4), (0x005a53b8, 0x650)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00405620"] = dict(fixture="c3co_dyn_00405610_ok", **_e)
HOOKS["dynInit_initGlobalArray_00405620_fail"] = dict(fixture="c3co_dyn_00405610_fail", **_e)
_e = dict(module="golf_clean.exe", addr=0x00405750, abi="default", ret="int", args=[], vectors=[()],
          state=_r([(0x0053b7d0, 0x288), (0x0053de68, 0x7e0), (0x00541570, 0x470), (0x00561250, 0xa4), (0x00567a10, 0xfc), (0x005791e8, 0x2e0)]) + _LOG)
HOOKS["dynInit_initGlobalArray_00405760"] = dict(fixture="c3co_dyn_00405750_ok", **_e)
HOOKS["dynInit_initGlobalArray_00405760_fail"] = dict(fixture="c3co_dyn_00405750_fail", **_e)
