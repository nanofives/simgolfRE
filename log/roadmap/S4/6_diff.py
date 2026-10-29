"""Where does the port differ from the emulated original? 6_diff.py <key> [--max N]: first mismatching vectors, per region the
differing byte offsets with the original's and the port's values (layout A)."""
import argparse
import importlib.util
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("s46_run", ROOT / "log" / "roadmap" / "S4" / "6_run.py")
RUN = importlib.util.module_from_spec(spec)
spec.loader.exec_module(RUN)
SR = RUN.SR
E = SR.E

ap = argparse.ArgumentParser()
ap.add_argument("key")
ap.add_argument("--max", type=int, default=2)
a = ap.parse_args()
lib = RUN.load_lib6()
with E.FixVM() as vm:
    hooks, _ = E.registry()
    if a.key in SR.DERIVED:
        hooks[a.key] = SR.spec_of(a.key)
    lp = SR.LivePort(lib, a.key, False)
    try:
        res = E.KeyRun(vm, a.key, "A", second_arm=lp).run()
    finally:
        lp.close()
    shown = 0
    for i, row in enumerate(res.rows):
        if row.match:
            continue
        print(f"--- vector {i}: orig ret {row.orig} port ret {row.re} error {row.error}")
        regs = res.regions
        o = (row.state_hex or "").split("|")
        p = (row.state_re_hex or "").split("|")
        for (base, n), ho, hp in zip(regs, o, p):
            if ho == hp:
                continue
            bo, bp = bytes.fromhex(ho), bytes.fromhex(hp)
            diffs = [k for k in range(min(len(bo), len(bp))) if bo[k] != bp[k]]
            print(f"  region 0x{base:08x} +{n:#x}: {len(diffs)} bytes differ")
            for k in diffs[:24]:
                print(f"    +{k:#x} (0x{base + k:08x}): orig {bo[k]:02x} port {bp[k]:02x}")
            if diffs:
                lo = max(0, diffs[0] - 8) & ~7
                print(f"    context +{lo:#x}: orig {bo[lo:lo + 24].hex()}  port {bp[lo:lo + 24].hex()}")
        shown += 1
        if shown >= a.max:
            break
