"""38f private build: a snapshot of port/ + re/model/include at a git revision (default HEAD) or of the working tree, built
x64 (port/build/38f) and Win32 (port/build/38f_w32, with portsim_runner32.exe) outside the shared build trees.

    ~/bin/py -3.12 log/p38/f_mirror.py [--rev HEAD | --worktree | --merged] [--arch x64 Win32] [--mine]

--mine overlays this stream's own port files (MINE below: the integrator module), adds its SG_MODULE line to modules.inc
and applies log/p38/38f_tick.patch (the batch-override hook in tick.cpp TraceInput) to the snapshot, as the parent's
merge would (log/p38/38f_merge.md).

The DLLs (and the runner) are copied to port/build/38f_run/<arch>_<stamp>/ so a running test never locks the build tree;
the last copy's paths are written to port/build/38f_run/latest.json (read by port/test/chain.py's --build 38f).
"""
import argparse
import io
import json
import pathlib
import shutil
import subprocess
import sys
import tarfile
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
DST = ROOT / "port" / "build" / "38f_root"
RUN = ROOT / "port" / "build" / "38f_run"
MINE = ["port/include/portsim/sim/integrator.h", "port/src/sim/integrator.cpp"]
TICK_EDITS = [  # (anchor line, line added after it): the content of log/p38/38f_tick.patch
    ('#include "portsim/sim/commands.h"\n', '#include "portsim/sim/integrator.h"\n'),
    ("    out = b.cmds;\n",
     "    if (const std::vector<Command> *rep = batch_override(ctx_, b.pos)) out = *rep;  // phase 3.8 gate (d): a replacement\n"),
]
CMAKE = pathlib.Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions"
                     r"\Microsoft\CMake\CMake\bin\cmake.exe")


def snapshot_rev(rev: str):
    data = subprocess.run(["git", "archive", rev, "port", "re/model/include"], cwd=ROOT, capture_output=True,
                          check=True).stdout
    with tarfile.open(fileobj=io.BytesIO(data)) as tf:
        for m in tf.getmembers():
            if not m.isfile() or m.name.startswith(("port/build/", "port/corpus/")):
                continue
            d = DST / m.name
            b = tf.extractfile(m).read()
            d.parent.mkdir(parents=True, exist_ok=True)
            if not d.exists() or d.read_bytes() != b:
                d.write_bytes(b)


def snapshot_worktree(merged_only: bool = False):
    """The working tree's port/ + re/model/include. merged_only: module sources (port/src/sim/<m>.cpp) only for the modules
    modules.inc lists (the parent's merged state), so a sibling's unmerged, possibly half-written module stays out."""
    import re
    listed = set(re.findall(r"SG_MODULE\((\w+)\)", (ROOT / "port" / "src" / "sim" / "modules.inc").read_text()))
    for top in ("port", "re/model/include"):
        src = ROOT / top
        for p in src.rglob("*"):
            rel = p.relative_to(ROOT).as_posix()
            if not p.is_file() or rel.startswith(("port/build/", "port/corpus/")) or "__pycache__" in rel:
                continue
            if (merged_only and rel.startswith("port/src/sim/") and rel.endswith(".cpp")
                    and rel.count("/") == 3 and p.stem not in listed):
                continue
            d = DST / rel
            b = p.read_bytes()
            d.parent.mkdir(parents=True, exist_ok=True)
            if not d.exists() or d.read_bytes() != b:
                d.write_bytes(b)


def overlay_mine():
    for rel in MINE:
        d = DST / rel
        d.parent.mkdir(parents=True, exist_ok=True)
        d.write_bytes((ROOT / rel).read_bytes())
    inc = DST / "port" / "src" / "sim" / "modules.inc"
    text = inc.read_text()
    if "SG_MODULE(integrator)" not in text:
        inc.write_text(text.rstrip("\n") + "\nSG_MODULE(integrator)\n")
    # log/p38/38f_tick.patch as text edits (git apply inside the repository's ignored build folder applies nothing): each
    # anchor must occur once; a snapshot that already holds the added line (a merged tree) is left as it is
    tick = DST / "port" / "src" / "sim" / "tick.cpp"
    s = tick.read_text(encoding="utf-8")
    for anchor, added in TICK_EDITS:
        if added in s:
            continue
        if s.count(anchor) != 1:
            raise SystemExit(f"tick.cpp: anchor {anchor!r} occurs {s.count(anchor)} times")
        s = s.replace(anchor, anchor + added)
    tick.write_text(s, encoding="utf-8")


def build(arch: str) -> dict:
    bd = ROOT / "port" / "build" / ("38f" if arch == "x64" else "38f_w32")
    for argv in ([str(CMAKE), "-S", str(DST / "port"), "-B", str(bd), "-A", arch, "-DPORTSIM_NEGCTL=ON"],
                 [str(CMAKE), "--build", str(bd), "--config", "Release", "--parallel", "3"]):
        r = subprocess.run(argv, capture_output=True, text=True, errors="replace")
        if r.returncode:
            sys.stdout.write(r.stdout[-6000:] + r.stderr[-2000:])
            raise SystemExit(f"{arch}: {' '.join(argv[:2])} failed")
    out = RUN / f"{arch}_{time.strftime('%Y%m%d_%H%M%S')}"
    out.mkdir(parents=True, exist_ok=True)
    res = {}
    for name in ("portsim_c.dll", "portsim_runner32.exe"):
        s = bd / "Release" / name
        if s.exists():
            shutil.copy2(s, out / name)
            res[name] = str(out / name)
    return res


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--rev", default="HEAD")
    g.add_argument("--worktree", action="store_true")
    g.add_argument("--merged", action="store_true", help="the working tree with only the modules modules.inc lists")
    ap.add_argument("--arch", nargs="+", default=["x64", "Win32"])
    ap.add_argument("--mine", action="store_true")
    a = ap.parse_args()
    if DST.exists():
        shutil.rmtree(DST)
    if a.worktree or a.merged:
        snapshot_worktree(merged_only=a.merged)
        src = "merged-worktree" if a.merged else "worktree"
    else:
        snapshot_rev(a.rev)
        src = subprocess.run(["git", "rev-parse", a.rev], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    if a.mine:
        overlay_mine()
        src += "+38f"
    latest = json.loads((RUN / "latest.json").read_text()) if (RUN / "latest.json").exists() else {}
    for arch in a.arch:
        latest[arch] = {"source": src, "built": time.strftime("%Y-%m-%dT%H:%M:%S"), **build(arch)}
        print(arch, json.dumps(latest[arch]))
    RUN.mkdir(parents=True, exist_ok=True)
    (RUN / "latest.json").write_text(json.dumps(latest, indent=1))


if __name__ == "__main__":
    main()
