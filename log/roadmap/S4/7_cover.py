"""Instruction and branch coverage of the ORIGINAL sound.dll functions of step S4.7 over their keys (layout A, the emulator).

    ~/bin/py -3.12 log/roadmap/S4/7_cover.py [--keys k ...] [--out log/roadmap/S4/7_coverage.tsv]

For each function (hooks.csv name) the union over its own keys of: instructions executed (of the function's ranges in
re/functions_ghidra_sound.dll.tsv, decoded with capstone from the user's sound.dll), and for each conditional jump whether
both outgoing edges (taken, fall through) were seen. The port is not run here; the table says how much of the original the
vectors that judge the port reach, and which ranges they never reach (a body error in unreached code is invisible to them).
"""
from __future__ import annotations

import argparse
import collections
import csv
import importlib.util
import pathlib
import struct
import sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location("s47_run", ROOT / "log" / "roadmap" / "S4" / "7_run.py")
RUN = importlib.util.module_from_spec(spec)
spec.loader.exec_module(RUN)
SR = RUN.SR
E = SR.E
from unicorn import UC_HOOK_CODE  # noqa: E402

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
    self.uc.hook_add(UC_HOOK_CODE, _code, None, BASE + 0x1000, BASE + 0x5A000)


E.Machine.__init__ = _init


def function_ranges():
    out = {}
    with open(ROOT / "re" / "functions_ghidra_sound.dll.tsv", newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            rs = []
            for part in r["ranges"].split(";"):
                lo, hi = part.split("-")
                rs.append((int(lo, 16), int(hi, 16)))
            out[int(r["entry"], 16)] = (r["name"], rs)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--keys", nargs="*")
    ap.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "S4" / "7_coverage.tsv"))
    a = ap.parse_args(argv)
    keys = RUN.keys_for_step(a.keys)
    fr = function_ranges()
    pe = pefile.PE(str(ROOT / "original" / "sound.dll"))
    img = pe.get_memory_mapped_image()
    ibase = pe.OPTIONAL_HEADER.ImageBase
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    per_fn_hits = collections.defaultdict(set)
    per_fn_edges = collections.defaultdict(set)
    with E.FixVM() as vm:
        hooks, _ = E.registry()
        for n, key in enumerate(keys, 1):
            if key in SR.DERIVED:
                hooks[key] = SR.spec_of(key)
            HITS.clear()
            EDGES.clear()
            _prev[0] = None
            try:
                E.KeyRun(vm, key, "A").run()
            except Exception as e:  # noqa: BLE001
                print(f"[{n}/{len(keys)}] {key} ERROR {e}", flush=True)
                continue
            fn = SR.port_name(key)
            per_fn_hits[fn] |= {h - BASE for h in HITS}
            per_fn_edges[fn] |= {(p - BASE, c - BASE) for p, c in EDGES}
            print(f"[{n}/{len(keys)}] {key} {len(HITS)} instructions", flush=True)
    rows = []
    rows_by_name = {r["name"]: int(r["rva"], 16) for r in SR.step_rows_all() if r["step"] == "S4.7"}
    for name, rva in sorted(rows_by_name.items(), key=lambda kv: kv[1]):
        if name not in per_fn_hits:
            rows.append((name, rva, 0, 0, 0, 0, 0, 0, "no key	"))
            continue
        ranges = fr[rva][1] if rva in fr else [(rva, rva + 1)]
        insns, jccs = [], []
        for lo, hi in ranges:
            for ins in md.disasm(img[lo:hi], ibase + lo):
                ad = ins.address - ibase
                if ad >= hi:
                    break
                insns.append(ad)
                if ins.mnemonic.startswith("j") and ins.mnemonic != "jmp" and ins.operands and ins.operands[0].type == 2:
                    jccs.append((ad, ins.operands[0].imm - ibase, ad + ins.size))
        hit = per_fn_hits[name]
        edges = per_fn_edges[name]
        cov = [x for x in insns if x in hit]
        both = one = none = 0
        sides = []
        for ad, t, f in jccs:
            ta, fa = (ad, t) in edges, (ad, f) in edges
            if ta and fa:
                both += 1
            elif ta or fa:
                one += 1
                sides.append(f"0x{ad:x}({'T' if ta else 'F'})")
            else:
                none += 1
                sides.append(f"0x{ad:x}(-)")
        miss = [x for x in insns if x not in hit]
        spans, cur = [], None
        for x in miss:
            if cur and x - cur[1] < 16:
                cur[1] = x
            else:
                cur = [x, x]
                spans.append(cur)
        rows.append((name, rva, len(insns), len(cov), len(jccs), both, one, none,
                     " ".join(f"0x{lo:x}-0x{hi:x}" for lo, hi in spans)[:400] + "	" + " ".join(sides)[:400]))
    with open(a.out, "w", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(["function", "rva", "instructions", "executed", "jcc", "jcc_both_sides", "jcc_one_side", "jcc_never", "unexecuted_spans", "jcc_one_sided_or_never"])
        for r in rows:
            w.writerow([r[0], f"0x{r[1]:x}", *r[2:-1], *r[-1].split("	")])
    full = sum(1 for r in rows if r[2] and r[2] == r[3])
    print(f"functions {len(rows)}  fully executed {full}  of which jcc both-sided {sum(1 for r in rows if r[2] and r[2] == r[3] and r[6] == 0 and r[7] == 0)}")
    tot_i = sum(r[2] for r in rows)
    tot_e = sum(r[3] for r in rows)
    tot_j = sum(r[4] for r in rows)
    tot_b = sum(r[5] for r in rows)
    print(f"instructions {tot_e}/{tot_i} ({100 * tot_e / max(tot_i, 1):.1f}%)  conditional jumps both sides {tot_b}/{tot_j}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
