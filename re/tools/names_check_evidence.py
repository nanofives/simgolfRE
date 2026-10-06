"""Check the mechanical evidence of re/names/*.tsv against the binaries: every `string 0x<addr>` must be referenced
by the named function (or, reported as weaker, by one of its direct callees), and every `import <Api>` must be
called through its IAT slot by the function or a direct callee. Uses the re/tools/xref.py index.

    py -3.12 re/tools/names_check_evidence.py [re/names/<id>.tsv ...]      (default: all non-globals TSVs)
Exit 1 when a citation is not found in the function or its callees."""
import glob, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import xref  # noqa: E402

STR = re.compile(r"\bstrings? (0x[0-9a-fA-F]+)")
IMP = re.compile(r"\bimports? ([A-Za-z_][\w@]*)")


def iat(mod):
    pe, _ = xref.load_pe(mod)
    pe.parse_data_directories()                    # load_pe is fast_load: imports are not parsed yet
    out = {}
    for e in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        for i in e.imports:
            if i.name:
                out.setdefault(i.name.decode(), set()).add(i.address)
    return out


def main():
    files = sys.argv[1:] or sorted(f for f in glob.glob("re/names/*.tsv") if not f.endswith("_globals.tsv"))
    idx, imps, bad, weak, checked = {}, {}, 0, 0, 0
    for f in files:
        lines = pathlib.Path(f).read_text(encoding="utf-8").splitlines()
        for line in lines[1:]:
            c = line.split("\t")
            if len(c) < 5:
                continue
            mod, va, name, ev = c[0], int(c[1], 16), c[2], c[4]
            if mod not in idx:
                idx[mod], imps[mod] = xref.load(mod), iat(mod)
            fn = idx[mod].get(va)
            if fn is None:
                continue                                   # thunk or not in the function list: nothing to check
            own = set(fn["strings"]) | set(fn["data"])
            sub = set()
            for t in fn["calls"]:
                g = idx[mod].get(t)
                if g:
                    sub |= set(g["strings"]) | set(g["data"])
            for s in STR.findall(ev):
                checked += 1
                a = int(s, 16)
                if a in own:
                    continue
                if a in sub:
                    weak += 1
                    print(f"WEAK  {f}: {c[1]} {name}: string {s} is referenced by a callee, not by the function")
                else:
                    bad += 1
                    print(f"BAD   {f}: {c[1]} {name}: string {s} not referenced by the function or its callees")
            for api in IMP.findall(ev):
                slots = imps[mod].get(api)
                if not slots:
                    continue                               # not an import of this module (e.g. a cited export)
                checked += 1
                if slots & own:
                    continue
                if slots & sub:
                    weak += 1
                    print(f"WEAK  {f}: {c[1]} {name}: import {api} is called by a callee, not by the function")
                else:
                    bad += 1
                    print(f"BAD   {f}: {c[1]} {name}: import {api} not called by the function or its callees")
    print(f"{checked} citations checked, {bad} bad, {weak} weak (callee only)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
