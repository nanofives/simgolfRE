"""S3.4b: the `fixed`-profile switch flic.treeAliasNames (port/src/sim/flic.cpp). The stock game dies at loadWorldAssets' first
`Tree Cerc` load on every tropical course (the exe asks for names with a space, the install holds Tree_Cerc / Tree_Drac:
recording port/corpus/flic/cov_b_s34_hawaii/stock, run log/roadmap/S3/4_rec_hawaii_stock.log, instance s3_4). Default 1 = the
original: a Fault. 0 = the install's underscore names are loaded.
(1) ON the missing-file load raises SG_E_FAULT, (2) OFF every tropical world load decodes equal to the alias recording
(the original reading the same files under the exe's names), (3) a completing control (loads that name no tree) is identical
ON and OFF, (4) a wrong body (negctl 5: the alias applied at 1) reads RED on the fault probe, (5) profiles.py lists the switch
in `fixed`. Notes: log/roadmap/S3/4b_notes.md."""
from __future__ import annotations

import json

import pytest

from port.test import lib as L
from port.test import profiles
from port.test import test_flic as TF

pytestmark = pytest.mark.port

SWITCH = "flic.treeAliasNames"


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    portlib.negctl_reset()
    yield
    portlib.defect_reset()
    portlib.negctl_reset()


@pytest.fixture
def flib(portlib):
    if not portlib.has("flic.load"):
        pytest.skip("this portsim build has no module flic")
    return portlib


def _tree(path: str) -> bool:
    return "Tree Cerc" in path or "Tree Drac" in path


def test_switch_exists_and_defaults_on(flib):
    d = flib.defects()
    assert SWITCH in d and d[SWITCH]["on"] == 1


PROFILE_ROW = (f"{SWITCH}\t0\tD5\tloadWorldAssets' tropical `Tree Cerc` / `Tree Drac` loads (the install holds Tree_Cerc / "
               "Tree_Drac) fault in the original: load the install's names instead\n")


def test_switch_is_in_the_fixed_profile(flib, tmp_path):
    """The member row is S6's file (re/port/defect_profiles.tsv; the line is in log/roadmap/S3/4b_merge.md). Until the
    integrator applies it, the table plus that line is validated, and once applied the real table must carry it."""
    real = profiles.DEFAULT_TABLE
    text = pathlib_read(real)
    if SWITCH in text:
        assert profiles.validate(flib).get(SWITCH) == 0
        return
    t = tmp_path / "defect_profiles.tsv"
    t.write_text(text.rstrip("\n") + "\n" + PROFILE_ROW, encoding="utf-8")
    assert profiles.validate(flib, t).get(SWITCH) == 0
    state = profiles.apply(flib, "fixed", t)
    assert state[SWITCH] == 0 and flib.defects()[SWITCH]["on"] == 0
    assert profiles.apply(flib, "original") == {} and flib.defects()[SWITCH]["on"] == 1


def pathlib_read(p) -> str:
    import pathlib
    return pathlib.Path(p).read_text(encoding="utf-8")


def _stock_probe():
    TF._need("cov_b_s34_hawaii", "stock")
    run = json.loads((TF.CORPUS / "cov_b_s34_hawaii" / "stock" / "run.json").read_text())
    f = run["failed_opens"][0]
    assert _tree(f["path"]) and f["open"]["ret"] == 0
    return f


def test_fault_probe_on_and_off(flib):
    """The stock run's failing load (the install without the exe's names): ON a Fault, OFF cells equal to the same load of
    the underscore files."""
    f = _stock_probe()
    a = f["args"]
    port = TF.Port(flib)            # the stock install: no alias copies
    try:
        with pytest.raises(L.SgError) as e:
            port.load(f["path"], a[2], a[3], a[4])
        assert e.value.code == L.SG_E_FAULT
        flib.defect(SWITCH, False)
        off = port.load(f["path"], a[2], a[3], a[4])
        ref = port.load(f["path"].replace("Tree Cerc", "Tree_Cerc").replace("Tree Drac", "Tree_Drac"), a[2], a[3], a[4])
    finally:
        port.close()
    assert off["status"] == 0 and off["cells"]
    assert off["cells"] == ref["cells"] and off["pal"] == ref["pal"] and off["cols"] == ref["cols"]


def test_tropical_world_loads_equal_the_alias_recording_off(flib):
    """OFF, on the stock install (no alias copies in the overlay), every distinct tropical world load of the alias recording
    decodes equal to the original's cells; the loads naming a tree are the ones the switch changes."""
    TF._need("cov_b_s34_hawaii", "flic")
    flib.defect(SWITCH, False)
    port = TF.Port(flib, False)
    try:
        loads = TF.unique_loads(TF.read_recording("cov_b_s34_hawaii", "flic"), True)
        trees = 0
        bad = []
        for ld in loads:
            a = ld["args"]
            trees += _tree(ld["path"])
            diff = TF.compare(ld, port.load(ld["path"], a[2], a[3], a[4]))
            if diff:
                bad.append((ld["path"], diff[:2]))
    finally:
        port.close()
    assert trees >= 2, "the recording holds no Tree Cerc / Tree Drac load"
    assert not bad, bad[:5]


def test_loads_naming_no_tree_are_identical_on_and_off(flib):
    """Completing control: sandbox_basic's (Parkland) loads, with the switch ON and OFF."""
    TF._need("sandbox_basic", "flic")
    loads = TF.unique_loads(TF.read_recording("sandbox_basic", "flic"), False)[:120]
    assert loads and not any(_tree(ld["path"]) for ld in loads)
    out = {}
    for on in (True, False):
        flib.defect(SWITCH, on)
        port = TF.Port(flib, False)
        try:
            out[on] = [port.load(ld["path"], ld["args"][2], ld["args"][3], ld["args"][4]) for ld in loads]
        finally:
            port.close()
    assert out[True] == out[False]
    assert all(r["status"] == 0 for r in out[True])


def test_wrong_body_reads_red(flib):
    """Negctl 5 applies the alias at 1 as well: the fault probe no longer raises."""
    f = _stock_probe()
    a = f["args"]
    flib.negctl("flic.load", 5)
    port = TF.Port(flib)
    try:
        port.load(f["path"], a[2], a[3], a[4])        # would raise at 1 in the original profile
    except L.SgError:
        pytest.fail("negctl 5 still faults: an equivalent mutant")
    finally:
        flib.negctl("flic.load", 0)
        port.close()


# ------------------------------------------------------------------------------ no gfx backend: menu.cpp's column path
def _load_world_assets(lib, tropical_alias: bool):
    """loadWorldAssets on a tropical course (type 2) with NO gfx backend, over the stock install (the underscore files only) or
    over it plus copies under the exe's spaced names (what the original needs to complete). Returns (code | 'ok', image bytes,
    tropical sheet columns, count of g_sheetCols entries)."""
    from port.test.test_golfers import S
    lib.exe_load()
    with lib.state() as st:
        st.fill_from_exe()
        st.files_root(str(TF.ROOT / "original"))
        if tropical_alias:
            for src, dst in TF.ALIASES:
                for f in sorted((TF.ROOT / "original" / src).glob("*.flc")):
                    st.files_put(dst + "\\" + f.name, f.read_bytes())
        s = S(lib, st)
        s.w("g_courseType", 0, "B", 2)
        try:
            st.call("loadWorldAssets", [])
            res = "ok"
        except L.SgError as e:
            res = e.code
        image = b"".join(st.read(o, n) for _, o, n in lib.objects())
        cols = s.r("g_sheetCols", 0, "512i") if res == "ok" else None
        return res, image, cols


def test_load_world_assets_without_backend(flib):
    """The 114 tropical loads (S3.4's alias recording) without a gfx backend: at 1 the first Tree Cerc open is a Fault (the stock
    game's access violation); at 0 the whole GameState equals the original-with-aliases run (the sheets' columns are no longer 0,
    so the later cell allocations are the original's)."""
    if not flib.has("loadWorldAssets"):
        pytest.skip("no menu module")
    TF._need("cov_b_s34_hawaii", "flic")
    on = _load_world_assets(flib, False)
    assert on[0] == L.SG_E_FAULT
    flib.defect(SWITCH, False)
    off = _load_world_assets(flib, False)
    flib.defect_reset()
    ref = _load_world_assets(flib, True)      # switch 1, spaced copies present: the original's completing run
    assert off[0] == ref[0] == "ok"
    assert off[1] == ref[1], "the stock install with the switch OFF differs from the original-with-aliases GameState"
    assert off[2] == ref[2] and any(c > 0 for c in off[2])


def test_load_world_assets_wrong_body_reads_red(flib):
    """Negctl 5 (alias applied at 1) turns the stock run's Fault into a completing run."""
    if not flib.has("loadWorldAssets"):
        pytest.skip("no menu module")
    flib.negctl("flic.load", 5)
    try:
        assert _load_world_assets(flib, False)[0] == "ok"
    finally:
        flib.negctl("flic.load", 0)
