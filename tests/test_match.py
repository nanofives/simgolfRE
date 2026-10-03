"""Matching decompilation with the original compiler (VC6 cl 12.00.8168, tools/vc6 extracted from the
user's Visual Studio 6 disc). Static layer; skipped when the toolchain is not present."""
import pathlib
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
import match  # noqa: E402

pytestmark = [pytest.mark.static,
              pytest.mark.skipif(not (ROOT / "tools" / "vc6" / "vc98" / "bin" / "cl.exe").exists(),
                                 reason="VC6 toolchain not extracted to tools/vc6")]


def test_compiler_is_the_build_in_the_rich_headers():
    import subprocess
    r = subprocess.run([str(match.VC / "bin" / "cl.exe")], capture_output=True, text=True)
    assert "Version 12.00.8168" in r.stderr + r.stdout


def test_terrain_accessors_match_both_builds():
    res = match.compare(ROOT / "re" / "match" / "terrain.cpp", "", False)
    assert len(res) == 8
    assert all(r[4] == 1.0 for r in res), [(r[0], hex(r[1]), r[4]) for r in res]


@pytest.mark.parametrize("src", sorted((ROOT / "re" / "match").glob("*.cpp")), ids=lambda p: p.stem)
def test_every_matched_source_is_100_percent(src):
    """Every MATCH annotation in re/match/ recompiles with VC6 to the original instructions."""
    res = match.compare(src, "", False)
    assert res, f"{src.name} has no MATCH annotations"
    bad = [(r[0], hex(r[1]), f"{100 * r[4]:.1f}%") for r in res if r[4] != 1.0]
    assert not bad, bad


def test_tilehit_matches_with_float_and_crt_calls():
    res = match.compare(ROOT / "re" / "match" / "terrain_tilehit.cpp", "", False)
    assert len(res) == 1 and res[0][4] == 1.0 and len(res[0][5]) >= 150


def test_tilehit_rejects_24_minus_a(tmp_path):
    """`24 - a` and `-a + 24` are the same value; only one is the original code (neg/add at 0x1000ac88)."""
    f = tmp_path / "t.cpp"
    f.write_text((ROOT / "re" / "match" / "terrain_tilehit.cpp").read_text().replace("tileY = -a + 24;", "tileY = 24 - a;"))
    assert match.compare(f, "", False)[0][4] < 1.0


def test_matcher_rejects_equivalent_but_different_source(tmp_path):
    src = (ROOT / "re" / "match" / "terrain.cpp").read_text()
    src = src.replace("if (x >= m_width || x < 0 || y >= m_height || y < 0)",
                      "if (x < 0 || y < 0 || x >= m_width || y >= m_height)")   # same semantics
    f = tmp_path / "t.cpp"
    f.write_text(src)
    res = {(r[0], r[2].split("@")[0]): r[4] for r in match.compare(f, "", False)}
    assert res[("golf_clean.exe", "?tileAt")] < 1.0
    assert res[("Terrain.dll", "?tileAt")] < 1.0


def test_matcher_rejects_wrong_flags():
    res = match.compare(ROOT / "re" / "match" / "terrain.cpp", "/O1", False)
    assert any(r[4] < 1.0 for r in res)


def test_shim_links_the_vc6_compiled_tileat():
    """The hook's detour is VC6 output: the original 57 bytes of 0x004490d0 (relocation-free) appear verbatim
    in the built shim (shim\build_vc6.bat + /DSG_VC6_TERRAIN)."""
    import pefile
    original = pefile.PE(str(ROOT / "original" / "golf_clean.exe")).get_data(0x490D0, 0x39)
    assert original in (ROOT / "shim" / "build" / "winmm.dll").read_bytes()


def test_switch_tables_are_stripped_but_case_order_still_counts(tmp_path):
    """The matcher drops VC6's trailing jump tables (not part of Ghidra's body) but still compares the
    case blocks: moving `case 6` ahead of `case 0x11` in Tile::setTypeId must not match."""
    src = (ROOT / "re" / "match" / "terrain_tile2.cpp").read_text()
    ok = [r for r in match.compare(ROOT / "re" / "match" / "terrain_tile2.cpp", "", False) if "setTypeId" in r[2]]
    assert ok[0][4] == 1.0
    a = "    case 0x11:\n        f_1000d480(type);\n        break;\n"
    b = "    case 6:\n        f_1000de60();\n        break;\n"
    assert a in src and b in src
    f = tmp_path / "t.cpp"
    f.write_text(src.replace(a + b, b + a))
    bad = [r for r in match.compare(f, "", False) if "setTypeId" in r[2]]
    assert bad[0][4] < 1.0
