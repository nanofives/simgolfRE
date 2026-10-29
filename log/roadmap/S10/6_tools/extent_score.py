"""S10.6: score the sound.dll procedures that read `no Ghidra size` with their PDB extent as a PROCESS-LOCAL override of
match.boundary_overrides (no edit of re/functions_ghidra*.tsv or re/match/boundaries.tsv, no evidence CSV).
~/bin/py -3.12 log/roadmap/S10/6_tools/extent_score.py --out log/roadmap/S10/6_extent.tsv
Per address: PDB size, our disassembly check of the extent (last instruction, next listed function start, whether the entry
lies inside a listed function) and the score; then the lines the orchestrator would apply (6_merge.md)."""
import argparse
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import capstone  # noqa: E402
import match  # noqa: E402
import smsg_import  # noqa: E402

MOD = "sound.dll"
RUNS = ROOT / "log" / "roadmap" / "S10" / "runs"
# PDB size includes the switch's jump and index tables after the last `ret` (found by the first run, 6_extent_first.tsv):
# the code extent ends at the last `ret`, as Ghidra ends its bodies (tables are data).
CODE_ONLY = {0x100250b0: 0x56, 0x100392c0: 0x53, 0x1002e2f0: 0x62}
SCRATCH = pathlib.Path(sys.argv[0]).resolve().parent / "scratch"


def error_rows():
    out = {}
    for f in sorted(RUNS.glob("sound_*.tsv")):
        for r in csv.DictReader(f.open(encoding="utf-8"), delimiter="\t"):
            if r["status"] == "error" and "no Ghidra size" in r["note"]:
                out.setdefault(r["source"], []).append(r)
    return out


def listed():
    """[(rva, size)] of our sound.dll function list, sorted."""
    rows = [l.split("\t") for l in (ROOT / "re" / match.TSV[MOD]).read_text().splitlines()[1:]]
    return sorted((int(c[0], 16), int(c[2])) for c in rows)


def check_extent(rva, size, funcs):
    """(last instruction text, next listed entry - rva or '', containing listed function or '')."""
    code = match.original_bytes(MOD, 0x10000000 + rva, size)
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    ins = list(md.disasm(code, rva))
    last = f"{ins[-1].mnemonic} {ins[-1].op_str}".strip() if ins else "?"
    end = ins[-1].address + ins[-1].size if ins else rva
    nxt = next((e for e, _ in funcs if e > rva), None)
    cont = next((f"0x{e:x}+{s}" for e, s in funcs if e < rva < e + s), "")
    return last, (f"{nxt - rva}" if nxt else ""), cont, end - rva == size


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--only", default="", help="substring of the source path (smoke runs)")
    a = ap.parse_args()
    SCRATCH.mkdir(exist_ok=True)
    funcs, ext = listed(), {}
    errs = error_rows()
    for src, rows in errs.items():
        for r in rows:
            ext[match.tracker_key(MOD, int(r["va"], 16))] = CODE_ONLY.get(int(r["va"], 16), int(r["size"]))
    orig = match.boundary_overrides
    match.boundary_overrides = lambda m: {**orig(m), **(ext if m == MOD else {})}   # process-local only
    out = []
    for src, rows in sorted(errs.items()):
        if a.only not in src:
            continue
        dsp = smsg_import.project_of(src)
        wip = SCRATCH / f"x_{smsg_import.stem_of(dsp, src)}.cpp"
        wip.write_text(smsg_import.header(dsp, src, 3))
        scored = {(x["va"], x["name"]): x for x in smsg_import.score_rows(src, dsp, wip)}
        wip.write_text("".join(l for l in smsg_import.header(dsp, src, 3).splitlines(True) if not l.startswith("// TOOLCHAIN")))
        old = {(x["va"], x["name"]): x for x in smsg_import.score_rows(src, dsp, wip)}      # same source, default 8168
        for r in rows:
            s = scored.get((int(r["va"], 16), r["pdb_name"]), {})
            rva = int(r["rva"], 16)
            esize = CODE_ONLY.get(int(r["va"], 16), int(r["size"]))
            last, gap, cont, exact = check_extent(rva, esize, funcs)
            out.append([src, r["va"], r["size"], esize, r["pdb_name"], s.get("symbol", ""), s.get("status", "?"),
                        s.get("score", ""), old.get((int(r["va"], 16), r["pdb_name"]), {}).get("score", "?"), s.get("note", "")[:80], last, gap, cont, "yes" if exact else "no"])
        wip.unlink()
    with open(a.out, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["source", "va", "pdb_size", "extent", "pdb_name", "symbol", "status", "score_sp5", "score_8168", "note", "last_ins", "next_entry_gap",
                    "inside_listed", "decodes_exact"])
        w.writerows(out)
    print(len(out), "rows", sum(1 for o in out if o[6] == "100"), "at 100%")


if __name__ == "__main__":
    main()
