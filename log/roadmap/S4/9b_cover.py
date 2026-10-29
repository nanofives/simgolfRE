"""Instruction and branch coverage of the ORIGINAL loadSoundFile (rva 0x41d0..0x5082, `ret 4` at 0x507f) over the S4.9b keys (layout A,
the emulator; the pattern of 9_cover.py). The port is not run; the table says which instructions and which conditional-jump sides
of the original the vectors that judge the port reach.

    ~/bin/py -3.12 log/roadmap/S4/9b_cover.py [--keys k ...] [--out log/roadmap/S4/9b_coverage.tsv]
"""
from __future__ import annotations

import argparse
import csv
import importlib.util
import pathlib
import sys

import capstone
import pefile

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
spec = importlib.util.spec_from_file_location("s49b_replay", HERE / "9b_replay.py")
RUN = importlib.util.module_from_spec(spec)
spec.loader.exec_module(RUN)
SR = RUN.R
E = SR.E
from unicorn import UC_HOOK_CODE  # noqa: E402

LO, HI = 0x41d0, 0x5082
BASE = E.LAYOUTS["A"]["base"]
HITS: set = set()
EDGES: set = set()
_prev = [None]


def _code(uc, addr, size, user):
    HITS.add(addr)
    p = _prev[0]
    if p is not None:
        EDGES.add((p, addr))
    _prev[0] = addr


_orig_init = E.Machine.__init__


def _init(self, *a, **k):
    _orig_init(self, *a, **k)
    self.uc.hook_add(UC_HOOK_CODE, _code, None, BASE + LO, BASE + HI - 1)
    self.uc.hook_add(UC_HOOK_CODE, _leave, None, BASE + 0x1000, BASE + 0x5A000)


def _leave(uc, addr, size, user):
    # an instruction outside the function breaks the edge chain (a call's callee is not an edge of the caller)
    if not (BASE + LO <= addr < BASE + HI):
        _prev[0] = None


E.Machine.__init__ = _init


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys", nargs="*")
    ap.add_argument("--out", default=str(HERE / "9b_coverage.tsv"))
    a = ap.parse_args(argv)
    keys = a.keys or RUN.family_keys("soundfile")
    pe = pefile.PE(str(ROOT / "original" / "sound.dll"))
    img = pe.get_memory_mapped_image()
    ib = pe.OPTIONAL_HEADER.ImageBase
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    insns, jccs = [], []
    for ins in md.disasm(img[LO:0x5080], ib + LO):
        ad = ins.address - ib
        insns.append(ad)
        if ins.mnemonic.startswith("j") and ins.mnemonic != "jmp" and ins.operands and ins.operands[0].type == 2:
            jccs.append((ad, ins.operands[0].imm - ib, ad + ins.size))
        if ad >= 0x507f:
            break
    hits, edges = set(), set()
    per_key = {}
    with E.FixVM() as vm:
        for n, key in enumerate(keys, 1):
            HITS.clear()
            EDGES.clear()
            _prev[0] = None
            try:
                res = RUN.KeyRun8(vm, key, "A", arms=1).run()
            except Exception as e:  # noqa: BLE001
                print(f"[{n}/{len(keys)}] {key} ERROR {e}", flush=True)
                continue
            h = {x - BASE for x in HITS}
            per_key[key] = h
            hits |= h
            edges |= {(p - BASE, c - BASE) for p, c in EDGES}
            print(f"[{n}/{len(keys)}] {key} {len([x for x in insns if x in h])} instructions, errors "
                  f"{sum(1 for r in res.rows if r.error)}", flush=True)
    rows = []
    both = one = never = 0
    for ad, t, f in jccs:
        ta, fa = (ad, t) in edges, (ad, f) in edges
        side = "both" if ta and fa else ("taken" if ta else ("fallthrough" if fa else "never"))
        both += side == "both"
        one += side in ("taken", "fallthrough")
        never += side == "never"
        rows.append((f"0x{ad:x}", f"0x{t:x}", f"0x{f:x}", side))
    miss = [x for x in insns if x not in hits]
    spans, cur = [], None
    for x in miss:
        if cur and x - cur[1] < 16:
            cur[1] = x
        else:
            cur = [x, x]
            spans.append(cur)
    with open(a.out, "w", newline="") as fh:
        w = csv.writer(fh, delimiter="\t")
        w.writerow(["jcc", "target", "fallthrough", "seen"])
        for r in rows:
            w.writerow(r)
        w.writerow([])
        w.writerow(["unexecuted_span_lo", "unexecuted_span_hi"])
        for lo, hi in spans:
            w.writerow([f"0x{lo:x}", f"0x{hi:x}"])
    print(f"instructions {len(insns) - len(miss)}/{len(insns)} ({100 * (len(insns) - len(miss)) / len(insns):.1f}%)  "
          f"jcc {len(jccs)}: both sides {both}, one side {one}, never {never}")
    print("one-sided / never: " + " ".join(f"{r[0]}({r[3]})" for r in rows if r[3] != "both"))
    print("unexecuted spans: " + " ".join(f"0x{lo:x}-0x{hi:x}" for lo, hi in spans))
    return 0


if __name__ == "__main__":
    sys.exit(main())
