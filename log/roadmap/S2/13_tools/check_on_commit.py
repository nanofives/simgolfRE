"""S2.13: check 13_shared.patch against a commit (e.g. integrator I5's S2 waves 1-4 merge, once it is HEAD).

usage: ~/bin/py -3.12 log/roadmap/S2/13_tools/check_on_commit.py <commit> <scratch dir>
Writes the commit's copies of the patched files (git show, LF) into a scratch repository, runs `git apply --check` and `git apply`
of 13_shared.patch, and compares the result with the integration mirror port/build/S2_13_head (equal = the commit holds exactly
the patch base: HEAD 2c05f02a + waves 1-4).
"""
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[4]
PATCH = ROOT / "log/roadmap/S2/13_shared.patch"
MIRROR = ROOT / "port/build/S2_13_head"


def main():
    commit, scratch = sys.argv[1], pathlib.Path(sys.argv[2])
    names = [l[len("+++ b/"):].strip() for l in PATCH.read_text(encoding="utf-8").split("\n") if l.startswith("+++ b/")]
    if scratch.exists():
        shutil.rmtree(scratch)
    subprocess.run(["git", "init", "-q", str(scratch)], check=True)
    missing = []
    for r in names:
        p = subprocess.run(["git", "-C", str(ROOT), "show", f"{commit}:{r}"], capture_output=True)
        if p.returncode:
            missing.append(r)
            continue
        (scratch / r).parent.mkdir(parents=True, exist_ok=True)
        (scratch / r).write_bytes(p.stdout.replace(b"\r\n", b"\n"))
    print(f"commit {commit}: {len(names)} patched files, missing there: {missing or 'none'}")
    r = subprocess.run(["git", "-C", str(scratch), "apply", "--check", str(PATCH)], capture_output=True, text=True)
    print(f"git apply --check: rc {r.returncode} {r.stderr.strip()[:800]}")
    if r.returncode == 0:
        subprocess.run(["git", "-C", str(scratch), "apply", str(PATCH)], check=True)
        bad = [n for n in names if (scratch / n).read_bytes() != (MIRROR / n).read_bytes().replace(b"\r\n", b"\n")]
        print(f"applied: equal to the mirror: {'yes' if not bad else 'NO: ' + ', '.join(bad)}")
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
