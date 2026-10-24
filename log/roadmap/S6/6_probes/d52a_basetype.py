"""D52a: generateRandomCourse 0x00470a60, base tile type [esp+0x10] read at 0x00470b91 without a write for course type 4 (n < 8 or g_flags bit 24 set)
and for course types above 4 (jump table 0x472208 holds 0x00470b4e for types 0..3, 0x00470b54 for 4, ja 0x00470b65 above)."""
import plib as P

REC, RS = 0x571ff4, 0x2e


def build(scen, n, ctype, flag24=False):
    im = P.Img(P.start_image(scen, "00"))
    im.w(REC + n * RS + 4, ctype, 1)        # [rec + 0x571ff8]
    im.w(REC + n * RS + 2, 0, 1)            # [rec + 0x571ff6]
    fl = im.r(0x59e7b8, 4) & ~0x1000000
    if flag24:
        fl |= 0x1000000
    im.w(0x59e7b8, fl, 4)
    return im


def run(label, im, n, fill, budget=60_000_000):
    seen = []
    r = P.emu(0x470a60, [n], im.done(), max_insns=budget, stack_fill=fill, hooks=[P.visit([0x470b91, 0x470c1f], seen)])
    t = bytes(r.writes.get(a, (0, 0))[1] for a in range(0x5722e8, 0x5722e8 + 0x9c4)) if r.writes else b""
    # first tile byte written by the base fill (0x470bb7 rep stosd over 0x5722e8)
    data0 = P.Img(im.done()).rb(0x5722e8, 4)
    print(f"[{label} stack fill {fill if fill is None else hex(fill)}] {r.status} eip={r.eip:#x} err={r.err} visited_fill_site={bool(seen)}")
    return r


for scen in ("sandbox_basic",):
    for (n, ctype, fl) in ((5, 4, False), (9, 4, False), (9, 4, True), (9, 5, True), (9, 3, False)):
        for fill in (0x00, 0x55):
            im = build(scen, n, ctype, fl)
            seen = []
            res = P.emu(0x470a60, [n], im.done(), max_insns=400_000_000, stack_fill=fill, hooks=[P.visit([0x470b91, 0x470c1f], seen)])
            # read the first tile type after the base fill: take it from the written bytes if the run wrote it
            tt = res.writes.get(0x5722e8)
            P.show(res, f"{scen} n={n} course type {ctype} flag24={fl} stack fill {fill:#x}", 0)
            print("    first tile type byte 0x5722e8 after run:", None if tt is None else hex(tt[1]), " (control: type 0..3 courses fill 4)")
