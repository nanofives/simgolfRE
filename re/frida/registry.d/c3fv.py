import os

# C3 batch c3fv (port readiness, 2026-10-15) of golf_clean.exe: gameMain 0x0045baf0 and the C2 callees of mainLoop.
# Reimplementations: shim/src/re/c3fv.cpp. Fixtures: re/frida/js/fixtures.d/c3fv.js. Purposes and per-jcc tables:
# log/c3/c3fv_purpose.md. Notes: log/c3/c3fv_notes.txt.
#
# The loaders are __cdecl with no arguments: the single "int" argument is an unused cdecl stack word the caller pops,
# and diff_hook resolves "$rN" before the state snapshot, so the getter of fx.rN applies row N.

EXE = "golf_clean.exe"

# re/frida/js/framemask.js: the block (records with frame pointers as object tokens, counters, hash)
FM_STATE = [("$fmlog", 0, 0x8000)]
CNT_STATE = [("$cnt", 0, 0x20)]

HOOKS = {}

# -------------------------------------------------------------------------------------- loadInfoScreenArt 0x0044bde0
# Written: nothing directly; every effect goes through stood-in callees ($fmlog) and the fixture Surface's counters.
_LISA = dict(module=EXE, addr=0x0044BDE0, abi="default", ret="void", args=["int"], state=FM_STATE + CNT_STATE)
HOOKS["loadInfoScreenArt"] = dict(fixture="c3fv_lisa", vectors=[("$r%d" % i,) for i in range(8)], **_LISA)

# Controls, registered only for the control runner (C3FV_CONTROLS=1): the frame mask off. A correct body must read RED
# there (the raw frame pointers differ between the arms).
if os.environ.get("C3FV_CONTROLS") == "1":
    HOOKS["loadInfoScreenArt_raw"] = dict(fixture="c3fv_lisa_raw", vectors=[("$r%d" % i,) for i in range(8)], **_LISA)

# ---------------------------------------------------------------------------------------- loadWorldAssets 0x0043dbe0
# Written directly: the course-type byte (rows set it), 0x004c89a8, 0x00820b6c, three flic slot ranges and their flag
# bytes, the object-type table 0x00578350 and the building-name strings 0x004c273c..; everything else goes through
# stood-in callees ($fmlog). The fixture freezes the first thread and restores these regions after the key.
LWA_STATE = [(0x005A34E0, 0, 4), (0x004C89A8, 0, 4), (0x00820B6C, 0, 4), (0x005A9A48, 0, 0x148),
             (0x0081AC5C, 0, 82 * 0x32), (0x005A9C28, 0, 0x3C), (0x0081C3CC, 0, 15 * 0x32), (0x005A99B0, 0, 0x90),
             (0x0081A4F0, 0, 36 * 0x32), (0x00578350, 0, 0x450), (0x004C273C, 0, 0xA8)]
_LWA = dict(module=EXE, addr=0x0043DBE0, abi="default", ret="void", args=["int"],
            state=[("$fmlog", 0, 0x10000)] + LWA_STATE)
HOOKS["loadWorldAssets"] = dict(fixture="c3fv_lwa", vectors=[("$r%d" % i,) for i in range(12)], **_LWA)
if os.environ.get("C3FV_CONTROLS") == "1":
    HOOKS["loadWorldAssets_raw"] = dict(fixture="c3fv_lwa_raw", vectors=[("$r%d" % i,) for i in range(12)], **_LWA)

# --------------------------------------------------------------------------------------------- gameMain 0x0045baf0
# __stdcall with four stack arguments it never reads (ret 0x10). Every callee except scaleX is a recorded stand-in in
# one ordered log ($rec); written globals are compared and restored; $table is the colour table malloc returns.
GM_STATE = [("$rec", 0, 0x4000), ("$table", 0, 0x20000), (0x004E42C8, 0, 0x2C8), (0x0083AB6C, 0, 0x0C),
            (0x0083ABDC, 0, 4), (0x0083AD44, 0, 4), (0x0083AFD8, 0, 0x0C), (0x0083B604, 0, 8), (0x0083B9D0, 0, 4),
            (0x0083B9F0, 0, 4), (0x00822C74, 0, 4), (0x00822C8C, 0, 8)]
HOOKS["gameMain"] = dict(module=EXE, addr=0x0045BAF0, abi="stdcall", ret="int", args=["int", "int", "int", "int"],
                         fixture="c3fv_gm", vectors=[("$r%d" % i, 0, 0, 0) for i in range(10)], state=GM_STATE)

# --------------------------------------------------------------------------------------- loadInterfaceArt 0x00442180
# __cdecl (full): every table it resets or fills is compared and restored (frozen first thread); every callee is on
# the fixture's ordered recorder ($rec), the asset callees with the dword 0x00820b6c appended to each record.
LIA_STATE = [("$rec", 0, 0x30000)] + [(a, 0, n) for a, n in (
    (0x00575AB0, 0x28A0), (0x005794B8, 0x9C00), (0x00585850, 0x1300), (0x005409AC, 0x3E8), (0x0051B388, 0x3E8),
    (0x0053E63C, 0x3E8), (0x00520640, 0x3E8), (0x00568600, 0x3E8), (0x0059DEA0, 0x80), (0x00584210, 0x7D0),
    (0x0056AE70, 0x1700), (0x005422F8, 0xF0), (0x005685F0, 4), (0x0059AE80, 0x1C8), (0x0053CAF0, 0x1388),
    (0x0053EA24, 0x9C4), (0x005A4998, 0xA2C), (0x0059BFC4, 0x20), (0x0058BCB8, 0x1000), (0x0059B730, 4),
    (0x0059AE7C, 4), (0x00543D10, 0x19A28), (0x005A9370, 0x938), (0x0053F3E8, 0x938), (0x008156D0, 0x733C),
    (0x00820B6C, 4), (0x00820B70, 0x190), (0x0059B76C, 8), (0x0051A068, 0x100), (0x0080B130, 0x1C00))]
HOOKS["loadInterfaceArt"] = dict(module=EXE, addr=0x00442180, abi="default", ret="void", args=["int"],
                                 fixture="c3fv_lia", vectors=[("$r%d" % i,) for i in range(7)], state=LIA_STATE)

# --------------------------------------------------------------------------------------- drawWorldObjects 0x00430360
# Every table it reads or writes is seeded by the row and compared; every callee is on the ordered recorder ($rec).
DWO_STATE = [("$rec", 0, 0x40000)] + [(a, 0, n) for a, n in (
    (0x00572CAC, 0xA04), (0x005419D0, 0x40), (0x005794B8, 0x9800), (0x0053F3E8, 0x4D0), (0x005722E8, 0x9C4),
    (0x00578350, 0x6F0), (0x0059E7B8, 4), (0x005A5A00, 4), (0x00834170, 4), (0x005685F4, 4), (0x005A60B0, 4),
    (0x0053FA6C, 4), (0x0053FC00, 8), (0x005A9CC8, 4), (0x0051A068, 0x40), (0x0058A528, 0x10))]
HOOKS["drawWorldObjects"] = dict(module=EXE, addr=0x00430360, abi="default", ret="void", args=["int"],
                                 fixture="c3fv_dwo", vectors=[("$r%d" % i,) for i in range(27)], state=DWO_STATE)
