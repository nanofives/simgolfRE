"""D52b: generateRandomCourse 0x00470a60 over many RNG seeds and course types: which runs complete and which exhaust the budget (and where).
usage: d52b_seeds.py <ctype> <n> <seeds> [budget_millions]"""
import random
import sys
import plib as P

REC, RS = 0x571ff4, 0x2e
ctype, n, seeds = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
budget = int(sys.argv[4]) * 1_000_000 if len(sys.argv) > 4 else 600_000_000
base = P.Img(P.start_image("sandbox_basic", "00"))
base.w(REC + n * RS + 4, ctype, 1)
base.w(0x59e7b8, base.r(0x59e7b8, 4) & ~0x1000000, 4)
rg = random.Random(1000 + ctype * 100 + n)
done = hang = 0
for k in range(seeds):
    im = P.Img(base.done())
    im.w(0x822d9c, rg.getrandbits(32), 4)
    r = P.emu(0x470a60, [n], im.done(), max_insns=budget)
    tag = "completed" if r.status == "completed" else r.status
    print(f"ctype {ctype} n {n} seed#{k} rng={im.r(0x822d9c, 4):#010x}: {tag} eip={r.eip:#x} err={r.err}", flush=True)
    if r.status == "completed":
        done += 1
    else:
        hang += 1
print(f"SUMMARY ctype {ctype} n {n}: completed {done}, not completed {hang} of {seeds}")
