"""Roadmap S6.3: the defect profile mechanism (port/test/profiles.py, re/port/defect_profiles.tsv).

Static tests: table parsing and refusals. Port tests: `original` equals no profile (switch state, segment results, the
existing switch tests' vectors), `fixed` applies a fixture list, unknown names are refused, and a profile that turns a known
switch off makes that switch's existing test read RED (negative control)."""
from __future__ import annotations

import pathlib
import sys

import pytest

from port.test import profiles as P

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import vector_lib as VL  # noqa: E402

need_corpus = pytest.mark.skipif(not VL.VEC_DIR.exists(), reason="vector corpus not present (port/corpus/vectors, local)")
HDR = "switch\tvalue\tdecision_id\treason\n"


def _table(tmp_path, *rows, header=HDR):
    p = tmp_path / "profiles.tsv"
    p.write_text(header + "".join(r + "\n" for r in rows), encoding="utf-8")
    return p


# ------------------------------------------------------------------------------------------------ static
@pytest.mark.static
def test_shipped_table_is_valid_and_documented():
    t = P.DEFAULT_TABLE.read_text(encoding="utf-8")
    assert t.splitlines()[0].split("\t") == P.COLUMNS
    assert any(ln.startswith("#") for ln in t.splitlines()), "the table documents its columns"
    P.load_table()   # parses (an empty body is allowed until the lead fills it)


@pytest.mark.static
def test_table_parsing(tmp_path):
    p = _table(tmp_path, "a.b\t0\tD1\tone", "# comment", "", "c.d\t1\tD2\ttwo")
    assert P.members("fixed", p) == {"a.b": 0, "c.d": 1}
    assert P.members("fixed", _table(tmp_path)) == {}


@pytest.mark.static
@pytest.mark.parametrize("rows,header,why", [
    (["a.b\t2\tD1\tx"], HDR, "not 0 or 1"),
    (["a.b\t0\tD1\tx", "a.b\t0\tD1\ty"], HDR, "duplicate"),
    (["a.b\t0\t\tx"], HDR, "required"),
    (["a.b\t0\tD1"], HDR, "columns"),
    (["a.b\t0\tD1\tx"], "switch\tvalue\treason\n", "header"),
])
def test_bad_tables_refused(tmp_path, rows, header, why):
    with pytest.raises(P.ProfileError, match=why):
        P.load_table(_table(tmp_path, *rows, header=header))


@pytest.mark.static
def test_profile_names_and_original_not_editable(tmp_path):
    p = _table(tmp_path, "a.b\t0\tD1\tx")
    assert P.members("original") == {}
    with pytest.raises(P.ProfileError, match="not editable"):
        P.members("original", p)
    with pytest.raises(P.ProfileError, match="unknown profile"):
        P.members("fast")
    assert P.EDITABLE == ("fixed",) and "original" not in P.EDITABLE
    with pytest.raises(P.ProfileError, match="not found"):
        P.load_table(tmp_path / "missing.tsv")


@pytest.mark.static
def test_original_is_never_a_table_row():
    # `original` has no member list: nothing in the shipped table may name a switch with value 1 only as a way to edit it
    assert all(r["value"] in (0, 1) for r in P.load_table())


# ------------------------------------------------------------------------------------------------ port
@pytest.mark.port
def test_original_is_every_switch_one(portlib):
    portlib.defect("shotPower.cacheIgnoresPutt", 0)
    assert P.apply(portlib, "original") == {}
    assert set(P.state(portlib).values()) == {1}
    assert P.current(portlib) == "original"


@pytest.mark.port
def test_fixed_applies_fixture_list_and_reset(portlib, tmp_path):
    names = sorted(portlib.defects())
    assert len(names) >= 2
    p = _table(tmp_path, f"{names[0]}\t0\tD0\tfixture", f"{names[1]}\t1\tD0\tfixture")
    assert P.apply(portlib, "fixed", p) == {names[0]: 0, names[1]: 1}
    st = P.state(portlib)
    assert st[names[0]] == 0 and sum(1 for v in st.values() if v == 0) == 1
    assert P.current(portlib, p) == "fixed"
    P.apply(portlib, "original")
    assert set(P.state(portlib).values()) == {1}, "applying original undoes fixed"


@pytest.mark.port
def test_empty_fixed_equals_original(portlib, tmp_path):
    P.apply(portlib, "fixed", _table(tmp_path))
    assert P.current(portlib) == "original"


@pytest.mark.port
def test_unknown_switch_is_an_error_and_changes_nothing(portlib, tmp_path):
    name = sorted(portlib.defects())[0]
    p = _table(tmp_path, f"{name}\t0\tD0\tfixture", "no.suchSwitch\t0\tD0\tfixture")
    with pytest.raises(P.ProfileError, match="no.suchSwitch"):
        P.apply(portlib, "fixed", p)
    assert set(P.state(portlib).values()) == {1}, "a refused profile leaves the switches alone"
    with pytest.raises(P.ProfileError):
        P.validate(portlib, p)


@pytest.mark.port
def test_shipped_fixed_list_names_exist_in_the_library(portlib):
    P.validate(portlib)


SEGMENTS = ["sandbox_basic/00", "course_season/00", "cov_a_flags/05"]


@pytest.mark.port
@pytest.mark.parametrize("name", SEGMENTS)
def test_original_profile_segments_equal_no_profile(portlib, srunner, name):
    from port.test import segments as S
    sc, k = name.split("/")
    d = S.SEG_DIR / sc / k
    if not d.exists():
        pytest.skip(f"segment {name} not in the local corpus")
    seg = S.load_segment(d)
    base = srunner.run(seg)
    P.apply(portlib, "original")
    got = srunner.run(seg)
    key = lambda r: (r.status, r.frames_run, r.frames_expected, r.first_divergence, r.noncausal, r.run_detail)  # noqa: E731
    assert key(got) == key(base)


@need_corpus
@pytest.mark.port
def test_original_profile_keeps_the_existing_switch_tests_green(portlib, vrunner):
    """The ON arm of test_shots.py::test_defect_shot_power_putt under profile `original`."""
    P.apply(portlib, "original")
    assert vrunner.run_function("shotPower").verdict == "GREEN"


@need_corpus
@pytest.mark.port
def test_profile_turning_a_switch_off_reads_red(portlib, vrunner, tmp_path):
    """Negative control: `fixed` listing shotPower.cacheIgnoresPutt = 0 changes exactly the putt vectors that
    test_defect_shot_power_putt sees; original is GREEN again afterwards."""
    from port.test import vectors as V
    p = _table(tmp_path, "shotPower.cacheIgnoresPutt\t0\tD0\tfixture")
    P.apply(portlib, "fixed", p)
    r = vrunner.run_function("shotPower")
    assert r.failed > 0
    vecs = {v["i"]: v for k in VL.read_vec(V.vec_path("shotPower"))["keys"] for v in k["vectors"]}
    assert all(vecs[f.i]["args"][2]["int"] != 0 for f in r.failures)
    P.apply(portlib, "original")
    assert vrunner.run_function("shotPower").verdict == "GREEN"
