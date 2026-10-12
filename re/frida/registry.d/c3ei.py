# C3 batch c3ei (round 30, 2026-10-12) of golf_clean.exe: image consumers. Reimplementations: shim/src/re/c3ei.cpp.
# Fixtures: re/frida/js/fixtures.d/c3ei.js (generated from log/c3/c3ei_parts). Purposes and per-jcc tables: log/c3/c3ei_purpose.md.
#
# Every key freezes the game's first thread (re/frida/js/freeze.js). Callees that run ORIGINAL (their C3 hooks are switched off in
# the main build's callee-off pass) are listed per key below.

import json
import pathlib

EXE = "golf_clean.exe"
_P = "pointer"

_CLOG = ("$clog", 0, 0x4000)


def _h(addr, fixture, vectors, state, ret="int", args=(), abi="default"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


HOOKS = {}
_COUNTS = json.loads((pathlib.Path(__file__).resolve().parents[3] / "log/c3/c3ei_counts.json").read_text())

# ---------------------------------------------------------------------------------------------------- loaders
# Argument-free cdecl functions: the one dummy argument is the row getter ($r<i> selects the stand-ins' results for the vector).
# Real callees: Image ctor 0x00474ae0 (and its Timer/list sub-constructors), the Image destructor 0x00474c40 (its 0x004928d0 calls).
HOOKS["loadPcxSurface"] = _h(0x0049D1B0, "c3ei_lp", [(f"$r{i}",) for i in range(12)], [_CLOG], args=("int",))
HOOKS["loadFonts"] = _h(0x0049FE50, "c3ei_lf", [(f"$r{i}",) for i in range(12)], [_CLOG], args=("int",))
HOOKS["loadCursors"] = _h(0x0049E9D0, "c3ei_lc", [(f"$r{i}",) for i in range(12)], [_CLOG], args=("int",))

# ---------------------------------------------------------------------------------------------------- addSprite
_SQ_ARR = [0x00830164, 0x00831164, 0x0082C164, 0x0082915C, 0x0082B160, 0x0082415C, 0x0082A15C, 0x0082815C]
_SQ_STATE = [_CLOG]
for _a in _SQ_ARR:
    _SQ_STATE += [(_a, 0, 0x20), (_a, 0x400, 4), (_a, 0xFFC, 4)]
_SQN = 72
HOOKS["addSprite"] = _h(0x004628D0, "c3ei_sq",
                        [(f"$v{i}_0", f"$v{i}_1", f"$r{i}", f"$v{i}_2", f"$v{i}_3", f"$v{i}_4", f"$v{i}_5", f"$v{i}_6")
                         for i in range(_SQN)],
                        _SQ_STATE, args=("int",) * 8)

# ---------------------------------------------------------------------------------------------------- drawQueuedSprite
# Callees stood in on the A/B thread (calls in $clog): formatGolferTitle 0x00462020, Sprite_draw 0x004741b0, Sprite_drawScaled
# 0x00474260, Surface_drawSprite 0x00474440, Surface_blit2 0x004740f0, setDrawTarget 0x004762d0, Widget_fillRect 0x00478b80,
# drawCenteredText 0x00404b70, buildGolferName 0x004676e0, Display::setScale (the live display's slot 0x9c). Real callees: the
# Cache482fd0 constructor 0x00482fd0 / destructor 0x00483010 and _itoa 0x004ad425.
HOOKS["drawQueuedSprite"] = _h(0x00462BE0, "c3ei_dq", [(f"$r{i}",) for i in range(_COUNTS["dq"])],
                               [_CLOG, (0x0051A068, 0, 0x40), (0x00834164, 0, 0x20)], args=("int",))

# vectors for the keys whose arguments come from the fixture: spec entries are "$r" (the row getter, argument value = the row's
# first result), "$v<k>" (fx.v<i>_<k>) or a plain number
def _vecs(n, spec):
    out = []
    for i in range(n):
        out.append(tuple(f"$r{i}" if s == "$r" else (f"$v{i}_{s[2:]}" if isinstance(s, str) and s.startswith("$v") else s)
                         for s in spec))
    return out


# ---------------------------------------------------------------------------------------------------- drawThoughtBubble
# Stood in: formatGolferTitle 0x00462020, typeBit7Clear 0x0046c940, thoughtFlag 0x004675d0, Win_fillRect 0x00478af0,
# Surface_drawScaled 0x00473cb0, Image_draw 0x00473e60.
HOOKS["drawThoughtBubble"] = _h(0x0045C200, "c3ei_tb", _vecs(_COUNTS["tb"], ["$r", "$v0", "$v1", "$v2"]), [_CLOG], args=("int",) * 4)

# ---------------------------------------------------------------------------------------------------- drawTerrainToolbar
# Stood in: hitBuildTool433c60 0x00433c60, Surface_stretchBlit 0x00473f60, setDrawTarget 0x004762d0, Image_draw 0x00473e60,
# drawPanelBackdrop 0x00432620. The text buffer 0x0051a068 and the two hold globals are compared state.
HOOKS["drawTerrainToolbar"] = _h(0x00433E50, "c3ei_tt", _vecs(_COUNTS["tt"], ["$r", "$v0"]),
                                 [_CLOG, (0x0051A068, 0, 0x40), (0x005AA554, 0, 4), (0x005A9F54, 0, 4)], args=("int",) * 2)

# ---------------------------------------------------------------------------------------------------- drawAmenityRow
# Stood in: Surface_stretchBlit 0x00473f60, Surface_drawScaled 0x00473cb0, cellAt 0x0043d6f0, addSprite 0x004628d0, the live display's
# slot 0x9c. Real: approxDistance 0x00467170.
HOOKS["drawAmenityRow"] = _h(0x00432200, "c3ei_ar",
                             _vecs(_COUNTS["ar"], ["$r", "$v0", "$v1", "$v2", "$v3", "$v4", "$v5", "$v6", "$v7", "$v8", "$v9"]),
                             [_CLOG, (0x005AA6C4, 0, 4)], args=("int",) * 11)

# ---------------------------------------------------------------------------------------------------- drawWindow
# Stood in: setDrawTarget 0x004762d0, wrapMessageBuffer 0x0045af30, drawWindowFrame 0x0040cdd0, drawFrameR3 0x0040cef0, drawFrameS2
# 0x0040d0b0, playSound 0x004481b0, drawMessageBuffer 0x0045b0d0, Surface_stretchBlit 0x00473f60, drawThoughtBubble 0x0045c200 (this
# batch), Surface_drawScaled 0x00473cb0.
HOOKS["drawWindow"] = _h(0x0040D320, "c3ei_wd", _vecs(_COUNTS["wd"], ["$r", "$v0", 0, "$v1"]),
                         [_CLOG, (0x00587EA0, 0, 4)], args=("int",) * 4)

# ---------------------------------------------------------------------------------------------------- scrollbarDraw
# Stood in: Window::paint 0x004808c0, Widget_drawBevelBoxR 0x004795d0, scrollbarThumbRect 0x004974d0, Window::paintBackground 0x00480a10,
# blitRight 0x00476140, Image_draw 0x00473e60, the live display's slot 0x9c. The virtual slots 0x120 / 0xd4 / 0xd8 / 0xdc record in $rec.
HOOKS["scrollbarDraw"] = _h(0x00496FC0, "c3ei_sb", [(f"$r{i}",) for i in range(_COUNTS["sb"])],
                            [_CLOG, ("$rec", 0, 0x2000), ("$cnt", 0, 0x80)], args=(_P,), abi="thiscall")

# ---------------------------------------------------------------------------------------------------- drawRecords
# Stood in: worldToScreen 0x0042fb90, Image_draw 0x00473e60, enqueueSprite 0x00462a30, cellAt 0x0043d6f0, addSprite 0x004628d0.
# The eax left at ret is an incidental leftover (void).
HOOKS["drawRecords"] = _h(0x004012D0, "c3ei_dr", _vecs(_COUNTS["dr"], ["$r", "$v0", "$v1"]),
                          [_CLOG, (0x004E9A80, 0, 4)] + [(0x004E6D20 + 116 * k, 0, 116) for k in range(4)], ret="void", args=("int",) * 3)

# ---------------------------------------------------------------------------------------------------- drawPanel4810d0
# Stood in: Widget_applyPalette 0x00478b30, Image_draw 0x00473e60, blitRight 0x00476140, drawPanelContents 0x0047d850, the live display's
# slot 0x9c. The object's vtable slots 0x118 / 0x164, the callback +0x56c and the piece handles' slots 0xc0 / 0xd8 / 0xdc record in $rec.
HOOKS["drawPanel4810d0"] = _h(0x004810D0, "c3ei_pn", _vecs(_COUNTS["pn"], ["$r", "$v0"]),
                              [_CLOG, ("$rec", 0, 0x2000), ("$cnt", 0, 0x80)], ret="void", args=(_P, "int"), abi="thiscall")

# ---------------------------------------------------------------------------------------------------- drawBuildingPanel
# Stood in: drawTerrainToolbar 0x00433e50 (this batch), the hit test 0x004340a0, clamp 0x00467130, drawFrameS2 0x0040d0b0, setDrawTarget
# 0x004762d0, drawCenteredText 0x00404b70, appendNumber 0x0042dc00, Surface_stretchBlit 0x00473f60, Image_draw 0x00473e60,
# Surface_drawScaled 0x00473cb0, drawPanelBackdrop 0x00432620.
HOOKS["drawBuildingPanel"] = _h(0x00434350, "c3ei_bp", _vecs(_COUNTS["bp"], ["$r", "$v0"]),
                                [_CLOG, (0x0051A068, 0, 0x40), (0x005AA554, 0, 4), (0x005A9F54, 0, 4)], args=("int",) * 2)

# ---------------------------------------------------------------------------------------------------- drawAmenitiesPanel
# Stood in: Surface_stretchBlit 0x00473f60, the hit test 0x00434980, Image_draw 0x00473e60, drawAmenityRow 0x00432200 (this batch), 0x004074a0,
# drawPanelBackdrop 0x00432620, setDrawTarget 0x004762d0, drawMapLine 0x004493d0, drawCenteredText 0x00404b70. Real: _itoa 0x004ad425.
# The eax left at ret is an incidental leftover (void).
HOOKS["drawAmenitiesPanel"] = _h(0x00434CF0, "c3ei_ap", _vecs(_COUNTS["ap"], ["$r", "$v0"]),
                                 [_CLOG, (0x0051A068, 0, 0x40), (0x005A9F6C, 0, 0x20), (0x005A9F50, 0, 4), (0x005A9F54, 0, 4),
                                  (0x005AA554, 0, 4)], ret="void", args=("int",) * 2)

# ---------------------------------------------------------------------------------------------------- formatGolferTitle
# typeBit7Clear 0x0046c940 is stood in; the part objects, the second table and the compositor are objects of ours whose native slots log into $clog.
# The type record window (the body writes six of its bytes back) is compared state for the types the rows use.
HOOKS["formatGolferTitle"] = _h(0x00462020, "c3ei_tl", [(f"$r{i}",) for i in range(_COUNTS["tl"])],
                                [_CLOG, (0x0081CA14, 0, 4), (0x0081CA10, 0, 4)] + [(0x004D60AA + 560 * t, 0, 0x10) for t in range(16)], args=("int",))
