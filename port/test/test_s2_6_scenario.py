"""Roadmap S2.6 (log/roadmap/S2/6_notes.md): the pinned scenario cov_r_s2_screens (re/tools/scenario_s2_6.py), one checkpoint
inside each 2D screen no older pinned checkpoint shows, run from process start by the port.

The boot run equals the original's capture in records 0..770. Record 771 differs in g_cursorTileX / g_cursorTileY only
(U-1253, the symptom of U-1062): its cause is not measured, so no KNOWN entry is taken (orchestrator, 2026-10-24) and the tests
past it are xfail(strict) on U-1253 until the measurement exists. showBuyLandScreen is not ported (re/port/commands_split.tsv: gap, U-1251), so the
scenario visits buy land last and the run stops there with that name missing, after record 1156 of 1268.

Control: showEndOfYearScreen mode 1 (the membership history slot of this month written as members + 1) reads RED at a record
after the scenario's injected year end."""
from __future__ import annotations

import pytest

from port.test import boot as B
from port.test import lib as L

pytestmark = pytest.mark.port

SCEN = "cov_r_s2_screens"
FIRST_DIFF = 771                   # g_cursorTileX / g_cursorTileY only (U-1253, not measured)
STOP_AT_BUY_LAND = 1156            # records the port reaches before it needs showBuyLandScreen
U1253 = pytest.mark.xfail(strict=True, reason="U-1253: record 771 differs in the cursor tile; cause not measured, no KNOWN entry")


@pytest.fixture(scope="module")
def portlib():
    return L.load()


def _need():
    if not (B.TRACES / f"{SCEN}.trace").exists() or B.expected_records(SCEN)[1] is None:
        pytest.skip(f"no trace or capture for {SCEN} (port/corpus/traces, log/p2/p24; re/tools/s2_6_runs.py seg capture + trace)")


def test_boot_equals_the_original_before_record_771(portlib):
    _need()
    r = B.run(SCEN, portlib, max_records=FIRST_DIFF)
    assert r.status == "PASS" and r.first_divergence is None, f"{r.status} {r.detail} | {r.run_detail[:300]}"


@U1253
def test_boot_equals_the_original_up_to_buy_land(portlib):
    _need()
    r = B.run(SCEN, portlib, max_records=STOP_AT_BUY_LAND)
    assert r.status == "PASS" and r.records == STOP_AT_BUY_LAND, f"{r.status} {r.detail} | {r.run_detail[:300]}"


@U1253
def test_boot_stops_only_at_buy_land(portlib):
    _need()
    r = B.run(SCEN, portlib)
    assert r.first_divergence is None, f"{r.status} {r.detail}"
    if r.status not in ("PASS", "KNOWN"):
        assert "showBuyLandScreen" in r.run_detail, f"{r.status} {r.detail} | {r.run_detail[:300]}"


def test_record_771_differs_in_the_cursor_tile_only(portlib):
    """U-1253's symptom, pinned so a change in it is seen: the first divergence is record 771 and names the cursor tile only."""
    _need()
    r = B.run(SCEN, portlib, max_records=FIRST_DIFF + 1)
    d = r.first_divergence or {}
    assert d.get("record") == FIRST_DIFF and sorted(d.get("objects", [])) == ["g_cursorTileX", "g_cursorTileY"], r.detail


def test_end_of_year_control_reads_red(portlib):
    """showEndOfYearScreen mode 1 adds g_memberTypeHistory to record 771's divergent objects (the unmutated run has only the
    cursor tile there, test above)."""
    _need()
    portlib.negctl("showEndOfYearScreen", 1)
    try:
        r = B.run(SCEN, portlib, max_records=FIRST_DIFF + 1)
    finally:
        portlib.negctl_reset()
    d = r.first_divergence or {}
    assert d.get("record") == FIRST_DIFF and "g_memberTypeHistory" in d.get("objects", []), r.detail
