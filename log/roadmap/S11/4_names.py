"""S11.4 tracker part: rename the 70 rows of log/roadmap/S11/4_renames.tsv in <root>/re/names (the name column of the row at the
row's module + addr) and carry the function names into hooks.csv with `scripts/re_classify.py --root <root> retag` (levels and
callers unchanged, one CHANGELOG line per row). Globals (g_animalRecords, g_recordBanks) have no hooks.csv row.

    ~/bin/py -3.12 log/roadmap/S11/4_names.py --root <dir> [--dry-run]

Refuses a row whose names row holds neither the old nor the new name; skips (and says so) rows already renamed. Afterwards it runs
re/tools/names_validate.py on every touched names id (read-only check).
"""
from __future__ import annotations

import csv
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent


def main() -> None:
    a = sys.argv
    root = pathlib.Path(a[a.index("--root") + 1]).resolve()
    dry = "--dry-run" in a
    rows = list(csv.DictReader(open(HERE / "4_renames.tsv", encoding="utf-8"), delimiter="\t"))
    by_file: dict[str, list[dict]] = {}
    for r in rows:
        by_file.setdefault(r["names_file"], []).append(r)
    retag = []
    done = skipped = 0
    touched = set()
    for fname, rs in sorted(by_file.items()):
        p = root / "re" / "names" / fname
        raw = p.read_bytes().decode("utf-8")
        crlf = "\r\n" in raw
        lines = raw.replace("\r\n", "\n").split("\n")
        for r in rs:
            hit = [k for k, l in enumerate(lines) if l.split("\t")[:2] == [r["module"], r["addr"]]]
            assert len(hit) == 1, (fname, r["addr"], hit)
            f = lines[hit[0]].split("\t")
            if f[2] == r["new"]:
                skipped += 1
            else:
                assert f[2] == r["old"], (fname, r["addr"], f[2], r["old"])
                f[2] = r["new"]
                lines[hit[0]] = "\t".join(f)
                done += 1
                touched.add(fname)
            if r["kind"] == "func":
                retag.append({"module": r["module"], "addr": r["addr"], "name": r["new"], "subsystem": f[3]})
        if not dry and fname in touched:
            out = "\n".join(lines)
            p.write_bytes((out.replace("\n", "\r\n") if crlf else out).encode("utf-8"))
    print(f"re/names: {done} renamed, {skipped} already renamed, files {sorted(touched)}")
    if dry:
        return
    # the retag table is kept (log/roadmap/S11/4_retag.tsv): CHANGELOG lines cite it by this repo-relative path
    t = HERE / "4_retag.tsv"
    with open(t, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("module\taddr\tname\tsubsystem\n")
        for r in retag:
            fh.write(f"{r['module']}\t{r['addr']}\t{r['name']}\t{r['subsystem']}\n")
    subprocess.run([sys.executable, "scripts/re_classify.py", "--root", str(root), "retag", "log/roadmap/S11/4_retag.tsv"],
                   cwd=root, check=True)
    ids = sorted({f[:-4] for f in touched if not f.endswith("_globals.tsv")})
    r = subprocess.run([sys.executable, "re/tools/names_validate.py", *ids], cwd=root, capture_output=True, text=True)
    print("names_validate:", r.returncode, (r.stdout + r.stderr).strip().splitlines()[-3:])


if __name__ == "__main__":
    main()
