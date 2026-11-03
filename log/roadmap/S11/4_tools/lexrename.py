"""S11.4: lexer-aware renaming used by gen.py. C++ text is split into code / comment / string segments by a small lexer; Python
text by the tokenize module. Each segment kind gets its own replacement rule (see gen.py)."""
from __future__ import annotations

import io
import re
import tokenize

WORD_L = r"(?<![A-Za-z0-9_])"
WORD_R = r"(?![A-Za-z0-9_])"
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def word_sub(text: str, pairs: list[tuple[str, str]], protect: list[str] = (), columns: bool = False) -> tuple[str, int]:
    """Replace whole words (identifier characters on neither side), longest old first. Protected substrings are left alone.
    columns: a name followed by two or more spaces keeps the next column where it was (comment tables), at least two spaces."""
    keep = {}
    for k, p in enumerate(protect):
        tok = f"\x00P{k}\x00"
        if p in text:
            keep[tok] = p
            text = text.replace(p, tok)
    n = 0
    for old, new in sorted(pairs, key=lambda x: -len(x[0])):
        if columns:
            def rep(m, old=old, new=new):
                sp = m.group(1) or ""
                if len(sp) >= 2:
                    sp = " " * max(2, len(sp) - (len(new) - len(old)))
                return new + sp
            text, c = re.subn(WORD_L + re.escape(old) + WORD_R + r"( {2,})?", rep, text)
        else:
            text, c = re.subn(WORD_L + re.escape(old) + WORD_R, lambda m, new=new: new, text)
        n += c
    for tok, p in keep.items():
        text = text.replace(tok, p)
    return text, n


def ident_sub(token: str, pairs: list[tuple[str, str]], exact_only: set[str] = frozenset()) -> str:
    """Rename one identifier token: the old name as the whole token, or as an `_`-delimited component of it."""
    for old, new in pairs:
        if token == old:
            return new
        if old in exact_only:
            continue
        if token.startswith(old + "_"):
            return new + token[len(old):]
        if token.endswith("_" + old):
            return token[: -len(old)] + new
        k = token.find("_" + old + "_")
        if k >= 0:
            return token[: k + 1] + new + token[k + 1 + len(old):]
    return token


# ----------------------------------------------------------------------------------------------------------------- C++
def cpp_segments(text: str) -> list[tuple[str, str]]:
    """[(kind, text)] with kind code / comment / string; concatenation == text."""
    out = []
    i, n, start, kind = 0, len(text), 0, "code"

    def flush(j, k):
        nonlocal start
        if j > start:
            out.append((k, text[start:j]))
        start = j

    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            flush(i, "code")
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(("comment", text[i:j]))
            start = i = j
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            flush(i, "code")
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(("comment", text[i:j]))
            start = i = j
            continue
        if c == "R" and text.startswith('R"', i) and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            m = re.match(r'R"([^ ()\\\t\n]{0,16})\(', text[i:])
            if m:
                flush(i, "code")
                end = text.find(")" + m.group(1) + '"', i)
                j = n if end < 0 else end + len(m.group(1)) + 2
                out.append(("string", text[i:j]))
                start = i = j
                continue
        if c in "\"'":
            if c == "'" and i > 0 and text[i - 1].isalnum():   # digit separator 1'000 (not used here, but harmless)
                i += 1
                continue
            flush(i, "code")
            j = i + 1
            while j < n and text[j] != c:
                if text[j] == "\\":
                    j += 1
                elif text[j] == "\n":
                    break
                j += 1
            j = min(j + 1, n)
            out.append(("string", text[i:j]))
            start = i = j
            continue
        i += 1
    flush(n, "code")
    assert "".join(t for _, t in out) == text
    return out


def cpp_code_sub(code: str, id_pairs: list[tuple[str, str]], exact_only: set[str]) -> str:
    return IDENT.sub(lambda m: ident_sub(m.group(0), id_pairs, exact_only), code)


# -------------------------------------------------------------------------------------------------------------- Python
def py_spans(text: str) -> list[tuple[int, int, str]]:
    """(start offset, end offset, kind) of every string / comment token of a Python source."""
    lines = text.splitlines(keepends=True)
    starts = [0]
    for ln in lines:
        starts.append(starts[-1] + len(ln))
    off = lambda rc: starts[rc[0] - 1] + rc[1]
    out = []
    for t in tokenize.generate_tokens(io.StringIO(text).readline):
        if t.type == tokenize.COMMENT:
            out.append((off(t.start), off(t.end), "comment"))
        elif t.type == tokenize.STRING or t.type == getattr(tokenize, "FSTRING_MIDDLE", -1):
            out.append((off(t.start), off(t.end), "string"))
    return out
