"""S3.12: turn wave 6 (S3.10, S3.10b, the LiveTerrain log switch, the app-test facts parameter) into anchored exact-text edits on
e179bd24 (12_edits.py), with 13_make_edits.py's method (hunks grown to unique anchors, merged when they overlap, checked by applying
every edit to the base text and comparing with the target).

Base text: the blob at BASE_COMMIT (LF). Targets (line ends normalised to LF):
  S3.10's files (its tree port/build/S3_10_head; HEAD == bbc9072f for them): gl_raster.cpp, gl_raster.h, test_gl_raster.py, taken
    from 12_src/ = S3.10's copies plus the worker negative control `#workerctl` (GlRaster::worker_ctl, 12_notes.md section 3).
    gfx_terrain.cpp: 12_src/gfx_terrain.cpp = S3.10's change merged onto S3.8's file (`git merge-file`, 0 conflicts; it is the
    file 01d84188 had committed) + the comment of the two new controls. e179bd24 holds I13's file without S3.10's lines, so the
    edits bring S3.10's thread calls (12_notes.md section 1).
  S3.10b's files (port/build/S3_10b_head): terrain_state.cpp, test_terrain_state.py.
  12_src/: terrain_live.cpp / .h (10b_live's lazy log + set_keep_log / calls() + controls 6 / 7), host.cpp / host.h (S7.9's host
    with TerrainLogBound over log_size / drop_log / set_keep_log / calls), test_app_ingame.py (SG_APP_FACTS_DIR, the stale zoom xfail dropped),
    test_app_live.py / test_app_present.py (SG_APP_FACTS_DIR).

    ~/bin/py -3.12 log/roadmap/S3/12_make_edits.py            # writes log/roadmap/S3/12_edits.py
"""
import difflib
import pathlib
import pprint
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[3]
HERE = ROOT / "log" / "roadmap" / "S3"
BASE_COMMIT = "e179bd24"   # was 01d84188 until the orchestrator's fix commit (12_notes.md section 1)
OUT = HERE / "12_edits.py"
S310 = ROOT / "port" / "build" / "S3_10_head"
S310B = ROOT / "port" / "build" / "S3_10b_head"
SRC = HERE / "12_src"
TARGETS = {
    "port/src/sim/gl_raster.cpp": SRC / "gl_raster.cpp",
    "port/include/portsim/sim/gl_raster.h": SRC / "gl_raster.h",
    "port/test/test_gl_raster.py": SRC / "test_gl_raster.py",
    "port/src/sim/gfx_terrain.cpp": SRC / "gfx_terrain.cpp",
    "port/src/sim/terrain_state.cpp": S310B / "port/src/sim/terrain_state.cpp",
    "port/test/test_terrain_state.py": S310B / "port/test/test_terrain_state.py",
    "port/src/sim/terrain_live.cpp": SRC / "terrain_live.cpp",
    "port/include/portsim/sim/terrain_live.h": SRC / "terrain_live.h",
    "port/app/host.cpp": SRC / "host.cpp",
    "port/app/host.h": SRC / "host.h",
    "port/test/test_app_ingame.py": SRC / "test_app_ingame.py",
    "port/test/test_app_live.py": SRC / "test_app_live.py",
    "port/test/test_app_present.py": SRC / "test_app_present.py",
}


def lf(b: bytes) -> str:
    return b.decode("utf-8").replace("\r\n", "\n")


def base_text(rel: str) -> str:
    return lf(subprocess.run(["git", "show", f"{BASE_COMMIT}:{rel}"], cwd=ROOT, capture_output=True, check=True).stdout)


def hunks(a, b):
    """[(i1, i2, j1, j2)] line ranges that differ, grown to unique anchors. A hunk grows only through the equal lines between it
    and its neighbours (so a[lo - 1] == b[jlo - 1] holds for every step); one that reaches a neighbour merges with it.
    (13_make_edits.py's version could grow into the previous hunk's changed lines and drop them when merging.)"""
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    hs = [[i1, i2, j1, j2] for tag, i1, i2, j1, j2 in sm.get_opcodes() if tag != "equal"]
    text = "".join(a)
    i = 0
    while i < len(hs):
        lo, hi, jlo, jhi = hs[i]
        old = "".join(a[lo:hi])
        if old and text.count(old) == 1:
            i += 1
            continue
        floor = hs[i - 1][1] if i > 0 else 0
        ceil = hs[i + 1][0] if i + 1 < len(hs) else len(a)
        if lo == floor and i > 0:              # touches the previous hunk: merge into it and re-check it
            p = hs[i - 1]
            hs[i - 1:i + 1] = [[p[0], hi, p[2], jhi]]
            i -= 1
            continue
        if hi == ceil and i + 1 < len(hs):     # touches the next hunk: merge with it
            n = hs[i + 1]
            hs[i:i + 2] = [[lo, n[1], jlo, n[3]]]
            continue
        assert lo > floor or hi < ceil, "no unique anchor"
        if lo > floor:
            lo, jlo = lo - 1, jlo - 1
        if hi < ceil:
            hi, jhi = hi + 1, jhi + 1
        hs[i] = [lo, hi, jlo, jhi]
    return [tuple(h) for h in hs]


def main():
    edits = []
    for rel, src in TARGETS.items():
        a = base_text(rel).splitlines(keepends=True)
        b = lf(src.read_bytes()).splitlines(keepends=True)
        hs = hunks(a, b)
        # applied in order, an earlier edit's new text can hold a later edit's anchor (a moved block): merge such a hunk into
        # the one before it until every anchor occurs exactly once in the text it is applied to
        while True:
            t, bad = "".join(a), None
            for i, (lo, hi, jlo, jhi) in enumerate(hs):
                old = "".join(a[lo:hi])
                if t.count(old) != 1:
                    bad = i
                    break
                t = t.replace(old, "".join(b[jlo:jhi]))
            if bad is None:
                break
            assert bad > 0, (rel, bad)
            p = hs[bad - 1]
            hs[bad - 1:bad + 1] = [(p[0], hs[bad][1], p[2], hs[bad][3])]
        for lo, hi, jlo, jhi in hs:
            edits.append((rel, "".join(a[lo:hi]), "".join(b[jlo:jhi])))
    # check: every old text exactly once, in order, gives the target; and no old text is already the new one
    for rel, src in TARGETS.items():
        t = base_text(rel)
        for r, old, new in edits:
            if r != rel:
                continue
            assert t.count(old) == 1, (rel, old[:80])
            t = t.replace(old, new)
        assert t == lf(src.read_bytes()), rel
    body = ('"""S3.12 edits (generated by log/roadmap/S3/12_make_edits.py: e179bd24 -> wave 6; applied by 12_assemble.py).\n'
            '(path, old, new): each old text exactly once, in order."""\n'
            "EDITS = " + pprint.pformat(edits, width=130) + "\n")
    OUT.write_text(body, encoding="utf-8")
    per = {}
    for r, _, _ in edits:
        per[r] = per.get(r, 0) + 1
    for r in TARGETS:
        print(f"{per.get(r, 0):3d} {r}")
    print("edits", len(edits), "->", OUT)


if __name__ == "__main__":
    main()
