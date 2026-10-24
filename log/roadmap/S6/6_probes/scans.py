"""Static scans behind rows D2, D3, D4, D13, D47b, D49 of 6_decisions.tsv (file image only, no emulation). Output: scans.log"""
import glob
import re
import plib as P

insns = P._all_insns()
idx = {a: i for i, (a, f, m, o) in enumerate(insns)}


def arg0_sites(target):
    out = []
    for a, f, m, o in insns:
        if m == "call" and o == target:
            k = idx[a] - 1
            while k >= max(0, idx[a] - 30) and insns[k][2] != "push":
                k -= 1
            out.append((a, P.IMG.func_starts[f], insns[k][3]))
    return out


print("== D2 playSound 0x4481b0 / startSoundSlot 0x448200: first argument of each call site")
sites = arg0_sites("0x4481b0") + arg0_sites("0x448200")
imm = [(a, n, r) for a, n, r in sites if re.fullmatch(r"-?(0x[0-9a-f]+|\d+)", r)]
vals = [int(r, 16) if "0x" in r else int(r) for a, n, r in imm]
print(f"call sites {len(sites)}, immediate {len(imm)} (min {min(vals)}, max {max(vals)}), computed {len(sites) - len(imm)}")
for a, n, r in sites:
    if (a, n, r) not in imm:
        print("  computed:", hex(a), n, r)

print("== D49 postEvent 0x46e7b0: first argument of each call site")
for a, n, r in arg0_sites("0x46e7b0"):
    print(" ", hex(a), n, r)

print("== D4 comparisons against the sprite queue count")
for name, lo, hi in (("addSprite", 0x4628d0, 0x462a29), ("enqueueSprite", 0x462a30, 0x462be0)):
    print(name, [(hex(a), o) for a, f, m, o in insns if lo <= a < hi and m == "cmp"])

print("== D3 movie list: references")
P.pr_refs(0x8406e4, 0x8406e8)
P.pr_refs(0x840698, 0x8406e4)

print("== D13 type names at 0x4d6098 + t * 0x230 (76 types)")
f = P.IMG.data
im = P.Img(P.start_image())
names = []
for t in range(76):
    a = 0x4d6098 + t * 0x230
    names.append((len(f[a - 0x400000:a - 0x400000 + 0x230].split(b"\0")[0]), f[a - 0x400000:a - 0x400000 + 0x230].split(b"\0")[0].decode("latin-1")))
print("longest:", sorted(names, reverse=True)[:3], " sentinel:", f[0x4d32c4 - 0x400000:0x4d32d7 - 0x400000])

print("== D47b story files under original/Themes: longest header + reply (+ newline + NUL)")
worst = (0, "")
for p in glob.glob(str(P.ROOT / "original" / "Themes" / "*" / "*.txt")):
    lines = [l.rstrip("\r") for l in open(p, "rb").read().decode("latin-1").split("\n")]
    heads = [l for l in lines if len(l) > 1 and not l.startswith(" ")]
    reps = [l for l in lines if len(l) > 1 and l.startswith(" ")]
    if heads and reps:
        w = max(map(len, heads)) + max(map(len, reps)) + 2
        if w > worst[0]:
            worst = (w, p)
print(worst)

print("== D50 tile type flags (exe constant table 0x4c1a40 + t * 0x30 + 0x2c)")
fl = [int.from_bytes(f[0x4c1a40 + t * 0x30 + 0x2c - 0x400000:0x4c1a40 + t * 0x30 + 0x30 - 0x400000], "little") for t in range(23)]
print([hex(x) for x in fl])
print("== D1 grid readers"); P.pr_refs(0x5830b8, 0x583a7c)
