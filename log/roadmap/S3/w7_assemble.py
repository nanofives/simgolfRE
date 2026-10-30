"""S3.9: the S3 wave 7 tree = `git archive 37e58e96` (wave 6 committed) + the junctions of 11_assemble.py + the wave 7 steps'
edits, each through its own assembler's --root <tree> --only form, in the lead's order:

    1. S3.17  17_assemble.py --only --no-docs --no-tick   (routing.cpp, gfx_surface.cpp, test_gfx_routing.py, test_gfx_s3_17.py)
    2. S3.17  tick.cpp item (17_merge.md section 3), applied here from 17_edits.py as its own pass
    3. S3.18  18_assemble.py --only --no-docs             (gfx_bulletin.cpp, then the dialogs.cpp pass; test_gfx_s3_18.py)
    4. S3.19  19_assemble.py --only --no-docs             (gfx_text.cpp, gfx.cpp, chareditor.cpp, gfx_families.inc, gfx_editbox.cpp)
    5. S3.16  16_assemble.py --only drawing                (tick_leaves.inc, shots.cpp, dialogs.cpp, tick_ball.inc, commands_panels.inc)
    6. S3.16  16_assemble.py --only snapshot               (port/test/gfx.py, framegate.py: checkpoint position)
    7. S3.15  15_assemble.py --only-15                     (15_edits.py + modules.inc line + 15_framegate_edits.py; gl_wgl.*)
    8. S3.9   9_edits.py (test_app_ingame.py: framegate's checkpoint position in checkpoint_plan); --no-s39 leaves it out

No CLAUDE.md / UNCERTAINTIES.md edit goes into the tree (they are in 9_merge_inputs.md as text). Every anchor must apply exactly
once (each assembler refuses otherwise and this script stops at the first failure).

    ~/bin/py -3.12 log/roadmap/S3/w7_assemble.py [--dst S3_9] [--commit 37e58e96] [--plain]
    ~/bin/py -3.12 log/roadmap/S3/w7_assemble.py --verify <dirA> <dirB>     # byte compare two assembled trees (port/, no build/)

--plain makes the archive tree only (the reference build port/build/S3_9_ref). The destination is always rebuilt from the archive
(its port/build subdirectory, the builds, is kept). Note: 16_assemble.py writes its files with Windows line ends (CRLF), the other
assemblers keep the file's own (LF from the archive); git's `text=auto` normalises both."""
import argparse
import hashlib
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
PY = sys.executable

import importlib.util  # noqa: E402

_spec = importlib.util.spec_from_file_location("s3_11_assemble", HERE / "11_assemble.py")
A = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(A)

TICK = "port/src/sim/tick.cpp"
# gitignored toolchains S10's re/match/smsg_* sources include (`// INCLUDE tools/dxsdk8/include`, `// TOOLCHAIN sp5`), not among
# 11_assemble.py's junctions: without them a git-archive tree compiles those sources against other headers (repo static's
# test_match then reads 94.7 / 98.8 / 99.0 % on three smsg_golf files)
EXTRA_JUNCTIONS = ["tools/dxsdk8", "tools/vc6sp5", "tools/vc6sp2"]


def extra_junctions(dst):
    for j in EXTRA_JUNCTIONS:
        link = dst / j
        if (A.ROOT / j).exists() and not link.exists():
            link.parent.mkdir(parents=True, exist_ok=True)
            subprocess.run(["cmd", "/c", "mklink", "/J", str(link), str(A.ROOT / j)], check=True, capture_output=True)
            print("junction", j)


def run(args):
    print("+", " ".join(str(a) for a in args), flush=True)
    subprocess.run([PY] + [str(a) for a in args], check=True)


def tick_pass(dst):
    ed = [e for e in A.env_of(HERE / "17_edits.py")["EDITS"] if A.portrel(e[0]) == TICK]
    assert len(ed) == 1, f"17_edits.py: {len(ed)} tick.cpp edits (expected 1)"
    R = A.Root(dst, False)
    A.edits(R, ed, "s317 tick.cpp (17_merge.md section 3)")
    R.flush()


def assemble(dst, commit, plain, s39=True):
    for j in EXTRA_JUNCTIONS:   # unlink first: make_tree removes the archive's tools/ directory with rmtree
        if os.path.isjunction(dst / j):
            os.rmdir(dst / j)
    A.make_tree(dst, commit, True)
    extra_junctions(dst)
    if plain:
        print("plain tree", dst)
        return
    r = ["--root", dst]
    run([HERE / "17_assemble.py", *r, "--only", "--no-docs", "--no-tick"])
    tick_pass(dst)
    run([HERE / "18_assemble.py", *r, "--only", "--no-docs"])
    run([HERE / "19_assemble.py", *r, "--only", "--no-docs"])
    run([HERE / "16_assemble.py", *r, "--only", "drawing"])
    run([HERE / "16_assemble.py", *r, "--only", "snapshot"])
    run([HERE / "15_assemble.py", *r, "--only-15"])
    if s39:   # 8. S3.9's fix-up of the interaction between S3.16's checkpoint position and S3.7's app test (9_edits.py)
        R = A.Root(dst, False)
        A.edits(R, A.env_of(HERE / "9_edits.py")["EDITS"], "s39 9_edits")
        R.flush()
    (dst / "port" / "src" / "registry.cpp").touch()   # MSBuild does not rebuild it when only an .inc changes
    print("done", dst)


def tree_hashes(d):
    out = {}
    for sub in ("port", "re/model/include"):
        base = d / sub
        for dp, dns, fns in os.walk(base):
            rel = pathlib.Path(dp).relative_to(d).as_posix()
            if sub == "port":
                dns[:] = [n for n in dns if not (rel == "port" and n in ("build", "corpus")) and n not in ("__pycache__", ".pytest_cache")]
            for f in fns:
                p = pathlib.Path(dp) / f
                out[p.relative_to(d).as_posix()] = hashlib.sha256(p.read_bytes()).hexdigest()
    return out


def verify(a, b):
    ha, hb = tree_hashes(a), tree_hashes(b)
    diff = sorted(k for k in set(ha) | set(hb) if ha.get(k) != hb.get(k))
    print(f"{len(ha)} / {len(hb)} files, {len(diff)} differ")
    for k in diff:
        print("DIFF", k)
    return not diff


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dst", default="S3_9")
    ap.add_argument("--commit", default="37e58e96")
    ap.add_argument("--plain", action="store_true")
    ap.add_argument("--no-s39", action="store_true", help="leave out S3.9's fix-up (9_edits.py): the steps' edits alone")
    ap.add_argument("--verify", nargs=2)
    ap.add_argument("--junctions-only", action="store_true", help="add EXTRA_JUNCTIONS to an existing --dst tree")
    a = ap.parse_args()
    if a.junctions_only:
        extra_junctions(ROOT / "port" / "build" / a.dst)
        return
    if a.verify:
        sys.exit(0 if verify(*(pathlib.Path(p).resolve() for p in a.verify)) else 1)
    assemble(ROOT / "port" / "build" / a.dst, a.commit, a.plain, not a.no_s39)


if __name__ == "__main__":
    main()
