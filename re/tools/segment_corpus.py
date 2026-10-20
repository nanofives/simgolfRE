r"""Phase 2.4 segment corpus (re/PHASE23_PLAN.md 2.4; write-up re/analysis/data/segment_corpus.md).

A SEGMENT is whole-run test data the port must reproduce: a start state image taken at a mainLoop frame head (the
timeGetTime call at 0x0040fc29, return address 0x0040fc2f), the recorded clock / input trace from that frame on, and the
expected per-object hashes at every later frame head and checkpoint up to the segment's end checkpoint. The validity rule
is phase 2.1's (re/analysis/data/premise_check.md section 7): a frame-head image of the whole .data section of a
continuous run, with the dwords that differ between continuous runs (per-process heap pointers) masked; never a save.

Pipeline per pinned scenario (re/analysis/data/scenario_refs.json), every hook off, C3FT_AUDIO=none:
    # 1. three continuous runs: frame-head captures after every checkpoint (restart_ab.py's IMAGE block), per-frame
    #    sim hashes (C3FT_FRAMEHASH=sim) and per-checkpoint object hashes (C3FT_OBJHASH=1)
    ~/bin/py -3.12 re/tools/segment_corpus.py capture --scenario sandbox_basic --label c_a     (c_b, c_c)
    # 2. the clock / input trace of the scenario (re/tools/trace_tool.py's recorder, separate run)
    ~/bin/py -3.12 re/tools/segment_corpus.py trace --scenario sandbox_basic
    # 3. the segment table: starts, ends, continuous agreement, trace slices, input summary
    ~/bin/py -3.12 re/tools/segment_corpus.py plan --scenario sandbox_basic
    # 4. determinism proof: two restarts per segment from its image (restart_ab.py data variant), plus one restart with
    #    the segment's inputs dropped (the counterfactual) for segments that hold input
    ~/bin/py -3.12 re/tools/segment_corpus.py restart --scenario sandbox_basic --segment 3 --label r1   (r2, cf)
    # 5. verdicts, classification, corpus files and manifests
    ~/bin/py -3.12 re/tools/segment_corpus.py verify --scenario sandbox_basic
    ~/bin/py -3.12 re/tools/segment_corpus.py extract --scenario sandbox_basic
    ~/bin/py -3.12 re/tools/segment_corpus.py manifest
    # all of it for one scenario (and --cleanup of the run folders' scratch files afterwards)
    ~/bin/py -3.12 re/tools/segment_corpus.py pipeline --scenario sandbox_basic [--instance p24] [--cleanup]

Run folders: log/p2/p24/<scenario>/<label>/ (instances\p24 / p24b, winmm.dll 381ed2b2 of the pinned references).
Corpus (local, gitignored): port/corpus/segments/<scenario>/<k>/ and port/corpus/segments.json; committed manifest
re/port/segments_manifest.tsv.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import shutil
import struct
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "re" / "frida"))

import trace_format as TF  # noqa: E402

OUT = pathlib.Path(os.environ["P24_OUT"]).resolve() if os.environ.get("P24_OUT") else ROOT / "log" / "p2" / "p24"
PINNED = ROOT / "log" / "c3" / "alt" / "refs"
REFS_MANIFEST = ROOT / "re" / "analysis" / "data" / "scenario_refs.json"
TRACES = ROOT / "port" / "corpus" / "traces"
CORPUS = ROOT / "port" / "corpus" / "segments"
MANIFEST_JSON = ROOT / "port" / "corpus" / "segments.json"
MANIFEST_TSV = ROOT / "re" / "port" / "segments_manifest.tsv"
DATA_VA, DATA_N = 0x004C1000, 0x381174
FRAME_HEAD_RA = 0x0040FC2F
T0 = 0x00100000                    # scenario_ab.T0: the first main-thread timeGetTime value
FRAME_MS = 99                      # a normal frame under the virtual clock (re/analysis/sim/time_model.md section 3)
MODAL_GAP_MS = 200                 # a frame-head interval longer than this holds time spent outside the frame (modal)
CONTINUOUS = ("c_a", "c_b", "c_c")
SOURCE, NOISE = "c_a", ("c_b", "c_c")
INPUT_TYPES = (TF.DELIVER, TF.MSG, TF.HANDLER, TF.TAKE)
MIN_FRAMES = 20                    # a segment holds at least this many frame heads after its start
MAX_SEGMENTS = 6                   # per scenario (course_season: 10, see max_segments)
CLASSES = ("TIME-FORWARD", "COMMAND", "UI-ONLY")


# ================================================================ pure helpers (tests/test_segment_corpus.py)
def sha256_file(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def read_frames(run: pathlib.Path):
    """(symbols, T[u32], idx[u32], H[uint64 records x objects]) of a run folder's framehash.bin, or None."""
    import frame_trace as FT
    r = FT.read_frames(run)
    if r is None:
        return None
    meta, T, I, H = r
    return meta["symbols"], T, I, H


def frames_dtype(n: int):
    import numpy as np
    return np.dtype([("T", "<u4"), ("idx", "<u4"), ("h", "<u8", (n,))])


def pack_frames(T, I, H) -> bytes:
    """framehash.bin layout: per record u32 T, u32 frame index, u64 sgh64 x n."""
    import numpy as np
    n = H.shape[1] if len(H.shape) == 2 else 0
    a = np.zeros(len(T), frames_dtype(n))
    a["T"], a["idx"] = T, I
    if len(T):
        a["h"] = H
    return a.tobytes()


def unpack_frames(blob: bytes, n: int):
    import numpy as np
    rec = 8 + 8 * n
    if len(blob) % rec:
        raise ValueError(f"{len(blob)} bytes is not a multiple of the {rec}-byte frame record")
    a = np.frombuffer(blob, frames_dtype(n), count=len(blob) // rec)
    return a["T"], a["idx"], a["h"].reshape(len(a), n)


def frame_window(T, start_ret: int, end_T: int):
    """Indices of the frame records of one segment: the start frame (T == start_ret, the frame head whose state is the
    image) and every later frame head before the end checkpoint's dump (T < end_T, the agent's T at that poll: a poll fires
    inside a clock call after the frame-head hash of that call, so a frame whose value is end_T - 1 is before the dump)."""
    import numpy as np
    T = np.asarray(T)
    idx = np.nonzero((T >= start_ret) & (T < end_T))[0]
    return idx


def compare_frames(exp_T, exp_H, got_T, got_H, symbols) -> dict:
    """Records aligned by order. SAME when both hold the same number of records and every T and every object hash agree;
    otherwise the first divergent record (index, T of both, objects whose hash differs)."""
    import numpy as np
    n = min(len(exp_T), len(got_T))
    out = {"expected": int(len(exp_T)), "got": int(len(got_T)), "first_divergent": None}
    if n:
        bad = (np.asarray(exp_T[:n]) != np.asarray(got_T[:n])) | (np.asarray(exp_H[:n]) != np.asarray(got_H[:n])).any(axis=1)
        hit = np.nonzero(bad)[0]
        if len(hit):
            i = int(hit[0])
            objs = [symbols[j] for j in np.nonzero(np.asarray(exp_H[i]) != np.asarray(got_H[i]))[0]]
            out["first_divergent"] = {"record": i, "T": [int(exp_T[i]) - T0, int(got_T[i]) - T0], "objects": objs,
                                      "divergent_records": int(len(hit))}
    if out["first_divergent"] is None and len(exp_T) != len(got_T):
        out["first_divergent"] = {"record": n, "T": [int(exp_T[n]) - T0 if n < len(exp_T) else None,
                                                     int(got_T[n]) - T0 if n < len(got_T) else None],
                                  "objects": [], "divergent_records": abs(len(exp_T) - len(got_T)),
                                  "reason": "record count differs"}
    out["same"] = out["first_divergent"] is None
    return out


def compare_objhash(exp: dict, got: dict, classes=("sim",)) -> dict:
    """{class: [symbols whose hash differs]} over the given classes (None = all); `same` when the list is empty."""
    diff: dict[str, list[str]] = {}
    for k in sorted(set(exp) | set(got)):
        a, b = exp.get(k), got.get(k)
        cls = (a or b)[0]
        if classes and cls not in classes:
            continue
        if a != b:
            diff.setdefault(cls, []).append(k)
    return {"same": not diff, "differ": diff}


def mask_runs(mask) -> list[list[int]]:
    """[[offset, length]] of the True runs of a byte mask."""
    import numpy as np
    m = np.concatenate(([False], np.asarray(mask, bool), [False])).astype(np.int8)
    d = np.diff(m)
    return [[int(s), int(e - s)] for s, e in zip(np.nonzero(d == 1)[0], np.nonzero(d == -1)[0])]


def noise_bitmap(mask) -> bytes:
    """A byte mask of the data section as a dword bitmap: bit i (LSB first) set when any byte of the dword at offset 4*i
    is masked (the noise is dword-granular, restart_ab.dwords)."""
    import numpy as np
    m = np.asarray(mask, bool)
    n = (len(m) + 3) // 4
    w = np.zeros(n * 4, bool)
    w[:len(m)] = m
    return np.packbits(w.reshape(n, 4).any(axis=1), bitorder="little").tobytes()


def bitmap_mask(blob: bytes, n: int):
    """The byte mask of a dword bitmap (noise_bitmap's inverse, widened to whole dwords)."""
    import numpy as np
    bits = np.unpackbits(np.frombuffer(blob, np.uint8), bitorder="little")[:(n + 3) // 4].astype(bool)
    return np.repeat(bits, 4)[:n]


def runs_mask(runs, n: int):
    import numpy as np
    m = np.zeros(n, bool)
    for off, k in runs:
        m[off:off + k] = True
    return m


# ---------------------------------------------------------------- trace slices
def read_trace(path) -> TF.Trace:
    """trace_format.read, except that a DELIVER of a kind schema v1 has no format for is kept as {pos, kind: 'unknownN'}
    instead of failing the whole file: trace_rec.js writes kind 0 with no fields for a scenario_det action it does not know
    (measured: golfer_events' injected landmark notice, action k='call', log/c3/s36a_run.py). slice_trace refuses a slice
    that holds one."""
    data = pathlib.Path(path).read_bytes()
    tr = TF.parse(data, decode_fields=False)
    for r in tr.records:
        if r.type == TF.DELIVER and r.sub not in TF.DELIVER_FMT:
            r.f = {"pos": struct.unpack_from("<I", r.payload)[0] if len(r.payload) >= 4 else 0, "kind": f"unknown{r.sub}"}
        else:
            r.f = TF.decode(r)
    return tr


def trace_path(scen: str) -> pathlib.Path | None:
    """The scenario's trace: port/corpus/traces/<scenario>.trace, or log/p2/p24/<scenario>/trace_rec.trace when the
    recording does not conform to schema v1 (kept out of the corpus, see cmd_trace)."""
    for p in (TRACES / f"{scen}.trace", OUT / scen / "trace_rec.trace"):
        if p.exists():
            return p
    return None


def find_frame(tr: TF.Trace, t: int) -> int:
    """Index of the FRAME record whose frame-head clock value is t (-1: none)."""
    for i, r in enumerate(tr.records):
        if r.type == TF.FRAME and r.f["t"] == t:
            return i
    return -1


def find_poll(tr: TF.Trace, name: str, after: int = 0) -> int:
    for i in range(after, len(tr.records)):
        r = tr.records[i]
        if r.type == TF.POLL and r.f["checkpoint"] == name:
            return i
    return -1


def slice_trace(tr: TF.Trace, start_t: int, end_checkpoint: str, extra_meta: dict | None = None) -> tuple[bytes, dict]:
    """A stand-alone, valid trace (trace_format.validate) of one segment: the frame-head clock call whose value is start_t
    (the instant of the start image), every record after it, up to and including the POLL of `end_checkpoint` and a FRAME
    record of that same clock call if it follows the POLL. Clock ordinals, frame indices and poll indices are rebased to
    0; the header's t0 / q0 become the values of the slice's first timeGetTime / QPC call; the last POLL is marked final;
    an END record closes it. `meta.slice` keeps the source offsets (first ordinal, first frame, first poll)."""
    fi = find_frame(tr, start_t)
    if fi < 0:
        raise ValueError(f"no FRAME record with t 0x{start_t:08x}")
    fr = tr.records[fi]
    head_ord = fr.f["pos"] - 1                     # the frame-head call is the last clock call before the FRAME record
    ci = fi - 1
    while ci >= 0 and tr.records[ci].type != TF.CLOCKRUN:
        ci -= 1
    cr = tr.records[ci].f
    if not (cr["first"] <= head_ord < cr["first"] + cr["count"]) or cr["ra"] != FRAME_HEAD_RA:
        raise ValueError("the FRAME record does not follow its frame-head clock call")
    pi = find_poll(tr, end_checkpoint, fi)
    if pi < 0:
        raise ValueError(f"no POLL of checkpoint {end_checkpoint} after the start frame")
    bad = [r.f["kind"] for r in tr.records[fi:pi + 2] if r.type == TF.DELIVER and r.sub not in TF.DELIVER_FMT]
    if bad:
        raise ValueError(f"the slice holds {len(bad)} DELIVER record(s) of a kind schema v1 does not define ({bad[0]}: a "
                         f"scenario_det action trace_rec.js does not encode, e.g. k='call')")
    last = pi
    if pi + 1 < len(tr.records) and tr.records[pi + 1].type == TF.FRAME and tr.records[pi + 1].f["pos"] == tr.records[pi].f["pos"]:
        last = pi + 1
    # clock values at the cut: count the tgt / qpc calls before the frame-head call
    tcount = qcount = 0
    for r in tr.records[:ci]:
        if r.type == TF.CLOCKRUN:
            if r.f["kind"] == 1:
                tcount += r.f["count"]
            else:
                qcount += r.f["count"]
    k_before = head_ord - cr["first"]              # calls of the cut CLOCKRUN before the frame-head call
    if cr["kind"] != 1:
        raise ValueError("frame-head clock run is not timeGetTime")
    tcount += k_before
    t0 = tr.t0 + tcount * tr.step
    if t0 != start_t:
        raise ValueError(f"virtual clock at the cut 0x{t0:08x} != start 0x{start_t:08x}")
    q0 = tr.q0 + qcount * tr.qstep
    f0, p0 = fr.f["frame"], None
    meta = dict(tr.meta)
    meta["slice"] = {"first_ordinal": head_ord, "first_frame": f0, "end_checkpoint": end_checkpoint,
                     "source_t0": tr.t0, "source_q0": tr.q0, **(extra_meta or {})}
    body = bytearray()
    n = frames = polls = dumps = 0
    finals = 0
    recs = [("cut", cr)] + [(None, r) for r in tr.records[ci + 1:last + 1]]
    for tag, r in recs:
        if tag == "cut":
            cnt = cr["count"] - k_before
            v0 = cr["v0"] + k_before * tr.step
            body += TF.rec(TF.CLOCKRUN, 1, struct.pack("<IIIIQ", 0, cnt, cr["ra"], cr["slot"], v0))
            n += cnt
            continue
        f = r.f
        if r.type == TF.CLOCKRUN:
            body += TF.rec(TF.CLOCKRUN, r.sub, struct.pack("<IIIIQ", f["first"] - head_ord, f["count"], f["ra"], f["slot"],
                                                           f["v0"]))
            n += f["count"]
            continue
        p = bytearray(r.payload)
        if r.type == TF.FRAME:
            struct.pack_into("<II", p, 0, f["frame"] - f0, f["pos"] - head_ord)
            frames += 1
        elif r.type == TF.POLL:
            if p0 is None:
                p0 = f["index"]
            struct.pack_into("<II", p, 0, f["pos"] - head_ord, f["index"] - p0)
            sub = r.sub & 1
            if r is tr.records[pi]:
                sub |= 2
                finals += 1
            polls += 1
            dumps += f["checkpoint"] is not None
            body += TF.rec(TF.POLL, sub, bytes(p))
            continue
        elif r.type == TF.END:
            continue
        elif "pos" in f:
            struct.pack_into("<I", p, 0, f["pos"] - head_ord)
        body += TF.rec(r.type, r.sub, bytes(p))
    meta["slice"]["first_poll"] = p0
    body += TF.rec(TF.END, 0, struct.pack("<IIII", n, frames, polls, dumps))
    data = TF.header_bytes(meta, tr.step, t0, tr.qstep, q0) + bytes(body)
    return data, {"clock_calls": n, "frames": frames, "polls": polls, "dumps": dumps, "first_ordinal": head_ord,
                  "first_frame": f0, "t0": t0, "q0": q0}


def queued_posts(tr: TF.Trace, upto: int) -> list[dict]:
    """The harness's posted messages still in the game's message queue when record `upto` is written (a FRAME record:
    the frame head). Queue model, FIFO: a DELIVER `post` enqueues (msg, wp, lp); a MSG record (the window procedure
    entered on the main thread) with the same triple removes it and every older entry (posted messages are retrieved in
    order; an older one with no MSG record of its own was taken by a filtered peek or a modal pump without dispatch:
    measured KEYUP 121 posted / 120 dispatched in cov_b_holes18). A MSG with no queued match (WM_CHAR made by
    TranslateMessage) changes nothing. Measured: cov_l_camera step0's VK_UP WM_KEYDOWN was posted at the checkpoint poll
    and dispatched in the H1 of the next frame, after its frame head, so a .data image taken at that frame head lacks it."""
    q: list[dict] = []
    for r in tr.records[:upto]:
        if r.type == TF.DELIVER and r.f.get("kind") == "post":
            q.append({"msg": r.f["msg"], "wp": r.f["wp"], "lp": r.f["lp"], "cls": r.f["cls"], "hwnd": r.f["hwnd"],
                      "pos": r.f["pos"]})
        elif r.type == TF.MSG:
            key = (r.f["msg"], r.f["wp"], r.f["lp"])
            for j, e in enumerate(q):
                if (e["msg"], e["wp"], e["lp"]) == key:
                    del q[:j + 1]
                    break
    return q


def input_summary(tr: TF.Trace) -> dict:
    """What a trace (slice) holds as input: harness deliveries by kind, window messages by number, root-window handler
    entries by name, H1 take-and-clear results, frames, frame-head intervals (normal 99 virtual ms; a longer one is time
    spent outside mainLoop's frame, a modal screen or a blocking pump), and the date / pause / RNG columns of the FRAME
    snapshots."""
    out = {"deliver": {}, "msg": {}, "handler": {}, "take": [], "inputs": 0, "frames": 0, "intervals": {},
           "modal_ms": 0, "max_interval": 0, "paused_frames": 0, "date": None, "rng_changes": 0, "w32": []}
    ts, last_rng, date0, date1 = [], None, None, None
    for r in tr.records:
        f = r.f
        if r.type == TF.DELIVER:
            out["deliver"][f["kind"]] = out["deliver"].get(f["kind"], 0) + 1
            if f["kind"] == "w32":
                out["w32"].append([f"0x{f['addr']:08x}", f["v"]])
        elif r.type == TF.MSG:
            k = f"0x{f['msg']:x}"
            out["msg"][k] = out["msg"].get(k, 0) + 1
        elif r.type == TF.HANDLER:
            k = TF.HANDLERS.get(f["handler"], str(f["handler"]))
            out["handler"][k] = out["handler"].get(k, 0) + 1
        elif r.type == TF.TAKE:
            out["take"].append([f["which"], f["value"]])
        elif r.type == TF.FRAME:
            out["frames"] += 1
            ts.append(f["t"])
            s = TF.snap_fields(f["snap"])
            if s["flags"] & 4:
                out["paused_frames"] += 1
            if date0 is None:
                date0 = s["date"]
            date1 = s["date"]
            if last_rng is not None and s["rng"] != last_rng:
                out["rng_changes"] += 1
            last_rng = s["rng"]
        if r.type in INPUT_TYPES:
            out["inputs"] += 1
    for a, b in zip(ts, ts[1:]):
        dt = b - a
        out["intervals"][str(dt)] = out["intervals"].get(str(dt), 0) + 1
        out["max_interval"] = max(out["max_interval"], dt)
        if dt > MODAL_GAP_MS:
            out["modal_ms"] += dt - FRAME_MS
    if date0 is not None:
        out["date"] = [date0, date1]
    return out


def classify(inp: dict, cf: dict | None) -> tuple[str, str]:
    """(class, reason). TIME-FORWARD: the slice holds no input record (no harness delivery, window message, root handler
    entry or H1 take). With input, the counterfactual restart (the same image with the segment's inputs dropped) decides:
    COMMAND when it diverges from the expected sim hashes (the inputs changed simulation state; its first divergent frame
    and objects are the command's first effect), UI-ONLY when every sim hash is still equal at every frame (the inputs
    reached only UI / rendering state, nothing a headless simulation is tested on). Without a counterfactual result, or
    when it failed, the class is not decided ('?')."""
    if inp["inputs"] == 0:
        return "TIME-FORWARD", "no input record in the slice"
    if cf is None:
        return "?", "input present, no counterfactual run"
    if cf.get("error"):
        return "?", f"input present; the counterfactual run failed ({cf['error'][:120]})"
    if cf["frames"]["same"] and cf["end_sim"]["same"]:
        return "UI-ONLY", "input present, but without it every sim object hash is equal at every frame and at the end"
    d = cf["frames"].get("first_divergent") or {}
    objs = ", ".join(d.get("objects", [])[:6]) or "end checkpoint " + ", ".join(cf["end_sim"]["differ"].get("sim", [])[:6])
    return "COMMAND", f"without the input the sim state diverges (record {d.get('record')}, T {d.get('T')}: {objs})"


def select_segments(caps: list[dict], checkpoints: list[dict], frame_T, min_frames=MIN_FRAMES, max_segments=MAX_SEGMENTS):
    """Segment table of one scenario. caps: [{name, T, ret}] frame-head captures in run order (name = the checkpoint after
    which it was armed; ret = the value the frame head received); checkpoints: [{name, T}] in order. The first capture is
    the restart prefix (a fresh process restored at it would be restored onto its own state), so segments start at the
    later captures. A segment ends at the first checkpoint after its start that leaves at least `min_frames` frame heads
    in between (the next capture then starts after that checkpoint); captures inside a segment are skipped. With more
    candidates than `max_segments`, an evenly spaced subset is kept."""
    import numpy as np
    T = np.asarray(frame_T)
    cand = []
    i = 1
    while i < len(caps):
        c = caps[i]
        end = None
        for cp in checkpoints:
            if cp["T"] <= c["ret"]:
                continue
            nf = int(((T > c["ret"]) & (T < cp["T"])).sum())
            if nf >= min_frames:
                end = cp
                break
        if end is None:
            break
        cand.append({"start": c["name"], "start_ret": c["ret"], "start_T": c["T"], "end": end["name"], "end_T": end["T"],
                     "frames": int(((T > c["ret"]) & (T < end["T"])).sum())})
        # next start: the first capture taken after this segment's end checkpoint
        i += 1
        while i < len(caps) and caps[i]["ret"] < end["T"]:
            i += 1
    if len(cand) > max_segments:
        keep = sorted({int(round(k * (len(cand) - 1) / (max_segments - 1))) for k in range(max_segments)})
        cand = [cand[k] for k in keep]
    for k, s in enumerate(cand):
        s["id"] = k
    return cand


def max_segments(scenario: str) -> int:
    return 10 if scenario in ("course_season", "course_season_6y") else MAX_SEGMENTS


# ================================================================ runs (need the game; one run per process)
def setup(scen: str, instance: pathlib.Path):
    """scenario_refs.setup with run folders and the instance lock under log/p2/p24 (the boot lock stays the shared one),
    every hook off, the device-less audio answers, per-checkpoint object hashes and per-frame sim hashes."""
    import scenario_refs as SR
    SR.REFS = OUT
    os.environ.setdefault("C3FT_AUDIO", "none")
    no_boot_lock()
    os.environ.pop("C3FT_FULLDUMP", None)
    os.environ["C3FT_OBJHASH"] = "1"
    os.environ["C3FT_FRAMEHASH"] = "sim"
    # restart_ab.py's setting: a one-dword trace of the RNG word turns on scenario_det.js's main-thread exception reports
    os.environ.setdefault("C3FT_TRACE", "822d9c:4")
    S = SR.setup(scen, instance)
    return S, SR


def no_boot_lock():
    """The shared boot lock (log/c3/alt/refs/boot.lock, held from spawn to the first checkpoint) is skipped by default:
    on 2026-10-18 two restarts each waited 600 s for a holder that never released it (the lock's stale rule). Batch b2
    measured concurrent boots identical under C3FT_AUDIO=none (re/analysis/data/scenario_baseline.md), and every run of this
    tool is checked against the pinned det_b (captures) or the expected hashes (restarts). P24_BOOT_LOCK=1 restores it."""
    if os.environ.get("P24_BOOT_LOCK") != "1":
        os.environ["SCENARIO_REFS_NO_LOCK"] = "1"


def sched_path(scen: str) -> pathlib.Path:
    """The pinned schedule, copied (read only) into log/p2/p24/<scenario>/schedule.json; its sha256 must equal the
    manifest's."""
    p = OUT / scen / "schedule.json"
    if not p.exists():
        p.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(PINNED / scen / "schedule.json", p)
    want = json.loads(REFS_MANIFEST.read_text())["scenarios"][scen]["schedule_sha256"]
    if sha256_file(p) != want:
        raise SystemExit(f"{p}: sha256 differs from the pinned schedule {want[:16]}")
    return p


SPAWN_LOCK = "spawn.lock"
SPAWN_TIMEOUT = 150            # s from the spawn to the agent's `ready`; measured healthy boots: under 10 s


def spawn_lock_acquire(timeout=900):
    """This tool's own spawn lock (log/p2/p24/spawn.lock, a directory): one Frida spawn / attach / script load at a time
    among this tool's runs. Three runs hung with the game suspended at spawn (working set about 120 KB, no `ready`) after
    the shared boot lock was dropped, the first one when three batch workers spawned in the same second. Stale after
    SPAWN_TIMEOUT + 60 s."""
    lk = OUT / SPAWN_LOCK
    OUT.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    while True:
        try:
            lk.mkdir()
            (lk / "owner.txt").write_text(f"{os.getpid()} {time.time()}")
            return
        except FileExistsError:
            try:
                if time.time() - lk.stat().st_mtime > SPAWN_TIMEOUT + 60:
                    shutil.rmtree(lk, ignore_errors=True)
                    continue
            except OSError:
                pass
            if time.time() - t0 > timeout:
                raise TimeoutError(f"{lk} held for {timeout} s")
            time.sleep(0.5)


def spawn_lock_release():
    lk = OUT / SPAWN_LOCK
    try:
        if (lk / "owner.txt").read_text().split()[0] == str(os.getpid()):
            shutil.rmtree(lk, ignore_errors=True)
    except OSError:
        pass


def spawn_watchdog(instance: pathlib.Path):
    """A timer that, if the agent's `ready` has not arrived SPAWN_TIMEOUT s after the spawn began, kills the
    golf_clean.exe processes whose parent is this runner (frida.spawn makes the game this process's child), which makes the
    hung attach fail and the step fail (the pipeline retries it). Matched by parent pid: a process still suspended at spawn
    reports an empty ExecutablePath (measured twice), so a path test never hits."""
    import subprocess
    import threading
    me = os.getpid()

    def fire():
        import faulthandler
        print(f"  SPAWN WATCHDOG: no agent ready after {SPAWN_TIMEOUT} s; Python stacks:", flush=True)
        faulthandler.dump_traceback(file=sys.stdout, all_threads=True)
        sys.stdout.flush()
        ps = ("Get-CimInstance Win32_Process -Filter \"Name='golf_clean.exe'\" | ForEach-Object { \"$($_.ProcessId)`t"
              "$($_.ParentProcessId)\" }")
        r = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True, timeout=60)
        for line in r.stdout.splitlines():
            pid, _, parent = line.partition("\t")
            if parent.strip() == str(me):
                subprocess.run(["taskkill", "/F", "/PID", pid.strip()], capture_output=True)
                print(f"  SPAWN WATCHDOG: no agent ready after {SPAWN_TIMEOUT} s, killed pid {pid.strip()} "
                      f"(instance {instance.name})", flush=True)
        # a spawn blocked before any child exists (measured twice: the runner wrote agent.js, no game process, no
        # frida-helper of its own, for 18 minutes): leave the run so the pipeline retries it
        if not done.wait(30):
            print("  SPAWN WATCHDOG: still no agent ready 30 s later; leaving the run (rc 3)", flush=True)
            spawn_lock_release()
            os._exit(3)

    done = threading.Event()
    t = threading.Timer(SPAWN_TIMEOUT, fire)
    t.daemon = True
    t.start()

    class _Watch:
        def cancel(self):
            done.set()
            t.cancel()
    return _Watch()


def make_run_class(S):
    import restart_ab as RA
    P21Run = RA.make_run_class(S)
    base_dump = super(P21Run, P21Run).take_dump          # the family Run's take_dump (P21Run's adds dlldata + capture)

    class P24Run(P21Run):
        """restart_ab's run with: at most one capture armed at a time (an armed frame-head watch is replaced by the next
        request, so a checkpoint inside a modal screen would mislabel captures); no DLL page dumps; the frame-head hashes
        fetched at the end; an early stop after `stop_at`; `drop_input` (counterfactual): after the restore no harness
        action is delivered."""

        def __init__(self, *a, stop_at=None, drop_input=False, **kw):
            super().__init__(*a, **kw)
            self.stop_at = stop_at
            self.drop_input = drop_input
            self.pending_capture = []
            self.cap_events: list = []

        def _on_message(self, m, data):
            if m.get("type") == "send" and isinstance(m["payload"], dict) and m["payload"].get("type") == "img":
                p = m["payload"]
                if p.get("op") == "timeout" and self.pending_capture and not self.restart:
                    name = self.pending_capture.pop(0)
                    self.cap_events.append({"name": name, "timeout": True, "seen": p.get("seen")})
                    return
                if p.get("op") == "capture":
                    self.cap_events.append({"name": self.pending_capture[0] if self.pending_capture else None,
                                            "T": p["meta"]["T"], "ret": p["meta"]["ret"], "seen": p["meta"]["seen"]})
            return super()._on_message(m, data)

        def take_dump(self, sc, name):
            base_dump(self, sc, name)
            if self.restart or name not in self.captures:
                return
            if self.pending_capture:
                self.cap_events.append({"name": name, "skipped": True, "pending": self.pending_capture[0]})
                return
            self.pending_capture.append(name)
            sc.post({"type": "go", "img": {"op": "capture", "stack": list(RA.STACK)}, "hwnd": self.hwnd_val()})

        def _go(self, script_fn, env, js, t_start):
            Game = S.Game
            T0_, POLL_MS = S.T0, S.POLL_MS
            spawn_lock_acquire()
            wd = spawn_watchdog(S.INSTANCE)
            try:
                g = Game(env=env, extra_js=[str(js)], on_message=self._on_message).__enter__()
            except BaseException:
                wd.cancel()
                spawn_lock_release()
                raise
            try:
                self.g = g
                sc = g.scripts[-1]
                try:
                    p, _ = self.wait_msg(("ready",), timeout=60)
                finally:
                    wd.cancel()
                    spawn_lock_release()
                self.slots = p["slots"]
                ctx = S.Ctx(self)
                gen = (self.restart["prefix_fn"] if self.restart else script_fn)(ctx)
                first = True
                while True:
                    p, _ = self.wait_msg(("poll",))
                    self.polls += 1
                    ctx.T = p["st"]["T"] - T0_
                    self.agent_log += p.get("log", [])
                    self.trace.append([ctx.T, p["st"]["qpc"], p["st"]["forwarded"], round(time.time() - t_start, 3)])
                    ctx.actions = []
                    if first and os.environ.get("C3FT_NO_VKEYS") != "1":
                        for vk in range(1, 255):
                            ctx.act(0, k="vkey", vk=vk, st=1)
                        first = False
                    ctx.dump_name = None
                    try:
                        next(gen)
                        finished = False
                    except StopIteration:
                        finished = True
                    if ctx.dump_name:
                        self.take_dump(sc, ctx.dump_name)
                    if self.restart and not self.restarted and ctx.dump_name == self.restart["from"]:
                        ctx, gen = self.do_restart(sc, script_fn)
                        continue
                    if self.stop_at and ctx.dump_name == self.stop_at and (not self.restart or self.restarted):
                        finished = True
                    if self.drop_input and self.restarted:
                        ctx.actions = []
                    if finished:
                        if self.frame_hash:
                            self.fetch_frame_hashes(sc, "end")
                        self.post(sc, actions=ctx.actions, hwnd=self.hwnd_val())
                        break
                    self.post(sc, actions=ctx.actions, next=POLL_MS, hwnd=self.hwnd_val())
                    if not g.alive:
                        raise RuntimeError(f"{self.label}: game died: {g.detach_reason}")
                self.final_status = p["st"]
            finally:
                g.__exit__(None, None, None)

    return P24Run


def _finish(run, out: pathlib.Path, extra: dict):
    sp = out / "summary.json"
    if not sp.exists():
        return
    s = json.loads(sp.read_text())
    s["p24"] = {"cap_events": getattr(run, "cap_events", []), "img_log": getattr(run, "img_log", []), **extra}
    for k in ("restore_meta", "plan_stats"):
        if getattr(run, k, None):
            s["p24"][k] = getattr(run, k)
    sp.write_text(json.dumps(s, indent=1))


def _run(a, mode_kw: dict, extra: dict, schedule) -> int:
    instance = (ROOT / "instances" / a.instance).resolve()
    S, SR = setup(a.scenario, instance)
    Run = make_run_class(S)
    out = OUT / a.scenario / a.label
    if out.exists():
        shutil.rmtree(out)
    ilock = SR.instance_lock(instance)
    try:
        dll, backup = instance / "winmm.dll", instance / "winmm.dll.c3ft_main"
        if backup.exists() and SR.sha256(dll) != SR.sha256(backup):
            raise SystemExit(f"{dll} differs from {backup.name}: a killed control run left its DLL (restore it first)")
        want = json.loads(REFS_MANIFEST.read_text())["scenarios"][a.scenario]["build"]["winmm_sha256"]
        if SR.sha256(dll) != want:
            raise SystemExit(f"{dll}: sha256 is not the pinned references' build {want[:8]}")
        S.write_ini_hooks([])
        cond = SR.conditions(instance)
        run = Run("p24", a.scenario, "replay", S.arm_env_off(), [], [], schedule=schedule, out_dir=out, **mode_kw)
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
            _finish(run, out, extra)
        print(f"{a.scenario}/{a.label}: {'OK' if rc == 0 else 'FAILED'} polls={run.polls} checkpoints={len(run.checkpoints)} "
              f"wall={getattr(run, 'wall', 0):.0f}s", flush=True)
        return rc
    finally:
        shutil.rmtree(ilock, ignore_errors=True)


def cmd_capture(a) -> int:
    """A continuous run: the pinned schedule, a frame-head capture armed after every checkpoint but the last (one at a
    time), per-frame sim hashes, per-checkpoint object hashes."""
    sched = json.loads(sched_path(a.scenario).read_text())
    cps = json.loads(REFS_MANIFEST.read_text())["scenarios"][a.scenario]["checkpoints"]
    caps = cps[:-1]
    rc = _run(a, {"captures": caps}, {"captures": caps}, sched)
    out = OUT / a.scenario / a.label
    if (out / "summary.json").exists():
        ref = ROOT / json.loads(REFS_MANIFEST.read_text())["scenarios"][a.scenario]["reference"]
        import scenario_refs as SR
        c = SR.compare(ref, out)
        s = json.loads((out / "summary.json").read_text())
        s["p24"]["vs_pinned"] = SR.summarize(c)
        (out / "summary.json").write_text(json.dumps(s, indent=1))
        print(f"  vs pinned det_b: {s['p24']['vs_pinned']}", flush=True)
    return rc


def cmd_trace(a) -> int:
    """The scenario's clock / input trace (trace_tool.py's recorder) into port/corpus/traces/<scenario>.trace. When that
    file exists (phase 2.3 recorded three), the new recording goes to log/p2/p24/<scenario>/trace_rec.trace and must be
    byte-identical to it."""
    import scenario_refs as SR
    import trace_tool as TT
    SR.REFS = OUT
    sched_path(a.scenario)
    os.environ.setdefault("C3FT_AUDIO", "none")
    no_boot_lock()
    # the spawn guard of this tool's own runs (spawn lock + watchdog around Game.start), for the recorder's run too
    orig_setup = SR.setup

    def guarded_setup(scen, inst):
        S = orig_setup(scen, inst)
        Base = S.Game

        class GuardedGame(Base):
            def start(self):
                spawn_lock_acquire()
                wd = spawn_watchdog(inst)
                try:
                    return super().start()
                finally:
                    wd.cancel()
                    spawn_lock_release()
        S.Game = GuardedGame
        return S
    SR.setup = guarded_setup
    dst = TRACES / f"{a.scenario}.trace"
    out = dst if not dst.exists() else OUT / a.scenario / "trace_rec.trace"
    ns = argparse.Namespace(scenario=a.scenario, label="t_rec", instance=a.instance, out=str(out))
    try:
        rc = TT.cmd_record(ns)
    except TF.TraceError as e:
        # the recorder wrote a file the schema v1 reader rejects: keep it out of port/corpus/traces
        rc = 4
        print(f"trace: the recording does not conform to schema v1: {e}", flush=True)
        if out.exists():
            keep = OUT / a.scenario / "trace_rec.trace"
            if out != keep:
                keep.write_bytes(out.read_bytes())
                out.unlink()
                side = out.parent / (out.name + ".json")
                if side.exists():
                    side.replace(keep.parent / (keep.name + ".json"))
            (OUT / a.scenario / "trace_check.json").write_text(json.dumps({"nonconforming": str(e), "kept": str(keep)}))
        return rc
    if out != dst and out.exists():
        same = out.read_bytes() == dst.read_bytes()
        print(f"trace: new recording {'BYTE-IDENTICAL to' if same else 'DIFFERS from'} {dst}", flush=True)
        (OUT / a.scenario / "trace_check.json").write_text(json.dumps({"existing": str(dst), "new": str(out),
                                                                        "byte_identical": same}))
        if not same:
            return 3
    return rc


def load_plan(scen: str) -> dict:
    return json.loads((OUT / scen / "plan.json").read_text())


def cmd_restart(a) -> int:
    """One restart of a planned segment: a fresh process replays the scenario up to the prefix checkpoint, writes c_a's
    frame-head image of the segment start (restart_ab.py `data` variant: all of .data minus the dwords that differ in c_b
    or c_c there and minus the 12 model flag-gap dwords), hands over the captured clock / events / timers / input, and
    continues c_a's schedule to the segment's end checkpoint. `--drop-input`: the counterfactual (no action after the
    restore; the captured pending actions are dropped too)."""
    import restart_ab as RA
    plan = load_plan(a.scenario)
    seg = next(s for s in plan["segments"] if s["id"] == a.segment)
    src_dir = OUT / a.scenario / SOURCE
    noise_dirs = [OUT / a.scenario / n for n in NOISE]
    nd, ns, (meta, data, stack) = RA.noise_masks(src_dir, noise_dirs, seg["start"])
    instance = (ROOT / "instances" / a.instance).resolve()
    S, SR = setup(a.scenario, instance)
    smap = RA.SymbolMap()
    nd = nd | smap.gaps

    def build(pre_meta, pre):
        regions, stack_regions, blob, stats, _pw = RA.restore_plan(a.variant, False, meta, data, stack, nd, ns, pre_meta,
                                                                   pre, smap, a.corrupt)
        print(f"  restore plan: {stats['image_regions']} regions, {stats['image_bytes']} bytes, {stats['noise_bytes']} "
              f"noise bytes", flush=True)
        return regions, stack_regions, blob, stats

    src_sum = json.loads((src_dir / "summary.json").read_text())
    src_polls = [row[0] for row in json.loads((src_dir / "poll_log.json").read_text())["trace"]]
    events = meta["events"]
    if a.drop_input:
        events = [e for e in events if e.get("k") != "act"]
    else:
        # posted messages still in the source's queue at the start frame head (queued_posts): posted again by the restore
        # itself (an event due at the captured clock value fires inside the restoring clock call, before the frame body)
        events = [{"t": meta["ret"], "k": "act", "a": {"k": "post", "msg": q["msg"], "wp": q["wp"], "lp": q["lp"],
                                                       "tag": "queued at the start"}}
                  for q in seg.get("queued") or []] + events
    restart = {"from": plan["prefix"], "image": seg["start"], "build": build,
               "clock": {"ret": meta["ret"], "T": meta["T"], "Qlo": meta["Qlo"], "Qhi": meta["Qhi"]},
               "events": events, "timers": meta["timers"], "input": meta["input"],
               "schedule": src_sum["schedule"], "src_polls": src_polls, "capture_T": meta["T"],
               "prefix_fn": S.SCENARIOS[a.scenario]}
    sched = json.loads(sched_path(a.scenario).read_text())
    extra = {"segment": a.segment, "start": seg["start"], "end": seg["end"], "prefix": plan["prefix"],
             "drop_input": bool(a.drop_input), "dropped_events": len(meta["events"]) - len(events), "corrupt": a.corrupt,
             "queued_reposted": 0 if a.drop_input else len(seg.get("queued") or []),
             "variant": a.variant}
    return _run(a, {"restart": restart, "stop_at": seg["end"], "drop_input": a.drop_input}, extra, sched)


# ================================================================ plan / verify / extract (no game)
def _summary(d: pathlib.Path) -> dict | None:
    p = d / "summary.json"
    return json.loads(p.read_text()) if p.exists() else None


def run_captures(s: dict) -> list[dict]:
    """[{name, T, ret, seen}] of a capture run's frame-head captures, in order (seen = main-thread clock calls between the
    checkpoint and the frame head)."""
    return [{"name": e["name"], "T": e["T"], "ret": e["ret"], "seen": e.get("seen")}
            for e in s.get("p24", {}).get("cap_events", []) if "ret" in e and e.get("name")]


IN_FRAME_SEEN = 250        # a checkpoint taken inside the frame loop meets the next frame head within one frame (99 calls)


def prefix_index(caps: list[dict]) -> int | None:
    """The restart prefix: the first checkpoint taken inside mainLoop's frame loop (its capture met the frame head within
    IN_FRAME_SEEN clock calls). A restart stops polling the prefix script after the prefix checkpoint and waits for the next
    frame head there, so a checkpoint on the title menu or a modal screen never reaches one (measured: sandbox_basic `menu`,
    200,001 calls without a frame head)."""
    for i, c in enumerate(caps):
        if c.get("seen") is not None and c["seen"] <= IN_FRAME_SEEN and not c.get("queued"):
            return i
    return None


def window_of(fr, seg: dict):
    """(T, H) of a run's frame records inside a segment's window (start frame included)."""
    _sym, T, _I, H = fr
    w = frame_window(T, seg["start_ret"], seg["end_T"])
    return T[w], H[w]


def run_checkpoints(s: dict) -> list[dict]:
    return sorted(({"name": n, "T": c["T"]} for n, c in s["checkpoints"].items()), key=lambda x: x["T"])


def cmd_plan(a) -> int:
    """The segment table of a scenario from its three continuous runs and its trace: log/p2/p24/<scenario>/plan.json."""
    import numpy as np
    sd = OUT / a.scenario
    sums = {l: _summary(sd / l) for l in CONTINUOUS}
    problems = []
    for l, s in sums.items():
        if s is None or s.get("failure"):
            problems.append(f"{l}: {'no summary' if s is None else s['failure'][:200]}")
    if problems:
        print("plan: " + "; ".join(problems))
        (sd / "plan.json").write_text(json.dumps({"scenario": a.scenario, "segments": [], "problems": problems}, indent=1))
        return 2
    caps = {l: run_captures(s) for l, s in sums.items()}
    agree_caps = all(caps[l] == caps[SOURCE] for l in NOISE)
    vs_pinned = {l: s["p24"].get("vs_pinned") for l, s in sums.items()}
    fr = {l: read_frames(sd / l) for l in CONTINUOUS}
    sym, T, I, H = fr[SOURCE]
    cont = {}
    for l in NOISE:
        _s, Tb, _Ib, Hb = fr[l]
        cont[l] = compare_frames(T, H, Tb, Hb, sym)
    trace_p = trace_path(a.scenario)
    tr = read_trace(trace_p) if trace_p is not None else None
    join = None
    if tr is not None:
        ft = [(r.f["frame"], r.f["t"]) for r in tr.records if r.type == TF.FRAME]
        tt = np.array([t for _f, t in ft], np.uint32)
        ti = np.array([f for f, _t in ft], np.uint32)
        join = {"trace_frames": len(ft), "hash_frames": int(len(T)),
                "same_T": bool(len(tt) == len(T) and (tt == T).all()),
                "same_index": bool(len(ti) == len(I) and (ti == I).all())}
    cps = run_checkpoints(sums[SOURCE])
    if tr is not None:
        for c in caps[SOURCE]:
            fi = find_frame(tr, c["ret"])
            c["queued"] = queued_posts(tr, fi) if fi >= 0 else None
    pi = prefix_index(caps[SOURCE])
    segs = select_segments(caps[SOURCE][pi:], cps, T, max_segments=max_segments(a.scenario)) if pi is not None else []
    for s in segs:
        w = frame_window(T, s["start_ret"], s["end_T"])
        s["frame_records"] = int(len(w))
        s["start_frame_index"] = int(I[w[0]]) if len(w) else None
        s["continuous_same"] = all(compare_frames(*window_of(fr[SOURCE], s), *window_of(fr[l], s), sym)["same"]
                                   for l in NOISE)
        if tr is None:
            s["trace"] = {"error": "no trace of this scenario: the recorder delivered no complete trace (trace_tool record; "
                                   "a scenario that ends with the game quitting never reaches the final poll)"}
        else:
            try:
                data, info = slice_trace(tr, s["start_ret"], s["end"])
                st = TF.parse(data)
                s["trace"] = {**info, "bytes": len(data), "valid": TF.validate(st) or "OK"}
                s["inputs"] = input_summary(st)
                s["queued"] = queued_posts(tr, find_frame(tr, s["start_ret"]))
                s["inputs"]["queued"] = len(s["queued"])
                s["inputs"]["inputs"] += len(s["queued"])
            except ValueError as e:
                s["trace"] = {"error": str(e)}
    plan = {"scenario": a.scenario, "prefix": caps[SOURCE][pi]["name"] if pi is not None else None,
            "captures": caps[SOURCE], "captures_agree": agree_caps, "checkpoints": cps, "vs_pinned": vs_pinned,
            "continuous": cont, "trace": str(trace_p.relative_to(ROOT)) if tr is not None else None, "trace_join": join,
            "cap_events": sums[SOURCE]["p24"].get("cap_events", []), "segments": segs, "problems": problems}
    (sd / "plan.json").write_text(json.dumps(plan, indent=1))
    print(f"plan {a.scenario}: {len(caps[SOURCE])} captures (agree {agree_caps}), prefix {plan['prefix']}, "
          f"{len(segs)} segments, continuous {[c['same'] for c in cont.values()]}, trace join {join}", flush=True)
    for s in segs:
        inp = s.get("inputs") or {}
        print(f"  seg {s['id']}: {s['start']} -> {s['end']} frames {s['frames']} cont {s['continuous_same']} "
              f"inputs {inp.get('inputs')} modal_ms {inp.get('modal_ms')} trace {(s.get('trace') or {}).get('valid')}",
              flush=True)
    return 0


def restart_result(sd: pathlib.Path, label: str, seg: dict, exp, sym, end_hash: dict) -> dict:
    """One restart run against the expected frames (records after the start frame) and the end checkpoint's hashes."""
    rd = sd / label
    s = _summary(rd)
    if s is None:
        return {"error": "no summary"}
    if s.get("failure"):
        return {"error": s["failure"][:300]}
    fr = read_frames(rd)
    if fr is None:
        return {"error": "no framehash"}
    gsym, gT, _gI, gH = fr
    if gsym != sym:
        return {"error": "object list differs"}
    sel = (gT > seg["start_ret"]) & (gT < seg["end_T"])
    eT, eH = exp
    res = {"frames": compare_frames(eT, eH, gT[sel], gH[sel], sym)}
    oh = rd / f"{seg['end']}.objhash.json"
    if not oh.exists():
        res["error"] = f"no {seg['end']}.objhash.json"
        res["end_sim"] = {"same": False, "differ": {}}
        return res
    got = json.loads(oh.read_text())["hashes"]
    res["end_sim"] = compare_objhash(end_hash, got, ("sim",))
    res["end_other"] = compare_objhash(end_hash, got, ("hidden", "ui", "render", "engine", "const"))
    res["end_T"] = [seg["end_T"] - T0, s["checkpoints"].get(seg["end"], {}).get("T", 0) - T0]
    rm = (s.get("p24") or {}).get("restore_meta") or {}
    res["restore"] = {k: rm.get(k) for k in ("T", "ret", "regions", "bytes", "timersMissing", "timersDropped", "events")}
    res["wall_s"] = s.get("wall_s")
    return res


def cmd_verify(a) -> int:
    """Verdicts of every planned segment: accepted when both restarts reproduce every expected frame record and the end
    checkpoint's sim hashes, and the three continuous runs agree over the window; class from the trace slice and the
    counterfactual run. Writes log/p2/p24/<scenario>/verify.json."""
    sd = OUT / a.scenario
    plan = load_plan(a.scenario)
    fr = read_frames(sd / SOURCE)
    sym, T, I, H = fr
    rows = []
    for seg in plan["segments"]:
        w = frame_window(T, seg["start_ret"], seg["end_T"])
        post = w[T[w] > seg["start_ret"]]
        end_hash = json.loads((sd / SOURCE / f"{seg['end']}.objhash.json").read_text())["hashes"]
        row = {"id": seg["id"], "start": seg["start"], "end": seg["end"], "frames": int(len(post)),
               "continuous_same": seg["continuous_same"], "restarts": {}}
        for lab in ("r1", "r2"):
            row["restarts"][lab] = restart_result(sd, f"s{seg['id']:02d}_{lab}", seg, (T[post], H[post]), sym, end_hash)
        cf = None
        if (seg.get("inputs") or {}).get("inputs"):
            cfd = sd / f"s{seg['id']:02d}_cf"
            if cfd.exists():
                cf = restart_result(sd, f"s{seg['id']:02d}_cf", seg, (T[post], H[post]), sym, end_hash)
                if "frames" not in cf:
                    cf = {"error": cf.get("error", "?")}
        row["counterfactual"] = cf
        reasons = []
        if not seg["continuous_same"]:
            reasons.append("the three continuous runs differ inside the window")
        if (seg.get("trace") or {}).get("valid") != "OK":
            reasons.append(f"trace slice: {(seg.get('trace') or {}).get('error') or (seg.get('trace') or {}).get('valid')}")
        for lab, r in row["restarts"].items():
            if r.get("error"):
                reasons.append(f"{lab}: {r['error']}")
                continue
            if not r["frames"]["same"]:
                d = r["frames"]["first_divergent"]
                reasons.append(f"{lab}: first divergent frame record {d['record']} T {d['T']} objects "
                               f"{', '.join(d['objects'][:6])}")
            if not r["end_sim"]["same"]:
                reasons.append(f"{lab}: end checkpoint sim objects differ: {', '.join(r['end_sim']['differ'].get('sim', [])[:6])}")
        row["accepted"] = not reasons
        row["reasons"] = reasons
        if (seg.get("trace") or {}).get("valid") != "OK":
            row["class"], row["class_reason"] = "?", "no valid trace slice: the inputs are not known"
        else:
            row["class"], row["class_reason"] = classify(seg.get("inputs") or {"inputs": 0}, cf)
        rows.append(row)
        print(f"  seg {seg['id']} {seg['start']} -> {seg['end']}: {'ACCEPTED' if row['accepted'] else 'REJECTED'} "
              f"{row['class']} frames {row['frames']}" + ("" if row["accepted"] else f" ({'; '.join(reasons)[:300]})"),
              flush=True)
    (sd / "verify.json").write_text(json.dumps({"scenario": a.scenario, "segments": rows}, indent=1))
    return 0


def noise_mask_for(sd: pathlib.Path, start: str):
    """Dword noise of the start image (bytes that differ in c_b or c_c at the capture instant, widened to dwords) plus the
    12 model flag-gap dwords: restart_ab.noise_masks and SymbolMap.gaps."""
    import restart_ab as RA
    nd, _ns, (meta, data, _stack) = RA.noise_masks(sd / SOURCE, [sd / n for n in NOISE], start)
    smap = RA.SymbolMap()
    return nd | smap.gaps, meta, data


def cmd_extract(a) -> int:
    """Corpus files of every accepted segment that is TIME-FORWARD or COMMAND: port/corpus/segments/<scenario>/<k>/
    start.data.bin (the .data image), start.json (instant, clock, events, input, noise runs), trace.trace (the slice),
    frames.bin (expected frame records: the start frame, then every frame head to the end), checkpoints.json (expected
    object hashes of every checkpoint inside the segment), verify.json (the two restarts). Then the manifests."""
    import numpy as np
    import state_image as SI
    sd = OUT / a.scenario
    plan = load_plan(a.scenario)
    ver = {r["id"]: r for r in json.loads((sd / "verify.json").read_text())["segments"]}
    sym, T, I, H = read_frames(sd / SOURCE)
    tp = trace_path(a.scenario)
    tr = read_trace(tp) if tp is not None else None
    L = SI.layout()
    dst_root = CORPUS / a.scenario
    if dst_root.exists():
        shutil.rmtree(dst_root)
    n = 0
    for seg in plan["segments"]:
        v = ver[seg["id"]]
        if not v["accepted"] or v["class"] not in ("TIME-FORWARD", "COMMAND"):
            continue
        d = dst_root / f"{seg['id']:02d}"
        d.mkdir(parents=True, exist_ok=True)
        noise, meta, data = noise_mask_for(sd, seg["start"])
        (d / "start.data.bin").write_bytes(data)
        img = SI.StateImage(data, np.ones(DATA_N, bool), "start")
        start_hash = {k: v2["hash"] for k, v2 in img.hashes({"sim"}).items()}
        w = frame_window(T, seg["start_ret"], seg["end_T"])
        rec0 = {sym[j]: f"{int(H[w[0]][j]):016x}" for j in range(len(sym))}
        start_check = (int(T[w[0]]) == seg["start_ret"]) and rec0 == start_hash
        noise_sim = int((noise & np.isin(L.idx, [i for i, o in enumerate(L.objs) if o["cls"] == "sim"])).sum())
        sj = {"scenario": a.scenario, "segment": seg["id"], "start_checkpoint": seg["start"], "end_checkpoint": seg["end"],
              "class": v["class"], "class_reason": v["class_reason"],
              "data_section": [f"0x{DATA_VA:08x}", f"0x{DATA_N:x}"], "frame_head": f"0x{FRAME_HEAD_RA:08x}",
              "start": {"frame_value": meta["ret"], "frame_value_rel": meta["ret"] - T0, "T": meta["T"],
                        "Qlo": meta["Qlo"], "Qhi": meta["Qhi"], "frame_index": int(I[w[0]]),
                        "tgt_calls": meta.get("tgt"), "qpc_calls": meta.get("qpc"), "events": meta["events"],
                        "timers": meta["timers"], "input": meta["input"]},
              "end": {"T": seg["end_T"], "T_rel": seg["end_T"] - T0},
              "noise": {"file": "noise.bin", "format": "bitmap, bit i (LSB first) = the dword at .data offset 4*i",
                        "dwords": int(noise[:DATA_N - DATA_N % 4].reshape(-1, 4).any(axis=1).sum()),
                        "bytes": int(noise.sum()), "sim_bytes": noise_sim,
                        "from": [SOURCE, *NOISE], "flag_gaps": "re/analysis/data/premise_check_flag_gaps.tsv"},
              "start_frame_hash_check": bool(start_check),
              "hidden_state": "the image is the whole .data section, so it carries g_rng 0x00822d9c and g_lastTipDate "
                              "0x0055e5ac (phase 2.1 H1); a save would not",
              "queued_messages": [{k: q[k] for k in ("msg", "wp", "lp", "cls")} for q in seg.get("queued") or []],
              "objects": "port/include/gamestate_meta.inc", "trace_slice": seg["trace"], "inputs": seg.get("inputs")}
        (d / "start.json").write_text(json.dumps(sj, indent=1))
        (d / "noise.bin").write_bytes(noise_bitmap(noise))
        tdata, _info = slice_trace(tr, seg["start_ret"], seg["end"], {"scenario": a.scenario, "segment": seg["id"]})
        (d / "trace.trace").write_bytes(tdata)
        (d / "frames.bin").write_bytes(pack_frames(T[w], I[w], H[w]))
        (d / "frames.json").write_text(json.dumps({"record": "u32 T (the frame-head clock value), u32 frame-head index "
                                                   "(since boot), u64 sgh64 x n", "classes": ["sim"], "symbols": sym,
                                                   "records": int(len(w)), "first_is_start": True}, indent=1))
        cps = {}
        for cp in plan["checkpoints"]:
            if seg["start_T"] <= cp["T"] <= seg["end_T"]:
                cps[cp["name"]] = {"T": cp["T"],
                                   "hashes": json.loads((sd / SOURCE / f"{cp['name']}.objhash.json").read_text())["hashes"]}
        (d / "checkpoints.json").write_text(json.dumps(cps))
        (d / "verify.json").write_text(json.dumps(v, indent=1))
        n += 1
    print(f"extract {a.scenario}: {n} segments -> {dst_root}", flush=True)
    return write_manifests()


CORPUS_FILES = ("start.data.bin", "start.json", "noise.bin", "trace.trace", "frames.bin", "frames.json",
                "checkpoints.json", "verify.json")


def write_manifests() -> int:
    """port/corpus/segments.json (every scenario's plan / verify rows, corpus files with sha256 and bytes) and the
    committed re/port/segments_manifest.tsv (one row per segment, accepted or not)."""
    entries, rows = [], []
    for sd in sorted(p for p in OUT.iterdir() if p.is_dir()):
        vp = sd / "verify.json"
        if not vp.exists():
            continue
        plan = json.loads((sd / "plan.json").read_text())
        segp = {s["id"]: s for s in plan["segments"]}
        for v in json.loads(vp.read_text())["segments"]:
            s = segp[v["id"]]
            d = CORPUS / sd.name / f"{v['id']:02d}"
            files = {}
            if d.exists():
                for fn in CORPUS_FILES:
                    p = d / fn
                    if p.exists():
                        files[fn] = {"sha256": sha256_file(p), "bytes": p.stat().st_size}
            status = ("accepted" if v["accepted"] and v["class"] in ("TIME-FORWARD", "COMMAND") else
                      "excluded" if v["accepted"] else "rejected")
            e = {"scenario": sd.name, "segment": v["id"], "start": v["start"], "end": v["end"],
                 "start_frame": s.get("start_frame_index"), "frames": v["frames"], "class": v["class"], "status": status,
                 "reason": "; ".join(v["reasons"]) if v["reasons"] else v["class_reason"],
                 "dir": str(d.relative_to(ROOT)).replace("\\", "/") if files else None, "files": files}
            entries.append(e)
            rows.append(e)
    MANIFEST_JSON.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST_JSON.write_text(json.dumps({"schema": 1, "doc": "re/analysis/data/segment_corpus.md",
                                         "generated_by": "re/tools/segment_corpus.py manifest", "segments": entries},
                                        indent=1))
    MANIFEST_TSV.parent.mkdir(parents=True, exist_ok=True)
    head = ["scenario", "segment", "start_checkpoint", "end_checkpoint", "start_frame", "frames", "class", "status"]
    head += [f"sha256_{fn.split('.')[0]}_{fn.split('.')[1]}" for fn in CORPUS_FILES] + ["bytes", "reason"]
    lines = ["\t".join(head)]
    for e in rows:
        f = e["files"]
        lines.append("\t".join([e["scenario"], str(e["segment"]), e["start"], e["end"], str(e["start_frame"]),
                                str(e["frames"]), e["class"], e["status"]]
                               + [f[fn]["sha256"] if fn in f else "-" for fn in CORPUS_FILES]
                               + [str(sum(x["bytes"] for x in f.values())) if f else "0",
                                  e["reason"].replace("\t", " ").replace("\n", " ")[:300]]))
    with MANIFEST_TSV.open("w", encoding="utf-8", newline="\n") as f:      # committed file: LF like the repo
        f.write("\n".join(lines) + "\n")
    acc = sum(e["status"] == "accepted" for e in entries)
    print(f"manifest: {len(entries)} segments, {acc} accepted -> {MANIFEST_JSON.relative_to(ROOT)}, "
          f"{MANIFEST_TSV.relative_to(ROOT)}", flush=True)
    return 0


def cmd_manifest(a) -> int:
    return write_manifests()


def cmd_report(a) -> int:
    """Markdown tables for re/analysis/data/segment_corpus.md: per scenario the captures, prefix, segments by class and
    status, frames, and the reason a scenario gave no segment; totals per class."""
    pinned = json.loads(REFS_MANIFEST.read_text())["scenarios"]
    lines = ["| scenario | captures (in frame) | prefix | segments | accepted TF / CMD | excluded UI-ONLY | rejected | frames "
             "(accepted) | note |", "|---|---|---|---|---|---|---|---|---|"]
    tot = {"TIME-FORWARD": [0, 0], "COMMAND": [0, 0], "UI-ONLY": [0, 0], "rejected": [0, 0]}
    per_scen = {}
    for scen in pinned:
        sd = OUT / scen
        plan = json.loads((sd / "plan.json").read_text()) if (sd / "plan.json").exists() else None
        ver = json.loads((sd / "verify.json").read_text())["segments"] if (sd / "verify.json").exists() else []
        if plan is None:
            lines.append(f"| {scen} | - | - | - | - | - | - | - | not run |")
            continue
        caps = plan.get("captures", [])
        inf = sum(1 for c in caps if (c.get("seen") or 10 ** 9) <= IN_FRAME_SEEN)
        acc = [v for v in ver if v["accepted"] and v["class"] in ("TIME-FORWARD", "COMMAND")]
        tf = sum(v["class"] == "TIME-FORWARD" for v in acc)
        cm = sum(v["class"] == "COMMAND" for v in acc)
        ui = [v for v in ver if v["accepted"] and v["class"] == "UI-ONLY"]
        rej = [v for v in ver if not v["accepted"]]
        fr = sum(v["frames"] for v in acc)
        for v in acc:
            tot[v["class"]][0] += 1
            tot[v["class"]][1] += v["frames"]
        tot["UI-ONLY"][0] += len(ui)
        tot["UI-ONLY"][1] += sum(v["frames"] for v in ui)
        tot["rejected"][0] += len(rej)
        tot["rejected"][1] += sum(v["frames"] for v in rej)
        note = "; ".join(plan.get("problems", []))
        if not plan.get("segments") and not note:
            note = ("no capture inside the frame loop" if plan.get("prefix") is None else
                    "no segment of >= %d frames after the prefix" % MIN_FRAMES)
        if rej:
            note = (note + "; " if note else "") + "; ".join(f"{v['id']}: {v['reasons'][0][:90]}" for v in rej)
        per_scen[scen] = {"accepted": len(acc), "tf": tf, "cmd": cm, "ui": len(ui), "rejected": len(rej), "frames": fr}
        lines.append(f"| {scen} | {len(caps)} ({inf}) | {plan.get('prefix') or '-'} | {len(plan.get('segments', []))} | "
                     f"{tf} / {cm} | {len(ui)} | {len(rej)} | {fr} | {note} |")
    lines += ["", "| class | segments | frames |", "|---|---|---|"]
    for k, (n, f) in tot.items():
        lines.append(f"| {k} | {n} | {f} |")
    print("\n".join(lines))
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps({"scenarios": per_scen, "totals": tot}, indent=1))
    return 0


SCRATCH = ("*.fh.bin", "*.pre.bin", "*.png", "*.window.png", "*.bin", "shim.log", "agent.js", "*.dll.bin", "*.full.bin")
KEEP = ("framehash.bin",)


def cleanup(sd: pathlib.Path) -> int:
    """Scratch files of a scenario's run folders after extraction: images, dumps, frames, logs (framehash.bin, summaries,
    object hashes, plan / verify stay). The continuous runs' frame-head images stay while a segment is rejected (a
    restart needs them). Returns the bytes removed."""
    gone = 0
    vp = sd / "verify.json"
    keep_caps = not vp.exists() or any(not v["accepted"] for v in json.loads(vp.read_text())["segments"])
    for rd in [p for p in sd.iterdir() if p.is_dir()]:
        for pat in SCRATCH:
            for f in rd.glob(pat):
                if f.name in KEEP or (keep_caps and rd.name in CONTINUOUS and f.name.endswith(".fh.bin")):
                    continue
                gone += f.stat().st_size
                f.unlink()
    return gone


SUB_TIMEOUT = {"capture": 2400, "trace": 2400, "restart": 1200}


def _sub(argv: list[str], log: pathlib.Path) -> int:
    """Run one step in a child process. A step that outlives its timeout (measured 2026-10-18: a Frida attach that never
    returned left the game suspended at spawn and the runner waiting for 11 minutes) is killed with its process tree (the
    child and the game it spawned, nothing else) and reported as rc 124."""
    import subprocess
    log.parent.mkdir(parents=True, exist_ok=True)
    cmd = [sys.executable, str(pathlib.Path(__file__).resolve())] + argv
    step = next((x for x in argv if x in SUB_TIMEOUT), None)
    t0 = time.time()
    with log.open("a", encoding="utf-8") as f:
        f.write(f"\n$ {' '.join(argv)}\n")
        f.flush()
        p = subprocess.Popen(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=str(ROOT))
        try:
            rc = p.wait(timeout=SUB_TIMEOUT.get(step))
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(p.pid)], capture_output=True)
            p.wait(timeout=60)
            f.write(f"\nWATCHDOG: killed after {SUB_TIMEOUT[step]} s\n")
            rc = 124
    print(f"  [{time.strftime('%H:%M:%S')}] {' '.join(argv)} -> rc {rc} ({time.time() - t0:.0f} s)", flush=True)
    return rc


def _run_ok(d: pathlib.Path) -> bool:
    s = _summary(d)
    return bool(s and not s.get("failure") and s.get("checkpoints"))


def cmd_pipeline(a) -> int:
    sd = OUT / a.scenario
    log = sd / "pipeline.log"
    inst = ["--instance", a.instance]
    done = sd / "pipeline_done.json"
    if done.exists() and not a.redo:
        print(f"pipeline {a.scenario}: already done ({done}); --redo to run it again", flush=True)
        return 0
    busy = sd / "pipeline_busy.txt"
    if busy.exists() and not a.redo:
        try:
            pid = int(busy.read_text().split()[0])
            import subprocess
            alive = str(pid) in subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], capture_output=True,
                                               text=True).stdout
        except (OSError, ValueError, IndexError):
            alive = False
        if alive:
            print(f"pipeline {a.scenario}: another pipeline (pid {pid}) is running it", flush=True)
            return 0
    sd.mkdir(parents=True, exist_ok=True)
    busy.write_text(f"{os.getpid()} {time.time()}")
    for l in CONTINUOUS:
        if a.redo or not _run_ok(sd / l):
            for attempt in range(2):
                if _sub(inst + ["capture", "--scenario", a.scenario, "--label", l], log) == 0 and _run_ok(sd / l):
                    break
    if trace_path(a.scenario) is None or a.redo:
        for attempt in range(2):
            if _sub(inst + ["trace", "--scenario", a.scenario], log) in (0, 4) or trace_path(a.scenario) is not None:
                break
    if _sub(["plan", "--scenario", a.scenario], log) != 0:
        return 2
    plan = load_plan(a.scenario)
    for seg in plan["segments"]:
        if not seg["continuous_same"]:
            continue
        labs = ["r1", "r2"] + (["cf"] if (seg.get("inputs") or {}).get("inputs") else [])
        for lab in labs:
            d = sd / f"s{seg['id']:02d}_{lab}"
            if not a.redo and _run_ok(d):
                continue
            argv = inst + ["restart", "--scenario", a.scenario, "--segment", str(seg["id"]), "--label",
                           f"s{seg['id']:02d}_{lab}"] + (["--drop-input"] if lab == "cf" else [])
            for attempt in range(2):
                if _sub(argv, log) == 0 and _run_ok(d):
                    break
    _sub(["verify", "--scenario", a.scenario], log)
    rc_x = _sub(["extract", "--scenario", a.scenario], log)
    if a.cleanup:
        print(f"cleanup: {cleanup(sd) / 1e6:.0f} MB removed", flush=True)
    if rc_x == 0:
        done.write_text(json.dumps({"finished": time.strftime("%Y-%m-%dT%H:%M:%S"), "instance": a.instance}))
    busy.unlink(missing_ok=True)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--instance", default="p24")
    sp = ap.add_subparsers(dest="cmd", required=True)
    p = sp.add_parser("capture")
    p.add_argument("--scenario", required=True)
    p.add_argument("--label", required=True)
    p.set_defaults(fn=cmd_capture)
    p = sp.add_parser("trace")
    p.add_argument("--scenario", required=True)
    p.set_defaults(fn=cmd_trace)
    p = sp.add_parser("plan")
    p.add_argument("--scenario", required=True)
    p.set_defaults(fn=cmd_plan)
    p = sp.add_parser("restart")
    p.add_argument("--scenario", required=True)
    p.add_argument("--segment", type=int, required=True)
    p.add_argument("--label", required=True)
    p.add_argument("--drop-input", action="store_true")
    p.add_argument("--corrupt", help="<model symbol>[:delta]: negative control (restart_ab.corrupt on the written image)")
    p.add_argument("--variant", choices=("data", "data+"), default="data+",
                   help="restart_ab write variant: data = .data minus noise; data+ also keeps this process's own value of "
                        "every pointer-valued dword inside pointer-flagged objects and unmodeled bytes")
    p.set_defaults(fn=cmd_restart)
    p = sp.add_parser("verify")
    p.add_argument("--scenario", required=True)
    p.set_defaults(fn=cmd_verify)
    p = sp.add_parser("extract")
    p.add_argument("--scenario", required=True)
    p.set_defaults(fn=cmd_extract)
    p = sp.add_parser("manifest")
    p.set_defaults(fn=cmd_manifest)
    p = sp.add_parser("report")
    p.add_argument("--json")
    p.set_defaults(fn=cmd_report)
    p = sp.add_parser("pipeline")
    p.add_argument("--scenario", required=True)
    p.add_argument("--redo", action="store_true")
    p.add_argument("--cleanup", action="store_true")
    p.set_defaults(fn=cmd_pipeline)
    a = ap.parse_args(argv)
    return a.fn(a) or 0


if __name__ == "__main__":
    sys.exit(main())
