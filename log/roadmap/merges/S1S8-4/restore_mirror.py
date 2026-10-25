"""I4: undo apply_shared.py in the mirror (to re-apply after a script fix): the six files of S1's diff and the base shared files
from HEAD (git archive, as mirror.py wrote them), then the section files again (mirror.py --recopy) and sync_base.py.
Usage: ~/bin/py -3.12 log/roadmap/merges/S1S8-4/restore_mirror.py"""
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
M = ROOT / "port" / "build" / "I4_S1S8"
src = (HERE / "sync_base.py").read_text().replace("\r\n", "\n")
ns = {"pathlib": pathlib, "__file__": str(HERE / "sync_base.py")}
exec(src.split("\n\ndef norm", 1)[0].split("import sys", 1)[1], ns)   # the BASE / SIX lists only
FILES = ns["SIX"] + ns["BASE"]
head = (M / "HEAD_COMMIT").read_text().strip()
tar = subprocess.run(["git", "archive", head] + FILES, cwd=ROOT, capture_output=True, check=True).stdout
subprocess.run(["tar", "-xf", "-"], cwd=M, input=tar, check=True)
print("restored from", head, len(FILES), "files")
subprocess.run([sys.executable, str(HERE / "mirror.py"), "--recopy"], check=True)
subprocess.run([sys.executable, str(HERE / "sync_base.py")], check=True)
