"""S8.1: the movie / FLIC inventory (re/tools/movie_inventory.py, re/port/movies.tsv, re/port/flics_census.tsv) is
complete against a fresh derivation from original/golf_clean.exe, and a changed row address reads RED."""
import copy
import struct
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
pytestmark = pytest.mark.static

if not (ROOT / "original" / "golf_clean.exe").exists():
    pytest.skip("original/golf_clean.exe missing", allow_module_level=True)

import movie_inventory as mi  # noqa: E402


@pytest.fixture(scope="module")
def derived():
    return mi.derive()


@pytest.fixture(scope="module")
def tables():
    return mi.read_tsv(mi.MOVIES_TSV), mi.read_tsv(mi.FLICS_TSV)


def test_committed_tables_are_complete(derived, tables):
    assert mi.verify(derived, *tables) == []


def test_committed_tables_equal_a_fresh_derivation(derived, tables):
    mr, fr = mi.movies_rows(derived), mi.flic_rows(derived)
    assert tables[0] == [{c: str(r.get(c, "")) for c in mi.MOVIE_COLS} for r in mr]
    assert tables[1] == [{c: str(r.get(c, "")) for c in mi.FLIC_COLS} for r in fr]


def test_the_two_movies_from_raw_bytes(tables):
    """Independent of the tool's scanner: the call bytes and the string bytes at the cited addresses."""
    data = (ROOT / "original" / "golf_clean.exe").read_bytes()
    pe = mi.pefile.PE(str(ROOT / "original" / "golf_clean.exe"))

    def at(va, n):
        return data[pe.get_offset_from_rva(va - 0x400000):][:n]

    movies = {r["call_site"]: r for r in tables[0] if r["kind"] == "movie"}
    assert sorted(movies) == ["0x0040faa4", "0x0043cd97"]
    for cs, path_va, imm in ((0x0040FAA4, 0x004C6BB4, "1"), (0x0043CD97, 0x004C8A0C, "0")):
        rel = struct.unpack_from("<i", at(cs + 1, 4))[0]
        assert at(cs, 1) == b"\xe8" and (cs + 5 + rel) & 0xFFFFFFFF == mi.PLAY_AUDIO_FILE
        assert at(cs - 5, 1) == b"\x68" and struct.unpack_from("<I", at(cs - 4, 4))[0] == path_va
        r = movies[f"0x{cs:08x}"]
        assert r["string_va"] == f"0x{path_va:08x}" and r["arg_immediate"] == imm
        assert at(path_va, 40).split(b"\0")[0].decode().lower() == r["path_string"].lower()
    # the opcode before the immediate argument: push 0 / push 1 (6a 00 / 6a 01) before the path push
    assert at(0x0040FAA4 - 7, 2) == b"\x6a\x01" and at(0x0043CD97 - 6, 1) == b"\x53"  # push 1 ; push ebx (ebx = 0)


def test_nine_imports_claim_is_sixteen(derived):
    names = [n for _, n in derived["imports"]["golf_clean.exe"]]
    assert len(names) == 16 and "_BinkOpen@8" in names
    assert all(not derived["imports"][m] for m in mi.MODULES[1:])


def test_no_loadflic_call_site_is_unnamed(derived):
    assert len(derived["flic"]) == 821
    assert all(c["names"] or c.get("missing") for c in derived["flic"])


def test_control_a_changed_row_address_reads_red(derived, tables):
    mr, fr = copy.deepcopy(tables[0]), tables[1]
    row = next(r for r in mr if r["kind"] == "movie")
    row["call_site"] = "0x0040faa5"
    assert mi.verify(derived, mr, fr) != []


def test_control_a_dropped_slot_row_reads_red(derived, tables):
    mr = [r for r in tables[0] if r["call_site"] != "0x00402863"]
    assert any("0x00402863" in p for p in mi.verify(derived, mr, tables[1]))
