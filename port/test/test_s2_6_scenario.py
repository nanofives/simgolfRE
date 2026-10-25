"""Roadmap S2.6 (log/roadmap/S2/6_notes.md): the pinned scenario cov_r_s2_screens (re/tools/scenario_s2_6.py), one checkpoint
inside each 2D screen no older pinned checkpoint shows, run from process start by the port.

The boot run equals the original's capture in every record up to 1156 of 1268. showBuyLandScreen is not ported
(re/port/commands_split.tsv: gap, U-1251), so the scenario visits buy land last and the run stops there with that name missing.

Record 771 (the first frame after the end-of-year screen) used to differ in g_cursorTileX / g_cursorTileY (U-1253, U-1254): the
port's end-of-year loop made no cursor read per pass, so the frame's next read (P6's View_toLocal) got the cursor from before
the screen. Roadmap S2.13 gave the loop the original's View_toLocal read (reports.cpp report_loop; c3er.cpp
CareerRetirementFlow_re reads it after pumpMessages on every pass), and showEndOfYearScreen mode 6, which leaves that read out,
brings the divergence back (the control).

Control: showEndOfYearScreen mode 1 (the membership history slot of this month written as members + 1) reads RED at record 771,
the first record after the scenario's injected year end."""
from __future__ import annotations

import pytest

from port.test import boot as B
from port.test import lib as L

pytestmark = pytest.mark.port

SCEN = "cov_r_s2_screens"
AFTER_YEAR_END = 771               # the first record after the end-of-year screen
STOP_AT_BUY_LAND = 1156            # records the port reaches before it needs showBuyLandScreen


@pytest.fixture(scope="module")
def portlib():
    return L.load()


def _need():
    if not (B.TRACES / f"{SCEN}.trace").exists() or B.expected_records(SCEN)[1] is None:
        pytest.skip(f"no trace or capture for {SCEN} (port/corpus/traces, log/p2/p24; re/tools/s2_6_runs.py seg capture + trace)")


def _run(lib, mode_eoy=0, **kw):
    lib.negctl("showEndOfYearScreen", mode_eoy)
    try:
        return B.run(SCEN, lib, **kw)
    finally:
        lib.negctl("showEndOfYearScreen", 0)


def test_boot_equals_the_original_up_to_buy_land(portlib):
    _need()
    r = _run(portlib, max_records=STOP_AT_BUY_LAND)
    assert r.status == "PASS" and r.records == STOP_AT_BUY_LAND, f"{r.status} {r.detail} | {r.run_detail[:300]}"


def test_boot_stops_only_at_buy_land(portlib):
    _need()
    r = _run(portlib)
    assert r.first_divergence is None, f"{r.status} {r.detail}"
    if r.status not in ("PASS", "KNOWN"):
        if r.run_detail:
            assert "showBuyLandScreen" in r.run_detail, f"{r.status} {r.detail} | {r.run_detail[:300]}"
        else:   # the Win32 runner gives no run detail text: the stop is judged by its record
            assert r.records == STOP_AT_BUY_LAND and "-11" in r.detail, f"{r.status} {r.detail}"


def test_end_of_year_cursor_read_control_reads_red(portlib):
    """U-1254's control: without the loop's View_toLocal read record 771 differs in the cursor tile only (the old symptom)."""
    _need()
    r = _run(portlib, 6, max_records=AFTER_YEAR_END + 1)
    d = r.first_divergence or {}
    assert d.get("record") == AFTER_YEAR_END and sorted(d.get("objects", [])) == ["g_cursorTileX", "g_cursorTileY"], r.detail


def test_end_of_year_control_reads_red(portlib):
    """showEndOfYearScreen mode 1 makes record 771 differ in g_memberTypeHistory."""
    _need()
    r = _run(portlib, 1, max_records=AFTER_YEAR_END + 1)
    d = r.first_divergence or {}
    assert d.get("record") == AFTER_YEAR_END and "g_memberTypeHistory" in d.get("objects", []), r.detail
