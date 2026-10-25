"""S6.8 D52b3: generateRandomCourse 0x00470a60, the two evalPlacementArea 0x0040db90 loops of the location-feature switch.

  loop A  0x0047172e..0x0047176f  location 4 (jump table 0x472244[4] = 0x0047169f, then `cmp eax, 4` at 0x00471720):
          x = 5 + rand(0x28), y = 5 + rand(0x28), repeat while evalPlacementArea(x, y, 2, 4) == -1
  loop B  0x004718a4..0x004718e7  locations 5, 6, 8, 12, 13, 15 (table slots 0x004718a4):
          x = rand(0x28) + 3, y = rand(0x28) + 3, repeat while evalPlacementArea(x, y, 5, 0xf) == -1
  (0x0047195e is a guard `je` inside the p3 loop, not a search; 0x0047160b is the clubhouse search, D52b2.)

The location id is the byte at 0x571ff4 + n * 0x2e (the 0x1c field of the course record). Hang arm: at the loop entry the flag
word of every cell of the 50 x 50 grid is OR-ed with 0x8080 (evalPlacementArea returns -1 for any area whose interior holds such a
cell: cmp at 0x0040dc7f), so no candidate the draws can produce is accepted. Control arm: the grid as generated. Control 2: the
hang grid with the 0x8080 bits left off in a window (a candidate set of known size).

Usage: ~/bin/py -3.12 d52b3_placement.py [budget]
"""
import sys

import plib as P
from unicorn import UC_HOOK_CODE

REC, RS = 0x571ff4, 0x2e
TYPE, FLAGS = 0x5722e8, 0x53caf0
RANGE = 0x45c1e0
EVAL = (0x40db90, 0x40e000)
LOOPS = {"A": (0x47172e, 0x47176f), "B": (0x4718a4, 0x4718e7)}


def hooks_for(loop, mode, log):
    entry = LOOPS[loop][0]

    def install(uc):
        st = {"entered": 0, "rewritten": False, "evals": 0, "rng": 0}

        def on_entry(u, a, s, _):
            st["entered"] += 1
            if st["rewritten"]:
                return
            st["rewritten"] = True
            if mode in ("hang", "window"):
                raw = bytearray(u.mem_read(FLAGS, 2500 * 2))
                for i in range(2500):
                    raw[2 * i] |= 0x80
                    raw[2 * i + 1] |= 0x80
                if mode == "window":
                    # leave a 8 x 8 block of cells (rows 20..27, columns 20..27) without the bits and make them plain fairway
                    for x in range(20, 28):
                        for y in range(20, 28):
                            raw[2 * (x * 50 + y)] &= 0x7F
                            raw[2 * (x * 50 + y) + 1] &= 0x7F
                u.mem_write(FLAGS, bytes(raw))
            log.append(f"grid {mode}")

        def on_eval(u, a, s, _):
            st["evals"] += 1

        def on_rng(u, a, s, _):
            st["rng"] += 1
        uc.hook_add(UC_HOOK_CODE, on_entry, None, entry, entry)
        uc.hook_add(UC_HOOK_CODE, on_eval, None, EVAL[0], EVAL[0])
        uc.hook_add(UC_HOOK_CODE, on_rng, None, RANGE, RANGE)
        log.append(st)
    return install


def run(base, loc, loop, mode, budget):
    log = []
    im = P.Img(base.done())
    im.w(REC + 9 * RS, loc, 1)
    im.w(0x822d9c, 0x12345678, 4)
    r = P.emu(0x470a60, [9], im.done(), max_insns=budget, hooks=[hooks_for(loop, mode, log)])
    st = [x for x in log if isinstance(x, dict)][0]
    lo, hi = LOOPS[loop]
    in_loop = lo <= r.eip <= hi or RANGE <= r.eip <= RANGE + 0x40 or EVAL[0] <= r.eip < EVAL[1]
    return r, st, in_loop


def main():
    budget = int(sys.argv[1]) if len(sys.argv) > 1 else 300_000_000
    base = P.Img(P.start_image("sandbox_basic", "00"))
    n, ctype = 9, 3
    base.w(REC + n * RS + 4, ctype, 1)
    base.w(0x59e7b8, base.r(0x59e7b8, 4) & ~0x1000000, 4)
    base.w(0x822c88, 1, 4)
    cases = [("A", 4)] + [("B", c) for c in (5, 6, 8, 12, 13, 15)]
    for loop, loc in cases:
        for mode in ("control", "window", "hang"):
            r, st, in_loop = run(base, loc, loop, mode, budget)
            P.show(r, f"loop {loop} loc {loc} {mode} budget {budget:,}", 0)
            print(f"    loop entered {st['entered']}x  evalPlacementArea calls {st['evals']}  Random::range calls {st['rng']}  "
                  f"eip in loop/range/eval: {in_loop}")


main()
