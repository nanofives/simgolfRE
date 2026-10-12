# C3 batch c3ee (round 29, 2026-10-12): the image pipeline (re/C3_UI_PLAN.md, blocker 1). Every key's fixture freezes the
# game's first thread, swaps the display 0x0083ad50 for a recording display, builds surface handles / palette devices whose
# virtual slots work on fixture memory, and stands in the CRT heap (heapstandin.js) and the files (filestandin.js: KERNEL32
# for MappedFile, the static CRT for fopen / fread / fwrite / fclose). Reimplementations: shim/src/re/c3ee.cpp.
# Fixtures: re/frida/js/fixtures.d/c3ee.js (fixture files from log/c3/c3ee_gen.py). Purposes and per-jcc tables:
# log/c3/c3ee_purpose.md.
#
# Keys on other batches' hooks (their load / copy arms, which earlier keys never ran):
#   Image_draw 0x00473e60 and Surface_drawScaled 0x00473cb0 (c3df), Surface_blit 0x00473bf0 (c3h).
# Hooked callees (re-run with these hooks off for the callees-off pass), see log/c3/c3ee_notes.txt.

EXE = "golf_clean.exe"

OBJ = 0xA000          # the object block (c3ee.js: display, handles, palette devices, images, caches, pixels, data)
REC = 0x4000          # the record buffer
ARENA = 0x10000       # the heap stand-in's arena
FSB = 0x6000          # the file stand-in's block

_OBJS = [("$objs", 0, OBJ), ("$rec", 0, REC)]
_HEAP = [("$arena", 0, 0x10 + ARENA), ("$freelog", 0, 0x404)]
_FILES = [("$fsblock", 0, FSB), ("$implog", 0, 0x400)]
_PAINT = [(0x00839AB0, 0, 4), (0x0083AD4C, 0, 4)]   # Surface_paint's flag, pumpInputDisplay483da0's flag word


def _h(addr, fixture, vectors, state, ret="int", args=("pointer",), abi="thiscall"):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=list(args), fixture=fixture, state=state, vectors=vectors)


HOOKS = {}

# ---------------------------------------------------------------------------------------------------- Surface_paint
# (this, w, h, depth): i0..i3 real Image vtable (Surface_createFull arm; i0 / i3 with the old handle H1, i2 with H2, i1
# none); i4..i7 the sub-object vtable (+0x2c0: a sub-object, NULL, another, a third)
_PV = [("$i0", 7, 5, 8), ("$i1", 1, 1, 16), ("$i2", 0, 3, 8), ("$i3", -2, 9, 0x10), ("$i4", 3, 4, 8), ("$i5", 12, 2, 8),
       ("$i6", 5, 5, 16), ("$i7", 9, 1, 32), ("$i0", 0x7FFF, 1, 8), ("$i4", 0, 0, 0), ("$i5", -1, -1, 24), ("$i7", 1, 2, 3)]
for _k in ("ok", "fail"):
    HOOKS[f"Surface_paint_{_k}"] = _h(0x00474E70, f"c3ee_paint_{_k}", _PV, _OBJS + _HEAP + _PAINT,
                                      args=("pointer", "int", "int", "int"))

# ---------------------------------------------------------------------------------------------------- decodeImagePaletted
_DARGS = ("pointer", "pointer", "int", "pointer", "int", "int")


def _d(img, f, pal=0, ofs=0, cnt=256):
    return (img, f"$d_{f}" if f else 0, f"$n_{f}" if f else 0, pal, ofs, cnt)


_DEC8 = [_d("$i0", "p_basic"), _d("$i0", "p_basic", "$c0"), _d("$i0", "p_magic", "$c1", 5, 10), _d("$i1", "p_runs", "$c0", -3, 6),
         _d("$i0", "p_cross", 0, 250, 6), _d("$i0", "p_v3", "$c0", 0, 4), _d("$i0", "p_v2", 0, 1, 3), _d("$i0", "p_bpp4"),
         _d("$i0", "p_bpp4p16", "$c0", 0x10, 0x20), _d("$i0", "p_bpp8p2"), _d("$i0", "p_mfr"), _d("$i0", "p_enc0"),
         _d("$i0", "p_offset", "$c0", 0x10, 0x20), _d("$i0", "p_zero", "$c1", 0, 0x10), _d("$i0", "p_zero_m", 0, 7, 1),
         _d("$i0", "p_1x1", "$c0", -10, 12), _d("$i0", "p_wide", "$c0", 0, 0), _d("$i0", "p_neg"), _d("$i0", None),
         _d("$i2", "p_basic", "$c0"), _d("$i3", "p_basic"), _d("$i4", "p_basic"), _d("$i5", "p_basic", "$c0"),
         _d("$i2", "p_runs", "$c1", 200, 56), _d("$i0", "p_basic", "$c0", -1, 1)]
_DECST = _OBJS + _HEAP + _PAINT
HOOKS["decodeImagePaletted"] = _h(0x00495100, "c3ee_dec_8", _DEC8, _DECST, args=_DARGS)
HOOKS["decodeImagePaletted_fail"] = _h(0x00495100, "c3ee_dec_8_fail",
                                       [_d("$i0", "p_basic"), _d("$i1", "p_runs", "$c0"), _d("$i0", "p_v3", "$c0"),
                                        _d("$i2", "p_basic", "$c0"), _d("$i0", "p_mfr")], _DECST, args=_DARGS)
HOOKS["decodeImagePaletted_pitch"] = _h(0x00495100, "c3ee_dec_8_pitch",
                                        [_d("$i0", "p_basic", "$c0"), _d("$i0", "p_cross"), _d("$i0", "p_wide", "$c1", 3, 9),
                                         _d("$i0", "p_runs")], _DECST, args=_DARGS)

# ---------------------------------------------------------------------------------------------------- decodeImage16
# Only images whose handle stays non-NULL (sub-object arm: i2 H1, i4 H2, i5 failing sub-object), and only files whose
# decoded total is 0 or that stop before the RLE loop: the loop reads its colour from a NULL table and faults (purpose).
_DEC16 = [_d("$i2", None), _d("$i2", "p_mfr"), _d("$i2", "p_enc0"), _d("$i2", "p_v3", "$c0"), _d("$i2", "p_bpp4"),
          _d("$i2", "p_bpp8p2"), _d("$i5", "p_basic"), _d("$i4", "p_basic"), _d("$i2", "p_zero"), _d("$i2", "p_zero", "$c0"),
          _d("$i2", "p_zero_m", "$c1", 3, 4), _d("$i4", "p_zero", "$c0"), _d("$i2", "p_v2", "$c1"), _d("$i5", "p_mfr"),
          _d("$i2", "p_zero_v3"), _d("$i2", "p_zero_v3", "$c0")]
HOOKS["decodeImage16"] = _h(0x00495420, "c3ee_dec_16", _DEC16, _DECST, args=_DARGS)
HOOKS["decodeImage16_f"] = _h(0x00495420, "c3ee_dec_16f", _DEC16, _DECST, args=_DARGS)

# ---------------------------------------------------------------------------------------------------- loadImageData
_LID = [_d("$i0", "p_basic", "$c0"), _d("$i0", "p_magic"), _d("$i0", "p_bpp4"), _d("$i0", None), _d("$i2", "p_basic", "$c0"),
        _d("$i2", "p_magic", "$c1", 2, 9), _d("$i0", "p_v3", "$c0"), _d("$i3", "p_runs"), _d("$i5", "p_basic"),
        _d("$i2", "p_zero_m", "$c0", 1, 2)]
_LID16 = [_d("$i2", "p_zero"), _d("$i2", "p_bpp4"), _d("$i0", "p_basic", "$c0"), _d("$i5", "p_basic"), _d("$i4", "p_basic"),
          _d("$i2", "p_mfr"), _d("$i2", "p_zero_m", "$c0", 1, 2), _d("$i0", None), _d("$i0", "p_runs")]
HOOKS["loadImageData_d0"] = _h(0x00478CD0, "c3ee_dec_d0", _LID, _DECST, args=_DARGS)
HOOKS["loadImageData_d8"] = _h(0x00478CD0, "c3ee_dec_d8", _LID, _DECST, args=_DARGS)
HOOKS["loadImageData_d16"] = _h(0x00478CD0, "c3ee_dec_16", _LID16, _DECST, args=_DARGS)

# ---------------------------------------------------------------------------------------------------- loadImage
_LARGS = ("pointer", "pointer", "pointer", "int", "int", "int")
_LIST = _OBJS + _HEAP + _FILES + _PAINT
_LI = [("$i0", "$s0", 0, 0, 256, 0), ("$i0", "$s1", "$c0", 0, 256, 0), ("$i0", "$s2", "$c1", 3, 8, 0), ("$i0", "$s3", 0, 0, 0, 0),
       ("$i0", "$s4", 0, 0, 0, 0), ("$i0", "$s5", 0, 0, 0, 0), ("$i0", "$s6", "$c0", 9, 3, 0), ("$i0", "$s7", 0, 0, 0, 0),
       ("$i0", "$s8", 0, 0, 0, 0), ("$i0", "$s9", "$c0", 0, 4, 0), ("$i0", "$s10", 0, 0, 0, 0), ("$i0", "$s0", "$c0", 0, 256, 1),
       ("$i0", "$s0", 0, 0, 256, 1), ("$i2", "$s1", 0, 0, 256, 1), ("$i3", "$s11", 0, 10, 20, 1), ("$i0", "$s3", 0, 0, 0, 1),
       ("$i0", "$s0", "$c0", 0, 256, 4), ("$i3", "$s9", 0, 0, 4, 1), ("$i1", "$s12", 0, 0, 0, 0), ("$i0", "$s13", 0, 0, 0, 0),
       ("$i2", "$s4", 0, 0, 0, 1), ("$i0", "$s8", 0, 0, 0, 1)]
HOOKS["loadImage"] = _h(0x00475840, "c3ee_li", _LI, _LIST, args=_LARGS)
# flag 2: the list 0x0083ac88 (0x28 bytes) and the palette pointer 0x00839a98 are state
HOOKS["loadImage_f2"] = _h(0x00475840, "c3ee_li_f2",
                          [("$i0", "$s0", 0, 0, 256, 2), ("$i2", "$s1", 0, 0, 256, 2), ("$i0", "$s3", 0, 0, 0, 2),
                           ("$i0", "$s8", 0, 0, 0, 2), ("$i3", "$s11", 0, 10, 20, 2), ("$i0", "$s0", "$c0", 0, 256, 2),
                           ("$i0", "$s4", 0, 0, 0, 2), ("$i2", "$s9", 0, 0, 4, 6)],
                          _LIST + [(0x0083AC88, 0, 0x28), (0x00839A98, 0, 4)], args=_LARGS)
HOOKS["loadImage_new0"] = _h(0x00475840, "c3ee_li_new0",
                             [("$i0", "$s0", 0, 0, 256, 1), ("$i1", "$s1", 0, 0, 256, 1), ("$i0", "$s3", 0, 0, 0, 1),
                              ("$i2", "$s0", 0, 0, 256, 1)], _LIST, args=_LARGS)
HOOKS["loadImage_d16"] = _h(0x00475840, "c3ee_li_d16",
                            [("$i3", "$s12", 0, 0, 0, 0), ("$i3", "$s12", 0, 0, 0, 1), ("$i1", "$s12", "$c0", 0, 3, 0),
                             ("$i1", "$s8", 0, 0, 0, 0), ("$i3", "$s10", 0, 0, 0, 1), ("$i1", "$s3", 0, 0, 0, 0)],
                            _LIST, args=_LARGS)

# ---------------------------------------------------------------------------------------------------- openImageAsset
_OAV = [(f"$s{i}", "$pw", "$ph") for i in range(12)] + [("$s0", 0, "$ph"), ("$s0", "$pw", 0), ("$s2", "$pw2", "$ph2")]
_OAST = [("$out", 0, 0x40), ("$fsblock", 0, FSB), ("$implog", 0, 0x400), (0x0083FE5C, 0, 4)]
for _m in (0, 1, 2, 7):
    HOOKS[f"openImageAsset_m{_m}"] = _h(0x00478F50, f"c3ee_oa_m{_m}", _OAV, _OAST, abi="mscdecl",
                                        args=("pointer", "pointer", "pointer"))

# ---------------------------------------------------------------------------------------------------- loadBmp / loadBmpSurface / reloadBmp
_BST = [("$out", 0, 0x20)] + _OBJS + _HEAP + _FILES + _PAINT
_BV = [(f"$s{i}", "$out") for i in range(13)] + [("$s0", "$out")]
for _k in ("", "_f0", "_f1"):
    HOOKS[f"loadBmp{_k}"] = _h(0x00474EE0, f"c3ee_bmp{_k}", _BV, _BST, ret="pointer", abi="stdcall", args=("pointer", "pointer"))
# i2's handle H2 has a NULL lock: only bitmaps larger than it (which re-create the surface) are given to it, because
# neither function tests the lock before writing (purpose)
_BSV = [("$i0", "$s0"), ("$i1", "$s1"), ("$i1", "$s2"), ("$i0", "$s3"), ("$i1", "$s4"), ("$i2", "$s5"), ("$i0", "$s6"),
        ("$i0", "$s7"), ("$i0", "$s8"), ("$i0", "$s9"), ("$i1", "$s10"), ("$i0", "$s11"), ("$i0", "$s12"), ("$i2", "$s0"),
        ("$i1", "$s3"), ("$i2", "$s12")]
for _k in ("", "_555", "_f1"):
    HOOKS[f"loadBmpSurface{_k}"] = _h(0x00475210, f"c3ee_bmp{_k}", _BSV, _BST, args=("pointer", "pointer"))
_RV = [("$i0", "$inf0", "$bits0"), ("$i1", "$inf1", "$bits1"), ("$i2", "$inf0", "$bits0"), ("$i0", "$inf3", "$bits3"),
       ("$i1", "$inf0", "$bits0"), ("$i2", "$inf3", "$bits3"), ("$i0", "$inf1", "$bits1"), ("$i1", "$inf3", "$bits3"),
       ("$i1", "$inf2", "$bits2"), ("$i0", "$inf2", "$bits2")]
for _k in ("", "_555"):
    HOOKS[f"reloadBmp{_k}"] = _h(0x00475050, f"c3ee_reload{_k}", _RV, _OBJS + _HEAP + _PAINT,
                                 args=("pointer", "pointer", "pointer"))

# ---------------------------------------------------------------------------------------------------- cropSprite / Surface_blit
_CARGS = ("pointer", "pointer", "int", "int", "int", "int", "int", "int")
_CST = [("$surfs", 0, 0x30 * 10)] + _OBJS + _HEAP
_CV = [("$t0", "$src0", 0, 0, 0x18, 0x10, 3, 1), ("$t1", "$src0", 0, 0, 0x18, 0x10, 3, 0), ("$t2", "$src0", 2, 1, 10, 8, 0, 1),
       ("$t3", "$src0", 4, 3, 6, 6, 7, 1), ("$t0", "$src0", 0, 0, 3, 2, 0, 1), ("$t0", 0, 0, 0, 4, 4, 0, 1),
       ("$t1", "$src1", 0, 0, 4, 4, 0, 1), ("$t2", "$src2", 0, 0, 4, 4, 0, 1), ("$t0", "$src0", 0, 0, 0, 5, 0, 1),
       ("$t0", "$src0", 0, 0, 5, 0, 0, 1), ("$t1", "$src0", 10, 0, 0x10, 4, 0, 1), ("$t2", "$src0", 0, 8, 4, 9, 0, 1),
       ("$t3", "$src3", 0, 0, 2, 2, 0, 1), ("$t1", "$src0", 3, 2, 16, 10, 1, 0x100), ("$t2", "$src0", -2, 0, 4, 4, 0, 1),
       ("$t3", "$src0", 0, 0, -3, 2, 0, 1), ("$t0", "$src0", 3, 5, 16, 1, 0, 1), ("$t1", "$src0", 8, 2, 1, 10, 2, 1),
       ("$t2", "$src4", 1, 1, 6, 3, 0, 1), ("$t3", "$src0", 3, 2, 16, 10, 5, 0xff01)]
HOOKS["cropSprite"] = _h(0x00492000, "c3ee_crop", _CV, _CST, args=_CARGS)
HOOKS["cropSprite_f0"] = _h(0x00492000, "c3ee_crop_f0", _CV[:5] + _CV[13:15], _CST, args=_CARGS)
HOOKS["cropSprite_res"] = _h(0x00492000, "c3ee_crop_res", _CV[:5] + _CV[13:15], _CST, args=_CARGS)
HOOKS["cropSprite_clear"] = _h(0x00492000, "c3ee_crop_clear", _CV[:5] + _CV[13:15], _CST, args=_CARGS)
HOOKS["Surface_blit_copy"] = _h(0x00473BF0, "c3ee_crop", [v for v in _CV if v[1] not in ("$src2", "$src3")], _CST, args=_CARGS)
HOOKS["Surface_blit_fmt16"] = _h(0x00473BF0, "c3ee_crop_fmt16",
                                 [("$t0", "$src4", 0, 0, 4, 4, 0, 1), ("$t1", "$src0", 2, 1, 10, 8, 0, 1), ("$t2", 0, 0, 0, 1, 1, 0, 0),
                                  ("$t3", "$src1", 0, 0, 2, 2, 0, 1)], _CST, args=_CARGS)

# ---------------------------------------------------------------------------------------------------- decodeSprite4827d0
_CHUNKS = ["d_copy", "d_fill", "d_skip", "d_last", "d_zero", "d_empty", "d_m128", "d_mixed", "d_bigskip", "d_many"]
for _s in (16, 9):
    HOOKS[f"decodeSprite4827d0_s{_s}"] = _h(0x004827D0, f"c3ee_spr_{_s}",
                                             [("$flic", f"${c}", i * 0x101, "$surf") for i, c in enumerate(_CHUNKS)],
                                             _OBJS, args=("pointer", "pointer", "int", "pointer"))

# ---------------------------------------------------------------------------------------------------- Image::loadPcx / savePcx
_LPV = [("$i0", f"$f{i}") for i in range(12)] + [("$i1", "$f0"), ("$i2", "$f0"), ("$i3", "$f0"), ("$i4", "$f0"), ("$i1", "$f2"),
                                                 ("$i1", "$f9")]
HOOKS["Image::loadPcx"] = _h(0x00485790, "c3ee_lpcx", _LPV, _LIST, args=("pointer", "pointer"))
HOOKS["Image::loadPcx_fail"] = _h(0x00485790, "c3ee_lpcx_fail", [("$i0", "$f0"), ("$i0", "$f3"), ("$i1", "$f1"), ("$i0", "$f6")],
                                  _LIST, args=("pointer", "pointer"))
_SPV = [("$i0", "$f0"), ("$i0", "$f1"), ("$i0", "$f2"), ("$i1", "$f0")]
for _k in ("", "_p1", "_p2", "_h0"):
    HOOKS[f"Image::savePcx{_k}"] = _h(0x00485AA0, f"c3ee_spcx{_k}", _SPV, _LIST, args=("pointer", "pointer"))

# ---------------------------------------------------------------------------------------------------- Image_draw / Surface_drawScaled load arm
_DLST = [("$surfs", 0, 0x40 * 8)] + _OBJS + _HEAP + _FILES + _PAINT + [(0x00839348, 0, 0x2C0)]
_IDV = [("$t0", "$src", 5, 7, 0), ("$t1", "$src", 0, 0, 0), ("$t2", "$src", 1, 1, 0), ("$t3", "$src", -4, 9, "$src"),
        ("$t4", "$src", 2, 3, 0), ("$t5", "$src", 8, 8, 0), ("$t0", 0, 3, 3, 0), ("$t0", "$srcnh", 6, 2, 0)]
HOOKS["Image_draw_load"] = _h(0x00473E60, "c3ee_drawload", _IDV, _DLST, args=("pointer", "pointer", "int", "int", "pointer"))
HOOKS["Image_draw_load_fail"] = _h(0x00473E60, "c3ee_drawload_fail", _IDV[:3] + _IDV[6:], _DLST,
                                   args=("pointer", "pointer", "int", "int", "pointer"))
_DSV = [("$t0", "$src", 5, 7, 1, 1, 1, 0), ("$t1", "$src", 0, 0, 2, 3, 4, 0), ("$t2", "$src", 1, 1, 1, 1, 1, 0),
        ("$t3", "$src", -4, 9, 3, 3, 2, "$src"), ("$t4", "$src", 2, 3, 1, 2, 1, 0), ("$t5", "$src", 8, 8, 1, 1, 1, 0),
        ("$t0", 0, 3, 3, 1, 1, 1, 0), ("$t0", "$srcnh", 6, 2, 1, 1, 1, 0)]
_DSA = ("pointer", "pointer", "int", "int", "int", "int", "int", "pointer")
HOOKS["Surface_drawScaled_load"] = _h(0x00473CB0, "c3ee_drawload", _DSV, _DLST, args=_DSA)
HOOKS["Surface_drawScaled_load_fail"] = _h(0x00473CB0, "c3ee_drawload_fail", _DSV[:3] + _DSV[6:], _DLST, args=_DSA)

# ---------------------------------------------------------------------------------------------------- loadFileWinResource
# argument-free: one call per key, one key per stand-in setting (readSectionGlobal / readLineGlobal results, the block)
for _k in ("ok", "long", "empty", "start", "prev", "sect", "sectneg", "noline", "nomem", "nomem2"):
    HOOKS[f"loadFileWinResource_{_k}"] = _h(0x0049D3B0, f"c3ee_fw_{_k}", [()],
                                            [("$rec", 0, REC)] + _HEAP + [(0x00840820, 0, 4)], abi="mscdecl", args=())
