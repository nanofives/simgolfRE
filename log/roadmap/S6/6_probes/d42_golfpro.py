"""D42: loadGolfPro 0x0046c970. Divisions at 0x0046cbab (ecx = (d * 5 + 0x14) * 2) and 0x0046cc63 (edi = 4 - d), d = g_difficulty 0x822c88;
rejection loop 0x0046cb70..0x0046cc6f with no filled pro record 1..99 (+0x20 of the 0x38-byte records at 0x58dd50 is 0xff for an empty one)."""
import plib as P

PRO, PS = 0x58dd50, 0x38


def build(scen, filled, d, skill=0x0a):
    im = P.Img(P.start_image(scen, "00"))
    for i in range(100):
        ok = i in filled
        im.w(PRO + i * PS + 0x20, 0x00 if ok else 0xFF, 1)
        im.w(PRO + i * PS + 0x36, skill if ok else 0, 2)
    im.w(0x822c88, d & 0xFFFFFFFF, 4)
    return im


def run(label, im, budget=3_000_000):
    r = P.emu(0x46c970, [], im.done(), max_insns=budget)
    P.show(r, label, 3)
    return r


for scen in ("sandbox_basic", "course_season"):
    print("=====", scen)
    for d in (0, 1, 2, 3):
        run(f"d={d}, pro 1..99 filled", build(scen, set(range(1, 100)), d))
    for d in (-1, 5, 6):
        run(f"d={d}, pro 1..99 filled", build(scen, set(range(1, 100)), d))
    run("d=4 (divisor 4 - d = 0)", build(scen, set(range(1, 100)), 4))
    run("d=-4 (divisor (d * 5 + 0x14) * 2 = 0)", build(scen, set(range(1, 100)), -4))
    run("d=2, only record 1 filled", build(scen, {1}, 2), budget=20_000_000)
    run("d=2, only record 0 filled", build(scen, {0}, 2))
    run("d=2, no record filled", build(scen, set(), 2))
    run("d=2, no record filled, 10x budget", build(scen, set(), 2), budget=30_000_000)
    run("d=5, only record 1 filled", build(scen, {1}, 5))
    run("d=6, only record 1 filled", build(scen, {1}, 6))
