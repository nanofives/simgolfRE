"""Phase 4: the .sve round trip through the port (re/PORT_PLAN.md "Next steps").

The ORIGINAL played re/tools/scenario.py's cov_r_reload: it loaded mood5.sve, then stories5.sve through the in-game
browser, played, saved with the save key (save_1, "10 September"), loaded save_1 back (the record run checks the date went
back), played and saved again (save_2, "21 September"); on the way the browser wrote While_Browsing.sve and the month
changes rewrote the two autosaves. cov_r_sve is the same without the Esc before each key: there a SimFoto wait took the
second 'L', so no reload happened, and it stays as the regression for that wait (U-1061). Every saved game the original
held at the end checkpoint was collected (port/test/sve_refs.py: the pinned references, the segment corpus captures, the
trace run and both restarts, 9 runs per scenario, agree byte for byte).

The port runs the scenario's segments from their start images with the recorded input (SegmentRunner, as the phase 3
gate does). A segment must PASS (every frame record and the end checkpoint), and then every saved game the original
wrote must be in the port's files with the same BODY BYTES (everything after the 0x64-byte header line) and the same
HEADER TEXT up to and including its NUL. A saved game the original did not change must be unchanged in the port. Files
the port reads are the user's install plus its in-memory overlay (original/ is never written).

HEADER RESIDUE. saveGame writes all 0x64 bytes of g_textBuffer, so the bytes after the header's NUL are whatever earlier
text the buffer held: in the original that is text composed for drawing (the news ticker's wrapped message lines,
wrapMessageBuffer's newlines), which the port does not draw. Measured (2026-10-21): the text up to the NUL equal in all
ten written files of the two scenarios, the residue different in nine (2 to 57 bytes). Whether a reader of a loaded header uses
the bytes past the NUL is not checked (U-1060); until it is, the residue is reported, not failed.

Negative controls (all RED): saveGame mode 4 flips one body byte of the file and leaves the state alone: on cov_r_sve the
segment still PASSES, so only the byte comparison sees it; on cov_r_reload the reload reads the flipped byte back and the
segment FAILS (g_holes, record 579), which shows the reload reads the port's own file; mode 1 drops the date from the header text, mode 3 writes a 0x63-byte
header (every body byte shifts); loadGame mode 9 skips the third body field when save_1 is read back; pumpAndRedraw mode 3 hands the SimFoto wait's key to its first pass instead of pacing
it by the clock reads (the segment stops in narrateShot and the saves are never made).
"""
from __future__ import annotations

import csv
import pathlib

import pytest

from port.test import lib as L
from port.test import segments as SG
from port.test import sve_refs

pytestmark = pytest.mark.port

ROOT = pathlib.Path(__file__).resolve().parents[2]
SCENARIOS = ("cov_r_sve", "cov_r_reload", "cov_r_course")
# files the original writes that the port does not: the course screenshots saveThumbAndFull takes (rendering, phase 5)
NOT_PORTED = ("Thumb.jpg", "Full.jpg")
FIELDS = ROOT / "re" / "model" / "sve_fields.tsv"


def _rows() -> list[dict]:
    if not SG.MANIFEST_TSV.exists():
        return []
    return [r for r in SG.manifest_rows() if r["scenario"] in SCENARIOS and r["status"].lower() == "accepted"]


def _field_at(off: int) -> str:
    """The sve_fields.tsv row holding file offset `off` (non-championship file layout)."""
    with FIELDS.open(newline="") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            try:
                lo, n = int(r["file_offset"], 16), int(r["size"], 16)
            except (ValueError, KeyError):
                continue
            if r["part"] != "header2" and lo <= off < lo + n:
                return f"{r['index']} {r['model_symbols']} +0x{off - lo:x}"
    return "?"


def _first_diff(a: bytes, b: bytes) -> str:
    if a == b:
        return ""
    k = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
    nd = sum(1 for i in range(min(len(a), len(b))) if a[i] != b[i])
    return (f"len {len(a)} vs {len(b)}, {nd} bytes differ, first at 0x{k:x} ({_field_at(k)}): "
            f"port {a[k:k + 1].hex() or '-'} original {b[k:k + 1].hex() or '-'}")


HEADER = 0x64


def _headers(name: str) -> int:
    """0x64-byte g_textBuffer header lines at the start of the file: one in a saved game (saveGame), two in a course file
    (saveCourse: the title line and the course-record line); the info file .srf has none (compared whole)."""
    low = name.lower()
    return 1 if low.endswith(".sve") else 2 if low.endswith(".cse") else 0


def _compare(got: bytes | None, want: bytes, name: str = ".sve") -> tuple[str, str]:
    """(failure, residue note) for one written file; '' when that part is equal. Each header line is compared up to and
    including its NUL; the bytes after it are residue (U-1060)."""
    if got is None:
        return "the port has no such file (the original wrote it)", ""
    if len(got) != len(want):
        return _first_diff(got, want), ""
    body = HEADER * _headers(name)
    notes = []
    for h in range(0, body, HEADER):
        nul = want.find(0, h, h + HEADER)
        text = h + HEADER if nul < 0 else nul + 1
        if got[h:text] != want[h:text]:
            return f"header line {h // HEADER} text: " + _first_diff(got[h:text], want[h:text]), ""
        nres = sum(1 for i in range(text, h + HEADER) if got[i] != want[i])
        if nres:
            notes.append(f"{nres} residue bytes after line {h // HEADER}'s NUL at 0x{text - 1:x}")
    if got[body:] != want[body:]:
        k = next(i for i in range(body, len(want)) if got[i] != want[i])
        nd = sum(1 for i in range(body, len(want)) if got[i] != want[i])
        where = f" ({_field_at(k)})" if body == HEADER else ""
        return (f"body: {nd} bytes differ, first at 0x{k:x}{where}: port {got[k]:02x} original {want[k]:02x}"), ""
    return "", "; ".join(notes)


def roundtrip(lib: L.Lib, row: dict) -> tuple[SG.SegmentResult, dict[str, str], int, dict[str, str]]:
    """Runs the segment; returns its result, {file: mismatch}, the number of original-written files compared and
    {file: header residue note}."""
    scen, end = row["scenario"], row["end_checkpoint"]
    orig = sve_refs.load(scen, end)
    if orig is None:
        pytest.skip(f"no collected saves for {scen}@{end} (port/test/sve_refs.py collect {scen})")
    seg = SG.load_segment(SG.segment_dir(row))
    disk = {n: (SG.INSTALL / sve_refs.logical(n)).read_bytes() for n in orig
            if (SG.INSTALL / sve_refs.logical(n)).exists()}
    # the files the original held at the segment's start (a load before it can autosave): given to the port in its
    # overlay where they differ from the install, and the baseline "written in the segment" is judged against
    start = sve_refs.load(scen, row["start_checkpoint"])
    install = dict(disk) if start is None else start
    bad, residue, written = {}, {}, 0

    def before(st):
        for name, data in (start or {}).items():
            if disk.get(name) != data:
                st.files_put(sve_refs.logical(name), data)

    def after(st, res):
        nonlocal written
        for name, want in orig.items():
            got = st.files_read(sve_refs.logical(name))
            if install.get(name) != want and name.endswith(NOT_PORTED):
                residue[name] = "written by the original, not by the port (rendering, phase 5)"
            elif install.get(name) != want:
                written += 1
                why, note = _compare(got, want, name)
                if why:
                    bad[name] = why
                if note:
                    residue[name] = note
            elif got != want:
                bad[name] = "unchanged by the original, changed by the port: " + _first_diff(got or b"", want)

    res = SG.SegmentRunner(lib).run(seg, after=after, before=before)
    return res, bad, written, residue


def _save_rows() -> list[dict]:
    out = []
    for r in _rows():
        if sve_refs.load(r["scenario"], r["end_checkpoint"]) is not None:
            out.append(r)
    return out


@pytest.fixture(scope="module")
def portlib():
    return L.load()


@pytest.mark.parametrize("scen", SCENARIOS)
def test_scenario_has_a_save_segment(scen):
    rows = [r for r in _rows() if r["scenario"] == scen]
    if not rows:
        pytest.skip(f"no {scen} segments in the corpus (re/tools/segment_corpus.py pipeline --scenario {scen})")
    assert [r for r in _save_rows() if r["scenario"] == scen], f"no {scen} segment ends at a checkpoint with collected saves"


@pytest.mark.parametrize("row", _save_rows(), ids=lambda r: f"{r['scenario']}/{int(r['segment']):02d}")
def test_port_writes_the_originals_saves(portlib, row):
    res, bad, written, residue = roundtrip(portlib, row)
    assert res.status == "PASS", f"{res.name}: {res.status} {res.detail} {res.run_detail[:300]}"
    assert written >= 3, f"only {written} files written by the original in this segment"
    assert not bad, "\n".join(f"{k}: {v}" for k, v in bad.items())
    for k, v in residue.items():
        print(f"  residue (not compared): {k}: {v}")


# (scenario, function, mode, what it does wrong, what the segment itself must do: "PASS", "FAIL" or None = either)
NEGCTL = [("cov_r_sve", "saveGame", 4, "one body byte of the file flipped (no reload: only the bytes show it)", "PASS"),
          ("cov_r_reload", "saveGame", 4, "one body byte of save_1 flipped (read back by the reload)", "FAIL"),
          ("cov_r_reload", "saveGame", 1, "no date in the header", None),
          ("cov_r_reload", "saveGame", 3, "a 0x63-byte header", None),
          ("cov_r_reload", "loadGame", 9, "the third body field is skipped on load", None),
          ("cov_r_sve", "pumpAndRedraw", 3, "the SimFoto wait not paced by its clock reads", None),
          ("cov_r_course", "saveCourse", 2, "a 4-byte end marker in the course file", None),
          ("cov_r_course", "saveCourse", 1, "no course body (the tournament then loads the port's broken file)", "FAIL"),
          ("cov_r_course", "flushCourseInfo", 2, "the info file's Holes line without the - 1", "PASS")]


@pytest.mark.parametrize("scen,fname,mode,what,seg", NEGCTL)
def test_negctl_reads_red(portlib, scen, fname, mode, what, seg):
    rows = [r for r in _save_rows() if r["scenario"] == scen]
    if not rows:
        pytest.skip(f"no collected {scen} segment")
    portlib.negctl(fname, mode)
    try:
        res, bad, _, _ = roundtrip(portlib, rows[-1])
    finally:
        portlib.negctl_reset()
    if seg is not None:
        assert res.status == seg, f"{fname} mode {mode}: the segment should {seg} ({res.status} {res.detail})"
    assert bad, f"{fname} mode {mode} ({what}) read GREEN: segment {res.status}, every file equal"
