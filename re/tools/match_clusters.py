"""Neighbourhood clusters for matching: runs of address-adjacent functions that share a referenced global/string
or call each other. VC6 links each object file's functions contiguously, so a cluster is a candidate source file;
it is NOT a proven object boundary (no symbols or map file exist; only a few debug asserts name files).
Use it to put related functions into one re/match file and to pick the struct/global declarations they share.

    py -3.12 re/tools/match_clusters.py --module golf_clean.exe --summary
    py -3.12 re/tools/match_clusters.py --module jgld.dll --at 0x10017f30     # the cluster around one function
    py -3.12 re/tools/match_clusters.py --module Terrain.dll --list > log/clusters_terrain.tsv"""
import argparse, collections, pathlib, sys

import capstone
import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
from match import TSV, image_base, tracker_key  # noqa: E402
from match_queue import annotated, not_game, rows  # noqa: E402


def function_facts(module):
    """[(va, size, name, data_refs, call_targets)] in address order, game and library functions alike."""
    pe = pefile.PE(str(ROOT / "original" / module), fast_load=True)
    base = image_base(module)
    data = [(base + s.VirtualAddress, base + s.VirtualAddress + s.Misc_VirtualSize) for s in pe.sections
            if not s.Characteristics & 0x20000000]
    is_data = lambda v: any(lo <= v < hi for lo, hi in data)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    out = []
    for r in rows(ROOT / "re" / TSV[module]):
        key, size = int(r["entry"], 16), int(r["size"])
        va = key if module == "golf_clean.exe" else key + base
        refs, calls = set(), set()
        for ins in md.disasm(pe.get_data(va - base, size), va):
            if ins.mnemonic == "call" and ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                calls.add(ins.operands[0].imm)
            for op in ins.operands:
                v = op.imm if op.type == capstone.x86.X86_OP_IMM else (
                    op.mem.disp if op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 else None)
                if v is not None and is_data(v & 0xffffffff):
                    refs.add(v & 0xffffffff)
        out.append((va, size, r["name"], refs, calls))
    return sorted(out)


def clusters(module, window=4, common=40):
    """Link a function to the current run when it shares a data reference or a callee with any of the last
    `window` functions, or calls / is called by one of them; data or callees used by more than `common` functions
    (g_text, g_flags, strcat, ...) do not link."""
    # data references and call targets both count as shared references
    facts = [(va, size, name, refs | {("call", c) for c in calls}, calls)
             for va, size, name, refs, calls in function_facts(module)]
    uses = collections.Counter(r for f in facts for r in f[3])
    rare = lambda refs: {r for r in refs if uses[r] <= common}
    groups, cur = [], []
    for f in facts:
        if cur:
            tail = cur[-window:]
            refs = set().union(*(rare(g[3]) for g in tail))
            vas = {g[0] for g in tail}
            linked = (rare(f[3]) & refs) or (f[4] & vas) or any(f[0] in g[4] for g in tail)
            if not linked:
                groups.append(cur)
                cur = []
        cur.append(f)
    if cur:
        groups.append(cur)
    return groups


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", default="golf_clean.exe", choices=sorted(TSV))
    ap.add_argument("--summary", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--at", default="")
    a = ap.parse_args()
    done = annotated(a.module)
    base = image_base(a.module)
    pe = pefile.PE(str(ROOT / "original" / a.module), fast_load=True)
    gs = clusters(a.module)

    def game(f):
        return not not_game(f[2], f[1], pe.get_data(f[0] - base, 2))

    if a.at:
        t = int(a.at, 16)
        for g in gs:
            if any(f[0] == t for f in g):
                shared = collections.Counter(r for f in g for r in f[3] if not isinstance(r, tuple))
                print(f"# cluster 0x{g[0][0]:08x}..0x{g[-1][0] + g[-1][1]:08x}: {len(g)} functions")
                for f in g:
                    mark = "M" if tracker_key(a.module, f[0]) in done else ("." if game(f) else "lib")
                    print(f"0x{f[0]:08x}\t{f[1]}\t{mark}\t{f[2]}")
                print("# data shared by 2+ functions:", " ".join(f"0x{r:08x}x{n}" for r, n in shared.most_common(12)
                                                            if n > 1))
                return
        raise SystemExit("address is not a function entry")
    if a.list:
        print("start\tend\tfunctions\tgame\tmatched")
        for g in gs:
            gm = [f for f in g if game(f)]
            print(f"0x{g[0][0]:08x}\t0x{g[-1][0] + g[-1][1]:08x}\t{len(g)}\t{len(gm)}\t"
                  f"{sum(tracker_key(a.module, f[0]) in done for f in gm)}")
        return
    sizes = collections.Counter(min(len(g), 20) for g in gs)
    print(f"{a.module}: {len(gs)} clusters over {sum(len(g) for g in gs)} functions; "
          f"singletons {sizes[1]}, 2-5: {sum(sizes[k] for k in range(2, 6))}, 6-19: "
          f"{sum(sizes[k] for k in range(6, 20))}, 20+: {sizes[20]}")


if __name__ == "__main__":
    main()
