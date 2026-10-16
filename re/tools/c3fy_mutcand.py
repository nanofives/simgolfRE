r"""c3fyc mutation campaign: propose single-edit wrong-body mutants of shim/src/re/c3fy.cpp, one table row each, and pick
a spread over mainLoop's phases (re/tools/c3fy_mutants.tsv, applied by re/tools/c3fy_mutgen.py).

Kinds (each a one-line edit through the macros c3fy_mutgen.py defines; all dormant unless SIMGOLF_C3FY_NEG = the id):
  flip     `if (C)` -> `if (MV(id, (C), !(C)))`                       a compare / test turned around
  bound    `a < b` -> `a <= b` (and <= <, > >=, >= >) inside one `if`  an off-by-one compare
  const    one literal argument of a call (or a stack-model push) + 1
  dropcall a call statement skipped                                    (MS)
  skipstore a plain store `X = v;` skipped                             (MSKIP)
  width    one signed load G8 / G16 / FB / FW read zero-extended (or the reverse)
  swap     two adjacent call statements in the opposite order           (MI)
Never mutated: loop heads (`for`, `while`, `do`), a `goto` to a label at a lower address (a loop back edge), switch /
case lines, the naked entry, the negctl selector, declarations without a citation. A mutant's phase is the phase of the
block its first cited address lies in (log/c3/c3fy2_coverage.tsv), and it is `covered` when some scenario runs that
block.

    py -3.12 re/tools/c3fy_mutcand.py --list            # every candidate
    py -3.12 re/tools/c3fy_mutcand.py --pick 13 --seed 7 --out re/tools/c3fy_mutants.tsv
"""
from __future__ import annotations

import argparse
import bisect
import collections
import random
import re
import sys

import c3fy_audit_common as A

COV = A.ROOT / "log" / "c3" / "c3fy2_coverage.tsv"
REL = re.compile(r"(<=|>=|==|!=|(?<![<>])<(?![<=])|(?<![<>-])>(?![>=]))")


def load_cov():
    rows = []
    for ln in COV.read_text().splitlines()[1:]:
        b, ph, _n, sc = (ln.split("\t") + [""])[:4]
        rows.append((int(b, 16), ph, [x for x in sc.split(",") if x and x != "-"]))
    rows.sort()
    return rows


def block_of(rows, va):
    k = bisect.bisect_right([r[0] for r in rows], va) - 1
    return rows[k] if k >= 0 else None


def label_addr(text):
    m = re.search(r"\bgoto\s+(?:La_|L_?|a)([0-9a-fA-F]{6})\b", text)
    return int(m.group(1), 16) if m else None


def top_level_if(code):
    """(start, end) of the condition of a single `if (...)` on the line, else None."""
    m = re.search(r"\bif\s*\(", code)
    if not m or code.count("if (") + code.count("if(") != 1:
        return None
    args, end = A.split_args(code, m.end() - 1)
    if len(args) != 1:
        return None
    return m.end(), end - 1


def candidates(src, rows):
    out = []
    prev_call = None
    for k, ln in enumerate(src):
        code = ln.code
        s = code.strip()
        if not s or not ln.own:
            prev_call = None
            continue
        if re.match(r"(for|while|do\b|switch|case|default|#|template|static|__asm|\{|\})", s) or "__asm" in code \
                or "c3fyNegMode" in code or "static inline" in code:
            prev_call = None
            continue
        va = ln.own[0]
        blk = block_of(rows, va)
        if blk is None:
            continue
        phase, covered = blk[1], blk[2]
        tgt = label_addr(code)
        backward = tgt is not None and tgt <= va
        base = {"line": ln.n, "va": va, "phase": phase, "covered": ",".join(covered), "text": s}
        cond = top_level_if(code)
        if cond and not backward:
            c0, c1 = cond
            c = code[c0:c1]
            if "MV(" not in c and not re.search(r"\b(CC|TC|Rnd|Itoa)\(", c) and "++" not in c and "--" not in c \
                    and "=" not in re.sub(r"[<>!=]=|==", "", c):
                out.append(dict(base, kind="flip", old=f"({c})", new=f"(MV(@, ({c}), !({c})))"))
                ops = REL.findall(c)
                if len(ops) == 1 and ops[0] in ("<", "<=", ">", ">="):
                    sw = {"<": "<=", "<=": "<", ">": ">=", ">=": ">"}[ops[0]]
                    c2 = REL.sub(lambda m: sw, c, count=1)
                    out.append(dict(base, kind="bound", old=f"({c})", new=f"(MV(@, ({c}), ({c2})))"))
        # call statements
        mcall = re.match(r"\s*(CC|TC)\((0x[0-9A-Fa-f]+)", code)
        if mcall and s.endswith(";") or (mcall and ";" in code):
            if re.match(r"\s*(CC|TC)\(", code) and code.strip().endswith(";") and code.count(";") == 1:
                out.append(dict(base, kind="dropcall", old=s.split("//")[0].strip(), new="MS(@) " + s.split("//")[0].strip()))
                if prev_call is not None and prev_call["line"] == ln.n - 1 and prev_call["phase"] == phase:
                    out.append(dict(base, kind="swap", old=prev_call["stmt"], new=s.split("//")[0].strip(),
                                    line2=prev_call["line"]))
                prev_call = {"line": ln.n, "stmt": s.split("//")[0].strip(), "phase": phase}
            else:
                prev_call = None
        else:
            prev_call = None
        # literal call arguments
        for mc in re.finditer(r"\b(CC|TC)\(", code):
            args, end = A.split_args(code, mc.end() - 1)
            for ai, a in enumerate(args[1:], 1):
                if mc.group(1) == "TC" and ai == 1:
                    continue
                v = A.int_literal(a)
                lit = a.strip()
                once = len(re.findall(r"(?<![\w.])" + re.escape(lit) + r"(?![\w.])", code.split("//")[0])) == 1
                if v is not None and 2 <= v < 0x10000 and once and                         re.fullmatch(r"\(?(?:int\))?\s*(0x[0-9A-Fa-f]+|\d+)", lit):
                    out.append(dict(base, kind="const", old=a.strip(), new=f"MV(@, {a.strip()}, ({a.strip()}) + 1)",
                                    argpos=ai))
                    break
            break
        ms = re.match(r"\s*stk\[--sp\] = \(int\)\((0x[0-9a-fA-F]+|\d+)\);", code)
        if ms and 2 <= int(ms.group(1), 0) < 0x10000:
            L = ms.group(1)
            out.append(dict(base, kind="const", old=f"(int)({L})", new=f"(int)(MV(@, {L}, ({L}) + 1))"))
        # plain stores
        mst = re.match(r"\s*((?:G|GU)(?:8|16|32)\([^;]*?\)|F(?:D|U|W|UW|B|UB)\([^;]*?\))\s*=\s*([^=;][^;]*);\s*(//.*)?$", code)
        if mst and "MV(" not in code:
            lhs, rhs = mst.group(1), mst.group(2)
            if lhs.count("(") == lhs.count(")") and not re.search(r"\b(CC|TC)\(", rhs):
                out.append(dict(base, kind="skipstore", old=f"{lhs} = {rhs};", new=f"MSKIP(@, {lhs}, {rhs});"))
        # widths
        mw = re.search(r"\b(G8|G16|FB|FW|GU8|GU16|FUB|FUW)\(", code)
        if mw and "=" in code:
            mac = mw.group(1)
            args, end = A.split_args(code, mw.end() - 1)
            if len(args) == 1:
                whole = code[mw.start():end]
                other = {"G8": "GU8", "G16": "GU16", "FB": "FUB", "FW": "FUW", "GU8": "G8", "GU16": "G16", "FUB": "FB",
                         "FUW": "FW"}[mac]
                lhs_side = code[:mw.start()]
                if not re.search(r"^\s*$", lhs_side) or not re.match(r"\s*" + re.escape(whole) + r"\s*=[^=]", code):
                    out.append(dict(base, kind="width", old=whole,
                                    new=f"MV(@, (int){whole}, (int){other}({args[0]}))"))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--pick", type=int, default=0, help="mutants per phase")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--first-id", type=int, default=100)
    ap.add_argument("--out", default=str(A.ROOT / "re" / "tools" / "c3fy_mutants.tsv"))
    a = ap.parse_args()
    src = A.load_source()
    rows = load_cov()
    cands = candidates(src, rows)
    byph = collections.defaultdict(list)
    for c in cands:
        byph[c["phase"]].append(c)
    if a.list or not a.pick:
        for ph, cs in sorted(byph.items()):
            kinds = collections.Counter(c["kind"] for c in cs)
            cov = sum(1 for c in cs if c["covered"])
            print(f"{ph:18s} {len(cs):5d} candidates ({cov} in covered blocks) {dict(kinds)}")
        return 0
    rnd = random.Random(a.seed)
    kinds_order = ["flip", "skipstore", "const", "dropcall", "bound", "width", "flip", "skipstore", "const",
                   "dropcall", "swap", "flip", "bound", "const", "skipstore"]
    picked = []
    for ph, cs in sorted(byph.items()):
        used_lines = set()
        cov = [c for c in cs if c["covered"]]
        unc = [c for c in cs if not c["covered"]]
        rnd.shuffle(cov)
        rnd.shuffle(unc)
        n_unc = max(2, a.pick // 4) if unc else 0
        want = [(kinds_order[i % len(kinds_order)], i >= a.pick - n_unc) for i in range(a.pick)]
        for kind, from_unc in want:
            pool = unc if from_unc and unc else cov or unc
            pick = next((c for c in pool if c["kind"] == kind and c["line"] not in used_lines), None) or \
                next((c for c in pool if c["line"] not in used_lines), None)
            if pick is None:
                continue
            used_lines.add(pick["line"])
            used_lines.add(pick.get("line2", -1))
            picked.append(pick)
    lines = ["id\tphase\tva\tkind\tline\tline2\tcovered\told\tnew"]
    for i, c in enumerate(picked):
        mid = a.first_id + i
        lines.append("\t".join([str(mid), c["phase"], f"0x{c['va']:08x}", c["kind"], str(c["line"]), str(c.get("line2", "")),
                                c["covered"] or "-", c["old"], c["new"].replace("@", str(mid))]))
    A.pathlib.Path(a.out).write_text("\n".join(lines) + "\n")
    print(f"{len(picked)} mutants -> {a.out}")
    print(collections.Counter(c["phase"] for c in picked))
    print(collections.Counter(c["kind"] for c in picked))
    return 0


if __name__ == "__main__":
    sys.exit(main())
