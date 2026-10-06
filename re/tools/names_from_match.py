"""Names that come straight from matched sources: the decorated symbol of every `// MATCH:` line in the given
re/match files, undecorated (MSVC C++: ?name@Class@@..., ??0/??1 constructors and destructors; C: _name).
Placeholder names chosen while matching (FUN_, f_<addr> namespaces, C<addr>/T<addr> structs, draw16_<addr>...)
are skipped: those functions are named by evidence, not by their matched source.

    py -3.12 re/tools/names_from_match.py --module Terrain.dll --subsystem terrain re/match/terrain*.cpp > re/names/terrain.tsv"""
import argparse, pathlib, re, sys

OPS = {"0": "{ctor}", "1": "~", "4": "operator=", "8": "operator==", "9": "operator!=", "A": "operator[]",
       "D": "operator*", "G": "operator-", "H": "operator+", "X": "operator*=", "Y": "operator+=", "Z": "operator-=",
       "_G": "{scalar deleting dtor}", "_E": "{vector deleting dtor}", "_D": "{vbase dtor}", "E": "operator++",
       "2": "operator new"}
PLACEHOLDER = re.compile(r"(^|::)(FUN_|f_[0-9a-f]{8}|C_?FUN|[A-Z]\d?_?[0-9a-f]{5,8}|[A-Za-z]+_[0-9a-f]{6,8}$|\$E\d)", re.I)


# the only templates in the matched sources: Terrain.dll's std::list<Tile*> (VC6 STL)
TEMPLATES = [("?$list@PAVTile@@V?$allocator@PAVTile@@@std@@@std@@", "list<Tile*>@std@@"),
             ("?$allocator@PAVTile@@@std@@", "allocator<Tile*>@std@@")]
ID = r"[\w<>*]+"


def undecorate(sym: str) -> str | None:
    if sym.startswith("_") and not sym.startswith("__"):
        return sym[1:]                                    # C function (LANG c): _name
    if sym == "??2@YAPAXIPAX@Z":
        return "operator new"                             # placement new
    rb = re.match(r"\?\?0\?\$reverse_bidirectional_iterator@V(const_iterator|iterator)@", sym)
    if rb:
        return f"std::reverse_bidirectional_iterator<std::list<Tile*>::{rb.group(1)}>::ctor"
    for blob, short in TEMPLATES:
        sym = sym.replace(blob, short)
    m = re.match(rf"\?\?(_[GED]|[0-9A-Z])({ID})@((?:{ID}@)*)@", sym)
    if m:                                                 # special member: ??0Class@@ ctor, ??1 dtor, operators
        op, cls = m.group(1), m.group(2)
        outer = [p for p in m.group(3).split("@") if p][::-1]
        scope = "::".join(outer + [cls])
        base = cls.split("<")[0]
        if op == "0":
            return f"{scope}::{base}"
        if op == "1":
            return f"{scope}::~{base}"
        return f"{scope}::{OPS.get(op, 'operator' + op)}"
    m = re.match(rf"\?({ID})@((?:{ID}@)*)@", sym)
    if m:
        parts = [p for p in m.group(2).split("@") if p][::-1]
        return "::".join(parts + [m.group(1)])
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--module", required=True)
    ap.add_argument("--subsystem", required=True)
    ap.add_argument("--evidence", default="", help="extra evidence text appended to `match re/match/<file>`")
    a = ap.parse_args()
    seen = {}
    print("module\taddr\tname\tsubsystem\tevidence\tpurpose")
    for f in a.files:
        text = pathlib.Path(f).read_text(errors="replace")
        for mod, addr, sym in re.findall(r"//\s*MATCH:\s*(\S+)\s+(0x[0-9a-fA-F]+)\s+(\S+)", text):
            if mod != a.module:
                continue
            name = undecorate(sym)
            if not name or PLACEHOLDER.search(name) or int(addr, 16) in seen:
                continue
            if name in seen.values():
                name = f"{name}_{int(addr, 16):x}"         # overloads / identical folds: keep names unique
            seen[int(addr, 16)] = name
            ev = f"match re/match/{pathlib.Path(f).name}" + (f"; {a.evidence}" if a.evidence else "")
            print(f"{mod}\t0x{int(addr, 16):08x}\t{name}\t{a.subsystem}\t{ev}\tsymbol of the 100% matched source")
    print(f"{len(seen)} names", file=sys.stderr)


if __name__ == "__main__":
    main()
