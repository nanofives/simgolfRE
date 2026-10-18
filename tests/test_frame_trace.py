"""Phase 2.2 checkpoint tracer: opt-in gating in scenario_ab.py / scenario_det.js and the comparisons of
re/tools/frame_trace.py on synthetic run folders. The game runs that prove it (two OFF runs identical on sandbox_basic and
course_season, a perturbation located at its frame and object) are recorded in re/analysis/data/gamestate_gen.md; the
CModule hash was checked against state_image.sgh64 in a 32-bit process (log/p2/p22_notes.txt)."""
import json
import os
import re
import struct
import subprocess
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import frame_trace as FT  # noqa: E402
import state_image as SI  # noqa: E402

pytestmark = pytest.mark.static

JS = (ROOT / "re" / "frida" / "js" / "scenario_det.js").read_text(encoding="utf-8")
C_SRC = (ROOT / "re" / "frida" / "js" / "framehash_cm.c").read_text(encoding="utf-8")

PROBE = r"""
import json, pathlib, sys, tempfile
sys.argv = [sys.argv[0]]
sys.path.insert(0, r"%s")
import scenario_ab as s
d = pathlib.Path(tempfile.mkdtemp())
r = s.Run("x", "sandbox_basic", "replay", {}, [], [], out_dir=d)
first = r.agent_source().read_text().split("\n", 1)[0]
print(json.dumps({"cfg": json.loads(first[len("const SCENARIO_CFG = "):-1]), "obj": r.obj_hash, "fh": r.frame_hash,
                  "pt": r.perturb}))
"""


def _probe(env):
    e = {k: v for k, v in os.environ.items() if not k.startswith("C3FT_")}
    e.update(env)
    out = subprocess.run([sys.executable, "-c", PROBE % (ROOT / "re" / "frida")], capture_output=True, text=True,
                         timeout=180, cwd=str(ROOT), env=e)
    assert out.returncode == 0, out.stderr[-3000:]
    return json.loads(out.stdout.strip().splitlines()[-1])


def test_default_agent_config_has_no_tracer_key():
    r = _probe({})
    assert "frameHash" not in r["cfg"] and not r["obj"] and r["fh"] is None and r["pt"] is None


def test_frame_hash_config_lists_the_sim_objects():
    r = _probe({"C3FT_FRAMEHASH": "sim", "C3FT_OBJHASH": "1", "C3FT_PERTURB": "822d9c:1@20000"})
    fh = r["cfg"]["frameHash"]
    sims = [o for o in SI.layout().objs if o["cls"] == "sim"]
    assert fh["ra"] == 0x0040FC2F and len(fh["objs"]) == len(sims) == len(r["fh"]["symbols"])
    assert fh["objs"][0] == [SI.layout().base + sims[0]["offset"], sims[0]["size"]]
    assert "fh_frame" in fh["src"] and r["obj"] == "1"
    assert r["pt"] == {"va": 0x822D9C, "xor": 1, "t": 20000, "done_at": None}


def test_tracer_paths_are_gated_in_the_agent():
    # the stub bytes come only from frameHashBytes, which returns [] without cfg.frameHash
    assert "b.push(...frameHashBytes());" in JS
    assert re.search(r"function frameHashBytes\(\) \{\s*if \(!CFG\.frameHash\) return \[\];", JS)
    assert "let FH = null;\nif (CFG.frameHash) {" in JS
    assert "if (reply.frh && CFG.frameHash) {" in JS
    # x32 is a new action kind: only the opt-in perturbation sends it
    assert "case 'x32':" in JS


def test_stub_bytes_preserve_eax_and_skip_other_calls():
    m = re.search(r"return \[0x81, 0x3c, 0x24, \.\.\.u32\(CFG\.frameHash\.ra >>> 0\), 0x75, 12,\s*"
                  r"0x50, 0x50, 0xff, 0x15, \.\.\.d\(0x50\), 0x83, 0xc4, 0x04, 0x58\];", JS)
    assert m
    # jne skips exactly the 12 bytes push eax; push eax; call [abs32]; add esp, 4; pop eax
    assert 1 + 1 + 6 + 3 + 1 == 12


def test_c_hash_is_the_python_hash():
    # the same constants and steps as state_image.sgh64 / _mix
    for k in ("0x5BD1E995u", "0x9E3779B97F4A7C15ULL", "0xBF58476D1CE4E5B9ULL", "0x94D049BB133111EBULL"):
        assert k in C_SRC
    assert SI.K_XOR == 0x5BD1E995 and SI.K_MUL == 0x9E3779B97F4A7C15
    assert "fh_mix(s ^ (u64) n)" in C_SRC


def _write_run(d, symbols, recs, objhash=None, dropped=0):
    d.mkdir(parents=True, exist_ok=True)
    blob = b"".join(struct.pack("<II", t, i) + b"".join(struct.pack("<Q", h) for h in hs) for t, i, hs in recs)
    (d / "framehash.bin").write_bytes(blob)
    (d / "framehash.json").write_text(json.dumps({"symbols": symbols, "fetches": [{"count": len(recs),
                                                                                    "dropped": dropped}]}))
    for name, hs in (objhash or {}).items():
        (d / f"{name}.objhash.json").write_text(json.dumps({"checkpoint": name, "hashes": hs}))


def test_compare_frames_locates_the_first_divergent_frame_and_objects(tmp_path):
    syms = ["g_rng", "g_date", "g_golfers"]
    a = [(100 + 10 * i, i, [1, 2, 3]) for i in range(6)]
    b = [list(r) for r in a]
    b[3] = (130, 3, [9, 2, 3])
    b[4] = (140, 4, [9, 2, 7])
    _write_run(tmp_path / "a", syms, a, {"k1": {"g_rng": ["sim", "x"], "g_sounds": ["audio", "p"]}})
    _write_run(tmp_path / "b", syms, [tuple(r) for r in b], {"k1": {"g_rng": ["sim", "y"], "g_sounds": ["audio", "q"]}})
    fr = FT.compare_frames(tmp_path / "a", tmp_path / "b")
    d = fr["first_divergent"]
    assert (d["record"], d["T"], d["objects"], d["divergent_records"], d["previous_T"]) == (3, [130, 130], ["g_rng"], 2, 120)
    assert not fr["same"]
    oh = FT.compare_objhash(tmp_path / "a", tmp_path / "b")
    assert oh["checkpoints"]["k1"] == {"sim": ["g_rng"], "audio": ["g_sounds"]} and oh["first_divergent"] == "k1"
    assert FT.compare_objhash(tmp_path / "a", tmp_path / "b", {"audio"})["checkpoints"]["k1"] == {"audio": ["g_sounds"]}


def test_compare_frames_same_and_timing_divergence(tmp_path):
    syms = ["g_rng"]
    a = [(100 + i, i, [5]) for i in range(4)]
    _write_run(tmp_path / "a", syms, a)
    _write_run(tmp_path / "b", syms, a)
    assert FT.compare_frames(tmp_path / "a", tmp_path / "b")["same"]
    _write_run(tmp_path / "c", syms, a[:2] + [(999, 2, [5])] + a[3:])
    d = FT.compare_frames(tmp_path / "a", tmp_path / "c")["first_divergent"]
    assert d["record"] == 2 and d["objects"] == [] and d["T"] == [102, 999]
    _write_run(tmp_path / "e", syms, a, dropped=1)
    assert not FT.compare_frames(tmp_path / "a", tmp_path / "e")["same"]       # dropped records are never SAME
