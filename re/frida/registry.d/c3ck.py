# Batch c3ck: jgld.dll - libpng 1.0.5's chunk readers, the Palette methods reached through the Palette
# vtable VA 0x1011db10, and windowProc (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
# Reimplementations and the instruction citations: shim/src/re/c3ck.cpp; branch tables and the Purposes:
# log/c3/c3ck_purpose.md; fixtures: re/frida/js/fixtures.d/c3ck.js; the chunk bytes those embed:
# log/c3/c3ck_gen.py.
#
# THE SEVEN CHUNK READERS. Every png_struct and png_info is the fixture's. The png_struct carries c3ch's
# recording reader in the read_data_fn slot at +0x50 and c3bq's one-byte `ret` in the warning slot at
# +0x44 and the error slot at +0x40, so no byte comes from a file and no warning reaches a C stream. The
# compared state is the png_structs (mode, the image fields, sig_bit), the png_infos (what each setter
# wrote) and the control blocks (the reader's cursor, its call count and the last length it was asked
# for), so the sequence of reads each arm made is evidence and not only the final fields.
#
# THE FOUR PALETTE METHODS. The compared region of each palette is +0 to +0x810. Everything from +0x810
# on - the generation id, the id Palette::apply last worked from and the QueryPerformanceCounter value -
# is drawn by Palette::update 0x1006a3d0 from a performance counter and differs between two runs by
# construction, so it is deliberately left out; what the A/B compares is the 256 entries at +0xc and the
# two 16-bit tables at +0x40c and +0x60c that Palette::apply derives from them, which are a pure
# function of the entries.
#
# windowProc. The compared state is the fixture's event sink (which slot each message reached, with
# which arguments, in which order) and the command handler's record. The two module globals the body
# reads are swapped for those objects with the game's first thread suspended and written back when the
# key ends; neither is compared state.
#
# Hooked callees to switch off when re-running on the main build (SIMGOLF_HOOKS_OFF, all `jgld.dll:<rva>`):
#   png_handle_IHDR     794b0,79510,79620,78c40,787d0,9ca70,793b0,7dc70,79220
#   png_handle_IEND     79510,79620,78c40,787d0,9ca70,79220
#   png_handle_pHYs     794b0,79510,79620,78c40,787d0,9ca70,793b0,7dfc0,79220
#   png_handle_oFFs     794b0,79510,79620,78c40,787d0,9ca70,793b0,7ddb0,79220
#   png_handle_tIME     794b0,79510,79620,78c40,787d0,9ca70,79470,7e400,79220
#   png_handle_sBIT     794b0,79510,79620,78c40,787d0,9ca70,7e070,79220
#   png_handle_unknown  79510,79620,78c40,787d0,9ca70,79220
#     (png_crc_read 0x794b0, png_crc_finish 0x79510, png_crc_error 0x79620, png_read_data 0x78c40 are
#      c3ch; png_calculate_crc 0x787d0 is c3ao; crc32 0x9ca70 and png_get_int_32 0x793b0 / png_get_uint_16
#      0x79470 are c3ax / c3ak; png_set_IHDR 0x7dc70 is c3bq, png_set_pHYs 0x7dfc0 c3ak, png_set_oFFs
#      0x7ddb0 and png_set_sBIT 0x7e070 c3al, png_set_tIME 0x7e400 c3ao; png_format_buffer 0x79220 is
#      c3bb and is reached from png_chunk_warning 0x79250 on every disagreeing CRC.)
#   Palette::setDefault 6a380 (Palette::reset, c3bq - the body reaches it through vtable slot 2),
#                       6a4a0 (Palette::apply, c3bq), 75b0 (Random::range, c3ak), both through
#                       Palette::update 0x6a3d0, which is still C2 and carries no hook
#   Palette::setRGB     6a4a0, 75b0 (same, through Palette::update)
#   Palette::getRGBRange  none      Palette::getColor  none
#   windowProc          69d50,69db0,69e10,69e70,69ee0,69f40,69fb0,6a010,6a080,6a0e0 (c3cd's nine
#                       forwarders and onCommand). onKey 0x69c70, onPaint 0x69ac0, onActivate 0x69b90,
#                       onPaletteChanged 0x69a70 and realizePalette 0x699c0 carry no hook anywhere.
#
# AND THE OTHER WAY ROUND: nothing already at C3 calls anything this batch hooks. The seven chunk
# readers' only callers are png_read_info 0x1006be20 and png_read_end 0x1006cd00, both C2; the four
# Palette methods and windowProc have `callers none` and are reached through the Palette vtable
# 0x1011db10 and through the WNDPROC Display::createWindow 0x100657e0 stores at 0x10065836.
#
# Leaf rule: Palette::getRGBRange 0x6a8e0 and Palette::getColor 0x6add0 have no callee at all (their
# epilogues skip even the debug build's __chkesp 0x1007e780), so they carry 14 vectors each. The other
# ten all call at least one function that has a hooks.csv row.

_PNG = ["pointer", "pointer", "uint32"]

_IHDR_LEN = [13] * 16
_IEND_LEN = [0, 0, 1, 4, 7, 8, 9, 16, 17, 23, 32, 3]
_PHYS_LEN = [9, 9, 9, 9, 9, 9, 0, 1, 8, 10, 16, 9, 9, 4]
_TIME_LEN = [7, 7, 7, 7, 7, 7, 0, 1, 6, 8, 14, 7, 7, 3]
_SBIT_LEN = [1, 1, 2, 2, 3, 3, 4, 4, 0, 2, 5, 8, 3, 1]
_UNK_LEN = [0, 0, 2, 5, 8, 9, 16, 20, 31, 32, 1, 12]


def _state(prefix, n):
    return [("$%sPng" % prefix, 0, n * 0x200), ("$%sInfo" % prefix, 0, n * 0x100),
            ("$%sCtl" % prefix, 0, n * 0x10)]


def _vectors(prefix, lengths):
    return [("$%sP%d" % (prefix, i), "$%sI%d" % (prefix, i), n) for i, n in enumerate(lengths)]


# The sixteen palette blocks, compared from +0 to +0x810 (above that sits the generation id and the
# performance counter Palette::update draws).
_PAL = [("$p%d" % i, 0, 0x810) for i in range(16)]
_PAL_G = [("$g%d" % i, 0, 0x810) for i in range(16)]


HOOKS = {
    # ---------------------------------------------------------------- libpng 1.0.5 chunk readers
    "png_handle_IHDR": dict(module="jgld.dll", addr=0x000796E0, abi="default", ret="void", args=_PNG,
                            fixture="c3ck_ihdr", state=_state("ih", 16),
                            vectors=_vectors("ih", _IHDR_LEN)),

    "png_handle_IEND": dict(module="jgld.dll", addr=0x00079C20, abi="default", ret="void", args=_PNG,
                            fixture="c3ck_iend", state=_state("ie", 12),
                            vectors=_vectors("ie", _IEND_LEN)),

    "png_handle_pHYs": dict(module="jgld.dll", addr=0x0007B380, abi="default", ret="void", args=_PNG,
                            fixture="c3ck_phys", state=_state("ph", 14),
                            vectors=_vectors("ph", _PHYS_LEN)),

    "png_handle_oFFs": dict(module="jgld.dll", addr=0x0007B4D0, abi="default", ret="void", args=_PNG,
                            fixture="c3ck_offs", state=_state("of", 14),
                            vectors=_vectors("of", _PHYS_LEN)),

    "png_handle_tIME": dict(module="jgld.dll", addr=0x0007B9B0, abi="default", ret="void", args=_PNG,
                            fixture="c3ck_time", state=_state("tm", 14),
                            vectors=_vectors("tm", _TIME_LEN)),

    "png_handle_sBIT": dict(module="jgld.dll", addr=0x00079EC0, abi="default", ret="void", args=_PNG,
                            fixture="c3ck_sbit", state=_state("sb", 14),
                            vectors=_vectors("sb", _SBIT_LEN)),

    "png_handle_unknown": dict(module="jgld.dll", addr=0x0007C150, abi="default", ret="void", args=_PNG,
                               fixture="c3ck_unknown", state=_state("uk", 12),
                               vectors=_vectors("uk", _UNK_LEN)),

    # ------------------------------------------------------------------------- the Palette methods
    #
    # setDefault takes no argument but `this`, so each of the twelve vectors is one call over a palette
    # pre-filled differently: what it writes is the same every time (the body reads nothing but the two
    # system-colour tables and the entry index), and that is the point - the twelve different pre-fills
    # show exactly which bytes it overwrites and which it leaves standing.
    "Palette::setDefault": dict(module="jgld.dll", addr=0x0006A600, abi="thiscall", ret="int",
                                args=["pointer"], fixture="c3ck_palettes", state=_PAL,
                                vectors=[("$p%d" % i,) for i in range(12)]),

    # setRGB(rgb, first, n): a count of zero and a negative one leave the palette alone (the `jge` at
    # 0x1006a9d5), a count of 256 rewrites every entry, and first = 250 with n = 10 runs off the end of
    # the entry table into the RGB555 table at +0x40c, which Palette::update then overwrites - the body
    # checks neither argument.
    "Palette::setRGB": dict(module="jgld.dll", addr=0x0006A9A0, abi="thiscall", ret="int",
                            args=["pointer", "pointer", "int", "int"], fixture="c3ck_palettes",
                            state=_PAL,
                            vectors=[("$p0", "$rgb0", 0, 1),
                                     ("$p1", "$rgb0", 0, 0),
                                     ("$p2", "$rgb0", 0, -5),
                                     ("$p3", "$rgb1", 10, 16),
                                     ("$p4", "$rgb1", 0, 256),
                                     ("$p5", "$rgb2", 250, 10),
                                     ("$p6", "$rgb2", 128, 1),
                                     ("$p7", "$rgb3", 1, 2),
                                     ("$p8", "$rgb3", 200, 55),
                                     ("$p9", "$rgb0", 255, 1),
                                     ("$p10", "$rgb1", 7, 3),
                                     ("$p11", "$rgb2", 0, 100),
                                     ("$p12", "$rgb3", 64, 64),
                                     ("$p13", "$rgb0", 32, 8)]),

    # getRGBRange(dst, first, n): the read side, and a leaf - fourteen vectors, with the same zero and
    # negative counts and the same run off the end of the entry table (which reads the RGB555 table).
    "Palette::getRGBRange": dict(module="jgld.dll", addr=0x0006A8E0, abi="thiscall", ret="int",
                                 args=["pointer", "pointer", "int", "int"], fixture="c3ck_palettes",
                                 state=_PAL + [("$dstBlk", 0, 16 * 0x400)],
                                 vectors=[("$p0", "$dst0", 0, 1),
                                          ("$p1", "$dst1", 0, 0),
                                          ("$p2", "$dst2", 0, -3),
                                          ("$p3", "$dst3", 10, 16),
                                          ("$p4", "$dst4", 0, 256),
                                          ("$p5", "$dst5", 250, 10),
                                          ("$p6", "$dst6", 128, 1),
                                          ("$p7", "$dst7", 1, 2),
                                          ("$p8", "$dst8", 200, 55),
                                          ("$p9", "$dst9", 255, 1),
                                          ("$p10", "$dst10", 7, 3),
                                          ("$p11", "$dst11", 0, 100),
                                          ("$p12", "$dst12", 64, 64),
                                          ("$p13", "$dst13", 32, 8)]),

    # getColor(index): a leaf whose only output is the three module bytes at VA 0x10128734 and their
    # address, so the compared state is those three bytes; the key suspends the game's first thread,
    # which is the one that enters this body at the menu.
    "Palette::getColor": dict(module="jgld.dll", addr=0x0006ADD0, abi="thiscall", ret="pointer",
                              args=["pointer", "int"], fixture="c3ck_getcolor",
                              state=[("$scratch", 0, 3)] + _PAL_G,
                              vectors=[("$g0", 0), ("$g1", 1), ("$g2", 9), ("$g3", 10),
                                       ("$g4", 0x40), ("$g5", 0x80), ("$g6", 0xf5), ("$g7", 0xf6),
                                       ("$g8", 0xff), ("$g9", 0x100), ("$g10", 0x101),
                                       ("$g11", 0x2ff), ("$g12", 7), ("$g13", 0x7f)]),

    # ------------------------------------------------------------------------------- windowProc
    #
    # Forty-five messages: every one of the twenty-three the body handles, the three arms of the
    # WM_SYSCOMMAND / WM_POWERBROADCAST pair, and twelve messages that land on the default arm from
    # every one of the five places that can reach it. Every WM_KEYDOWN / WM_SYSKEYDOWN vector carries a
    # virtual key inside onKey's 0x60..0x6f window and every key-up vector has down = 0, so the loop at
    # 0x10069cbc that calls the LIVE display never runs (re/frida/js/fixtures.d/c3ck.js).
    "windowProc": dict(module="jgld.dll", addr=0x000692B0, abi="stdcall", ret="int",
                       args=["pointer", "uint32", "uint32", "uint32"], fixture="c3ck_wndproc",
                       state=[("$sink", 0, 0x200), ("$rec", 0, 0x40)],
                       vectors=[("$hwnd", 0x200, 0x1234, 0x00070005),   # WM_MOUSEMOVE
                                ("$hwnd", 0x200, 0, 0xFFF8FFF9),        #   negative coordinates
                                ("$hwnd", 0x100, 0x60, 0x00010001),     # WM_KEYDOWN, vk 0x60
                                ("$hwnd", 0x100, 0x6F, 0x12345678),     # WM_KEYDOWN, vk 0x6f
                                ("$hwnd", 0x101, 0x41, 0x00020003),     # WM_KEYUP
                                ("$hwnd", 0x104, 0x65, 0),              # WM_SYSKEYDOWN, vk 0x65
                                ("$hwnd", 0x105, 0x1B, 0x80008000),     # WM_SYSKEYUP
                                ("$hwnd", 0x102, 0x41, 0x00010001),     # WM_CHAR
                                ("$hwnd", 0x102, 0x1FF, 0x7FFF7FFF),    #   only the low byte is passed
                                ("$hwnd", 0x005, 2, 0x01900258),        # WM_SIZE
                                ("$hwnd", 0x006, 0x00010002, 0x9999),   # WM_ACTIVATE, active
                                ("$hwnd", 0x006, 0, 0x1111),            # WM_ACTIVATE, inactive
                                ("$hwnd", 0x006, 0x00010001, 0x2222),   # WM_ACTIVATE, minimized
                                ("$hwnd", 0x00F, 0, 0),                 # WM_PAINT
                                ("$hwnd", 0x04A, 0x1111, 0x2222),       # WM_COPYDATA -> onCommand
                                ("$hwnd", 0x04A, 0, 0),                 #   handler returns 0
                                ("$hwnd", 0x084, 0, 0x00320064),        # WM_NCHITTEST, result passed up
                                ("$hwnd", 0x084, 0, 0xFFFFFFFF),
                                ("$hwnd", 0x201, 1, 0x000A000B),        # WM_LBUTTONDOWN
                                ("$hwnd", 0x202, 0, 0x000C000D),        # WM_LBUTTONUP
                                ("$hwnd", 0x203, 1, 0x000E000F),        # WM_LBUTTONDBLCLK
                                ("$hwnd", 0x204, 2, 0x00100011),        # WM_RBUTTONDOWN
                                ("$hwnd", 0x205, 0, 0x00120013),        # WM_RBUTTONUP
                                ("$hwnd", 0x206, 3, 0x00140015),        # WM_RBUTTONDBLCLK
                                ("$hwnd", 0x20A, 0x00780002, 0x00160017),   # WM_MOUSEWHEEL
                                ("$hwnd", 0x20A, 0xFF880001, 0xFFFEFFFD),   #   negative delta
                                ("$hwnd", 0x112, 0xF140, 0),            # WM_SYSCOMMAND SC_SCREENSAVE
                                ("$hwnd", 0x112, 0xF170, 0),            # WM_SYSCOMMAND SC_MONITORPOWER
                                ("$hwnd", 0x112, 0xF100, 0),            # WM_SYSCOMMAND, other
                                ("$hwnd", 0x218, 0, 0x5555),            # PBT_APMQUERYSUSPEND
                                ("$hwnd", 0x218, 0xF140, 0),            #   falls into WM_SYSCOMMAND
                                ("$hwnd", 0x218, 0xF170, 0),
                                ("$hwnd", 0x218, 7, 0),                 #   and on to the default arm
                                ("$hwnd", 0x30F, 0, 0),                 # realizePalette, low 16 bits
                                ("$hwnd", 0x311, 0x9876, 0),            # WM_PALETTECHANGED, other window
                                ("$hwnd", 0x311, 1, 0),                 #   our own window: no call
                                ("$hwnd", 0x401, 0x11223344, 0),        # the private message
                                ("$hwnd", 0x401, 0, 0xABCDEF01),
                                ("$hwnd", 0x000, 0, 0),                 # default: msg - 5 wraps
                                ("$hwnd", 0x004, 0, 0),
                                ("$hwnd", 0x010, 0, 0),                 # default slot of table 1
                                ("$hwnd", 0x085, 0, 0),                 # msg - 5 above 0x7f
                                ("$hwnd", 0x111, 5, 6),                 # WM_COMMAND: default slot
                                ("$hwnd", 0x1FF, 0, 0),                 # msg - 0x101 above 0x11
                                ("$hwnd", 0x207, 0, 0),                 # msg - 0x201 above 5
                                ("$hwnd", 0x20B, 0, 0),                 # between the third and fourth
                                ("$hwnd", 0x312, 0, 0),                 # above 0x311, not 0x401
                                ("$hwnd", 0x402, 0, 0)]),
}
