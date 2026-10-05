"""Variant search for a function that does not match yet: applies mechanical source rewrites to the function's
body, recompiles with VC6, keeps the best instruction-match score (hill climbing) and stops at 100%.

The rewrites are the ones that closed the last 5-20% by hand (re/match/PLAN.md):
  cmp     `x < 5` <-> `x <= 4`, `x > 5` <-> `x >= 6` (integer literals), and operand swap `a < b` -> `b > a`
  ifelse  `if (C) {A} else {B}` -> `if (!(C)) {B} else {A}` (braced blocks)
  tern    `C ? A : B` -> `!(C) ? B : A` (simple ternaries)
  swap    two adjacent plain statements/declarations at the same indentation
  neg     `!x` <-> `x == 0`, `x != 0` -> `x`

    py -3.12 re/tools/match_permute.py re/match/wip/golf_b22.cpp 0x004671a0
    py -3.12 re/tools/match_permute.py <file> <addr> --rounds 4 --write    # rewrite the file with the best variant
The best variant goes to log/permute/<addr>.cpp either way. Variants that do not compile are skipped."""
from __future__ import annotations

import argparse, contextlib, io, pathlib, re, sys, tempfile

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


REWRITES = {"cmp": rw_cmp, "swapcmp": rw_swapcmp, "ifelse": rw_ifelse, "tern": rw_tern, "swap": rw_swap,
            "neg": rw_neg, "litcmp": rw_litcmp, "mulswap": rw_mulswap, "if2tern": rw_if2tern,
            "loopfor": rw_loopfor}


# ------------------------------------------------------------------ scoring
def score(text: str, path: pathlib.Path, addr: int) -> float | None:
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="perm_")) / path.name
    tmp.write_text(text)
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            res = match.compare(tmp, "", False, only=addr, quiet=True)
    except SystemExit:
        return None
    return res[0][4] if res else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src")
    ap.add_argument("addr")
    ap.add_argument("--rounds", type=int, default=6, help="hill-climbing rounds (one accepted rewrite each)")
    ap.add_argument("--only", default=",".join(REWRITES), help="comma-separated rewrite kinds")
    ap.add_argument("--write", action="store_true", help="replace the function in src with the best variant")
    a = ap.parse_args()
    path, addr = pathlib.Path(a.src), int(a.addr, 16)
    text = path.read_text()
    best = score(text, path, addr)
    if best is None:
        raise SystemExit("the file does not compile as given")
    print(f"start {100 * best:.1f}%")
    kinds = [k for k in a.only.split(",") if k in REWRITES]
    tried = 0
    for rnd in range(a.rounds):
        if best >= 1:
            break
        s, e = function_span(text, addr)
        body = text[s:e]
        round_best, round_text, round_kind = best, None, None
        seen = set()
        for kind in kinds:
            for nb in REWRITES[kind](body):
                if nb in seen or nb == body:
                    continue
                seen.add(nb)
                cand = text[:s] + nb + text[e:]
                sc = score(cand, path, addr)
                tried += 1
                if sc is not None and sc > round_best:
                    round_best, round_text, round_kind = sc, cand, kind
                    if sc >= 1:
                        break
            if round_best >= 1:
                break
        if round_text is None:
            print(f"round {rnd + 1}: no improvement ({len(seen)} variants)")
            break
        best, text = round_best, round_text
        print(f"round {rnd + 1}: {100 * best:.1f}% via {round_kind} ({len(seen)} variants)")
    out = ROOT / "log" / "permute" / f"{addr:08x}.cpp"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text)
    print(f"best {100 * best:.1f}% after {tried} compiles -> {out.relative_to(ROOT)}")
    if a.write and best > (score(path.read_text(), path, addr) or 0):
        path.write_text(text)
        print(f"wrote {path}")
    return 0 if best >= 1 else 1


if __name__ == "__main__":
    sys.exit(main())
