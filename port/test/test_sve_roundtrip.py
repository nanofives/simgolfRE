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
SCENARIOS = ("cov_r_sve", "cov_r_reload")
SAVES_DIR = "Saved Games/"
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


def _compare(got: bytes | None, want: bytes) -> tuple[str, str]:
    """(failure, residue note) for one written file; '' when that part is equal."""
    if got is None:
        return "the port has no such file (the original wrote it)", ""
    if len(got) != len(want):
        return _first_diff(got, want), ""
    nul = want.find(0, 0, HEADER)
    text = HEADER if nul < 0 else nul + 1
    if got[:text] != want[:text]:
        return "header text: " + _first_diff(got[:text], want[:text]), ""
    if got[HEADER:] != want[HEADER:]:
        k = next(i for i in range(HEADER, len(want)) if got[i] != want[i])
        nd = sum(1 for i in range(HEADER, len(want)) if got[i] != want[i])
        return (f"body: {nd} bytes differ, first at 0x{k:x} ({_field_at(k)}): port {got[k]:02x} original {want[k]:02x}"), ""
    nres = sum(1 for i in range(text, HEADER) if got[i] != want[i])
    return "", (f"{nres} header residue bytes after the NUL at 0x{text - 1:x}" if nres else "")


def roundtrip(lib: L.Lib, row: dict) -> tuple[SG.SegmentResult, dict[str, str], int, dict[str, str]]:
    """Runs the segment; returns its result, {file: mismatch}, the number of original-written files compared and
    {file: header residue note}."""
    scen, end = row["scenario"], row["end_checkpoint"]
    orig = sve_refs.load(scen, end)
    if orig is None:
        pytest.skip(f"no collected saves for {scen}@{end} (port/test/sve_refs.py collect {scen})")
    seg = SG.load_segment(SG.segment_dir(row))
    install = {n: (SG.INSTALL / "Saved Games" / n).read_bytes() for n in orig
               if (SG.INSTALL / "Saved Games" / n).exists()}
    bad, residue, written = {}, {}, 0

    def after(st, res):
        nonlocal written
        for name, want in orig.items():
            got = st.files_read(SAVES_DIR + name)
            if install.get(name) != want:
                written += 1
                why, note = _compare(got, want)
                if why:
                    bad[name] = why
                if note:
                    residue[name] = note
            elif got != want:
                bad[name] = "unchanged by the original, changed by the port: " + _first_diff(got or b"", want)

    res = SG.SegmentRunner(lib).run(seg, after=after)
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
          ("cov_r_sve", "pumpAndRedraw", 3, "the SimFoto wait not paced by its clock reads", None)]


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
