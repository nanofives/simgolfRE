# C3 batch c3da (round 24, 2026-10-10) of golf_clean.exe: the Surface wrapper layer below 0x004781f0. Reimplementations
# in shim/src/re/c3da.cpp, fixtures in re/frida/js/fixtures.d/c3da.js, Purposes and per-jcc branch coverage in
# log/c3/c3da_purpose.md.
#
# Hooked callees from other batches (the verifier re-runs these keys with SIMGOLF_HOOKS_OFF so the original arm uses
# original callees; on the c3da instance they are unhooked, since only c3da.cpp is linked there):
#   Surface_blitThunk -> Surface_blit3 0x00475d00 (C3, c3h)
#   drawLineOnSurface -> Widget_fillRect 0x00478b80 (C3, c3ae)
#   drawRectScaled    -> scaleX 0x00404970 (C3, golf_math)
#   drawPanelS2, drawFrameS2 -> fillWindowRect 0x00480b00 -> Win_fillRect 0x00478af0 -> Widget_fillArea 0x00478b50
#                        (Widget_fillArea is C3; fillWindowRect is in c3db's address range)
# Hooked callees inside this batch:
#   Surface_drawScaled2 -> Surface_blit2; Surface_draw -> Surface_drawSprite; drawScaledSprite421b90 ->
#   Surface_drawScaled2 (-> Surface_blit2); blitPanel -> Surface_blit2; drawPanelS2 / drawFrameS2 -> Surface_stretchBlit;
#   drawFrameR3 -> Surface_stretchBlit, blitPanel (-> Surface_blit2); drawRectScaled -> drawLineOnSurface.

_REC = ("$rec", 0, 0x2000)
_SURFS = ("$surfs", 0, 16 * 0xC0)
_DISP = ("$disp", 0, 0x40)
_ST_PLAIN = [_REC, _SURFS]
_ST_DISP = [_REC, _SURFS, _DISP]

_MIN = -0x80000000
_MAX = 0x7FFFFFFF


def _e(addr, args, vectors, fixture, state, ret="int", abi="thiscall"):
    return dict(module="golf_clean.exe", addr=addr, abi=abi, ret=ret, args=args, fixture=fixture, state=state,
                vectors=vectors)


# ---- leaves without the display (fixture c3da_plain) --------------------------------------------------------------
# Surface_stretchBlit(this, mask, dst, x, y, extra)
_SB = [("$s0", "$s7", "$s5", 10, 20, 0), ("$s0", "$s7", "$s5", -5, 7, "$s9"),
       ("$s1", "$s7", "$s6", _MAX, _MIN, "$s8"), ("$s2", "$s9", "$s6", 1, 2, "$s7"), ("$s4", "$s7", "$s5", 3, 3, 0),
       ("$s0", 0, "$s5", 1, 1, 0), ("$s0", "$s7", 0, 1, 1, 0), ("$s0", 0, 0, 1, 1, 0), ("$s3", "$s7", "$s5", 1, 1, 0),
       ("$s0", "$s7", "$s3", 1, 1, 0), ("$s0", "$s8", "$s5", 1, 1, 0), ("$s3", 0, "$s5", 4, 4, 0)]
# Surface_blitMasked(this, a1, a2, a3, a4, a5, a6)
_BM = [("$s0", "$s7", "$s5", "$s9", 10, 20, 0), ("$s0", "$s7", "$s5", "$s9", -1, -2, "$s6"),
       ("$s1", "$s9", "$s6", "$s7", _MAX, 5, "$s8"), ("$s2", "$s5", "$s6", "$s0", 0, 0, "$s7"),
       ("$s0", "$s7", 0, "$s9", 1, 1, 0), ("$s0", 0, "$s5", "$s9", 1, 1, 0), ("$s0", 0, 0, 0, 1, 1, 0),
       ("$s3", "$s7", "$s5", "$s9", 1, 1, 0), ("$s0", "$s7", "$s3", "$s9", 1, 1, 0),
       ("$s0", "$s8", "$s5", "$s9", 1, 1, 0), ("$s0", "$s7", "$s5", "$s3", 1, 1, 0), ("$s3", "$s7", "$s5", 0, 1, 1, 0),
       ("$s4", "$s5", "$s6", "$s7", 9, 8, "$s9")]
# Surface_drawSprite(this, dst, x, y, mode, mask, extra)
_DS = [("$s0", "$s5", 10, 20, 1, 0, 0), ("$s0", "$s5", -1, -2, 2, "$s7", 0x1234),
       ("$s1", "$s6", 0x10, _MAX, 3, "$s8", -1), ("$s2", "$s6", 0, 0, 0, "$s9", 5), ("$s4", "$s5", 1, 1, 1, 0, 0),
       ("$s4", 0, 1, 1, 1, 0, 0), ("$s0", 0, 1, 1, 1, 0, 0), ("$s3", "$s5", 1, 1, 1, 0, 0),
       ("$s0", "$s3", 1, 1, 1, 0, 0), ("$s3", 0, 1, 1, 1, 0, 0), ("$s2", "$s5", 7, 8, 9, "$s7", 10)]
# Surface_blitThunk(ecx = this; dst, dx, dy, sx, sy, w, h)
_BT = [("$s0", "$s5", 1, 2, 3, 4, 5, 6), ("$s1", "$s6", -10, 20, 30, -40, 100, 200), ("$s2", "$s5", 0, 0, 0, 0, 0, 0),
       ("$s0", "$s6", _MAX, 1, 2, _MIN, 1, 1), ("$s0", 0, 1, 2, 3, 4, 5, 6), ("$s3", "$s5", 1, 2, 3, 4, 5, 6),
       ("$s0", "$s3", 1, 2, 3, 4, 5, 6), ("$s4", "$s5", 8, 7, 6, 5, 4, 3)]
# Surface_blitTakeDirty(this, src, a2..a7): src is never null (the original reads src+0xac unconditionally).
_TD = [("$s0", "$s14", 1, 2, 3, 4, 5, 6), ("$s1", "$s15", -1, -2, -3, -4, -5, -6), ("$s2", "$s12", 0, 0, 0, 0, 0, 0),
       ("$s10", "$s14", 9, 9, 9, 9, 9, 9), ("$s0", "$s13", 7, 0, 7, 0, 7, 0), ("$s3", "$s15", 1, 1, 1, 1, 1, 1),
       ("$s5", "$s0", 2, 3, 4, 5, 6, 7), ("$s11", "$s1", 0, 1, 0, 1, 0, 1)]
# drawLineOnSurface(widget, x0, y0, x1, y1, colour): cdecl
_DL = [("$s0", 1, 2, 3, 4, 5), ("$s1", -1, -2, -3, -4, 0x1FF), ("$s2", 0, 0, 800, 600, -1), ("$s5", 10, 10, 10, 10, 0x80),
       ("$s6", _MAX, _MIN, 0, 0, 0x12345678), ("$s7", 3, 2, 1, 0, 0x7F), ("$s9", 100, -100, 50, -50, 0x100),
       ("$s0", 0, 0, 0, 0, 0)]

_P = "pointer"
HOOKS = {
    "Surface_stretchBlit": _e(0x00473F60, [_P, _P, _P, "int", "int", _P], _SB, "c3da_plain", _ST_PLAIN),
    "Surface_blitMasked": _e(0x00473FC0, [_P, _P, _P, _P, "int", "int", _P], _BM, "c3da_plain", _ST_PLAIN),
    "Surface_drawSprite": _e(0x00474440, [_P, _P, "int", "int", "int", _P, "int"], _DS, "c3da_plain", _ST_PLAIN),
    "Surface_blitThunk": _e(0x00475C60, [_P, _P] + ["int"] * 6, _BT, "c3da_plain", _ST_PLAIN),
    "Surface_blitTakeDirty": _e(0x00473C60, [_P, _P] + ["int"] * 6, _TD, "c3da_plain", _ST_PLAIN),
    "drawLineOnSurface": _e(0x004049A0, [_P] + ["int"] * 5, _DL, "c3da_plain", _ST_PLAIN, abi="default"),
}

# ---- display keys (first thread frozen, 0x0083ad50 = the fixture display) ------------------------------------------
# Surface_blit2 / Sprite_draw(this, dst, x, y, mode, mask)
_B2 = [("$s0", "$s5", 10, 20, 0x77, 0), ("$s0", "$s5", -5, 7, 1, "$s7"), ("$s1", "$s6", 3, 4, 5, "$s8"),
       ("$s2", "$s6", _MAX, _MIN, 0, "$s9"), ("$s4", "$s5", 1, 1, 1, 0), ("$s4", 0, 1, 1, 1, 0), ("$s0", 0, 1, 1, 1, 0),
       ("$s3", "$s5", 1, 1, 1, 0), ("$s0", "$s3", 1, 1, 1, 0), ("$s3", 0, 1, 1, 1, 0), ("$s2", "$s5", -100, 100, 2, "$s7")]
# Sprite_drawScaled(this, dst, x, y, mode, sx, sy, den, mask, extra): den is never 0 where it is reached.
_SS = [("$s0", "$s5", 10, 20, 1, 2, 3, 1, 0, 0), ("$s1", "$s6", 0, 0, 2, -3, 5, 7, "$s7", 9),
       ("$s2", "$s6", -5, 5, 3, 100, 1, -3, "$s8", -1), ("$s0", 0, 1, 1, 1, 1, 1, 1, 0, 0),
       ("$s3", "$s5", 1, 1, 1, 1, 1, 1, 0, 0), ("$s0", "$s3", 1, 1, 1, 1, 1, 1, 0, 0),
       ("$s4", "$s5", 1, 1, 1, 4, 4, 2, "$s9", 0), ("$s2", "$s5", _MAX, _MIN, 0, 0x10000, 0x10000, 0x100, "$s7", 3),
       ("$s0", "$s6", 7, 7, 7, -1, -1, -1, 0, 7), ("$s1", "$s5", 100, 200, 0, 1, 0, 5, "$s9", 0)]
# Surface_drawScaled2(this, dst, x, y, mode, sx, sy, den, mask)
_D2 = [("$s0", "$s5", 10, 20, 1, 2, 3, 1, 0), ("$s1", "$s6", 0, 0, 2, -3, 5, 7, "$s7"),
       ("$s2", "$s6", -5, 5, 3, 100, 1, -3, "$s8"), ("$s0", 0, 1, 1, 1, 2, 2, 1, 0), ("$s3", "$s5", 1, 1, 1, 1, 1, 1, 0),
       ("$s4", "$s5", 1, 1, 1, 4, 4, 2, "$s9"), ("$s2", "$s5", _MAX, _MIN, 0, 0x10000, 0x10000, 0x100, "$s7"),
       ("$s0", "$s3", 7, 7, 7, -1, -1, -1, 0), ("$s1", "$s5", 100, 200, 0, 1, 0, 5, "$s9")]
# Surface_draw(this, dst, x, y, mode, sx, sy, den, mask, extra)
_DR = [("$s0", "$s5", 10, 20, 1, 2, 3, 1, 0, 0), ("$s1", "$s6", 0, 0, 2, -3, 5, 7, "$s7", 9),
       ("$s2", "$s6", -5, 5, 3, 100, 1, -3, "$s8", -1), ("$s0", 0, 1, 1, 1, 2, 2, 1, 0, 0),
       ("$s3", "$s5", 1, 1, 1, 1, 1, 1, 0, 0), ("$s4", "$s5", 1, 1, 1, 4, 4, 2, "$s9", 0),
       ("$s2", "$s5", _MAX, _MIN, 0, 0x10000, 0x10000, 0x100, "$s7", 3), ("$s0", "$s3", 7, 7, 7, -1, -1, -1, 0, 7),
       ("$s1", "$s5", 100, 200, 0, 1, 0, 5, "$s9", 0)]
# drawScaledSprite421b90(ecx = this; dst, x, y, mode, scale, den, mask)
_S421 = [("$s0", "$s5", 10, 20, 1, 2, 1, 0), ("$s1", "$s6", 0, 0, 2, -3, 7, "$s7"), ("$s2", "$s6", -5, 5, 3, 100, -3, "$s8"),
         ("$s0", 0, 1, 1, 1, 2, 1, 0), ("$s3", "$s5", 1, 1, 1, 1, 1, 0), ("$s4", "$s5", 1, 1, 1, 4, 2, "$s9"),
         ("$s2", "$s5", _MAX, _MIN, 0, 0x10000, 0x100, "$s7"), ("$s1", "$s5", 100, 200, 0, 5, 3, "$s9")]

for _sc in ("s1", "s2", "s3", "s4"):
    _fx = "c3da_disp_" + _sc
    HOOKS["Surface_blit2_" + _sc] = _e(0x004740F0, [_P, _P, "int", "int", "int", _P], _B2, _fx, _ST_DISP)
    HOOKS["Sprite_draw_" + _sc] = _e(0x004741B0, [_P, _P, "int", "int", "int", _P], _B2, _fx, _ST_DISP)
for _sc in ("s1", "s3"):
    _fx = "c3da_disp_" + _sc
    HOOKS["Sprite_drawScaled_" + _sc] = _e(0x00474260, [_P, _P] + ["int"] * 6 + [_P, "int"], _SS, _fx, _ST_DISP)
    HOOKS["Surface_drawScaled2_" + _sc] = _e(0x00474030, [_P, _P] + ["int"] * 6 + [_P], _D2, _fx, _ST_DISP)
    HOOKS["Surface_draw_" + _sc] = _e(0x00474380, [_P, _P] + ["int"] * 6 + [_P, "int"], _DR, _fx, _ST_DISP)
    HOOKS["drawScaledSprite421b90_" + _sc] = _e(0x00421B90, [_P, _P] + ["int"] * 5 + [_P], _S421, _fx, _ST_DISP)

# ---- lifetime keys (c3da_life_r0 / _r9: the created handle's slot +4 returns 0 / 9) --------------------------------
# Surface_copyFrom(this, src): returns this. Targets have m_8 = m_c = 0, so Surface_free never reaches _free / delete.
_CF = [("$s10", "$s13"), ("$s10", "$s12"), ("$s11", "$s13"), ("$s11", "$s12"), ("$s0", "$s13"), ("$s5", "$s14"),
       ("$s0", "$s1"), ("$s11", "$s9")]
# Surface_create / Surface_init2(this, w, h)
_CR = [("$s10", 64, 32), ("$s11", 1, 1), ("$s0", 0, 0), ("$s10", -5, _MAX), ("$s5", 800, 600), ("$s11", _MIN, 16)]
for _r in ("r0", "r9"):
    _fx = "c3da_life_" + _r
    HOOKS["Surface_copyFrom_" + _r] = _e(0x004744C0, [_P, _P], _CF, _fx, _ST_DISP, ret="pointer")
    HOOKS["Surface_create_" + _r] = _e(0x00474550, [_P, "int", "int"], _CR, _fx, _ST_DISP)
    HOOKS["Surface_init2_" + _r] = _e(0x004745C0, [_P, "int", "int"], _CR, _fx, _ST_DISP)

# ---- static-table keys (first thread frozen; the game's tile Surfaces, *0x004c1570, the window handle and the style
# byte 0x005a34e0 replaced by fixture values) --------------------------------------------------------------------------
_ST_STATIC = [_REC, _DISP]
# blitPanel(x, y, w, h, style)
_BP = [(0, 0, 48, 32, 0), (0, 0, 48, 32, 1), (5, 7, 16, 16, 1), (0, 0, 0, 10, 1), (10, 0, -5, 10, 1), (0, 0, 40, 1, 1),
       (100, 50, 64, 48, 2), (-8, -8, 33, 17, 1), (0, 0, 16, 40, 1), (0, 0, 17, 17, 1), (3, 3, 10, 0, 1),
       (0, 0, 32, 48, -1)]
# drawPanelS2(x, y, w, h)
_PS = [(0, 0, 64, 48), (10, 10, 50, 37), (0, 0, 32, 32), (0, 0, 33, 33), (5, 5, 200, 100), (0, 0, 16, 16),
       (3, 4, 1, 1), (0, 0, 48, 17), (-20, -30, 49, 50)]
# drawFrameR3 / drawFrameS2(x, y, w, h, fill)
_FR = [(0, 0, 64, 48, 0), (0, 0, 64, 48, 1), (10, 10, 50, 37, 1), (0, 0, 32, 32, 0), (0, 0, 33, 33, 1),
       (5, 5, 200, 100, 0), (3, 4, 1, 1, 1), (-20, -30, 49, 50, 7), (0, 0, 48, 17, 0)]
_FR0 = [v for v in _FR if v[4] == 0]
# drawRectScaled(x, y, w, h, colour)
_RS = [(0, 0, 100, 50, 1), (10, 20, 30, 40, 0x1FF), (-5, -5, 10, 10, 0x80), (0, 0, 0, 0, 0), (799, 599, 1, 1, 0xFF),
       (100, 100, -50, -50, 3), (_MAX, 1, 1, 1, 2), (320, 240, 160, 120, 0x40), (_MIN, 7, 640, 480, -1)]

_C5 = ["int"] * 5
HOOKS.update({
    "blitPanel": _e(0x0040CA10, _C5, _BP, "c3da_static", _ST_STATIC, ret="void", abi="default"),
    "blitPanel_busy": _e(0x0040CA10, _C5, _BP[:3] + _BP[6:8], "c3da_static_busy", _ST_STATIC, ret="void",
                         abi="default"),
    "drawPanelS2": _e(0x0040CC00, ["int"] * 4, _PS, "c3da_static", _ST_STATIC, ret="void", abi="default"),
    "drawPanelS2_nodisp": _e(0x0040CC00, ["int"] * 4, _PS[:5], "c3da_static_nodisp", _ST_STATIC, ret="void",
                             abi="default"),
    "drawFrameR3": _e(0x0040CEF0, _C5, _FR, "c3da_static", _ST_STATIC, ret="void", abi="default"),
    # without a display blitPanel's Surface_blit2 would call through a null display: fill = 0 only
    "drawFrameR3_nodisp": _e(0x0040CEF0, _C5, _FR0, "c3da_static_nodisp", _ST_STATIC, ret="void", abi="default"),
    "drawFrameS2": _e(0x0040D0B0, _C5, _FR, "c3da_static", _ST_STATIC, ret="void", abi="default"),
    "drawFrameS2_st2": _e(0x0040D0B0, _C5, _FR[:5], "c3da_static_st2", _ST_STATIC, ret="void", abi="default"),
    "drawFrameS2_st3": _e(0x0040D0B0, _C5, _FR[:5], "c3da_static_st3", _ST_STATIC, ret="void", abi="default"),
    "drawFrameS2_nodisp": _e(0x0040D0B0, _C5, _FR[:5], "c3da_static_nodisp", _ST_STATIC, ret="void", abi="default"),
    "drawRectScaled": _e(0x0046E710, _C5, _RS, "c3da_target", [_REC, _SURFS], abi="default"),
})
