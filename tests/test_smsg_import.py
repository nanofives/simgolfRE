"""Roadmap S10.1: SMSG-decomp sources -> re/match wrappers (re/tools/smsg_import.py, re/tools/smsg_pdb.py) and the
match.py directives it relies on (`// INCLUDE`, `// COMPILE`). Static layer; the compile tests need tools/vc6."""
import pathlib
import re
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402
import smsg_import  # noqa: E402
import smsg_pdb  # noqa: E402

pytestmark = [pytest.mark.static,
              pytest.mark.skipif(not (ROOT / "third_party" / "smsg" / "release" / "Terrain.pdb").exists(),
                                 reason="SMSG-decomp snapshot not present")]
needs_vc6 = pytest.mark.skipif(not (match.VC / "bin" / "cl.exe").exists(), reason="VC6 not extracted to tools/vc6")


def test_pdb_reader_gives_procedures_with_their_object():
    pdb = smsg_pdb.load("Terrain")
    assert len(pdb.procs) == 642
    course = pdb.procs_of("Course.obj")
    assert [(p["rva"], p["size"], p["name"]) for p in course] == [(0x1AF0, 91, "Terrain::loadNewCourseType")]
    assert "?loadNewCourseType@Terrain@@QAEXH@Z" in pdb.public_names()[0x1AF0]


def test_library_members_are_told_apart_by_library():
    procs = smsg_import.pdb_procs("golf/jackal.lib/jackal.dsp", "golf/jackal.lib/src/fifo.cpp")
    assert procs and all(p["lib"].lower().endswith("\\jackal.lib") for p in procs)


def test_dsp_flags_take_per_file_overrides():
    plot, inc = smsg_import.source_config("golf/golf.exe/golf.dsp", "golf/golf.exe/src/Plot.cpp")
    emp, _ = smsg_import.source_config("golf/golf.exe/golf.dsp", "golf/golf.exe/src/Employee.cpp")
    assert plot.split() == emp.split() + ["/GX"]          # golf.dsp: `# ADD CPP /GX` under Plot.cpp only
    assert "/O2" in emp.split() and "/YX" not in plot and "/nologo" not in plot
    assert inc[:2] == ["third_party/smsg/SMSG-decomp/golf/golf.exe/include",
                       "third_party/smsg/SMSG-decomp/golf/jackal.lib/include"]
    assert inc.index("tools/dxsdk8/include") < inc.index("$VC")   # build.bat :golf puts DXSDK first


def test_include_and_compile_directives():
    vc = str(match.VC / "include")
    assert match.include_path("int x;") == vc
    assert match.include_path("// INCLUDE a/b\n// INCLUDE $VC\n// INCLUDE c") == ";".join(
        [str(ROOT / "a/b"), vc, str(ROOT / "c")])
    assert match.include_path("// INCLUDE a/b") == f"{ROOT / 'a/b'};{vc}"
    src = ROOT / "re" / "match" / "x.cpp"
    assert match.compiled_file(src, "// MATCH: x") == src
    assert match.compiled_file(src, "// COMPILE third_party/y.cpp\n") == ROOT / "third_party" / "y.cpp"


def _score(tmp_path, dsp, src, flags=None):
    wip = tmp_path / "w.cpp"
    text = smsg_import.header(dsp, src, 3)
    if flags is not None:
        text = re.sub(r"(?m)^// FLAGS (\S+): .*$", lambda m: f"// FLAGS {m.group(1)}: {flags}", text)
    wip.write_text(text)
    real = smsg_import.source_config
    if flags is not None:
        smsg_import.source_config = lambda d, s: (flags, real(d, s)[1])
    try:
        return smsg_import.score_rows(src, dsp, wip)
    finally:
        smsg_import.source_config = real


@needs_vc6
def test_terrain_course_matches_and_wrong_flags_do_not(tmp_path):
    dsp, src = "terrain/Terrain.dll/Terrain.dsp", "terrain/Terrain.dll/src/Course.cpp"
    rows = _score(tmp_path, dsp, src)
    assert [(r["va"], r["status"], r["how"]) for r in rows] == [(0x10001AF0, "100", "public")]
    bad = _score(tmp_path, dsp, src, flags="/O2 /GX")       # negative control: release flags on a debug object
    assert bad[0]["status"] == "miss"


@needs_vc6
def test_overloads_without_parameter_types_pick_the_matching_body(tmp_path):
    """PDB names lack parameters: ClassList<T>::fill_entry(LinkListEntry *) and the virtual fill_entry(T *) share one;
    0x402180 is `xor eax, eax; ret 4`, the virtual's body, ICF-folded for every T."""
    rows = _score(tmp_path, "golf/golf.exe/golf.dsp", "golf/golf.exe/src/Plot.cpp")
    at = {r["pdb_name"] if "pdb_name" in r else r["name"]: r for r in rows if r["va"] == 0x402180}
    pal = at["ClassList<PalStruct>::fill_entry"]
    assert pal["status"] == "100" and "PAUPalStruct@@@Z" in pal["symbol"] and pal["how"] == "overload"


def test_canonical_smsg_files_name_their_source():
    for f in sorted((ROOT / "re" / "match").glob("smsg_*.cpp")):
        text = f.read_text()
        comp = re.search(r"(?m)^// COMPILE (\S+)$", text)
        assert comp and (ROOT / comp.group(1)).is_file(), f.name
        assert re.search(r"(?m)^// FLAGS \S+: ", text) and "#include" not in text, f.name
        assert re.findall(r"//\s*MATCH:", text), f.name
        vas = re.findall(r"//\s*MATCH:\s*\S+\s+(0x[0-9a-f]+)", text)
        assert len(vas) == len(set(vas)), f"{f.name}: one MATCH per address"


# ---- S10.6: converter fixes (ICF across smsg files, error rows as comments, no ALSO-only canonical file) ----
SND = ("sound/sound.dll/sound.dsp", "sound/sound.dll/src/dsb.cpp")


def _row(va, status, symbol="?f@@YAXXZ", note="", score="100.0"):
    return {"va": va, "rva": va - 0x10000000, "size": 8, "kind": "G", "name": f"f{va:x}", "symbol": symbol, "how": "public",
            "status": status, "score": score, "note": note}


def _fake_tree(tmp_path, monkeypatch, files):
    (tmp_path / "re" / "match" / "wip").mkdir(parents=True)
    for name, text in files.items():
        (tmp_path / "re" / "match" / name).write_text(text)
    monkeypatch.setattr(smsg_import, "ROOT", tmp_path)
    monkeypatch.setattr(smsg_import, "header", lambda *a: "// header\n")
    return tmp_path / "re" / "match"


def test_icf_address_held_by_an_earlier_smsg_file_is_not_written_again(tmp_path, monkeypatch):
    mm = _fake_tree(tmp_path, monkeypatch, {"smsg_sound_CTimer.cpp": "// MATCH: sound.dll 0x10001000 ?f@@YAXXZ\n"})
    out = smsg_import.write_outputs(SND[1], SND[0], [_row(0x10001000, "100")], False)
    assert out["new"] == 0 and out["calib"] == 1
    assert not (mm / "smsg_sound_dsb.cpp").exists()           # a canonical file of only ALSO lines is never written


def test_a_later_smsg_file_does_not_hold_the_address(tmp_path, monkeypatch):
    _fake_tree(tmp_path, monkeypatch, {"smsg_sound_zzz.cpp": "// MATCH: sound.dll 0x10001000 ?f@@YAXXZ\n"})
    out = smsg_import.our_matches("smsg_sound_dsb.cpp")
    assert (smsg_import.smsg_pdb.OUR_MODULE["sound"], 0x10001000) not in out
    assert out == {} and smsg_import.our_matches("smsg_sound_zzzz.cpp")


def test_stale_ALSO_only_canonical_file_is_removed(tmp_path, monkeypatch):
    mm = _fake_tree(tmp_path, monkeypatch, {"smsg_sound_CTimer.cpp": "// MATCH: sound.dll 0x10001000 ?f@@YAXXZ\n",
                                           "smsg_sound_dsb.cpp": "// ALSO 100%, already in x: sound.dll 0x10001000 s\n"})
    smsg_import.write_outputs(SND[1], SND[0], [_row(0x10001000, "100")], False)
    assert not (mm / "smsg_sound_dsb.cpp").exists()


def test_error_rows_go_to_wip_as_comments_not_match_lines(tmp_path, monkeypatch):
    mm = _fake_tree(tmp_path, monkeypatch, {})
    rows = [_row(0x10002000, "error", note="no Ghidra size for sound.dll 0x10002000", score=""),
            _row(0x10003000, "miss", score="97.0")]
    out = smsg_import.write_outputs(SND[1], SND[0], rows, False)
    text = (mm / "wip" / "smsg_sound_dsb.cpp").read_text()
    assert out["new"] == 0
    assert "// SMSG-ERROR: sound.dll 0x10002000" in text and "symbol=?f@@YAXXZ" in text
    assert re.findall(r"(?m)^// MATCH: sound.dll (0x\w+)", text) == ["0x10003000"]    # only the scored miss
