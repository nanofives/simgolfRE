"""S10.10: print each non-ICF, non-forwarder row of 10_facts_exe.json with its names-file row (file + purpose).
~/bin/py -3.12 log/roadmap/S10/10_tools/rows.py > log/roadmap/S10/10_rows_purpose.txt"""
import glob, json, pathlib
R = pathlib.Path(__file__).resolve().parents[4]


def name_rows():
    rows = {}
    for f in glob.glob(str(R / "re/names/*.tsv")):
        if f.endswith("_globals.tsv"):
            continue
        for l in open(f, encoding="utf-8").read().splitlines()[1:]:
            c = l.split("\t")
            if len(c) == 6 and c[0] == "golf_clean.exe":
                rows[int(c[1], 16)] = (pathlib.Path(f).name, c)
    return rows


def demangled(v):
    return sorted({n for n in v["pdb_names"] if not n.startswith("?")})


if __name__ == "__main__":
    d = json.loads((R / "log/roadmap/S10/10_facts_exe.json").read_text(encoding="utf-8"))
    fw = {l.split("\t")[0] for l in (R / "log/roadmap/S10/10_fwd.tsv").read_text(encoding="utf-8").splitlines()[1:]}
    rows = name_rows()
    for k, v in d.items():
        dn = demangled(v)
        if len(dn) > 1 or k in fw:
            continue
        r = rows.get(int(k, 16))
        print(k, v["ours"], "=>", dn[0] if dn else "-", "|", r[0] if r else "NOROW")
        if r:
            print("   ", r[1][5][:330])
