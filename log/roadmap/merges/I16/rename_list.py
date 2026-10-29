"""I16: collect the `-` rename rows (D1 option c) from 8_merge.md 2c, 9_renames_sound.tsv, 10_renames.tsv, check each old
name has no whole-word reference in port/src, port/test, shim/src, re/frida, and write log/roadmap/merges/I16/renames.tsv
(file, addr, old, new, source).   ~/bin/py -3.12 log/roadmap/merges/I16/rename_list.py"""
import pathlib, re
R = pathlib.Path(__file__).resolve().parents[4]
S = R / "log/roadmap/S10"
rows = []
for l in (S / "10_renames.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = l.split("\t")
    if c[4] == "-":
        rows.append((c[0], c[1], c[2], c[3], "10_renames.tsv"))
for l in (S / "9_renames_sound.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = l.split("\t")
    rows.append((c[0], c[1], c[2], c[3], "9_renames_sound.tsv"))
sec = (S / "8_merge.md").read_text(encoding="utf-8").split("### 2c.")[1].split("## 3.")[0]
for l in sec.splitlines():
    m = re.match(r"\| (0x[0-9a-f]{8}) \| (\S+) \| (\S+) \|", l)
    if m:
        rows.append(("model_h3.tsv", m.group(1), m.group(2), m.group(3), "8_merge.md 2c"))

EXT = {".cpp", ".h", ".inc", ".py", ".js", ".json", ".tsv", ".txt", ".md", ".c"}
corpus = []
for d in ["port/src", "port/test", "shim/src", "re/frida"]:
    for p in (R / d).rglob("*"):
        if p.is_file() and p.suffix in EXT and p.stat().st_size < 30_000_000:
            corpus.append((p, p.read_text(encoding="utf-8", errors="ignore")))
out = ["file\taddr\told\tnew\tsource"]
for f, a, o, n, src in rows:
    rx = re.compile(r"(?<![A-Za-z0-9_])" + re.escape(o) + r"(?![A-Za-z0-9_])")
    hit = [str(p.relative_to(R)) for p, t in corpus if rx.search(t)]
    if hit:
        print("PORT-REFERENCED, skipped:", a, o, hit[:3])
        continue
    out.append("\t".join([f, a, o, n, src]))
(R / "log/roadmap/merges/I16/renames.tsv").write_text("\n".join(out) + "\n", encoding="utf-8")
print(len(out) - 1, "renames of", len(rows))
