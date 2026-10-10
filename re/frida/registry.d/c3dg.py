# Batch c3dg (round 25, jgld.dll): rows c3dc dropped for Win32 imports, plus the MappedFile writers c3cy left.
# Reimplementations and instruction citations: shim/src/re/c3dg.cpp; fixtures: re/frida/js/fixtures.d/c3dg.js;
# Purposes and per-jcc branch tables: log/c3/c3dg_purpose.md; what was not taken: log/c3/c3dg_notes.txt.
# addr is the RVA (VA = 0x10000000 + addr).
#
# Every key runs under the import stand-in (re/frida/js/importstandin.js): the imports the function calls are
# recording stubs for the key and `$implog` (the call sequence and arguments) is compared state. Keys that write a
# module global freeze the game's first thread (re/frida/js/freeze.js), which writes the game's value back before
# resuming; keys that allocate run under the heap stand-in (arena and free log compared).
#
# Callees hooked by THIS batch (re-run these keys with them off: SIMGOLF_HOOKS_OFF=jgld.dll:<rva>,...):
#   onPaletteChanged_*        -> 699c0 realizePalette
#   createPalette_*           -> 6ad60 deleteLogPalette
#   Display::createPalette_*  -> 6aa70 createPalette
#   Display::initFonts_*      -> 68f30 initFonts
#   Factory::createSprite_*   -> 14cd0 Sprite::ctorN
#   Sprite::create_*          -> 1b690 Sprite::create8, 1a250 Sprite::create16
# Callees at C3 in other batches (not linked in the c3dg instance; the verifier runs them both ways):
#   MappedFile::*             -> 2e10 MappedFile::close (c3cy)
#   createPalette             -> 14a40 jalloc, 14a90 jfree (c3cn)
#   Surface::stretchTo, Display::present -> 9120 rectWidth, 9160 rectHeight, 8590 intersect (present)
#   Display::enumModes        -> 66ac0 / 66b90 Array resets, 681a0 / 68400 Array adds (c3bb, c3cn keys)
#   Sprite::ctorN             -> (SpriteBase::ctor 14c70 is C2) -> 6ab0 Tracked::ctor -> 6f40 LinkedList::add (c3dc)
#   Sprite::dtor              -> 14eb0 Sprite::release (c3dc), 168e0 SpriteBase::~SpriteBase (c3dc)
#   Factory::createPalette    -> (Palette::ctor 6a150, PaletteBase::ctor 6a260 are C2) -> 6ab0 Tracked::ctor
#   Sprite::create8 / create16 -> 14eb0 Sprite::release (slot +8, c3dc), 14a40 jalloc / 14a90 jfree (c3cn)
#   Sprite::create            -> 17eb0 Sprite::setField10 (slot +0x10, C3)

_LOG = ("$implog", 0, 0x400)
_FREE = ("$freelog", 0, 0x404)

HOOKS = {}

# ======================================================================== group M: MappedFile writers
_MF = [("$mfblk", 0, 0x20 * 12), ("$views", 0, 0x100 * 12), _LOG]
# openWrite (this, name, sequential): names n0..n11 = modes 4 1 2 3 4 4 1 2 3 4 4 2 (fixture), every exit of the
# body with both flag words, behind objects whose leading close has something to release.
HOOKS["MappedFile::openWrite"] = dict(
    module="jgld.dll", addr=0x00002A20, abi="thiscall", ret="pointer", args=["pointer", "pointer", "int"],
    fixture="c3dg_mf_open", state=_MF,
    vectors=[("$mf0", "$n0", 0), ("$mf7", "$n1", 1), ("$mf3", "$n2", 0), ("$mf5", "$n3", 1), ("$mf11", "$n4", 1),
             ("$mf2", "$n5", 0), ("$mf4", "$n6", 0), ("$mf6", "$n7", 1), ("$mf9", "$n8", 0), ("$mf1", "$n9", -1),
             ("$mf8", "$n10", 0x100), ("$mf10", "$n11", 1)])
# create (this, name, size, sequential): names cn0..cn11 = modes 4 1 2 3 5 6 1 5 6 4 3 4; sizes up to the 0x100
# view the fixture hands out (memset writes it).
HOOKS["MappedFile::create"] = dict(
    module="jgld.dll", addr=0x00002BD0, abi="thiscall", ret="pointer",
    args=["pointer", "pointer", "uint32", "int"], fixture="c3dg_mf_open", state=_MF,
    vectors=[("$mf0", "$cn0", 0x40, 0), ("$mf7", "$cn1", 0x10, 1), ("$mf3", "$cn2", 0x80, 0),
             ("$mf5", "$cn3", 0x20, 1), ("$mf11", "$cn4", 0x30, 0), ("$mf2", "$cn5", 0x44, 1),
             ("$mf4", "$cn6", 0, 0), ("$mf6", "$cn7", 0, 1), ("$mf9", "$cn8", 0x100, 0),
             ("$mf1", "$cn9", 0xff, -1), ("$mf8", "$cn10", 1, 7), ("$mf10", "$cn11", 0x63, 0)])
# closeTruncate (this, end): rows t0..t11 with their end pointers e0..e11 (fixture TR_ROWS)
HOOKS["MappedFile::closeTruncate"] = dict(
    module="jgld.dll", addr=0x00002EF0, abi="thiscall", ret="void", args=["pointer", "pointer"],
    fixture="c3dg_mf_trunc", state=_MF, vectors=[(f"$t{i}", f"$e{i}") for i in range(12)])

# ======================================================================== group P: the module palette
_PAL = [("$gpal", 0, 12), _LOG]
_HWND = [0x00010100, 0x00010101, 0x0002a0f3, 0, 1, 0xffffffff, 0x7fff0042, 0x80000000, 0x12345600, 0x00abcdef,
         0x0000ff01, 0x00000100]
for _v in ("pal0", "palX"):
    HOOKS[f"realizePalette_{_v}"] = dict(
        module="jgld.dll", addr=0x000699C0, abi="default", ret="int", args=["pointer"],
        fixture=f"c3dg_realize_{_v}", state=_PAL, vectors=[(h,) for h in _HWND])
    HOOKS[f"onPaletteChanged_{_v}"] = dict(
        module="jgld.dll", addr=0x00069A70, abi="default", ret="void", args=["pointer", "pointer"],
        fixture=f"c3dg_realize_{_v}", state=_PAL,
        vectors=[(0x10101, 0x10101), (0x10101, 0x10102), (0, 0), (0, 1), (0x2a0f3, 0), (0xffffffff, 0xffffffff),
                 (0xffffffff, 0), (0x100, 0x200), (0x7fff0042, 0x7fff0042), (0x12345600, 0x12345601),
                 (0x80000000, 0x80000000), (0xabcdef, 0x1abcdef)])
# argument-free: one key per value of the palette global (0x10128738) and DeleteObject result
for _k in range(10):
    HOOKS[f"deleteLogPalette_{_k}"] = dict(
        module="jgld.dll", addr=0x0006AD60, abi="default", ret="void", args=[],
        fixture=f"c3dg_deletepal_{_k}", state=_PAL, vectors=[()])
_CP = [("$clients", 0, 0x200), ("$gpal", 0, 12), ("$arena", 0, 0x430), _FREE, _LOG]
_CPV = [("$c0", "$c1"), ("$c2", "$c3"), ("$c4", "$c4"), ("$c7", "$c0"), ("$c5", "$c6"), ("$c1", "$c2")]
for _v in ("ok", "null", "oom"):
    HOOKS[f"createPalette_{_v}"] = dict(
        module="jgld.dll", addr=0x0006AA70, abi="default", ret="int", args=["pointer", "pointer"],
        fixture=f"c3dg_createpal_{_v}", state=_CP, vectors=_CPV)
    HOOKS[f"Display::createPalette_{_v}"] = dict(
        module="jgld.dll", addr=0x00067E80, abi="thiscall", ret="int", args=["pointer", "pointer", "pointer"],
        fixture=f"c3dg_createpal_{_v}", state=_CP, vectors=[("$c6",) + v for v in _CPV[:3]] + [(0,) + _CPV[3]])

# ======================================================================== group F: the font DC
_FONT = [("$gfont", 0, 4), _LOG]
for _v in ("0_0", "0_dc", "dc_0", "dc_dc", "m1_1", "1_m1"):
    HOOKS[f"initFonts_{_v}"] = dict(
        module="jgld.dll", addr=0x00068F30, abi="default", ret="int", args=[],
        fixture=f"c3dg_fonts_{_v}", state=_FONT, vectors=[()])
    HOOKS[f"Display::initFonts_{_v}"] = dict(
        module="jgld.dll", addr=0x00067E00, abi="thiscall", ret="int", args=["pointer"],
        fixture=f"c3dg_fonts_{_v}", state=_FONT, vectors=[("$implog",)])

# ======================================================================== group B: GDI blits
# Surfaces s0..s7 (DC, bounds): s2 and s5 hand out DC 0. RECTs r0..r11 (fixture RECTS).
_SURF = [("$surfs", 0, 0x40 * 8), ("$rects", 0, 0x10 * 12), _LOG]
HOOKS["Surface::stretchTo"] = dict(
    module="jgld.dll", addr=0x00008F70, abi="thiscall", ret="int",
    args=["pointer", "pointer", "pointer", "pointer"], fixture="c3dg_stretch", state=_SURF,
    vectors=[("$s0", "$s1", "$r0", "$r1"), ("$s0", 0, "$r0", "$r1"), ("$s0", "$s1", 0, "$r1"),
             ("$s0", "$s1", "$r0", 0), ("$s0", "$s2", "$r0", "$r1"), ("$s2", "$s1", "$r1", "$r2"),
             ("$s3", "$s4", "$r3", "$r4"), ("$s1", "$s0", "$r5", "$r6"), ("$s6", "$s7", "$r7", "$r8"),
             ("$s4", "$s4", "$r9", "$r10"), ("$s7", "$s3", "$r11", "$r0"), ("$s5", "$s0", "$r2", "$r3")])
# Displays d0..d3 present surfaces s0, s1, s3, s4; the palette client 0x1012873c is $c3 (its +4 goes to slot +0xec)
HOOKS["Display::present"] = dict(
    module="jgld.dll", addr=0x00066470, abi="thiscall", ret="int", args=["pointer", "pointer"],
    fixture="c3dg_present",
    state=[("$disps", 0, 0x800), ("$surfs", 0, 0x40 * 8), ("$rects", 0, 0x10 * 12), ("$clients", 0, 0x200),
           ("$gpal", 0, 12), _LOG],
    vectors=[("$d0", 0), ("$d0", "$r1"), ("$d0", "$r6"), ("$d1", 0), ("$d1", "$r3"), ("$d2", "$r2"),
             ("$d2", 0), ("$d3", "$r9"), ("$d3", "$r2"), ("$d0", "$r5"), ("$d1", "$r8"), ("$d3", 0)])

# ======================================================================== group K: onKey
# (hwnd, key, down, x, y) as windowProc passes them; PeekMessageA answers 1 for the first (hwnd & 0xf) calls; the
# display's pump slot answers 1 for the first d calls (keys _d0 / _d3).
_KEYV = [(0x1203, 0x41, 1, 5, 6), (0x1205, 0x60, 1, 0, 0), (0x1202, 0x6F, 1, 1, 1), (0x1200, 0x65, 1, 2, 2),
         (0x1207, 0x5F, 1, 3, 3), (0x1203, 0x70, 1, 4, 4), (0x1204, 0x60, 0, 5, 5), (0x1201, 0x41, 0, 6, 6),
         (0x120F, 0x6A, 1, 7, 7), (0x1203, 0xFFFFFFFF, 1, 8, 8), (0x1203, 0x80000060, 2, 9, 9), (0x1206, 0, -1, 0, 1)]
for _d in ("d0", "d3"):
    HOOKS[f"onKey_{_d}"] = dict(
        module="jgld.dll", addr=0x00069C70, abi="default", ret="void",
        args=["pointer", "uint32", "int", "int", "uint32"], fixture=f"c3dg_key_{_d}",
        state=[("$sink", 0, 0x100), ("$disp", 0, 0x40), _LOG], vectors=_KEYV)

# ======================================================================== group E: enumModes
# modes answered (0, 1, 3, 7) and the arrays' starting shape: count to be reset, room or growth, NULL data.
for _v in ("0", "1", "3grow", "7null"):
    HOOKS[f"Display::enumModes_{_v}"] = dict(
        module="jgld.dll", addr=0x00065A90, abi="thiscall", ret="pointer", args=["pointer"],
        fixture=f"c3dg_modes_{_v}",
        state=[("$g14", 0, 0x14), ("$g94", 0, 0x14), ("$mdata", 0, 0x800), ("$arena", 0, 0x2010), _FREE, _LOG],
        vectors=[(0,), ("$mdata",)])

# ======================================================================== group S: Sprite construction
_S = [("$blk", 0, 0x2000), ("$glist", 0, 0x1C), ("$gcount", 0, 8), ("$arena", 0, 0x410), _FREE, _LOG]
_SB = [("$blk", 0, 0x2000), ("$glist", 0, 0x1C), ("$gcount", 0, 8), ("$arena", 0, 0x1010), _FREE, _LOG]
_CTORV = [("$p0", "$w0"), ("$p1", 0), ("$p5", "$w5"), ("$p11", "$w11"), ("$p7", 0), ("$p2", "$w9")]
for _v in ("empty", "five"):
    HOOKS[f"Sprite::ctorN_{_v}"] = dict(
        module="jgld.dll", addr=0x00014CD0, abi="thiscall", ret="pointer", args=["pointer", "pointer"],
        fixture=f"c3dg_spr_{_v}", state=_S, vectors=_CTORV)
for _v in ("empty", "five", "all"):
    HOOKS[f"Sprite::dtor_{_v}"] = dict(
        module="jgld.dll", addr=0x00014DE0, abi="thiscall", ret="void", args=["pointer"],
        fixture=f"c3dg_spr_{_v}", state=_S, vectors=[(f"$p{i}",) for i in range(12)])
for _v in ("new", "new_oom"):
    HOOKS[f"Factory::createSprite_{_v}"] = dict(
        module="jgld.dll", addr=0x00067A90, abi="thiscall", ret="pointer", args=["pointer", "pointer", "uint32"],
        fixture=f"c3dg_spr_{_v}", state=_SB,
        vectors=[("$blk", "$w0", 0), ("$blk", 0, 1), ("$blk", "$w3", 0xFFFFFFFF), (0, "$w7", 5)])
    HOOKS[f"Factory::createPalette_{_v}"] = dict(
        module="jgld.dll", addr=0x00067910, abi="thiscall", ret="pointer", args=["pointer", "pointer"],
        fixture=f"c3dg_spr_{_v}", state=_SB,
        vectors=[("$blk", "$w0"), ("$blk", 0), ("$blk", "$p3"), (0, 0x12345678)])

# ======================================================================== group C: Sprite::create / create8 / create16
# (this, bits, w, h, bpp, e). Sources $s8a/$s16a mixed, $s8b/$s16b row kinds, $s8c/$s16c no key pixel. Keys:
# _span (0x101284c4 = 1), _flat (0: every vector takes the flat form), _oom (the payload jalloc fails), _spanoom
# (the span-table jalloc fails). Each vector first empties the sprite through slot +8 (Sprite::release).
_C = [("$blk", 0, 0x1000), ("$gspr", 0, 12), ("$arena", 0, 0x22010), _FREE, ("$exlog", 0, 0x104)]
def _cvec(d):
    a, b, c = f"$s{d}a", f"$s{d}b", f"$s{d}c"
    return [("$q0", a, 16, 8, d, 0), ("$q1", b, 12, 6, d, 0), ("$q2", c, 8, 4, d, 0), ("$q3", 0, 20, 5, d, 0),
            ("$q4", a, 4, 8, d, 0), ("$q5", a, 5, 3, d, 0), ("$q6", a, 0xFF, 2, d, 0), ("$q7", a, 0x100, 2, d, 0),
            ("$q8", a, 0x10, 0x1000, d, 0), ("$q9", b, 7, 0, d, 0), ("$q10", a, 9, 9, d, 0), ("$q11", b, 33, 3, d, 0)]
_CARGS = ["pointer", "pointer", "uint32", "int", "int", "int"]
for _d, _addr in ((8, 0x0001B690), (16, 0x0001A250)):
    for _v in ("span", "flat", "oom", "spanoom"):
        HOOKS[f"Sprite::create{_d}_{_v}"] = dict(
            module="jgld.dll", addr=_addr, abi="thiscall", ret="int", args=_CARGS,
            fixture=f"c3dg_create{_d}_{_v}", state=_C, vectors=_cvec(_d))
# Sprite::create (this, bits, w, h, bpp, e, key): bpp 8 / 16 / others; _oom makes the depth body return 4
_SCV = [("$q0", "$s8a", 16, 8, 8, 0, 0x11), ("$q1", "$s16b", 12, 6, 16, 0, 0x22), ("$q2", "$s8c", 8, 4, 8, 3, 0x33),
        ("$q3", "$s16a", 6, 6, 16, 1, -1), ("$q4", "$s8a", 9, 2, 0, 0, 0x44), ("$q5", "$s8a", 9, 2, 24, 0, 0x55),
        ("$q6", 0, 10, 10, 8, 0, 0x66), ("$q7", "$s8b", 64, 4, 32, 0, 0x77), ("$q8", "$s16c", 5, 5, 16, 0, 0),
        ("$q9", "$s8b", 70, 3, 8, 7, 0x99), ("$q10", "$s8a", 9, 9, 7, 0, 0xAA), ("$q11", "$s16a", 255, 1, 16, 0, 0xBB)]
for _v in ("span", "oom"):
    HOOKS[f"Sprite::create_{_v}"] = dict(
        module="jgld.dll", addr=0x00014FF0, abi="thiscall", ret="int", args=_CARGS + ["int"],
        fixture=f"c3dg_create8_{_v}", state=_C, vectors=_SCV)
