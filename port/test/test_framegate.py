"""The frame gate (roadmap step S0.2, port/test/framegate.py): checkpoints of pinned scenarios rendered with gfx + LiveTerrain and
compared by region with the original's back-buffer frames.

Static part: the region partition, the one-level measure, the gfx.log parser, the waitTicks period, the context classifier and the
terrain rule on the recorded traces, and the NOT_DRAWING list against tick.cpp's kDrops reasons.
Port part (one boot per scenario): sandbox_basic `menu` and cov_b_location's five checkpoints read PASS as test_gfx.py has them;
an in-game course_season checkpoint is a terrain-on-screen yes row; the boot's per-frame sim hashes with gfx and LiveTerrain
attached equal the capture's (boot PASS, every record), with the Terrain object at the pin recording's address and at
LiveTerrain's own; resume and the job cap. Controls RED: gfx negctl 1 on `menu` reads FAIL, a swapped reference reads FAIL.

    ~/bin/py -3.12 -m pytest port/test/test_framegate.py -q
"""
from __future__ import annotations

import json
import re

import numpy as np
import pytest

from port.test import boot as B
from port.test import framegate as FG
from port.test import gfx as GX
from port.test import segments as SG


def _trace(scenario):
    import trace_format as TF
    p = B.TRACES / f"{scenario}.trace"
    if not p.exists() or not (GX.REFS / scenario / "det_b" / "summary.json").exists():
        pytest.skip(f"no trace or reference for {scenario}")
    return TF.read(p)


def _ctx(scenario, checkpoint):
    import trace_format as TF
    tr = _trace(scenario)
    st = json.loads((GX.REFS / scenario / "det_b" / "summary.json").read_text())["checkpoints"][checkpoint]["status"]
    k = 0
    for o, _ra, _sl, kind, v in TF.clock_calls(tr):
        if kind == 1:
            if k == st["tgt"]:
                assert v == st["T"]
                return FG.classify(tr, o)
            k += 1
    raise AssertionError("no tgt call")


# ------------------------------------------------------------------------------------------------ static
@pytest.mark.static
@pytest.mark.parametrize("kind", ["title", "location", "ingame", "frame"])
def test_regions_partition_the_frame(kind):
    lab, names = FG.label_map(kind)
    assert lab.shape == (FG.H, FG.W)
    assert set(np.unique(lab)) <= set(range(len(names)))
    a = np.zeros((FG.H, FG.W, 3), np.uint8)
    b = a.copy()
    b[::7, ::5, 1] = 8           # one 5-bit level on green
    b[::11, ::3, 0] = 24         # three levels on red
    r = FG.region_numbers(a, b, kind)
    d = (a != b).any(axis=2)
    assert sum(v["px"] for v in r.values()) == int(d.sum())
    assert sum(v["area"] for v in r.values()) == FG.W * FG.H
    for v in r.values():
        assert v["px"] == v["one"] + v["beyond"]


@pytest.mark.static
def test_one_level_measure():
    a = np.zeros((FG.H, FG.W, 3), np.uint8)
    b = a.copy()
    b[0, 0] = (8, 0, 0)          # 1 level
    b[0, 1] = (7, 0, 0)          # 0 levels (same 5-bit value)
    b[0, 2] = (16, 8, 0)         # 2 levels
    r = FG.region_numbers(a, b, "frame")["frame"]
    assert (r["px"], r["one"], r["beyond"]) == (2, 1, 1)


@pytest.mark.static
def test_first_region_wins():
    lab, names = FG.label_map("title")
    x, y, _w, _h = dict(FG.menu_region_list())["label_new"]
    assert names[lab[y + 1, x + 1]] == "label_new"          # the label lies inside button_new's rectangle


@pytest.mark.static
def test_gfx_log_parser_and_order():
    log = ("handled\tImage_draw\t3\nunsupported\tno body: playSound\t2\nunsupported\tno body: Widget_writePixel\t5\n"
           "unsupported\tSurface_stretchBlit: a SpriteRef the backend has no sprite for\t1\nsnapshots\t2 of 2\n"
           "snapshot\t0\tunsupported before it\t0\nsnapshot\t1\tunsupported before it\t2\n")
    g = FG.parse_gfx_log(log)
    assert g["handled"] == {"Image_draw": 3} and g["snap_unsup"] == {0: 0, 1: 2}
    assert FG.reason_name("no body: Window::refreshFull") == "Window::refreshFull"
    assert FG.reason_name("Window::refreshFull: a window other than the root") == "Window::refreshFull"
    assert FG.unsupported_before(g, 0, None) == ([], "none")
    probe = {"Widget_writePixel": 10, "playSound": 20, "Surface_stretchBlit": 30}
    assert FG.unsupported_before(g, 1, probe) == (["no body: Widget_writePixel", "no body: playSound"], "probe")
    g2 = FG.parse_gfx_log(log + "first\t0\tno body: playSound\nfirst\t1\tno body: Widget_writePixel\n")
    assert FG.unsupported_before(g2, 1, probe) == (["no body: playSound", "no body: Widget_writePixel"], "gfx")


@pytest.mark.static
def test_not_drawing_names_are_kdrops_with_those_reasons():
    src = (FG.ROOT / "port" / "src" / "sim" / "tick.cpp").read_text()
    drops = dict(re.findall(r'\{"([^"]+)", "([^"]+)"\}', src))
    want = {"capsLockOff": "keybd_event", "emptyFunc_4a0890": "empty function", "waitTicks": "wait loop",
            "saveCourseJpg": "screenshot file", "Widget_readPixel": "pixel read", "enterModal483d30": "modal stack"}
    # the three audio requests left kDrops with S4.10: audio_exe registers them (log/roadmap/S4/10_merge.md section 2)
    audio = {"playSound", "startSoundSlot", "Snd::status"}
    assert set(want) | audio == FG.NOT_DRAWING
    for n, w in want.items():
        assert n in drops and w in drops[n], (n, drops.get(n))
    exe = (FG.ROOT / "port" / "src" / "sim" / "audio_exe.cpp").read_text()
    for n in audio:
        assert n not in drops and f'{{"{n}", ' in exe, n


@pytest.mark.static
def test_wait_ticks_period():
    starts, rows = FG._funcs()
    wt = next(r for r in rows if r[2] == "waitTicks")
    a, b = wt[0] + 1, wt[0] + 2

    def loop(n):
        return ([a] * (10 * n) + [b]) * 6
    assert FG.wait_ticks(loop(1)) == 1
    assert FG.wait_ticks(loop(5)) == 5
    assert FG.wait_ticks([starts[0]]) == -1


@pytest.mark.static
@pytest.mark.parametrize("scenario,checkpoint,frame0,context,terrain", [
    ("sandbox_basic", "menu", True, "BOOT:playAudioFile", "no"),
    ("sandbox_basic", "sandbox_00", False, "H1WAIT", "yes"),
    ("course_season", "hole_open", False, "H1WAIT", "yes"),
    ("cov_b_location", "tip", True, "BOOT:pumpAndRedraw", "no"),
    ("cov_k2_simfoto", "bottom", False, "MODAL:pumpAndRedraw", "yes"),
    ("cov_r_quit", "credits", False, "MODAL_AFTER_H1:showCreditsScreen", "no"),
    ("cov_r_quit", "title1", False, "AFTER_H1:title", "no"),
    ("cov_r_quit", "exit_save", False, "AFTER_H1:waitTicks", "yes"),
    ("cov_k3_career", "prompt", False, "AFTER_H1:waitTicks", "yes"),
])
def test_context_and_terrain_rule(scenario, checkpoint, frame0, context, terrain):
    c = _ctx(scenario, checkpoint)
    assert (c["frame"] == 0) == frame0 and c["context"] == context, c
    assert FG.terrain_rule(c["context"], c["frame"])[0] == terrain


@pytest.mark.static
def test_title_and_location_kinds():
    back, tip = _ctx("cov_b_location", "back"), _ctx("cov_b_location", "tip")
    handled = {"courseActionMenu.setup": 1}
    assert (back["wait_ticks"], tip["wait_ticks"]) == (1, 5)
    assert FG.screen_kind(back["context"], back["frame"], back["wait_ticks"], handled) == "title"
    assert FG.screen_kind(tip["context"], tip["frame"], tip["wait_ticks"], handled) == "location"
    assert FG.screen_kind(tip["context"], tip["frame"], tip["wait_ticks"], {}) == "frame"


@pytest.mark.static
def test_title_after_the_session_is_title_kind():
    c = _ctx("cov_r_quit", "title1")
    assert FG.screen_kind(c["context"], c["frame"], c["wait_ticks"], {}) == "title"


@pytest.mark.static
def test_job_cap():
    assert [FG.effective_jobs(n) for n in (0, 1, 2, 5)] == [1, 1, 2, 2]


# ------------------------------------------------------------------------------------------------ port runs


@pytest.fixture(scope="module")
def portlib():
    from port.test import lib as L
    lib = L.load()
    if not lib.has("gfx.attach"):
        pytest.skip("the library has no gfx module")
    if lib.exe_load():
        pytest.skip("no exe image")
    if not SG.INSTALL.is_dir():
        pytest.skip("no install (original/)")
    if not lib.has("gditext.attach"):
        pytest.skip("no host text provider")
    return lib


_RUNS = {}


def _run(lib, scenario, checkpoints, **kw):
    key = (scenario, tuple(checkpoints), repr(sorted(kw.items())))
    if key not in _RUNS:
        _trace(scenario)
        _RUNS[key] = {r["checkpoint"]: r for r in FG.run_scenario(scenario, checkpoints, lib, probe=False, **kw)}
    return _RUNS[key]


@pytest.mark.port
def test_menu_pass_with_both_attached(portlib):
    r = _run(portlib, "sandbox_basic", ["menu", "sandbox_00"])
    m = r["menu"]
    assert m["status"] == "PASS" and m["total_px"] == 0 and m["kind"] == "title", m
    assert m["unsup_before"] == 0 and m["terrain"] == "no"
    assert m["liveterrain"].startswith("0x") or m["backend"] != "x64"


@pytest.mark.port
def test_sim_hashes_unchanged_with_both_attached(portlib):
    r = _run(portlib, "sandbox_basic", ["menu", "sandbox_00"])["sandbox_00"]
    done, exp = r["boot_records"].split("/")
    assert r["boot_status"] == "PASS" and done == exp, r
    # the Terrain object at LiveTerrain's own address (0) instead of the pin recording's: the same verdict
    r0 = _run(portlib, "sandbox_basic", ["menu", "sandbox_00"], terrain_va=0)["sandbox_00"]
    assert r0["boot_status"] == "PASS" and r0["boot_records"] == r["boot_records"], r0
    assert r0["liveterrain"] != r["liveterrain"] or r["backend"] != "x64"
    assert r0["total_px"] == r["total_px"]


@pytest.mark.port
@pytest.mark.parametrize("chk", ["refused", "site_hover", "buttons", "tip", "back"])
def test_location_checkpoints_pass(portlib, chk):
    r = _run(portlib, "cov_b_location", ["refused", "site_hover", "buttons", "tip", "back"])[chk]
    assert r["status"] == "PASS" and r["total_px"] == 0 and r["unsup_before"] == 0, r
    assert r["kind"] == ("title" if chk == "back" else "location")


@pytest.mark.port
def test_course_view_checkpoint_is_terrain_yes(portlib):
    r = _run(portlib, "course_season", ["hole_open"])["hole_open"]
    assert r["kind"] == "ingame" and r["terrain"] == "yes" and r["context"] == "H1WAIT", r
    assert r["status"] in ("FAIL", "UNSUPPORTED") and r["total_px"] > 0
    assert r["boot_status"] == "PASS"
    names = [x.split("=")[0] for x in r["regions"].split(";")]
    assert names == [n for n, _ in FG.INGAME_REGIONS] + ["view"]


@pytest.mark.port
def test_negctl_gfx1_menu_reads_fail(portlib):
    r = _run(portlib, "sandbox_basic", ["menu"], negctl=("gfx.attach", 1))["menu"]
    assert r["status"] == "FAIL" and r["total_px"] > 0, r


@pytest.mark.port
def test_swapped_reference_reads_fail(portlib):
    r = _run(portlib, "cov_b_location", ["buttons"], swap={"buttons": "cov_b_location/tip"})["buttons"]
    assert r["status"] == "FAIL" and r["total_px"] > 0 and r["reference"] == "cov_b_location/tip", r


@pytest.mark.port
def test_cli_resume(portlib, tmp_path):
    out = tmp_path / "g.tsv"
    args = ["cov_b_location", "--checkpoints", "tip", "--no-probe", "--out", str(out)]
    assert FG.main(args) == 0
    first = out.read_text()
    assert FG.main(args + ["--resume"]) == 0
    assert out.read_text() == first
    rows = [ln.split("\t") for ln in first.splitlines()]
    assert rows[0] == FG.COLUMNS and rows[1][2] == "PASS"
