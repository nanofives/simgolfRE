"""The confidence gate (scripts/re_classify.py) on throwaway project trees: it must refuse every
anti-pattern in re/CONFIDENCE.md and keep the trackers append-only."""
import os
import pathlib
import sys
import time

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "scripts"))
import re_classify as rc  # noqa: E402

pytestmark = pytest.mark.static

ADDR = "0x00401000"
NOTE = """# 0x00401000 Foo
## Purpose
Adds the two args (`0x00401000`).
## Signature
int __cdecl Foo(int, int)
## Reads
none
## Writes
none
## Callees
none
## Constants
none
"""


@pytest.fixture
def proj(tmp_path):
    (tmp_path / "re" / "analysis" / "util").mkdir(parents=True)
    (tmp_path / "re" / "analysis" / "CHANGELOG.md").write_text("# CHANGELOG\n\n<!-- ENTRIES -->\nOLD ENTRY\n")
    (tmp_path / "re" / "functions_ghidra.tsv").write_text("entry\tname\tsize\tcallers\tcallees\n00401000\tFUN_00401000\t10\t1\t0\n")
    (tmp_path / "UNCERTAINTIES.md").write_text("| ID | addr | type | d | p | status |\n|---|---|---|---|---|---|\n")
    (tmp_path / "hooks.csv").write_text(",".join(rc.COLUMNS) + "\n")
    return tmp_path


def run(root, *args):
    return rc.main(["--root", str(root), *args])


def to_c1(root):
    return run(root, "promote", ADDR, "--to", "C1", "--name", "Foo", "--subsystem", "util", "--evidence", "string xref")


def to_c2(root, note=NOTE, **kw):
    (root / "re" / "analysis" / "util" / "00401000_Foo.md").write_text(note)
    return run(root, "promote", ADDR, "--to", "C2", "--note", "re/analysis/util/00401000_Foo.md",
               "--callers", kw.get("callers", "0x00402000"), "--callees", kw.get("callees", "none"))


def level(root):
    return rc.Project(root).rows()[("golf_clean.exe", "00401000")]["confidence"]


def test_c1_requires_real_name(proj):
    assert run(proj, "promote", ADDR, "--to", "C1", "--name", "FUN_00401000", "--subsystem", "util", "--evidence", "x") == 2
    assert to_c1(proj) == 0 and level(proj) == "C1"


def test_no_level_skipping(proj):
    assert run(proj, "promote", ADDR, "--to", "C2", "--name", "Foo") == 2


def test_c2_rejects_hedge_words(proj):
    to_c1(proj)
    assert to_c2(proj, note=NOTE.replace("Adds the two args", "Probably adds the two args")) == 2
    assert level(proj) == "C1"


def test_c2_rejects_unfiled_uncertainty(proj):
    to_c1(proj)
    assert to_c2(proj, note=NOTE + "\n[UNCERTAIN] what is arg2\n") == 2
    assert to_c2(proj, note=NOTE + "\n[UNCERTAIN U-0009] what is arg2\n") == 2  # not filed
    (proj / "UNCERTAINTIES.md").write_text((proj / "UNCERTAINTIES.md").read_text()
                                           + "| U-0009 | 0x00401000 | behavioural | arg2 | trace it | open |\n")
    assert to_c2(proj, note=NOTE + "\n[UNCERTAIN U-0009] what is arg2\n") == 0


def _c3_ready(proj, green=True, vectors=12):
    src = proj / "shim" / "src" / "re" / "Foo.cpp"
    src.parent.mkdir(parents=True)
    src.write_text("// 0x00401000\nint Foo_re(int a,int b){return a+b;}\nSG_HOOK(\"golf_clean.exe\", 0x00401000, Foo, Foo_re, Foo_orig);\n")
    dll = proj / "shim" / "build" / "winmm.dll"
    dll.parent.mkdir(parents=True)
    time.sleep(0.01)
    dll.write_bytes(b"MZ")
    d = proj / "log" / "diff"
    d.mkdir(parents=True)
    rows = "\n".join(f"{i} {i},{i},{i},True" for i in range(vectors))
    (d / "00401000_Foo.path1.csv").write_text(
        f"vector,original,reimpl,match\n{rows}\ninstall_witness,0xe9,0xe9,True\nVERDICT,{'GREEN' if green else 'RED'},{vectors},\n")
    return "shim/src/re/Foo.cpp"


def test_c3_requires_green_ab(proj):
    to_c1(proj); to_c2(proj)
    f = _c3_ready(proj, green=False)
    assert run(proj, "promote", ADDR, "--to", "C3", "--file", f) == 2


def test_c3_dead_code_is_refused(proj):
    to_c1(proj); to_c2(proj, callers="none")
    f = _c3_ready(proj)
    assert run(proj, "promote", ADDR, "--to", "C3", "--file", f) == 2


def test_c3_leaf_needs_enough_vectors(proj):
    to_c1(proj); to_c2(proj)
    f = _c3_ready(proj, vectors=3)
    assert run(proj, "promote", ADDR, "--to", "C3", "--file", f) == 2


def test_c3_leaf_summary_rows_are_not_vectors(proj):
    # diff_hook appends state_regions and distinct_results rows: 8 vectors plus those two must not reach 10
    to_c1(proj); to_c2(proj)
    f = _c3_ready(proj, vectors=8)
    p = proj / "log" / "diff" / "00401000_Foo.path1.csv"
    p.write_text(p.read_text().replace("install_witness", "state_regions,0x1+0x0:4,,\ndistinct_results,8,,\ninstall_witness"))
    assert run(proj, "promote", ADDR, "--to", "C3", "--file", f) == 2


def test_c3_leaf_vectors_sum_over_keys(proj):
    # one registry entry per fixture variant: 6 + 6 vectors in two CSVs of one address pass; a RED key fails all
    to_c1(proj); to_c2(proj, callers="GameTick")
    f = _c3_ready(proj, vectors=6)
    d = proj / "log" / "diff"
    (d / "00401000_Foo_b.path1.csv").write_text((d / "00401000_Foo.path1.csv").read_text())
    assert run(proj, "check", ADDR, "--to", "C3", "--file", f) == 0
    (d / "00401000_Foo_c.path1.csv").write_text((d / "00401000_Foo.path1.csv").read_text().replace("GREEN", "RED"))
    assert run(proj, "check", ADDR, "--to", "C3", "--file", f) == 2


def test_c3_anonymous_caller_is_an_island(proj):
    to_c1(proj); to_c2(proj, callers="FUN_00402000")
    f = _c3_ready(proj)
    assert run(proj, "promote", ADDR, "--to", "C3", "--file", f) == 2


def test_c3_happy_path_and_changelog_is_append_only(proj):
    to_c1(proj); to_c2(proj, callers="GameTick")
    f = _c3_ready(proj)
    assert run(proj, "promote", ADDR, "--to", "C3", "--file", f) == 0
    assert level(proj) == "C3"
    log = (proj / "re" / "analysis" / "CHANGELOG.md").read_text()
    assert "OLD ENTRY" in log
    lines = log.split("<!-- ENTRIES -->", 1)[1].strip().splitlines()
    assert "C2->C3" in lines[0] and "C1->C2" in lines[1] and "C0->C1" in lines[2] and lines[-1] == "OLD ENTRY"


def test_c4_requires_scenario_not_path1(proj):
    to_c1(proj); to_c2(proj, callers="GameTick")
    f = _c3_ready(proj)
    run(proj, "promote", ADDR, "--to", "C3", "--file", f)
    assert run(proj, "promote", ADDR, "--to", "C4") == 2


def test_demote_is_logged(proj):
    to_c1(proj)
    assert run(proj, "demote", ADDR, "--to", "C0", "--reason", "anchor changed") == 0
    assert level(proj) == "C0"
    assert "DEMOTED: anchor changed" in (proj / "re" / "analysis" / "CHANGELOG.md").read_text()


def test_real_tracker_state():
    """The live tracker: Terrain::tileAt is C2 and held there (dead code)."""
    rows = rc.Project(ROOT).rows()
    assert rows[("golf_clean.exe", "004490d0")]["confidence"] == "C2"
    assert run(ROOT, "check", "0x004490d0", "--to", "C3", "--file", "shim/src/re/Terrain.cpp") == 2


def test_batch_promotes_through_the_gates_and_keys_dlls_by_module(proj):
    # jgld.dll and sound.dll share the image base: the same RVA in both must stay two rows
    for m in ("jgld.dll", "sound.dll"):
        (proj / "re" / f"functions_ghidra_{m}.tsv").write_text("entry\tname\tsize\tcallers\tcallees\n00002650\tFUN_10002650\t78\t1\t0\n")
    tsv = proj / "names.tsv"
    tsv.write_text("module\taddr\tname\tsubsystem\tevidence\tpurpose\n"
                   "golf_clean.exe\t0x00401000\tFoo\tutil\tstring 0x4d0000\tadds\n"
                   "jgld.dll\t0x10002650\tSurface::lock\trender\tmatch re/match/x.cpp\tlocks\n"
                   "sound.dll\t0x10002650\tSnd::open\taudio\timport waveOutOpen\topens\n"
                   "sound.dll\t0x10009999\tNotAFunction\taudio\timport x\tnot in the function list\n"
                   "golf_clean.exe\t0x00401000\tFoo2\tbogus\tx\tbad subsystem and already C1\n")
    assert run(proj, "batch", str(tsv), "--to", "C1") == 2          # two rows refused
    rows = rc.Project(proj).rows()
    assert rows[("jgld.dll", "00002650")]["name"] == "Surface::lock"
    assert rows[("sound.dll", "00002650")]["name"] == "Snd::open"
    assert ("sound.dll", "00009999") not in rows
    assert rows[("golf_clean.exe", "00401000")]["name"] == "Foo"
    log = (proj / "re" / "analysis" / "CHANGELOG.md").read_text()
    assert log.count("->C1") == 3 and "sound.dll:00002650" in log and "OLD ENTRY" in log


def test_retag_changes_only_the_subsystem_of_tracked_rows(proj):
    (proj / "re" / "functions_ghidra_sound.dll.tsv").write_text("entry\tname\tsize\tcallers\tcallees\n00002650\tFUN_10002650\t78\t1\t0\n")
    tsv = proj / "names.tsv"
    tsv.write_text("module\taddr\tname\tsubsystem\tevidence\tpurpose\n"
                   "sound.dll\t0x10002650\tnetPoll\tutil\timport recv\tpolls\n")
    assert run(proj, "batch", str(tsv), "--to", "C1") == 0
    tsv.write_text("module\taddr\tname\tsubsystem\tevidence\tpurpose\n"
                   "sound.dll\t0x10002650\tnetPoll\tnet\timport recv\tpolls\n"
                   "sound.dll\t0x10003000\tuntracked\tnet\tx\tnot in hooks.csv\n")
    assert run(proj, "retag", str(tsv)) == 0
    row = rc.Project(proj).rows()[("sound.dll", "00002650")]
    assert row["subsystem"] == "net" and row["confidence"] == "C1"
    assert "subsystem util->net" in (proj / "re" / "analysis" / "CHANGELOG.md").read_text()


def test_dll_path1_csvs_are_per_module(tmp_path):
    # DLL RVAs collide across jgld.dll / sound.dll / Terrain.dll: a DLL row reads only its own module's CSVs
    d = tmp_path / "log" / "diff"
    d.mkdir(parents=True)
    for n in ("000072b0_jgld_LinkedList_find", "000072b0_sound_Other", "000072b0_Terrain_Third"):
        (d / f"{n}.path1.csv").write_text("vector,original,reimpl,match\n")
    p = rc.Project(tmp_path)
    assert [x.name for x in p.diff_csvs("000072b0", "path1", "jgld.dll")] == ["000072b0_jgld_LinkedList_find.path1.csv"]
    assert [x.name for x in p.diff_csvs("000072b0", "path1", "sound.dll")] == ["000072b0_sound_Other.path1.csv"]


def test_dll_caller_va_is_matched_to_rva_row(proj):
    # callers of DLL functions are recorded as VAs (0x10006b80); the tracked row is keyed by RVA (00006b80)
    rows = {("jgld.dll", "00006b80"): {"addr": "00006b80", "module": "jgld.dll", "confidence": "C2", "name": "Tracked::dtor"}}
    assert rc._identified(rc.Project(proj), "0x10006b80", rows, "jgld.dll")


def test_cross_module_caller_is_looked_up_in_its_own_module(proj):
    # a Terrain.dll export called from the exe (re/tools/import_callers.py): `golf_clean.exe:0x...` resolves in the
    # exe's rows, not as a Terrain RVA; an unknown or default-named exe caller is still an island
    rows = {("golf_clean.exe", "00449470"): {"addr": "00449470", "module": "golf_clean.exe", "confidence": "C2",
                                              "name": "syncTiles"},
            ("Terrain.dll", "00449470"): {"addr": "00449470", "module": "Terrain.dll", "confidence": "C0", "name": ""}}
    p = rc.Project(proj)
    assert rc._identified(p, "golf_clean.exe:0x00449470", rows, "Terrain.dll")
    assert not rc._identified(p, "0x00449470", rows, "Terrain.dll")
    assert not rc._identified(p, "golf_clean.exe:0x00412345", rows, "Terrain.dll")
    assert not rc._identified(p, "nosuch.dll:0x00449470", rows, "Terrain.dll")


def test_retag_never_drops_recorded_callers(proj):
    # c2_note.py --refresh writes xref-only callers; a caller recorded at promotion (module-qualified exe caller or a
    # stored-pointer installer) must survive a refresh that reports `none` (2026-10-09: 68 rows lost theirs)
    (proj / "re" / "functions_ghidra_sound.dll.tsv").write_text("entry\tname\tsize\tcallers\tcallees\n00002650\tFUN_10002650\t78\t1\t0\n")
    tsv = proj / "names.tsv"
    tsv.write_text("module\taddr\tname\tsubsystem\tevidence\tpurpose\n"
                   "sound.dll\t0x10002650\tnetPoll\tutil\timport recv\tpolls\n")
    assert run(proj, "batch", str(tsv), "--to", "C1") == 0
    rows = rc.Project(proj).rows()
    rows[("sound.dll", "00002650")]["callers"] = "golf_clean.exe:0x00449470;0x10001000"
    rc.Project(proj).save(rows)
    ref = proj / "refresh.tsv"
    ref.write_text("module\taddr\tname\tsubsystem\tnote\tcallers\tcallees\n"
                   "sound.dll\t0x10002650\tnetPoll\tutil\tx\tnone\t\n")
    assert run(proj, "retag", str(ref)) == 0
    assert rc.Project(proj).rows()[("sound.dll", "00002650")]["callers"] == "golf_clean.exe:0x00449470;0x10001000"
    ref.write_text("module\taddr\tname\tsubsystem\tnote\tcallers\tcallees\n"
                   "sound.dll\t0x10002650\tnetPoll\tutil\tx\t0x10005000\t\n")
    assert run(proj, "retag", str(ref)) == 0
    assert rc.Project(proj).rows()[("sound.dll", "00002650")]["callers"] == \
        "golf_clean.exe:0x00449470;0x10001000;0x10005000"


def test_tracker_write_retries_a_transient_open_failure(tmp_path, monkeypatch):
    # 2026-10-11: opening CHANGELOG.md for writing failed once with OSError 22 after hooks.csv was written, leaving a C3 row
    # without its CHANGELOG line. _write_retry must survive a failure on the first open and write the full text.
    target = tmp_path / "CHANGELOG.md"
    real_open = pathlib.Path.open
    calls = {"n": 0}

    def flaky_open(self, *args, **kwargs):
        if self == target and calls["n"] == 0:
            calls["n"] += 1
            raise OSError(22, "Invalid argument")
        return real_open(self, *args, **kwargs)

    monkeypatch.setattr(pathlib.Path, "open", flaky_open)
    monkeypatch.setattr(rc.time, "sleep", lambda s: None)
    rc._write_retry(target, "line\n")
    assert calls["n"] == 1 and target.read_text() == "line\n"
