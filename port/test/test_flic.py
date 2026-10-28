"""Roadmap S3.4: the FLIC sprite-sheet decoder (port/src/sim/flic.cpp, module `flic`) against the original's sheet memory.

The oracle is re/tools/flic_cells_rec.py's recordings (port/corpus/flic/<scenario>/<label>/, local): for every loadFlic call
of a hooks-off run, its arguments, the count word it read, and per cell the SpriteRef record and the jgld Sprite behind it
(object words, pixel buffer), plus the palette entries of the load's slot and of the shadow slot afterwards. The port
decodes the same .flc files from the user's install (GameFiles over original/) and must give, per load, the same column
count and rows, the same number of cells and per cell the same cropSprite result, SpriteRef geometry (+0x10 .. +0x28),
Sprite words (bpp 8, size, pitch, no palette, no row spans) and pixel bytes, and the same palettes.

    ~/bin/py -3.12 -m pytest port/test/test_flic.py              # every recorded load (loadWorldAssets' are the gate)
    ~/bin/py -3.12 port/test/test_flic.py [--all]                # the report: per recording, loads / cells equal

Recordings: sandbox_basic (boot + Parkland), cov_b_career_types (Links, Desert, Parkland), cov_b_s34_hawaii (Tropical, with
the install's Tree_Cerc / Tree_Drac copied to the exe's `Tree Cerc` / `Tree Drac` names in the instance, U-1701) and
cov_b_s34_hawaii/stock (the stock install: the failed open the port reports as kMainOpenFailed).
"""
from __future__ import annotations

import collections
import ctypes
import json
import pathlib
import struct
import sys

import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from port.test import lib as L  # noqa: E402

CORPUS = ROOT / "port" / "corpus" / "flic"
EXE = ROOT / "original" / "golf_clean.exe"
NAME_VA = 0x10000000          # a harness block for the strings flic.load reads
WA_LO, WA_HI = 0x0043DBE0, 0x00442180   # loadWorldAssets: the loads the S3.4 gate covers
STATUS = {0: "ok", 1: "main open failed", 2: "shadow open failed", 3: "no next frame", 4: "decode fault", 5: "unconverted",
          6: "bad crop"}
RECORDINGS = [("sandbox_basic", "flic"), ("cov_b_career_types", "flic"), ("cov_b_s34_hawaii", "flic")]
# The recordings made with flic_cells_rec.py --tree-alias: the instance held copies of Tree_Cerc / Tree_Drac under the exe's
# names, so the port gets the same copies in its files overlay (the install is never written)
ALIASED = {("cov_b_s34_hawaii", "flic")}
ALIASES = [("Flics/Trees/Tropic/Tree_Cerc", "flics/trees/Tropic/Tree Cerc"),
           ("Flics/Trees/Tropic/Tree_Drac", "flics/trees/Tropic/Tree Drac")]


def exe_names() -> tuple[str, str, str]:
    """loadFlic's file-name parts from the user's exe: the prefix (pool 0x004c8a28 + 0x3c), the main suffix (+0x28) and
    the shadow suffix (+0x30)."""
    import pefile
    pe = pefile.PE(str(EXE), fast_load=True)
    img = pe.get_memory_mapped_image()
    base = pe.OPTIONAL_HEADER.ImageBase

    def s(va):
        o = va - base
        return img[o:img.index(b"\0", o)].decode("latin-1")
    return s(0x004C8A64), s(0x004C8A50), s(0x004C8A58)


# ---------------------------------------------------------------- the recordings
def read_recording(scenario: str, label: str) -> list[dict]:
    d = CORPUS / scenario / label
    loads = [json.loads(x) for x in (d / "loads.jsonl").read_text().splitlines() if x]
    blob = (d / "cells.bin").read_bytes()
    for ld in loads:
        off, n = ld["bin"]
        data = blob[off:off + n]
        o = 0
        for c in ld["cells"]:
            c["rec"] = data[o:o + c["recLen"]]
            o += c["recLen"]
            c["obj"] = data[o:o + c.get("objLen", 0)]
            o += c.get("objLen", 0)
            c["px"] = data[o:o + c.get("pxLen", 0)]
            o += c.get("pxLen", 0)
            c["spans"] = data[o:o + c.get("spanLen", 0)]
            o += c.get("spanLen", 0)
        for p in ld.get("pals", []):
            p["rgb"] = data[o:o + p["len"]]
            o += p["len"]
        if o != n:
            raise ValueError(f"{scenario}/{label} load {ld['seq']}: {n - o} bytes left over")
    return loads


def available() -> list[tuple[str, str]]:
    return [(s, l) for s, l in RECORDINGS if (CORPUS / s / l / "loads.jsonl").exists()]


# ---------------------------------------------------------------- the port
class Port:
    def __init__(self, lib, alias: bool = False):
        self.lib = lib
        self.st = lib.state()
        self.buf = self.st.map(NAME_VA, bytearray(0x1000))
        self.st.files_root(str(ROOT / "original"))
        if alias:
            for src, dst in ALIASES:
                for f in sorted((ROOT / "original" / src).glob("*.flc")):
                    self.st.files_put(dst + "\\" + f.name, f.read_bytes())
        pre, main, shadow = exe_names()
        self.names_va = self._put(0x800, pre.encode() + b"\0" + main.encode() + b"\0" + shadow.encode() + b"\0")

    def _put(self, off: int, raw: bytes) -> int:
        ctypes.memmove(ctypes.addressof(self.buf) + off, raw, len(raw))
        return NAME_VA + off

    def load(self, path: str, frames: int, size: int, flag: int) -> dict:
        va = self._put(0, path.encode("latin-1") + b"\0")
        status = self.st.call("flic.load", [va, frames & 0xFFFFFFFF, size & 0xFFFFFFFF, flag & 0xFFFFFFFF, self.names_va]).eax
        raw = self.st.files_read("$flic/load.bin")
        st, cols, seqs, shadow, n = struct.unpack_from("<5i", raw, 0)
        o = 20
        cells = []
        for _ in range(n):
            w = struct.unpack_from("<9i", raw, o)
            o += 36
            cells.append({"result": w[0], "keep_w": w[1], "keep_h": w[2], "w": w[3], "h": w[4], "left": w[5], "top": w[6],
                          "kind": w[7], "px": raw[o:o + w[8]]})
            o += w[8]
        pals = []
        for _ in range(2):
            pals.append((struct.unpack_from("<i", raw, o)[0], raw[o + 4:o + 4 + 0x300]))
            o += 4 + 0x300
        assert st == (status if status < 0x80000000 else status - (1 << 32))
        return {"status": st, "cols": cols, "seqs": seqs, "shadow": bool(shadow), "cells": cells, "pal": pals[0],
                "shadow_pal": pals[1], "detail": raw[o:].decode("latin-1")}

    def close(self):
        self.st.close()


# ---------------------------------------------------------------- comparison
def compare(rec: dict, got: dict) -> list[str]:
    """The differences between one recorded load and the port's decode (empty = equal)."""
    out = []
    a = rec["args"]
    size = a[3] - (1 << 32) if a[3] & 0x80000000 else a[3]
    failed = [o for o in rec["opens"] if o["ret"] == 0]
    if failed:
        if got["status"] != 1:
            out.append(f"the original's open failed ({failed[0]['path']}), port status {STATUS.get(got['status'])}")
        return out
    if got["status"] != 0:
        return [f"port status {STATUS.get(got['status'])}: {got['detail']}"]
    if got["cols"] != rec["count"]:
        out.append(f"cols {got['cols']} != count word {rec['count']}")
    rows = 1 if size == -1 else a[2]
    if rec["rows"] != rows:
        out.append(f"rows {rec['rows']} != frames argument {rows}")
    if len(got["cells"]) != len(rec["cells"]):
        out.append(f"{len(got['cells'])} cells, the original cut {len(rec['cells'])}")
    for k, (rc, pc) in enumerate(zip(rec["cells"], got["cells"])):
        bad = []
        if rc["ret"] != pc["result"]:
            bad.append(f"result {pc['result']} != {rc['ret']}")
        rw = struct.unpack_from("<11i", rc["rec"], 0)          # +0x00 .. +0x28
        want = {"+0x08": 0, "+0x0c": 0, "+0x10": pc["keep_w"], "+0x14": pc["keep_h"], "+0x18": pc["w"], "+0x1c": pc["h"],
                "+0x20": pc["left"], "+0x24": pc["top"], "+0x28": pc["kind"]}
        if rc["ret"] == 3:   # cropSprite stops before it writes the geometry
            want = {}
        for i, key in enumerate(["+0x00", "+0x04", "+0x08", "+0x0c", "+0x10", "+0x14", "+0x18", "+0x1c", "+0x20", "+0x24",
                                 "+0x28"]):
            if key in want and rw[i] != want[key]:
                bad.append(f"SpriteRef {key} {rw[i]} != port {want[key]}")
        if rc["ret"] == 0:
            ow = struct.unpack_from("<14I", rc["obj"], 0)
            sprite = {"m_10 palette": (ow[4], 0), "m_18 flags": (ow[6] & 1, 0), "m_20 bpp": (ow[8], 8),
                      "m_24 bytes": (ow[9], len(pc["px"])), "m_2c pitch": (ow[11], pc["keep_w"]),
                      "m_30 width": (ow[12], pc["keep_w"]), "m_34 height": (ow[13], pc["keep_h"])}
            for key, (have, port) in sprite.items():
                if have != port:
                    bad.append(f"Sprite {key} {have} != port {port}")
            if rc["px"] != pc["px"]:
                n = sum(1 for x, y in zip(rc["px"], pc["px"]) if x != y) + abs(len(rc["px"]) - len(pc["px"]))
                bad.append(f"{n} pixel bytes differ")
        if bad:
            out.append(f"cell {k}: " + "; ".join(bad[:4]))
            if len(out) > 8:
                break
    pals = {p["slot"]: p for p in rec.get("pals", [])}
    main = pals.get(rec["id"])
    if main is not None:
        if main["len"] and (not got["pal"][0] or bytes(got["pal"][1]) != main["rgb"]):
            out.append("slot palette differs" if got["pal"][0] else "slot palette: the port decoded none")
        if not main["len"] and got["pal"][0]:
            out.append("the port has a slot palette, the original none")
    sh = pals.get(590)
    if got["shadow"] and sh is not None and sh["len"]:
        if not got["shadow_pal"][0] or bytes(got["shadow_pal"][1]) != sh["rgb"]:
            out.append("shadow palette differs")
    return out


def unique_loads(loads: list[dict], wa_only: bool) -> list[dict]:
    """One recorded load per distinct (path, frames, size, flag); the original's cells for equal arguments must agree."""
    seen: dict = {}
    for ld in loads:
        if wa_only and not (WA_LO <= ld["caller"] < WA_HI):
            continue
        key = (ld["path"], *ld["args"][2:5])
        seen.setdefault(key, ld)
    return list(seen.values())


def run_all(lib, wa_only: bool = True, recordings=None, limit: int = 0) -> dict:
    res = {"loads": 0, "equal": 0, "cells": 0, "fail": [], "themes": collections.Counter(), "by_rec": {}}
    for scen, label in recordings or available():
        port = Port(lib, (scen, label) in ALIASED)
        try:
            loads = unique_loads(read_recording(scen, label), wa_only)
            if limit:
                loads = loads[:limit]
            eq = 0
            for ld in loads:
                a = ld["args"]
                got = port.load(ld["path"], a[2], a[3], a[4])
                diff = compare(ld, got)
                res["loads"] += 1
                res["cells"] += len(ld["cells"])
                if diff:
                    res["fail"].append((scen, ld["seq"], ld["path"], a[2:5], diff))
                else:
                    eq += 1
                    res["equal"] += 1
            res["by_rec"][f"{scen}/{label}"] = (eq, len(loads))
        finally:
            port.close()
    return res


# ---------------------------------------------------------------- tests
@pytest.fixture(scope="module")
def flib(portlib):
    if not portlib.has("flic.load"):
        pytest.skip("this portsim build has no module flic (log/roadmap/S3/4_merge.md section 1)")
    return portlib


def _need(scen, label):
    if not (CORPUS / scen / label / "loads.jsonl").exists():
        pytest.skip(f"no recording {scen}/{label} (re/tools/flic_cells_rec.py record)")


@pytest.mark.parametrize("scen,label", RECORDINGS)
def test_world_loads_equal_the_original(flib, scen, label):
    _need(scen, label)
    r = run_all(flib, True, [(scen, label)])
    assert r["loads"] > 0
    assert not r["fail"], r["fail"][:5]


@pytest.mark.parametrize("scen,label", RECORDINGS[:1])
def test_boot_loads_equal_the_original(flib, scen, label):
    """loadInterfaceArt's loads at boot use the same loadFlic: every one of them too."""
    _need(scen, label)
    r = run_all(flib, False, [(scen, label)])
    assert r["loads"] > 300
    assert not r["fail"], r["fail"][:5]


def test_tree_cerc_names_fail_like_the_original(flib):
    """U-1701: on the stock install the original's first `Tree Cerc` open fails (flicOpenStream returns NULL, the game dies in
    loadFlic's Image_draw); the port (switch flic.treeAliasNames at its default 1) raises a Fault for the same load (S3.4b,
    D5; before it reported kMainOpenFailed)."""
    _need("cov_b_s34_hawaii", "stock")
    run = json.loads((CORPUS / "cov_b_s34_hawaii" / "stock" / "run.json").read_text())
    assert len(run["failed_opens"]) == 1
    f = run["failed_opens"][0]
    assert "Tree Cerc" in f["path"] and f["open"]["ret"] == 0
    port = Port(flib)
    try:
        a = f["args"]
        flib.defect_reset()
        with pytest.raises(L.SgError) as e:
            port.load(f["path"], a[2], a[3], a[4])
        assert e.value.code == L.SG_E_FAULT
        ok = port.load(f["path"].replace("Tree Cerc", "Tree_Cerc"), a[2], a[3], a[4])
    finally:
        flib.defect_reset()
        port.close()
    assert ok["status"] == 0 and ok["cells"]


def test_sheet_to_gfx_draws_the_recorded_cells(flib):
    """The menu.cpp hook-up (flic::sheet_to_gfx, log/roadmap/S3/4_merge.md section 2): with a gfx backend attached, a load's
    cells become backend sprites under their g_cellPool addresses; the backend's Image_draw of each cell onto the root surface
    puts the original's recorded pixels (bytes < 0xfe, through the slot palette) at (left, top)."""
    if not flib.has("gfx.attach"):
        pytest.skip("no gfx module")
    _need("sandbox_basic", "flic")
    loads = read_recording("sandbox_basic", "flic")
    ld = next(x for x in loads if WA_LO <= x["caller"] < WA_HI and x["args"][4] and len(x["cells"]) >= 4)
    pal = next(p for p in ld["pals"] if p["slot"] == ld["id"])["rgb"]
    import numpy as np
    if not flib.exe_loaded():
        flib.exe_load()
    with flib.state() as st:
        st.fill_from_exe()
        st.files_root(str(ROOT / "original"))
        buf = st.map(NAME_VA, bytearray(0x1000))
        raw = ld["path"].encode("latin-1") + b"\0" + b"Image_draw\0"
        ctypes.memmove(ctypes.addressof(buf), raw, len(raw))
        st.call("gfx.attach", [])
        a = ld["args"]
        r = st.call("flic.toGfx", [NAME_VA, a[2], a[3] & 0xFFFFFFFF, a[4], 7]).eax
        if r == 0xFFFFFFFF:
            pytest.skip("this build has no gfx body interface (gfx_body.h, roadmap S2 wave 1): sheet_to_gfx is a no-op")
        assert r == 0
        cell0 = st.data_base + 0xE9A30 + 0x2C * 7          # g_cellPool's offset in the GameState
        name_va = NAME_VA + len(ld["path"]) + 1
        lut = np.array([((pal[3 * i] >> 3) << 10) | ((pal[3 * i + 1] >> 3) << 5) | (pal[3 * i + 2] >> 3) for i in range(256)],
                       np.uint16)
        # roadmap S3.11: with the scene family in the tree a sheet cell draws with a palette OBJECT (its queue entry's, U-1332
        # resolved by S3.5), not with the slot palette, so the cell is drawn with the slot palette given as Image_draw's mask
        # (gfx_scene.setPalette fills the object); without the scene family the mask stays 0 and the core path draws as before
        sp = b"gfx_scene.setPalette\0"
        ctypes.memmove(ctypes.addressof(buf) + 0x400, sp, len(sp))
        ctypes.memmove(ctypes.addressof(buf) + 0x800, lut.tobytes(), 0x200)
        pal_key = NAME_VA + 0xF00
        mask = pal_key if st.call("gfx.call", [NAME_VA + 0x400, pal_key, NAME_VA + 0x800] + [0] * 9).eax == 1 else 0
        drawn = 0
        for k, c in enumerate(ld["cells"][:4]):
            if c["ret"] != 0:
                continue
            assert st.call("gfx.call", [name_va, cell0 + 0x2C * k, 0, 0, 0, mask] + [0] * 6).eax == 1
            assert st.call("gfx.dump", [1]).eax == 0
            img = np.frombuffer(st.files_read("$gfx/root.555"), "<u2").reshape(0x258, 0x320)
            rw = struct.unpack_from("<11i", c["rec"], 0)
            kw, kh, left, top = rw[4], rw[5], rw[8], rw[9]
            px = np.frombuffer(c["px"], np.uint8).reshape(kh, kw)
            region = img[top:top + kh, left:left + kw]
            keep = px < 0xFE
            assert (region[keep] == lut[px[keep]]).all(), (k, ld["path"])
            drawn += 1
    assert drawn >= 2


# The negative controls: each wrong body must change at least one recorded load's comparison.
NEGCTL = [(1, "the first word-delta chunk of each stream skipped"), (2, "the colour chunks ignored"),
          (3, "a sheet row's frames cut in reverse order"), (4, "byte 0xfe drawn (transparency test > 0xfe)")]


@pytest.mark.parametrize("mode,what", NEGCTL)
def test_negctl_red(flib, mode, what):
    recs = available()
    if not recs:
        pytest.skip("no recordings")
    prev = flib.negctl("flic.load", mode)
    try:
        r = run_all(flib, True, recs[:1] if mode != 4 else recs)
    finally:
        flib.negctl("flic.load", prev or 0)
    assert r["fail"], f"mode {mode} ({what}) reads GREEN: an equivalent mutant on these recordings"


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true", help="also the boot loads (loadInterfaceArt)")
    ap.add_argument("--negctl", type=int, default=0)
    a = ap.parse_args()
    lib = L.load()
    if a.negctl:
        lib.negctl("flic.load", a.negctl)
    r = run_all(lib, not a.all)
    for k, (eq, n) in r["by_rec"].items():
        print(f"{k}: {eq} / {n} loads equal")
    print(f"total {r['equal']} / {r['loads']} loads equal ({r['cells']} recorded cells)")
    for f in r["fail"][:20]:
        print("FAIL", f)
    return 0 if not r["fail"] else 1


if __name__ == "__main__":
    sys.exit(main())
