r"""Roadmap S8.2 driver: the ORIGINAL's intro / closing movies measured (log/roadmap/S8/2_notes.md, re/analysis/video/movie_runs.md).

Runs re/tools/scenario_refs.py and re/tools/trace_tool.py unchanged, with three additions made from outside:
  * re/tools/scenario_s8_2.py is imported first (its scenarios cov_r_s8_intro / cov_r_s8_full are registered in scenario.SCENARIOS,
    which the end-of-file hook of scenario.py does not do for scenario_s8_*.py files);
  * the run's game environment gets SIMGOLF_SKIP_INTRO=<--skip-intro> (scenario_ab.Run.go sets 1 and lets Run.env override);
  * with --bink-log the agent also carries a binkw32 call recorder (Interceptor on the nine imported exports and the other
    exports the exe reaches, no clock call, so the virtual timeline is unchanged): <run>/s8_bink.json.

    ~/bin/py -3.12 re/tools/s8_movie_runs.py --skip-intro 0 --bink-log ref run --instance s8_2 --scenario cov_r_s8_full --label det_a --record --clock-log 60000
    ~/bin/py -3.12 re/tools/s8_movie_runs.py --skip-intro 0 --bink-log ref run --instance s8_2 --scenario cov_r_s8_full --label r1 --clock-log 60000
    ~/bin/py -3.12 re/tools/s8_movie_runs.py --skip-intro 0 trace record --instance s8_2 --scenario cov_r_s8_full --label t1 --out <file>
    ~/bin/py -3.12 re/tools/s8_movie_runs.py analyze ...        (see `analyze --help`)

Every boot is on instances\<name> (never original\): scenario_refs.setup points scenario_ab at the instance given by --instance.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
for _p in (ROOT / "log" / "c3", ROOT / "re" / "frida", ROOT / "re" / "tools"):
    sys.path.insert(0, str(_p))

BINK_JS = r"""
// S8.2 binkw32 call recorder (appended to the scenario agent; shares scenario_det.js's scope: T() is the virtual clock)
const S8 = { rows: [], seq: 0, wait: null, sent: 0, handles: {} };
const Q = () => D.add(0x10).readU32() - (CFG.q0 >>> 0);   // virtual QPC counts since q0 (main-thread calls advance it by cfg.qstep)
function s8flush(force) {
  if (S8.wait) { S8.rows.push(S8.wait); S8.wait = null; }
  if (S8.rows.length && (force || S8.rows.length >= 256)) {
    send({ type: 's8bink', rows: S8.rows.splice(0, S8.rows.length) });
  }
}
function s8row(name, args, ret, extra) {
  const r = [S8.seq++, T() - CFG.t0, name, args, ret, extra === undefined ? null : extra, Q()];
  S8.rows.push(r);
  s8flush(name === '_BinkClose@4' || name === '_BinkBufferClose@4');
}
function s8hdr(h) {   // eight dwords at the BINK handle: Width Height Frames FrameNum LastFrameNum FrameRate FrameRateDiv ReadError
  try { const o = []; for (let i = 0; i < 8; i++) o.push(h.add(i * 4).readU32()); return o; } catch (e) { return String(e); }
}
function s8cstr(p) { try { return p.readCString(); } catch (e) { return null; } }
const S8_SPEC = {
  '_BinkSetSoundSystem@8': { n: 2 }, '_BinkSetIOSize@4': { n: 1 }, '_BinkOpen@8': { n: 2, str0: true, hdrRet: true },
  '_BinkBufferOpen@16': { n: 4 }, '_BinkBufferSetOffset@12': { n: 3 }, '_BinkWait@4': { n: 1, wait: true },
  '_BinkDoFrame@4': { n: 1 }, '_BinkGetRects@8': { n: 2 }, '_BinkBufferLock@4': { n: 1 }, '_BinkCopyToBuffer@28': { n: 7 },
  '_BinkBufferUnlock@4': { n: 1 }, '_BinkBufferBlit@12': { n: 3 }, '_BinkNextFrame@4': { n: 1 },
  '_BinkClose@4': { n: 1, hdrArg: true }, '_BinkBufferClose@4': { n: 1 }, '_BinkOpenDirectSound@4': { n: 1 },
};
function s8attach(m) {
  if (m.name.toLowerCase() !== 'binkw32.dll') return;
  const done = [];
  for (const name of Object.keys(S8_SPEC)) {
    const a = m.findExportByName(name);
    if (!a) { send({ type: 's8bink', missing: name }); continue; }
    const sp = S8_SPEC[name];
    Interceptor.attach(a, {
      onEnter(args) {
        const th = this.threadId;
        this.s8 = (th === MAIN);
        if (!this.s8) { send({ type: 's8bink', offthread: name, tid: th }); return; }
        this.a = [];
        for (let i = 0; i < sp.n; i++) this.a.push(args[i].toUInt32());
        if (sp.str0) this.str = s8cstr(args[0]);
        if (sp.hdrArg) this.hdr = s8hdr(args[0]);
        if (sp.hdrArg) this.fr = this.a[0];
      },
      onLeave(ret) {
        if (!this.s8) return;
        const rv = ret.toInt32();
        if (sp.wait) {
          const w = S8.wait;
          if (w && w[3][0] === this.a[0] && w[4] === rv) { w[5][0]++; w[5][2] = T() - CFG.t0; w[5][4] = Q(); return; }
          if (w) S8.rows.push(w);
          S8.wait = [S8.seq++, T() - CFG.t0, name, [this.a[0]], rv, [1, T() - CFG.t0, T() - CFG.t0, Q(), Q()], Q()];
          return;
        }
        if (S8.wait) { S8.rows.push(S8.wait); S8.wait = null; }
        let extra;
        if (sp.str0) extra = { name: this.str, hdr: sp.hdrRet && rv ? s8hdr(ret) : undefined };
        if (sp.hdrArg) extra = { hdr: this.hdr };
        s8row(name, this.a, rv, extra);
      },
    });
    done.push(name);
  }
  send({ type: 's8bink', attached: done, info: { qstep: CFG.qstep, step: CFG.step, q0: CFG.q0, t0: CFG.t0 } });
}
(function () {
  const m = Process.findModuleByName('binkw32.dll');
  if (m) s8attach(m); else Process.attachModuleObserver({ onAdded(mm) { s8attach(mm); } });
})();
rpc.exports.s8flush = function () { s8flush(true); return S8.seq; };
"""


def install_bink(S, state: dict):
    Base = S.Run

    class BinkRun(Base):
        def __init__(self, *a, **kw):
            super().__init__(*a, **kw)
            self.s8 = {"rows": [], "notes": []}
            state["bink_run"] = self

        def agent_source(self):
            p = super().agent_source()
            p.write_text(p.read_text() + "\n" + BINK_JS)
            return p

        def _on_message(self, m, data):
            if m.get("type") == "send":
                pl = m["payload"]
                if isinstance(pl, dict) and pl.get("type") == "s8bink":
                    if "rows" in pl:
                        self.s8["rows"] += pl["rows"]
                    else:
                        self.s8["notes"].append({k: v for k, v in pl.items() if k != "type"})
                    return
            return super()._on_message(m, data)

        def go(self, *a, **kw):
            try:
                return super().go(*a, **kw)
            finally:
                try:
                    (self.out / "s8_bink.json").write_text(json.dumps(self.s8))
                except OSError as e:  # noqa: BLE001
                    print(f"s8_bink.json not written: {e}", flush=True)

    S.Run = BinkRun


def patch_refs(skip_intro: int, bink: bool):
    import scenario_s8_2  # noqa: F401  (registers into scenario.SCENARIOS)
    import scenario_refs as R
    orig_setup, orig_cmd_run = R.setup, R.cmd_run
    state: dict = {}

    def setup(scen, inst):
        S = orig_setup(scen, inst)
        if bink:
            install_bink(S, state)
        return S

    def cmd_run(a):
        a.env = list(a.env or []) + [f"SIMGOLF_SKIP_INTRO={skip_intro}"]
        return orig_cmd_run(a)

    R.setup, R.cmd_run = setup, cmd_run
    return R


ANALYZE_ROOT = ROOT / "log" / "c3" / "alt" / "refs"


def _load(scen: str, label: str) -> dict:
    d = ANALYZE_ROOT / scen / label
    out = {"dir": d, "summary": json.loads((d / "summary.json").read_text())}
    for k, f in (("bink", "s8_bink.json"), ("clock", "clock.json")):
        if (d / f).exists():
            out[k] = json.loads((d / f).read_text())
    return out


COMPARED_CLASSES = ("sim", "hidden", "ui", "engine", "render", "const")     # never compared: pointer, audio, gaps (phase 2.2)


def _objhash_compared(path: pathlib.Path):
    """sha of the objhash.json entries of the compared classes only: pointer-class objects hold heap addresses, which differ
    between processes whatever the scenario does."""
    if not path.exists():
        return None
    d = json.loads(path.read_text())["hashes"]
    keep = {k: v for k, v in d.items() if v[0] in COMPARED_CLASSES}
    return hashlib.sha256(json.dumps(keep, sort_keys=True).encode()).hexdigest()[:16]


def run_identity(scen: str, labels: list[str]) -> dict:
    """Runs of one condition against the first: per checkpoint the main-thread tgt / qpc counts, the .bin state sha, the frame
    sha, the objhash json, and the clock-log digest (kind, slot module, caller sequence); the exit-file hashes."""
    rows = {}
    for lab in labels:
        r = _load(scen, lab)
        s = r["summary"]
        cps = {}
        for name, cp in s["checkpoints"].items():
            st = cp["status"]
            b = (r["dir"] / f"{name}.bin").read_bytes()
            oh = r["dir"] / f"{name}.objhash.json"
            cps[name] = {"tgt": st["tgt"], "qpc": st["qpc"], "T": cp["T"], "frame_sha": cp["frame_sha"][:16],
                         "state_sha": hashlib.sha256(b).hexdigest()[:16],
                         "objhash": _objhash_compared(oh)}
        clk = r.get("clock")
        digest = None
        if clk:
            digest = (clk["total"], hashlib.sha256(json.dumps([x[1:] for x in clk["rows"]]).encode()).hexdigest()[:16])
        rows[lab] = {"checkpoints": cps, "clock": digest, "exit": (s.get("exit") or {}).get("files"),
                     "exit_code": (s.get("exit") or {}).get("exit_code"), "polls": s["polls"]}
    first = rows[labels[0]]
    for lab in labels:
        r = rows[lab]
        r["identical_to_first"] = (r["checkpoints"] == first["checkpoints"] and r["clock"] == first["clock"]
                                   and r["exit"] == first["exit"] and r["exit_code"] == first["exit_code"])
    return rows


def movie_segments(bink: dict) -> list[dict]:
    """One dict per BinkOpen .. BinkClose: name, header words, per-export call counts (BinkWait counts every call), the rows,
    virtual Q / T at open and close and how it ended."""
    info = next((n["info"] for n in bink["notes"] if "info" in n), {"qstep": 10000, "step": 1})
    qstep, step = info["qstep"], info["step"]
    segs, cur = [], None
    for r in bink["rows"]:
        name = r[2]
        if name == "_BinkOpen@8" and r[5] and r[5].get("name") and r[4] == 0:
            segs.append({"name": r[5]["name"], "open_hdr": None, "rows": [r], "q_open": r[6], "t_open": r[1],
                         "calls": {name: 1}, "open_returned_null": True})
            continue
        if name == "_BinkOpen@8" and r[5] and r[5].get("name"):
            cur = {"name": r[5]["name"], "open_hdr": r[5]["hdr"], "rows": [], "q_open": r[6], "t_open": r[1], "calls": {}}
            segs.append(cur)
        elif cur is None:
            continue
        cur["rows"].append(r)
        n = r[5][0] if name == "_BinkWait@4" else 1
        cur["calls"][name] = cur["calls"].get(name, 0) + n
        if name == "_BinkClose@4":
            cur["close_hdr"] = r[5]["hdr"] if r[5] else None
            cur["q_close"], cur["t_close"] = r[6], r[1]
            cur = None
    for sg in segs:
        sg["qstep"], sg["step"] = qstep, step
        if "q_close" in sg:
            sg["virtual_ms_q"] = (sg["q_close"] - sg["q_open"]) / qstep * step
            sg["virtual_ms_t"] = sg["t_close"] - sg["t_open"]
            fr, fn = sg["close_hdr"][2], sg["close_hdr"][3]
            sg["ended_by"] = "clip end (FrameNum == Frames at BinkClose)" if fn == fr else f"early (FrameNum {fn} of {fr})"
    return segs


def clock_by_site(clk: dict) -> dict:
    out: dict = {}
    for _g, _i, kind, mod, caller in clk["rows"]:
        out[(kind, mod, caller)] = out.get((kind, mod, caller), 0) + 1
    return out


def image_diff(on: list[pathlib.Path], skip: list[pathlib.Path]) -> dict:
    """Full .data images of several runs of each condition. A dword is STABLE in a condition when all its runs agree (heap
    addresses vary from process to process, so they are not); a difference is attributed to the intro only when the dword is
    stable in both conditions and the two values differ."""
    import numpy as np
    import state_image as SI
    L = SI.layout()
    A = np.stack([np.frombuffer(p.read_bytes(), "<u4") for p in on])
    B = np.stack([np.frombuffer(p.read_bytes(), "<u4") for p in skip])
    st_on, st_sk = (A == A[0]).all(0), (B == B[0]).all(0)
    differ = A[0] != B[0]
    real = np.nonzero(differ & st_on & st_sk)[0]
    out = {"dwords": int(A.shape[1]), "differ_first_runs": int(differ.sum()), "unstable_on": int((~st_on).sum()),
           "unstable_skip": int((~st_sk).sum()), "attributed": []}
    for i in real:
        sym = L.symbol_at(int(i) * 4)
        o = L.by.get(sym)
        x, y = int(B[0][i]), int(A[0][i])
        out["attributed"].append({"va": f"0x{int(i) * 4 + L.base:08x}", "symbol": sym, "class": o["cls"] if o else "gap",
                                  "pointer_flag": o["pointer"] if o else "-", "skip": f"0x{x:08x}", "on": f"0x{y:08x}",
                                  "delta": f"0x{(y - x) & 0xFFFFFFFF:08x}",
                                  "heap_pointer_like": 0x00800000 <= x < 0x40000000 and 0x00800000 <= y < 0x40000000})
    return out


def qpc_per_export(bink: dict) -> dict:
    """Virtual QPC steps (main-thread QueryPerformanceCounter calls, one per step) spent inside each binkw32 export, from the
    Q the recorder read at each export's return: the steps between the previous row's end and this row's return."""
    info = next((n["info"] for n in bink["notes"] if "info" in n), {"qstep": 10000})
    qstep = info["qstep"]
    out: dict = {}
    prev = None
    for r in bink["rows"]:
        name = r[2]
        q_first = r[5][3] if name == "_BinkWait@4" else r[6]
        q_last = r[5][4] if name == "_BinkWait@4" else r[6]
        if prev is not None:
            out[name] = out.get(name, 0) + (q_last - prev) // qstep
        prev = q_last
    return out


def build_report() -> dict:
    """Everything movie_runs.md quotes, recomputed from the run folders. Labels: cov_r_s8_full ON rec_on on2 on3, skip skip_rec
    skip2 skip3, traces t1..t3 / rp3; cov_r_s8_intro ON i1 i2 i3, skip j1 j2 j3."""
    out: dict = {"scenarios": {}}
    for scen, ons, sks in (("cov_r_s8_full", ["rec_on", "on2", "on3"], ["skip_rec", "skip2", "skip3"]),
                           ("cov_r_s8_intro", ["i1", "i2", "i3"], ["j1", "j2", "j3"])):
        e: dict = {}
        for cond, labs in (("on", ons), ("skip", sks)):
            ident = run_identity(scen, labs)
            e[f"identity_{cond}"] = {"labels": labs, "all_identical": all(r["identical_to_first"] for r in ident.values()),
                                     "clock_total": ident[labs[0]]["clock"][0],
                                     "checkpoints": {n: {k: cp[k] for k in ("T", "tgt", "qpc", "state_sha", "frame_sha", "objhash")}
                                                     for n, cp in ident[labs[0]]["checkpoints"].items()},
                                     "exit_code": ident[labs[0]]["exit_code"]}
        on0, sk0 = _load(scen, ons[0]), _load(scen, sks[0])
        ca, cb = clock_by_site(on0["clock"]), clock_by_site(sk0["clock"])
        e["clock_added_by_intro"] = {f"{k[0]} {k[1]} {k[2]}": [ca.get(k, 0), cb.get(k, 0), ca.get(k, 0) - cb.get(k, 0)]
                                     for k in sorted(set(ca) | set(cb)) if ca.get(k, 0) != cb.get(k, 0)}
        e["clock_sites_common"] = {f"{k[0]} {k[1]} {k[2]}": [ca.get(k, 0), cb.get(k, 0)]
                                   for k in sorted(set(ca) | set(cb)) if ca.get(k, 0) == cb.get(k, 0) and ca.get(k, 0) > 100}
        root = ANALYZE_ROOT / scen
        sd = {}
        for n in on0["summary"]["checkpoints"]:
            d = image_diff([root / x / f"{n}.full.bin" for x in ons], [root / x / f"{n}.full.bin" for x in sks])
            sd[n] = {"differ_first_runs": d["differ_first_runs"], "unstable_on": d["unstable_on"], "unstable_skip": d["unstable_skip"],
                     "attributed": d["attributed"]}
        e["statediff"] = sd
        e["movies_on"] = []
        for sg in movie_segments(on0["bink"]):
            q = qpc_per_export({"rows": sg["rows"], "notes": on0["bink"]["notes"]})
            waits = [r for r in sg["rows"] if r[2] == "_BinkWait@4"]
            copies = [r for r in sg["rows"] if r[2] == "_BinkCopyToBuffer@28"]
            e["movies_on"].append({
                "name": sg["name"], "open_hdr": sg["open_hdr"], "close_hdr": sg.get("close_hdr"), "calls": sg["calls"],
                "virtual_ms_q": sg.get("virtual_ms_q"), "virtual_ms_t": sg.get("virtual_ms_t"), "ended_by": sg.get("ended_by"),
                "wait_nonzero_calls": sum(r[5][0] for r in waits if r[4] != 0), "wait_zero_calls": sum(r[5][0] for r in waits if r[4] == 0),
                "qpc_steps_per_export": q, "copy_flag_values": sorted({r[3][6] for r in copies}),
                "open_args": sg["rows"][0][3], "first_rows": sg["rows"][:14], "last_rows": sg["rows"][-9:]})
        e["movies_skip"] = [{"name": sg["name"], "calls": sg["calls"], "ended_by": sg.get("ended_by"),
                             "open_returned_null": sg.get("open_returned_null", False)} for sg in movie_segments(sk0["bink"])]
        e["bink_notes"] = [n for n in on0["bink"]["notes"] if "missing" in n or "offthread" in n]
        out["scenarios"][scen] = e
    tr = {}
    tdir = ROOT / "port" / "corpus" / "traces" / "s8_2"
    for f in sorted(tdir.glob("*.trace.json")) if tdir.exists() else []:
        j = json.loads(f.read_text())
        tr[f.name[:-11]] = {k: j.get(k) for k in ("sha256", "size", "validate", "byte_identical", "same_as_source")}
        if j.get("summary"):
            tr[f.name[:-11]]["clock_calls"] = j["summary"].get("clock_calls")
            tr[f.name[:-11]]["records"] = j["summary"].get("records")
        if j.get("stats"):
            rep = (j["stats"].get("stats") or {}).get("replay") if isinstance(j["stats"], dict) else None
            if rep:
                tr[f.name[:-11]]["replay"] = rep
    out["traces"] = tr
    return out


def analyze_main(rest: list[str]) -> int:
    ap = argparse.ArgumentParser(prog="s8_movie_runs.py analyze")
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("identity", help="runs of one condition against the first")
    p.add_argument("--scenario", required=True)
    p.add_argument("labels", nargs="+")
    p = sp.add_parser("movies", help="per-movie binkw32 call summary of one run")
    p.add_argument("--scenario", required=True)
    p.add_argument("label")
    p.add_argument("--rows", type=int, default=14, help="first / last rows to print per movie")
    p = sp.add_parser("report", help="write the machine-readable result file (log/roadmap/S8/2_results.json)")
    p.add_argument("--out", default=str(ROOT / "log" / "roadmap" / "S8" / "2_results.json"))
    p = sp.add_parser("clockdiff", help="main-thread clock calls per (kind, module, caller) of two runs")
    p.add_argument("--scenario", required=True)
    p.add_argument("on")
    p.add_argument("skip")
    p = sp.add_parser("statediff", help="full-image diff, runs of each condition, at every common checkpoint")
    p.add_argument("--scenario", required=True)
    p.add_argument("--on", nargs="+", required=True)
    p.add_argument("--skip", nargs="+", required=True)
    p.add_argument("--json")
    a = ap.parse_args(rest)
    if a.cmd == "identity":
        rows = run_identity(a.scenario, a.labels)
        for lab, r in rows.items():
            print(f"{lab:10s} clock {r['clock']} exit {r['exit_code']} polls {r['polls']} identical_to_{a.labels[0]}={r['identical_to_first']}")
        f = rows[a.labels[0]]
        for name, cp in f["checkpoints"].items():
            print(f"  {name:12s} T={cp['T']:8d} tgt={cp['tgt']:7d} qpc={cp['qpc']:8d} state {cp['state_sha']} frame {cp['frame_sha']} objhash {cp['objhash']}")
        return 0 if all(r["identical_to_first"] for r in rows.values()) else 1
    if a.cmd == "movies":
        r = _load(a.scenario, a.label)
        for sg in movie_segments(r["bink"]):
            print(f"movie {sg['name']} header {sg['open_hdr']} (Width Height Frames FrameNum LastFrameNum FrameRate FrameRateDiv ReadError)")
            print(f"  calls {sg['calls']}")
            print(f"  Q open {sg['q_open']} close {sg.get('q_close')} -> {sg.get('virtual_ms_q')} virtual ms by the QPC clock; "
                  f"timeGetTime clock T open {sg['t_open']} close {sg.get('t_close')} ({sg.get('virtual_ms_t')} ms); {sg.get('ended_by')}; close hdr {sg.get('close_hdr')}")
            for row in sg["rows"][:a.rows]:
                print("   ", row)
            print("    ...")
            for row in sg["rows"][-a.rows:]:
                print("   ", row)
        return 0
    if a.cmd == "report":
        res = build_report()
        pathlib.Path(a.out).write_text(json.dumps(res, indent=1) + "\n")
        print(f"wrote {a.out}")
        return 0
    if a.cmd == "clockdiff":
        on, sk = _load(a.scenario, a.on), _load(a.scenario, a.skip)
        ca, cb = clock_by_site(on["clock"]), clock_by_site(sk["clock"])
        print(f"clock total on {on['clock']['total']} skip {sk['clock']['total']} (limits {on['clock']['limit']} / {sk['clock']['limit']})")
        for k in sorted(set(ca) | set(cb), key=lambda k: -(ca.get(k, 0) - cb.get(k, 0))):
            if ca.get(k, 0) != cb.get(k, 0):
                print(f"  {k[0]:4s} {k[1]:14s} {k[2]:22s} on {ca.get(k, 0):7d} skip {cb.get(k, 0):7d} added {ca.get(k, 0) - cb.get(k, 0):7d}")
        return 0
    if a.cmd == "statediff":
        root = ANALYZE_ROOT / a.scenario
        res = {}
        names = list(json.loads((root / a.on[0] / "summary.json").read_text())["checkpoints"])
        for n in names:
            d = image_diff([root / x / f"{n}.full.bin" for x in a.on], [root / x / f"{n}.full.bin" for x in a.skip])
            res[n] = d
            print(f"{n:12s} differ {d['differ_first_runs']} dwords; unstable (heap addresses) on {d['unstable_on']} skip {d['unstable_skip']}; "
                  f"attributed to the intro {len(d['attributed'])}")
            for x in d["attributed"]:
                print(f"    {x['class']:8s} {x['symbol']:22s} {x['va']} skip {x['skip']} on {x['on']} delta {x['delta']} "
                      f"pointer_flag={x['pointer_flag']} heap_pointer_like={x['heap_pointer_like']}")
        if a.json:
            pathlib.Path(a.json).write_text(json.dumps(res, indent=1))
        return 0
    return 2


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--skip-intro", type=int, default=0, choices=[0, 1])
    ap.add_argument("--bink-log", action="store_true")
    ap.add_argument("tool", choices=["ref", "trace", "analyze"])
    ap.add_argument("rest", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    if a.tool == "analyze":
        return analyze_main(a.rest)
    R = patch_refs(a.skip_intro, a.bink_log)
    sys.argv = [sys.argv[0]] + a.rest
    if a.tool == "ref":
        return R.main()
    import trace_tool as TT
    if os.environ.get("S8_REPLAY_EXIT") == "1":
        # trace_tool's replay script never sets exit_expected, so a replay of an exiting scenario ends its trace at the last
        # poll, before the Esc / ExitProcess tail. With S8_REPLAY_EXIT=1 the replayed scenario expects the exit too.
        orig_install = TT.install

        def install(S, scenario, mode, state):
            orig_install(S, scenario, mode, state)
            if mode == "replay":
                fn = S.SCENARIOS[scenario]

                def replay_exit(c):
                    c.run.exit_expected = True
                    yield from fn(c)
                    for _ in range(300):          # keep answering polls until the game's own ExitProcess ends the trace
                        yield
                S.SCENARIOS[scenario] = replay_exit
        TT.install = install
    return TT.main()


if __name__ == "__main__":
    sys.exit(main())
