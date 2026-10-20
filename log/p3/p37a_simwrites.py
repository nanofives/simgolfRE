"""Phase 3.7a helper: static memory-write census of golf_clean.exe functions (capstone over re/functions_ghidra.tsv).

For every function: the data objects (re/model/addresses.tsv top rows, GameState class from gamestate_meta.inc) its
instructions WRITE through an operand whose displacement lies in .data, the writes through a register with a small
displacement (`opaque`: target not known statically; esp / ebp based ones are frame locals and not counted), and the
transitive closure over direct callees. Used to decide which mainLoop callees can be dropped from the headless frame step
(no sim-class write, no RNG draw) and which must be ported. Conservative: a sim write found anywhere in the closure keeps
the callee. usage: py -3.12 log/p3/p37a_simwrites.py [0x<addr> ...] [--tsv out.tsv]
"""
import csv, json, pathlib, re, sys
import capstone
from capstone import x86

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import xref  # noqa: E402

DATA_LO, DATA_HI = 0x004C1000, 0x004C1000 + 0x381174
WRITE_MN = re.compile(r"^(mov|movs[bwd]?|stos[bwd]?|add|sub|inc|dec|or|and|xor|not|neg|shl|shr|sar|rol|ror|rcl|rcr|adc|sbb|xchg|set\w+|fst\w*|fist\w*|cmpxchg|bts|btr|btc|pop|movzx|movsx)$")


def objects():
    cls = {}
    for line in (ROOT / "port" / "include" / "gamestate_meta.inc").read_text().splitlines():
        m = re.match(r"SG_OBJECT\((\w+),\s*(0x[0-9a-f]+),\s*(0x[0-9a-f]+),\s*(\w+)", line)
        if m:
            cls[m.group(1)] = m.group(4)
    rows = []
    with open(ROOT / "re" / "model" / "addresses.tsv", encoding="utf8") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            if r["relation"] != "top" or r["module"] != "golf_clean.exe":
                continue
            try:
                rows.append((int(r["va"], 16), int(r["size"], 16), r["symbol"]))
            except ValueError:
                pass
    rows.sort()
    return rows, cls


ROWS, CLS = objects()


_STARTS = None


def obj_of(v):
    global _STARTS
    import bisect
    if _STARTS is None:
        _STARTS = [r[0] for r in ROWS]
    i = bisect.bisect_right(_STARTS, v) - 1
    while i >= 0:
        b, s, n = ROWS[i]
        if b <= v < b + s:
            return n
        if v - b > 0x100000:
            break
        i -= 1
    return None


def census():
    pe, base = xref.load_pe("golf_clean.exe")
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    out = {}
    entries = {f[0] for f in xref.functions("golf_clean.exe")}
    for va, name, size, rngs in xref.functions("golf_clean.exe"):
        writes, opaque, calls, taken = {}, [], set(), {}
        for lo, hi in rngs:
            try:
                ins_list = list(md.disasm(pe.get_data(lo - base, hi - lo), lo))
            except Exception:
                continue
            for ins in ins_list:
                if ins.mnemonic == "call" and ins.operands and ins.operands[0].type == x86.X86_OP_IMM:
                    t = ins.operands[0].imm & 0xFFFFFFFF
                    if t in entries:
                        calls.add(t)
                    continue
                if ins.mnemonic.startswith("rep") or ins.mnemonic in ("stosd", "stosb", "stosw", "movsd", "movsb", "movsw"):
                    opaque.append((ins.address, ins.mnemonic + " " + ins.op_str))
                    continue
                for o in ins.operands:
                    if o.type == x86.X86_OP_IMM and DATA_LO <= (o.imm & 0xFFFFFFFF) < DATA_HI:
                        n = obj_of(o.imm & 0xFFFFFFFF)
                        if n:
                            taken.setdefault(n, []).append(ins.address)
                    if ins.mnemonic == "lea" and o.type == x86.X86_OP_MEM and DATA_LO <= (o.mem.disp & 0xFFFFFFFF) < DATA_HI:
                        n = obj_of(o.mem.disp & 0xFFFFFFFF)
                        if n:
                            taken.setdefault(n, []).append(ins.address)
                if not ins.operands or not WRITE_MN.match(ins.mnemonic):
                    continue
                if ins.mnemonic in ("movzx", "movsx", "pop"):
                    continue
                op = ins.operands[0]
                if op.type != x86.X86_OP_MEM:
                    if ins.mnemonic == "xchg" and len(ins.operands) > 1 and ins.operands[1].type == x86.X86_OP_MEM:
                        op = ins.operands[1]
                    else:
                        continue
                disp = op.mem.disp & 0xFFFFFFFF
                if DATA_LO <= disp < DATA_HI:
                    n = obj_of(disp) or ("?%08x" % disp)
                    writes.setdefault(n, []).append(ins.address)
                else:
                    breg = ins.reg_name(op.mem.base) if op.mem.base else ""
                    if breg in ("esp", "ebp") and not op.mem.index:
                        continue
                    if op.mem.segment and ins.reg_name(op.mem.segment) == "fs":
                        continue
                    opaque.append((ins.address, ins.mnemonic + " " + ins.op_str))
        out[va] = {"name": name, "writes": writes, "opaque": opaque, "calls": sorted(calls), "taken": taken}
    return out


def closure(idx, va, seen=None):
    seen = set() if seen is None else seen
    if va in seen or va not in idx:
        return seen
    seen.add(va)
    for c in idx[va]["calls"]:
        closure(idx, c, seen)
    return seen


def summary(idx, va):
    f = idx[va]
    sub = closure(idx, va)
    sim_direct = sorted(n for n in f["writes"] if CLS.get(n) == "sim")
    sim_trans = {}
    for c in sub:
        for n in idx[c]["writes"]:
            if CLS.get(n) == "sim":
                sim_trans.setdefault(n, set()).add(idx[c]["name"])
    opq = sum(len(idx[c]["opaque"]) for c in sub)
    taken = {}
    for c in sub:
        for n in idx[c]["taken"]:
            if CLS.get(n) == "sim":
                taken.setdefault(n, set()).add(idx[c]["name"])
    return sim_direct, sim_trans, opq, len(f["opaque"]), sub, taken


def main():
    idx = census()
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    tsv = None
    if "--tsv" in sys.argv:
        tsv = sys.argv[sys.argv.index("--tsv") + 1]
        args = [a for a in args if a != tsv]
    rows = []
    for a in args:
        va = int(a, 16)
        if va not in idx:
            print(a, "not a function")
            continue
        d, t, opq, own, sub, tk = summary(idx, va)
        rows.append((a, idx[va]["name"], ",".join(d), ";".join("%s<%s>" % (k, ",".join(sorted(v))[:60]) for k, v in sorted(t.items())), own, opq, len(sub), ";".join("%s<%s>" % (k, ",".join(sorted(v))[:60]) for k, v in sorted(tk.items()))))
    for r in rows:
        print("\t".join(map(str, r)))
    if tsv:
        with open(tsv, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["addr", "name", "sim_direct", "sim_transitive", "opaque_own", "opaque_closure", "closure_size", "sim_address_taken"])
            w.writerows(rows)


if __name__ == "__main__":
    main()
