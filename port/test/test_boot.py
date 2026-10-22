"""Phase 4, the boot path (re/PHASE4_BOOT_PLAN.md, port/test/boot.py): whole scenarios from process start over the user's exe.

A boot run starts from the exe's initialised data alone (sg_exe_fill_state 1, no image of the original), attaches the install
plus the scenario's own files (segment_files.files_for: cov_b_title's Themes/Default.pro, cov_a_flags' fixture saves) and the
host text provider, and runs sg_run_boot over the scenario's whole trace. Record k must equal record k of the original's
per-frame sim hashes (log/p2/p24/<scenario>/c_a/framehash.bin, a capture of the same pinned schedule).

Measured (2026-10-22, x64 and Win32 identical): 57 of 60 scenarios PASS every record; cov_a_golfer and cov_b_holes18 are
KNOWN (U-1062: screenToTile's first distance reads two uninitialised locals when its first estimate is off the grid; under
the capture's frame-hash tracer the junk gives distance 0 and the original keeps the off-grid tile, records 4334..4400 and
727..733, cursor tile only); cov_b_location has no frame record (the capture stays on the location screen).

Negative controls (NEGCTL, all RED): the RNG seed, the top-score file read, the first interface flic, the terrain buttons'
and the tool strip's cut geometry, the tutorial camera reset, the theme-pack row press, the window procedure's mouse-move
cursor reads counted, the hire dialog's row text appended. Controls the trace corpus cannot see, so they are not listed:
showTutorialText 1..3 (every corpus call is with page 0, which returns after the camera reset), showThemePackSelect 2 (no
scenario cancels the theme screen), loadInterfaceArt 3 (no click reaches palette groups 21 / 22): that one is read by
test_palette_cuts_equal_the_original instead, which compares the cut fields with the original's segment-start images.
"""
from __future__ import annotations

import struct

import pytest

from port.test import boot as B
from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

# a fast set that covers the title menu, the theme-pack screen, the tutorial hook, the build palette and the UI tour
DEFAULT = ("cov_l_point", "sandbox_basic", "cov_a_flags", "cov_b_title", "cov_k2_build", "ui_tour")

# (scenario, function, mode, what it does wrong)
NEGCTL = [("sandbox_basic", "seedFromTime", 1, "the clock value + 1"),
          ("sandbox_basic", "loadTopScores", 2, "0x614 bytes of top10.sve read instead of 0x618"),
          ("sandbox_basic", "loadInterfaceArt", 2, "the first interface flic not loaded"),
          ("cov_k2_build", "loadWorldAssets", 4, "the terrain buttons not cut"),
          ("cov_k2_build", "loadWorldAssets", 5, "the tool strip not cut"),
          ("cov_a_flags", "showTutorialText", 4, "the camera cache reset skipped"),
          ("cov_b_title", "showThemePackSelect", 1, "a row press does not change g_theme"),
          ("cov_a_golfer", "tick.frame", 6, "the window procedure's mouse-move cursor reads counted"),
          ("ui_tour", "showHireEmployeeDialog", 2, "the row text appended instead of copied")]

# (scenario, segment) per course type: the palette cut fields at the segment's start frame
PALETTE_SEGMENTS = (("cov_b_holes18", 0), ("cov_b_career_types", 2), ("cov_b_career_types", 0))


@pytest.fixture(scope="module")
def portlib():
    return L.load()


def _need(scen):
    if not (B.TRACES / f"{scen}.trace").exists() or B.expected_records(scen)[1] is None:
        pytest.skip(f"no trace or capture for {scen} (port/corpus/traces, log/p2/p24)")


@pytest.mark.parametrize("scen", DEFAULT)
def test_boot_passes(portlib, scen):
    _need(scen)
    r = B.run(scen, portlib)
    assert r.status == "PASS", f"{scen}: {r.status} {r.detail} | {r.run_detail[:300]}"


@pytest.mark.slow
@pytest.mark.parametrize("scen", B.scenarios())
def test_boot_gate(portlib, scen):
    r = B.run(scen, portlib)
    if scen in B.KNOWN:
        assert r.status == "KNOWN", f"{scen}: {r.status} {r.detail}"
        return
    assert r.status in ("PASS", "NO_FRAMES"), f"{scen}: {r.status} {r.detail} | {r.run_detail[:300]}"


def test_known_ranges_are_the_cursor_only(portlib):
    """A KNOWN range hides the cursor tile only, and only where the port climbs to an on-grid neighbour of the original's
    off-grid tile (the hashes of every other object must agree there)."""
    for scen, ranges in B.KNOWN.items():
        for lo, hi, objs, row in ranges:
            assert set(objs) == {"g_cursorTileX", "g_cursorTileY"} and row == "U-1062" and lo < hi, (scen, lo, hi)
    _need("cov_b_holes18")
    r = B.run("cov_b_holes18", portlib)
    assert r.status == "KNOWN" and "727..733" in r.detail, r.detail


@pytest.mark.parametrize("scen,seg", PALETTE_SEGMENTS, ids=lambda v: str(v))
def test_palette_cuts_equal_the_original(portlib, scen, seg):
    """The cut width / height / kind (+0x18 / +0x1c / +0x28) of all 96 palette SpriteRefs from g_sprites570cb8, after the
    boot run reached the segment's start frame, equal the original's frame-head image there (the trimmed size and offsets
    at +0x10..+0x24 come from pixels: the renderer's). loadInterfaceArt mode 3 and loadWorldAssets 4 / 5 must break it."""
    _need(scen)
    rows = [r for r in SG.manifest_rows() if r["scenario"] == scen and int(r["segment"]) == seg]
    if not rows:
        pytest.skip(f"no segment {scen}/{seg}")
    start = SG.load_segment(SG.segment_dir(rows[0])).start
    k = int(rows[0]["start_frame"])
    o = [x for n, x, _ in portlib.objects("pointer") if n == "g_sprites570cb8"][0]

    def cuts(img):
        return [struct.unpack_from("<3i", img, o + 0x2c * i + 0x18) for i in range(96)]

    def boot_cuts():
        out = {}
        B.run(scen, portlib, max_records=k + 1, after=lambda st, res: out.setdefault("img", st.image()))
        return cuts(out["img"])

    want = cuts(start)
    assert boot_cuts() == want
    for fname, mode in (("loadInterfaceArt", 3), ("loadWorldAssets", 4), ("loadWorldAssets", 5)):
        portlib.negctl(fname, mode)
        try:
            got = boot_cuts()
        finally:
            portlib.negctl_reset()
        assert got != want, f"{fname} mode {mode} left the palette cuts equal"


@pytest.mark.parametrize("scen,fname,mode,what", NEGCTL)
def test_negctl_reads_red(portlib, scen, fname, mode, what):
    _need(scen)
    portlib.negctl(fname, mode)
    try:
        r = B.run(scen, portlib)
    finally:
        portlib.negctl_reset()
    assert r.status not in ("PASS", "KNOWN"), f"{fname} mode {mode} ({what}) read GREEN on {scen}"


# ------------------------------------------------------------------------------------------------ the quit (U-1050 part 2)
# cov_r_quit (re/tools/scenario.py): a Sandbox session, Exit the Game with its three answers, a second session saved before
# leaving, then the title menu's exit icon. mainLoop's QUIT block writes top10.sve (__open / __write / __close of the 0x618-byte
# g_topScores block), promptAndSaveFile(0, -2, 0) Themes/Default.pro, the closing Bink plays (dropped), the credits scroll
# until the recorded Esc, mainLoop returns. The boot run goes on past the last record (to_end) to the end of the trace, and
# then every file the original wrote (its end folder against its start folder: port/test/sve_refs.py, 11 runs agree) and
# top10.sve must be in the port's files with the same compared bytes (a character file: its records, sve_refs.compared_bytes).
from port.test import sve_refs  # noqa: E402

QUIT = "cov_r_quit"
# (function, mode, what it does wrong, the file it must break or None = the run itself fails)
QUIT_NEGCTL = [("__write", 1, "top10.sve written four bytes short", "top10.sve"),
               ("promptAndSaveFile", 1, "the skill record's first byte flipped", "Themes/Default.pro"),
               ("saveGame", 4, "one body byte of the save flipped", "&QuitSave.sve"),
               ("showCreditsScreen", 1, "a key does not end the scroll", None)]


def _quit_run(portlib):
    end, start = sve_refs.load(QUIT, "end"), sve_refs.load(QUIT, "run_a")
    if end is None or start is None:
        pytest.skip("no collected cov_r_quit files (~/bin/py -3.12 -m port.test.sve_refs collect cov_r_quit)")
    _need(QUIT)
    want = {k: v for k, v in end.items() if start.get(k) != v or k == "top10.sve"}
    got = {}

    def after(st, res):
        for k in want:
            got[k] = st.files_read(sve_refs.logical(k))

    r = B.run(QUIT, portlib, after=after, to_end=True)
    bad = {k: ("missing" if got.get(k) is None else "differs") for k, v in want.items()
           if got.get(k) is None or sve_refs.compared_bytes(k, got[k]) != sve_refs.compared_bytes(k, v)}
    return r, want, bad


def test_quit_writes_the_originals_files(portlib):
    r, want, bad = _quit_run(portlib)
    assert r.status == "PASS", f"{r.status} {r.detail} | {r.run_detail[:300]}"
    assert "mainLoop returned" in r.run_detail, r.run_detail[:300]
    assert {"top10.sve", "Themes/Default.pro", "&QuitSave.sve"} <= set(want), sorted(want)
    assert not bad, bad


@pytest.mark.parametrize("fname,mode,what,target", QUIT_NEGCTL)
def test_quit_negctl_reads_red(portlib, fname, mode, what, target):
    portlib.negctl(fname, mode)
    try:
        r, want, bad = _quit_run(portlib)
    finally:
        portlib.negctl_reset()
    if target is None:
        assert r.status != "PASS" or "mainLoop returned" not in r.run_detail, f"{fname} mode {mode} ({what}) read GREEN"
    else:
        assert target in bad, f"{fname} mode {mode} ({what}) left {target} equal ({r.status}, bad {bad})"
