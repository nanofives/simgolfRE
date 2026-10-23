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
    bind(lib.dll)
    dll = lib.dll
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
    ks = (C.c_uint32 * len(order))(*[pos[n] for n in order])
    got = {}
    log = []

    def before(st):
        assert dll.sg_ts_live_attach(st.ctx, calls[0].ret, ks, len(order)) == 0

    def after(st, _res):
        n = C.c_size_t(0)
        dll.sg_ts_live_log(st.ctx, None, 0, C.byref(n))
        b = C.create_string_buffer(n.value + 1)
        dll.sg_ts_live_log(st.ctx, b, n.value + 1, C.byref(n))
        log.extend(TC.Call(ln) for ln in b.raw[:n.value].decode().splitlines())
        for k in range(dll.sg_ts_live_snapshot(st.ctx, -1, None, None, None, None, None, None)):
            o, v, nn = (C.c_uint8 * OBJ)(), (C.c_float * (NV * 3))(), (C.c_float * (NV * 3))()
            t, view, aft = (C.c_uint32 * G.N_TEXTURES)(), C.c_int32(), C.c_uint32()
            dll.sg_ts_live_snapshot(st.ctx, k, o, v, nn, t, C.byref(view), C.byref(aft))
            got[order[k]] = (bytes(o), bytes(v), bytes(nn), bytes(t), view.value)
        dll.sg_ts_live_detach(st.ctx)

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
