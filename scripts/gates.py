r"""One runner for every regression gate of the port and the RE tooling (replaces the per-merge gates.sh / queue_gates.sh /
suite_by_file.sh copies under log/roadmap/merges/<I>/ and log/p38/).

    ~/bin/py -3.12 scripts/gates.py --list
    ~/bin/py -3.12 scripts/gates.py --tier fast                       # G30's fast gates (as I25 ran them)
    ~/bin/py -3.12 scripts/gates.py --tier full --build
    ~/bin/py -3.12 scripts/gates.py segs_x64 boot_x64 t_test_tick     # named gates
    ~/bin/py -3.12 scripts/gates.py port_suite                        # every port/test file, one process each
    ~/bin/py -3.12 scripts/gates.py --tier fast --tree port/build/I26 --compare log/gates/<earlier run>

Options:
  --tree <dir>     run against a mirror (a copy of the repo, e.g. port/build/I26): cwd = the tree, and the DLL / runner /
                   app env vars point at its port/build/{x64,Win32} (as log/roadmap/merges/I25/env.sh did). Default: this repo,
                   whose libraries the harness finds by itself.
  --build          build x64 and Win32 (port/test/build.py) in the tree first; a failed build stops the run.
  --out <dir>      default log/gates/<YYYYmmdd-HHMMSS>[-<label>]. Each gate writes <out>/<gate>.log (full output) and, when
                   it makes one, <out>/<gate>.tsv; <out>/summary.tsv holds one row per gate.
  --compare <dir>  an earlier run's out dir: every TSV gate is compared row by row (key = first column(s), compared columns
                   status / digest / records): the summary line gets `same` or `N rows changed`, details in <gate>.cmp.txt.
  --lock <step>    take log/roadmap/S3/w8_lock.py's heavy-run lock around each gate (add --lock-priority for the lead).
  --min-free-gb N  wait for N GB of free commit charge before each gate (default 8, 0 = no wait; at most 10 min, then it starts anyway).
  --keep-going     run the remaining gates after a FAIL (default: yes for tiers and lists; --stop-on-fail stops).

stdout carries only one `[N/M] gate STATUS seconds :: summary` line per gate (the Happy progress-bar format) and the
final table; read <gate>.log only for a gate that failed. Non-pytest gates run inside a machine-wide test slot
(..\.claude\scripts\testslot\proyectos_test_slot.py); pytest gates take one by themselves.
"""
from __future__ import annotations

import argparse
import csv
import ctypes
import datetime as dt
import os
import re
import pathlib
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
SLOT = ROOT.parent / ".claude" / "scripts" / "testslot" / "proyectos_test_slot.py"
LOCK = ROOT / "log" / "roadmap" / "S3" / "w8_lock.py"
PY = sys.executable
PYTEST = [PY, "-m", "pytest", "-q", "-p", "no:cacheprovider"]
W32_ENV = {"SG_BACKEND": "win32"}
FT_ENV = {"SG_TEXT_METRICS": "freetype"}
# row statuses that are never a pass (a census FAIL is a measured pixel difference, so the census gates use their own set)
BAD_ROWS = frozenset({"ERROR", "FAIL", "LOAD_FAILED", "CHAIN_INVALID", "NOT_RUN", "UNJUDGED", "", None})
CENSUS_BAD = frozenset({"ERROR", "NOT_RUN", "UNJUDGED", "", None})


class Gate:
    def __init__(self, name, cmd, *, env=None, tsv=False, slot=True, doc="", key=1, grep=None, status_col="status",
                 bad=None):
        # grep: a regex; the summary is every log line matching it, joined (default: the log's last line)
        # status_col: the TSV column the summary counts and the verdict reads; bad: row statuses that FAIL the gate even
        # when the command exits 0 (2026-11-04: an all-ERROR segs_ft_x64 and a MemoryError chain both read PASS)
        self.name, self.cmd, self.env, self.tsv, self.slot, self.doc, self.key, self.grep = (
            name, cmd, env or {}, tsv, slot, doc, key, grep)
        self.status_col = status_col
        self.bad = BAD_ROWS if bad is None else frozenset(bad)


def _gates(out: pathlib.Path) -> dict[str, Gate]:
    def t(n):
        return str(out / f"{n}.tsv")
    g = [
        Gate("static", PYTEST + ["-m", "static"], slot=False, doc="repo static suite (anchors, trackers, tools)"),
        Gate("segs_x64", [PY, "-m", "port.test.integrator", "segments", "--backend", "x64", "--jobs", "4", "--out", t("segs_x64")],
             tsv=True, doc="205 accepted segments, x64, with digests"),
        Gate("segs_win32", [PY, "-m", "port.test.integrator", "segments", "--backend", "win32", "--jobs", "4", "--out", t("segs_win32")],
             tsv=True, doc="the same on the Win32 runner"),
        Gate("segs_ft_x64", [PY, "-m", "port.test.integrator", "segments", "--backend", "x64", "--jobs", "4", "--out", t("segs_ft_x64")],
             env=FT_ENV, tsv=True, doc="segments with the FreeType text provider"),
        Gate("boot_x64", [PY, "-m", "port.test.boot", "--all", "--jobs", "3", "--out", t("boot_x64")], tsv=True,
             doc="boot gate (every scenario from process start)"),
        Gate("boot_win32", [PY, "-m", "port.test.boot", "--all", "--jobs", "3", "--out", t("boot_win32")], env=W32_ENV, tsv=True,
             doc="boot gate on Win32"),
        Gate("boot_ft_x64", [PY, "-m", "port.test.boot", "--all", "--jobs", "3", "--out", t("boot_ft_x64")], env=FT_ENV, tsv=True,
             doc="boot gate with FreeType"),
        Gate("app_replay_x64", [PY, "-m", "port.test.app_replay", "gate", "--out", t("app_replay_x64")], tsv=True,
             doc="the app's recorded sessions (hole, saveload, report)"),
        Gate("app_replay_win32", [PY, "-m", "port.test.app_replay", "gate", "--backend", "win32", "--out", t("app_replay_win32")],
             tsv=True, doc="the same on Win32"),
        Gate("gfx_check", [PY, str(ROOT / "log/roadmap/S3/13_gfx_check.py"), "x64", "win32"], doc="0 duplicate gfx bodies",
             grep=r"duplicates \d+"),
        Gate("live_equiv_x64", [PY, "-m", "port.test.live", "equiv", "--all", "--jobs", "3", "--out", t("live_equiv_x64")],
             tsv=True, doc="live core == sg_run_boot"),
        Gate("live_equiv_win32", [PY, "-m", "port.test.live", "equiv", "--all", "--jobs", "3", "--backend", "win32",
                                  "--out", t("live_equiv_win32")], tsv=True, doc="the same on Win32"),
        Gate("chain", [PY, "-m", "port.test.integrator", "chain", "--backend", "x64", "--all", "--jobs", "4", "--out", t("chain")],
             tsv=True, doc="segment chains as one run (gate c)"),
        Gate("census", [PY, "-m", "port.test.framegate", "--all", "--jobs", "2", "--no-probe", "--out", t("census"), "--resume"],
             tsv=True, key=2, bad=CENSUS_BAD, doc="frame census, GlRaster (raw)"),
        Gate("census_split", [PY, "-m", "port.test.framegate", "--all", "--jobs", "2", "--no-probe", "--split",
                              "--out", t("census_split"), "--resume"], tsv=True, key=2, status_col="split_status", bad=CENSUS_BAD,
             doc="frame census, split verdict (G18)"),
        Gate("terrain", [PY, str(ROOT / "log/roadmap/S3/2_census.py"), "terrain", "--jobs", "3", "--out", t("terrain")], tsv=True,
             doc="S3.2 terrain verdicts"),
        Gate("segs0_x64", [PY, str(ROOT / "log/roadmap/S7/12_lead_segs_sw0.py"), t("segs0_x64")], tsv=True,
             doc="segments with S7.12's switch at 0"),
        Gate("boot0_x64", [PY, str(ROOT / "log/roadmap/S7/12_lead_boot_sw0.py"), t("boot0_x64"), "--jobs", "3"], tsv=True,
             doc="boot with S7.12's switch at 0"),
    ]
    return {x.name: x for x in g}


TIERS = {
    # decision G30 (2026-11-03): the fast gates I25 ran
    "fast": ["static", "segs_x64", "segs_win32", "boot_x64", "boot_win32", "app_replay_x64", "gfx_check",
             "t_test_static", "t_test_golfers", "t_test_tick"],
    "full": ["static", "segs_x64", "segs_win32", "segs_ft_x64", "boot_x64", "boot_win32", "boot_ft_x64",
             "app_replay_x64", "app_replay_win32", "gfx_check", "live_equiv_x64", "live_equiv_win32", "chain",
             "census", "census_split", "terrain", "port_suite", "port_suite_w32"],
}


def port_files(tree: pathlib.Path) -> list[str]:
    return sorted(p.stem for p in (tree / "port" / "test").glob("test_*.py"))


def resolve(names: list[str], gates: dict[str, Gate], tree: pathlib.Path) -> list[Gate]:
    out, seen = [], set()
    for n in names:
        if n == "port_suite":
            sub = [f"t_{f}" for f in port_files(tree)]
        elif n == "port_suite_w32":
            sub = [f"w32_{f}" for f in port_files(tree)]
        else:
            sub = [n]
        for s in sub:
            if s in seen:
                continue
            seen.add(s)
            if s in gates:
                out.append(gates[s])
            elif s.startswith(("t_", "w32_")):
                w32 = s.startswith("w32_")
                f = s[4:] if w32 else s[2:]
                if not (tree / "port" / "test" / f"{f}.py").exists():
                    raise SystemExit(f"no port/test/{f}.py in {tree}")
                out.append(Gate(s, PYTEST + [f"port/test/{f}.py", "-m", ""], env=W32_ENV if w32 else {}, slot=False,
                                doc=f"pytest port/test/{f}.py{' on Win32' if w32 else ''}"))
            else:
                raise SystemExit(f"unknown gate {s!r} (--list shows them)")
    return out


def tree_env(tree: pathlib.Path) -> dict[str, str]:
    if tree.resolve() == ROOT:
        return {}
    b = tree / "port" / "build"
    return {
        "SG_PORTSIM_DLL": str(b / "x64/Release/portsim_c.dll"),
        "SG_PORTSIM_W32_DIR": str(b / "Win32"),
        "SG_PORTSIM_DLL32": str(b / "Win32/Release/portsim_c.dll"),
        "SG_PORTSIM_RUNNER32": str(b / "Win32/Release/portsim_runner32.exe"),
        "SG_APP_EXE": str(b / "x64/app/Release/simgolf_port.exe"),
        "SG_TREE": str(tree),
    }


def free_commit_gb() -> float:
    class MS(ctypes.Structure):
        _fields_ = [("dwLength", ctypes.c_ulong), ("dwMemoryLoad", ctypes.c_ulong)] + [
            (n, ctypes.c_ulonglong) for n in ("tp", "ap", "tpf", "apf", "tv", "av", "aev")]
    m = MS()
    m.dwLength = ctypes.sizeof(MS)
    ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(m))
    return m.apf / 2**30


MEM_WAIT_MAX = 600


def wait_memory(need: float) -> None:
    # Same policy as the test slot: wait at most 10 min, then start anyway (the slot's Job Object caps the run).
    # A fixed threshold can exceed what the machine ever frees: LIVING-PC's commit limit was 23.9 GB on 2026-11-04
    # with 10-12 GB free at rest, and the old default of 16 GB waited forever.
    if need <= 0:
        return
    deadline = time.time() + MEM_WAIT_MAX
    while (f := free_commit_gb()) < need:
        if time.time() >= deadline:
            print(f"  (free commit {f:.1f} GB < {need} after {MEM_WAIT_MAX // 60} min, starting anyway)", flush=True)
            return
        print(f"  (free commit {f:.1f} GB < {need}, waiting)", flush=True)
        time.sleep(60)


def last_line(p: pathlib.Path, grep: str | None = None) -> str:
    try:
        lines = [x for x in p.read_text(encoding="utf-8", errors="replace").splitlines() if x.strip()]
    except OSError:
        return ""
    if grep:
        hit = [x.strip() for x in lines if re.search(grep, x)]
        if hit:
            return " | ".join(hit)[:160]
    return lines[-1].strip()[:160] if lines else ""


def read_tsv(p: pathlib.Path):
    with p.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def compare(gate: Gate, new: pathlib.Path, old: pathlib.Path, cmp_out: pathlib.Path) -> str:
    if not old.exists():
        return "no earlier tsv"
    a, b = read_tsv(old), read_tsv(new)
    if not b:
        return "empty tsv"
    cols = list(b[0].keys())
    keyc = cols[:gate.key]
    cmpc = [c for c in ("status", "digest", "records", "total_px", "beyond_px") if c in cols]
    k = lambda r: tuple(r.get(c, "") for c in keyc)
    old_rows = {k(r): r for r in a}
    lines, changed = [], 0
    for r in b:
        o = old_rows.pop(k(r), None)
        if o is None:
            changed += 1
            lines.append(f"NEW  {'/'.join(k(r))} {[r.get(c) for c in cmpc]}")
            continue
        d = [f"{c}: {o.get(c)} -> {r.get(c)}" for c in cmpc if o.get(c) != r.get(c)]
        if d:
            changed += 1
            lines.append(f"DIFF {'/'.join(k(r))} " + "; ".join(d))
    for key in old_rows:
        changed += 1
        lines.append(f"GONE {'/'.join(key)}")
    cmp_out.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    return "same" if not changed else f"{changed} rows changed (see {cmp_out.name})"


def lock(action: str, step: str, what: str, prio: bool, need: float) -> None:
    cmd = [PY, str(LOCK), action, step]
    if action == "take":
        cmd += ["--what", what, "--need-gb", str(int(need))] + (["--priority"] if prio else [])
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def run_gate(g: Gate, i: int, m: int, a, tree: pathlib.Path, out: pathlib.Path, base_env: dict) -> dict:
    if a.lock:
        lock("take", a.lock, f"gate {g.name}", a.lock_priority, a.min_free_gb)
    wait_memory(a.min_free_gb)
    env = dict(os.environ)
    for v in ("SG_BACKEND", "SG_TEXT_METRICS", "SG_EMU_ORACLE", "SG_APP_PRIORITY"):
        env.pop(v, None)
    env.update(base_env)
    env.update(g.env)
    env.setdefault("SG_APP_FACTS_DIR", str(out / "facts"))
    cmd = ([PY, str(SLOT)] if g.slot and SLOT.exists() else []) + g.cmd
    log = out / f"{g.name}.log"
    t0 = time.time()
    with log.open("w", encoding="utf-8") as f:
        f.write(f"== {g.name} {dt.datetime.now():%F %T} cwd={tree}\n== {' '.join(cmd)}\n")
        f.flush()
        rc = subprocess.run(cmd, cwd=str(tree), env=env, stdout=f, stderr=subprocess.STDOUT).returncode
    if a.lock:
        lock("release", a.lock, "", False, 0)
    secs = round(time.time() - t0)
    ok = rc == 0 or (rc == 5 and g.cmd[:3] == PYTEST[:3])        # pytest 5 = nothing collected / all deselected
    summary = last_line(log, g.grep)
    cmp = ""
    tsv = out / f"{g.name}.tsv"
    if g.tsv and tsv.exists():
        rows = read_tsv(tsv)
        col = g.status_col if rows and g.status_col in rows[0] else "status"
        if rows and col in rows[0]:
            counts = {}
            for r in rows:
                counts[r[col]] = counts.get(r[col], 0) + 1
            summary = " ".join(f"{k} {v}" for k, v in sorted(counts.items(), key=lambda kv: -kv[1]))
            nbad = sum(v for k, v in counts.items() if k in g.bad)
            if nbad and ok:
                ok, summary = False, f"{nbad} bad rows; {summary}"
        elif g.tsv and ok:
            ok, summary = False, f"{tsv.name} has no {g.status_col} column; {summary}"
        if a.compare:
            cmp = compare(g, tsv, pathlib.Path(a.compare) / f"{g.name}.tsv", out / f"{g.name}.cmp.txt")
    elif g.tsv and ok:
        ok, summary = False, f"no {tsv.name} written; {summary}"
    st = "PASS" if ok else "FAIL"
    print(f"[{i}/{m}] {g.name} {st} {secs} s :: {summary}" + (f" | vs earlier: {cmp}" if cmp else ""), flush=True)
    return {"gate": g.name, "status": st, "rc": rc, "seconds": secs, "summary": summary, "compare": cmp, "log": log.name}


def build(tree: pathlib.Path, out: pathlib.Path, base_env: dict) -> bool:
    env = dict(os.environ, **base_env)
    env.setdefault("SG_FREETYPE_ROOT", r"C:\Users\maria\vcpkg\installed")
    for arch in ("x64", "Win32"):
        log = out / f"build_{arch}.log"
        t0 = time.time()
        with log.open("w", encoding="utf-8") as f:
            rc = subprocess.run([PY, "port/test/build.py", "--arch", arch], cwd=str(tree), env=env, stdout=f,
                                stderr=subprocess.STDOUT).returncode
        print(f"[build] {arch} {'PASS' if rc == 0 else 'FAIL'} {round(time.time() - t0)} s :: {last_line(log)}", flush=True)
        if rc:
            return False
    return True


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("gates", nargs="*")
    ap.add_argument("--tier", choices=sorted(TIERS))
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--tree", default=str(ROOT))
    ap.add_argument("--build", action="store_true")
    ap.add_argument("--out")
    ap.add_argument("--label", default="")
    ap.add_argument("--compare")
    ap.add_argument("--lock")
    ap.add_argument("--lock-priority", action="store_true")
    ap.add_argument("--min-free-gb", type=float, default=8)
    ap.add_argument("--stop-on-fail", action="store_true")
    ap.add_argument("--dry-run", action="store_true", help="print the plan (commands) and exit")
    a = ap.parse_args(argv)

    tree = pathlib.Path(a.tree)
    if not tree.is_absolute():
        tree = (ROOT / tree).resolve()
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S") + (f"-{a.label}" if a.label else "")
    out = pathlib.Path(a.out) if a.out else ROOT / "log" / "gates" / stamp
    out = out if out.is_absolute() else (ROOT / out)
    gates = _gates(out)

    if a.list:
        print("gates:")
        for g in gates.values():
            print(f"  {g.name:18} {g.doc}")
        print("  t_<file>           pytest port/test/<file>.py (one process)")
        print("  w32_<file>         the same with SG_BACKEND=win32")
        print("  port_suite         every port/test file as t_<file>;  port_suite_w32: as w32_<file>")
        for k, v in TIERS.items():
            print(f"tier {k}: {' '.join(v)}")
        return 0
    names = (TIERS[a.tier] if a.tier else []) + a.gates
    if not names:
        ap.error("name gates or a --tier (--list shows them)")
    plan = resolve(names, gates, tree)
    base_env = tree_env(tree)
    if a.dry_run:
        for i, g in enumerate(plan, 1):
            print(f"[{i}/{len(plan)}] {g.name}: {' '.join(f'{k}={v}' for k, v in g.env.items())} {' '.join(g.cmd)}")
        return 0

    out.mkdir(parents=True, exist_ok=True)
    print(f"gates: {len(plan)} in {tree} -> {out}", flush=True)
    if a.build and not build(tree, out, base_env):
        print("build failed: no gate run")
        return 2
    results = []
    for i, g in enumerate(plan, 1):
        r = run_gate(g, i, len(plan), a, tree, out, base_env)
        results.append(r)
        if r["status"] == "FAIL" and a.stop_on_fail:
            break
    with (out / "summary.tsv").open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(results[0].keys()), delimiter="\t")
        w.writeheader()
        w.writerows(results)
    fails = [r["gate"] for r in results if r["status"] == "FAIL"]
    print(f"\n{len(results) - len(fails)} PASS / {len(fails)} FAIL" + (f": {' '.join(fails)}" if fails else "")
          + f"  (summary {out / 'summary.tsv'})")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
