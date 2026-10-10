# Batch c3dk (round 26, jgld.dll): the leads c3dg left and the two rows blocked only by unnamed callees.
# Reimplementations and instruction citations: shim/src/re/c3dk.cpp; fixtures: re/frida/js/fixtures.d/c3dk.js;
# Purposes and per-jcc branch tables: log/c3/c3dk_purpose.md; what was not taken: log/c3/c3dk_notes.txt.
# addr is the RVA (VA = 0x10000000 + addr).

HOOKS = {}

# ======================================================================== group D: Display construction
# $D0..$D11: `this` pointers inside the patterned block $blk (0x160 apart, some misaligned). $gcmd is the module's
# command-handler pointer 0x10128728, set to a sentinel under a frozen first thread (fixture c3dk_display).
_D = [f"$D{i}" for i in range(12)]
_DST = [("$blk", 0, 0x1400), ("$gcmd", 0, 4)]
HOOKS["Display::Display"] = dict(
    module="jgld.dll", addr=0x000654B0, abi="thiscall", ret="pointer", args=["pointer"],
    fixture="c3dk_display", state=_DST, vectors=[(d,) for d in _D])
HOOKS["DisplayBase::ctor"] = dict(
    module="jgld.dll", addr=0x00065630, abi="thiscall", ret="pointer", args=["pointer"],
    fixture="c3dk_display", state=_DST, vectors=[(d,) for d in _D])
HOOKS["DisplayMember::ctor"] = dict(
    module="jgld.dll", addr=0x00069270, abi="thiscall", ret="pointer", args=["pointer"],
    fixture="c3dk_display", state=_DST, vectors=[(d,) for d in _D])

# ======================================================================== group S: Sprite dispatchers
# c3dc's object set (copied into c3dk.js): fourteen surfaces on jgld's Surface vtable (d0..d6 16-bit, d2 format 1
# = 555, d4 no pixel base; e0..e3 8-bit, e3 no pixel base; x0/x1/x2 depths 24/12/0), Sprites on jgld's Sprite
# vtable (bA/bB/bW 8 bpp, bBsy/bBsy2/bBnp busy run-length, bNP no Palette, bNB no payload, b16/b16w/b16Bsy 16 bpp,
# bX 24 bpp), Palettes $pal/$pal2. One key per scale setting (_s111 unscaled bodies, _s223 the stepped ones):
# the dispatchers read none of the scale globals. Every key freezes the first thread and silences jgld's debug
# reports (c3dc). State: c3dc's _SPR list (pixels, sprites, payloads, lock windows, the blitters' module blocks).
_SPR = ([("$pix", 0, 14 * 0x1000), ("$spr", 0, 15 * 0x80), ("$spix", 0, 6 * 0x4000)] +
        [(f"$l{i}", 0, 0x14) for i in range(14)] +
        [("$gmask", 0, 0xC), ("$gvstep", 0, 8), ("$gtbl", 0, 4), ("$gpos", 0, 8), ("$glevel", 0, 4),
         ("$gcols", 0, 8), ("$gxstep", 0, 0x14), ("$grle", 0, 0x5C), ("$growpair", 0, 8),
         ("$g8vstep", 0, 8), ("$g8tbl", 0, 4), ("$g8pos", 0, 8), ("$g8cols", 0, 4), ("$g8colour", 0, 4),
         ("$g8pix", 0, 4), ("$g8step", 0, 8), ("$g8xstep", 0, 0xC), ("$g8rowpair", 0, 8)])


def _mask_vectors(colour):
    """(sprite, dst, x, y, mask, level[, colour]) over every arm of drawC / drawD; colour None for drawD."""
    c = () if colour is None else (colour,)
    v = [("$bA", "$d0", 0, 0, "$e0", 0x80) + c + (0,),          # 8 bpp -> 16, not busy: draw16m
         ("$bA", "$d1", 3, 2, "$e1", 0x40) + c + ("$pal2",),    # clip inside, Palette override
         ("$bB", "$d2", -2, -1, "$e0", 0x80) + c + (0,),        # format 1, clipped left and top
         ("$bW", "$d5", 12, 8, "$e2", 0x10) + c + (0,),         # stride 24, clipped right and bottom
         ("$bNP", "$d0", 1, 1, "$e0", 0xC0) + c + (0,),         # no Palette of its own: the client's
         ("$bA", "$d0", 0, 0, "$e0", 0x00) + c + (0,),          # level 0
         ("$bA", "$d0", 0, 0, "$e1", 0x180) + c + (0,),         # level masked to a byte
         ("$bA", "$d0", 0, 0, 0, 0x80) + c + (0,),              # mask NULL: the blitter's guard
         ("$bA", "$d0", 0, 0, "$e3", 0x80) + c + (0,),          # mask without pixel base: its lock fails
         ("$bA", "$d4", 0, 0, "$e0", 0x80) + c + (0,),          # destination without pixel base
         ("$bBsy", "$d0", 0, 0, "$e0", 0x80) + c + (0,),        # busy 8 bpp -> 16: the run-length blitter
         ("$bBsy2", "$d1", 2, 1, "$e1", 0x40) + c + ("$pal",),  # busy, 20 x 10, override
         ("$bBnp", "$d0", 0, 0, "$e0", 0x20) + c + (0,),        # busy, no Palette
         ("$bA", "$e0", 0, 0, "$e1", 0x80) + c + (0,),          # 8 bpp -> 8, not busy: report, 0x17
         ("$bBsy", "$e1", 1, 1, "$e0", 0x80) + c + (0,),        # 8 bpp -> 8, busy: report, result untouched
         ("$bA", "$x0", 0, 0, "$e0", 0x80) + c + (0,),          # 8 bpp -> depth 24: report, 0x17
         ("$b16w", "$d0", 0, 0, "$e0", 0x80) + c + (0,),        # 16 bpp -> 16, not busy: report, 0x17
         ("$b16Bsy", "$d0", 0, 0, "$e0", 0x80) + c + ("$pal2",),  # 16 bpp -> 16, busy: result untouched
         ("$b16w", "$e0", 0, 0, "$e1", 0x80) + c + (0,),        # 16 bpp -> 8, not busy
         ("$b16w", "$x1", 0, 0, "$e0", 0x80) + c + (0,),        # 16 bpp -> depth 12: report, 0x17
         ("$bX", "$d0", 0, 0, "$e0", 0x80) + c + (0,),          # 24 bpp: the outer default
         ("$bA", 0, 0, 0, "$e0", 0x80) + c + ("$pal2",),        # NULL destination: 7, Palette untouched
         ("$bNB", "$d0", 0, 0, "$e0", 0x80) + c + ("$pal2",)]   # slot +0x20 returns 0: 7, Palette untouched
    return v


# drawAlpha: one alpha per interval of the tree and every boundary value (g = 0.03125; all exact in float)
_ALPHAS = [1.0, 0.95, 0.90625, 0.875, 0.84375, 0.8, 0.78125, 0.75, 0.71875, 0.7, 0.65625, 0.625, 0.6, 0.59375,
           0.55, 0.53125, 0.5, 0.46875, 0.45, 0.40625, 0.375, 0.34375, 0.3, 0.28125, 0.25, 0.21875, 0.2, 0.15625,
           0.125, 0.1, 0.09375, 0.05, 0.03125, 0.0, -0.0, 0.01]
_ALPHA_V = ([("$bA", "$d0", 0, 0, a, 0, i & 1) for i, a in enumerate(_ALPHAS)] +           # 565
            [("$bB", "$d2", -2, -1, a, 0, (i + 1) & 1) for i, a in enumerate(_ALPHAS)] +   # 555 (format word 1)
            [("$bA", "$d0", 0, 0, -0.25, 0, 0), ("$bA", "$d0", 0, 0, -1e-7, 0, 0),         # alpha < 0 -> 3
             ("$bA", "$d0", 0, 0, 1.5, 0, 0), ("$bA", "$d0", 0, 0, 1.0000001, 0, 0),       # alpha > 1 -> 3
             ("$bA", "$d0", 0, 0, "$nan", 0, 0),                                           # unordered -> 3
             ("$bA", "$d1", 3, 2, 0.6, "$pal2", 1),                                        # clip, override
             ("$bNP", "$d0", 1, 1, 0.3, 0, 0),                                             # client's Palette
             ("$bW", "$d5", 12, 8, 0.9, 0, 0),                                             # clipped
             ("$bA", "$d3", 0, 0, 0.6, 0, 0),                                              # format word 2: 565
             ("$bA", "$d4", 0, 0, 0.6, 0, 0),                                              # no pixel base
             ("$bA", "$e0", 0, 0, 0.6, 0, 0),                                              # 8 -> 8: report, 0
             ("$bBsy", "$d0", 0, 0, 0.6, 0, 0),                                            # busy: report, 0
             ("$bA", "$x0", 0, 0, 0.6, 0, 0),                                              # depth 24: 0x17
             ("$b16w", "$d0", 0, 0, 0.6, 0, 0),                                            # 16 bpp: 0x17
             ("$bX", "$d0", 0, 0, 0.6, "$pal", 0),                                         # 24 bpp: 0x17
             ("$bA", 0, 0, 0, 0.6, "$pal2", 0),                                            # NULL: 0x10
             ("$bNB", "$d0", 0, 0, 0.6, "$pal2", 0)])                                      # invalid: 7

for _s in ("s111", "s223"):
    _fx = f"c3dk_spr_{_s}"
    HOOKS[f"Sprite::drawC_{_s}"] = dict(
        module="jgld.dll", addr=0x000157E0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "pointer", "int", "int", "pointer"], fixture=_fx, state=_SPR,
        vectors=_mask_vectors(0x1234) + [("$bA", "$d0", 0, 0, "$e0", 0x80, 0x10001, 0),
                                         ("$bBsy", "$d0", 0, 0, "$e1", 0x80, 0xFFFF, 0)])
    HOOKS[f"Sprite::drawD_{_s}"] = dict(
        module="jgld.dll", addr=0x00015BB0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "pointer", "int", "pointer"], fixture=_fx, state=_SPR,
        vectors=_mask_vectors(None))
    HOOKS[f"Sprite::drawAlpha_{_s}"] = dict(
        module="jgld.dll", addr=0x00018920, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "float", "pointer", "int"], fixture=_fx, state=_SPR,
        vectors=_ALPHA_V if _s == "s111" else _ALPHA_V[::3])

# ======================================================================== group W: Display::createWindow
# (this, listener, title, mode). One key per import outcome: _ok, _regfail (RegisterClassA 0), _atom0 (an atom whose
# low word is 0: 0x10000), _cwfail (CreateWindowExA NULL). The listener is only stored, so plain numbers serve.
_WIN = [("$W0", 0x11110001, "$t0", "$M0"), ("$W1", 0, "$t1", "$M1"), ("$W2", "$blk", "$t0", "$M1"),
        ("$W3", 0x7FFF0000, 0, "$M0"), ("$W4", 0xFFFFFFFF, "$t1", "$M0"), ("$W5", 0x22220002, "$t0", "$M1")]
_WST = [("$blk", 0, 0x1000), ("$gsink", 0, 4), ("$implog", 0, 0x400), ("$modelog", 0, 0x100)]
for _k in ("ok", "regfail", "atom0", "cwfail"):
    HOOKS[f"Display::createWindow_{_k}"] = dict(
        module="jgld.dll", addr=0x000657E0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "pointer", "pointer"], fixture=f"c3dk_win_{_k}", state=_WST, vectors=_WIN)

# ======================================================================== group A: allocDrawBuffer
# (this, w, h, bpp, unused). Surfaces S0 32x16 depth 16 with every handle set, S1 the same with none, S2 7x3
# depth 8 with only a DC, S3 0x0 depth 0. Keys: _d16 (the module Display answers 16), _d0 (it answers 0 -> 8),
# _dcfail (CreateCompatibleDC NULL). CreateDIBSection's failure arm ends in _exit and is not run (purpose file).
_ADB = [("$S0", -1, 5, 16, 0), ("$S0", 5, -1, 16, 0), ("$S0", 32, 16, 16, 0), ("$S0", 32, 16, 0, 0),
        ("$S0", 32, 16, 8, 7), ("$S0", 32, 15, 16, 0), ("$S1", 40, 20, 8, 0), ("$S1", 41, 20, 16, 0),
        ("$S2", 41, 21, 24, 0), ("$S2", 42, 21, 32, 0), ("$S3", 43, 5, 15, 0), ("$S3", 7, 3, 0, 0),
        ("$S1", 0, 0, 0, 0), ("$S2", 7, 3, 0, 0), ("$S2", 7, 3, 8, 0), ("$S2", 1, 1, 8, 0x1234),
        ("$S1", 0x7FFFFFFE, 1, 8, 0), ("$S1", 0x7FFFFFFF, 1, 16, 0), ("$S0", 33, 16, 24, 0), ("$S3", 6, 2, 32, 0),
        ("$S1", 9, 9, 40, 0), ("$S2", 9, 9, 4, 0)]
_AST = [("$surf", 0, 4 * 0x820), ("$implog", 0, 0x800), ("$displog", 0, 0x40), ("$bits", 0, 0x400)]
for _k in ("d16", "d0", "dcfail"):
    HOOKS[f"allocDrawBuffer_{_k}"] = dict(
        module="jgld.dll", addr=0x00007E40, abi="thiscall", ret="int",
        args=["pointer", "int", "int", "int", "int"], fixture=f"c3dk_adb_{_k}", state=_AST,
        vectors=_ADB if _k in ("d16", "d0") else _ADB[2:12])

# ======================================================================== group F: Font::loadFile
# (this, file, name, a, b). Fonts F0..F3 on a recording vtable; file names f0..f4 (f3 "abcd" is exactly four
# characters); names n0 / n1 (empty). Keys by import outcome: _ok, _csfr50 / _csfr0 (CreateScalableFontResourceA 0
# with GetLastError 0x50 / 0: carry on), _csfr5 (code 5: free, 1), _addfail (AddFontResourceA 0: free, 1),
# _allocfail (the first allocation of each vector fails: 4).
_FONT = [("$F0", "$f0", "$n0", 1, 2), ("$F1", "$f1", "$n1", 0, 0), ("$F2", "$f2", "$n0", -1, 7),
         ("$F3", "$f3", "$n0", 3, 3), ("$F0", "$f4", "$n1", 9, 0), ("$F1", 0, "$n0", 1, 1), ("$F2", "$f0", 0, 1, 1)]
_FST = [("$blk", 0, 0x400), ("$fontlog", 0, 0x100), ("$implog", 0, 0x800), ("$arena", 0, 0x410),
        ("$freelog", 0, 0x404)]
for _k in ("ok", "csfr50", "csfr0", "csfr5", "addfail", "allocfail"):
    HOOKS[f"Font::loadFile_{_k}"] = dict(
        module="jgld.dll", addr=0x00068AE0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "pointer", "int", "int"], fixture=f"c3dk_font_{_k}", state=_FST, vectors=_FONT)

# ======================================================================== group P: png_set_pCAL / png_read_destroy
# png_set_pCAL(png_ptr, info_ptr, purpose, X0, X1, type, nparams, units, params): the two NULL early returns,
# nparams 0..5 (A<n> holds n strings, some empty), type and nparams above a byte (only the low byte is stored).
_PCAL = [("$png", "$I0", "$sP", 1, 2, 0, 0, "$sU", "$A0"), ("$png", "$I1", "$sP2", -5, 7, 1, 1, "$sU", "$A1"),
         ("$png", "$I2", "$sE", 0x7FFFFFFF, -1, 2, 3, "$sE", "$A3"), ("$png", "$I3", "$sP", 3, 4, 0x103, 2, "$sU2", "$A2"),
         ("$png", "$I0", "$sU2", 0, 0, 3, 5, "$sP2", "$A5"), ("$png", "$I1", "$sP", -1, -2, 0xFF, 4, "$sU", "$A4"),
         (0, "$I0", "$sP", 1, 2, 0, 0, "$sU", "$A0"), ("$png", 0, "$sP", 1, 2, 0, 0, "$sU", "$A0"),
         (0, 0, "$sP", 1, 2, 0, 1, "$sU", "$A1")]
HOOKS["png_set_pCAL"] = dict(
    module="jgld.dll", addr=0x0007DE10, abi="default", ret="void",
    args=["pointer", "pointer", "pointer", "int", "int", "int", "int", "pointer", "pointer"],
    fixture="c3dk_pcal", state=[("$blk", 0, 0x1000), ("$arena", 0, 0x810), ("$freelog", 0, 0x404)], vectors=_PCAL)
# png_read_destroy(png_ptr, info_ptr, end_info_ptr): P0..P5 cover the three flag bits, the three table arrays with
# shifts 6 / 8 / 9 / 7 / 5 (counts 4, 1, none, 2, 8), all-NULL singles; info structs J0..J3 with and without text
# and pCAL arrays, NULL info / end_info.
_RD = [("$P0", "$J0", "$J1"), ("$P1", 0, 0), ("$P2", "$J2", 0), ("$P3", 0, "$J3"), ("$P4", "$J1", "$J2"),
       ("$P5", "$J3", "$J0"), ("$P0", 0, 0), ("$P2", "$J1", "$J3"), ("$P5", 0, "$J2"), ("$P3", "$J0", 0)]
HOOKS["png_read_destroy"] = dict(
    module="jgld.dll", addr=0x0006D250, abi="default", ret="void", args=["pointer", "pointer", "pointer"],
    fixture="c3dk_read_destroy", state=[("$blk", 0, 0x2000), ("$arena", 0, 0x110), ("$freelog", 0, 0x404)],
    vectors=_RD)

# png_read_start_row(png_ptr): R0..R26 (fixture table START_ROWS) reach every transformation arm: interlaced with
# and without INTERLACE and passes 0 / 1 / 3 / 6, PACK below and at 8 bits, EXPAND for palette with and without
# tRNS, gray below 8 bits with tRNS, gray 16, RGB with tRNS (the * 4 / 3), FILLER for palette / gray at 8 and 16 /
# RGB at 24 and 48, GRAY_TO_RGB with EXPAND + tRNS, with FILLER, with gray-alpha at 16 and 32 bits, and plain at 8 and 16 bits,
# USER_TRANSFORM larger and not larger than max, EXPAND and FILLER with gray-alpha (no arm), GRAY_TO_RGB with tRNS
# but no EXPAND.
HOOKS["png_read_start_row"] = dict(
    module="jgld.dll", addr=0x0007D6B0, abi="default", ret="void", args=["pointer"], fixture="c3dk_start_row",
    state=[("$blk", 0, 0x220 * 27), ("$arena", 0, 0x2010), ("$freelog", 0, 0x404)],
    vectors=[(f"$R{i}",) for i in range(27)])

# png_build_gamma_table(png_ptr): G0..G15 (fixture table GAMMAS): gamma 0 and NaN (nothing), screen_gamma above and
# at or below 1e-6, 8-bit and below with and without BACKGROUND 0x80 / RGB_TO_GRAY 0x200000 / 0x400000; 16-bit with
# colour (max of three sig_bit bytes, each of the three the largest once) and gray, shifts clamped up by 16_TO_8
# (to 5) or kept by it (6), clamped down to 8 and up from a negative value (sig_bit 20) to 0, num 1..256,
# the 0x480 arm (fill by `last`) and the other (pow per entry), the 16-bit to_1 / from_1 tables. Operands are not
# exact in binary (0.45455, 1/3, 0.7, 1.7...), so a rounding difference cannot hide.
HOOKS["png_build_gamma_table"] = dict(
    module="jgld.dll", addr=0x00077D30, abi="default", ret="void", args=["pointer"], fixture="c3dk_gamma",
    state=[("$blk", 0, 0x220 * 16), ("$arena", 0, 0x30010), ("$freelog", 0, 0x404)],
    vectors=[(f"$G{i}",) for i in range(16)])
