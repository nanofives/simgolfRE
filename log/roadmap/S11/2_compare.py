"""S11.2: compare an ExportNames.java export of the Ghidra master with re/names rows.

usage: 2_compare.py <names_dir> <export_dir> <out.tsv>

Expected name per row, as ghidra/scripts/ApplyNames.java (overwrite) would leave it:
- function rows (files not *_globals.tsv / withdrawn.tsv): the function at addr named `Class::method` with the scope's
  `<` `>` `*` mapped to `(` `)` `P` and whitespace in the last part replaced by `_`; files are applied in sorted order,
  so for an address named twice the last file wins (reported as `dup_addr` when the names differ).
- withdrawn.tsv rows whose address no function file names: the default FUN_ name (source DEFAULT); rows whose reason
  starts `label <name>`: no label of that name at the address unless a globals row names it there.
- *_globals.tsv rows: the primary symbol at addr.
One output row per difference; category says which kind.
"""
import csv
import os
import re
import sys

MODS = ["golf_clean.exe", "jgld.dll", "Terrain.dll", "sound.dll"]


def ghidra_name(full):
    sep = full.rfind("::")
    if sep > 0:
        scope = full[:sep].replace("<", "(").replace(">", ")").replace("*", "P")
        name = full[sep + 2:]
    else:
        scope, name = "", full
    name = re.sub(r"\s", "_", name)
    return scope + "::" + name if scope else name


def label_name(s):
    return re.sub(r"\s", "_", s)


def load_export(d, mod):
    fns, prim, syms = {}, {}, {}
    with open(os.path.join(d, mod + ".tsv"), encoding="utf-8") as f:
        r = csv.reader(f, delimiter="\t", quoting=csv.QUOTE_NONE)
        next(r)
        for kind, addr, name, src, primary, st in r:
            a = int(addr, 16)
            if kind == "FN":
                fns[a] = (name, src)
            else:
                syms.setdefault(a, []).append((name, src, primary == "1"))
                if primary == "1":
                    prim[a] = (name, src)
    return fns, prim, syms


def main():
    names_dir, export_dir, out = sys.argv[1:4]
    files = sorted(f for f in os.listdir(names_dir) if f.endswith(".tsv"))
    fn_rows, gl_rows, wd_rows = [], [], []
    for f in files:
        with open(os.path.join(names_dir, f), encoding="utf-8") as h:
            lines = h.read().split("\n")
        for ln in lines[1:]:
            c = ln.rstrip("\r").split("\t")
            if len(c) < 3 or c[0] not in MODS or not c[2]:
                continue
            a = int(c[1].replace("0x", ""), 16)
            row = (c[0], a, c[2], f)
            if f == "withdrawn.tsv":
                wd_rows.append(row)
            elif f.endswith("_globals.tsv"):
                gl_rows.append(row)
            else:
                fn_rows.append(row)
    exp = {m: load_export(export_dir, m) for m in MODS}
    diffs = []
    counts = {"fn_rows": len(fn_rows), "globals_rows": len(gl_rows), "withdrawn_rows": len(wd_rows)}
    # function rows: last file wins
    last = {}
    for m, a, n, f in fn_rows:
        prev = last.get((m, a))
        if prev and ghidra_name(prev[0]) != ghidra_name(n):
            diffs.append((m, a, f, n, "", "", "dup_addr", "also named %s in %s (applied in file order, last wins)" % prev))
        last[(m, a)] = (n, f)
    for (m, a), (n, f) in sorted(last.items()):
        fns = exp[m][0]
        want = ghidra_name(n)
        if a not in fns:
            diffs.append((m, a, f, want, "", "", "fn_missing", "no function at the address"))
        elif fns[a][0] != want:
            diffs.append((m, a, f, want, fns[a][0], fns[a][1], "fn_name", ""))
    named = set(last)
    gnamed = {(m, a, label_name(n)) for m, a, n, f in gl_rows}
    for m, a, n, f in wd_rows:
        if n.startswith("label "):
            # `label <name> withdrawn ...`: that label must be gone unless a globals row names it again
            lab = label_name(n.split(" ")[1])
            if (m, a, lab) in gnamed:
                continue
            present = [s for s in exp[m][2].get(a, []) if s[0] == lab]
            if present:
                diffs.append((m, a, f, "(no label %s)" % lab, lab, present[0][1], "withdrawn_label_present",
                              "primary" if present[0][2] else "non-primary"))
            continue
        if (m, a) in named:
            continue
        fns = exp[m][0]
        if a in fns and fns[a][1] != "DEFAULT":
            diffs.append((m, a, f, "FUN_%08x" % a, fns[a][0], fns[a][1], "withdrawn_named", ""))
    glast = {}
    for m, a, n, f in gl_rows:
        prev = glast.get((m, a))
        if prev and label_name(prev[0]) != label_name(n):
            diffs.append((m, a, f, n, "", "", "dup_global", "also labelled %s in %s (last wins)" % prev))
        glast[(m, a)] = (n, f)
    for (m, a), (n, f) in sorted(glast.items()):
        fns, prim, syms = exp[m]
        want = label_name(n)
        if a in fns:
            cur = fns[a]
            diffs.append((m, a, f, want, cur[0], cur[1], "global_is_function", "primary symbol is a function"))
            continue
        cur = prim.get(a)
        if cur is None:
            diffs.append((m, a, f, want, "", "DEFAULT", "global_missing", "no non-default primary label"))
        elif cur[0] != want:
            others = ",".join(s[0] for s in syms.get(a, []) if not s[2])
            diffs.append((m, a, f, want, cur[0], cur[1], "global_name", ("other labels: " + others) if others else ""))
    counts["fn_addrs"] = len(last)
    counts["global_addrs"] = len(glast)
    with open(out, "w", encoding="utf-8", newline="") as h:
        w = csv.writer(h, delimiter="\t", lineterminator="\n")
        w.writerow(["module", "addr", "file", "expected", "master_name", "master_source", "category", "detail"])
        for m, a, f, want, cur, src, cat, det in diffs:
            w.writerow([m, "0x%08x" % a, f, want, cur, src, cat, det])
    by = {}
    for d in diffs:
        by[(d[0], d[6])] = by.get((d[0], d[6]), 0) + 1
    print("counts", counts)
    print("differences", len(diffs))
    for k in sorted(by):
        print("  %-15s %-20s %d" % (k[0], k[1], by[k]))


if __name__ == "__main__":
    main()
