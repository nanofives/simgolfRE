# Batch c3bl: the two large libpng 1.0.5 functions of jgld.dll batch c3bg left as leads (module-aware
# hooks; addr is the RVA, VA = 0x10000000 + addr). Every block the vectors touch is allocated by the
# fixtures (re/frida/js/fixtures.d/c3bl.js); nothing live in the running jgld.dll (Display, Surface,
# Palette, Font, the game's png_struct / png_info or its heap blocks) is passed in.
# Reimplementations and the instruction citations: shim/src/re/c3bl.cpp; branch tables:
# log/c3/c3bl_purpose.md.
#
# Leaf rule: neither function is a leaf. png_do_background calls the CRT memcpy 0x1007f3a0 and the
# debug stack check __chkesp 0x1007e780; png_do_read_transformations calls eighteen png_do_* row
# routines plus png_warning 0x10079040, png_error 0x10078fe0, sprintf 0x10082c80 and __chkesp.
#
# Hooked callees to switch off when re-running. png_do_background has none. All EIGHTEEN png_do_*
# callees of png_do_read_transformations carry an SG_HOOK:
#   c3ao  png_do_invert 0x0006d900, png_do_swap 0x0006d980, png_do_packswap 0x0006da10,
#         png_do_chop 0x00072430
#   c3ax  png_do_bgr 0x0006e010, png_do_unpack 0x00071e80, png_do_read_swap_alpha 0x000724f0,
#         png_do_read_invert_alpha 0x00072860, png_do_gray_to_rgb 0x00073310
#   c3bb  png_do_unshift 0x000720c0, png_do_expand_palette 0x00076ec0, png_do_dither 0x00077a90
#   c3bg  png_do_strip_filler 0x0006dac0, png_do_read_filler 0x00072bd0,
#         png_do_rgb_to_gray 0x000736c0, png_do_gamma 0x00076760, png_do_expand 0x00077300
#   c3bl  png_do_background 0x00074750 (this batch)
# On instances\c3bl build_instance.ps1 links only c3bl.cpp, so only png_do_background was hooked
# there and the key was re-run with SIMGOLF_HOOKS_OFF=jgld.dll:74750. On the main build the whole
# list above must be switched off.

# One state region per fixture block. The gamma tables, the palette, the transparency array and the
# dither lookups are their own regions, which is how "the body only reads them" is tested; the
# pattern-filled blocks around each object catch a store one byte too wide.
_BG = [("$bghblk", 0, 20 * 0xC), ("$bgrowblk", 0, 0x400), ("$bgcolblk", 0, 11 * 0x10),
       ("$bggt", 0, 256), ("$bggf1", 0, 256), ("$bggt1", 0, 256),
       ("$bgg16", 0, 256 * 4), ("$bgg16sub", 0, 8 * 512),
       ("$bgg16f", 0, 256 * 4), ("$bgg16fsub", 0, 8 * 512),
       ("$bgg16t", 0, 256 * 4), ("$bgg16tsub", 0, 8 * 512)]

_RT = [("$rtblk", 0, 35 * 0x280), ("$rtrowblk", 0, 35 * 0x200),
       ("$rtpal", 0, 768), ("$rttrans", 0, 256), ("$rtgt", 0, 256), ("$rtdither", 0, 256),
       ("$rtlookup", 0, 0x8000), ("$rtg16", 0, 256 * 4), ("$rtg16sub", 0, 8 * 512)]

# png_do_background's twelve arguments, in the order the caller pushes them (0x10071a8f backwards to
# 0x10071a8e): row_info, row, trans_values, background, background_1, gamma_table, gamma_from_1,
# gamma_to_1, gamma_16, gamma_16_from_1, gamma_16_to_1, gamma_shift.
_G8 = ("$bggt", "$bggf1", "$bggt1")
_G16 = ("$bgg16", "$bgg16f", "$bgg16t")


def _bg(hdr, row, tv, bk, bk1, g8=_G8, g16=_G16, shift=4):
    return (hdr, row, tv, bk, bk1, g8[0], g8[1], g8[2], g16[0], g16[1], g16[2], shift)


_BG_VECTORS = [
    # grey, the five depths that have a body, each in its gamma and its non-gamma form where the
    # body has both, plus the two depths that have none.
    _bg("$bgh0", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                        # 0  depth 1
    _bg("$bgh1", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                        # 1  depth 2, gamma
    _bg("$bgh1", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a", (0, "$bggf1", "$bggt1")),  # 2  depth 2, plain
    _bg("$bgh2", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                        # 3  depth 4, gamma
    _bg("$bgh2", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a", (0, "$bggf1", "$bggt1")),  # 4  depth 4, plain
    _bg("$bgh3", "$bgrgray", "$bg_tvg55", "$bg_bk1", "$bg_bk1a"),                       # 5  depth 8, gamma
    _bg("$bgh3", "$bgrgray", "$bg_tvg55", "$bg_bk1", "$bg_bk1a", (0, "$bggf1", "$bggt1")),  # 6  depth 8, plain
    _bg("$bgh4", "$bgrgray", "$bg_tvg55aa", "$bg_bk2", "$bg_bk2a"),                     # 7  depth 16, gamma
    _bg("$bgh4", "$bgrgray", "$bg_tvg55aa", "$bg_bk2", "$bg_bk2a",
        _G8, (0, "$bgg16f", "$bgg16t")),                                                # 8  depth 16, plain
    _bg("$bgh5", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                        # 9  depth 3: byte-table default
    _bg("$bgh6", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                        # 10 depth 24: range test
    # RGB.
    _bg("$bgh7", "$bgrrgb8", "$bg_tvrgb8", "$bg_bk1", "$bg_bk1a"),                      # 11 RGB 8, gamma
    _bg("$bgh7", "$bgrrgb8", "$bg_tvrgb8", "$bg_bk1", "$bg_bk1a", (0, "$bggf1", "$bggt1")),  # 12 RGB 8, plain
    _bg("$bgh8", "$bgrrgb16", "$bg_tvrgb16", "$bg_bk2", "$bg_bk2a"),                    # 13 RGB 16, gamma
    _bg("$bgh8", "$bgrrgb16", "$bg_tvrgb16", "$bg_bk2", "$bg_bk2a",
        _G8, (0, "$bgg16f", "$bgg16t")),                                                # 14 RGB 16, plain
    # grey + alpha, eight bits: the three-pointer guard with each pointer in turn NULL.
    _bg("$bgh9", "$bgrga8", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                         # 15 all three set
    _bg("$bgh9", "$bgrga8", "$bg_tvg0", "$bg_bk1", "$bg_bk1a", ("$bggt", "$bggf1", 0)),  # 16 gamma_to_1 NULL
    _bg("$bgh9", "$bgrga8", "$bg_tvg0", "$bg_bk1", "$bg_bk1a", ("$bggt", 0, "$bggt1")),  # 17 gamma_from_1 NULL
    _bg("$bgh9", "$bgrga8", "$bg_tvg0", "$bg_bk1", "$bg_bk1a", (0, "$bggf1", "$bggt1")),  # 18 gamma_table NULL
    # grey + alpha, sixteen bits.
    _bg("$bgh10", "$bgrga16", "$bg_tvg0", "$bg_bk2", "$bg_bk2a"),                       # 19 all three set
    _bg("$bgh10", "$bgrga16", "$bg_tvg0", "$bg_bk2", "$bg_bk2a",
        _G8, (0, "$bgg16f", "$bgg16t")),                                                # 20 gamma_16 NULL
    _bg("$bgh10", "$bgrga16", "$bg_tvg0", "$bg_bk2", "$bg_bk2a",
        _G8, ("$bgg16", 0, "$bgg16t")),                                                 # 21 gamma_16_from_1 NULL
    _bg("$bgh10", "$bgrga16", "$bg_tvg0", "$bg_bk2", "$bg_bk2a",
        _G8, ("$bgg16", "$bgg16f", 0)),                                                 # 22 gamma_16_to_1 NULL
    # RGBA, eight bits.
    _bg("$bgh11", "$bgrrgba8", "$bg_tvrgb8", "$bg_bk1", "$bg_bk1a"),                    # 23 all three set
    _bg("$bgh11", "$bgrrgba8", "$bg_tvrgb8", "$bg_bk1", "$bg_bk1a", ("$bggt", "$bggf1", 0)),  # 24 gamma_to_1 NULL
    _bg("$bgh11", "$bgrrgba8", "$bg_tvrgb8", "$bg_bk1", "$bg_bk1a", ("$bggt", 0, "$bggt1")),  # 25 gamma_from_1 NULL
    _bg("$bgh11", "$bgrrgba8", "$bg_tvrgb8", "$bg_bk1", "$bg_bk1a", (0, "$bggf1", "$bggt1")),  # 26 gamma_table NULL
    # RGBA, sixteen bits.
    _bg("$bgh12", "$bgrrgba16", "$bg_tvrgb16", "$bg_bk2", "$bg_bk2a"),                  # 27 all three set
    _bg("$bgh12", "$bgrrgba16", "$bg_tvrgb16", "$bg_bk2", "$bg_bk2a",
        _G8, (0, "$bgg16f", "$bgg16t")),                                                # 28 gamma_16 NULL
    _bg("$bgh12", "$bgrrgba16", "$bg_tvrgb16", "$bg_bk2", "$bg_bk2a",
        _G8, ("$bgg16", 0, "$bgg16t")),                                                 # 29 gamma_16_from_1 NULL
    _bg("$bgh12", "$bgrrgba16", "$bg_tvrgb16", "$bg_bk2", "$bg_bk2a",
        _G8, ("$bgg16", "$bgg16f", 0)),                                                 # 30 gamma_16_to_1 NULL
    # the guards, the colour types with no body, the empty row and the two extra rounding cases.
    ("$bgh3", "$bgrgray", "$bg_tvg55", 0, "$bg_bk1a", "$bggt", "$bggf1", "$bggt1",
     "$bgg16", "$bgg16f", "$bgg16t", 4),                                                # 31 background NULL
    ("$bgh18", "$bgrrgba8", 0, "$bg_bk1", "$bg_bk1a", "$bggt", "$bggf1", "$bggt1",
     "$bgg16", "$bgg16f", "$bgg16t", 4),                                                # 32 alpha with trans_values NULL
    _bg("$bgh13", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                       # 33 colour type 3
    _bg("$bgh14", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                       # 34 colour type 1
    _bg("$bgh15", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                       # 35 colour type 5: tail only
    _bg("$bgh16", "$bgrgray", "$bg_tvg0", "$bg_bk1", "$bg_bk1a"),                       # 36 colour type 7: tail only
    _bg("$bgh17", "$bgrgray", "$bg_tvg55", "$bg_bk1", "$bg_bk1a"),                      # 37 width 0
    _bg("$bgh4", "$bgrgray", "$bg_tvg55aa", "$bg_bk2", "$bg_bk2a", _G8, _G16, 0),       # 38 grey 16 gamma, shift 0
    _bg("$bgh10", "$bgrga16", "$bg_tvg0", "$bg_bk2", "$bg_bk2a", _G8, _G16, 8),         # 39 GA 16 gamma, shift 8
    _bg("$bgh19", "$bgrgray", "$bg_tvg1", "$bg_bk0", "$bg_bk1a"),                       # 40 depth 1, the other polarity
    _bg("$bgh12", "$bgrrgba16", "$bg_tvrgb16", "$bg_bk2", "$bg_bk2a", _G8, _G16, 0),    # 41 RGBA 16 gamma, shift 0
]

HOOKS = {
    # png_do_background 0x00074750: the colour-type jump table at 0x10076713, the grey depth table at
    # 0x10076747, the one-table and three-table gamma guards of all eight bodies, the three branches
    # of every alpha body, both guards and the header rewrite that follows an alpha colour type.
    "png_do_background": dict(module="jgld.dll", addr=0x00074750, abi="default", ret="void",
                              args=["pointer", "pointer", "pointer", "pointer", "pointer",
                                    "pointer", "pointer", "pointer", "pointer", "pointer",
                                    "pointer", "int"],
                              fixture="c3bl_bg", state=_BG, vectors=_BG_VECTORS),

    # png_do_read_transformations 0x000717d0: one scratch png_struct per vector, one transformation
    # (or one combination) each, so every bit test of the word at +0x70 is taken and not taken and
    # each of the four guards around PNG_GAMMA and the two around PNG_GRAY_TO_RGB is decided by a
    # different field. The three png_error exits (a NULL row_buf at 0x100717f2, the
    # PNG_RGB_TO_GRAY_ERR arm at 0x1007198b and the rowbytes-0 arm at 0x10071b9c) are not taken by
    # any vector: png_error longjmps out of the A/B.
    "png_do_read_transformations": dict(module="jgld.dll", addr=0x000717D0, abi="default", ret="void",
                                        args=["pointer"], fixture="c3bl_rt", state=_RT,
                                        vectors=[("$rt%d" % i,) for i in range(35)]),
}
