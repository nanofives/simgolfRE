"""The port's exe-side Terrain calls against the original's on a pinned scenario (phase 5 step 3, re/PHASE5_TERRAIN_PLAN.md section 14).

A recording pin_<scenario> (re/tools/terrain_calls.py --scenario <scenario>, exported with port.test.terraincorpus) holds every exported
Terrain call the ORIGINAL made while scenario_refs.py replayed the pinned schedule (the run SAME as the pinned reference), and a copy
of Terrain.dll's state at each checkpoint. The port boots on the same scenario's trace (port/test/boot.py, the boot gate) with
LiveTerrain attached (sg_ts_live_attach: its TerrainApi is the port's own Terrain.dll state); its calls are logged the same way and
its state is copied after the same number of calls as each of the original's copies.

Compared: the call sequences, restricted to the methods TerrainApi carries (the exe's other Terrain calls, drawLine and the
splines, are overlay drawing the port's exe side drops), with tile pointers as tile indices, passCollarInfo's table pointer and
initSystem's DC as don't-care; and at every copy the object (the context handle and the draw list's head node as zero / nonzero),
the vertex and normal arrays, the texture table and the view quadrant. The port's object is placed at the original's address, so
the neighbour links compare directly.

    ~/bin/py -3.12 -m port.test.terrainlive [scenario ...]
"""
from __future__ import annotations

import ctypes as C
import json
import pathlib
import struct
import sys
import zlib

import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from port.test import boot, glcorpus as G, terraincorpus as TC  # noqa: E402

API = {"getInstance", "passCollarInfo", "loadNewCourseType", "setType", "layPath", "setWall", "calcNormals", "calcAllNormals",
       "elevateCorner", "lowerCorner", "lowerEdgeCorner", "updatePath", "initSystem", "initTerrain", "setZoomLevel", "render",
       "localRender", "pathUpdateRender", "resetTerrain"}
NV = G.N_VERTICES
OBJ = 0x164AD0


def pinned() -> list[str]:
    return sorted(d.name[4:] for d in TC.CORPUS.glob("pin_*") if (d / "index.json").exists() and TC.snaps(d))


def bind(dll):
    P, U32, I32 = C.c_void_p, C.c_uint32, C.c_int32
    dll.sg_ts_live_attach.restype, dll.sg_ts_live_attach.argtypes = C.c_int, [P, U32, P, I32]
    dll.sg_ts_live_detach.restype, dll.sg_ts_live_detach.argtypes = C.c_int, [P]
    dll.sg_ts_live_log.restype, dll.sg_ts_live_log.argtypes = C.c_int, [P, C.c_char_p, C.c_size_t, C.POINTER(C.c_size_t)]
    dll.sg_ts_live_snapshot.restype = I32
    dll.sg_ts_live_snapshot.argtypes = [P, I32, P, P, P, P, C.POINTER(I32), C.POINTER(U32)]
    dll.sg_ts_negctl.restype, dll.sg_ts_negctl.argtypes = C.c_int, [I32]


# ---- the sg_ts_live_* calls on either backend: x64 through ctypes on the DLL, Win32 through the runner's ops (lib32.State32,
# roadmap S3.2 / S0 recommendation 5)
def _win32(st) -> bool:
    return hasattr(st, "live_attach")


def live_attach(lib, st, va: int, ks=()) -> int:
    if _win32(st):
        return st.live_attach(va, ks)
    bind(lib.dll)
    arr = (C.c_uint32 * max(1, len(ks)))(*ks) if ks else None
    return lib.dll.sg_ts_live_attach(st.ctx, va, arr, len(ks))


def live_detach(lib, st) -> int:
    return st.live_detach() if _win32(st) else lib.dll.sg_ts_live_detach(st.ctx)


def live_object(lib, st) -> int:
    if _win32(st):
        return st.live_object()
    lib.dll.sg_ts_live_object.restype, lib.dll.sg_ts_live_object.argtypes = C.c_uint32, [C.c_void_p]
    return lib.dll.sg_ts_live_object(st.ctx)


def live_log(lib, st) -> str:
    if _win32(st):
        return st.live_log()
    n = C.c_size_t(0)
    lib.dll.sg_ts_live_log(st.ctx, None, 0, C.byref(n))
    b = C.create_string_buffer(n.value + 1)
    lib.dll.sg_ts_live_log(st.ctx, b, n.value + 1, C.byref(n))
    return b.raw[:n.value].decode()


def live_snapshots(lib, st) -> list:
    """[(object, vertices, normals, textures, view)] of every copy the LiveTerrain kept"""
    out = []
    if _win32(st):
        for k in range(st.live_snapshot(-1)):
            after, view, o, v, nn, t = st.live_snapshot(k, OBJ, NV * 3, G.N_TEXTURES)
            out.append((o, v, nn, t, view))
        return out
    dll = lib.dll
    for k in range(dll.sg_ts_live_snapshot(st.ctx, -1, None, None, None, None, None, None)):
        o, v, nn = (C.c_uint8 * OBJ)(), (C.c_float * (NV * 3))(), (C.c_float * (NV * 3))()
        t, view, aft = (C.c_uint32 * G.N_TEXTURES)(), C.c_int32(), C.c_uint32()
        dll.sg_ts_live_snapshot(st.ctx, k, o, v, nn, t, C.byref(view), C.byref(aft))
        out.append((bytes(o), bytes(v), bytes(nn), bytes(t), view.value))
    return out


def canon(calls: list) -> list[str]:
    """The comparable form of a call log: TerrainApi methods only, tile pointers as T<index>, the object as O."""
    base = calls[0].ret
    out = []
    for c in calls:
        if c.name not in API:
            continue
        a = []
        for i, v in enumerate(c.args):
            if c.name == "passCollarInfo" and i == 0:
                a.append("*")
                continue
            if c.name == "initSystem" and i == 2:
                a.append("dc" if v else "0")
                continue
            o = v - base - 0x3A4
            if 0 <= o < 2500 * 0x248 and o % 0x248 == 0:
                a.append(f"T{o // 0x248}")
            elif v == base and v:
                a.append("O")
            else:
                a.append(hex(v))
        out.append(" ".join([c.name] + a + [f"{k}={v}" for k, v in c.extra.items()]))
    return out


class Result:
    def __init__(self, scenario):
        self.scenario = scenario
        self.boot = None
        self.orig: list[str] = []
        self.port: list[str] = []
        self.first_call_diff = None     # (index, orig, port)
        self.states: dict[str, dict] = {}
        self.limit = None              # calls compared (valid_until), None = all

    @property
    def calls_equal(self) -> bool:
        return self.first_call_diff is None and len(self.orig) == len(self.port)


def run(lib, scenario: str) -> Result:
    if getattr(lib, "dll", None) is not None:
        bind(lib.dll)
    root = TC.CORPUS / ("pin_" + scenario)
    calls = TC.calls(root)
    names = TC.snaps(root)
    res = Result(scenario)
    seqs = {n: TC.TSnap(root, n).seq for n in names}
    api_seqs = [c.seq for c in calls if c.name in API]
    pos = {n: sum(1 for q in api_seqs if q < seqs[n]) for n in names}
    # valid_until (index.json): the last checkpoint at which the recorded run was still SAME as the pinned reference; a recording
    # whose later part the recorder perturbed (cov_r_reload, U-1078) is compared only up to there
    until = json.loads((root / "index.json").read_text()).get("valid_until")
    res.limit = pos[until] if until else None
    if until:
        names = [n for n in names if pos[n] <= res.limit]
    res.orig = canon(calls)[:res.limit] if until else canon(calls)
    order = sorted(names, key=lambda n: pos[n])
    ks = [pos[n] for n in order]
    got = {}
    log = []

    def before(st):
        assert live_attach(lib, st, calls[0].ret, ks) == 0

    def after(st, _res):
        log.extend(TC.Call(ln) for ln in live_log(lib, st).splitlines())
        for k, s in enumerate(live_snapshots(lib, st)):
            got[order[k]] = s
        live_detach(lib, st)

    res.boot = boot.run(scenario, lib, before=before, after=after)
    res.port = canon(log) if log else []
    if res.limit is not None:
        res.port = res.port[:res.limit]
    for i, (a, b) in enumerate(zip(res.orig, res.port)):
        if a != b:
            res.first_call_diff = (i, a, b)
            break
    else:
        if len(res.orig) != len(res.port):
            i = min(len(res.orig), len(res.port))
            res.first_call_diff = (i, res.orig[i] if i < len(res.orig) else "<end>", res.port[i] if i < len(res.port) else "<end>")
    for nm in order:
        res.states[nm] = state_diff(got.get(nm), TC.TSnap(root, nm))
    return res


def state_diff(got, snap) -> dict:
    if got is None:
        return {"missing": 1}
    o, v, n, t, view = got
    a, b = np.frombuffer(o, np.uint8).copy(), np.frombuffer(snap.tobj, np.uint8).copy()
    handles = 0
    for off in TC.HANDLE_FIELDS:
        handles += int(a[off:off + 4].any() != b[off:off + 4].any())
        a[off:off + 4] = b[off:off + 4]
    rd = lambda rva, cnt: np.frombuffer(snap.tdata, "<u4", cnt, rva - G.DATA_RVA0)
    return {"object": int((a != b).sum()), "handles": handles,
            "vertices": int((np.frombuffer(v, "<u4") != rd(G.RVA["vertices"], NV * 3)).sum()),
            "normals": int((np.frombuffer(n, "<u4") != rd(G.RVA["normals"], NV * 3)).sum()),
            "textures": int((np.frombuffer(t, "<u4") != rd(G.RVA["textures"], G.N_TEXTURES)).sum()),
            "view": int(view != struct.unpack_from("<i", snap.tdata, G.RVA["view"] - G.DATA_RVA0)[0])}


# ------------------------------------------------------------------------------------------------ the course view (roadmap S3.2)
# A recording with --pass-frames (re/tools/terrain_calls.py --scenario <s> --pass-frames) keeps, at each checkpoint, the root surface
# the original's GL draws into around the last updateTerrainRender before it: frames/<checkpoint>/{entry, exit, pass<j>_pre,
# pass<j>_post}.bin (RGB555, 800 x 600, memory order = rows top-down) and meta.json (the frame's ordinal from 0, its arguments, the
# passes with their kinds and arguments). The port boots on the same trace with the graphics backend and LiveTerrain attached and
# copies its root surface at the same points (terrainLive.keepFrame); frame_compare judges each copy by the G2 criterion.
FRAME_W, FRAME_H = 800, 600
PASS_KIND = {"stripRender": 1, "render": 2, "localRender": 3, "pathUpdateRender": 4}


def recorded_frames(label: str) -> dict:
    """checkpoint -> {"meta": dict, "<what>": uint16 array (600, 800)} of log/p5/terrain/<label>/frames"""
    d = ROOT / "log" / "p5" / "terrain" / label / "frames"
    out = {}
    for c in sorted(d.iterdir()) if d.is_dir() else []:
        m = json.loads((c / "meta.json").read_text())
        if m.get("none"):
            continue
        e = {"meta": m}
        for f in list(c.glob("*.bin")) + list(c.glob("*.z")):
            raw = zlib.decompress(f.read_bytes()) if f.suffix == ".z" else f.read_bytes()
            e[f.stem] = np.frombuffer(raw, "<u2").reshape(FRAME_H, FRAME_W)
        out[c.name] = e
    return out


def port_frames(lib, scenario: str, frames: list[int], checkpoint: str, negctl: tuple[str, int] | None = None,
                trace: pathlib.Path | None = None, defects: dict[str, bool] | None = None) -> dict:
    """Boot `scenario` up to `checkpoint`'s budget with gfx + LiveTerrain; {(frame, what): uint16 (600, 800)} for each kept frame
    (what = entry, exit, p<j>_pre, p<j>_post), plus "log" (gfx.log text), "boot" (the BootResult), "passes", "frames".
    trace: a study scenario's trace (run_trace: to its end, nothing compared; `checkpoint` is not used).
    defects: compatibility switches set for the run ({name: on}), reset to their defaults after it (roadmap S3.4c: a recording
    made on an install the stock game cannot run, such as the tropical Tree alias of U-1330, runs at the switch's fixed value)."""
    from port.test import gfx as GX
    root = TC.CORPUS / ("pin_" + scenario)
    va = TC.calls(root)[0].ret if (root / "calls.txt").exists() else 0
    max_records, to_end = GX.record_budget(scenario, GX.checkpoint_ordinal(scenario, checkpoint)) if trace is None else (0, True)
    out = {}

    def before(st):
        st.call("gfx.attach", [])
        assert live_attach(lib, st, va) == 0
        for k in frames:
            st.call("terrainLive.keepFrame", [k])

    def after(st, _res):
        for k in frames:
            for what in ["entry", "exit"] + [f"p{j}_{w}" for j in range(8) for w in ("pre", "post")]:
                raw = st.files_read(f"$gfx/terrain_{k}_{what}.555")
                if raw is not None and len(raw) == FRAME_W * FRAME_H * 2:
                    out[(k, what)] = np.frombuffer(raw, "<u2").reshape(FRAME_H, FRAME_W).copy()
        out["passes"] = st.call("terrainLive.passes", []).eax
        out["frames"] = st.call("terrainLive.frames", []).eax
        st.call("gfx.log", [])
        t = st.files_read("$gfx/log.txt")
        out["log"] = t.decode() if t else ""
        st.call("gfx.detach", [])
        live_detach(lib, st)

    prev = lib.negctl(negctl[0], negctl[1]) if negctl else None
    for name, on in (defects or {}).items():
        lib.defect(name, on)      # raises SgError when the tree has no such switch
    try:
        if trace is None:
            out["boot"] = boot.run(scenario, lib, max_records=max_records, before=before, after=after, to_end=to_end)
        else:
            out["boot"] = run_trace(lib, trace, scenario, before, after)
    finally:
        if negctl:
            lib.negctl(negctl[0], prev or 0)
        if defects:
            lib.defect_reset()
    return out


class TraceRun:
    """The result of run_trace (a boot over a trace with no capture to compare: status RAN or the sg_run_boot error)."""

    def __init__(self, status, detail, records):
        self.status, self.detail, self.records = status, detail, records


def run_trace(lib, trace: pathlib.Path, scenario: str, before=None, after=None) -> TraceRun:
    """sg_run_boot over `trace` (a study scenario's, outside port/corpus/traces, so the boot gate never sees it) to its end, set up
    as port/test/boot.py does (the exe image, the install, the scenario's files, the GDI text provider). Nothing is compared: the
    caller reads what `after` collects."""
    import os
    from port.test import segment_files, segments as SG
    sys.path.insert(0, str(ROOT / "re" / "tools"))
    import trace_format as TF
    n = sum(1 for r in TF.read(trace).records if r.type == TF.FRAME)
    why = lib.exe_load()
    if why:
        return TraceRun("MISSING", f"exe: {why}", 0)
    with lib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.fill_from_exe(True)
        if SG.INSTALL.is_dir():
            st.files_root(SG.INSTALL)
        for fname, data in segment_files.files_for(scenario).items():
            st.files_put(fname, data)
        gdi = lib.has("gditext.attach") and os.environ.get("SG_TEXT_METRICS", "gdi") == "gdi"
        if gdi:
            st.call("gditext.attach", [])
        try:
            if before:
                before(st)
            code, done, _ts, _hs = st.run_boot(trace.read_bytes(), n + 1)
            if after:
                after(st, None)
        finally:
            if gdi:
                st.call("gditext.detach", [])
    return TraceRun("RAN" if code in (0, -11) else "ERROR", f"sg_run_boot {code} after {done} of {n} frame records", done)


def g2(orig, port, pre=None) -> dict:
    """The G2 numbers of one copy: pixels one 5-bit level off at most and further off, over the pixels the original's frame drew
    (changed from `pre`, the copy before the frame, when given; else every pixel)."""
    from port.test import glraster as GR
    o, p = orig.reshape(-1), port.reshape(-1)
    drawn = (o != pre.reshape(-1)) if pre is not None else np.ones(o.shape, bool)
    rgb = lambda a: np.stack([(a >> 10) & 31, (a >> 5) & 31, a & 31], axis=1).astype(int)
    dd = np.abs(rgb(o) - rgb(p)).max(axis=1)
    diff = o != p
    one, big = int(((dd == 1) & diff).sum()), int(((dd > 1) & diff).sum())
    n = int(drawn.sum())
    ok = one <= GR.MAX_ONE_LEVEL * max(n, 1) and big <= GR.MAX_BIG
    return {"drawn": n, "diff": int(diff.sum()), "one": one, "big": big, "pass": bool(ok)}


def main():
    from port.test import lib as L
    lib = L.load()
    for s in sys.argv[1:] or pinned():
        r = run(lib, s)
        bad = {k: d for k, d in r.states.items() if any(d.values())}
        print(f"{s:18s} boot {r.boot.status:7s} calls orig {len(r.orig)} port {len(r.port)} "
              f"{'EQUAL' if r.calls_equal else 'DIFF at ' + str(r.first_call_diff)} states {len(r.states) - len(bad)}/{len(r.states)} equal",
              flush=True)
        for k, d in bad.items():
            print(f"    {k}: {d}")


if __name__ == "__main__":
    main()
