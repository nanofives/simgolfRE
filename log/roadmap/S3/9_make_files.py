"""S3.9: the union file list of wave 7 (9_files.txt) from 15..19_files.txt + S3.9's own files.
Mirror paths (port/build/S3_1x/<rel>) become <rel>; a directory entry is expanded to its files; 17_fg/champ_tick_trace.txt
(200 MB) is left out.  ~/bin/py -3.12 log/roadmap/S3/9_make_files.py"""
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
EXCLUDE = {"log/roadmap/S3/17_fg/champ_tick_trace.txt"}
OWN = ["log/roadmap/S3/w7_assemble.py", "log/roadmap/S3/9_gates.sh", "log/roadmap/S3/9_make_files.py",
       "log/roadmap/S3/9_census_cmp.py", "log/roadmap/S3/9_compare.py", "log/roadmap/S3/9_edits.py", "log/roadmap/S3/9_make_merge_inputs.py",
       "log/roadmap/S3/9_merge_inputs.tpl.md", "log/roadmap/S3/9_notes.md", "log/roadmap/S3/9_merge_inputs.md",
       "log/roadmap/S3/9_files.txt", "log/roadmap/S3/9_uncertain.tsv", "log/roadmap/S3/9_assemble.log",
       "log/roadmap/S3/9_assemble_chk.log", "log/roadmap/S3/9_assemble_verify.log", "log/roadmap/S3/9_assemble_ref.log",
       "log/roadmap/S3/9_assemble_chk2.log", "log/roadmap/S3/9_assemble_verify2.log", "log/roadmap/S3/9_assemble_s39_inplace.log",
       "log/roadmap/S3/9_assemble_chk3.log", "log/roadmap/S3/9_assemble_verify3.log", "log/roadmap/S3/9_census_cmp.log",
       "port/test/test_app_ingame.py", "port/test/test_gfx_credits.py", "log/roadmap/S3/9_out/", "re/roadmap/S3_census.tsv"]

out = set()
for k in range(15, 20):
    for line in (HERE / f"{k}_files.txt").read_text(encoding="utf-8").splitlines():
        p = line.strip().split("  ")[0].split("\t")[0].strip()
        if not p or p.startswith("#"):
            continue
        p = re.sub(r"^port/build/S3_1[5-9][a-z_]*/", "", p)
        out.add(p)
out.update(OWN)
files = set()
for p in out:
    q = ROOT / p
    if p.endswith("/") or q.is_dir():
        if q.is_dir():
            files.update(f.relative_to(ROOT).as_posix() for f in q.rglob("*") if f.is_file())
        else:
            files.add(p)   # listed, not on disk yet (written at the end of the step)
    else:
        files.add(p)
files -= EXCLUDE
(HERE / "9_files.txt").write_text("\n".join(sorted(files)) + "\n", encoding="utf-8", newline="\n")
print(len(files), "files ->", HERE / "9_files.txt")
