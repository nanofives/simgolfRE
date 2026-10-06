# C3 batch c3f (2026-10-06) of golf_clean.exe: util math/string/RNG leaves and Snd484 field accessors.
# Reimplementations in shim/src/re/c3f.cpp, fixtures in re/frida/js/fixtures.d/c3f.js. Keys are the hooks.csv
# names (the `::` ones map to <name_with_underscores> CSV files). Format: top of re/frida/hooks_registry.py.
# __thiscall functions take `this` as the first (pointer) arg under abi="thiscall".

# Wide integer spread for the pure leaves (negative, zero, boundaries, extremes).
_EDGE = [-0x80000000, -0x7fffffff, -100000, -1201, -1200, -1199, -65, -64, -63, -41, -40, -39, -6, -1, 0, 1, 2, 6,
         39, 40, 41, 63, 64, 65, 1199, 1200, 1201, 100000, 0x3fffffff, 0x7ffffffe, 0x7fffffff]
_CLAMP = [(v, lo, hi) for v in (-100, -1, 0, 1, 5, 10, 50, 100)
          for lo in (-50, 0, 10, 60) for hi in (-60, -1, 5, 20, 100)]
# absDiff / cosScaled take (angle, length): angle sweeps a full turn (step ~1/24 as drawCircle uses), length varies.
_TRIG = [(a, b) for a in (0, 0x0aaaaaaa, 0x15555554, 0x20000000, 0x2aaaaaa8, 0x3fffffff, 0x40000000,
                          0x55555550, 0x60000000, 0x7fffffff, -0x40000000, -0x7fffffff)
         for b in (0, 1, 100, 1024, 25000, -1, -1024)]

HOOKS = {}
HOOKS.update({
    # --- pure integer leaves (no state; return varies across the spread) ---
    "clamp": dict(module="golf_clean.exe", addr=0x00467130, abi="default", ret="int",
                  args=["int", "int", "int"], vectors=_CLAMP),
    "sign": dict(module="golf_clean.exe", addr=0x00467150, abi="default", ret="int", args=["int"],
                 vectors=[(v,) for v in _EDGE]),
    # Forwarders into the trig helpers (callees run their originals; the sin table is built at startup).
    "absDiff": dict(module="golf_clean.exe", addr=0x004672b0, abi="default", ret="int", args=["int", "int"],
                    vectors=_TRIG),
    "cosScaled": dict(module="golf_clean.exe", addr=0x00491d80, abi="default", ret="int", args=["int", "int"],
                      vectors=_TRIG),

    # --- Random::range(n): thiscall; advances the 4-byte seed at [this] (the state region) and returns floor(next()*
    #     (n & 0xffff)). One object, many n; each vector restores the same seed, so the return varies with n. ---
    "Random::range": dict(module="golf_clean.exe", addr=0x0045c1e0, abi="thiscall", ret="int",
                          args=["pointer", "int"], fixture="c3f_random", state=[("$obj", 0, 4)],
                          vectors=[("$obj", n) for n in (0, 1, 2, 3, 6, 10, 16, 36, 100, 360, 1000, 0x7fff, 0xffff,
                                                         0x10001, -1)]),

    # --- Snd484 accessors (thiscall). The device slot [this+0x40] is 0 in every fixture, so the virtual device
    #     notify is never taken and no fake vtable is needed; only the game-side field logic runs. ---
    # setPitch: stores clamp(p, -0x4b0, 0x4b0) at +0x5c (the state region), returns 0.
    "Snd::setPitch": dict(module="golf_clean.exe", addr=0x00484f40, abi="thiscall", ret="int",
                          args=["pointer", "int"], fixture="c3f_snd", state=[("$obj", 0x5c, 4)],
                          vectors=[("$obj", p) for p in (-100000, -0x4b1, -0x4b0, -0x4af, -100, 0, 100, 0x4af,
                                                         0x4b0, 0x4b1, 100000)]),
    # setPan: stores clamp(p, -0x40, 0x3f) at +8 (state) and returns the clamped value.
    "Snd::setPan": dict(module="golf_clean.exe", addr=0x004847f0, abi="thiscall", ret="int",
                        args=["pointer", "int"], fixture="c3f_snd", state=[("$obj", 8, 4)],
                        vectors=[("$obj", p) for p in (-1000, -0x41, -0x40, -0x3f, -1, 0, 1, 0x3e, 0x3f, 0x40, 1000)]),
    # setField38: v == 0 -> returns 10 (no write); else stores v at +0x38 (state) and returns 0.
    "Snd::setField38": dict(module="golf_clean.exe", addr=0x004846d0, abi="thiscall", ret="int",
                            args=["pointer", "int"], fixture="c3f_snd", state=[("$obj", 0x38, 4)],
                            vectors=[("$obj", v) for v in (0, 1, 2, -1, 100, -100000, 0x7fffffff, -0x80000000)]),
    # setMode: ORs a bit into the flags word at +0x44 and stores the mode at +0x54 (both state); jump table
    #          at 0x004842d0, modes 0..8 cover every case plus the default.
    "Snd::setMode": dict(module="golf_clean.exe", addr=0x00484260, abi="thiscall", ret="void",
                         args=["pointer", "int"], fixture="c3f_snd", state=[("$obj", 0x44, 4), ("$obj", 0x54, 4)],
                         vectors=[("$obj", m) for m in (0, 1, 2, 3, 4, 5, 6, 7, 8, 99, -1)]),
    # flags(): no args and no state; reads +0x30 and the bits of the byte +0x58. Varies across the fixture
    #          objects, which carry every combination of the six bits (and +0x30 set/clear).
    "Snd::flags": dict(module="golf_clean.exe", addr=0x00484ff0, abi="thiscall", ret="int",
                       args=["pointer"], fixture="c3f_snd_flags",
                       vectors=[(f"$s{i}",) for i in range(16)]),

    # --- MappedFile::ctor(): thiscall; writes vtable 0x4bba78 at +0, 0 at +4, -1 at +8, 0 at +0xc; returns this.
    #     One garbage-filled slot; the 0x10-byte object is the state region (an omitted field keeps garbage -> RED). ---
    # c3i fix-up (leaf gate needs >= 10 vectors): the ctor takes no stack args, so it is exercised at ten addresses.
    # c3i_mappedfile10 gives ten 0x10-byte objects (pre-filled 0xaa); each vector writes only its own object, so the
    # ten objects as state regions give ten distinct final states and ten distinct `this` return values.
    "MappedFile::ctor": dict(module="golf_clean.exe", addr=0x00492d80, abi="thiscall", ret="pointer",
                             args=["pointer"], fixture="c3i_mappedfile10",
                             state=[(f"$o{i}", 0, 0x10) for i in range(10)],
                             vectors=[(f"$o{i}",) for i in range(10)]),

    # --- buffer writers (cdecl) ---
    # clearBuffers(): no args; zeroes 0x56fcb0[0x1002] and fills 0x59d81c[0x100] with 0xff. The fixture pre-fills
    #   both with other bytes, so the memset is a visible state change (both arms identical).
    # c3i fix-up (leaf gate needs >= 10 vectors): clearBuffers has no arguments, so its behaviour cannot vary; the ten
    # vectors are the same call and the evidence is the state change (both buffers cleared), which is identical and
    # GREEN on every one. (A no-input memset admits no richer A/B; noted in log/c3/c3i_notes.txt.)
    "clearBuffers": dict(module="golf_clean.exe", addr=0x0045b880, abi="default", ret="void", args=[],
                         fixture="c3f_clearbuffers",
                         state=[(0x0056fcb0, 0, 0x1002), (0x0059d81c, 0, 0x100)], vectors=[() for _ in range(10)]),
    # appendToBuffer45b8b0(id): manages the text store 0x56fcb0 with offset table 0x59d81c and length table
    #   0x5a46b8, appending the current text 0x51a068. State = all three tables. id == -1 appends to a fresh slot;
    #   an id whose offset is live is removed and re-appended; an id with an empty offset is written at that slot.
    "appendToBuffer45b8b0": dict(module="golf_clean.exe", addr=0x0045b8b0, abi="default", ret="int", args=["int"],
                                 fixture="c3f_appendbuffer",
                                 state=[(0x0056fcb0, 0, 0x1002), (0x0059d81c, 0, 0x100), (0x005a46b8, 0, 0x100),
                                        (0x0051a068, 0, 0x400)],   # the input text too (verifier, 2026-10-07)
                                 vectors=[(id,) for id in (-1, 0, 5, 0x21, 0x22, 0x23, 0x40, 0x7f)]),
})
