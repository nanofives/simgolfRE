r"""Phase 2.2 checkpoint tracer runs: replay a pinned scenario with the opt-in tracer of re/frida/scenario_ab.py and compare
runs object by object and frame by frame (re/PHASE23_PLAN.md 2.2; write-up re/analysis/data/gamestate_gen.md).

    # a replay of the pinned schedule (log/c3/alt/refs/<scenario>/schedule.json), every hook off, C3FT_AUDIO=none
    ~/bin/py -3.12 re/frida/state_trace.py run --scenario sandbox_basic --label off_a --obj-hash 1 --frame-hash sim
    ~/bin/py -3.12 re/frida/state_trace.py run --scenario sandbox_basic --label pert --obj-hash 1 --frame-hash sim \
        --perturb 822d9c:1@20000
    # two runs: first divergent frame and object (frame-head hashes), per-checkpoint object hashes
    ~/bin/py -3.12 re/frida/state_trace.py cmp --scenario sandbox_basic --a off_a --b pert [--classes sim]
    # a run against the pinned reference det_b (default checkpoint dumps and frames, scenario_ab.compare_runs)
    ~/bin/py -3.12 re/frida/state_trace.py check --scenario sandbox_basic --run off_a

Outputs: log/p2/p22/<scenario>/<label>/ (the scenario_ab files plus <name>.objhash.json, framehash.bin / .json). Instance
instances\p22 (created with re/tools/instances.py, winmm.dll of the pinned references 381ed2b2...), SIMGOLF_ALLOW_MULTI=1.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "log" / "p2" / "p22"
PINNED = ROOT / "log" / "c3" / "alt" / "refs"


def setup(scen: str, instance: pathlib.Path):
    """scenario_refs.setup with every output under log/p2/p22 (the boot lock stays the shared one)."""
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import scenario_refs as SR
    SR.REFS = OUT
    os.environ.setdefault("C3FT_AUDIO", "none")
    S = SR.setup(scen, instance)
    return S, SR


def sched_path(scen: str) -> pathlib.Path:
    p = OUT / scen / "schedule.json"
    if not p.exists():
        p.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PINNED / scen / "schedule.json", p)
    return p


def cmd_run(a):
    for flag, var in (("obj_hash", "C3FT_OBJHASH"), ("frame_hash", "C3FT_FRAMEHASH"), ("perturb", "C3FT_PERTURB")):
        if getattr(a, flag):
            os.environ[var] = getattr(a, flag)
    instance = (ROOT / "instances" / a.instance).resolve()
    S, SR = setup(a.scenario, instance)
    sched = json.loads(sched_path(a.scenario).read_text())
    out = OUT / a.scenario / a.label
    if out.exists():
        shutil.rmtree(out)
    ilock = SR.instance_lock(instance)
    try:
        dll, backup = instance / "winmm.dll", instance / "winmm.dll.c3ft_main"
        if backup.exists() and SR.sha256(dll) != SR.sha256(backup):
            raise SystemExit(f"{dll} differs from {backup.name}: a killed control run left its DLL (restore it first)")
        S.write_ini_hooks([])
        cond = SR.conditions(instance)
        run = S.Run("p22", a.scenario, "replay", S.arm_env_off(), [], [], schedule=sched, out_dir=out)
        rc = 0
        try:
            run.go(S.SCENARIOS[a.scenario])
        except Exception as e:  # noqa: BLE001
            print(f"RUN FAILED: {type(e).__name__}: {e}", flush=True)
            rc = 2
        finally:
            log = instance / "simgolf_shim.log"
            if log.exists():
                shutil.copy2(log, out / "shim.log")
            (out / "conditions.json").write_text(json.dumps(cond, indent=1))
        print(f"{a.scenario}/{a.label}: {'OK' if rc == 0 else 'FAILED'} polls={run.polls} checkpoints={len(run.checkpoints)} "
              f"wall={getattr(run, 'wall', 0):.0f}s")
        return rc
    finally:
        shutil.rmtree(ilock, ignore_errors=True)


def cmd_cmp(a):
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import frame_trace as FT
    da, db = OUT / a.scenario / a.a, OUT / a.scenario / a.b
    fr = FT.compare_frames(da, db)
    oh = FT.compare_objhash(da, db, set(a.classes.split(",")) if a.classes else None)
    FT.print_report(fr, oh)
    res = {"a": str(da), "b": str(db), "classes": a.classes, "frames": fr, "objhash": oh}
    (OUT / a.scenario / f"cmp_{a.a}_vs_{a.b}{'_' + a.classes.replace(',', '+') if a.classes else ''}.json").write_text(
        json.dumps(res, indent=1))
    return 0 if fr.get("same") and oh["same"] else 1


def cmd_check(a):
    """A run's default checkpoint dumps and frames against the pinned det_b (scenario_ab.compare_runs)."""
    instance = (ROOT / "instances" / a.instance).resolve()
    S, _SR = setup(a.scenario, instance)
    ref = PINNED / a.scenario / a.ref
    run = OUT / a.scenario / a.run
    cmp = S.compare_runs(ref, run)
    S.print_compare(cmp)
    same = all(not r.get("missing") and not r["regions_differ"] and r["frame_px_differ"] == 0 for r in cmp.values())
    (OUT / a.scenario / f"check_{a.run}_vs_{a.ref}.json").write_text(json.dumps(cmp, indent=1))
    print("SAME as the pinned reference" if same else "DIFFERS from the pinned reference")
    return 0 if same else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("run")
    p.add_argument("--scenario", required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--instance", default="p22")
    p.add_argument("--obj-hash", help="1 | image (C3FT_OBJHASH)")
    p.add_argument("--frame-hash", help="classes, e.g. sim (C3FT_FRAMEHASH)")
    p.add_argument("--perturb", help="<va>:<xor>@<virtual ms> (C3FT_PERTURB)")
    p.set_defaults(fn=cmd_run)
    p = sp.add_parser("cmp")
    p.add_argument("--scenario", required=True)
    p.add_argument("--a", required=True)
    p.add_argument("--b", required=True)
    p.add_argument("--classes")
    p.set_defaults(fn=cmd_cmp)
    p = sp.add_parser("check")
    p.add_argument("--scenario", required=True)
    p.add_argument("--run", required=True)
    p.add_argument("--ref", default="det_b")
    p.add_argument("--instance", default="p22")
    p.set_defaults(fn=cmd_check)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
