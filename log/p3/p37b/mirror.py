"""3.7b private build: mirror port/ into port/build/m37b_src without the sibling's half-written module files (tick*),
so a sibling's mid-edit never stops this build. Usage: mirror.py [exclude-glob ...] (default tick*)"""
import fnmatch, pathlib, shutil, subprocess, sys
# every tracked file a sibling has modified is built at its committed version (HEAD + this child's own new files):
# a consistent baseline while the sibling's work is in flight
HEAD_ONLY = {l[len("port/"):] for l in subprocess.run(["git", "diff", "--name-only", "HEAD", "--", "port/"],
             cwd=pathlib.Path(__file__).resolve().parents[3], capture_output=True, text=True, check=True).stdout.split()}
HEAD_ONLY.discard("src/sim/modules.inc")
ROOT = pathlib.Path(__file__).resolve().parents[3]
SRC, DST = ROOT / "port", ROOT / "port" / "build" / "m37b_root" / "port"
MODEL = ROOT / "re" / "model" / "include"
excl = sys.argv[1:] or ["tick*"]
skipmods = {pathlib.Path(e).stem.rstrip("*") for e in excl}
seen = set()
for p in SRC.rglob("*"):
    rel = p.relative_to(SRC)
    if rel.parts[0] in ("build", "corpus", "__pycache__") or "__pycache__" in rel.parts or p.is_dir():
        continue
    if rel.parts[:2] == ("src", "sim") and any(fnmatch.fnmatch(p.name, e) for e in excl):
        continue
    d = DST / rel
    seen.add(d)
    d.parent.mkdir(parents=True, exist_ok=True)
    if rel.as_posix() == "src/sim/modules.inc":
        head = subprocess.run(["git", "show", "HEAD:port/src/sim/modules.inc"], cwd=ROOT, capture_output=True, text=True,
                              check=True).stdout.splitlines()
        lines = head + ["SG_MODULE(commands)"]
        new = "\n".join(lines) + "\n"
        if not d.exists() or d.read_text() != new:
            d.write_text(new)
        continue
    data = p.read_bytes()
    if rel.as_posix() in HEAD_ONLY:   # a sibling's uncommitted edit of a shared file: build the committed one
        data = subprocess.run(["git", "show", "HEAD:port/" + rel.as_posix()], cwd=ROOT, capture_output=True, check=True).stdout
    if not d.exists() or d.read_bytes() != data:
        d.write_bytes(data)
for p in MODEL.rglob("*"):
    if p.is_file():
        d = DST.parent / "re" / "model" / "include" / p.relative_to(MODEL)
        d.parent.mkdir(parents=True, exist_ok=True)
        if not d.exists() or d.read_bytes() != p.read_bytes():
            shutil.copy2(p, d)
for d in list(DST.rglob("*")):
    if d.is_file() and d not in seen:
        d.unlink()
