r"""Phase 3.8 integrator gates (stream 38f, re/PHASE38_PLAN.md section 3; notes log/p38/38f_notes.md).

    ~/bin/py -3.12 -m port.test.integrator segments [--backend x64|win32] [--jobs 8] [--cls all] [--out <tsv>]
    ~/bin/py -3.12 -m port.test.integrator chain [--scenario course_season ...|--all] [--final-only] [--out <tsv>]
    ~/bin/py -3.12 -m port.test.integrator speed [--frames 8192] [--segment course_season/09] [--backend ...]
    ~/bin/py -3.12 -m port.test.integrator ledger [--x64 <segments tsv>] [--out re/port/divergences.tsv]
    ~/bin/py -3.12 -m port.test.integrator compare <tsv a> <tsv b>

Library: --dll <portsim_c.dll> (x64), --dll32 <portsim_c.dll> --runner32 <portsim_runner32.exe> (Win32), or --build 38f
(port/build/38f_run/latest.json, written by log/p38/f_mirror.py); default $SG_PORTSIM_DLL / port/build/<arch> as lib.load().

segments: every accepted segment of re/port/segments_manifest.tsv through sg_run_frames, as port/test/segments.py's
SegmentRunner judges it (load check, every frame record, the end checkpoint's sim hashes; NON_CAUSAL objects reported apart),
plus a DIGEST per segment: sha256 over the return code, the frame count and every returned (T, 203 hashes) record, and the
list of end-check objects that differ. Two backends that give the same digest for a segment computed the same frames.

chain (gate c): the accepted segments of a scenario whose windows touch (segment k+1 starts at the checkpoint segment k ends
at, and its first frame head is the one after segment k's last record) form a CHAIN. For every checkpoint inside the chain,
one sg_run_frames call starts from the FIRST segment's image and runs through a slice of the scenario's whole trace
(port/corpus/traces/<scenario>.trace, cut by re/tools/segment_corpus.py's slice_trace from the first segment's start frame
head to that checkpoint's POLL): no restart in between. Every frame record of the run is compared with the segments'
frames.bin records concatenated, and the state at the checkpoint with that checkpoint's sim hashes (checkpoints.json). The
run for the chain's last checkpoint covers every frame record of the chain. Self-check: the slice from segment k's own start
to its end checkpoint equals segment k's trace.trace byte for byte (`check-slices`).

speed (gate f): a synthetic trace with no input record (one frame-head timeGetTime call and 98 more clock calls per frame,
so frame heads are 99 virtual ms apart, as a normal frame of the original under the virtual clock: time_model.md section 3)
from a segment's start image; reports frames run, seconds and frames per second. The budget is frames x 87 ms (the
original's frame wait, H1).
"""
from __future__ import annotations

import argparse
import concurrent.futures as CF
import csv
import dataclasses
import hashlib
import json
import os
import pathlib
import struct
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
for _p in (ROOT / "re" / "tools",):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

from port.test import build as B  # noqa: E402
from port.test import lib as L  # noqa: E402
from port.test import segments as S  # noqa: E402

TRACES = ROOT / "port" / "corpus" / "traces"
OUT = ROOT / "log" / "p38" / "38f"
LEDGER = ROOT / "re" / "port" / "divergences.tsv"
LATEST = ROOT / "port" / "build" / "38f_run" / "latest.json"
FRAME_WAIT_MS = 87          # H1's frame wait: frame-head time + 87 ms (re/analysis/sim/time_model.md)


# ------------------------------------------------------------------------------------------- library
@dataclasses.dataclass
class LibSpec:
    backend: str = "x64"
    dll: str | None = None          # x64 DLL
    dll32: str | None = None        # Win32 DLL
    runner32: str | None = None     # Win32 runner

    @classmethod
    def from_args(cls, a) -> "LibSpec":
        sp = cls(backend=a.backend or L.backend(), dll=a.dll, dll32=a.dll32, runner32=a.runner32)
        if a.build:
            src = ROOT / "port" / "build" / f"{a.build}_run" / "latest.json" if a.build != "38f" else LATEST
            lt = json.loads(src.read_text())
            sp.dll = sp.dll or lt.get("x64", {}).get("portsim_c.dll")
            sp.dll32 = sp.dll32 or lt.get("Win32", {}).get("portsim_c.dll")
            sp.runner32 = sp.runner32 or lt.get("Win32", {}).get("portsim_runner32.exe")
        return sp

    def describe(self) -> str:
        if self.backend == "win32":
            return f"win32 dll={self.dll32 or B.dll_path('Win32')} runner={self.runner32 or B.runner32_path()}"
        return f"x64 dll={self.dll or os.environ.get('SG_PORTSIM_DLL') or B.dll_path('x64')}"


_LIBS: dict = {}


def load_lib(spec: LibSpec):
    """The library of `spec` (one per process). Win32 with explicit paths: the runner and DLL given, never rebuilt (the
    shared port/build/Win32 tree is not touched)."""
    key = dataclasses.astuple(spec)
    if key in _LIBS:
        return _LIBS[key]
    if spec.backend == "win32":
        from port.test import lib32
        if spec.dll32 or spec.runner32:
            dll32 = pathlib.Path(spec.dll32 or B.dll_path("Win32"))
            run32 = pathlib.Path(spec.runner32 or B.runner32_path())
            orig_dll = B.dll_path
            B.runner32_path = lambda config="Release": run32
            B.dll_path = lambda arch=None, config="Release": dll32 if (arch or B.default_arch()) == "Win32" else orig_dll(arch, config)
            B.runner32_stale = lambda config="Release": not (run32.exists() and dll32.exists())
            lib = lib32.Lib32(rebuild=False)
        else:
            lib = lib32.load32()
    else:
        lib = L.load(spec.dll) if spec.dll else L.load()
    _LIBS[key] = lib
    return lib


def run_detail(lib, st) -> str:
    dll = getattr(lib, "dll", None)
    if dll is None:
        return ""
    import ctypes as C
    f = dll.sg_run_frames_detail
    f.restype = C.c_char_p
    f.argtypes = [C.c_void_p]
    try:
        return (f(st.ctx) or b"").decode("utf-8", "replace")
    except Exception:  # noqa: BLE001
        return ""


# ------------------------------------------------------------------------------------------- one run
@dataclasses.dataclass
class RunResult:
    name: str
    status: str                 # PASS / FAIL / LOAD_FAILED / CHAIN_INVALID / ERROR
    frames_expected: int = 0
    frames_run: int = 0
    code: int = 0
    record: str = ""            # first divergent record ("end" for the end check)
    objects: str = ""
    noncausal: str = ""
    detail: str = ""
    run_detail: str = ""
    digest: str = ""
    seconds: float = 0.0


def digest_of(code: int, n: int, ts: list, hs: list, end_diff: list) -> str:
    h = hashlib.sha256(struct.pack("<ii", code, n))
    for k in range(n):
        h.update(struct.pack("<I", ts[k]))
        h.update(struct.pack(f"<{len(hs[k])}Q", *hs[k]))
    h.update(",".join(end_diff).encode())
    return h.hexdigest()[:16]


def run_from(lib, name: str, start: S.Segment, trace: bytes, expected: list, end_hashes: dict | None,
             prepare=None, after=None) -> RunResult:
    """sg_run_frames from `start`'s image over `trace`, compared with `expected` records [(T, index, [hash])] (record 0 = the
    start) and, at the end, with `end_hashes` {symbol: [class, hex]} (sim class compared; None: no end check).
    prepare(st) runs after the load check and before sg_run_frames, after(st, res) right after it (port/test/semantic.py)."""
    runner = S.SegmentRunner(lib)
    res = RunResult(name, "LOAD_FAILED", frames_expected=len(expected) - 1)
    t0 = time.time()
    with lib.state() as st:
        st.set_data_base(S.DATA_VA)
        if S.INSTALL.is_dir():
            st.files_root(S.INSTALL)
        # the same set-up as SegmentRunner.run (port/test/segments.py after 38a / 38b's merges): the files the recording's
        # install held beyond original/, and the host GDI TextMetrics provider unless SG_TEXT_METRICS=none
        overlay = getattr(S, "segment_files", None)
        if overlay is not None:
            for fname, data in overlay.files_for(start.scenario).items():
                st.files_put(fname, data)
        gdi = getattr(runner, "text_metrics", "none") == "gdi" and lib.has("gditext.attach")
        if gdi:
            st.call("gditext.attach", [])
        try:
            return _run_loaded(lib, runner, res, t0, st, start, trace, expected, end_hashes, prepare, after)
        finally:
            if gdi:
                st.call("gditext.detach", [])


def _run_loaded(lib, runner, res, t0, st, start, trace, expected, end_hashes, prepare, after) -> RunResult:
    """run_from after the set-up: load check, sg_run_frames, comparison."""
    why = runner.load_check(start, st)
    if why:
        res.detail = why
        res.seconds = round(time.time() - t0, 2)
        return res
    if prepare is not None:
        prepare(st)
    code, n, ts, hs = st.run_frames(trace, start.queued, res.frames_expected)
    res.code, res.frames_run = code, n
    res.run_detail = run_detail(lib, st).replace("\t", " ")
    if after is not None:
        after(st, res)
    names = [s for s, _, _ in runner.sim]
    end_diff = []
    if code == L.SG_OK and end_hashes is not None:
        got = dict(zip(names, st.hash_class("sim")))
        end_diff = [s for s, (cls, h) in end_hashes.items() if cls == "sim" and int(h, 16) != got.get(s)]
    res.digest = digest_of(code, n, ts, hs, end_diff)
    res.seconds = round(time.time() - t0, 2)
    if code != L.SG_OK:
        res.status = "FAIL"
        res.detail = f"sg_run_frames returned {code} ({L.ERRORS.get(code, '?')})"
        return res
    got = [expected[0]] + [(ts[k], 0, hs[k]) for k in range(n)]
    div = S.first_divergent_record(expected, got, start.symbols, ignore=S.NON_CAUSAL)
    res.noncausal = S.noncausal_differences(expected, got, start.symbols)
    if div is None:
        nc_end = [s for s in end_diff if s in S.NON_CAUSAL]
        end = [s for s in end_diff if s not in S.NON_CAUSAL]
        if nc_end:
            res.noncausal = ",".join(filter(None, [res.noncausal] + [f"{s}@end" for s in nc_end]))
        if end:
            div = {"record": "end", "objects": end}
    if div is None:
        res.status = "PASS"
    else:
        res.status = "FAIL"
        res.record = str(div.get("record"))
        res.objects = ",".join(div.get("objects", []))[:400]
        res.detail = json.dumps(div)[:300]
    return res


# ------------------------------------------------------------------------------------------- segments
def accepted_rows(cls: str = "all") -> list[dict]:
    return [r for r in S.manifest_rows() if r["status"] == "accepted" and (cls == "all" or r["class"] == cls)]


def seg_name(r: dict) -> str:
    return f"{r['scenario']}/{int(r['segment']):02d}"


def _run_segment(spec: LibSpec, name: str, defects_off: tuple = ()) -> dict:
    lib = load_lib(spec)
    scen, k = name.split("/")
    seg = S.load_segment(S.SEG_DIR / scen / k)
    end = list(seg.checkpoints.values())[-1]["hashes"] if seg.checkpoints else None
    for d in defects_off:
        lib.defect(d, False)
    try:
        res = run_from(lib, name, seg, seg.trace, seg.records, end)
    except Exception as e:  # noqa: BLE001
        res = RunResult(name, "ERROR", detail=repr(e)[:300])
    finally:
        for d in defects_off:
            lib.defect(d, True)
    return dataclasses.asdict(res)


def cmd_defects(a) -> int:
    """Every compatibility switch (lib.defects(), default 1 = the original's behaviour) turned OFF in turn over every
    accepted segment; a segment whose digest changes is one where the switch changes the result (gate e). Compared with
    the switch-ON run of the same library in the same pass."""
    spec = LibSpec.from_args(a)
    lib = load_lib(spec)
    names = [seg_name(r) for r in accepted_rows(a.cls)]
    switches = a.switch or list(lib.defects())
    rows = []
    with CF.ProcessPoolExecutor(a.jobs) as ex:
        base = {r["name"]: r for r in ex.map(_run_segment, [spec] * len(names), names)}
        print(f"baseline: {sum(1 for r in base.values() if r['status'] == 'PASS')} PASS of {len(base)}", flush=True)
        for sw in switches:
            got = list(ex.map(_run_segment, [spec] * len(names), names, [(sw,)] * len(names)))
            changed = [g for g in got if g["digest"] != base[g["name"]]["digest"]]
            flips = [f"{g['name']}:{base[g['name']]['status']}->{g['status']}@{g['record'] or '-'}" for g in changed
                     if g["status"] != base[g["name"]]["status"]]
            row = {"switch": sw, "module": lib.defects()[sw]["module"], "ref": lib.defects()[sw]["ref"],
                   "segments_changed": len(changed), "status_flips": len(flips),
                   "changed": ",".join(g["name"] for g in changed[:12]) + (" ..." if len(changed) > 12 else ""),
                   "flips": ",".join(flips[:12]) + (" ..." if len(flips) > 12 else ""),
                   "what": lib.defects()[sw]["what"]}
            rows.append(row)
            print(json.dumps(row), flush=True)
    out = pathlib.Path(a.out or OUT / f"defects_{spec.backend}.tsv")
    write_tsv(out, rows, ["switch", "module", "ref", "segments_changed", "status_flips", "changed", "flips", "what"])
    print(f"-> {out}")
    return 0


COLS = ["name", "status", "frames_run", "frames_expected", "code", "record", "objects", "noncausal", "digest", "seconds",
        "run_detail", "detail"]


def write_tsv(path: pathlib.Path, rows: list[dict], cols=COLS):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as fh:
        fh.write("\t".join(cols) + "\n")
        for r in rows:
            fh.write("\t".join(str(r.get(c, "")).replace("\t", " ").replace("\n", " ") for c in cols) + "\n")


def read_tsv(path: pathlib.Path) -> list[dict]:
    with pathlib.Path(path).open(encoding="utf-8", newline="") as fh:
        return list(csv.DictReader(fh, delimiter="\t"))


def cmd_segments(a) -> int:
    spec = LibSpec.from_args(a)
    names = [seg_name(r) for r in accepted_rows(a.cls)]
    if a.only:
        want = set(a.only.split(","))
        names = [n for n in names if n in want or n.split("/")[0] in want]
    out = pathlib.Path(a.out or OUT / f"segments_{spec.backend}.tsv")
    print(f"library: {spec.describe()}; {len(names)} segments -> {out}", flush=True)
    rows = []
    t0 = time.time()
    with CF.ProcessPoolExecutor(a.jobs) as ex:
        futs = {ex.submit(_run_segment, spec, n): n for n in names}
        for i, f in enumerate(CF.as_completed(futs), 1):
            try:
                r = f.result()
            except Exception as e:  # noqa: BLE001
                r = dataclasses.asdict(RunResult(futs[f], "ERROR", detail=repr(e)[:300]))
            rows.append(r)
            print(f"[{i}/{len(names)}] {r['name']} {r['status']} {r['frames_run']}/{r['frames_expected']} "
                  f"rec={r['record']} {r['objects'][:60]} digest={r['digest']} {r['seconds']}s | {r['run_detail'][:120]}",
                  flush=True)
    rows.sort(key=lambda r: r["name"])
    write_tsv(out, rows)
    counts = {}
    for r in rows:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    print(json.dumps({"backend": spec.backend, "counts": counts, "wall_s": round(time.time() - t0, 1)}), flush=True)
    return 0


# ------------------------------------------------------------------------------------------- chains
def chains_of(scenario: str, breaks=frozenset()) -> list[list[dict]]:
    """The scenario's accepted segments grouped into maximal chains (each a list of manifest rows, in order). `breaks`: segment
    indices that start a new chain although their window touches the previous one (a mainLoop exit at that boundary)."""
    rows = sorted((r for r in accepted_rows() if r["scenario"] == scenario), key=lambda r: int(r["segment"]))
    out: list[list[dict]] = []
    for r in rows:
        if out and int(r["segment"]) not in breaks:
            p = out[-1][-1]
            if (r["start_checkpoint"] == p["end_checkpoint"]
                    and int(r["start_frame"]) == int(p["start_frame"]) + int(p["frames"]) + 1):
                out[-1].append(r)
                continue
        out.append([r])
    return out


def chain_scenarios() -> list[str]:
    seen = []
    for r in accepted_rows():
        if r["scenario"] not in seen:
            seen.append(r["scenario"])
    return seen


_TRACES: dict = {}


def full_trace(scenario: str):
    import segment_corpus as SC
    if scenario not in _TRACES:
        p = SC.trace_path(scenario)
        if p is None:
            raise FileNotFoundError(f"no trace for {scenario}")
        _TRACES[scenario] = SC.read_trace(p)
    return _TRACES[scenario]


def chain_slice(scenario: str, start_t: int, checkpoint: str) -> tuple[bytes, dict]:
    import segment_corpus as SC
    data, info = SC.slice_trace(full_trace(scenario), start_t, checkpoint)
    data, info["dropped_poll_writes"] = drop_final_poll_writes(data)
    return data, info


def drop_final_poll_writes(data: bytes) -> tuple[bytes, int]:
    """The slice without the harness memory writes (DELIVER w32) made in the final POLL's own clock call. The original takes
    a poll's checkpoint dump before it applies that poll's actions (re/frida/js/scenario_det.js: `reply.dump` is sent, then
    `reply.actions` run), so those writes come after the checkpoint the slice ends at; the frame step would apply them at its
    frame wait, before the end comparison. Measured on cov_k3_camera2 checkpoint y_inj0 (inside segment 02): the poll writes
    gCameraTileY = 54, the original's checkpoint holds 3. Returns (slice, writes dropped)."""
    import trace_format as TF
    tr = TF.parse(data)
    fin = next((r for r in tr.records if r.type == TF.POLL and r.f["final"]), None)
    if fin is None:
        return data, 0
    keep, dropped, seen_final = [], 0, False
    for r in tr.records:
        if r is fin:
            seen_final = True
        if (not seen_final and r.type == TF.DELIVER and r.sub == 4 and r.f["pos"] == fin.f["pos"]):
            dropped += 1
            continue
        keep.append(r)
    if not dropped:
        return data, 0
    hsize, mlen = 32, struct.unpack_from("<I", data, 12)[0]
    body = b"".join(TF.rec(r.type, r.sub, r.payload) for r in keep)
    return data[:hsize + mlen] + body, dropped


def frame_ts(trace: bytes) -> list[int]:
    import trace_format as TF
    return [r.f["t"] for r in TF.parse(trace).records if r.type == TF.FRAME]


def chain_plan(chain: list[dict], final_only: bool = False) -> dict:
    """The segments loaded, the concatenated expected records and the checkpoint list [(checkpoint, segment index)]."""
    segs = [S.load_segment(S.segment_dir(r)) for r in chain]
    records = []
    for s in segs:
        records += s.records
    cps = [(c, k) for k, s in enumerate(segs) for c in s.checkpoints]
    if final_only:
        cps = cps[-1:]
    return {"segments": segs, "records": records, "checkpoints": cps,
            "start_t": segs[0].start_json["start"]["frame_value"]}


def chain_name(chain: list[dict]) -> str:
    a, b = int(chain[0]["segment"]), int(chain[-1]["segment"])
    return f"{chain[0]['scenario']}/{a:02d}-{b:02d}"


def _run_chain_checkpoint(spec: LibSpec, scenario: str, seg_ids: list[int], checkpoint: str, final_only: bool) -> dict:
    lib = load_lib(spec)
    by = {int(r["segment"]): r for r in accepted_rows() if r["scenario"] == scenario}
    chain = [by[i] for i in seg_ids]
    plan = chain_plan(chain)
    segs, records = plan["segments"], plan["records"]
    k = dict(plan["checkpoints"])[checkpoint]
    name = f"{chain_name(chain)}@{checkpoint}"
    try:
        data, info = chain_slice(scenario, plan["start_t"], checkpoint)
    except Exception as e:  # noqa: BLE001
        return dataclasses.asdict(RunResult(name, "CHAIN_INVALID", detail=f"slice: {e}"[:300]))
    nfr = info["frames"]
    exp = records[:nfr]
    ts = frame_ts(data)
    if len(exp) != nfr or [r[0] for r in exp] != ts:
        bad = next((i for i in range(min(len(ts), len(exp))) if ts[i] != exp[i][0]), min(len(ts), len(exp)))
        return dataclasses.asdict(RunResult(name, "CHAIN_INVALID", frames_expected=nfr - 1,
                                            detail=f"slice FRAME T != concatenated records at record {bad} "
                                                   f"({len(ts)} FRAME records, {len(exp)} expected)"))
    try:
        res = run_from(lib, name, segs[0], data, exp, segs[k].checkpoints[checkpoint]["hashes"])
    except Exception as e:  # noqa: BLE001
        res = RunResult(name, "ERROR", detail=repr(e)[:300])
    d = dataclasses.asdict(res)
    d.update(scenario=scenario, chain=chain_name(chain), segments=len(chain), checkpoint=checkpoint,
             frames_in_chain=len(records) - 1, slice_clock_calls=info["clock_calls"],
             dropped_poll_writes=info.get("dropped_poll_writes", 0), boundaries=_boundaries(chain))
    return d


def _boundaries(chain: list[dict]) -> str:
    """Chain record index at which each segment after the first starts: "<segment>:<record>,..."."""
    out, acc = [], 0
    for r in chain:
        if acc:
            out.append(f"{int(r['segment'])}:{acc}")
        acc += int(r["frames"]) + 1
    return ",".join(out)


CHAIN_COLS = ["chain", "checkpoint", "status", "frames_run", "frames_expected", "frames_in_chain", "segments", "code",
              "record", "objects", "noncausal", "digest", "seconds", "slice_clock_calls", "dropped_poll_writes", "boundaries",
              "run_detail", "detail"]
MENU_EXIT = "the frame left to the title menu"
# scenarios whose script leaves mainLoop between segments on purpose (each championship round starts from the title menu:
# re/tools/scenario.py _cov_b_champ_rounds; the boundary gap is 23.3 .. 23.7 virtual s with no frame head)
MENU_ROUND_SCENARIOS = ("cov_b_shots", "cov_b_shots2")


def menu_break(r: dict) -> int | None:
    """The segment a chain run stopped in front of when it ended by leaving mainLoop for the title menu exactly at that
    segment's first frame head (sg_run_frames_detail MENU_EXIT): the original left mainLoop there too (the next segment
    starts after the title menu and a new game's set-up, which is not frame-step code), so the chain breaks there."""
    if not (r.get("status") == "FAIL" and str(r.get("run_detail", "")).startswith(MENU_EXIT) and r.get("boundaries")):
        return None
    for item in str(r["boundaries"]).split(","):
        seg, rec = item.split(":")
        if str(rec) == str(r.get("record")):
            return int(seg)
    return None


def cmd_chain(a) -> int:
    """Gate (c). Chains of every scenario asked for; a run that stops by a mainLoop exit at a segment boundary marks a BREAK
    there, and the chain's later checkpoints are run again from the next segment's image (until a pass finds no new break)."""
    spec = LibSpec.from_args(a)
    scens = chain_scenarios() if a.all else (a.scenario or ["course_season", "golfer_stories", "championship",
                                                             "stories_fixture", "stories9_fixture", "mood5_fixture"])
    breaks: dict[str, set] = {sc: set() for sc in scens}
    done: dict[tuple, dict] = {}
    out = pathlib.Path(a.out or OUT / f"chain_{spec.backend}.tsv")
    t0 = time.time()
    with CF.ProcessPoolExecutor(a.jobs) as ex:
        while True:
            jobs = []
            for sc in scens:
                for ch in chains_of(sc, frozenset(breaks[sc])):
                    if a.min_segments and len(ch) < a.min_segments and not breaks[sc]:
                        continue
                    plan = chain_plan(ch, a.final_only)
                    ids = tuple(int(r["segment"]) for r in ch)
                    for cp, _k in plan["checkpoints"]:
                        if (sc, ids, cp) not in done:
                            jobs.append((sc, list(ids), cp))
            if not jobs:
                break
            print(f"library: {spec.describe()}; {len(jobs)} chain runs (breaks so far: "
                  f"{ {k: sorted(v) for k, v in breaks.items() if v} })", flush=True)
            futs = {ex.submit(_run_chain_checkpoint, spec, sc, ids, cp, a.final_only): (sc, ids, cp) for sc, ids, cp in jobs}
            new_break = False
            for i, f in enumerate(CF.as_completed(futs), 1):
                sc, ids, cp = futs[f]
                try:
                    r = f.result()
                except Exception as e:  # noqa: BLE001
                    r = dataclasses.asdict(RunResult(f"{sc}@{cp}", "ERROR", detail=repr(e)[:300]))
                    r.update(chain=sc, checkpoint=cp)
                seg = menu_break(r)
                if seg is not None:
                    r["status"] = "BREAK"
                    r["detail"] = f"mainLoop exits to the title menu at segment {seg}'s first frame head (record {r['record']})"
                    if seg not in breaks[sc]:
                        breaks[sc].add(seg)
                        new_break = True
                done[(sc, tuple(ids), cp)] = r
                print(f"[{i}/{len(jobs)}] {r.get('chain')}@{r.get('checkpoint')} {r['status']} "
                      f"{r['frames_run']}/{r['frames_expected']} rec={r['record']} {r['objects'][:60]} {r['seconds']}s | "
                      f"{(r['run_detail'] or r['detail'])[:140]}", flush=True)
            if not new_break:
                break
    rows = sorted(done.values(), key=lambda r: (r.get("chain", ""), int(r.get("frames_expected") or 0)))
    write_tsv(out, rows, CHAIN_COLS)
    counts = {}
    for r in rows:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    print(json.dumps({"backend": spec.backend, "counts": counts, "breaks": {k: sorted(v) for k, v in breaks.items() if v},
                      "wall_s": round(time.time() - t0, 1)}), flush=True)
    return 0


def cmd_check_slices(a) -> int:
    """slice_trace(whole trace, segment start, end checkpoint) == the segment's trace.trace, for every accepted segment of
    the scenarios given (the chain runner's slices come from the same function)."""
    import segment_corpus as SC
    scens = a.scenario or chain_scenarios()
    bad = 0
    n = 0
    for sc in scens:
        for r in [r for r in accepted_rows() if r["scenario"] == sc]:
            seg = S.load_segment(S.segment_dir(r))
            try:
                data, _ = SC.slice_trace(full_trace(sc), seg.start_json["start"]["frame_value"], r["end_checkpoint"],
                                         {"scenario": sc, "segment": int(r["segment"])})
            except Exception as e:  # noqa: BLE001
                print(f"{seg.name}: slice failed: {e}")
                bad += 1
                continue
            n += 1
            if data != seg.trace:
                bad += 1
                print(f"{seg.name}: slice differs from trace.trace ({len(data)} vs {len(seg.trace)} bytes)")
    print(f"{n} slices compared, {bad} differ or failed")
    return 1 if bad else 0


# ------------------------------------------------------------------------------------------- speed
INPUT_TYPES = None   # filled by _input_types(): the record types the corpus counts as input (segment_corpus.INPUT_TYPES)


def _input_types():
    import segment_corpus as SC
    import trace_format as TF
    # input deliveries and what only input produces (segment_corpus.md section 3), plus timer records and SLEEP
    return set(SC.INPUT_TYPES) | {TF.KEYSTATE, TF.MAPVK, TF.TIMERSET, TF.SLEEP}


def template_frame(trace: bytes) -> tuple[list, dict]:
    """The records of one frame of `trace` (between two FRAME records, the FRAME records excluded, POLL records dropped)
    that holds no input record and whose shape (record types, call sites, counts) is the most frequent among such frames:
    a normal frame of the original with no input. Returns (records, info)."""
    import collections
    import trace_format as TF
    tr = TF.parse(trace)
    bad = _input_types()
    fi = [i for i, r in enumerate(tr.records) if r.type == TF.FRAME]
    shapes = collections.Counter()
    first = {}
    for a, b in zip(fi, fi[1:]):
        body = [r for r in tr.records[a + 1:b] if r.type != TF.POLL]
        if any(r.type in bad for r in body):
            continue
        key = tuple((r.type, r.sub, r.f.get("ra"), r.f.get("count"), r.f.get("ok"),
                     r.f.get("x"), r.f.get("y"), tuple(r.f.get("keys") or ())) for r in body)
        shapes[key] += 1
        first.setdefault(key, (a, body))
    if not shapes:
        raise ValueError("no frame without input in the trace")
    key, count = shapes.most_common(1)[0]
    a, body = first[key]
    tgt = sum(r.f["count"] for r in body if r.type == TF.CLOCKRUN and r.f["kind"] == 1) + 1
    info = {"template_frame": tr.records[a].f["frame"], "frames_with_this_shape": count, "input_free_frames": sum(shapes.values()),
            "frames_in_trace": len(fi), "tgt_calls_per_frame": tgt,
            "records": collections.Counter(TF.TYPE_NAMES[r.type] for r in body)}
    return body, {**info, "records": dict(info["records"])}


def synthetic_trace(seg_trace: bytes, t0: int, frames: int, source_meta: dict | None = None) -> tuple[bytes, dict]:
    """A trace with no input record: frames + 1 copies of template_frame(seg_trace) (each after its frame-head timeGetTime
    call and FRAME record), on the segment trace's own virtual clock (step, start values t0 / q0 of its header), with
    positions and values rebased; one final POLL at the last frame head, where the run ends. The cursor and key-state reads of the
    template are repeated as they are (the cursor where the harness left it, every key up)."""
    import trace_format as TF
    src = TF.parse(seg_trace)
    body, info = template_frame(seg_trace)
    meta = {"frame_ra": TF.FRAME_RA, "schema": 1, "spec": "re/model/trace_schema.md",
            "synthetic": "port/test/integrator.py speed: the segment's most frequent input-free frame repeated",
            **({"source": source_meta} if source_meta else {})}
    out = bytearray()
    n = tcount = qcount = 0
    step, qstep = src.step, src.qstep
    for k in range(frames + 1):
        v = t0 + tcount * step
        out += TF.rec(TF.CLOCKRUN, 1, struct.pack("<IIIIQ", n, 1, TF.FRAME_RA, 4, v))
        n += 1
        tcount += 1
        if k == frames:      # the last frame head: the final POLL rides on its clock call (the run ends there)
            out += TF.rec(TF.POLL, 2, struct.pack("<IIB", n, 0, 0))
        out += TF.rec(TF.FRAME, 0, struct.pack("<III", k, n, v) + bytes(TF.SNAP_LEN))
        if k == frames:
            break
        for r in body:
            f = r.f
            if r.type == TF.CLOCKRUN:
                if f["kind"] == 1:
                    v0 = t0 + tcount * step
                    tcount += f["count"]
                else:
                    v0 = src.q0 + qcount * qstep
                    qcount += f["count"]
                out += TF.rec(TF.CLOCKRUN, f["kind"], struct.pack("<IIIIQ", n, f["count"], f["ra"], f["slot"], v0))
                n += f["count"]
            else:
                p = bytearray(r.payload)
                struct.pack_into("<I", p, 0, n)       # every other record kept here starts with its u32 pos
                out += TF.rec(r.type, r.sub, bytes(p))
    out += TF.rec(TF.END, 0, struct.pack("<IIII", n, frames + 1, 1, 0))
    data = TF.header_bytes(meta, step, t0, qstep, src.q0) + bytes(out)
    errs = TF.validate(TF.parse(data))
    if errs:
        raise ValueError(f"synthetic trace invalid: {errs[:3]}")
    return data, info


def game_date(lib, image: bytes) -> dict:
    """g_date of a GameState image and its calendar reading (appendDate: year = date / 8192 + 2001, month index =
    date / 1024 % 8; re/analysis/sim/time_model.md)."""
    off = {n: o for n, o, _ in lib.objects(None)}["g_date"]
    d = struct.unpack_from("<i", image, off)[0]
    return {"date": d, "year": d // 8192 + 2001, "month_index": d // 1024 % 8, "years": round(d / 8192, 3)}


def _speed(spec: LibSpec, segname: str, frames: int) -> dict:
    lib = load_lib(spec)
    scen, k = segname.split("/")
    seg = S.load_segment(S.SEG_DIR / scen / k)
    t0 = seg.start_json["start"]["frame_value"]
    trace, tinfo = synthetic_trace(seg.trace, t0, frames, source_meta={"segment": segname})
    runner = S.SegmentRunner(lib)
    with lib.state() as st:
        st.set_data_base(S.DATA_VA)
        if S.INSTALL.is_dir():
            st.files_root(S.INSTALL)
        why = runner.load_check(seg, st)
        if why:
            return {"segment": segname, "status": "LOAD_FAILED", "detail": why}
        c0 = time.perf_counter()
        code, n, ts, hs = st.run_frames(trace, [], frames)
        dt = time.perf_counter() - c0
        det = run_detail(lib, st)
        names = [s for s, _, _ in runner.sim]
        date_at = {"start": game_date(lib, seg.start), "end": game_date(lib, st.image())}
    budget = frames * FRAME_WAIT_MS / 1000.0
    return {"segment": segname, "backend": spec.backend, "frames_requested": frames, "frames_run": n, "code": code,
            "seconds": round(dt, 3), "fps": round(n / dt, 1) if dt else 0.0, "budget_s": budget,
            "within_budget": n == frames and dt < budget, "digest": digest_of(code, n, ts, hs, []),
            "T_first": ts[0] if ts else None, "T_last": ts[-1] if ts else None, "run_detail": det[:400],
            "date_at": date_at, "sim_objects": len(names), "template": tinfo}


def cmd_speed(a) -> int:
    spec = LibSpec.from_args(a)
    r = _speed(spec, a.segment, a.frames)
    print(json.dumps(r, indent=1), flush=True)
    out = pathlib.Path(a.out or OUT / f"speed_{spec.backend}.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(r, indent=1))
    return 0 if r.get("within_budget") else 1


# ------------------------------------------------------------------------------------------- ledger (gate e)
# Why a run stops on a missing name: the name the frame step could not call (sg_run_frames_detail "missing: <name>"), what
# it needs, the uncertainty rows that record it and the phase 3.8 stream that owns it (re/PHASE38_PLAN.md section 1).
MISSING_CAUSES = {
    "Text_put": ("text widths: no TextMetrics provider (GDI GetTextExtentPoint32A on the game's font)", "U-0944,U-0947", "38a"),
    "drawGolferInfoPanel": ("the golfer info panel (P6) is not ported", "", "38d"),
    "showCourseReport": ("the course report screen's effect is not ported", "", "38c"),
    "shotStyleButtonCode": ("not ported (the scope manifest missed it)", "", "38b"),
    "loadGameBrowserScreen": ("GameFiles directory listing, loadGame / loadCourse", "U-0864", "38b"),
    "showHoleStatsScreen": ("animates by wall time: needs a clock-aware InputSource", "U-0949", "38e"),
    "showEndOfYearScreen": ("the year-end screen's effect is not ported", "", "38c"),
    "showBulletinBoard": ("the modal bulletin board (skillPointDialog, scrollTextWindow over Text_put)", "U-0950", "38d"),
    "showCharacterIntro": ("skillPointDialog as a Blocking-pump command", "U-0865,U-0931", "38d"),
    "courseActionMenu": ("popups (Text_put) and loadCourse / loadGame", "U-0944", "38b"),
    "showRoutingMapScreen": ("the F5 routing map screen is not ported (3.7b gap)", "", "none"),
    "pumpAndRedraw": ("a Blocking wait with no recorded input left", "U-0650", "-"),
    "showThemePackSelect": ("the title menu's Select A Theme path is not ported", "U-1050", "38j"),
    "__open": ("the title menu's quit (top10.sve, credits) is not ported", "U-1050", "38j"),
    "loadInterfaceArt(full != 0): the boot art (mainLoop's Setup)": ("mainLoop's Setup is boot code, not run", "U-1050", "38j"),
}
# A divergence (no missing name) is explained only by an entry here: segment name -> (cause, U-rows, owner). Empty at the
# time of writing: no accepted segment diverges.
DIVERGENCE_CAUSES: dict = {}
# Segments whose run ends with the frame step leaving mainLoop for the title menu inside the segment (sg_run_frames does not
# run mainLoop's title-menu section): measured by 38a with a debug build (log/p38/38a_notes.md section 5): frame 18's
# menu-bar click -> showSystemMenu -> "Exit the Game" -> "So long for now" -> FrameExit::Menu at ML+0xf106, and the
# original measures the title menu's strings next. The parent lists them in port/test/test_tick.py OPEN_DIVERGENT.
# Since phase 3.8 stream 38j sg_run_frames runs the title-menu section (tick_menu.inc) and those four segments PASS: empty.
SEGMENT_MENU_EXITS: dict = {}
# The frame (segment-local frame-head index) in which each of those segments leaves mainLoop: the segment runs stop after it
# (frames_run 18 / 19 / 18 / 18, the next record never reached). A checkpoint of the segment polled after that frame head is
# taken in the title menu (its poll rides on waitTicks, which the menu loop calls too): the port cannot reach that state.
SEGMENT_MENU_EXIT_FRAME: dict = {}   # was cov_b_career_types/00 18, /01 19, /02 18, cov_k1_animals/00 18 (before 38j)


def _segment_at(chain_row: dict) -> int | None:
    """The segment of a chain run's row that holds its stop record (from the boundaries column and the chain's first
    segment)."""
    try:
        rec = int(chain_row["record"])
        first = int(chain_row["chain"].split("/")[1].split("-")[0])
    except (KeyError, ValueError, IndexError):
        return None
    seg = first
    for item in filter(None, str(chain_row.get("boundaries", "")).split(",")):
        k, start = item.split(":")
        if rec >= int(start):
            seg = int(k)
    return seg


def _missing_name(run_detail: str) -> str:
    if run_detail.startswith("missing: "):
        return run_detail[len("missing: "):].split(";", 1)[0].strip()
    return ""


def _stop_path(run_detail: str) -> str:
    for part in run_detail.split(";"):
        if part.strip().startswith("stopped in "):
            return part.strip()[len("stopped in "):]
    return ""


def ledger_rows(seg_tsv: pathlib.Path, chain_tsv: pathlib.Path | None, defects_tsv: pathlib.Path | None) -> list[dict]:
    import segment_corpus as SC
    rows = []
    # 1. accepted segments that do not PASS
    for r in read_tsv(seg_tsv):
        if r["status"] == "PASS":
            continue
        miss = _missing_name(r["run_detail"])
        frames = f"{r['frames_run']}/{r['frames_expected']}"
        if miss:
            cause, u, owner = MISSING_CAUSES.get(miss, ("", "", ""))
            rows.append({"kind": "segment", "name": r["name"], "status": r["status"], "frames": frames,
                         "stop": f"missing {miss}" + (f" (in {_stop_path(r['run_detail'])})" if _stop_path(r["run_detail"]) else ""),
                         "cause": cause, "u_rows": u, "owner": owner})
        elif str(r["run_detail"]).startswith(MENU_EXIT):
            cause, u, owner = SEGMENT_MENU_EXITS.get(r["name"], ("", "", ""))
            rows.append({"kind": "segment", "name": r["name"], "status": r["status"], "frames": frames,
                         "stop": f"{MENU_EXIT} (record {r['record']})", "cause": cause, "u_rows": u, "owner": owner})
        else:
            cause, u, owner = DIVERGENCE_CAUSES.get(r["name"], ("", "", ""))
            rows.append({"kind": "segment", "name": r["name"], "status": r["status"], "frames": frames,
                         "stop": f"diverges at record {r['record']}: {r['objects']}" if r["record"] else r["detail"],
                         "cause": cause, "u_rows": u, "owner": owner})
    # 2. segments the corpus rejected
    for r in S.manifest_rows():
        if r["status"] == "accepted":
            continue
        why = r["reason"]
        u = "U-0681" if "kind schema v1 does not define" in why else ""
        rows.append({"kind": "segment-rejected", "name": f"{r['scenario']}/{int(r['segment']):02d}", "status": r["status"],
                     "frames": "", "stop": "", "cause": why, "u_rows": u, "owner": "2.4 corpus"})
    # 3. pinned scenarios with no segment row
    refs = json.loads(SC.REFS_MANIFEST.read_text())["scenarios"]
    have = {r["scenario"] for r in S.manifest_rows()}
    for sc in sorted(set(refs) - have):
        plan = SC.OUT / sc / "plan.json"
        if not plan.exists():
            cause = "no segment corpus run yet (log/p2/p24/<scenario>/plan.json absent)"
        else:
            p = json.loads(plan.read_text())
            if p.get("prefix") is None:
                cause = ("no checkpoint capture met a frame head inside the frame loop, so no restart prefix exists "
                         "(re/analysis/data/segment_corpus.md section 4.2, rule 1)")
            else:
                cause = (f"no checkpoint after the prefix `{p['prefix']}` leaves {SC.MIN_FRAMES} frame heads "
                         "(segment_corpus.md section 1, step 3)")
        rows.append({"kind": "scenario-no-segment", "name": sc, "status": "no segment", "frames": "", "stop": "",
                     "cause": cause, "u_rows": "", "owner": "2.4 corpus"})
    # 4. chained runs that do not PASS
    if chain_tsv and chain_tsv.exists():
        for r in read_tsv(chain_tsv):
            if r["status"] == "PASS":
                continue
            if r["status"] == "BREAK":
                rows.append({"kind": "chain-break", "name": f"{r['chain']}@{r['checkpoint']}", "status": "BREAK",
                             "frames": f"{r['frames_run']}/{r['frames_expected']}", "stop": r["detail"],
                             # explained only where the scenario script is known to leave mainLoop at that boundary
                             "cause": ("the frame step leaves mainLoop for the title menu at the next segment's first frame "
                                       "head, as the original does: the scenario starts each championship round from the "
                                       "title menu (re/tools/scenario.py _cov_b_champ_rounds); the next segment starts "
                                       "after the menu and a new game's set-up, outside the frame step, and the chain goes "
                                       "on from that segment's image"
                                       if r["chain"].split("/")[0] in MENU_ROUND_SCENARIOS else ""),
                             "u_rows": "", "owner": "-"})
                continue
            miss = _missing_name(r["run_detail"])
            cause, u, owner = MISSING_CAUSES.get(miss, ("", "", "")) if miss else ("", "", "")
            if not miss and str(r["run_detail"]).startswith(MENU_EXIT):
                seg = _segment_at(r)
                name = f"{r['chain'].split('/')[0]}/{seg:02d}" if seg is not None else ""
                cause, u, owner = SEGMENT_MENU_EXITS.get(name, ("", "", ""))
                if cause:
                    cause = f"inside segment {name}: " + cause
            elif not miss and r["record"] == "end":
                # an end check that fails at a checkpoint polled after a segment's measured mainLoop exit
                seg = _segment_at({**r, "record": r["frames_expected"]})
                name = f"{r['chain'].split('/')[0]}/{seg:02d}" if seg is not None else ""
                start = 0
                for item in filter(None, str(r.get("boundaries", "")).split(",")):
                    k, st0 = item.split(":")
                    if int(k) == seg:
                        start = int(st0)
                if name in SEGMENT_MENU_EXIT_FRAME and int(r["frames_expected"]) - start >= SEGMENT_MENU_EXIT_FRAME[name]:
                    cause = (f"checkpoint {r['checkpoint']} is polled after segment {name} leaves mainLoop (frame "
                             f"{SEGMENT_MENU_EXIT_FRAME[name]}): it is taken in the title menu, which the frame step does "
                             "not run; " + SEGMENT_MENU_EXITS[name][0])
                    u, owner = SEGMENT_MENU_EXITS[name][1], SEGMENT_MENU_EXITS[name][2]
            rows.append({"kind": "chain", "name": f"{r['chain']}@{r['checkpoint']}", "status": r["status"],
                         "frames": f"{r['frames_run']}/{r['frames_expected']}",
                         "stop": (f"missing {miss}" if miss else
                                  f"{MENU_EXIT} (record {r['record']})" if str(r["run_detail"]).startswith(MENU_EXIT) else
                                  f"diverges at record {r['record']}: {r['objects']}" if r["record"] else r["detail"]),
                         "cause": cause, "u_rows": u, "owner": owner})
    # 5. compatibility switches: the evidence that turning each OFF changes a result
    if defects_tsv and defects_tsv.exists():
        tests = {"sinScaled.readPastTable": "port/test/test_core.py::test_defect_sin_past_table (vectors)",
                 "shotPower.cacheIgnoresPutt": "port/test/test_shots.py::test_defect_shot_power_putt (vectors)",
                 "prepareShot.hazardCellTransposed": "port/test/test_shots.py::test_defect_hazard_cell_transposed (vectors)",
                 "narrateShot.maskNotRestored": "port/test/test_shots.py::test_defect_narrate_mask_not_restored (state)"}
        for r in read_tsv(defects_tsv):
            n = int(r["segments_changed"])
            ev = [f"{n} segments change ({r['changed']})"] if n else []
            if r["switch"] in tests:
                ev.append(tests[r["switch"]])
            rows.append({"kind": "defect-switch", "name": r["switch"], "status": "evidence" if ev else "no evidence",
                         "frames": "", "stop": "", "cause": "; ".join(ev) if ev else "",
                         "u_rows": "U-1012" if not ev else "", "owner": r["module"]})
    for r in rows:
        r["explained"] = "yes" if r["cause"] else "NO"
    return rows


LEDGER_COLS = ["kind", "name", "status", "frames", "stop", "cause", "u_rows", "owner", "explained"]


def cmd_ledger(a) -> int:
    if a.run:
        # regenerate the inputs first with the library given (about 20 minutes with 2 workers): the segment gate, every
        # chain of every scenario, every compatibility switch OFF over every segment
        lib_ns = dict(backend="x64", dll=a.dll, dll32=None, runner32=None, build=a.build, jobs=a.jobs)
        cmd_segments(argparse.Namespace(**lib_ns, out=str(OUT / "segments_x64.tsv"), cls="all", only=""))
        cmd_chain(argparse.Namespace(**lib_ns, out=str(OUT / "chain_x64.tsv"), scenario=None, all=True, min_segments=0,
                                     final_only=False))
        cmd_defects(argparse.Namespace(**lib_ns, out=str(OUT / "defects_x64.tsv"), cls="all", switch=None))
    seg = pathlib.Path(a.segments or OUT / "segments_x64.tsv").resolve()
    chain = (pathlib.Path(a.chain) if a.chain else OUT / "chain_x64.tsv").resolve()
    defects = (pathlib.Path(a.defects) if a.defects else OUT / "defects_x64.tsv").resolve()
    rows = ledger_rows(seg, chain, defects)
    out = pathlib.Path(a.out or LEDGER)
    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", encoding="utf-8", newline="") as fh:
        fh.write("# re/port/divergences.tsv: every segment, chained run, rejected segment, segment-less scenario and defect "
                 "switch that is not a plain PASS, with its cause. Regenerate: ~/bin/py -3.12 -m port.test.integrator "
                 "ledger [--run --dll <portsim_c.dll>] (--run first rewrites the inputs: "
                 f"{seg.relative_to(ROOT).as_posix()}"
                 + (f", {chain.relative_to(ROOT).as_posix()}" if chain.exists() else "")
                 + (f", {defects.relative_to(ROOT).as_posix()}" if defects.exists() else "") + ").\n")
        fh.write("\t".join(LEDGER_COLS) + "\n")
        for r in rows:
            fh.write("\t".join(str(r.get(c, "")).replace("\t", " ") for c in LEDGER_COLS) + "\n")
    bad = [r for r in rows if r["explained"] != "yes"]
    by = {}
    for r in rows:
        by[r["kind"]] = by.get(r["kind"], 0) + 1
    print(json.dumps({"rows": len(rows), "by_kind": by, "without_cause": len(bad)}))
    for r in bad:
        print(f"  NO CAUSE: {r['kind']} {r['name']} {r['stop']}")
    print(f"-> {out}")
    return 1 if bad else 0


# ------------------------------------------------------------------------------------------- compare
def cmd_compare(a) -> int:
    ra = {r["name"]: r for r in read_tsv(a.a)}
    rb = {r["name"]: r for r in read_tsv(a.b)}
    keys = ("status", "frames_run", "frames_expected", "code", "record", "objects", "noncausal", "digest")
    diff = []
    for n in sorted(set(ra) | set(rb)):
        x, y = ra.get(n), rb.get(n)
        if x is None or y is None:
            diff.append((n, "missing in " + ("a" if x is None else "b")))
            continue
        d = [k for k in keys if x.get(k, "") != y.get(k, "")]
        if d:
            diff.append((n, ", ".join(f"{k}: {x.get(k)} | {y.get(k)}" for k in d)))
    for n, why in diff:
        print(f"{n}: {why}")
    print(f"{len(set(ra) & set(rb))} rows in both, {len(diff)} differ")
    return 1 if diff else 0


# ------------------------------------------------------------------------------------------- main
def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sp = ap.add_subparsers(dest="cmd", required=True)

    def lib_args(p):
        p.add_argument("--backend", choices=["x64", "win32"], default=None)
        p.add_argument("--dll")
        p.add_argument("--dll32")
        p.add_argument("--runner32")
        p.add_argument("--build", help="a private build's name: port/build/<name>_run/latest.json")
        p.add_argument("--jobs", type=int, default=8)
        p.add_argument("--out")

    p = sp.add_parser("segments")
    lib_args(p)
    p.add_argument("--cls", default="all")
    p.add_argument("--only", default="")
    p = sp.add_parser("chain")
    lib_args(p)
    p.add_argument("--scenario", nargs="+")
    p.add_argument("--all", action="store_true", help="every scenario with accepted segments")
    p.add_argument("--min-segments", type=int, default=0)
    p.add_argument("--final-only", action="store_true", help="only the run to each chain's last checkpoint")
    p = sp.add_parser("check-slices")
    p.add_argument("--scenario", nargs="+")
    p = sp.add_parser("speed")
    lib_args(p)
    p.add_argument("--segment", default="course_season/09")
    p.add_argument("--frames", type=int, default=8192)
    p = sp.add_parser("defects")
    lib_args(p)
    p.add_argument("--cls", default="all")
    p.add_argument("--switch", nargs="+")
    p = sp.add_parser("ledger")
    p.add_argument("--segments", help="a segments TSV (default log/p38/38f/segments_x64.tsv)")
    p.add_argument("--chain")
    p.add_argument("--defects")
    p.add_argument("--out")
    p.add_argument("--run", action="store_true", help="regenerate the three inputs first (segments, chain --all, defects)")
    p.add_argument("--dll")
    p.add_argument("--build")
    p.add_argument("--jobs", type=int, default=2)
    p = sp.add_parser("compare")
    p.add_argument("a")
    p.add_argument("b")
    a = ap.parse_args(argv)
    return {"segments": cmd_segments, "chain": cmd_chain, "check-slices": cmd_check_slices, "speed": cmd_speed,
            "compare": cmd_compare, "defects": cmd_defects, "ledger": cmd_ledger}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
