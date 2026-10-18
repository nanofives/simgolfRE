"""Port scope manifest (phase 2.0, 2026-10-18; re/port/scope_rules.md): the rules on a synthetic mini graph, and the
committed scope.tsv / simcore_order.tsv against the function list and the GameState object classes."""
import csv
import re
import sys

import pytest

from conftest import ROOT

pytestmark = pytest.mark.static

sys.path.insert(0, str(ROOT / "re" / "tools"))
import port_scope as PS  # noqa: E402


def fn(addr, name, tag="-", calls=(), phases=(), boot=False, touch=None, size=10, tag_src="own"):
    return dict(addr=addr, name=name, size=size, tag=tag, tag_src=tag_src, calls=list(calls), phases=list(phases), depth=1,
                boot=boot, touch=touch or {})


OBJS = [["g_cash", 0x500000, 4, "sim"], ["g_rng", 0x500010, 4, "hidden"], ["g_win", 0x500020, 4, "pointer"],
        ["f_ptrtab_x", 0x500030, 4, "pointer"], ["g_snd", 0x500040, 4, "audio"]]
MINI = dict(objects=OBJS, hooks={}, defects={}, fns=[
    fn(0x401000, "payFee", "economy", [0x401100], ["P5"], touch={"g_cash": [1, 1]}),
    fn(0x401100, "clampInt", "util", [], ["P5"]),
    fn(0x401200, "drawCash", "ui", [], ["H1"], touch={"g_cash": [1, 0], "g_win": [1, 0]}),
    fn(0x401300, "spendLog", "ui", [], ["P5"], touch={"g_cash": [0, 1], "f_ptrtab_x": [1, 0]}, tag_src="callers"),
    fn(0x401400, "initBoot", "boot", [], [], boot=True),
    fn(0x401500, "playIt", "audio", [], ["P5"], touch={"g_snd": [1, 0]}),
    fn(0x401600, "mixed", "golfer", [], ["P5"], touch={"g_cash": [1, 0], "g_win": [1, 0]}),
    fn(0x4a5000, "memcpy", "crt", [], ["P5"]),
    fn(0x401700, "showScreen", "economy", [], ["H1"], touch={"g_cash": [1, 0]}),
])


def test_rules_on_a_mini_graph():
    res, F = PS.classify(MINI)
    mod = {a: r["module"] for a, r in res.items()}
    assert len(res) == len(MINI["fns"])
    assert mod[0x401000] == "sim-core" and res[0x401000]["rule"] == "R6-SIM"
    assert mod[0x401100] == "sim-leaf-util" and res[0x401100]["rule"] == "R10-LEAF"
    assert mod[0x401200] == "ui" and res[0x401200]["straddler"] == 1
    assert mod[0x401300] == "sim-core" and res[0x401300]["rule"] == "R6-SIM-RETAG"     # a constant table is not a heap object
    assert mod[0x401400] == "boot-only"
    assert mod[0x401500] == "backend-audio"
    assert mod[0x401600] == "sim-core" and res[0x401600]["straddler"] == 1
    assert mod[0x4A5000] == "crt-dropped"
    assert mod[0x401700] == "ui" and res[0x401700]["rule"] == "R8-UI-NAME"


def test_order_is_leaves_first():
    res, F = PS.classify(MINI)
    ordr, level = PS.order(F, res)
    assert ordr.index(0x401100) < ordr.index(0x401000)
    assert level[0x401100] == 0 and level[0x401000] == 1


def test_order_survives_a_cycle():
    inp = dict(MINI, fns=[fn(0x401000, "a", "sim", [0x401010], ["P5"], touch={"g_cash": [1, 0]}),
                          fn(0x401010, "b", "sim", [0x401000], ["P5"], touch={"g_cash": [1, 0]})])
    res, F = PS.classify(inp)
    ordr, _ = PS.order(F, res)
    assert sorted(ordr) == [0x401000, 0x401010]


def rows(name):
    p = ROOT / "re" / "port" / name
    if not p.exists():
        pytest.skip(f"{name} not generated")
    return list(csv.DictReader(open(p, encoding="utf-8"), delimiter="\t"))


def test_scope_covers_every_function_once():
    scope = rows("scope.tsv")
    listed = [l.split("\t")[0] for l in (ROOT / "re" / "functions_ghidra.tsv").read_text().splitlines()[1:]]
    got = [r["addr"] for r in scope]
    assert len(got) == len(set(got))
    assert sorted(int(a, 16) for a in got) == sorted(int(a, 16) for a in listed)
    assert {r["port_module"] for r in scope} <= {"sim-core", "sim-leaf-util", "formats-loaders", "backend-gfx", "backend-audio",
                                                   "ui", "boot-only", "crt-dropped", "net", "engine-glue", "unknown"}
    assert all(r["reach_reason"] for r in scope)


def test_sim_objects_of_sim_core_are_listed():
    meta = (ROOT / "port" / "include" / "gamestate_meta.inc").read_text()
    simsyms = set(re.findall(r"SG_OBJECT\((\w+), 0x[0-9a-f]+, 0x[0-9a-f]+, sim,", meta))
    scope = rows("scope.tsv")
    order = {r["addr"]: r for r in rows("simcore_order.tsv")}
    for r in scope:
        listed = [s for s in r["sim_objects"].split(";") if s]
        assert len(listed) == int(r["touches_sim_objects"])
        assert set(listed) <= simsyms
        if r["port_module"] in ("sim-core", "sim-leaf-util"):
            assert r["addr"] in order
            assert set(listed) <= set(order[r["addr"]]["globals_sim"].split(";"))
    strad = {r["addr"] for r in rows("scope_straddlers.tsv")}
    assert strad == {r["addr"] for r in scope if r["straddler"] == "1"}
    # leaves first: no function precedes a callee that is in the list with a higher level
    lv = [int(r["level"]) for r in rows("simcore_order.tsv")]
    assert lv == sorted(lv)
