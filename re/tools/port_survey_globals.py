"""Port survey 1: data addresses referenced by golf_clean.exe game-logic code (0x401000-0x4a4ecf).

Disassembles every range of every function in that span (xref.functions) and collects each memory displacement and
immediate that lands in .data/.bss (0x4c1000-0x842000) or .rdata (0x4ba000-0x4c0098). Reports, per address, the number
of referencing functions and whether re/names/*_globals.tsv names it, then groups addresses into clusters (gap <= 0x40)
because an array element is base + offset and one record is many addresses.

    py -3.12 re/tools/port_survey_globals.py [--out log/port_globals.tsv]
"""
import argparse, collections, glob, pathlib, sys

import capstone

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import xref  # noqa: E402

LO, HI = 0x401000, 0x4A4ED0
DATA = (0x4BA000, 0x842000)
GAP = 0x40


def named_globals():
    out = {}
    for f in glob.glob(str(ROOT / "re" / "names" / "*_globals.tsv")):
        for line in pathlib.Path(f).read_text(encoding="utf-8", errors="replace").splitlines()[1:]:
            c = line.split("\t")
            if len(c) > 2 and c[0] == "golf_clean.exe":
                try:
                    out[int(c[1], 16)] = c[2]
                except ValueError:
                    pass
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(ROOT / "log" / "port_globals.tsv"))
    a = ap.parse_args()
    pe, base = xref.load_pe("golf_clean.exe")
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    refs = collections.defaultdict(set)
    nfun = 0
    for va, name, size, rngs in xref.functions("golf_clean.exe"):
        if not (LO <= va < HI):
            continue
        nfun += 1
        for lo, hi in rngs:
            try:
                code = pe.get_data(lo - base, hi - lo)
            except Exception:
                continue
            for ins in md.disasm(code, lo):
                if ins.mnemonic in ("call", "jmp") or ins.mnemonic.startswith("j"):
                    continue
                for op in ins.operands:
                    v = None
                    if op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0 or (
                            op.type == capstone.x86.X86_OP_MEM and op.mem.base == 0):
                        v = op.mem.disp & 0xFFFFFFFF
                    elif op.type == capstone.x86.X86_OP_IMM:
                        v = op.imm & 0xFFFFFFFF
                    if v is not None and DATA[0] <= v < DATA[1]:
                        refs[v].add(va)
    names = named_globals()
    addrs = sorted(refs)
    clusters, cur = [], []
    for ad in addrs:
        if cur and ad - cur[-1] > GAP:
            clusters.append(cur)
            cur = []
        cur.append(ad)
    if cur:
        clusters.append(cur)
    pathlib.Path(a.out).parent.mkdir(exist_ok=True)
    with open(a.out, "w") as f:
        f.write("addr\tfunctions\tnamed\n")
        for ad in addrs:
            f.write(f"{ad:08x}\t{len(refs[ad])}\t{names.get(ad, '')}\n")
    named_hit = sum(1 for ad in addrs if ad in names)
    # a cluster counts as named when any member address is named (array / record base named)
    cl_named = sum(1 for c in clusters if any(x in names for x in c))
    print(f"game-logic functions scanned: {nfun}")
    print(f"distinct data addresses referenced: {len(addrs)}")
    print(f"  named exactly: {named_hit}  (named globals in the TSVs: {len(names)})")
    print(f"clusters (gap <= {GAP:#x}): {len(clusters)}, with a named member: {cl_named}")
    top = sorted(clusters, key=lambda c: -sum(len(refs[x]) for x in c))[:25]
    print("top 25 clusters by referencing-function count (lo-hi, addresses, functions, named):")
    for c in top:
        fs = set().union(*(refs[x] for x in c))
        nm = [names[x] for x in c if x in names]
        print(f"  {c[0]:08x}-{c[-1]:08x} addrs={len(c)} fns={len(fs)} named={','.join(nm[:3])}")
    unn = [c for c in clusters if not any(x in names for x in c)]
    unn_fns = set().union(*(refs[x] for c in unn for x in c)) if unn else set()
    print(f"unnamed clusters: {len(unn)}, touched by {len(unn_fns)} functions")


if __name__ == "__main__":
    main()
