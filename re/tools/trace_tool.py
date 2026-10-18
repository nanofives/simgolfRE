r"""Clock / input traces of the original (phase 2.3, 2026-10-18). Format: re/model/trace_schema.md; reader:
re/tools/trace_format.py; agent: re/frida/js/trace_rec.js (appended to scenario_det.js only for these runs).

    # RECORD: replay a pinned scenario's schedule (re/analysis/data/scenario_refs.json) under the virtual clock and write
    # its trace; the run's checkpoints are compared with the pinned reference (det_b)
    ~/bin/py -3.12 re/tools/trace_tool.py record --scenario sandbox_basic --label p23_rec1 [--out <file>] [--instance p23]
    # REPLAY: drive the original from a trace alone (recorded clock values and deliveries, no schedule, no virtual clock
    # on the main thread); writes the replay's own trace and compares it with the input trace and the checkpoints with
    # the pinned reference
    ~/bin/py -3.12 re/tools/trace_tool.py replay --scenario sandbox_basic --trace port/corpus/traces/sandbox_basic.trace
    # offline
    ~/bin/py -3.12 re/tools/trace_tool.py validate <trace>      # schema rules; exit 1 on a violation
    ~/bin/py -3.12 re/tools/trace_tool.py summary <trace> [--json]
    ~/bin/py -3.12 re/tools/trace_tool.py dump <trace> [--types FRAME,MSG] [--from N] [--limit N]
    ~/bin/py -3.12 re/tools/trace_tool.py diff <a> <b>          # first differing record; exit 1 when they differ

Record and replay go through re/tools/scenario_refs.py's `run` (instance lock, boot lock, the scenario family's restore
of saves / themes before the boot, the pinned schedule). Default output port/corpus/traces/<scenario>.trace (gitignored,
local corpus) with a sidecar <trace>.json (run folder, conditions, agent statistics, sha256, comparisons).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import trace_format as TF  # noqa: E402

TRACE_JS = ROOT / "re" / "frida" / "js" / "trace_rec.js"
CORPUS = ROOT / "port" / "corpus" / "traces"
MANIFEST = ROOT / "re" / "analysis" / "data" / "scenario_refs.json"


# ---------------------------------------------------------------- the run hook (installed after scenario_refs.setup)
def install(S, scenario: str, mode: str, state: dict):
    """Subclass the family's Run so the agent carries trace_rec.js and the trace chunks are collected."""
    Base = S.Run

    class TraceRun(Base):
        def __init__(self, *a, **kw):
            super().__init__(*a, **kw)
            self.trec_chunks: dict[int, bytes] = {}
            self.trec_final = None
            self.trec_stats = None
            state["run"] = self

        def agent_source(self):
            p = super().agent_source()
            cfg = {"mode": mode, "frameRa": TF.FRAME_RA}
            if mode == "replay":
                cfg.update(plan=str(state["plan_path"]), deliveries=state["deliveries"])
            p.write_text(p.read_text() + "\nconst TRACE_CFG = " + json.dumps(cfg) + ";\n" + TRACE_JS.read_text())
            return p

        def _on_message(self, m, data):
            if m.get("type") == "send":
                pl = m["payload"]
                if isinstance(pl, dict) and pl.get("type") in ("trec", "trec_stats", "trec_ready"):
                    if pl["type"] == "trec":
                        self.trec_chunks[pl["seq"]] = bytes(data or b"")
                        if pl.get("final"):
                            self.trec_final = pl["seq"]
                    elif pl["type"] == "trec_stats":
                        self.trec_stats = pl
                    return
            return super()._on_message(m, data)

        def post(self, sc, **kw):
            super().post(sc, **kw)
            if "actions" in kw and not kw.get("next"):
                # the script's last reply: the agent writes END and sends the last chunk after it returns from the poll,
                # so wait for it before _go leaves the Game context (which kills the game)
                t0 = time.time()
                while self.trec_final is None and time.time() - t0 < 60:
                    time.sleep(0.05)

        def take_dump(self, sc, name):
            sc.post({"type": "trecname", "name": name})
            return super().take_dump(sc, name)

    S.Run = TraceRun
    if mode == "replay":
        polls = state["polls"]

        def replay_script(c):
            for name in polls[:-1]:
                if name:
                    yield from c.checkpoint(name)
                else:
                    yield
        S.SCENARIOS[scenario] = replay_script


def trace_meta(S, scenario: str) -> dict:
    return {"schema": TF.VERSION, "scenario": scenario, "frame_ra": TF.FRAME_RA,
            "slots": [[s["module"], s["kind"], s["rva"]] for s in S.slot_config()],
            "snapshot": [[va, n] for va, n in TF.SNAPSHOT], "spec": "re/model/trace_schema.md"}


def run_traced(scenario: str, label: str, instance: str, mode: str, state: dict) -> tuple[int, pathlib.Path]:
    import scenario_refs as R
    orig_setup = R.setup
    meta_box = {}

    def setup(scen, inst):
        S = orig_setup(scen, inst)
        install(S, scenario, mode, state)
        meta_box["S"] = S
        return S

    R.setup = setup
    if mode == "replay":
        os.environ["C3FT_NO_VKEYS"] = "1"        # the vkey resets of the first poll are deliveries in the trace
    a = argparse.Namespace(instance=instance, scenario=scenario, label=label, schedule=None, record=False, on=None, env=None,
                           stop_after=0, clock_log=0, msg_log=0, trace=None, watch=None)
    rc = R.cmd_run(a)
    S = meta_box["S"]
    state["meta"] = trace_meta(S, scenario)
    state["clock"] = {"step": S.STEP_MS, "t0": S.T0, "qstep": max(S.qpc_freq() // 1000, 1) * S.STEP_MS, "q0": 0x10000000}
    return rc, R.REFS / scenario / label


def write_trace(state: dict, out: pathlib.Path) -> bytes | None:
    run = state.get("run")
    if run is None or run.trec_final is None:
        return None
    body = b"".join(run.trec_chunks[i] for i in range(run.trec_final + 1))
    c = state["clock"]
    data = TF.header_bytes(state["meta"], c["step"], c["t0"], c["qstep"], c["q0"]) + body
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    return data


def ref_compare(scenario: str, run_dir: pathlib.Path) -> str:
    import scenario_refs as R
    ref = json.loads(MANIFEST.read_text())["scenarios"].get(scenario)
    if not ref:
        return "no pinned reference"
    c = R.compare(ROOT / ref["reference"], run_dir)
    return R.summarize(c)


def sidecar(out: pathlib.Path, info: dict):
    out.parent.mkdir(parents=True, exist_ok=True)
    (out.parent / (out.name + ".json")).write_text(json.dumps(info, indent=1, default=str))


def cmd_record(a) -> int:
    out = pathlib.Path(a.out) if a.out else CORPUS / f"{a.scenario}.trace"
    state: dict = {}
    rc, run_dir = run_traced(a.scenario, a.label, a.instance, "record", state)
    data = write_trace(state, out)
    info = {"scenario": a.scenario, "label": a.label, "mode": "record", "run_dir": str(run_dir.relative_to(ROOT)), "rc": rc,
            "stats": getattr(state.get("run"), "trec_stats", None)}
    if data is None:
        print(f"record {a.scenario}/{a.label}: no complete trace (rc {rc})")
        sidecar(out, info)
        return 2
    tr = TF.parse(data)
    errs = TF.validate(tr)
    info.update(sha256=hashlib.sha256(data).hexdigest(), size=len(data), validate=errs or "OK",
                reference=ref_compare(a.scenario, run_dir), summary=TF.summary(tr))
    cond = run_dir / "conditions.json"
    if cond.exists():
        info["conditions"] = json.loads(cond.read_text())
    sidecar(out, info)
    print(f"record {a.scenario}/{a.label}: {out} {len(data)} bytes sha {info['sha256'][:16]} validate {info['validate']} "
          f"vs reference: {info['reference']}")
    return 0 if rc == 0 and not errs else 1


def cmd_replay(a) -> int:
    src = pathlib.Path(a.trace)
    tr = TF.read(src)
    plan, deliveries, polls = TF.replay_plan(tr)
    plan_path = ROOT / "log" / "p2" / "p23" / "plans" / f"{a.scenario}_{a.label}.bin"
    plan_path.parent.mkdir(parents=True, exist_ok=True)
    plan_path.write_bytes(plan)
    state = {"plan_path": plan_path, "deliveries": deliveries, "polls": polls}
    out = pathlib.Path(a.out) if a.out else CORPUS / "runs" / f"{a.scenario}.{a.label}.trace"
    rc, run_dir = run_traced(a.scenario, a.label, a.instance, "replay", state)
    data = write_trace(state, out)
    stats = getattr(state.get("run"), "trec_stats", None)
    info = {"scenario": a.scenario, "label": a.label, "mode": "replay", "source": str(src), "run_dir": str(run_dir.relative_to(ROOT)),
            "rc": rc, "stats": stats, "plan": {"ordinals": len(plan) // 12, "deliveries": len(deliveries), "polls": len(polls)}}
    if data is None:
        print(f"replay {a.scenario}/{a.label}: no complete trace (rc {rc})")
        sidecar(out, info)
        return 2
    d = TF.diff(tr, TF.parse(data))
    info.update(sha256=hashlib.sha256(data).hexdigest(), size=len(data), same_as_source=d,
                byte_identical=data == src.read_bytes(), reference=ref_compare(a.scenario, run_dir))
    sidecar(out, info)
    rep = (stats or {}).get("replay") or {}
    print(f"replay {a.scenario}/{a.label}: trace {'BYTE-IDENTICAL' if info['byte_identical'] else 'DIFFERS'} to the source "
          f"({d.get('where', 'same')}); site mismatches {rep.get('mismatches')}, overrun {rep.get('overrun')}, "
          f"deliveries {rep.get('delivered')}/{rep.get('planned')}; checkpoints vs reference: {info['reference']}")
    return 0 if rc == 0 and info["byte_identical"] else 1


def cmd_validate(a) -> int:
    rc = 0
    for p in a.traces:
        try:
            errs = TF.validate(TF.read(p), virtual_clock=not a.any_clock)
        except TF.TraceError as e:
            errs = [f"parse: {e}"]
        print(f"{p}: {'OK' if not errs else 'INVALID'}")
        for e in errs[:20]:
            print("  " + e)
        rc |= bool(errs)
    return rc


def cmd_summary(a) -> int:
    s = TF.summary(TF.read(a.trace))
    if a.json:
        print(json.dumps(s, indent=1))
        return 0
    for k, v in s.items():
        print(f"{k:24s} {v}")
    return 0


def cmd_dump(a) -> int:
    tr = TF.read(a.trace)
    want = set(a.types.upper().split(",")) if a.types else None
    shown = 0
    for i, r in enumerate(tr.records):
        if i < a.start or (want and r.name not in want):
            continue
        print(f"{i:7d} {TF.fmt_record(r)}")
        shown += 1
        if a.limit and shown >= a.limit:
            break
    return 0


def cmd_diff(a) -> int:
    da, db = pathlib.Path(a.a).read_bytes(), pathlib.Path(a.b).read_bytes()
    if da == db:
        print(f"byte-identical ({len(da)} bytes)")
        return 0
    d = TF.diff(TF.parse(da), TF.parse(db))
    print(json.dumps(d, indent=1))
    return 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    for name in ("record", "replay"):
        p = sp.add_parser(name)
        p.add_argument("--scenario", required=True)
        p.add_argument("--label", required=True)
        p.add_argument("--instance", default="p23")
        p.add_argument("--out")
        if name == "replay":
            p.add_argument("--trace", required=True)
        p.set_defaults(fn=cmd_record if name == "record" else cmd_replay)
    p = sp.add_parser("validate")
    p.add_argument("traces", nargs="+")
    p.add_argument("--any-clock", action="store_true", help="skip the virtual-clock value rule")
    p.set_defaults(fn=cmd_validate)
    p = sp.add_parser("summary")
    p.add_argument("trace")
    p.add_argument("--json", action="store_true")
    p.set_defaults(fn=cmd_summary)
    p = sp.add_parser("dump")
    p.add_argument("trace")
    p.add_argument("--types")
    p.add_argument("--from", dest="start", type=int, default=0)
    p.add_argument("--limit", type=int, default=0)
    p.set_defaults(fn=cmd_dump)
    p = sp.add_parser("diff")
    p.add_argument("a")
    p.add_argument("b")
    p.set_defaults(fn=cmd_diff)
    a = ap.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
