"""Batch c3ff (round 33, jgld.dll): saveImage / writePcx8, createDisplay, drawTo / drawToZ, loadPng.

Reimplementations shim/src/re/c3ff.cpp, fixtures re/frida/js/fixtures.d/c3ff.js, purposes log/c3/c3ff_purpose.md.

Every fixture freezes the game's first thread (re/frida/js/freeze.js) and calls __freeze.negOn() after it, so the
negative-control build's wrong bodies are armed only inside a key. Common state of every key:
  $sblk   the fixture block: synthetic Surfaces on recording vtables, palette objects, the palette client, pixel buffers
  $vlog   the recording vtables' call log (+0 records, +4 bytes used, +8 FNV-1a of every record, records from +0x10)
  $fmlog  framemask.js's block (frame pointers handed to a recorder are logged as object tokens)
"""

HOOKS = {}

_J = dict(module="jgld.dll")
SBLK, VLOG, FMLOG = 0x8000, 0x2000, 0x1000
_COMMON = [("$sblk", 0, SBLK), ("$vlog", 0, VLOG), ("$fmlog", 0, FMLOG)]

# ============================================================================ group F: Surface::writePcx8, saveImage
# The file stand-in block (16 files of 0x800 bytes, fs.size 0x9000) and its import log (CreateFileA / SetFilePointer /
# SetEndOfFile / CreateFileMappingA / MapViewOfFile / UnmapViewOfFile / CloseHandle / GetLastError).
FS_SIZE, IMPLOG = 0x9000, 0x400
_FILE = _COMMON + [("$fsblock", 0, FS_SIZE), ("$implog", 0, IMPLOG)]
# fixture.d/c3ff.js: S0..S8 8-bit surfaces (S5 has no pixels), S9 / S10 / S11 depth 16 / 0 / 24 for saveImage;
# N0..N7 names: img1.bmp, a.b.c, noext, .x, dir\pic.raw, trail., missing.bmp (no such file), "" (empty).
_WP_CLIENT = [("$S0", "$N0"), ("$S1", "$N1"), ("$S2", "$N2"), ("$S3", "$N3"), ("$S4", "$N4"), ("$S5", "$N0"),
              ("$S6", "$N5"), ("$S7", "$N0"), ("$S8", "$N1"), ("$S0", "$N6"), ("$S2", "$N7"), ("$S6", "$N4"),
              ("$S1", "$N2")]
_WP_NULL = [("$S2", "$N0"), ("$S4", "$N1"), ("$S0", "$N2"), ("$S1", "$N3"), ("$S2", "$N6"), ("$S5", "$N5"),
            ("$S7", "$N4"), ("$S8", "$N0"), ("$S6", "$N1"), ("$S3", "$N5"), ("$S2", "$N3")]
HOOKS["Surface::writePcx8_client"] = dict(_J, addr=0x00012140, abi="thiscall", ret="int",
                                          args=["pointer", "pointer"], fixture="c3ff_file_client", state=_FILE,
                                          vectors=_WP_CLIENT)
HOOKS["Surface::writePcx8_clientnull"] = dict(_J, addr=0x00012140, abi="thiscall", ret="int",
                                              args=["pointer", "pointer"], fixture="c3ff_file_clientnull",
                                              state=_FILE, vectors=_WP_NULL)
HOOKS["Surface::saveImage"] = dict(_J, addr=0x0000A550, abi="thiscall", ret="int", args=["pointer", "pointer"],
                                   fixture="c3ff_file_client", state=_FILE,
                                   vectors=[("$S0", "$N0"), ("$S9", "$N0"), ("$S10", "$N1"), ("$S11", "$N2"),
                                            ("$S2", "$N2"), ("$S5", "$N3"), ("$S1", "$N6"), ("$S6", "$N4"),
                                            ("$S9", "$N6"), ("$S7", "$N5"), ("$S12", "$N1"), ("$S4", "$N3")])

# ============================================================================ group D: createDisplay
# Argument-free cdecl: each vector passes one dummy int "$rN" (popped by the caller) whose getter sets, before the
# snapshot, the heap stand-in's cursor and fail mask and marker words in the three module globals it writes:
# 0x1012870c and the module display 0x10128420 (createDisplay), 0x10128728 (DisplayMember::ctor).
_DISP = [("$arena", 0, 0x10 + 0x800), ("$freelog", 0, 0x404), ("$g70c", 0, 4), ("$g420", 0, 4), ("$g728", 0, 4)]
HOOKS["createDisplay"] = dict(_J, addr=0x00065390, abi="default", ret="pointer", args=["int"],
                              fixture="c3ff_display", state=_DISP, vectors=[(f"$r{i}",) for i in range(8)])

# ============================================================================ group S: Surface::drawTo / drawToZ
# Sprite::ctorN / Sprite::dtor / Sprite::drawB / Sprite::drawG stood in by framemask.js: the frame Sprite is declared
# by the ctorN stand-in (0x50 bytes, filled with object tokens), the drawB / drawG and dtor stand-ins log its 20 words.
# T0..T5 surfaces; dst is a plain value (the stand-in only records it).
_DT = [("$T0", 0x0D570000, 0, 0), ("$T1", 0x0D570001, 5, 7), ("$T2", 0, -3, 2), ("$T3", 0x0D570003, 100, -100),
       ("$T4", 0x0D570004, 0x7FFFFFFF, -0x80000000), ("$T5", 0x0D570005, 1, 1), ("$T0", 0x0D570006, 9, 8),
       ("$T1", 0, -1, -1), ("$T2", 0x0D570008, 31, 15), ("$T3", 0x0D570009, 2, 3), ("$T4", 0x0D57000A, 4, 4),
       ("$T5", 0x0D57000B, 6, 0)]
HOOKS["Surface::drawTo"] = dict(_J, addr=0x00009FD0, abi="thiscall", ret="int",
                                args=["pointer", "pointer", "int", "int"], fixture="c3ff_draw", state=_COMMON,
                                vectors=_DT)
HOOKS["Surface::drawToZ"] = dict(_J, addr=0x0000A330, abi="thiscall", ret="int",
                                 args=["pointer", "pointer", "int", "int", "int"], fixture="c3ff_draw", state=_COMMON,
                                 vectors=[v + (z,) for v, z in zip(_DT, (0, 1, -1, 7, 0x7FFFFFFF, -0x80000000, 3, 9,
                                                                         100, -5, 2, 0x1234))])

# ============================================================================ group P: loadPng
# State: the heap arena (its header, and its body from +0x50: the png_struct is always the first block, at +0x10, and
# its jmp_buf (+0x10..+0x4f) holds the arm's own registers), the free log, the file stand-in block and import log
# (OutputDebugStringA through trace 0x10014550, WriteFile through jgld's stderr), jgld's stderr FILE and its buffer.
ARENA = 0x20000
_PNG = _COMMON + [("$arena", 0, 0x10), ("$arena", 0x50, ARENA - 0x40), ("$freelog", 0, 0x404),
                  ("$fsblock", 0, 0x4000), ("$implog", 0, 0x800), ("$giob", 0, 0x20), ("$gbuf", 0, 0x1000)]
# files (fixture c3ff_png): F0 rgb8.png 4x3, F1 rgb16.png 3x2, F2 ilace.png 5x5 Adam7, F3 gray.png, F4 rgba.png,
# F5 pal.png, F6 badsig.png, F7 trunc.png, F8 badz.png, F9 badcrc.png (IDAT CRC wrong), F10 warn.png (ancillary chunk
# with a wrong CRC before IDAT), F11 crit.png (unknown critical chunk), F12 missing.png (not in the stand-in).
_LP = [("$U0", "$F0", 0, 0, 0), ("$U1", "$F1", "$P", 1, 2), ("$U0", "$F2", 0, 0, 0), ("$U2", "$F3", "$P", 0, 0x100),
       ("$U1", "$F4", 0, 0, 0), ("$U0", "$F5", "$P", 3, 4), ("$U0", "$F5", 0, 0, 0), ("$U2", "$F6", 0, 0, 0),
       ("$U0", "$F7", "$P", 0, 0), ("$U1", "$F8", 0, 0, 0), ("$U0", "$F9", 0, 0, 0), ("$U2", "$F10", "$P", 5, 6),
       ("$U0", "$F11", 0, 0, 0), ("$U1", "$F12", 0, 0, 0), ("$U2", "$F0", "$P", 7, 8), ("$U1", "$F2", 0, 1, 1)]
HOOKS["loadPng"] = dict(_J, addr=0x000145A0, abi="thiscall", ret="int",
                        args=["pointer", "pointer", "pointer", "int", "int"], fixture="c3ff_png", state=_PNG,
                        vectors=_LP)
# Allocation failures: a4 = "$fK", whose getter sets the arena's fail mask to 1 << K before the snapshot (a4 is read
# only by the palette arm, which these files never reach). Call 6 is jalloc's row table, written without a NULL test:
# it is never failed.
HOOKS["loadPng_oom"] = dict(_J, addr=0x000145A0, abi="thiscall", ret="int",
                            args=["pointer", "pointer", "pointer", "int", "int"], fixture="c3ff_png", state=_PNG,
                            vectors=[("$U0", "$F0", 0, 0, f"$f{k}") for k in (0, 1, 2, 3, 4, 5, 7, 8, 9)]
                            + [("$U0", "$F3", 0, 0, "$f5"), ("$U1", "$F2", 0, 0, "$f8")])
