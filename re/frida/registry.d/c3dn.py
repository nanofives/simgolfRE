# Batch c3dn (round 27, jgld.dll): reachable C2 rows the import / heap stand-ins and the freeze helper unblock.
# Reimplementations and instruction citations: shim/src/re/c3dn.cpp; fixtures: re/frida/js/fixtures.d/c3dn.js;
# Purposes and per-jcc branch tables: log/c3/c3dn_purpose.md; what was not taken: log/c3/c3dn_notes.txt.
# addr is the RVA (VA = 0x10000000 + addr).

HOOKS = {}

_J = dict(module="jgld.dll")

# ======================================================================== group W: Display import wrappers
# Twelve synthetic Displays $D0..$D11 (fixture c3dn_disp_*): window handles at +0x144 (D2 and D7 hold 0), instance
# handles at +0x140, objects with a recording slot +8 at +0x148 (D0, D3, D6, D9), a patterned DEVMODE at +0x98, a
# vtable whose slot +0x58 records into $modelog. Every import is stood in; $implog is compared.
_DS = [("$blk", 0, 0x1400), ("$implog", 0, 0x400), ("$modelog", 0, 0x100), ("$rellog", 0, 0x80)]
_D = [f"$D{i}" for i in range(12)]
for _name, _addr in (("Display::showWindow", 0x000675E0), ("Display::hideWindow", 0x00067640),
                     ("Display::minimizeWindow", 0x000676A0), ("Display::maximizeWindow", 0x00067700)):
    HOOKS[_name] = dict(_J, addr=_addr, abi="thiscall", ret="int", args=["pointer"], fixture="c3dn_disp_ok",
                        state=_DS, vectors=[(d,) for d in _D])
HOOKS["Display::getWindowRect"] = dict(
    _J, addr=0x00066650, abi="thiscall", ret="int", args=["pointer", "pointer"], fixture="c3dn_disp_ok", state=_DS,
    vectors=[(d, ("$R0", "$R1", "$R2", 0)[i % 4]) for i, d in enumerate(_D)])
HOOKS["Display::invalidate"] = dict(
    _J, addr=0x00066710, abi="thiscall", ret="int", args=["pointer", "pointer"], fixture="c3dn_disp_ok", state=_DS,
    vectors=[(d, (0, "$R0", "$R1", "$R2")[i % 4]) for i, d in enumerate(_D)])
HOOKS["Display::setWindowPos"] = dict(
    _J, addr=0x00066810, abi="thiscall", ret="int", args=["pointer", "int", "int", "int", "int"],
    fixture="c3dn_disp_ok", state=_DS,
    vectors=[("$D0", 0, 0, 800, 600), ("$D1", 10, 20, 640, 480), ("$D2", -5, -7, 1, 1), ("$D3", 100, 0, 0, 0),
             ("$D4", 0x7FFFFFFF, -0x80000000, 3, 4), ("$D5", 1, 2, 3, 4), ("$D6", 33, 44, 1024, 768),
             ("$D7", 0, 1, 0, 1), ("$D8", -1, -1, -1, -1), ("$D9", 7, 9, 11, 13), ("$D10", 12, 34, 56, 78),
             ("$D11", 640, 480, 320, 240)])
HOOKS["Display::getCursor"] = dict(
    _J, addr=0x00066780, abi="thiscall", ret="pointer", args=["pointer", "pointer", "pointer"],
    fixture="c3dn_disp_ok", state=_DS,
    vectors=[(d, f"$P{(2 * i) % 6}", f"$P{(2 * i + 1) % 6}") for i, d in enumerate(_D)])
for _k in ("ok", "fail", "mix"):
    HOOKS[f"Display::restore_{_k}"] = dict(
        _J, addr=0x00066970, abi="thiscall", ret="int", args=["pointer"], fixture=f"c3dn_disp_{_k}", state=_DS,
        vectors=[(d,) for d in _D])
    HOOKS[f"Display::setResolution_{_k}"] = dict(
        _J, addr=0x00066890, abi="thiscall", ret="int", args=["pointer", "int", "int"], fixture=f"c3dn_disp_{_k}",
        state=_DS,
        vectors=[("$D0", 800, 600), ("$D1", 640, 480), ("$D2", 1024, 768), ("$D3", 0, 0), ("$D4", -1, -1),
                 ("$D5", 1, 2), ("$D6", 1280, 1024), ("$D7", 320, 200), ("$D8", 0x7FFFFFFF, 7),
                 ("$D9", 3, 0x10000), ("$D10", 1600, 1200), ("$D11", 512, 384)])
HOOKS["Display::setDesktopMode"] = dict(
    _J, addr=0x00066200, abi="thiscall", ret="int", args=["pointer", "int"], fixture="c3dn_disp_ok", state=_DS,
    vectors=[(d, (0, 1, -1, 16, 0x7FFFFFFF, 2)[i % 6]) for i, d in enumerate(_D)])
HOOKS["Display::shutdown"] = dict(
    _J, addr=0x000656C0, abi="thiscall", ret="pointer", args=["pointer"], fixture="c3dn_disp_ok", state=_DS,
    vectors=[(d,) for d in _D])

# ======================================================================== group M: module globals
# Fixture c3dn_glob*: the font DC 0x10128724, the module palette 0x10128738 and the event sink 0x1012872c set with
# the first thread frozen ($gfont / $gpal / $gsink are state, the game's values go back before the resume).
_GS = [("$blk", 0, 0x1000), ("$implog", 0, 0x400), ("$gfont", 0, 4), ("$gpal", 0, 4), ("$gsink", 0, 4),
       ("$sink", 0, 0x200)]
_O = [f"$O{i}" for i in range(12)]
for _i in range(10):
    HOOKS[f"initFontTable_{_i}"] = dict(
        _J, addr=0x00068FA0, abi="default", ret="int", args=[], fixture=f"c3dn_font_{_i}", state=_GS, vectors=[()])
    HOOKS[f"Display::shutdownFonts_{_i}"] = dict(
        _J, addr=0x00067E40, abi="thiscall", ret="int", args=["pointer"], fixture=f"c3dn_font_{_i}", state=_GS,
        vectors=[("$O0",)])
    HOOKS[f"Display::freePalette_{_i}"] = dict(
        _J, addr=0x00067ED0, abi="thiscall", ret="int", args=["pointer"], fixture=f"c3dn_pal_{_i}", state=_GS,
        vectors=[("$O1",)])
HOOKS["Palette::animate"] = dict(
    _J, addr=0x0006A860, abi="thiscall", ret="int", args=["pointer"], fixture="c3dn_glob", state=_GS,
    vectors=[(o,) for o in _O])
HOOKS["Palette::animate_zero"] = dict(
    _J, addr=0x0006A860, abi="thiscall", ret="int", args=["pointer"], fixture="c3dn_glob_zero", state=_GS,
    vectors=[(o,) for o in _O[:3]])
HOOKS["trace"] = dict(
    _J, addr=0x00014550, abi="default", ret="int", args=["pointer", "pointer"], fixture="c3dn_glob", state=_GS,
    vectors=[((0, "$O3", 0x1234, "$blk")[i % 4], f"$m{i}") for i in range(12)])
HOOKS["DllMain"] = dict(
    _J, addr=0x00065300, abi="stdcall", ret="int", args=["pointer", "int", "pointer"], fixture="c3dn_glob",
    state=_GS,
    vectors=[(0x10000000, 1, 0), (0x10000000, 0, 0), (0x10000000, 2, 0), (0x10000000, 3, 0), (0, 1, 1), (0, 0, 1),
             (0x10000000, 4, 0), (0x10000000, -1, 0), (0x10000000, 0x101, 0), (1, 0x80000000, 0),
             (0x10000000, 1, "$blk"), (0x10000000, 0x10000, 0)])
HOOKS["onPaint"] = dict(
    _J, addr=0x00069AC0, abi="default", ret="int", args=["int"], fixture="c3dn_glob", state=_GS,
    vectors=[(h,) for h in (0x00010010, 0x00020022, 0, 0x7FFF0004, 0xFFFF1234, 1, 0x0ABC0DEF, 0x00300030,
                            0x80000000, 0x0000FFFE, 0x12345678, 0x00000013)])
HOOKS["onActivate"] = dict(
    _J, addr=0x00069B90, abi="default", ret="int", args=["int", "int", "int", "int"], fixture="c3dn_glob",
    state=_GS,
    vectors=[(0x00010010, 1, 0, 0), (0x00010010, 0, 0, 0), (0x00020022, 2, 0x1234, 0), (0x00020022, 1, 0, 1),
             (0, 0, 0, 1), (0x7FFF0004, 0x10001, 5, 0), (0xFFFF1234, -1, -1, 0), (0x0ABC0DEF, 1, 7, 0x10002),
             (0x00300030, 0, 9, 0x8000), (0x12345678, 0x7FFF, 0, 0), (1, 1, 1, 1), (2, 0x20000, 3, 0)])

# ======================================================================== group G: Surface GDI wrappers
# Fixture c3dn_surf: twelve Surfaces on jgld's Surface vtable 0x1011d0b0 (S3 / S8 without a memory DC, S5 with a
# DC count of 2 left over), Fonts $F0..$F3 and pointers to them $pF0..$pF3, texts $t0..$t5; the font DC 0x10128724
# set with the first thread frozen.
_SS = [("$surf", 0, 12 * 0x820), ("$implog", 0, 0x400), ("$fonts", 0, 0x100), ("$gfont", 0, 4)]
_S = [f"$S{i}" for i in range(12)]
HOOKS["Surface::setTextColor16"] = dict(
    _J, addr=0x00008D00, abi="thiscall", ret="int", args=["pointer", "int"], fixture="c3dn_surf", state=_SS,
    vectors=[(s, c) for s, c in zip(_S, (0, 0xFFFF, 0x1F, 0x7C00, 0x12345678, -1, 0x8000, 0xF81F, 0x10000, 1,
                                          0x07E0, 0xABCD))])
HOOKS["Surface::setTextColorRGB"] = dict(
    _J, addr=0x00008C40, abi="thiscall", ret="int", args=["pointer", "int", "int", "int"], fixture="c3dn_surf",
    state=_SS,
    vectors=[(s,) + c for s, c in zip(_S, ((0, 0, 0), (255, 255, 255), (1, 2, 3), (0x100, 0x1FF, -1),
                                           (0x80, 0x40, 0x20), (-1, 0, 0x7F), (12, 34, 56), (0x12345678, 0, 0),
                                           (0, 0xABCD, 0), (0, 0, 0x10010), (200, 100, 50), (7, 7, 7)))])
HOOKS["Surface::selectObject"] = dict(
    _J, addr=0x00008AE0, abi="thiscall", ret="int", args=["pointer", "pointer"], fixture="c3dn_surf", state=_SS,
    vectors=[(s, f"$pF{i % 4}") for i, s in enumerate(_S)])
HOOKS["Surface::selectSystemFont"] = dict(
    _J, addr=0x00008B90, abi="thiscall", ret="int", args=["pointer"], fixture="c3dn_surf", state=_SS,
    vectors=[(s,) for s in _S])
HOOKS["Surface::textOut"] = dict(
    _J, addr=0x00008DA0, abi="thiscall", ret="int", args=["pointer", "int", "int", "pointer", "int"],
    fixture="c3dn_surf", state=_SS,
    vectors=[("$S0", 0, 0, "$t0", 5), ("$S1", 10, 20, "$t2", 20), ("$S2", -3, 4, "$t1", 0), ("$S3", 5, 5, "$t3", 2),
             ("$S4", 100, 200, "$t4", 3), ("$S5", 7, 8, "$t5", 30), ("$S6", 0, 0, "$t0", 3),
             ("$S7", 1, 1, "$t2", 7), ("$S8", 2, 2, "$t3", 1), ("$S9", 640, 480, "$t5", 12),
             ("$S10", -1, -1, "$t0", 4), ("$S11", 9, 9, "$t4", 5)])
HOOKS["Font::textWidth"] = dict(
    _J, addr=0x00068E60, abi="thiscall", ret="int", args=["pointer", "pointer", "int"], fixture="c3dn_surf",
    state=_SS,
    vectors=[("$F0", "$t0", 5), ("$F1", "$t2", 20), ("$F2", 0, 5), ("$F3", "$t3", 2), ("$F0", "$t5", 30),
             ("$F1", "$t1", 0), ("$F2", "$t4", 3), ("$F3", 0, 0), ("$F0", "$t2", 1), ("$F1", "$t5", 12),
             ("$F2", "$t0", 4), ("$F3", "$t4", 5)])

# ======================================================================== groups C / H: tracked objects
# Fixture c3dn_obj_<shape>: twelve Surface-shaped objects $o0..$o11 in $blk, the module's tracked list 0x10128428
# swapped (first thread frozen) for a list of our own holding none ('empty'), $o0..$o4 ('five', current node 2) or all
# twelve ('all'); jgld's operator new / malloc / calloc carve from $arena, operator delete / free only record into
# $freelog; the critical-section and GDI imports are stood in ($implog). c3dn_obj_oom: 'five' with the first
# allocation of each vector failing.
_OS = [("$blk", 0, 0x8000), ("$glist", 0, 0x1C), ("$arena", 0, 0x1010), ("$freelog", 0, 0x404),
       ("$implog", 0, 0x400)]
_OB = [f"$o{i}" for i in range(12)]
_OW = [f"$w{i}" for i in range(12)]
for _shape in ("empty", "five"):
    _fx = f"c3dn_obj_{_shape}"
    for _name, _addr in (("SurfaceBase::ctor", 0x00009690), ("SpriteBase::ctor", 0x00014C70),
                         ("PaletteBase::ctor", 0x0006A260), ("FontBase::FontBase", 0x00069220),
                         ("Array_e0::ctor", 0x00067FA0), ("Array_Rec14::ctor", 0x000680A0),
                         ("Array_Rec94::ctor", 0x00068300)):
        HOOKS[f"{_name}_{_shape}"] = dict(_J, addr=_addr, abi="thiscall", ret="pointer", args=["pointer"],
                                          fixture=_fx, state=_OS, vectors=[(o,) for o in _OB])
    for _name, _addr in (("Palette::ctor", 0x0006A150), ("Font::Font", 0x00068660),
                         ("Surface::ctorOwner", 0x00007970)):
        HOOKS[f"{_name}_{_shape}"] = dict(
            _J, addr=_addr, abi="thiscall", ret="pointer", args=["pointer", "pointer"], fixture=_fx, state=_OS,
            vectors=[(o, (w, 0, 0x12345678, w)[i % 4]) for i, (o, w) in enumerate(zip(_OB, _OW))])
HOOKS["Array_Rec14::clearRet"] = dict(
    _J, addr=0x000685C0, abi="thiscall", ret="int", args=["pointer"], fixture="c3dn_obj_empty", state=_OS,
    vectors=[(o,) for o in _OB])
for _shape in ("live", "pat", "five"):
    HOOKS[f"constructTrackedList_{_shape}"] = dict(
        _J, addr=0x000069B0, abi="default", ret="pointer", args=[], fixture=f"c3dn_list_{_shape}",
        state=[("$blk", 0, 0x400), ("$glist", 0, 0x1C)], vectors=[()])
HOOKS["Surface::release"] = dict(
    _J, addr=0x00007CC0, abi="thiscall", ret="void", args=["pointer"], fixture="c3dn_obj_empty", state=_OS,
    vectors=[(o,) for o in _OB])
for _shape in ("five", "all"):
    _fx = f"c3dn_obj_{_shape}"
    HOOKS[f"Surface::dtor_{_shape}"] = dict(
        _J, addr=0x00007BF0, abi="thiscall", ret="void", args=["pointer"], fixture=_fx, state=_OS,
        vectors=[(o,) for o in _OB])
    for _name, _addr in (("Surface::deletingDtor", 0x00007900), ("SurfaceBase::deletingDtor", 0x0000A4E0),
                         ("SpriteBase::deletingDtor", 0x00016930)):
        HOOKS[f"{_name}_{_shape}"] = dict(
            _J, addr=_addr, abi="thiscall", ret="pointer", args=["pointer", "uint32"], fixture=_fx, state=_OS,
            vectors=[(o, (1, 0, 3, 2, 0xFFFFFFFF, 0x80000000)[i % 6]) for i, o in enumerate(_OB)])
# Sprite::deletingDtor over c3dg's Sprite set (fixture c3dn_spr_<shape>; the sprite counters 0x101284c8 are $gcount)
_SPS = [("$blk", 0, 0x2000), ("$glist", 0, 0x1C), ("$gcount", 0, 8), ("$arena", 0, 0x410), ("$freelog", 0, 0x404),
        ("$implog", 0, 0x400)]
for _shape in ("five", "all"):
    HOOKS[f"Sprite::deletingDtor_{_shape}"] = dict(
        _J, addr=0x00014C00, abi="thiscall", ret="pointer", args=["pointer", "uint32"],
        fixture=f"c3dn_spr_{_shape}", state=_SPS,
        vectors=[(f"$p{i}", (1, 0, 3, 2, 0xFFFFFFFF, 0x80000000)[i % 6]) for i in range(12)])
# the factories: `this` (the factory) is never read; _new allocates, _oom fails the first allocation
for _k, _fx in (("new", "c3dn_obj_five"), ("oom", "c3dn_obj_oom"), ("empty", "c3dn_obj_empty")):
    HOOKS[f"Factory::createFont_{_k}"] = dict(
        _J, addr=0x00067850, abi="thiscall", ret="pointer", args=["pointer", "pointer"], fixture=_fx, state=_OS,
        vectors=[("$blk", "$w0"), ("$blk", 0), (0, "$w3"), ("$o5", 0x7FFFFFFF)])
    HOOKS[f"Factory::createSurface_{_k}"] = dict(
        _J, addr=0x000679D0, abi="thiscall", ret="pointer", args=["pointer", "pointer", "int"], fixture=_fx,
        state=_OS, vectors=[("$blk", "$w0", 0), ("$blk", 0, 1), (0, "$w3", -1), ("$o5", 0x7FFFFFFF, 7)])

# ======================================================================== group P: libpng helpers
# Fixture c3dn_png_<mode>: png_structs $P0..$P5 (c3dk's png_read_destroy layout, chunk names at +0x11c), info
# structs $J0..$J3, pointer-to-pointer slots $qP<i> / $qJ<j> / $qN, messages $m0..$m7. 'handler': +0x44 is a cdecl
# recorder of the message text ($hlog); 'default': +0x44 NULL, so png_default_warning's fprintf reaches jgld's CRT
# _write and the stood-in WriteFile ($implog); the stderr FILE 0x101233f0 is $giob.
_PS = [("$blk", 0, 0x2000), ("$arena", 0, 0x110), ("$freelog", 0, 0x404), ("$hlog", 0, 0x200)]
_PSD = [("$blk", 0, 0x2000), ("$hlog", 0, 0x200), ("$implog", 0, 0x1000), ("$giob", 0, 0x20), ("$gbuf", 0, 0x1000)]
_PW = [(f"$P{i % 6}", f"$m{i % 8}") for i in range(12)]
HOOKS["png_warning"] = dict(
    _J, addr=0x00079040, abi="default", ret="int", args=["pointer", "pointer"], fixture="c3dn_png_handler",
    state=_PS, vectors=_PW)
HOOKS["png_warning_default"] = dict(
    _J, addr=0x00079040, abi="default", ret="int", args=["pointer", "pointer"], fixture="c3dn_png_default",
    state=_PSD, vectors=_PW[:6])
HOOKS["png_chunk_warning"] = dict(
    _J, addr=0x00079250, abi="default", ret="int", args=["pointer", "pointer"], fixture="c3dn_png_handler",
    state=_PS, vectors=_PW)
HOOKS["png_chunk_warning_default"] = dict(
    _J, addr=0x00079250, abi="default", ret="int", args=["pointer", "pointer"], fixture="c3dn_png_default",
    state=_PSD, vectors=_PW[:4])
HOOKS["png_default_warning"] = dict(
    _J, addr=0x00079300, abi="default", ret="int", args=["pointer", "pointer"], fixture="c3dn_png_default",
    state=_PSD, vectors=[((0, "$P1", "$P2")[i % 3], f"$m{i % 8}") for i in range(12)])
HOOKS["png_info_destroy"] = dict(
    _J, addr=0x00078980, abi="default", ret="void", args=["pointer", "pointer"], fixture="c3dn_png_handler",
    state=_PS,
    vectors=[("$P0", "$J0"), ("$P1", "$J1"), ("$P2", "$J2"), ("$P3", "$J3"), ("$P4", "$J1"), ("$P5", "$J2"),
             ("$P0", "$J3"), ("$P1", "$J0"), (0, "$J1"), (0, "$J3"), ("$P2", "$J1"), ("$P3", "$J2")])
HOOKS["png_destroy_read_struct"] = dict(
    _J, addr=0x0006D150, abi="default", ret="void", args=["pointer", "pointer", "pointer"],
    fixture="c3dn_png_handler", state=_PS,
    vectors=[("$qP0", "$qJ0", "$qJ1"), ("$qP1", 0, 0), ("$qP2", "$qJ2", 0), ("$qP3", 0, "$qJ3"),
             ("$qP4", "$qJ1", "$qJ2"), ("$qP5", "$qJ3", "$qJ0"), ("$qP0", "$qN", "$qN"), ("$qP2", "$qJ1", "$qJ3"),
             ("$qP5", 0, "$qJ2"), ("$qP3", "$qJ0", 0), ("$qP1", "$qJ2", "$qN"), ("$qP4", "$qN", "$qJ1")])

# ======================================================================== group D: Surface dispatchers, setPalette
# Fixture c3dn_ce[_frozen]: c3ce's sixteen surfaces $s0..$s15 and rectangles (copied), four more Palettes $pal3..$pal6
# (+0x810 = 0x11, 0x22, 0x22, 0); the whole block is state. _frozen holds the first thread and writes back the row
# skip / pixel count words the primitives park (0x1012847c, 0x10128458, 0x10128450).
_CS = [("$blk", 0, 0x10200), ("$implog", 0, 0x800)]
HOOKS["Surface::fillRectDispatch"] = dict(
    _J, addr=0x00009770, abi="thiscall", ret="int", args=["pointer", "pointer", "int"], fixture="c3dn_ce_frozen",
    state=_CS,
    vectors=[("$s0", "$r_full", 0x11), ("$s0", "$r_odd", 0x12), ("$s0", "$r_neg", 0x13), ("$s0", "$r_out", 0x14),
             ("$s1", "$r_full", 0x15), ("$s2", "$r_s19", 0x16), ("$s12", "$r_s12", 0x17),
             ("$s3", "$r_full", 0x18), ("$s3", "$r_in", 0x8000ABCD), ("$s4", "$r_s16", 0x19),
             ("$s11", "$r_in", 0x1A), ("$s6", "$r_full", 0x1B), ("$s7", "$r_s24", 0x1C), ("$s8", "$r_s32", 0x1D),
             ("$s14", "$r_s16", 0x1E), ("$s0", "$r_7x5", 0x1F)])
HOOKS["Surface::drawDashedLine"] = dict(
    _J, addr=0x000095A0, abi="thiscall", ret="int",
    args=["pointer", "int", "int", "int", "int", "int", "int", "int", "int", "int"], fixture="c3dn_ce_frozen",
    state=_CS,
    vectors=[("$s0", 6, 3, 18, 10, 0x11, 0x22, 3, 2, 0), ("$s0", 6, 3, 18, 10, -1, 0x22, 3, 2, 1),
             ("$s0", 9, 3, 9, 10, 0x33, 0x44, 2, 2, 0), ("$s0", 6, 5, 18, 5, 0x11, 0x22, 1, 1, 0),
             ("$s1", 0, 0, 31, 15, 0x55, 0x66, 4, 3, 2), ("$s2", 0, 10, 18, 0, 0x77, 0x88, 5, 3, -3),
             ("$s3", 0, 0, 31, 15, 0x11, 0x22, 3, 2, 0), ("$s3", 4, 6, 20, 6, 0x33, 0x44, 2, 2, 1),
             ("$s4", 0, 9, 15, 0, 0x55, 0x66, 4, 1, 0), ("$s11", 6, 4, 18, 10, 0x80001234, 0x22, 3, 3, 2),
             ("$s6", 0, 0, 31, 15, 0x11, 0x22, 3, 2, 0), ("$s7", 0, 0, 9, 7, 0x11, 0x22, 3, 2, 0),
             ("$s14", 0, 0, 15, 7, 0x11, 0x22, 3, 2, 0)])
HOOKS["Surface::setPalette"] = dict(
    _J, addr=0x000089D0, abi="thiscall", ret="pointer", args=["pointer", "pointer"], fixture="c3dn_ce", state=_CS,
    vectors=[("$s0", "$pal4"), ("$s0", "$pal3"), ("$s1", "$pal3"), ("$s2", "$pal6"), ("$s3", "$pal5"),
             ("$s4", "$pal3"), ("$s11", "$pal4"), ("$s5", 0), ("$s0", 0), ("$s2", "$pal4"), ("$s4", "$pal6"),
             ("$s11", "$pal5")])
