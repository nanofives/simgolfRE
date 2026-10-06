"""The A/B diff tool itself: GREEN on a faithful reimplementation, RED on a wrong body (negative control)."""
import csv
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "frida"))
import diff_hook  # noqa: E402

pytestmark = pytest.mark.runtime

WRONG = "null"  # same signature, always returns NULL: must read RED on the in-range vectors


def test_diff_green_and_negative_control_red(tmp_path):
    assert diff_hook.run(["Terrain_tileAt"], out_dir=tmp_path) == {"Terrain_tileAt": True}
    rows = list(csv.reader((tmp_path / "004490d0_Terrain_tileAt.path1.csv").open()))
    assert rows[-1][:2] == ["VERDICT", "GREEN"]
    assert ["install_witness", "0xe9", "0xe9", "True"] in rows

    neg = tmp_path / "neg"
    assert diff_hook.run(["Terrain_tileAt"], override_re={"Terrain_tileAt": WRONG}, out_dir=neg) == {"Terrain_tileAt": False}
    rows = list(csv.reader((neg / "004490d0_Terrain_tileAt.path1.csv").open()))
    assert rows[-1][:2] == ["VERDICT", "RED"]


def test_negative_control_cannot_overwrite_evidence():
    with pytest.raises(ValueError):
        diff_hook.run(["Terrain_tileAt"], override_re={"Terrain_tileAt": WRONG})


def test_state_is_compared_and_restored(tmp_path):
    """Random::next writes its seed: GREEN when the reimplementation updates it like the original; RED when a body
    returns the original's value but leaves the seed unchanged (orig_restore_state), i.e. on state alone."""
    assert diff_hook.run(["Random_next"], out_dir=tmp_path) == {"Random_next": True}
    rows = list(csv.reader((tmp_path / "0045c1a0_Random_next.path1.csv").open()))
    assert rows[0][4:] == ["state_original", "state_reimpl", "state_changed"]
    data = [r for r in rows if r[0].startswith("$s")]
    assert data and all(r[6] == "True" for r in data)          # every call changed the seed

    neg = tmp_path / "neg"
    assert diff_hook.run(["Random_next"], override_re={"Random_next": "orig_restore_state"}, out_dir=neg) == {"Random_next": False}
    rows = [r for r in csv.reader((neg / "0045c1a0_Random_next.path1.csv").open()) if r[0].startswith("$s")]
    assert len(rows) == 32 and all(r[1] == r[2] for r in rows)   # same return values...
    assert all(r[3] == "False" and r[4] != r[5] for r in rows)  # ...but the state differs, so every vector is RED
