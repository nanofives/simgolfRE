"""re/tools/c2_note.py: the generated C2 transcription has the sections the C2 gate requires and agrees with the
hand-written note of 0x004490d0 (re/analysis/terrain/004490d0_tileAt.md) on every fact both state."""
import sys

import pytest

from conftest import ROOT

sys.path.insert(0, str(ROOT / "re" / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
import c2_note  # noqa: E402
import re_classify as rc  # noqa: E402

pytestmark = pytest.mark.static


@pytest.fixture(scope="module")
def ctx():
    return c2_note.Ctx()


def test_tileat_transcription_matches_the_hand_note(ctx):
    text, callers, callees = c2_note.note(ctx, "golf_clean.exe", 0x004490D0, "Terrain::tileAt", "terrain")
    for sec in rc.NOTE_SECTIONS_C2:
        assert sec in text
    assert "## Purpose" not in text                        # a generated note never claims the C3 purpose
    assert "`[ecx+0x14]` dword at 0x004490d0" in text      # width field read
    assert "`[ecx+0x18]` dword at 0x004490e5" in text      # height field read
    assert "`0x3a4` (932) at 0x004490fd" in text           # offset of tile 0
    assert "`jge` (signed) at 0x004490da, 0x004490eb" in text
    assert "`jl` (signed) at 0x004490de, 0x004490ef" in text
    assert "`ret 0x8`" in text and "callee pops 8 bytes" in text
    assert "`ecx` is read at 0x004490d0 before any write" in text
    assert "## Writes\nNone." in text and "## Callees\nNone (leaf)." in text
    assert callers == [] and callees == []


def test_debug_dll_thunks_and_imports_are_resolved(ctx):
    text, _, callees = c2_note.note(ctx, "jgld.dll", 0x10002870, "MappedFile::openRead", "util")
    assert "-> 0x10002e10" in text and "(incremental-linking thunk)" in text
    assert "import `KERNEL32.dll!CreateFileA` at 0x100028d8" in text
    assert 0x10002E10 in callees and 0x100018BB not in callees


def test_strings_are_cited_by_address_only(ctx):
    # 0x1002fc10 references no string; a function that does gets "address of a string (text not reproduced)"
    text, _, _ = c2_note.note(ctx, "jgld.dll", 0x10002870, "MappedFile::openRead", "util")
    assert '"' not in text.split("## Constants", 1)[1]
