"""P24D: check a language pack (the rules port/src/sim/text.cpp, lang section load_pack applies) and, given SG_LANG_DUMP files, list the drawn
English texts the pack does not cover.

    ~/bin/py -3.12 scripts/lang/check_pack.py [lang/simgolf_es.txt] [--dump <dump.tsv> ...] [--misses <out.tsv>]

Checks per line: `english<TAB>translation`; both sides encodable in code page 1252; the printf conversions (%s %d %5.1f ..., %%
a literal percent) the same ones in the same order on both sides; no English side twice. Exit 1 on any refused line."""
from __future__ import annotations

import argparse
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BLANKS = " \t\r\n"
CONV = re.compile(r"%(?:%|[-+ #0]*\d*(?:\.\d*)?[lh]*[diouxXcsfeEgG])")


def unescape(s: str) -> str:
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t", "r": "\r"}.get(m.group(1), m.group(1)), s)


def convs(s: str) -> list[str]:
    return [c for c in CONV.findall(s) if c != "%%"]


def lone_percent(s: str) -> bool:
    return bool(re.search(r"%", CONV.sub("", s)))


def to_regex(key: str) -> re.Pattern:
    out, i = "", 0
    for m in CONV.finditer(key):
        out += re.escape(key[i:m.start()])
        c = m.group(0)
        out += "%" if c == "%%" else "(.)" if c.endswith("c") else "(.+?)" if c.endswith("s") else r"(-?\d(?:[\d]|[,.](?=\d))*)"
        i = m.end()
    out += re.escape(key[i:])
    return re.compile(out + r"\Z", re.S)


def load(path: pathlib.Path):
    errs, exact, pats, seen = [], {}, [], {}
    for n, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        if "\t" not in line or line.startswith("\t") or line.endswith("\t"):
            errs.append(f"line {n}: not english<TAB>translation")
            continue
        en, tx = (unescape(x).rstrip(BLANKS) for x in line.split("\t", 1))  # trailing blanks are no part of a key
        try:
            en.encode("cp1252"), tx.encode("cp1252")
        except UnicodeEncodeError as e:
            errs.append(f"line {n}: not code page 1252: {e}")
            continue
        if lone_percent(en) or lone_percent(tx):
            errs.append(f"line {n}: a '%' that starts no conversion")
            continue
        if convs(en) != convs(tx):
            errs.append(f"line {n}: conversions differ {convs(en)} vs {convs(tx)}")
            continue
        if en in seen:
            errs.append(f"line {n}: English side already on line {seen[en]}")
        seen[en] = n
        if convs(en):
            pats.append(to_regex(en))
        else:
            exact[en.replace("%%", "%")] = tx
    return errs, exact, pats


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pack", nargs="?", default=str(ROOT / "lang" / "simgolf_es.txt"))
    ap.add_argument("--dump", nargs="*", default=[])
    ap.add_argument("--misses", default="")
    a = ap.parse_args()
    errs, exact, pats = load(pathlib.Path(a.pack))
    for e in errs:
        print(e)
    print(f"{len(exact)} exact, {len(pats)} patterns, {len(errs)} refused")
    if a.dump:
        texts = {}
        for d in a.dump:
            for line in pathlib.Path(d).read_text(encoding="utf-8").splitlines():
                parts = line.split("\t", 2)
                if len(parts) == 3:
                    texts.setdefault(unescape(parts[2]).rstrip(BLANKS), parts[1])
        miss = [t for t in texts if t not in exact and not any(p.match(t) for p in pats)]
        print(f"dump: {len(texts)} distinct texts, {len(texts) - len(miss)} covered, {len(miss)} not")
        if a.misses:
            with open(a.misses, "w", encoding="utf-8", newline="\n") as f:
                for t in sorted(miss):
                    f.write(f"{texts[t]}\t{t.encode('unicode_escape').decode('ascii') if False else t.replace(chr(92), chr(92) * 2).replace(chr(10), chr(92) + 'n').replace(chr(9), chr(92) + 't')}\n")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
