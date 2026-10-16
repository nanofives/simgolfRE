"""Every instruction that addresses a record table, grouped by field offset.

    py -3.12 re/tools/record_fields.py --base 0x5794b8 --stride 0x100 --count 0x9c            # golfer records
    py -3.12 re/tools/record_fields.py --base 0x585850 --stride 0x4c --count 0x40 --tsv log/walkers.tsv
    py -3.12 re/tools/record_fields.py ... --pre 0x10 --post 0x10      # also list displacements just outside

For each function of the module (re/functions_ghidra*.tsv, every address range) the instructions are decoded with
capstone and a reference is recorded when:
  * a memory operand's displacement lies in [base - pre, base + stride*count + post)   (kind `disp`, `disp+idx`
    when the operand has a base or index register, `abs` when it has neither), or
  * an immediate operand lies in that range (kind `imm`: a pointer into the table, e.g. `mov esi, 0x5794c0`), or
  * kind `reg`: a memory operand [r + k] whose register r was last loaded, earlier in the same straight-line
    decode order, with `mov r, imm` / `lea r, [disp]` of a table address and only changed since by `add/sub r, imm`.
    Control flow is not followed, so `reg` rows are leads to check in the disassembly, not evidence on their own.
Code outside every function range (mainLoop's far blocks 0x00411f51..0x0041f8fe sit outside its recorded ranges) is
swept linearly, one unit per gap, re-synchronising one byte past anything that does not decode; such rows carry the
function name `(no function)` and the gap start as their function address (--no-gaps leaves them out).

field = (address - base) mod stride, record = (address - base) // stride (only meaningful for `abs`).
Width comes from the operand size; signedness from the mnemonic (movsx = s, movzx = u) or, for a cmp/test of the
operand, from the next conditional jump (jl/jg/jle/jge = s, jb/ja/jbe/jae = u). Access is r, w or rw (capstone).

Output: a summary per field on stdout (widths, signedness, access, functions), and with --tsv every reference."""
import argparse, collections, pathlib, sys

import capstone
from capstone import x86

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import xref  # noqa: E402

SIGNED_J = {"jl", "jg", "jle", "jge", "js", "jns"}
UNSIGNED_J = {"jb", "ja", "jbe", "jae"}


def num(s):
    return int(s, 0)


def decode(md, data, start, resync):
    """Instructions of data at start; with resync, skip one byte past anything capstone cannot decode."""
    out, off = [], 0
    while off < len(data):
        n = 0
        for ins in md.disasm(data[off:], start + off):
            out.append(ins)
            n += ins.size
        if not resync:
            break
        off += n + 1
    return out


def scan(mod, base, stride, count, pre=0, post=0, gaps=True):
    pe, ib = xref.load_pe(mod)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = True
    lo, hi = base - pre, base + stride * count + post
    nm = xref.names(mod)
    refs = []
    units = []                                  # (entry, label, [ranges])
    covered = []
    for va, fname, size, rngs in xref.functions(mod):
        n = nm.get(va)
        label = n[1] if n and n[0] == "names" else (fname if not (fname.startswith("FUN_") and n) else n[1])
        covered += rngs
        if size > 5:
            units.append((va, label, rngs))
    if gaps:
        # code outside every function range (e.g. mainLoop's far blocks): linear sweep of the uncovered parts of
        # each executable section, one unit per gap, labelled `(no function)` with the gap start as its entry
        covered.sort()
        for s in pe.sections:
            if not s.Characteristics & 0x20000000:
                continue
            lo_s, hi_s = ib + s.VirtualAddress, ib + s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)
            cur = lo_s
            for clo, chi in covered + [(hi_s, hi_s)]:
                if chi <= cur or clo < lo_s:
                    continue
                if clo > cur:
                    units.append((cur, "(no function)", [(cur, min(clo, hi_s))]))
                cur = max(cur, chi)
                if cur >= hi_s:
                    break
    for va, label, rngs in units:
        for rlo, rhi in rngs:
            try:
                insns = decode(md, pe.get_data(rlo - ib, rhi - rlo), rlo, label == "(no function)")
            except Exception:
                continue
            anchor = {}                       # reg id -> table address it holds
            for i, ins in enumerate(insns):
                nxt = insns[i + 1] if i + 1 < len(insns) else None

                def sign(op_size):
                    if ins.mnemonic.startswith("movsx"):
                        return "s"
                    if ins.mnemonic.startswith("movzx"):
                        return "u"
                    if ins.mnemonic in ("cmp", "test") and nxt is not None:
                        if nxt.mnemonic in SIGNED_J:
                            return "s"
                        if nxt.mnemonic in UNSIGNED_J:
                            return "u"
                    return ""

                for oi, op in enumerate(ins.operands):
                    acc = ("r" if op.access & capstone.CS_AC_READ else "") + ("w" if op.access & capstone.CS_AC_WRITE else "")
                    if op.type == x86.X86_OP_MEM:
                        d = op.mem.disp & 0xFFFFFFFF
                        if lo <= d < hi:
                            kind = "abs" if not op.mem.base and not op.mem.index else "disp+idx"
                            if ins.mnemonic == "lea":
                                kind = "lea"
                                acc = ""
                            refs.append(dict(fn=va, name=label, ins=ins.address, kind=kind, addr=d, size=op.size,
                                             sign=sign(op.size), acc=acc, text=f"{ins.mnemonic} {ins.op_str}"))
                        elif op.mem.base and not op.mem.index and op.mem.base in anchor and ins.mnemonic != "lea":
                            a = (anchor[op.mem.base] + op.mem.disp) & 0xFFFFFFFF
                            if lo <= a < hi:
                                refs.append(dict(fn=va, name=label, ins=ins.address, kind="reg", addr=a, size=op.size,
                                                 sign=sign(op.size), acc=acc, text=f"{ins.mnemonic} {ins.op_str}"))
                    elif op.type == x86.X86_OP_IMM:
                        v = op.imm & 0xFFFFFFFF
                        if lo <= v < hi and ins.mnemonic not in ("call", "jmp") and not ins.mnemonic.startswith("j"):
                            refs.append(dict(fn=va, name=label, ins=ins.address, kind="imm", addr=v, size=0, sign="",
                                             acc="", text=f"{ins.mnemonic} {ins.op_str}"))
                # register anchor tracking (straight-line decode order only)
                ops = ins.operands
                dst = ops[0].reg if ops and ops[0].type == x86.X86_OP_REG else None
                if dst is not None:
                    if ins.mnemonic == "mov" and len(ops) == 2 and ops[1].type == x86.X86_OP_IMM and lo <= (ops[1].imm & 0xFFFFFFFF) < hi:
                        anchor[dst] = ops[1].imm & 0xFFFFFFFF
                        continue
                    if (ins.mnemonic == "lea" and len(ops) == 2 and ops[1].type == x86.X86_OP_MEM
                            and not ops[1].mem.base and not ops[1].mem.index and lo <= (ops[1].mem.disp & 0xFFFFFFFF) < hi):
                        anchor[dst] = ops[1].mem.disp & 0xFFFFFFFF
                        continue
                    if ins.mnemonic in ("add", "sub") and len(ops) == 2 and ops[1].type == x86.X86_OP_IMM and dst in anchor:
                        k = ops[1].imm if ins.mnemonic == "add" else -ops[1].imm
                        anchor[dst] = (anchor[dst] + k) & 0xFFFFFFFF
                        continue
                # any other write to a register drops its anchor
                try:
                    _, written = ins.regs_access()
                except Exception:
                    written = []
                for r in written:
                    anchor.pop(r, None)
                if ins.mnemonic in ("ret", "jmp", "call"):
                    if ins.mnemonic != "call":
                        anchor.clear()
                    else:
                        for r in (x86.X86_REG_EAX, x86.X86_REG_ECX, x86.X86_REG_EDX):
                            anchor.pop(r, None)
    for r in refs:
        r["field"] = (r["addr"] - base) % stride
        r["record"] = (r["addr"] - base) // stride
    return refs


def summarize(refs, stride, out=sys.stdout, with_reg=True):
    by = collections.defaultdict(list)
    for r in refs:
        if r["kind"] == "reg" and not with_reg:
            continue
        by[r["field"]].append(r)
    for f in sorted(by):
        rs = by[f]
        widths = collections.Counter(r["size"] for r in rs if r["size"])
        signs = collections.Counter(r["sign"] for r in rs if r["sign"])
        accs = collections.Counter(c for r in rs for c in r["acc"])
        kinds = collections.Counter(r["kind"] for r in rs)
        fns = sorted({(r["fn"], r["name"]) for r in rs})
        print(f"+0x{f:02x}\trefs={len(rs)}\twidth={dict(widths)}\tsign={dict(signs)}\tacc={dict(accs)}\tkinds={dict(kinds)}"
              f"\tfns={len(fns)}\t" + ", ".join(f"{n}@{a:#010x}" for a, n in fns[:12]) + (" ..." if len(fns) > 12 else ""),
              file=out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--module", default="golf_clean.exe")
    ap.add_argument("--base", type=num, required=True)
    ap.add_argument("--stride", type=num, required=True)
    ap.add_argument("--count", type=num, required=True)
    ap.add_argument("--pre", type=num, default=0, help="also scan this many bytes below base")
    ap.add_argument("--post", type=num, default=0, help="also scan this many bytes past the last record")
    ap.add_argument("--no-reg", action="store_true", help="leave register-tracked rows out of the summary")
    ap.add_argument("--no-gaps", action="store_true", help="skip code outside every function range")
    ap.add_argument("--tsv", help="write every reference here")
    a = ap.parse_args()
    refs = scan(a.module, a.base, a.stride, a.count, a.pre, a.post, gaps=not a.no_gaps)
    summarize(refs, a.stride, with_reg=not a.no_reg)
    if a.tsv:
        p = pathlib.Path(a.tsv)
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open("w") as fh:
            fh.write("field\trecord\taddr\tkind\twidth\tsign\taccess\tins\tfunction\tname\ttext\n")
            for r in sorted(refs, key=lambda r: (r["field"], r["ins"])):
                fh.write(f"0x{r['field']:02x}\t{r['record']}\t0x{r['addr']:08x}\t{r['kind']}\t{r['size']}\t{r['sign']}\t"
                         f"{r['acc']}\t0x{r['ins']:08x}\t0x{r['fn']:08x}\t{r['name']}\t{r['text']}\n")
    print(f"# {len(refs)} references, {len({r['fn'] for r in refs})} functions", file=sys.stderr)


if __name__ == "__main__":
    main()
