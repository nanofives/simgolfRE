# Batch c3dc (round 24, jgld.dll): C2 rows earlier batches dropped for the heap, drawing or live state.
# Reimplementations and instruction citations: shim/src/re/c3dc.cpp; fixtures: re/frida/js/fixtures.d/c3dc.js;
# Purposes and per-jcc branch tables: log/c3/c3dc_purpose.md; what was not taken: log/c3/c3dc_notes.txt.
# addr is the RVA (VA = 0x10000000 + addr).
#
# Every key runs under the heap stand-in (jgld operator new/malloc/calloc from a compared arena, operator delete
# and free recorded in a compared log). Keys that write a module global freeze the game's first thread
# (re/frida/js/freeze.js), which writes the game's value back before resuming.

_HEAP = [("$arena", 0, 0x410), ("$freelog", 0, 0x404)]
_HEAP_S = [("$arena", 0, 0x110), ("$freelog", 0, 0x404)]

# flags of a scalar deleting destructor: only bit 0 decides (`and eax, 1` at 0x10067518).
_FLAGS = [0x00000001, 0x00000000, 0x00000003, 0x00000002, 0x00000005, 0x00000004,
          0x80000001, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE, 0x00000101, 0x7FFFFFFF]

HOOKS = {}

# ======================================================================== group T: LinkedList::add
# Eight synthetic lists (fixture header): L0/L1 empty (ordered 0 / 1), L2/L3 one node (ordered 0 / 1),
# L4/L5 three nodes (ordered 0 / 0x7fffffff), L6 five nodes, L7 count 2 with no head. The debug message goes to
# the recorder $dbg that stands in for the import slot [0x1012c42c].
HOOKS["LinkedList::add"] = dict(
    module="jgld.dll", addr=0x00006F40, abi="thiscall", ret="void", args=["pointer", "pointer", "int"],
    fixture="c3dc_lists", state=[("$blk", 0, 0x2000), ("$dbg", 0, 8)] + _HEAP,
    vectors=[("$L0", 0x11110000, 0), ("$L0", 0x11110004, 1), ("$L1", 0x22220000, 0x7F),
             ("$L2", 0x33330000, 0), ("$L2", 0x33330004, 0x80), ("$L3", 0x44440000, 2),
             ("$L4", 0x55550000, 0xFF), ("$L4", 0x55550004, 0x1234), ("$L5", 0x66660000, 3),
             ("$L5", 0, 0), ("$L6", 0x77770000, 0x41), ("$L6", "$L0", 5), ("$L7", 0x88880000, 6),
             ("$L3", 0x99990000, -1)])

# ======================================================================== group T: the swapped tracked list
# c3cw's four shapes: empty, one ($o0 registered), five ($o0..$o4, current node 2), fivehead (current = head).
_OBJ = [f"$o{i}" for i in range(12)]
_T = [("$blk", 0, 0x1000), ("$glist", 0, 0x1C)] + _HEAP
for _shape in ("empty", "one", "five", "fivehead"):
    _fx = f"c3dc_tracked_{_shape}"
    HOOKS.update({
        f"Tracked::ctor_{_shape}": dict(module="jgld.dll", addr=0x00006AB0, abi="thiscall", ret="pointer",
                                        args=["pointer"], fixture=_fx, state=_T, vectors=[(o,) for o in _OBJ]),
        f"SurfaceBase::dtor_{_shape}": dict(module="jgld.dll", addr=0x000096F0, abi="thiscall", ret="void",
                                            args=["pointer"], fixture=_fx, state=_T, vectors=[(o,) for o in _OBJ]),
        f"SpriteBase::~SpriteBase_{_shape}": dict(module="jgld.dll", addr=0x000168E0, abi="thiscall", ret="void",
                                                  args=["pointer"], fixture=_fx, state=_T,
                                                  vectors=[(o,) for o in _OBJ]),
    })

# ======================================================================== Tracked::deleteAll
# Argument-free: one call per key, one key per list of real Tracked-class objects (fixture header).
_D = [("$blk", 0, 0x6000), ("$glist", 0, 0x1C)] + _HEAP
for _shape in ("empty", "one", "three", "five", "fivecur3"):
    HOOKS[f"Tracked::deleteAll_{_shape}"] = dict(
        module="jgld.dll", addr=0x00006BF0, abi="default", ret="void", args=[],
        fixture=f"c3dc_delall_{_shape}", state=_D, vectors=[()])

# ======================================================================== Lib::destroyDisplay
# `this` is never read: the vectors pass three different values and must give one result per key.
for _v in ("null_empty", "base_empty", "base_three", "null_five", "stub_one"):
    HOOKS[f"Lib::destroyDisplay_{_v}"] = dict(
        module="jgld.dll", addr=0x00065420, abi="thiscall", ret="void", args=["pointer"],
        fixture=f"c3dc_destroy_{_v}", state=_D + [("$gdisp", 0, 4)],
        vectors=[(0,), ("$blk",), (0x12345678,)])

# ======================================================================== atexitDestroyGlobalList
for _v in ("set_five", "setff_one", "clear_empty", "clear_one", "clear_five"):
    HOOKS[f"atexitDestroyGlobalList_{_v}"] = dict(
        module="jgld.dll", addr=0x00006A40, abi="default", ret="void", args=[],
        fixture=f"c3dc_atexit_{_v}",
        state=[("$blk", 0, 0x1000), ("$glist", 0, 0x1C), ("$gflag", 0, 1)] + _HEAP, vectors=[()])

# ======================================================================== DisplayBase destructors
_DB = [("$blk", 0, 0x400)] + _HEAP_S
HOOKS["DisplayBase::dtor"] = dict(
    module="jgld.dll", addr=0x00066D20, abi="thiscall", ret="void", args=["pointer"],
    fixture="c3dc_display_objs", state=_DB, vectors=[(f"$d{i}",) for i in range(12)])
HOOKS["DisplayBase::deletingDtor"] = dict(
    module="jgld.dll", addr=0x000674F0, abi="thiscall", ret="pointer", args=["pointer", "uint32"],
    fixture="c3dc_display_objs", state=_DB, vectors=[(f"$d{i}", _FLAGS[i]) for i in range(12)])

# ======================================================================== group S: Sprite::release
HOOKS["Sprite::release"] = dict(
    module="jgld.dll", addr=0x00014EB0, abi="thiscall", ret="void", args=["pointer"],
    fixture="c3dc_release", state=[("$blk", 0, 0x1000), ("$gcount", 0, 8)] + _HEAP_S,
    vectors=[(f"$r{i}",) for i in range(12)])

# ======================================================================== group C: the pixel-format helpers
# One key per value the Display's slot +0xb4 returns (1 takes the RGB565 arm; 0, 2 and -1 the other).
_RGB = [(0, 0, 0), (255, 255, 255), (0xF8, 0, 0), (0, 0xFC, 0), (0, 0, 0xF8), (7, 3, 7), (8, 4, 8),
        (0x123, 0x456, 0x789), (-1, -1, -1), (0x80, 0x40, 0x20), (0x7F, 0x81, 0xC3), (0x10, 0x20, 0x30)]
_COL = [0, 1, 0x7FFF, 0x8000, 0xFFFF, 0xF800, 0x07E0, 0x001F, 0x7C00, 0x03E0, -1, -0x8000, 0x12345678, 0x5A5A]
for _v in ("1", "0", "2", "m1"):
    _st = [("$disp", 0, 0xC)]
    HOOKS[f"makeColor_fmt{_v}"] = dict(module="jgld.dll", addr=0x0000A660, abi="default", ret="int",
                                       args=["int", "int", "int"], fixture=f"c3dc_fmt{_v}", state=_st,
                                       vectors=_RGB)
    HOOKS[f"redOf_fmt{_v}"] = dict(module="jgld.dll", addr=0x0000E440, abi="default", ret="int", args=["int"],
                                   fixture=f"c3dc_fmt{_v}", state=_st, vectors=[(c,) for c in _COL])
    HOOKS[f"greenOf_fmt{_v}"] = dict(module="jgld.dll", addr=0x0000E4C0, abi="default", ret="int", args=["int"],
                                     fixture=f"c3dc_fmt{_v}", state=_st, vectors=[(c,) for c in _COL])

# ======================================================================== group B: Surface copy dispatchers
# c3bh's surface set plus c3bc's shade table (fixture c3dc_surfaces). $pix holds all sixteen pixel slices and
# $l0..$l15 the lock windows (+0x4c0 base, +0x4c8 use count, +0x4cc current base), so a missing or extra release
# reads RED; $rects holds the RECTs blit16from16 intersects in place.
_SURF = [("$pix", 0, 16 * 0x1000)] + [(f"$l{i}", 0, 0x14) for i in range(16)] + [("$rects", 0, 0x70)]

# blit(other, x, y, sx, sy, w, h, key): this depth 8 -> other depth 8 = blit8from8, 16 = blitKey8to16, else 0x17;
# this depth not 8 -> 0x17 (the argument is not looked at).
HOOKS["Surface::blit"] = dict(
    module="jgld.dll", addr=0x00008E50, abi="thiscall", ret="int",
    args=["pointer", "pointer", "int", "int", "int", "int", "int", "int", "int"],
    fixture="c3dc_surfaces", state=_SURF,
    vectors=[("$s8b", "$s8c", 0, 0, 0, 0, 24, 12, 0x40),       # 8 <- 8: blit8from8, clip trims
             ("$s8b", "$s8a", 2, 1, 3, 2, 17, 8, 0x43),        # 8 <- 8: offsets, cols & 3 == 1
             ("$s8a", "$s8b", -3, -2, 5, 4, 10, 6, 0x42),      # 8 <- 8: both adjusted
             ("$s8a", "$s8nb", 0, 0, 0, 0, 16, 8, 0x42),       # 8 <- 8: the argument has no base -> 3
             ("$s8a", "$s8b", 0, 0, 100, 100, 8, 8, 0x42),     # 8 <- 8: empty intersection
             ("$s8a", "$d16a", 0, 0, 0, 0, 32, 16, 0x40),      # 8 -> 16: blitKey8to16
             ("$s8a", "$d16b", 2, 2, 4, 3, 10, 5, 0x45),       # 8 -> 16: clip inside
             ("$s8a", "$d16c", 0, 0, 0, 0, 19, 11, 0x44),      # 8 -> 16: format 1 (RGB555 table)
             ("$s8b", "$d16c", 0, 0, 0, 0, 19, 11, 0x44),      # 8 -> 16: the source's OTHER palette: no match
             ("$s8a", "$d16d", 0, 0, 0, 0, 16, 8, 0x42),       # 8 -> 16: format 2 -> 1
             ("$s8a", "$d16f", 0, 0, 0, 0, 16, 8, 0x42),       # 8 -> 16: no base -> 3
             ("$s8a", "$s12", 0, 0, 0, 0, 8, 8, 0x40),         # 8 -> 12: 0x17
             ("$s8a", "$x24", 0, 0, 0, 0, 8, 8, 0x40),         # 8 -> 24: 0x17
             ("$s8a", "$x0", 0, 0, 0, 0, 8, 8, 0x40),          # 8 -> 0: 0x17
             ("$d16a", "$s8a", 0, 0, 0, 0, 8, 8, 0x40),        # this 16: 0x17
             ("$s12", "$s8a", 0, 0, 0, 0, 8, 8, 0x40),         # this 12: 0x17
             ("$x24", "$d16a", 0, 0, 0, 0, 8, 8, 0x40),        # this 24: 0x17
             ("$s8a", "$s8b", 0, 0, 0, 0, 0, 0, 0x40),         # 8 <- 8: zero size
             ("$s8a", "$d16p", 1, 1, 3, 2, 12, 6, 0x41)])      # 8 -> 16: offsets on both surfaces

# blitT(dst, sx, sy, dx, dy, w, h): this+0x24 8 -> dst depth 8 = blitT_8to8, 16 = blitT_8to16, else 0x17;
# this+0x24 16 -> blitT_16to16 (dst not looked at, may be NULL); anything else 0x17.
HOOKS["Surface::blitT"] = dict(
    module="jgld.dll", addr=0x000091A0, abi="thiscall", ret="int",
    args=["pointer", "pointer", "int", "int", "int", "int", "int", "int"],
    fixture="c3dc_surfaces", state=_SURF,
    vectors=[("$s8a", "$s8b", 0, 0, 0, 0, 24, 12),     # 8 -> 8: blitT_8to8
             ("$s8a", "$s8c", 2, 2, 4, 3, 10, 5),      # 8 -> 8: clip inside
             ("$m1", "$m1", 12, 8, 4, 2, 8, 5),        # 8 -> 8: the same surface
             ("$s8nb", "$s8b", 0, 0, 0, 0, 8, 8),      # 8 -> 8: this has no base -> 7
             ("$s8a", "$d16a", 0, 0, 0, 0, 32, 16),    # 8 -> 16: blitT_8to16
             ("$s8a", "$d16b", -3, -2, 5, 4, 10, 6),   # 8 -> 16: both adjusted, clip inside
             ("$s8b", "$d16c", 0, 0, 0, 0, 19, 11),    # 8 -> 16: format 1
             ("$mb1", "$mb1", 0, 0, 16, 4, 10, 3),     # 8 -> 8 same surface, dx >= sx, dy >= sy
             ("$s8a", "$d16f", 0, 0, 0, 0, 16, 8),     # 8 -> 16: no base -> 3
             ("$s8a", "$s12", 0, 0, 0, 0, 8, 8),       # 8 -> 12: 0x17
             ("$s8a", "$x24", 0, 0, 0, 0, 8, 8),       # 8 -> 24: 0x17
             ("$d16a", "$d16b", 0, 0, 0, 0, 32, 16),   # 16: blitT_16to16
             ("$d16a", "$d16d", 1, 1, 2, 2, 7, 3),     # 16: odd width
             ("$m16", "$m16", 12, 2, 4, 8, 8, 5),      # 16: same surface, rows bottom to top
             ("$d16a", 0, 0, 0, 0, 0, 8, 8),           # 16: NULL dst (not dereferenced here) -> 3
             ("$d16a", "$s8a", 0, 0, 0, 0, 8, 8),      # 16: an 8-bit dst is not checked by the dispatcher
             ("$s12", "$s8a", 0, 0, 0, 0, 8, 8),       # 12: 0x17
             ("$x24", "$d16a", 0, 0, 0, 0, 8, 8),      # 24: 0x17
             ("$x0", 0, 0, 0, 0, 0, 8, 8)])            # 0: 0x17, NULL dst never read

# blit16(rect, src, alpha, table): this depth 16 and src depth 16 -> blit16from16, else 0x17.
HOOKS["Surface::blit16"] = dict(
    module="jgld.dll", addr=0x000098F0, abi="thiscall", ret="int",
    args=["pointer", "pointer", "pointer", "int", "pointer"],
    fixture="c3dc_surfaces", state=_SURF,
    vectors=[("$d16a", "$br_full", "$d16b", 0, "$tbl"),
             ("$d16a", "$br_full", "$d16b", 100, "$tbl"),
             ("$d16a", "$br_in", "$d16p", 33, "$tbl"),
             ("$d16a", "$br_odd", "$d16b", 66, "$tbl2"),
             ("$d16a", "$br_neg", "$d16b", 25, "$tbl"),
             ("$d16a", "$br_over", "$d16b", 75, "$tbl"),
             ("$d16a", "$br_out", "$d16b", 50, "$tbl"),   # empty intersection -> 3
             ("$d16c", "$br_s19", "$d16c", 50, "$tbl"),   # format 1 surfaces, stride 19
             ("$d16b", "$br_in", "$d16a", 90, "$tbl"),
             ("$d16a", "$br_full", "$s8a", 50, "$tbl"),   # src depth 8 -> 0x17
             ("$d16a", "$br_full", "$s12", 50, "$tbl"),   # src depth 12 -> 0x17
             ("$d16a", "$br_full", "$x24", 50, "$tbl"),   # src depth 24 -> 0x17
             ("$s8a", "$br_full", "$d16a", 50, "$tbl"),   # this depth 8 -> 0x17 (src not looked at)
             ("$x24", 0, 0, 50, 0),                       # this depth 24 -> 0x17 with NULL arguments
             ("$s12", "$br_in", "$d16a", 20, "$tbl")])    # this depth 12 -> 0x17

# ======================================================================== group D: Sprite draw dispatchers
# c3ci's object set plus run-length row tables (fixture header), one key per scale setting: _s111 (1, 1, 1) the
# unscaled bodies of the blitters, _s223 (2, 2, 3) the stepped ones. The dispatchers read none of the scale
# globals; the second key is spread and a check that they are agnostic. Every key freezes the first thread
# (the live Sprite::draw16_1001bc40 / draw_1005ed80 share the loop globals) and silences jgld's debug reports.
# State: $pix the fourteen pixel slices, $spr the thirteen sprites (the Palette word +0x10 every dispatcher saves,
# replaces and restores; the early returns leave it replaced), $spix the payloads, $l0..$l13 the lock windows
# (a missing or extra slot-+0x24 release reads RED) and the module blocks the blitters write, the run-length
# block $grle among them. The seven shared loop globals are not regions (c3av / c3ci).
_SPR = ([("$pix", 0, 14 * 0x1000), ("$spr", 0, 15 * 0x80), ("$spix", 0, 6 * 0x4000)] +
        [(f"$l{i}", 0, 0x14) for i in range(14)] +
        [("$gmask", 0, 0xC), ("$gvstep", 0, 8), ("$gtbl", 0, 4), ("$gpos", 0, 8), ("$glevel", 0, 4),
         ("$gcols", 0, 8), ("$gxstep", 0, 0x14), ("$grle", 0, 0x5C), ("$growpair", 0, 8),
         ("$g8vstep", 0, 8), ("$g8tbl", 0, 4), ("$g8pos", 0, 8), ("$g8cols", 0, 4), ("$g8colour", 0, 4),
         ("$g8pix", 0, 4), ("$g8step", 0, 8), ("$g8xstep", 0, 0xC), ("$g8rowpair", 0, 8)])


def _spr_vectors(extra):
    """(sprite, dst, x, y, *extra, pal) over every arm of the two-level switch.
    extra is the tuple of arguments between y and the Palette override."""
    e = tuple(extra)
    return [("$bA", "$d0", 0, 0) + e + (0,),            # 8 bpp -> 16: the not-busy blitter
            ("$bA", "$d1", 3, 2) + e + ("$pal2",),      # clip inside, Palette override
            ("$bB", "$d2", -2, -1) + e + (0,),          # format 1, clipped left and top
            ("$bW", "$d5", 12, 8) + e + (0,),           # stride 24, clipped right and bottom
            ("$bBsy", "$d0", 0, 0) + e + (0,),          # busy 8 bpp -> 16: run-length blitter or report
            ("$bBsy2", "$d1", 2, 1) + e + ("$pal",),    # busy, 20 x 10, override
            ("$bA", "$e0", 0, 0) + e + (0,),            # 8 bpp -> 8
            ("$bBsy", "$e1", 1, 1) + e + (0,),          # busy 8 bpp -> 8
            ("$bA", "$x0", 0, 0) + e + (0,),            # 8 bpp -> depth 24: report, 0x17
            ("$b16w", "$d0", 0, 0) + e + (0,),          # 16 bpp -> 16
            ("$b16w", "$d1", 4, 2) + e + ("$pal2",),    # 16 bpp -> 16, clip inside, override
            ("$b16Bsy", "$d0", 0, 0) + e + (0,),        # busy 16 bpp -> 16
            ("$b16w", "$e0", 0, 0) + e + (0,),          # 16 bpp -> 8
            ("$b16w", "$x1", 0, 0) + e + (0,),          # 16 bpp -> depth 12
            ("$bX", "$d0", 0, 0) + e + (0,),            # 24 bpp: the outer default
            ("$bA", 0, 0, 0) + e + ("$pal2",),          # NULL destination
            ("$bNB", "$d0", 0, 0) + e + ("$pal2",),     # no payload: slot +0x20 returns 0
            ("$bA", "$d4", 0, 0) + e + (0,)]            # destination without pixel base: the blitter's guard


_ARGS5 = ["pointer", "pointer", "int", "int", "pointer", "pointer"]
for _s in ("s111", "s223"):
    _fx = f"c3dc_spr_{_s}"
    HOOKS[f"Sprite::drawB_{_s}"] = dict(
        module="jgld.dll", addr=0x00015480, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "pointer"], fixture=_fx, state=_SPR,
        vectors=_spr_vectors(()) + [("$bBnp", "$d0", 0, 0, 0), ("$b16", "$e0", 0, 0, 0)])
    # drawE: the fourth argument is a colour (draw8 0x5fd20, draw 0x1f2d0, drawRle16c take a byte or an int)
    HOOKS[f"Sprite::drawE_{_s}"] = dict(
        module="jgld.dll", addr=0x00015F80, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "int", "pointer"], fixture=_fx, state=_SPR,
        vectors=_spr_vectors((0x5A,)) + [("$bA", "$d0", 0, 0, 0x1234, 0), ("$bBsy", "$d2", 0, 0, 0x33, 0)])
    # drawG / drawI: a 16-bit table ($t; draw16t 0x1ab40 indexes it by destination, 0x28630 / 0x27430 and the
    # run-length 0x5af00 / 0x58970 by colour or Palette value, all inside 0xe4100 bytes)
    HOOKS[f"Sprite::drawG_{_s}"] = dict(
        module="jgld.dll", addr=0x000165A0, abi="thiscall", ret="int", args=_ARGS5, fixture=_fx, state=_SPR,
        vectors=_spr_vectors(("$t",)) + [("$bA", "$d0", 0, 0, "$t2", 0), ("$b16", "$d0", 0, 0, "$t", 0)])
    HOOKS[f"Sprite::drawI_{_s}"] = dict(
        module="jgld.dll", addr=0x00016CB0, abi="thiscall", ret="int", args=_ARGS5, fixture=_fx, state=_SPR,
        vectors=_spr_vectors(("$t",)) + [("$bA", "$d0", 0, 0, "$t2", 0), ("$bBnp", "$d0", 0, 0, "$t", 0)])
    # drawK: a 256-byte index remap and a 16-bit table (0x20430 and drawRle16b 0x5c600)
    HOOKS[f"Sprite::drawK_{_s}"] = dict(
        module="jgld.dll", addr=0x000172B0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "pointer", "pointer", "pointer"], fixture=_fx, state=_SPR,
        vectors=_spr_vectors(("$m", "$t")) + [("$bA", "$d0", 0, 0, "$m2", "$t2", 0),
                                              ("$bBsy", "$d0", 0, 0, "$m2", "$t", 0)])
    # drawM: 0x2ab90 parks its fourth argument at 0x10128548 and indexes it as 16-bit entries with
    # ((byte - 0xf8) & 0xff) << 15 | destination for every payload byte below 0xfe, so its 8 bpp sprites are bP /
    # bP2 (payload 0xf8..0xff); draw16t 0x1b060 indexes the same pointer by (source << 16) | destination bytes,
    # which the 16-bit word payload keeps under 0x40000. $t is 0xe4100 bytes.
    HOOKS[f"Sprite::drawM_{_s}"] = dict(
        module="jgld.dll", addr=0x00017B40, abi="thiscall", ret="int", args=_ARGS5, fixture=_fx, state=_SPR,
        vectors=[tuple({"$bA": "$bP", "$bB": "$bP2", "$bW": "$bP"}.get(v, v) for v in vec)
                 for vec in _spr_vectors(("$t",))] + [("$bP", "$d0", 0, 0, "$t2", 0)])
    # drawN: 0x2bcf0 reads a 16-bit table ($t); the 16-bit arm's 0x1b590 returns 0
    HOOKS[f"Sprite::drawN_{_s}"] = dict(
        module="jgld.dll", addr=0x00018120, abi="thiscall", ret="int", args=_ARGS5, fixture=_fx, state=_SPR,
        vectors=_spr_vectors(("$t",)) + [("$bA", "$d0", 0, 0, "$t2", 0)])
    # drawP: the fourth argument is a percentage: 100 -> slot +0x44 (drawB), 50 / 25 -> the even / quarter
    # blitters (8 bpp) or bodies returning 0 (16 bpp), anything else -> 3
    _p = []
    for _pct in (100, 50, 25, 7):
        _p += [("$bA", "$d0", 0, 0, _pct, 0), ("$bB", "$d1", 2, 1, _pct, "$pal"), ("$b16w", "$d0", 0, 0, _pct, 0)]
    HOOKS[f"Sprite::drawP_{_s}"] = dict(
        module="jgld.dll", addr=0x000193D0, abi="thiscall", ret="int",
        args=["pointer", "pointer", "int", "int", "int", "pointer"], fixture=_fx, state=_SPR,
        vectors=_spr_vectors((100,)) + _p + [("$bA", "$d0", 0, 0, -50, 0), ("$b16w", "$d2", 0, 0, 0, 0)])

# ======================================================================== group P: libpng
_PNG = [("$blk", 0, 0x1000)] + [("$arena", 0, 0x810), ("$freelog", 0, 0x404)]
# png_create_info_struct(png_ptr): NULL -> 0; otherwise png_create_struct(2) + png_info_init. The block == NULL
# side (je 0x100788a3) is not reached: the stand-in's arena never fails (see the purpose file).
_PCI = [(0,), ("$png",), (1,), (0x12345678,), (0,), ("$I0",), ("$T0",), (-1,), (0,), ("$png",), (2,), (0,)]
HOOKS["png_create_info_struct"] = dict(module="jgld.dll", addr=0x00078870, abi="default", ret="pointer",
                                       args=["pointer"], fixture="c3dc_png", state=_PNG, vectors=_PCI)
# png_set_text(png_ptr, info_ptr, text_ptr, num_text): the three early returns, both growth arms, no growth
# (strictly below and exactly at max_text), the NULL / empty / non-empty text arms, a negative count.
HOOKS["png_set_text"] = dict(
    module="jgld.dll", addr=0x0007E220, abi="default", ret="void", args=["pointer", "pointer", "pointer", "int"],
    fixture="c3dc_png", state=_PNG,
    vectors=[(0, "$I0", "$T0", 3),            # png_ptr NULL
             ("$png", 0, "$T0", 3),           # info NULL
             ("$png", "$I1", "$T0", 0),       # num_text 0
             ("$png", "$I0", "$T0", 3),       # no array: (3 + 8) * 16 from png_malloc
             ("$png", "$I0", "$T2", 5),       # no array, five texts
             ("$png", "$I1", "$T0", 3),       # 2 + 3 > 4: grow to 13, copy 4 * 16, free the old array
             ("$png", "$I1", "$T1", 1),       # 2 + 1 <= 4: no growth
             ("$png", "$I1", "$T3", 2),       # 2 + 2 == 4: no growth (jle), both texts NULL -> ""
             ("$png", "$I2", "$T2", 5),       # 1 + 5 <= 10
             ("$png", "$I3", "$T2", 5),       # 3 + 5 > 6: grow to 16
             ("$png", "$I3", "$T1", 1),
             ("$png", "$I2", "$T0", -1),      # negative count: no growth, no iteration
             ("$png", "$I0", "$T3", 2),       # no array, NULL texts
             ("$png", "$I3", "$T0", 2)])      # 3 + 2 <= 6, the first two entries only
