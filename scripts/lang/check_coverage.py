"""P24K: the language pack's coverage gate. Every English string of the user's exe (lang/source/exe_strings.tsv, made by
scripts/lang/extract_strings.py) must be covered by the pack or listed in the exclusions; the uncovered ones are printed.

    ~/bin/py -3.12 scripts/lang/check_coverage.py [--lang-dir lang] [--pack <pack>] [--strings <tsv>] [--exclude <tsv>]
                                                  [--out <uncovered.tsv>] [--verbose]

No game text lives in this file: the strings, the pack and the exclusions are the user's files in the gitignored lang/ folder.

A string is covered when (the lookup rules of port/src/sim/text.cpp, lang section):
- it is an exact key, also flattened (line breaks and '^' as spaces) and without its leading / trailing blanks;
- a pattern of the pack matches it whole;
- it is a literal piece of a key (a fragment the simulation concatenates into a message the pack holds as a pattern), compared
  with its own blanks (" at " must be in a key as " at ");
- it holds line breaks and every non-empty line is covered by the rules above, also without its leading blanks (a popup:
  showPopup draws each line by itself; the lookup drops an option line's leading space);
- without its blanks it is a literal piece of a key and at least 4 bytes long.
Not text, skipped without a list: file names / paths, code identifiers (an underscore, '@', '?', '$', CamelCase or a printf-only
format with no space), binary noise (a byte outside letters, digits, blanks and the usual punctuation, or no word with a vowel,
ordinals such as 3rd excepted).
Exclusions file (`lang/coverage_exclude.tsv`): `[va<TAB>]0xVA<TAB>category<TAB>note`, `range<TAB>0xLO-0xHI<TAB>category<TAB>note` (a va range,
both ends included), `pool<TAB>f_strpool_xxxxxx<TAB>category<TAB>note` or `re<TAB><python regex><TAB>category<TAB>note` (a full
match on the text). Categories are free words (debug, crt, net, font, name, path ...); the summary counts them.
Exit 1 when any string is uncovered (or a file is missing), 0 otherwise."""
from __future__ import annotations

import argparse
import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
BLANKS = " \t\r\n"
CONV = re.compile(r"%(?:%|[-+ #0]*\d*(?:\.\d*)?[lh]*[diouxXcsfeEgG])")
FILE_EXT = re.compile(r"\.(wav|pcx|bmp|flc|txt|sve|tga|glf|chr|pro|dta|dll|exe|ttf|bik|ini|lib|jpg|pal|fot|cpp|h|cse|srf|pcf|pcg|"
                      r"tmp|dat|mid|rmi)\b", re.I)
TEXT_BYTES = re.compile(r"[A-Za-z0-9 \t\r\n.,;:'\"!?()\-&%/#+*=<>\[\]\xa7\xc0-\xff’]*")
ORDINAL = re.compile(r"\d+(st|nd|rd|th)")
VOWEL_WORD = re.compile(r"[A-Za-z]*[aeiouyAEIOUY][A-Za-z]*")


def unescape(s: str) -> str:
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t", "r": "\r"}.get(m.group(1), m.group(1)), s)


def flat(t: str) -> str:
    return t.replace("\n", " ").replace("^", " ").rstrip(BLANKS)


def to_regex(key: str) -> re.Pattern:
    out, i = "", 0
    for m in CONV.finditer(key):
        out += re.escape(key[i:m.start()])
        c = m.group(0)
        out += "%" if c == "%%" else "(.)" if c.endswith("c") else "(.*?)" if c.endswith("s") else r"(-?\d(?:[\d]|[,.](?=\d))*)"
        i = m.end()
    out += re.escape(key[i:])
    return re.compile(out + r"\Z", re.S)


class Pack:
    def __init__(self, path: pathlib.Path):
        self.exact: set[str] = set()
        self.pats: list[re.Pattern] = []
        pieces: list[str] = []
        for line in path.read_text(encoding="utf-8-sig").splitlines():
            if not line or line.startswith("#") or "\t" not in line:
                continue
            en = unescape(line.split("\t", 1)[0]).rstrip(BLANKS)
            parts = [p.replace("\x01", "%") for p in CONV.split(en.replace("%%", "\x01"))]  # %% is a literal percent sign
            pieces.extend(p for p in parts if p.strip(BLANKS))
            if len(parts) > 1:
                self.pats.append(to_regex(en))
            else:
                self.exact.add(en.replace("%%", "%"))
        self.exact_flat = {flat(k) for k in self.exact}
        self.blob = "\x00".join(pieces)

    def one(self, t: str) -> str | None:
        core = t.rstrip(BLANKS)
        for c in (core, core.lstrip(BLANKS)):
            if not c:
                continue
            if c in self.exact or flat(c) in self.exact_flat:
                return "exact"
            if any(p.match(c) or p.match(flat(c)) for p in self.pats):
                return "pattern"
        if core.strip(BLANKS) and t in self.blob:
            return "fragment"
        if core.strip(BLANKS) and core in self.blob:
            return "fragment"
        return None

    def status(self, t: str) -> str | None:
        st = self.one(t)
        if st is None and "\n" in t:
            lines = [x for x in t.split("\n") if x.strip(BLANKS)]
            if lines and all(self.one(x) or self.one(x.lstrip(BLANKS)) for x in lines):
                st = "lines"
        if st is None:
            # a fragment with blanks on both sides (" inland ") inside a key's words; at least 4 bytes, so " at " stays listed
            core = t.strip(BLANKS)
            if len(core) >= 4 and core in self.blob:
                st = "fragment"
        return st


def not_text(t: str) -> str | None:
    s = t.strip(BLANKS)
    if not s:
        return "blank"
    if FILE_EXT.search(s) or ("\\" in s and " " not in s):
        return "file"
    if " " not in s and (re.search(r"[_@?$]", s) or re.search(r"[a-z][A-Z]", s) or (CONV.search(s) and not re.search(r"[a-z]{3}", s))):
        return "ident"
    if not TEXT_BYTES.fullmatch(t):
        return "binary"
    if not VOWEL_WORD.search(s) and not ORDINAL.search(s):
        return "binary"
    if len(s) <= 4 and not re.search(r"[a-z]{2}", s) and not ORDINAL.search(s):  # pointer bytes read as text ("PZI", "0UH")
        return "binary"
    return None


class Exclusions:
    def __init__(self, path: pathlib.Path | None):
        self.vas: dict[int, str] = {}
        self.ranges: list[tuple[int, int, str]] = []
        self.pools: dict[str, str] = {}
        self.res: list[tuple[re.Pattern, str]] = []
        self.errors: list[str] = []
        if not path or not path.exists():
            return
        for n, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
            if not line.strip() or line.startswith("#"):
                continue
            f = line.split("\t")
            try:
                if f[0] == "range":
                    lo, hi = (int(x, 16) for x in f[1].split("-"))
                    self.ranges.append((lo, hi, f[2]))
                elif f[0] == "pool":
                    self.pools[f[1]] = f[2]
                elif f[0] == "re":
                    self.res.append((re.compile(f[1], re.S), f[2]))
                elif f[0] == "va":
                    self.vas[int(f[1], 16)] = f[2]
                else:
                    self.vas[int(f[0], 16)] = f[1]
            except (IndexError, ValueError, re.error) as e:
                self.errors.append(f"exclusions line {n}: {e}")

    def category(self, va: int, pool: str, t: str) -> str | None:
        if va in self.vas:
            return self.vas[va]
        for lo, hi, c in self.ranges:
            if lo <= va <= hi:
                return c
        if pool in self.pools:
            return self.pools[pool]
        for rx, c in self.res:
            if rx.fullmatch(t):
                return c
        return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang-dir", default=str(ROOT / "lang"))
    ap.add_argument("--pack", default="")
    ap.add_argument("--strings", default="")
    ap.add_argument("--exclude", default="")
    # plain text sources, one text per line (va 0, pool = the file's name); default: the ones present of lang/source/interface.txt,
    # stories.txt (Themes stories; PARTNER / MYNAME are %s in the pack), credits_roles.txt (the role titles of credits.txt) and
    # golfer_files.txt (the thought slots of the Themes golfer files; DATA / PARTNER / MYNAME are %s)
    ap.add_argument("--extra", nargs="*", default=None)
    ap.add_argument("--out", default="")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    d = pathlib.Path(a.lang_dir)
    pack_p = pathlib.Path(a.pack) if a.pack else d / "simgolf_es.txt"
    str_p = pathlib.Path(a.strings) if a.strings else d / "source" / "exe_strings.tsv"
    exc_p = pathlib.Path(a.exclude) if a.exclude else d / "coverage_exclude.tsv"
    for p in (pack_p, str_p):
        if not p.exists():
            print(f"missing {p}")
            return 1
    pack = Pack(pack_p)
    exc = Exclusions(exc_p)
    for e in exc.errors:
        print(e)
    rows = []
    for line in str_p.read_text(encoding="utf-8").splitlines():
        if line.startswith("#"):
            continue
        f = line.split(chr(9))
        if len(f) >= 3:
            rows.append((f[0], f[1], f[2]))
    extras = a.extra if a.extra is not None else [str(d / "source" / n) for n in ("interface.txt", "stories.txt", "credits_roles.txt", "golfer_files.txt")
                                                  if (d / "source" / n).exists()]
    for e in extras:
        ep = pathlib.Path(e)
        raw = ep.read_bytes()
        try:
            text = raw.decode("utf-8-sig")
        except UnicodeDecodeError:
            text = raw.decode("cp1252")  # the stories as the game reads them
        for line in text.splitlines():
            if " | " in line:  # interface.txt: `<file>:<name> | <text>`
                line = line.split(" | ", 1)[1]
            if line.strip(BLANKS) and not line.startswith("#") and not line.strip(BLANKS).isdigit():
                rows.append(("0x0", ep.name, line.replace("\\", "\\\\")))
    cnt: collections.Counter = collections.Counter()
    excl: collections.Counter = collections.Counter()
    miss = []
    for f in rows:
        va, pool, t = int(f[0], 16), f[1], unescape(f[2])
        j = not_text(t)
        if j:
            cnt["not text: " + j] += 1
            continue
        st = pack.status(t)
        if st:
            cnt[st] += 1
            continue
        c = exc.category(va, pool, t)
        if c:
            excl[c] += 1
            continue
        cnt["UNCOVERED"] += 1
        miss.append((f[0], pool, f[2]))
    print("covered: " + ", ".join(f"{k} {v}" for k, v in sorted(cnt.items()) if k != "UNCOVERED"))
    print("excluded: " + (", ".join(f"{k} {v}" for k, v in sorted(excl.items())) or "none"))
    for va, pool, t in miss[: None if a.verbose else 40]:
        print(f"UNCOVERED {va}\t{pool}\t{t[:160]}")
    print(f"uncovered {len(miss)}")
    if a.out:
        with open(a.out, "w", encoding="utf-8", newline="\n") as fo:
            for va, pool, t in miss:
                fo.write(f"{va}\t{pool}\t{t}\n")
    return 1 if miss or exc.errors else 0


if __name__ == "__main__":
    sys.exit(main())
