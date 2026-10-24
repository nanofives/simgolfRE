"""D52b: generateRandomCourse 0x00470a60, the type-0x11 search at 0x0047184a..0x00471885 (reached for layout kind 1: jump table 0x472244[1] = 0x0047182e) and the clubhouse search
0x004715b9..0x00471616. The hooks force the layout kind to 1 at 0x0047165c (a random draw in the original) and set the 30 x 30 window (x 10..39, y 0..29: the draws at 0x47184a..0x471879) of the tile type grid either without any
type-0x11 cell (hang arm) or with one (control)."""
import random
import plib as P
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ESP

REC, RS = 0x571ff4, 0x2e
TYPE = 0x5722e8


def hooks_for(with_0x11: bool, log):
    def install(uc):
        def force_kind(u, a, s, _):
            u.reg_write(UC_X86_REG_EBP, 1)
            log.append("kind forced to 1")

        def window(u, a, s, _):
            for x in range(10, 40):
                row = bytes(u.mem_read(TYPE + x * 50, 30))
                row = bytes(0 if b == 0x11 else b for b in row)
                u.mem_write(TYPE + x * 50, row)
            if with_0x11:
                u.mem_write(TYPE + 20 * 50 + 20, bytes([0x11]))
            log.append("window prepared")
        uc.hook_add(UC_HOOK_CODE, force_kind, None, 0x47165c, 0x47165c)
        uc.hook_add(UC_HOOK_CODE, window, None, 0x471845, 0x471845)
    return install


base = P.Img(P.start_image("sandbox_basic", "00"))
n, ctype = 9, 3
base.w(REC + n * RS + 4, ctype, 1)
base.w(0x59e7b8, base.r(0x59e7b8, 4) & ~0x1000000, 4)
base.w(0x822c88, 1, 4)                               # g_difficulty 1: 7 passes of the search
for label, w in (("window holds one type-0x11 cell (control)", True), ("window holds no type-0x11 cell", False)):
    for budget in (400_000_000, 1_200_000_000):
        log = []
        im = P.Img(base.done())
        im.w(0x822d9c, 0x12345678, 4)
        r = P.emu(0x470a60, [n], im.done(), max_insns=budget, hooks=[hooks_for(w, log)])
        in_loop = 0x47184a <= r.eip <= 0x471885 or 0x45c1a0 <= r.eip <= 0x45c1f0
        P.show(r, f"{label}, budget {budget:,}", 0)
        print("    hooks fired:", log[:2], " eip in the search loop or Random::range:", in_loop)
        if r.status == "completed":
            break
