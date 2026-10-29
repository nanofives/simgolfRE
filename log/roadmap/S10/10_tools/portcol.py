"""S10.10: port column for the accepted renames of 10_verdict_exe.tsv: P (n port / m shim+frida files) when the old name
occurs in port/src or port/test, or in shim/src or re/frida (whole-word match), else -.
~/bin/py -3.12 log/roadmap/S10/10_tools/portcol.py  -> log/roadmap/S10/10_renames.tsv"""
import pathlib, re
R = pathlib.Path(__file__).resolve().parents[4]
S = R / "log/roadmap/S10"
EXT = {".cpp", ".h", ".inc", ".py", ".js", ".json", ".tsv", ".txt", ".md", ".c"}


def texts(dirs):
    out = []
    for d in dirs:
        for p in (R / d).rglob("*"):
            if p.is_file() and p.suffix in EXT and p.stat().st_size < 30_000_000:
                try:
                    out.append((p, p.read_text(encoding="utf-8", errors="ignore")))
                except OSError:
                    pass
    return out


PORT = texts(["port/src", "port/test"])
SHIM = texts(["shim/src", "re/frida"])


def hits(name, corpus):
    rx = re.compile(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"(?![A-Za-z0-9_])")
    return sum(1 for _, t in corpus if rx.search(t))


lines = ["file\taddr\told\tnew\tport\trule\tevidence"]
for l in (S / "10_verdict_exe.tsv").read_text(encoding="utf-8").splitlines()[1:]:
    c = l.split("\t")
    if not c[4].startswith("ACCEPT"):
        continue
    p, s = hits(c[2], PORT), hits(c[2], SHIM)
    col = f"P ({p} port / {s} shim+frida files)" if p or s else "-"
    lines.append("\t".join([c[1], "0x%08x" % int(c[0], 16), c[2], c[5], col, c[4], c[6]]))
(S / "10_renames.tsv").write_text("\n".join(lines) + "\n", encoding="utf-8")
print(len(lines) - 1, "renames")
