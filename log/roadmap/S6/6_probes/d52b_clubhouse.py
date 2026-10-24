"""D52b second search: generateRandomCourse 0x00470a60, 0x004715b9..0x00471616: draw (si, di) = 15 + Random::range(0x11) twice until the tile type at di + si * 50 equals the base type
[esp + 0x10] and evalPlacementArea 0x0040db90 accepts it. The hook rewrites the 17 x 17 window (15..31) to a type that is not the base type at the loop's entry (0x004715b9)."""
import plib as P
from unicorn import UC_HOOK_CODE

REC, RS = 0x571ff4, 0x2e
TYPE = 0x5722e8


def hooks_for(modify: bool, log):
    def install(uc):
        st = {"done": False}

        def entry(u, a, s, _):
            if st["done"]:
                return
            st["done"] = True
            if modify:
                for x in range(15, 32):
                    u.mem_write(TYPE + x * 50 + 15, bytes([3] * 17))
            log.append("window rewritten" if modify else "window left")
        uc.hook_add(UC_HOOK_CODE, entry, None, 0x4715b9, 0x4715b9)
    return install


base = P.Img(P.start_image("sandbox_basic", "00"))
n, ctype = 9, 3
base.w(REC + n * RS + 4, ctype, 1)
base.w(0x59e7b8, base.r(0x59e7b8, 4) & ~0x1000000, 4)
base.w(0x822c88, 1, 4)
for label, mod in (("control, window as generated", False), ("no cell of the base type in the window", True)):
    log = []
    im = P.Img(base.done())
    im.w(0x822d9c, 0x12345678, 4)
    r = P.emu(0x470a60, [n], im.done(), max_insns=500_000_000, hooks=[hooks_for(mod, log)])
    in_loop = 0x4715b9 <= r.eip <= 0x471616 or 0x45c1a0 <= r.eip <= 0x45c1f0 or 0x40db90 <= r.eip < 0x40e000
    P.show(r, label, 0)
    print("    hooks:", log, " eip in the search loop, Random::range or evalPlacementArea:", in_loop)
