"""Variant search for a function that does not match yet: applies mechanical source rewrites to the function's
body, recompiles with VC6, keeps the best instruction-match score (hill climbing) and stops at 100%.

The rewrites are the ones that closed the last 5-20% by hand (re/match/PLAN.md):
  cmp     `x < 5` <-> `x <= 4`, `x > 5` <-> `x >= 6` (integer literals), and operand swap `a < b` -> `b > a`
  ifelse  `if (C) {A} else {B}` -> `if (!(C)) {B} else {A}` (braced blocks)
  tern    `C ? A : B` -> `!(C) ? B : A` (simple ternaries)
  swap    two adjacent plain statements/declarations at the same indentation
  neg     `!x` <-> `x == 0`, `x != 0` -> `x`
  litcmp  constant-left compares (`0x3ff < x` -> `x > 0x3ff` / `x >= 0x400`); mulswap; if2tern; loopfor (Ghidra's
          guarded do-while back to `for`)
Scheduling and register allocation (VC6 /O2 follows statement order and where variables are introduced):
  move      a statement 2-4 lines up/down in its run (swap covers 1)
  commute   operands of + & | ^ == !=
  incform   `i++` / `++i` / `i += 1` / `i = i + 1`
  signed    int <-> unsigned (char/short families) on local declarations
  initsplit `T x = e;` <-> `T x;` ... `x = e;`
  forwhile  `for` -> `while`
The score is (exact, registers-renamed, in-place): ties on the exact match are broken by how close the code is up
to register names and by instructions already at their original position. Variants compile in parallel (--jobs);
--walk N adds a random walk that accepts equal scores, for plateaus where two moves are needed.

    py -3.12 re/tools/match_permute.py re/match/wip/golf_b22.cpp 0x004671a0
    py -3.12 re/tools/match_permute.py <file> <addr> --rounds 4 --write    # rewrite the file with the best variant
    py -3.12 re/tools/match_permute.py <file> <addr> --rounds 3 --walk 60 --jobs 8
The best variant goes to log/permute/<addr>.cpp either way. Variants that do not compile are skipped."""
from __future__ import annotations

import argparse, concurrent.futures, contextlib, difflib, io, pathlib, random, re, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402


# ------------------------------------------------------------------ locate the function
def function_span(text: str, addr: int) -> tuple[int, int]:
    """(start, end) of the body braces of the function annotated with `// MATCH: <mod> 0x<addr>`."""
    m = None
    for mm in re.finditer(r"//\s*MATCH:\s*\S+\s+(0x[0-9a-fA-F]+)", text):
        if int(mm.group(1), 16) == addr:
            m = mm
    if not m:
        raise SystemExit(f"no // MATCH: annotation for 0x{addr:08x}")
    open_ = text.index("{", m.end())
    return open_, matching_brace(text, open_) + 1


def matching_brace(text: str, i: int) -> int:
    depth = 0
    for k in range(i, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                return k
    raise ValueError("unbalanced braces")


# ------------------------------------------------------------------ rewrites (each yields new bodies)
CMP = re.compile(r"(?<![<>=!])(<=|>=|<|>)(?![<>=])\s*(-?\d+)\b")


def rw_cmp(body: str):
    for m in CMP.finditer(body):
        op, n = m.group(1), int(m.group(2))
        alt = {"<": ("<=", n - 1), "<=": ("<", n + 1), ">": (">=", n + 1), ">=": (">", n - 1)}[op]
        yield body[:m.start()] + f"{alt[0]} {alt[1]}" + body[m.end():]


SIMPLE = r"[A-Za-z_][\w\.\->\[\]]*"
SWAPCMP = re.compile(rf"\b({SIMPLE})\s*(<=|>=|<|>|==|!=)\s*({SIMPLE})")


def rw_swapcmp(body: str):
    flip = {"<": ">", ">": "<", "<=": ">=", ">=": "<=", "==": "==", "!=": "!="}
    for m in SWAPCMP.finditer(body):
        a, op, b = m.groups()
        if a in ("return", "else") or b in ("return",):
            continue
        yield body[:m.start()] + f"{b} {flip[op]} {a}" + body[m.end():]


def rw_ifelse(body: str):
    for m in re.finditer(r"\bif\s*\(", body):
        cond_open = m.end() - 1
        depth, k = 0, cond_open
        while k < len(body):
            depth += body[k] == "("
            depth -= body[k] == ")"
            if depth == 0:
                break
            k += 1
        cond = body[cond_open + 1:k]
        j = k + 1
        while j < len(body) and body[j].isspace():
            j += 1
        if j >= len(body) or body[j] != "{":
            continue
        a_end = matching_brace(body, j)
        rest = body[a_end + 1:]
        em = re.match(r"\s*else\s*\{", rest)
        if not em:
            continue
        b_open = a_end + 1 + em.end() - 1
        b_end = matching_brace(body, b_open)
        a_blk, b_blk = body[j:a_end + 1], body[b_open:b_end + 1]
        yield body[:m.start()] + f"if (!({cond})) " + b_blk + " else " + a_blk + body[b_end + 1:]


TERN = re.compile(r"([^=;?(),]+?)\s*\?\s*([^:;?]+?)\s*:\s*([^;)?,]+)")


def rw_tern(body: str):
    for m in TERN.finditer(body):
        c, a, b = (g.strip() for g in m.groups())
        if not c or c.startswith("return"):
            pre = "return " if c.startswith("return") else ""
            c = c[len("return"):].strip() if pre else c
            if not c:
                continue
        else:
            pre = ""
        yield body[:m.start()] + f"{pre}!({c}) ? {b} : {a}" + body[m.end():]


STMT = re.compile(r"^([ \t]+)([^\n{}]*;)[ \t]*$", re.M)
CONTROL = re.compile(r"^\s*(if|else|for|while|do|return|switch|case|default|break|continue)\b")


def rw_swap(body: str):
    lines = body.split("\n")
    for i in range(len(lines) - 1):
        a, b = lines[i], lines[i + 1]
        ma, mb = STMT.match(a), STMT.match(b)
        if not (ma and mb) or ma.group(1) != mb.group(1):
            continue
        if CONTROL.match(a) or CONTROL.match(b):
            continue
        yield "\n".join(lines[:i] + [b, a] + lines[i + 2:])


def rw_neg(body: str):
    for m in re.finditer(r"!\s*(" + SIMPLE + r")(?!\s*\()", body):
        yield body[:m.start()] + f"{m.group(1)} == 0" + body[m.end():]
    for m in re.finditer(r"(" + SIMPLE + r")\s*!=\s*0\b", body):
        yield body[:m.start()] + m.group(1) + body[m.end():]
    for m in re.finditer(r"(" + SIMPLE + r")\s*==\s*0\b", body):
        yield body[:m.start()] + f"!{m.group(1)}" + body[m.end():]


def operand_end(s: str, i: int) -> int:
    """End of the expression starting at s[i]: stops at `)`, `&&`, `||`, `;`, `,` or `?` at depth 0."""
    depth = 0
    k = i
    while k < len(s):
        c = s[k]
        if c in "([":
            depth += 1
        elif c in ")]":
            if depth == 0:
                break
            depth -= 1
        elif depth == 0 and (s.startswith("&&", k) or s.startswith("||", k) or c in ";,?"):
            break
        k += 1
    while k > i and s[k - 1] == " ":
        k -= 1
    return k


LITCMP = re.compile(r"(?<![\w.)\]])(-?0x[0-9a-fA-F]+|-?\d+)\s*(<=|>=|<|>)(?![<>=])\s*")


def rw_litcmp(body: str):
    """Ghidra writes the constant first (`0x3ff < x`); sources put it second and VC6's code differs:
    `x > 0x3ff` and `x >= 0x400` are both tried."""
    flip = {"<": ">", ">": "<", "<=": ">=", ">=": "<="}
    for m in LITCMP.finditer(body):
        n = int(m.group(1), 0)
        op = m.group(2)
        e = operand_end(body, m.end())
        x = body[m.end():e]
        if not x.strip():
            continue
        f = flip[op]
        alts = [f"{x} {f} {m.group(1)}"]
        adj = {">": (">=", n + 1), "<": ("<=", n - 1), ">=": (">", n - 1), "<=": ("<", n + 1)}[f]
        alts.append(f"{x} {adj[0]} {adj[1]:#x}" if "0x" in m.group(1) and adj[1] >= 0 else f"{x} {adj[0]} {adj[1]}")
        for alt in alts:
            yield body[:m.start()] + alt + body[e:]


ATOM = rf"(?:{SIMPLE}|0x[0-9a-fA-F]+|\d+|\([^()]*\))"
MUL = re.compile(rf"({ATOM})\s*\*\s*({ATOM})")


def rw_mulswap(body: str):
    for m in MUL.finditer(body):
        a, b = m.groups()
        if a.endswith(("char", "int", "short", "void")) or b in ("",):
            continue
        yield body[:m.start()] + f"{b} * {a}" + body[m.end():]


IF2TERN = re.compile(r"if \((.+?)\) \{\n\s*(\w+) = ([^;\n]+);\n\s*\}\n\s*else \{\n\s*\2 = ([^;\n]+);\n\s*\}")


def rw_if2tern(body: str):
    """`if (C) {v = A;} else {v = B;}` -> `v = (C) ? A : B;` (and with the condition negated)."""
    for m in IF2TERN.finditer(body):
        c, v, a, b = m.groups()
        yield body[:m.start()] + f"{v} = ({c}) ? {a} : {b};" + body[m.end():]
        yield body[:m.start()] + f"{v} = !({c}) ? {b} : {a};" + body[m.end():]


GUARDED_DO = re.compile(r"(\n[ \t]*)(\w+) = ([^;\n]+);(\s*\n[ \t]*)if \((?:0 < ([^\n]+?)|([^\n]+?) > 0)\) \{\s*\n([ \t]*)do \{\n")


def rw_loopfor(body: str):
    """Ghidra's `v = 0; if (0 < n) { do { ... v = v + 1; ... } while (v < n); }` back to
    `for (v = 0; v < n; v++) { ... }` (the increment moved to the end) and to a plain `while`."""
    for m in GUARDED_DO.finditer(body):
        ind, v, init = m.group(1), m.group(2), m.group(3)
        n = m.group(5) or m.group(6)
        do_open = m.end() - 1                     # the newline after `do {`
        brace = body.rindex("{", 0, m.end())
        close = matching_brace(body, brace)
        tail = re.match(r"\s*while \((.+?)\);\s*\n[ \t]*\}", body[close + 1:], re.S)
        if not tail or tail.group(1).replace(" ", "") != f"{v}<{n}".replace(" ", ""):
            continue
        inner = body[do_open + 1:close]
        inc = re.compile(rf"^[ \t]*{re.escape(v)} = {re.escape(v)} \+ 1;[ \t]*\n", re.M)
        if len(inc.findall(inner)) != 1:
            continue
        inner2 = inc.sub("", inner)
        end = close + 1 + tail.end()
        lines = [l[2:] if l.startswith("  ") else l for l in inner2.split("\n")]
        new_inner = "\n".join(lines)
        yield (body[:m.start()] + f"{ind}for ({v} = {init}; {v} < {n}; {v}++) {{\n" + new_inner.rstrip() +
               f"{ind}}}" + body[end:])
        yield (body[:m.start()] + f"{ind}for ({v} = {init}; {v} < {n}; {v} = {v} + 1) {{\n" + new_inner.rstrip() +
               f"{ind}}}" + body[end:])


# ------------------------------------------------------------------ rewrites aimed at scheduling and registers
# VC6 /O2 picks registers and spill slots from the order in which variables are introduced and used, and
# schedules loads by the order of the statements; these rewrites move exactly those levers.
INC = re.compile(rf"(?<![\w+\-])({SIMPLE})(\+\+|--)(?![\w+\-])|(?<![\w+\-])(\+\+|--)({SIMPLE})")


def rw_incform(body: str):
    """`i++` <-> `++i` <-> `i += 1` <-> `i = i + 1` (statement-level only: the value is not used)."""
    for m in re.finditer(rf"(?m)^([ \t]*)(?:({SIMPLE})(\+\+|--)|(\+\+|--)({SIMPLE}));", body):
        ind = m.group(1)
        v = m.group(2) or m.group(5)
        op = (m.group(3) or m.group(4))[0]
        for alt in (f"{op}{op}{v}", f"{v}{op}{op}", f"{v} {op}= 1", f"{v} = {v} {op} 1"):
            if alt != m.group(0)[len(ind):-1]:
                yield body[:m.start()] + f"{ind}{alt};" + body[m.end():]
    for m in re.finditer(rf"\b({SIMPLE}) ([+-])= 1;", body):
        v, op = m.groups()
        yield body[:m.start()] + f"{v}{op}{op};" + body[m.end():]
    for m in re.finditer(rf"\b({SIMPLE}) = \1 ([+-]) 1;", body):
        v, op = m.groups()
        yield body[:m.start()] + f"{v}{op}{op};" + body[m.end():]
    # in a for header
    for m in re.finditer(rf";\s*({SIMPLE})(\+\+|--)\)", body):
        v, op = m.group(1), m.group(2)[0]
        yield body[:m.start()] + f"; {op}{op}{v})" + body[m.end():]


COMMUTE = re.compile(rf"({ATOM})\s*(\+|&|\||\^|==|!=)\s*({ATOM})(?![\w(\[.])")


def rw_commute(body: str):
    """Operand order of commutative operators decides evaluation order and which value lands in which register."""
    for m in COMMUTE.finditer(body):
        a, op, b = m.groups()
        if a == b or a in ("return", "case", "else") or re.fullmatch(r"\d+|0x[0-9a-fA-F]+", a):
            continue
        before = body[:m.start()].rstrip()
        if before.endswith(("*", "/", "-", "%", "<<", ">>")):    # left operand belongs to a tighter operator
            continue
        yield body[:m.start()] + f"{b} {op} {a}" + body[m.end():]


DECL = re.compile(r"(?m)^([ \t]+)(unsigned int|unsigned char|unsigned short|unsigned|int|char|short|long)( \*?\w+(?: = [^;\n]+)?;)$")


def rw_signed(body: str):
    """int <-> unsigned (and char/short families) on local declarations: changes compare/shift/extend forms."""
    flip = {"int": "unsigned", "unsigned": "int", "unsigned int": "int", "char": "unsigned char",
            "unsigned char": "char", "short": "unsigned short", "unsigned short": "short", "long": "unsigned long"}
    for m in DECL.finditer(body):
        yield body[:m.start()] + m.group(1) + flip[m.group(2)] + m.group(3) + body[m.end():]


def rw_initsplit(body: str):
    """`T x = e;` <-> `T x;` ... `x = e;` (where the variable is introduced changes its live range)."""
    for m in re.finditer(r"(?m)^([ \t]+)([A-Za-z_][\w ]*?[\w*]) (\*?)(\w+) = ([^;\n]+);$", body):
        ind, ty, star, v, e = m.groups()
        if ty in ("return", "else", "case") or "(" in ty:
            continue
        yield body[:m.start()] + f"{ind}{ty} {star}{v};\n{ind}{v} = {e};" + body[m.end():]
    lines = body.split("\n")
    for i in range(len(lines) - 1):
        d = re.match(r"^([ \t]+)([A-Za-z_][\w ]*?[\w*]) (\*?)(\w+);$", lines[i])
        if not d or d.group(2) in ("return", "else", "break", "continue"):
            continue
        for j in range(i + 1, min(i + 12, len(lines))):
            a = re.match(rf"^{re.escape(d.group(1))}{d.group(4)} = ([^;\n]+);$", lines[j])
            if a:
                nl = lines[:i] + lines[i + 1:j] + [f"{d.group(1)}{d.group(2)} {d.group(3)}{d.group(4)} = {a.group(1)};"] + lines[j + 1:]
                yield "\n".join(nl)
                break


def rw_move(body: str):
    """Move one plain statement 2-4 lines up or down inside a run of statements at the same indentation
    (rw_swap covers distance 1): VC6 schedules loads and stores in statement order."""
    lines = body.split("\n")
    n = len(lines)

    def plain(k, ind):
        m = STMT.match(lines[k])
        return m and m.group(1) == ind and not CONTROL.match(lines[k])
    for i in range(n):
        m = STMT.match(lines[i])
        if not m or CONTROL.match(lines[i]):
            continue
        ind = m.group(1)
        for d in (2, 3, 4):
            if i + d < n and all(plain(k, ind) for k in range(i, i + d + 1)):
                yield "\n".join(lines[:i] + lines[i + 1:i + d + 1] + [lines[i]] + lines[i + d + 1:])
            if i - d >= 0 and all(plain(k, ind) for k in range(i - d, i + 1)):
                yield "\n".join(lines[:i - d] + [lines[i]] + lines[i - d:i] + lines[i + 1:])


def rw_forwhile(body: str):
    """`for (init; c; inc) {B}` -> `init; while (c) {B inc;}` (only bodies without `continue`)."""
    for m in re.finditer(r"(?m)^([ \t]*)for \(([^;\n]*); ([^;\n]+); ([^)\n]+)\) \{\n", body):
        ind, init, c, inc = m.groups()
        close = matching_brace(body, m.end() - 2)
        inner = body[m.end():close]
        if "continue" in inner:
            continue
        pre = f"{ind}{init};\n" if init.strip() else ""
        yield (body[:m.start()] + f"{pre}{ind}while ({c}) {{\n" + inner + f"    {ind}{inc};\n{ind}}}" +
               body[close + 1:])


REWRITES = {"cmp": rw_cmp, "swapcmp": rw_swapcmp, "ifelse": rw_ifelse, "tern": rw_tern, "swap": rw_swap,
            "neg": rw_neg, "litcmp": rw_litcmp, "mulswap": rw_mulswap, "if2tern": rw_if2tern,
            "loopfor": rw_loopfor, "incform": rw_incform, "commute": rw_commute, "signed": rw_signed,
            "initsplit": rw_initsplit, "move": rw_move, "forwhile": rw_forwhile}


# ------------------------------------------------------------------ scoring
REGS = {}
for _fam, _names in {"a": "eax ax al ah", "b": "ebx bx bl bh", "c": "ecx cx cl ch", "d": "edx dx dl dh",
                     "si": "esi si", "di": "edi di", "bp": "ebp bp"}.items():
    for _n in _names.split():
        REGS[_n] = _fam
REG_RE = re.compile(r"\b(" + "|".join(sorted(REGS, key=len, reverse=True)) + r")\b")


def alpha(keys):
    """Rename general registers by order of first appearance: equal sequences then differ only in register choice."""
    m = {}

    def sub(x):
        f = REGS[x.group(1)]
        if f not in m:
            m[f] = f"r{len(m)}"
        return m[f] + x.group(1)[-1] if len(x.group(1)) == 2 and x.group(1)[-1] in "lh" else m[f]
    return [REG_RE.sub(sub, k) for k in keys]


def score(text: str, path: pathlib.Path, addr: int):
    """(exact match, match with registers renamed, positional match) or None when it does not compile.
    Tuples compare left to right, so the exact score decides and the others break ties: a variant whose code
    differs only in register roles, or that moves instructions into place, counts as progress."""
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="perm_")) / path.name
    tmp.write_text(text)
    try:
        # quiet=True prints nothing; no redirect_stdout here: it swaps the process-wide sys.stdout and, with
        # parallel threads, one thread restores another's buffer and the main output is lost
        res = match.compare(tmp, "", False, only=addr, quiet=True)
    except SystemExit:
        return None
    if not res:
        return None
    acc, a, b = res[0][4], res[0][5], res[0][6]
    ka, kb = [x[1] for x in a], [x[1] for x in b]
    sm = difflib.SequenceMatcher(None, alpha(ka), alpha(kb), autojunk=False)
    al = 2 * sum(bl.size for bl in sm.get_matching_blocks()) / max(1, len(ka) + len(kb))
    pos = sum(1 for x, y in zip(ka, kb) if x == y) / max(1, len(kb))
    return (acc, round(al, 4), round(pos, 4))


def fmt(sc):
    return f"{100 * sc[0]:.1f}% (regs-renamed {100 * sc[1]:.1f}%, in place {100 * sc[2]:.1f}%)"


def neighbours(text, addr, kinds):
    s, e = function_span(text, addr)
    body = text[s:e]
    seen = set()
    for kind in kinds:
        for nb in REWRITES[kind](body):
            if nb in seen or nb == body:
                continue
            seen.add(nb)
            yield kind, text[:s] + nb + text[e:]


def evaluate(cands, path, addr, jobs):
    """Compile candidates in parallel (each VC6 run is its own process and temp dir)."""
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        return list(zip(cands, ex.map(lambda c: score(c[1], path, addr), cands)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("addr")
    ap.add_argument("--rounds", type=int, default=6, help="greedy rounds (one accepted rewrite each)")
    ap.add_argument("--only", default=",".join(REWRITES), help="comma-separated rewrite kinds")
    ap.add_argument("--jobs", type=int, default=6, help="parallel compiles")
    ap.add_argument("--walk", type=int, default=0,
                    help="after the greedy rounds, N steps of a random walk that also accepts equal scores "
                         "(plateaus: register and scheduling changes often need two moves before the score moves)")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--write", action="store_true", help="replace the function in src with the best variant")
    a = ap.parse_args()
    path, addr = pathlib.Path(a.src), int(a.addr, 16)
    text = path.read_text()
    best = score(text, path, addr)
    if best is None:
        raise SystemExit("the file does not compile as given")
    print(f"start {fmt(best)}", flush=True)
    kinds = [k for k in a.only.split(",") if k in REWRITES]
    tried = 0
    for rnd in range(a.rounds):
        if best[0] >= 1:
            break
        cands = list(neighbours(text, addr, kinds))
        res = [(c, sc) for c, sc in evaluate(cands, path, addr, a.jobs) if sc is not None]
        tried += len(cands)
        top = max(res, key=lambda r: r[1], default=None)
        if not top or top[1] <= best:
            print(f"round {rnd + 1}: no improvement ({len(cands)} variants)", flush=True)
            break
        (kind, text), best = top
        print(f"round {rnd + 1}: {fmt(best)} via {kind} ({len(cands)} variants)", flush=True)
    if a.walk and best[0] < 1:
        rng = random.Random(a.seed)
        cur, cur_sc = text, best
        stall = 0
        for step in range(a.walk):
            cands = list(neighbours(cur, addr, kinds))
            if not cands:
                break
            pick = rng.sample(cands, min(len(cands), a.jobs))
            res = [(c, sc) for c, sc in evaluate(pick, path, addr, a.jobs) if sc is not None]
            tried += len(pick)
            ok = [r for r in res if r[1] >= cur_sc]
            if ok:
                (kind, cur), cur_sc = rng.choice(ok)
                stall = 0
                if cur_sc > best:
                    text, best = cur, cur_sc
                    print(f"walk {step + 1}: {fmt(best)} via {kind}", flush=True)
                    if best[0] >= 1:
                        break
            else:
                stall += 1
                if stall >= 8:                       # restart from the best point
                    cur, cur_sc, stall = text, best, 0
    out = ROOT / "log" / "permute" / f"{addr:08x}.cpp"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text)
    print(f"best {fmt(best)} after {tried} compiles -> {out.relative_to(ROOT)}", flush=True)
    if a.write and best > (score(path.read_text(), path, addr) or (0,)):
        path.write_text(text)
        print(f"wrote {path}", flush=True)
    return 0 if best[0] >= 1 else 1


if __name__ == "__main__":
    sys.exit(main())
