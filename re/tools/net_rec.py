r"""Record a scenario's schema v1 trace together with its NET.trace (DirectPlay calls), roadmap S9.2 (2026-10-29).

    # one side of a session on its own instance (a pinned or registered scenario; part 2 adds the S9 scenarios)
    SIMGOLF_ALLOW_MULTI=1 C3FT_AUDIO=none ~/bin/py -3.12 re/tools/net_rec.py record --scenario <name> --label <label> \
        --instance s9_host [--out port/corpus/net/<scenario>/<label>.trace]
    # offline: the combined agent (scenario_det.js + trace_rec.js + net_rec.js) parses, tables match the SDK headers
    ~/bin/py -3.12 re/tools/net_rec.py check

The run is trace_tool.py's record (scenario_refs.py `run`: instance lock, boot lock, family restore, pinned schedule) with
one more layer on the family's Run: the agent gets `const NET_CFG = {...};` + re/frida/js/net_rec.js after trace_rec.js,
and the agent's 'nrec' chunks are collected. Outputs: <out> (schema v1 trace), <out stem>.NET.trace (the side file, format
re/analysis/net/net_recordings.md), <out>.json (trace_tool's sidecar) and <out stem>.NET.json (net sidecar: sha256s,
validate, summary, agent errors). trace_tool.py, trace_format.py and trace_rec.js are used as they are.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import net_trace as NT  # noqa: E402

NET_JS = ROOT / "re" / "frida" / "js" / "net_rec.js"
CORPUS = ROOT / "port" / "corpus" / "net"
JS_PARTS = [ROOT / "re" / "frida" / "js" / "scenario_det.js", ROOT / "re" / "frida" / "js" / "trace_rec.js", NET_JS]


def net_meta(scenario: str) -> dict:
    block = NT.agent_block(NET_JS)
    if block is None:
        raise SystemExit("net_rec.js has no generated table block")
    body = block[block.index("{"):block.rindex("};") + 1]
    return NT.meta_for(json.loads(body), scenario, block)


def side_path(out: pathlib.Path) -> pathlib.Path:
    stem = out.name[:-len(".trace")] if out.name.endswith(".trace") else out.name
    return out.with_name(stem + ".NET.trace")


def install_net(TT, net_state: dict, cfg: dict):
    """Wrap trace_tool.install: after it has set S.Run = TraceRun, subclass that once more for net_rec.js."""
    base_install = TT.install

    def install(S, scenario, mode, state):
        base_install(S, scenario, mode, state)
        Base = S.Run

        class NetRun(Base):
            def __init__(self, *a, **kw):
                super().__init__(*a, **kw)
                self.nrec_chunks: dict[int, bytes] = {}
                self.nrec_final = None
                self.nrec_info: dict = {"errors": [], "ready": None, "watch": []}
                net_state["run"] = self

            def agent_source(self):
                p = super().agent_source()
                p.write_text(p.read_text() + "\nconst NET_CFG = " + json.dumps(cfg) + ";\n" + NET_JS.read_text(encoding="utf-8"))
                return p

            def _on_message(self, m, data):
                if m.get("type") == "send":
                    pl = m["payload"]
                    if isinstance(pl, dict) and pl.get("type") in ("nrec", "nrec_ready", "nrec_watch"):
                        if pl["type"] == "nrec":
                            self.nrec_chunks[pl["seq"]] = bytes(data or b"")
                            if pl.get("final"):
                                self.nrec_final = pl["seq"]
                        elif pl["type"] == "nrec_watch":
                            self.nrec_info["watch"].append(pl)
                        else:
                            self.nrec_info["ready"] = pl
                        return
                    if isinstance(pl, dict) and pl.get("type") == "error" and str(pl.get("where", "")).startswith("net"):
                        self.nrec_info["errors"].append(pl)
                return super()._on_message(m, data)

        S.Run = NetRun

    TT.install = install


def write_net(net_state: dict, meta: dict, out: pathlib.Path) -> bytes | None:
    run = net_state.get("run")
    if run is None or run.nrec_final is None:
        return None
    body = b"".join(run.nrec_chunks[i] for i in range(run.nrec_final + 1))
    data = NT.header_bytes(meta) + body
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    return data


def cmd_record(a) -> int:
    import trace_tool as TT
    out = pathlib.Path(a.out) if a.out else CORPUS / a.scenario / f"{a.label}.trace"
    net_state: dict = {}
    install_net(TT, net_state, {"maxBytes": a.max_bytes})
    rc = TT.cmd_record(argparse.Namespace(scenario=a.scenario, label=a.label, instance=a.instance, out=str(out)))
    meta = net_meta(a.scenario)
    if out.exists():
        meta["trace_sha256"] = hashlib.sha256(out.read_bytes()).hexdigest()
    side = side_path(out)
    data = write_net(net_state, meta, side)
    run = net_state.get("run")
    info = {"scenario": a.scenario, "label": a.label, "trace_rc": rc, "trace": str(out), "net": str(side),
            "agent": getattr(run, "nrec_info", None)}
    if data is None:
        info["result"] = "no complete NET.trace"
        rc = rc or 2
    else:
        tr = NT.parse(data)
        errs = NT.validate(tr)
        info.update(sha256=hashlib.sha256(data).hexdigest(), size=len(data), validate=errs or "OK", summary=NT.summary(tr))
        rc = rc or (1 if errs else 0)
    side.with_name(side.name[:-len(".trace")] + ".json").write_text(json.dumps(info, indent=1, default=str))
    print(f"net {a.scenario}/{a.label}: {side} {info.get('size')} bytes validate {info.get('validate')} "
          f"calls {(info.get('summary') or {}).get('by_type', {}).get('CALL', 0)}")
    return rc


def cmd_check(a) -> int:
    """Static: the agent's tables equal the SDK headers' (skipped without the SDK) and the combined agent parses."""
    rc = 0
    if NT.SDK.exists():
        ok = NT.agent_block(NET_JS) == NT.tables_js(NT.build_tables())
        print("tables:", "OK" if ok else "DIFFER (net_trace.py tables --write)")
        rc |= not ok
    else:
        print("tables: SDK headers absent, skipped")
    pre = {0: "const SCENARIO_CFG = {};\n", 1: "const TRACE_CFG = {};\n", 2: "const NET_CFG = {};\n"}
    src = "".join(pre[i] + p.read_text(encoding="utf-8") + "\n" for i, p in enumerate(JS_PARTS))
    with tempfile.TemporaryDirectory() as d:
        f = pathlib.Path(d) / "agent.js"
        f.write_text(src, encoding="utf-8")
        r = subprocess.run(["node", "--check", str(f)], capture_output=True, text=True)
    print("agent syntax:", "OK" if r.returncode == 0 else r.stderr.strip()[:2000])
    return rc | (r.returncode != 0)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("record")
    p.add_argument("--scenario", required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--instance", required=True)
    p.add_argument("--out")
    p.add_argument("--max-bytes", type=lambda s: int(s, 0), default=0x100000)
    p.set_defaults(fn=cmd_record)
    sp.add_parser("check").set_defaults(fn=cmd_check)
    a = ap.parse_args(argv)
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
