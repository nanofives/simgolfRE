# Batch c3dd (round 25, 2026-10-10): golf_clean.exe functions that reach the heap through heapAlloc 0x00474860 and its
# _realloc 0x004a6980 growth step (dropped by c3cz for want of a realloc entry in the heap stand-in).
# Reimplementations shim/src/re/c3dd.cpp, fixtures re/frida/js/fixtures.d/c3dd.js, purposes log/c3/c3dd_purpose.md.
#
# Every key runs under the fixture's heap stand-in (re/frida/js/heapstandin.js: malloc 0x004a504f and operator new
# 0x004a57f1 carve from an arena whose header - cursor, call counter, fail mask, fail size - is state; free 0x004a5007
# only records; _realloc 0x004a6980 carves a new block, copies the old contents and records the old pointer), so both
# arms receive the same blocks, a scheduled NULL hits both arms alike, and the frees / reallocs are compared state.
# 0x00839650 (the placement-heap global S3List_add's constructors read and clear) is saved and cleared under the shared
# freeze helper (re/frida/js/freeze.js) and listed in that key's state.
#
# Hooked callees (the verifier re-runs these with the callee hooks off):
#   THIS batch:  strDup -> heapAlloc; strAssign -> strDup; bufferCopyTo, Surface_get, View_attach, S3List_add,
#                S3List_add2 -> heapAlloc; TextView::addImage, postMessageLine, listAddEntry -> S3List_add
#   earlier:     strAssign -> strFree 0x0049d0e0 (c3cn), Buf_alloc 0x00474820 (c3cz) and through it Buf_releaseData
#                0x004747e0 (c3cn), Buf_init 0x00474780 (c3n); heapAlloc -> appendNewline 0x004925f0 (c3r, the
#                out-of-memory arm only)

EXE = "golf_clean.exe"
BLK = 0x20000
ARENA = 0x8000


def _st(extra=()):
    return [("$blk", 0, BLK), ("$arena", 0, ARENA + 0x10), ("$freelog", 0, 0x404), ("$rec", 0, 0x400)] + list(extra)


_G = [(0x00839650, 0, 4)]


def _h(addr, abi, ret, args, fixture, vectors, extra=()):
    return dict(module=EXE, addr=addr, abi=abi, ret=ret, args=args, fixture=fixture, state=_st(extra), vectors=vectors)


_HEAP = [("$h0", 0x10), ("$h0", 0), ("$h6", 0x400), ("$h6", 0), ("$h1", 0x100), ("$h2", 0x900), ("$h3", 0x100),
         ("$h3", 0x10), ("$h3", -5), ("$h4", 0x100), ("$h4", 0x500), ("$h5", 0x20), ("$h7", 0x401), ("$h8", 8),
         ("$h9", -0x40), ("$h9", 0), ("$h10", 0x500), ("$h11", 0x400), ("$h12", 0x10), ("$h12", 0x500)]
_DUP = [("$hf", "$s0"), ("$hf", "$s1"), ("$hf", "$s2"), ("$hf", "$s4"), ("$hf", "$s3"), ("$hs", "$s2"), ("$hs", "$s1"),
        ("$hz", "$s2"), ("$hz", "$s6"), ("$hflag", "$s2"), ("$hflag", "$s5"), ("$hnull", "$s1"), ("$hnull", "$s6"),
        ("$hq", "$s6"), ("$hs", "$s7")]
_REC = [(f"$r{i}", f"$d{i}") for i in range(16)]
_ASSIGN = [("$a0", 0x10), ("$a0", 5), ("$a0", 6), ("$a0", 0), ("$a0", 0x400), ("$a0", 0x333), ("$a1", 0x10),
           ("$a1", 5), ("$a2", 0x10), ("$a2", 1), ("$a3", 7), ("$a3", 0x333), ("$a4", 5), ("$a4", 0x20), ("$a5", 0),
           ("$a1", 0x333)]
_ADD = [(f"$l{i}", 0x11 + i) for i in range(12)] + [("$lz", 0x31), ("$lz2", 0x32)]
_ADD_F0 = [("$l0", 0x41), ("$l3", 0x42), ("$l6", 0x43), ("$l10", 0x44)]
_ADD_F1 = [("$l0", 0x51), ("$l3", 0x52), ("$l6", 0x53), ("$l10", 0x54)]
_ATTACH = [("$u0", "$v0"), ("$u0", "$v1"), ("$u0", "$v2"), ("$u1", "$v3"), ("$u2", "$v5"), ("$u0", "$v6"),
           ("$u3", "$v7"), ("$u4", "$v0"), ("$u5", "$v0"), ("$u6", 0), ("$u6", "$v1"), ("$u7", "$v4"),
           ("$u0", "$v4"), ("$u1", "$v0")]
_ATTACH_F0 = [("$u0", "$v0"), ("$u0", "$v2"), ("$u1", "$v5")]
_ATTACH_F1 = [("$u0", "$v0"), ("$u0", "$v2"), ("$u1", "$v5")]
_IMAGE = [("$t0", 0), ("$t0", 0x55), ("$t1", 0x56), ("$t2", 0x57), ("$t3", 0x58), ("$t4", 0), ("$t5", 0x59),
          ("$t4", 0x5a)]
_IMAGE_F = [("$t0", 0x55), ("$t2", 0x57), ("$t5", 0x59), ("$t4", 0x5b)]
_POST = [(f"$l{i}", 1, 0x61 + i, 2, 3) for i in (0, 3, 6, 10, 8, 11, 7, 4, 9, 5, 1, 2)] + [("$lz2", 5, 0x70, 6, 7)]
_POST_F0 = [("$l0", 1, 0x41, 2, 3), ("$l3", 4, 0x42, 5, 6)]
_POST_F1 = [("$l0", 1, 0x51, 2, 3), ("$l6", 4, 0x52, 5, 6)]
_ENTRY = [("$e0", 0x101, "$s2", 0x201), ("$e6", 0x102, 0, 0x202), ("$e1", 0x103, "$s1", 0x203),
          ("$e2", 0x104, "$s4", 0x204), ("$e3", 0x105, "$s0", 0x205), ("$e4", 0x106, "$s5", 0x206),
          ("$e5", 0x107, "$s1", 0x207), ("$e7", 0x108, "$s2", 0x208), ("$e8", 0x109, "$s6", 0x209),
          ("$e9", 0x10a, 0, 0x20a)]
_ENTRY_F0 = [("$e0", 0x101, "$s2", 0x201), ("$e6", 0x102, 0, 0x202), ("$e1", 0x103, "$s1", 0x203),
             ("$e7", 0x104, "$s0", 0x204)]
_ENTRY_F1 = [("$e0", 0x101, "$s2", 0x201), ("$e6", 0x102, "$s1", 0x202), ("$e1", 0x103, "$s1", 0x203),
             ("$e9", 0x104, "$s4", 0x204)]

HOOKS = {
    "heapAlloc": _h(0x00474860, "thiscall", "pointer", ["pointer", "int"], "c3dd_heaps", _HEAP),
    "strDup": _h(0x0049D100, "thiscall", "pointer", ["pointer", "pointer"], "c3dd_strdup", _DUP),
    "strAssign": _h(0x0049D090, "thiscall", "int", ["pointer", "uint32"], "c3dd_strassign", _ASSIGN),
    "bufferCopyTo": _h(0x004A0540, "thiscall", "int", ["pointer", "pointer"], "c3dd_bcopy", _REC),
    "Surface_get": _h(0x00474650, "thiscall", "int", ["pointer", "pointer"], "c3dd_sget", _REC),
    "S3List_add": _h(0x00401D10, "thiscall", "int", ["pointer", "int"], "c3dd_lists", _ADD, _G),
    "S3List_add_fail0": _h(0x00401D10, "thiscall", "int", ["pointer", "int"], "c3dd_lists_fail0", _ADD_F0, _G),
    "S3List_add_fail1": _h(0x00401D10, "thiscall", "int", ["pointer", "int"], "c3dd_lists_fail1", _ADD_F1, _G),
    "S3List_add2": _h(0x00402280, "thiscall", "int", ["pointer", "int"], "c3dd_lists", _ADD, _G),
    "S3List_add2_fail0": _h(0x00402280, "thiscall", "int", ["pointer", "int"], "c3dd_lists_fail0", _ADD_F0, _G),
    "S3List_add2_fail1": _h(0x00402280, "thiscall", "int", ["pointer", "int"], "c3dd_lists_fail1", _ADD_F1, _G),
    "View_attach": _h(0x0047ABE0, "thiscall", "void", ["pointer", "pointer"], "c3dd_views", _ATTACH),
    "View_attach_fail0": _h(0x0047ABE0, "thiscall", "void", ["pointer", "pointer"], "c3dd_views_fail0", _ATTACH_F0),
    "View_attach_fail1": _h(0x0047ABE0, "thiscall", "void", ["pointer", "pointer"], "c3dd_views_fail1", _ATTACH_F1),
    "TextView::addImage": _h(0x0048DFC0, "thiscall", "int", ["pointer", "int"], "c3dd_textviews", _IMAGE, _G),
    "TextView::addImage_fail": _h(0x0048DFC0, "thiscall", "int", ["pointer", "int"], "c3dd_textviews_fail", _IMAGE_F,
                                  _G),
    "postMessageLine": _h(0x004A0600, "thiscall", "int", ["pointer", "int", "int", "int", "int"], "c3dd_lists", _POST,
                          _G),
    "postMessageLine_fail0": _h(0x004A0600, "thiscall", "int", ["pointer", "int", "int", "int", "int"],
                                "c3dd_lists_fail0", _POST_F0, _G),
    "postMessageLine_fail1": _h(0x004A0600, "thiscall", "int", ["pointer", "int", "int", "int", "int"],
                                "c3dd_lists_fail1", _POST_F1, _G),
    "listAddEntry": _h(0x004A28A0, "thiscall", "int", ["pointer", "int", "pointer", "int"], "c3dd_entries", _ENTRY, _G),
    "listAddEntry_fail0": _h(0x004A28A0, "thiscall", "int", ["pointer", "int", "pointer", "int"], "c3dd_entries_fail0",
                             _ENTRY_F0, _G),
    "listAddEntry_fail1": _h(0x004A28A0, "thiscall", "int", ["pointer", "int", "pointer", "int"], "c3dd_entries_fail1",
                             _ENTRY_F1, _G),
}

# ---------------------------------------------------------------- Part 2: destructors and constructors over the heap
_DISP = [(0x0083AD50, 0, 4)]
_FLIC = [(f"$f{i}",) for i in range(16)]
_SCALAR = [(f"$f{i}", i & 1) for i in range(16)]
_IMG = [(f"$i{i}",) for i in range(12)]
_IMG_ST = _DISP + [(0x00839AB0, 0, 4), (0x0083AD44, 0, 4)]
_GS = [(f"$g{i}",) for i in range(4)]

HOOKS.update({
    "Stream482dd0::dtor": _h(0x00482DD0, "thiscall", "void", ["pointer"], "c3dd_flic_disp", _FLIC, _DISP),
    "Flic::reset": _h(0x00481BA0, "thiscall", "void", ["pointer"], "c3dd_flic_disp", _FLIC, _DISP),
    "R2C404280_dtor": _h(0x00404280, "thiscall", "void", ["pointer"], "c3dd_flic_disp", _FLIC, _DISP),
    "scalarDtor_404280": _h(0x004042F0, "thiscall", "pointer", ["pointer", "int"], "c3dd_flic_disp", _SCALAR, _DISP),
    "Image_releaseAll": _h(0x00474CB0, "thiscall", "void", ["pointer"], "c3dd_images_a", _IMG, _IMG_ST),
    "Image_releaseAll_skipcache": _h(0x00474CB0, "thiscall", "void", ["pointer"], "c3dd_images_b", _IMG, _IMG_ST),
    "gSurfaceCtor_44ae60": _h(0x0044AE60, "thiscall", "pointer", ["pointer"], "c3dd_gsurf", _GS),
    "gSurfaceCtor_44b060": _h(0x0044B060, "thiscall", "pointer", ["pointer"], "c3dd_gsurf", _GS),
    "gSurfaceCtor_44b2c0": _h(0x0044B2C0, "thiscall", "pointer", ["pointer"], "c3dd_gsurf", _GS),
    "gSurfaceCtor_44b450": _h(0x0044B450, "thiscall", "pointer", ["pointer"], "c3dd_gsurf", _GS),
    "gSurfaceCtor_44b5a0": _h(0x0044B5A0, "thiscall", "pointer", ["pointer"], "c3dd_gsurf", _GS),
    "gSurfaceCtor_44b6f0": _h(0x0044B6F0, "thiscall", "pointer", ["pointer"], "c3dd_gsurf", _GS),
    "gSurfaceCtor_44b8d0": _h(0x0044B8D0, "thiscall", "pointer", ["pointer"], "c3dd_gsurf", _GS),
    "gSurfaceCtor_839a68_a": _h(0x00474A90, "default", "int", [], "c3dd_gsurfg_a", [()], [(0x00839A68, 0, 0x30)]),
    "gSurfaceCtor_839a68_b": _h(0x00474A90, "default", "int", [], "c3dd_gsurfg_b", [()], [(0x00839A68, 0, 0x30)]),
    "gSurfaceCtor_839a68_c": _h(0x00474A90, "default", "int", [], "c3dd_gsurfg_c", [()], [(0x00839A68, 0, 0x30)]),
})

# ---------------------------------------------------------------- netHeapAlloc and the constructor it runs on its payload
HOOKS.update({
    "netHeapAlloc": _h(0x0049B970, "thiscall", "int", ["pointer", "int"], "c3dd_lists", _ADD, _G),
    "netHeapAlloc_fail0": _h(0x0049B970, "thiscall", "int", ["pointer", "int"], "c3dd_lists_fail0", _ADD_F0, _G),
    "netHeapAlloc_fail1": _h(0x0049B970, "thiscall", "int", ["pointer", "int"], "c3dd_lists_fail1", _ADD_F1, _G),
    "VoiceRx::ctor": _h(0x004852E0, "thiscall", "pointer", ["pointer"], "c3dd_voice", [(f"$o{i}",) for i in range(6)]),
})
