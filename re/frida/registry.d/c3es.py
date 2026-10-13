import os

# C3 batch c3es (round 32, 2026-10-13) of golf_clean.exe: the frame-mask pilot (re/frida/js/framemask.js) and the screens
# that keep their working objects in their own frame. Reimplementations: shim/src/re/c3es.cpp. Fixtures:
# re/frida/js/fixtures.d/c3es.js (generated from log/c3/alt/c3es_fixture_src.js + re/frida/js/framemask.js).
# Purposes and per-jcc tables: log/c3/c3es_purpose.md. Notes and drops: log/c3/c3es_notes.txt.
#
# Every key runs under __modal (pumps), __frameMask (recording stand-ins that log frame pointers as object tokens) and
# __uiTree (frozen first thread, recording display, heap and import stand-ins). The screens are __cdecl with no
# arguments: the single "int" argument is an unused cdecl stack word the caller pops, and diff_hook resolves "$rN" before
# the state snapshot, so the getter of fx.rN selects row N's script, cursor and scene.

EXE = "golf_clean.exe"

# re/frida/js/uitree.js: GLOBALS (4 bytes each), the z-list array, the recorder size
UI_GLOBALS = [0x0083AB94, 0x0083AB90, 0x0083AAE0, 0x0083AAEC, 0x0083AAA0, 0x0083AABC, 0x0083AAC0, 0x0083AAC4, 0x0083AA98,
              0x0083AAF0, 0x0083AB30, 0x0083AB34, 0x0083AB40, 0x0083AB44, 0x0083AB54, 0x0083AC18, 0x00839650, 0x0083AB2C,
              0x0083AA9C, 0x0083A2C8, 0x0083A2CC, 0x0083A2D0, 0x0083A2D4, 0x0083AD50, 0x0083AD4C]
REC_SIZE = 0x1000
ARENA = 0x4000


def UI_STATE(arena=ARENA):
    return ([("$heap", 0, 0x10 + arena), ("$freelog", 0, 0x404), ("$implog", 0, 0x400), ("$rec", 0, REC_SIZE)]
            + [(g, 0, 4) for g in UI_GLOBALS] + [(0x0083A2D8, 0, 0x200)])


# re/frida/js/modal.js: __modal.STATE
MODAL_STATE = [("$modal", 0, 0x1000), (0x00822B98, 0, 8), (0x00822D58, 0, 0x40), (0x004D3834, 0, 4), (0x0083AD4C, 0, 4),
               (0x0083AFCC, 0, 4), (0x00838208, 0, 4), (0x0083821C, 0, 4), (0x0083AB9C, 0, 4)]

# re/frida/js/framemask.js: the block (records with frame pointers as object tokens, counters, hash)
FM_SIZE = 0x4000
FM_STATE = [("$fmlog", 0, FM_SIZE)]

HOOKS = {}

# ------------------------------------------------------------------------------------ showShortcutsScreen 0x0044e770
# Written: the byte 0x00569498 the exit clears (0x0044f62f); everything else goes to stood-in callees ($fmlog), the
# pumps ($modal) and the display ($rec).
_SS_STATE = UI_STATE() + MODAL_STATE + FM_STATE + [(0x00569498, 0, 1)]
_SS = dict(module=EXE, addr=0x0044E770, abi="default", ret="int", args=["int"], state=_SS_STATE)

HOOKS["showShortcutsScreen"] = dict(fixture="c3es_ss", vectors=[("$r%d" % i,) for i in range(6)], **_SS)

# showThemePackSelect 0x004725b0 is DROPPED (round 32): its ORIGINAL arm cannot be run under the frame-mask fixture
# (with ??_L stubbed the Surface arrays are never constructed and its SEH record's saved-fs:[0] is clobbered, so the
# process dies in teardown; log/c3/c3es_notes.txt section 5). Not registered. Its reimplementation is compiled out in
# shim/src/re/c3es.cpp (#if 0) and the fixture c3es_tp_key is left unused in re/frida/js/fixtures.d/c3es.js.

# Controls, registered only for log/c3/alt/c3es_ctl.py (C3ES_CONTROLS=1): the same rows with the frame mask off. A
# correct body must read RED there (the raw frame pointers differ between the arms).
if os.environ.get("C3ES_CONTROLS") == "1":
    HOOKS["showShortcutsScreen_raw"] = dict(fixture="c3es_ss_raw", vectors=[("$r%d" % i,) for i in range(6)], **_SS)
