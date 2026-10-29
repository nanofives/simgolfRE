"""Lead S10: rebuild log/roadmap/S10/files.txt from the step files lists plus lead files (paths only, existing)."""
import glob, os, pathlib

ROOT = pathlib.Path(__file__).resolve().parents[4]
os.chdir(ROOT)
S = "log/roadmap/S10/"
FIX = {"extent_score.py", "promote_cmds_sound.py", "stage_lines.py"}
out = set()
for k in (1, 2, 3, 4, 5, 6, 7, 8, 9, 10):
    p = pathlib.Path(f"{S}{k}_files.txt")
    if not p.is_file():
        continue
    for line in p.read_text(encoding="utf-8").splitlines():
        s = line.strip().split(" (")[0].rstrip("/").rstrip(":")
        if s in FIX:
            s = S + "6_tools/" + s
        if s and not s.startswith("log/diff") and os.path.exists(s):
            out.add(s)
extra = ["tests/test_match.py", "re/roadmap/S10_steps.tsv", "re/names/smsg_exe_1.tsv",
         S + "STATUS.md", S + "MERGE.md", S + "files.txt", S + "pdb", S + "runs",
         S + "6_tools", S + "7_lib", S + "7_tools", S + "lead_tools"]
out.update(e for e in extra if os.path.exists(e) or e.endswith("files.txt"))
out.update(glob.glob(S + "lead_*"))
out.update(p for p in glob.glob(S + "[0-9]*_*") if os.path.isfile(p))
out = sorted({p.replace(os.sep, "/") for p in out})
pathlib.Path(S + "files.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
print(len(out), "paths")
