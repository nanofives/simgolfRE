"""D50: rebuildHeightfield 0x0042f7a0 relax / raise pass (0x0042f88a..0x0042f8fb). Type flags: dword at 0x57837c + type * 0x30 (bit0 gate, bit1 relaxTile, bit2 raiseFromNeighbours);
the table is copied from the exe's constant table at 0x4c1a40 by mainLoop (0x0040f5fc..0x0040f60e)."""
import random
import plib as P

TYPE = 0x5722e8
FLAGS = 0x57837c


def flags_table():
    f = P.IMG.data
    out = []
    for t in range(23):
        a = 0x4c1a40 + t * 0x30 + 0x2c
        out.append(int.from_bytes(f[a - 0x400000:a - 0x400000 + 4], "little"))
    return out


print("constant table flags per type 0..22:", [hex(x) for x in flags_table()])
print("types with bit0 and bit1 (relax):", [t for t, x in enumerate(flags_table()) if x & 3 == 3],
      " bit0 and bit2 (raise):", [t for t, x in enumerate(flags_table()) if x & 5 == 5],
      " all three:", [t for t, x in enumerate(flags_table()) if x & 7 == 7])

rng = random.Random(1)
for scen in ("sandbox_basic", "course_season"):
    base = P.start_image(scen, "00")

    def mk(types, flag21=None, flag22=None):
        im = P.Img(base)
        for i in range(2500):
            im.w(TYPE + i, rng.choice(types), 1)
        if flag21 is not None:
            im.w(FLAGS + 21 * 0x30, flag21, 4)
        if flag22 is not None:
            im.w(FLAGS + 22 * 0x30, flag22, 4)
        return im

    for label, types, f21, f22 in (("shipped flags, grid of types 21/22/2/4", [21, 22, 2, 4, 0, 1], None, None),
                                   ("shipped flags, grid all type 21", [21], None, None),
                                   ("type 21 flags forced to 7 (control), grid of 21/22/2/4", [21, 22, 2, 4], 7, None),
                                   ("types 21 flags 3 (relax only), grid 21/2", [21, 2], 3, None)):
        for seed in range(3):
            rng.seed(seed)
            im = mk(types, f21, f22)
            # no level data randomness beyond the start image
            r = P.emu(0x42f7a0, [], im.done(), max_insns=60_000_000)
            P.show(r, f"{scen} {label} seed {seed}", 0)
