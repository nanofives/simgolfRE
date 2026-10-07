# Batch c3ak: twelve jgld.dll functions (module-aware hooks; addr is the RVA, VA = 0x10000000 + addr).
# Every object the vectors touch is allocated by the fixture (re/frida/js/fixtures.d/c3ak.js); nothing live in the
# running jgld.dll (no Display, Surface, Palette or sound object) is passed in or written.
# Keys that would collide with the exe's own Random hooks ("Random_next" in hooks_registry.py, "Random::range" in
# c3f.py) carry a jgld_ prefix; c3_verify matches the function by (module, addr), not by key.

# Rect readers: 12 RECTs inside one block ($r0 is the block base, the others are $r0+0x10*i), covering positive and
# negative, zero-area, inverted (right < left) and extreme-coordinate rectangles so width/height vary in sign and
# magnitude and the subtraction wraps at both ends of int32.
_RECTS = [f"$r{i}" for i in range(12)]

# (x, y, w, h) for the two rect writers: zero, positive, negative, mixed and overflowing sums.
_XYWH = [(0, 0, 0, 0), (1, 2, 3, 4), (-5, -7, 2, 3), (10, 20, -30, -40), (0x7FFFFFFF, 0, 1, 0),
         (0, 0x7FFFFFFF, 0, 1), (-0x80000000, -0x80000000, -1, -1), (100, 200, 300, 400), (-1, -1, 1, 1),
         (0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF, 0x3FFFFFFF), (7, 0, 0, 7), (0, 0, -1, -1)]

HOOKS = {
    # --- RECT helpers (__cdecl). rectWidth/rectHeight are read-only leaves: 12 vectors each, one per rect. ---
    "rectWidth": dict(module="jgld.dll", addr=0x00009120, abi="default", ret="int", args=["pointer"],
                      fixture="c3ak_rects", vectors=[(r,) for r in _RECTS]),
    "rectHeight": dict(module="jgld.dll", addr=0x00009160, abi="default", ret="int", args=["pointer"],
                       fixture="c3ak_rects", vectors=[(r,) for r in _RECTS]),
    # Writers: one scratch RECT ($w, 16 bytes of state); each vector restores it, so the final state varies only
    # with the arguments.
    "rectFromXYWH": dict(module="jgld.dll", addr=0x00008360, abi="default", ret="void",
                         args=["pointer", "int", "int", "int", "int"], fixture="c3ak_rects",
                         state=[("$w", 0, 0x10)], vectors=[("$w",) + v for v in _XYWH]),
    "setRect4": dict(module="jgld.dll", addr=0x00010390, abi="default", ret="void",
                     args=["pointer", "int", "int", "int", "int"], fixture="c3ak_rects",
                     state=[("$w2", 0, 0x10)], vectors=[("$w2",) + v for v in _XYWH]),

    # --- Sprite / Surface field accessors (__thiscall, this in ecx). 12 objects in one block; $s0 is its base, so
    #     one state region covers them all. getFlag18 reads [this+0x18], whose bit 0 alternates across objects. ---
    "Sprite::getFlag18": dict(module="jgld.dll", addr=0x000180E0, abi="thiscall", ret="int", args=["pointer"],
                              fixture="c3ak_objs", vectors=[(f"$s{i}",) for i in range(12)]),
    "setField28": dict(module="jgld.dll", addr=0x0000AEB0, abi="thiscall", ret="void", args=["pointer", "pointer"],
                       fixture="c3ak_objs", state=[("$t0", 0, 12 * 0x40)],
                       vectors=[(f"$t{i}", v) for i, v in enumerate(
                           (0, 1, 0x7FFFFFFF, 0xFFFFFFFF, 0x1000, 0x10000000, 4, 0x80000000, 0x2C, 0xDEAD0000,
                            0x55555555, 0xAAAAAAAA))]),

    # --- libpng big-endian loads (__cdecl leaves). 12 four-byte windows into one 16-byte pattern buffer, so each
    #     vector sees a different byte quadruple (zero, 0xff, sign bit set and clear, ascending and descending). ---
    "png_get_int_32": dict(module="jgld.dll", addr=0x000793B0, abi="default", ret="int", args=["pointer"],
                           fixture="c3ak_png", vectors=[(f"$b{i}",) for i in range(12)]),
    "png_get_uint_16": dict(module="jgld.dll", addr=0x00079470, abi="default", ret="int", args=["pointer"],
                            fixture="c3ak_png", vectors=[(f"$b{i}",) for i in range(12)]),

    # --- libpng info-chunk setters (__cdecl). Both guard on `png_ptr == NULL || info_ptr == NULL`; the three null
    #     combinations take the early exit, the other nine write the info struct. $png is a plain scratch block:
    #     the functions only test it for null. ---
    "png_set_pHYs": dict(module="jgld.dll", addr=0x0007DFC0, abi="default", ret="void",
                         args=["pointer", "pointer", "uint32", "uint32", "int"], fixture="c3ak_png",
                         state=[("$infoP", 0, 0x80)],
                         vectors=[("$png", "$infoP", 0, 0, 0), ("$png", "$infoP", 1, 2, 1),
                                  ("$png", "$infoP", 0xFFFFFFFF, 0x7FFFFFFF, 0xFF),
                                  ("$png", "$infoP", 2835, 2835, 1), ("$png", "$infoP", 0x100, 0x200, 0x100),
                                  ("$png", "$infoP", 72, 0, 2), ("$png", "$infoP", 0, 96, -1),
                                  ("$png", "$infoP", 0x12345678, 0x9ABCDEF0, 3),
                                  ("$png", "$infoP", 1, 1, 0x7FFFFFFF),
                                  (0, "$infoP", 5, 6, 1), ("$png", 0, 5, 6, 1), (0, 0, 5, 6, 1)]),
    "png_set_PLTE": dict(module="jgld.dll", addr=0x0007E020, abi="default", ret="void",
                         args=["pointer", "pointer", "pointer", "int"], fixture="c3ak_png",
                         state=[("$infoL", 0, 0x80)],
                         vectors=[("$png", "$infoL", "$pal", 0), ("$png", "$infoL", "$pal", 1),
                                  ("$png", "$infoL", "$pal", 256), ("$png", "$infoL", "$pal", 0xFFFF),
                                  ("$png", "$infoL", "$pal", 0x10000), ("$png", "$infoL", "$pal", -1),
                                  ("$png", "$infoL", 0, 16), ("$png", "$infoL", "$pal", 0x7FFFFFFF),
                                  ("$png", "$infoL", "$pal", 0x1234),
                                  (0, "$infoL", "$pal", 4), ("$png", 0, "$pal", 4), (0, 0, "$pal", 4)]),

    # --- Random (__thiscall). 12 seeds in one block ($g0 is its base, so one state region covers all of them);
    #     next() advances its seed and returns it scaled into [0, 1), so both the return and the state vary. ---
    "jgld_Random::next": dict(module="jgld.dll", addr=0x00007530, abi="thiscall", ret="double", args=["pointer"],
                              fixture="c3ak_random", state=[("$g0", 0, 12 * 4)],
                              vectors=[(f"$g{i}",) for i in range(12)]),
    # range(n): one seed object, many n (every vector starts from the same restored seed, so the return varies with
    # n alone), plus the three other seeds to vary the generator side.
    "jgld_Random::range": dict(module="jgld.dll", addr=0x000075B0, abi="thiscall", ret="int",
                               args=["pointer", "int"], fixture="c3ak_random", state=[("$h0", 0, 4 * 4)],
                               vectors=[("$h0", n) for n in (0, 1, 2, 3, 6, 10, 18, 36, 100, 360, 1000, 0x7FFF,
                                                             0xFFFF, 0x10000, 0x10001, -1, -0x80000000)]
                                       + [(f"$h{i}", n) for i in (1, 2, 3) for n in (0, 1, 100, 0xFFFF)]),
}
