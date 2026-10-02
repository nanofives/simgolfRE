"""Patch framework: static signature validation of committed patches + runtime apply/refuse behaviour."""
import configparser
import ctypes
import pathlib
import time

import pefile
import pytest

from conftest import GAME_DIR, ROOT
from game import Game

PATCH_DIR = ROOT / "patches"
TITLE_RVA = 0xD3910  # "Sid Meier's SimGolf\0" in golf_clean.exe .data (VA 0x004d3910)


def _read_patch(path):
    cp = configparser.ConfigParser()
    cp.read(path)
    s = cp["patch"]
    return {
        "module": s["module"], "rva": int(s["rva"], 0),
        "expect": bytes.fromhex(s["expect"].replace(",", " ")),
        "write": bytes.fromhex(s["write"].replace(",", " ")),
    }


def _bytes_at(module, rva, n):
    pe = pefile.PE(str(GAME_DIR / module))
    return pe.get_data(rva, n)


@pytest.mark.static
@pytest.mark.parametrize("path", sorted(PATCH_DIR.glob("*.ini")), ids=lambda p: p.stem)
def test_committed_patch_matches_anchor(path):
    """Every patch in patches/ must find its expected bytes in the anchored binary."""
    p = _read_patch(path)
    assert len(p["expect"]) == len(p["write"])
    assert _bytes_at(p["module"], p["rva"], len(p["expect"])) == p["expect"]


@pytest.mark.static
def test_title_string_location():
    assert _bytes_at("golf_clean.exe", TITLE_RVA, 20) == b"Sid Meier's SimGolf\x00"


def _write(dirpath, name, module, rva, expect: bytes, write: bytes, desc="test"):
    (dirpath / f"{name}.ini").write_text(
        f"[patch]\ndesc={desc}\nmodule={module}\nrva=0x{rva:x}\nexpect={expect.hex(' ')}\nwrite={write.hex(' ')}\nenabled=1\n")


def _title(hwnd):
    buf = ctypes.create_unicode_buffer(128)
    ctypes.windll.user32.GetWindowTextW(hwnd, buf, 128)
    return buf.value


@pytest.mark.runtime
def test_patches_apply_refuse_and_defer(tmp_path):
    good = b"SimGolf RE patched!"
    assert len(good) == len(b"Sid Meier's SimGolf")
    _write(tmp_path, "title", "golf_clean.exe", TITLE_RVA, b"Sid Meier's SimGolf", good)
    _write(tmp_path, "wrong_sig", "golf_clean.exe", TITLE_RVA, b"XXXXXXXXXXXXXXXXXXX", b"YYYYYYYYYYYYYYYYYYY")
    jg = (GAME_DIR / "jgld.dll").read_bytes()
    i = jg.find(b"invalid window size")
    pe = pefile.PE(str(GAME_DIR / "jgld.dll"))
    _write(tmp_path, "late_dll", "jgld.dll", pe.get_rva_from_offset(i), b"invalid", b"INVALID")

    if Game.running_instances():
        pytest.fail("SimGolf already running")
    with Game(env={"SIMGOLF_SKIP_INTRO": "1", "SIMGOLF_PATCH_DIR": str(tmp_path)}) as g:
        h = g.wait_window()
        time.sleep(2)
        title = _title(h)
        log = (GAME_DIR / "simgolf_shim.log").read_text()
    assert title == good.decode(), log
    assert "patch title: applied 19 bytes at golf_clean.exe+0xd3910" in log
    assert "patch wrong_sig: SKIP mismatch" in log
    assert "patch late_dll: pending jgld.dll" in log
    assert "patch late_dll: applied 7 bytes at jgld.dll+" in log
