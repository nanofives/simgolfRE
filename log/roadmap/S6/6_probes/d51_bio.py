"""D51: buildGolferBioText 0x004315e0, the twelve arrival picks 0x00431719..0x0043180a (Random::range(0x4c) until the type fits).
The type table (0x4d60a8, stride 0x230) is a constant of the exe, so the picks complete for every RNG or hang for every RNG."""
import plib as P

f = P.IMG.data


def tbl(t, off):
    return f[0x4d60a8 + t * 0x230 + off - 0x400000]


need = {0: 5, 1: 3, 2: 6, 3: 7}


def klass(t):
    if tbl(t, 1) & 0x80:
        return 6
    b = tbl(t, 0)
    s = 1
    if b & 0x11:
        s = 3
    if b & 0x0C:
        s |= 4
    if s == 1:
        s |= (3 - 4) if False else (2 if t & 1 == 0 else -1) and 0
        s = 3 if (t & 1) else 5
    return s


# static model of the loop (0x00431742..0x0043180a)
used = set()
ok = True
for i in range(12):
    cands = [t for t in range(0x4c) if t % 3 == i % 3 and t not in used and klass(t) == need[i // 3] if True]
    print(f"pick {i}: residue {i % 3}, needs class {need[i // 3]}, candidates {len(cands)}: {cands[:8]}")
    if not cands:
        ok = False
        break
    used.add(cands[0])
print("static model: every pick has a candidate:", ok)

# emulator cross-check: cycle the draws 0..0x4b
for scen in ("course_season",):
    data = P.start_image(scen, "00")
    cyc = lambda n, k: (k % 0x4c) if n == 0x4c else 0
    seen = []
    r = P.emu(0x4315e0, [], data, max_insns=40_000_000, hooks=[P.script_random(cyc), P.visit([0x431810], seen)])
    P.show(r, f"{scen} draws cycle 0..0x4b", 0)
    print("   reached the end of the picks (0x431810):", bool(seen))
