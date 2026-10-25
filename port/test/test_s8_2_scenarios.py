"""Roadmap S8.2: the original's intro and closing movies measured (re/analysis/video/movie_runs.md, log/roadmap/S8/2_notes.md).

Static (no game, no DLL): the scenario module registers its two scenarios (lockstep callables, expect_exit on the quitting one, names in
scenario_refs' s36a family); the analysis functions of re/tools/s8_movie_runs.py work on synthetic data (movie segments, the QPC steps per
export, the stable-dword attribution of a state difference); the movie strings the runs name are in the user's exe at the addresses the
notes cite; and, when log/roadmap/S8/2_results.json exists, its numbers satisfy the checker below and movie_runs.md quotes them.
Controls (each reads RED on the checker or the function it changes): a movie that ends before its last frame, a flipped stable dword, a
run whose identity flag is false, a results file with another frame count.

    ~/bin/py -3.12 -m pytest port/test/test_s8_2_scenarios.py -q
"""
from __future__ import annotations

import copy
import json
import pathlib
import struct
import sys

import numpy as np
import pytest

ROOT = pathlib.Path(__file__).resolve().parents[2]
for _p in (ROOT / "re" / "tools", ROOT / "re" / "frida", ROOT / "log" / "c3"):
    if str(_p) not in sys.path:
        sys.path.insert(0, str(_p))

pytestmark = pytest.mark.static

RESULTS = ROOT / "log" / "roadmap" / "S8" / "2_results.json"
DOC = ROOT / "re" / "analysis" / "video" / "movie_runs.md"
EXE = ROOT / "original" / "golf_clean.exe"


def runs():
    import s8_movie_runs as M
    return M


# ---------------------------------------------------------------- scenarios
def test_scenarios_registered():
    import scenario_s8_2 as S
    import scenario as SC
    for name in ("cov_r_s8_intro", "cov_r_s8_full"):
        e = SC.SCENARIOS[name]
        assert callable(e["lockstep"]) and name.startswith("cov_r")
    assert getattr(SC.SCENARIOS["cov_r_s8_full"]["lockstep"], "expect_exit", False)
    assert not getattr(SC.SCENARIOS["cov_r_s8_intro"]["lockstep"], "expect_exit", False)
    assert S.SCENARIOS is SC.SCENARIOS


def test_scenario_hook_in_scenario_py_loads_this_module():
    """re/tools/scenario.py's end-of-file hook globs scenario_s[0-9]*_*.py (S8 merge item 5, widened by S2 wave 4), so it registers
    this module; the driver still imports scenario_s8_2 itself."""
    import fnmatch
    src = (ROOT / "re" / "tools" / "scenario.py").read_text(encoding="utf-8")
    assert 'scenario_s[0-9]*_*.py' in src
    assert fnmatch.fnmatch("scenario_s8_2.py", "scenario_s[0-9]*_*.py")
    assert "import scenario_s8_2" in (ROOT / "re" / "tools" / "s8_movie_runs.py").read_text(encoding="utf-8")


# ---------------------------------------------------------------- analysis on synthetic data
def bink_rows(frames=3, end_frame=None, null_open=False):
    """[seq, T, name, args, ret, extra, Q] rows of one movie as the recorder writes them (qstep 10000)."""
    rows, q, seq = [], 0, 0

    def add(name, args, ret, extra=None):
        nonlocal seq
        rows.append([seq, 2, name, args, ret, extra, q])
        seq += 1
    if null_open:
        add("_BinkOpen@8", [1, 0x1000000], 0, {"name": "./flics/SMSG_introfinal.bik"})
        return rows
    q += 60000
    add("_BinkSetIOSize@4", [0x9c4000], 0x9c4000)
    q += 60000
    add("_BinkOpen@8", [1, 0x1000000], 99, {"name": "./flics/SMSG_x.bik", "hdr": [800, 600, frames, 1, 0xFFFFFFFF, 15, 1, 0]})
    for i in range(frames):
        n = 62
        rows.append([seq, 2, "_BinkWait@4", [99], 1, [n, 2, 2, q, q + 10000 * (n - 1)], q])
        seq += 1
        q += 10000 * n
        rows.append([seq, 2, "_BinkWait@4", [99], 0, [1, 2, 2, q + 10000, q + 10000], q + 10000])
        seq += 1
        q += 10000
        q += 30000
        add("_BinkDoFrame@4", [99], 0)
        add("_BinkBufferLock@4", [7], 1)
        add("_BinkCopyToBuffer@28", [99, 1, 3200, 600, 0, 0, 0x80000003 if i == 0 else 3], 0)
        if i < frames - 1:
            q += 10000
            add("_BinkNextFrame@4", [99], 1)
    fn = frames if end_frame is None else end_frame
    q += 10000
    add("_BinkClose@4", [99], 1, {"hdr": [800, 600, frames, fn, fn, 15, 1, 0]})
    return rows


def test_movie_segments_clip_end_and_virtual_time():
    M = runs()
    bink = {"rows": bink_rows(), "notes": [{"info": {"qstep": 10000, "step": 1}}]}
    (sg,) = M.movie_segments(bink)
    assert sg["name"] == "./flics/SMSG_x.bik" and sg["open_hdr"][2] == 3
    assert sg["calls"]["_BinkWait@4"] == 3 * 63 and sg["calls"]["_BinkDoFrame@4"] == 3 and sg["calls"]["_BinkNextFrame@4"] == 2
    assert sg["ended_by"].startswith("clip end") and sg["virtual_ms_t"] == 0
    assert sg["virtual_ms_q"] == (sg["q_close"] - sg["q_open"]) / 10000
    q = M.qpc_per_export({"rows": sg["rows"], "notes": bink["notes"]})
    assert q["_BinkDoFrame@4"] == 3 * 3 and q["_BinkNextFrame@4"] == 2 and q["_BinkClose@4"] == 1
    assert q["_BinkWait@4"] > 0 and q["_BinkBufferLock@4"] == 0 and q["_BinkCopyToBuffer@28"] == 0


def test_movie_segments_control_early_end_reads_early():
    M = runs()
    bink = {"rows": bink_rows(frames=5, end_frame=3), "notes": [{"info": {"qstep": 10000, "step": 1}}]}
    (sg,) = M.movie_segments(bink)
    assert sg["ended_by"].startswith("early (FrameNum 3 of 5)")


def test_open_returning_null_is_a_skipped_movie():
    M = runs()
    (sg,) = M.movie_segments({"rows": bink_rows(null_open=True), "notes": []})
    assert sg["open_returned_null"] and "q_close" not in sg


def write_images(tmp_path, groups):
    """groups: {label: {dword_index: value}} over a zero image of 0x381174 bytes; returns {label: path}."""
    import state_image as SI
    n = SI.layout().n
    out = {}
    for lab, d in groups.items():
        a = np.zeros(n // 4, "<u4")
        for i, v in d.items():
            a[i] = v
        p = tmp_path / f"{lab}.bin"
        p.write_bytes(a.tobytes() + b"\0" * (n - a.nbytes))
        out[lab] = p
    return out


def test_image_diff_attributes_only_stable_dwords(tmp_path):
    import state_image as SI
    M = runs()
    L = SI.layout()
    stable = (0x0083AB9C - L.base) // 4
    heap = stable + 100
    imgs = write_images(tmp_path, {
        "on1": {stable: 1, heap: 0x03900000}, "on2": {stable: 1, heap: 0x03910000},
        "sk1": {stable: 0, heap: 0x03800000}, "sk2": {stable: 0, heap: 0x03810000}})
    d = M.image_diff([imgs["on1"], imgs["on2"]], [imgs["sk1"], imgs["sk2"]])
    assert [x["va"] for x in d["attributed"]] == ["0x0083ab9c"]
    assert d["attributed"][0]["symbol"] == "g_ui83ab9c" and d["attributed"][0]["on"] == "0x00000001"
    assert d["unstable_on"] == 1 and d["unstable_skip"] == 1


def test_image_diff_control_flipped_stable_dword_is_attributed(tmp_path):
    import state_image as SI
    M = runs()
    L = SI.layout()
    stable = (0x0083AB9C - L.base) // 4
    other = (0x00822D9C - L.base) // 4          # g_rng: a sim word the intro must not move
    imgs = write_images(tmp_path, {
        "on1": {stable: 1, other: 5}, "on2": {stable: 1, other: 5}, "sk1": {stable: 0}, "sk2": {stable: 0}})
    d = M.image_diff([imgs["on1"], imgs["on2"]], [imgs["sk1"], imgs["sk2"]])
    assert sorted(x["va"] for x in d["attributed"]) == ["0x00822d9c", "0x0083ab9c"]


def test_image_diff_same_conditions_attribute_nothing(tmp_path):
    M = runs()
    imgs = write_images(tmp_path, {"a": {7: 1}, "b": {7: 1}, "c": {7: 1}, "d": {7: 1}})
    d = M.image_diff([imgs["a"], imgs["b"]], [imgs["c"], imgs["d"]])
    assert d["attributed"] == [] and d["differ_first_runs"] == 0


# ---------------------------------------------------------------- the user's exe names the movies where the notes say
def test_movie_strings_in_exe():
    if not EXE.exists():
        pytest.skip("no original/golf_clean.exe")
    import pefile
    pe = pefile.PE(str(EXE))
    base = pe.OPTIONAL_HEADER.ImageBase
    for va, text in ((0x004C8A0C, b"./flics/SMSG_introfinal.bik"), (0x004C6BB4, b"./flics/SMSG_closingfinal.bik")):
        assert pe.get_data(va - base, len(text) + 1) == text + b"\0"


# ---------------------------------------------------------------- the results file and the write-up
def check_results(d: dict) -> list[str]:
    """Problems in a 2_results.json; empty = every measured claim of movie_runs.md holds."""
    bad = []
    full = d["scenarios"]["cov_r_s8_full"]
    for sc, e in d["scenarios"].items():
        for cond in ("on", "skip"):
            if not e[f"identity_{cond}"]["all_identical"]:
                bad.append(f"{sc}: runs of {cond} differ")
    for cp, v in full["statediff"].items():
        want = ["0x0083ab9c"] if cp in ("intro_early", "menu", "hover_exit") else []
        got = [x["va"] for x in v["attributed"]]
        if got != want:
            bad.append(f"full {cp}: attributed {got}, expected {want}")
    mv = {m["name"]: m for m in full["movies_on"]}
    for name, frames in (("./flics/SMSG_introfinal.bik", 825), ("./flics/SMSG_closingfinal.bik", 589)):
        m = mv.get(name)
        if not m:
            bad.append(f"no movie {name}")
            continue
        if m["open_hdr"][2] != frames or m["close_hdr"][3] != frames or not m["ended_by"].startswith("clip end"):
            bad.append(f"{name}: frames {m['open_hdr'][2]} / {m['close_hdr'][3]} / {m['ended_by']}")
        if m["calls"].get("_BinkDoFrame@4") != frames:
            bad.append(f"{name}: DoFrame calls {m['calls'].get('_BinkDoFrame@4')}")
        if m["virtual_ms_t"] != 0:
            bad.append(f"{name}: timeGetTime advanced {m['virtual_ms_t']} ms inside the movie")
        if "_BinkSetSoundSystem@8" in m["calls"]:
            bad.append(f"{name}: BinkSetSoundSystem called")
    added = full["clock_added_by_intro"]
    if list(added) != ["qpc binkw32.dll binkw32.dll+b6e7"] or added["qpc binkw32.dll binkw32.dll+b6e7"][2] != 54945:
        bad.append(f"clock added by the intro: {added}")
    skip_first = full["movies_skip"][0]
    if not skip_first["open_returned_null"]:
        bad.append("the skipped intro's BinkOpen did not return NULL")
    return bad


@pytest.fixture(scope="module")
def results():
    if not RESULTS.exists():
        pytest.skip("log/roadmap/S8/2_results.json not generated (`s8_movie_runs.py analyze report`)")
    return json.loads(RESULTS.read_text())


def test_results_satisfy_the_checker(results):
    assert check_results(results) == []


def test_results_control_wrong_frame_count_reads_red(results):
    bad = copy.deepcopy(results)
    bad["scenarios"]["cov_r_s8_full"]["movies_on"][0]["close_hdr"][3] = 800
    assert any("frames" in p for p in check_results(bad))


def test_results_control_unstable_run_reads_red(results):
    bad = copy.deepcopy(results)
    bad["scenarios"]["cov_r_s8_intro"]["identity_on"]["all_identical"] = False
    assert any("differ" in p for p in check_results(bad))


def test_results_control_extra_attributed_dword_reads_red(results):
    bad = copy.deepcopy(results)
    bad["scenarios"]["cov_r_s8_full"]["statediff"]["menu"]["attributed"].append({"va": "0x00822d9c"})
    assert any("attributed" in p for p in check_results(bad))


def test_doc_quotes_the_measured_numbers(results):
    text = DOC.read_text(encoding="utf-8")
    full = results["scenarios"]["cov_r_s8_full"]
    for m in full["movies_on"]:
        for n in (m["calls"]["_BinkDoFrame@4"], m["calls"]["_BinkWait@4"], m["wait_nonzero_calls"]):
            assert f"{n:,}" in text, n
        assert f"{int(m['virtual_ms_q']):,} ms" in text
    assert f"{full['clock_added_by_intro']['qpc binkw32.dll binkw32.dll+b6e7'][2]:,}" in text
    assert f"{full['identity_on']['clock_total']:,}" in text and f"{full['identity_skip']['clock_total']:,}" in text
    for word in ("probably", "likely", "seems to", "appears to", "I think", "presumably", "might be", "maybe"):
        assert word not in text
