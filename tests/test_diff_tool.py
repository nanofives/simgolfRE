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
