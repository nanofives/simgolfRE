r"""The original's FLIC sheet cells (roadmap S3.4, 2026-10-24; notes log/roadmap/S3/4_notes.md).

loadFlic (golf_clean.exe 0x0043d740, a C2 row) decodes a .flc animation frame by frame and cuts every frame into a sprite
cell of the cell pool (0x005aaa30, 0x2c bytes per cell) through Surface_blit -> cropSprite. The port's decoder
(port/src/sim/flic.cpp) must give the same cells. This tool records them: a scenario_refs.py run (hooks off, the
device-less audio timeline) with re/frida/js/flic_cells.js appended to the agent, which reports every loadFlic call with
its arguments, its flicOpenStream results and, for each cell the load cut, the cell's SpriteRef record and the jgld Sprite
behind it (object words and pixel buffer). The run's checkpoints are compared with the pinned reference, so the recorder
is shown not to change the run.

    ~/bin/py -3.12 re/tools/flic_cells_rec.py record --scenario cov_b_career_types --instance s3_4 [--stop-after 11]
    ~/bin/py -3.12 re/tools/flic_cells_rec.py record --scenario cov_b_s34_hawaii --instance s3_4 --record \
        --extra log/roadmap/S3/4_scenarios.py [--tree-alias]
    ~/bin/py -3.12 re/tools/flic_cells_rec.py summary

--record makes a study scenario's schedule (SCENARIO_REFS_EXTRA registers it); a pinned scenario always replays its pinned
schedule. --tree-alias copies the install's Flics\Trees\Tropic\Tree_Cerc / Tree_Drac to `Tree Cerc` / `Tree Drac` in the
INSTANCE for the run (removed afterwards): the exe names them with a space (U-1701), and without the copies the stock game
dies at the first of those loads.

Output (local corpus, gitignored): port/corpus/flic/<scenario>/<label>/loads.jsonl (one JSON object per loadFlic call:
meta from flic_cells.js plus `bin` = [offset, length] into cells.bin), cells.bin (per cell: SpriteRef 0x2c bytes, jgld
Sprite words 0x38 bytes, pixel buffer, row spans), run.json (counters, failed opens, reference comparison).
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import scenario_refs as R  # noqa: E402  (before anything that imports game: it fixes the install dir)

FLIC_JS = ROOT / "re" / "frida" / "js" / "flic_cells.js"
OUT = ROOT / "port" / "corpus" / "flic"
MANIFEST = ROOT / "re" / "analysis" / "data" / "scenario_refs.json"
ALIASES = [("Flics/Trees/Tropic/Tree_Cerc", "Flics/Trees/Tropic/Tree Cerc"),
           ("Flics/Trees/Tropic/Tree_Drac", "Flics/Trees/Tropic/Tree Drac")]


def install(S, state: dict):
    Base = S.Run

    class FlicRun(Base):
        def __init__(self, *a, **kw):
            super().__init__(*a, **kw)
            self.fc_loads = []
            self.fc_failed = []
            self.fc_ready = False
            state["run"] = self

        def agent_source(self):
            p = super().agent_source()
            p.write_text(p.read_text() + "\nconst FLIC_CFG = {};\n" + FLIC_JS.read_text())
            return p

        def _on_message(self, m, data):
            if m.get("type") == "send":
                pl = m["payload"]
                if isinstance(pl, dict) and pl.get("type") in ("flic", "flic_open_failed", "flic_ready"):
                    if pl["type"] == "flic":
                        self.fc_loads.append((pl["meta"], bytes(data or b"")))
                    elif pl["type"] == "flic_open_failed":
                        self.fc_failed.append(pl)
                        print(f"  [flic] failed open: {pl.get('path')} {pl.get('open')}", flush=True)
                    else:
                        self.fc_ready = True
                    return
            elif m.get("type") == "error":
                state.setdefault("script_errors", []).append(m.get("description", ""))
            return super()._on_message(m, data)

    S.Run = FlicRun


def alias(instance: pathlib.Path, on: bool) -> list[str]:
    done = []
    for src, dst in ALIASES:
        s, d = instance / src, instance / dst
        if on:
            if not d.exists():
                shutil.copytree(s, d)
                done.append(str(d))
        elif d.exists():
            shutil.rmtree(d)
            done.append(str(d))
    return done


def cmd_record(a) -> int:
    instance = (ROOT / "instances" / a.instance).resolve()
    if a.extra:
        os.environ["SCENARIO_REFS_EXTRA"] = str((ROOT / a.extra).resolve())
    state: dict = {}
    orig_setup = R.setup

    def setup(scen, inst):
        S = orig_setup(scen, inst)
        install(S, state)
        return S

    R.setup = setup
    aliased = alias(instance, True) if a.tree_alias else []
    ns = argparse.Namespace(instance=a.instance, scenario=a.scenario, label=a.label, schedule=None, record=a.record, on=None,
                            env=None, stop_after=a.stop_after, clock_log=0, msg_log=0, trace=None, watch=None)
    try:
        rc = R.cmd_run(ns)
    finally:
        R.setup = orig_setup
        if aliased:
            alias(instance, False)
    run = state.get("run")
    out = OUT / a.scenario / a.label
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    n_cells = 0
    with (out / "cells.bin").open("wb") as fb, (out / "loads.jsonl").open("w", encoding="utf-8", newline="\n") as fj:
        for meta, data in (run.fc_loads if run else []):
            meta["bin"] = [fb.tell(), len(data)]
            fb.write(data)
            fj.write(json.dumps(meta) + "\n")
            n_cells += len(meta.get("cells", []))
    run_dir = R.REFS / a.scenario / a.label
    ref = json.loads(MANIFEST.read_text())["scenarios"].get(a.scenario)
    cmp = "no pinned reference"
    if ref and run_dir.exists():
        cmp = R.summarize(R.compare(ROOT / ref["reference"], run_dir))
    info = {"scenario": a.scenario, "label": a.label, "instance": a.instance, "rc": rc, "record": a.record,
            "stop_after": a.stop_after, "tree_alias": aliased, "ready": bool(run and run.fc_ready),
            "loads": len(run.fc_loads) if run else 0, "cells": n_cells, "failed_opens": run.fc_failed if run else [],
            "script_errors": state.get("script_errors", []), "reference": cmp, "run_dir": str(run_dir.relative_to(ROOT))}
    (out / "run.json").write_text(json.dumps(info, indent=1))
    print(f"flic {a.scenario}/{a.label}: rc {rc}, {info['loads']} loads, {n_cells} cells, {len(info['failed_opens'])} failed "
          f"opens; reference: {cmp}")
    return rc


def cmd_summary(a) -> int:
    for d in sorted(OUT.glob("*/*/run.json")):
        i = json.loads(d.read_text())
        print(f"{i['scenario']}/{i['label']}: rc {i['rc']} loads {i['loads']} cells {i['cells']} failed {len(i['failed_opens'])} "
              f"ref {i['reference']}")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("record")
    r.add_argument("--scenario", required=True)
    r.add_argument("--instance", default="s3_4")
    r.add_argument("--label", default="flic")
    r.add_argument("--record", action="store_true", help="make the schedule (study scenarios only)")
    r.add_argument("--stop-after", type=int, default=0)
    r.add_argument("--extra", help="SCENARIO_REFS_EXTRA file registering study scenarios")
    r.add_argument("--tree-alias", action="store_true")
    sub.add_parser("summary")
    a = ap.parse_args()
    return {"record": cmd_record, "summary": cmd_summary}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
