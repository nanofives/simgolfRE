"""P19D (Mariano's play-test 7 item 6, log/roadmap/P19/P19D_notes.md): the play switch showBulletinBoard.photoLastPostOnly.

The bulletin board keeps ONE pending photo (f_w_4e3db8: the record id and its x; the y in g_scrollTextWin + 0x54), and every
postEvent overwrites it. On Easy openHole posts records 0 / 1 / 4 (the first dogleg right / dogleg left / par 5 hole) in one
call, so the original saves only the last record's photo (snapshots\\accomp<id>.bmp) and the earlier records' polaroids stay empty. With the switch at 0
(the `play` profile) the capture also saves the same picture for every other record of the same date whose photo is missing.

  * switch 1 (the original, also the negative control of the 0 arm): only accomp4.bmp;
  * switch 0: accomp0.bmp equal to accomp4.bmp; a record of another date and an existing photo are left as they are;
  * no `snapshots` directory: nothing is written at either value.
"""
from __future__ import annotations

import pytest

from port.test import lib as L
from port.test import segments as SG

pytestmark = pytest.mark.port

SNAP = "snapshots\\accomp"
SWITCH = "showBulletinBoard.photoLastPostOnly"


@pytest.fixture(scope="module")
def glib():
    lib = L.load()
    if SWITCH not in lib.defects():
        pytest.skip("the library has no " + SWITCH)
    if not lib.exe_loaded() and lib.exe_load():
        pytest.skip("no exe image (original/golf_clean.exe or SG_EXE_PATH)")
    yield lib
    lib.defect_reset()


def _off(lib, name):
    for n, o, _s in lib.objects():
        if n == name:
            return o
    raise KeyError(name)


def _w32(st, o, v):
    st.write(o, (v & 0xFFFFFFFF).to_bytes(4, "little"))


def _r32(st, o):
    return int.from_bytes(st.read(o, 4), "little", signed=True)


def _board(lib, switch_on, existing=(), with_dir=True):
    """records 0 and 4 dated 0x5000 (openHole's first dogleg right and first par 5), record 9 dated 0x4000, record 4 pending at
    the camera tile of an 800 x 600 view; showBulletinBoard called once (the capture, then g_bulletinAnimTick 1 < 0x14 returns)"""
    lib.defect(SWITCH, switch_on)
    with lib.state() as st:
        st.set_data_base(SG.DATA_VA)
        st.fill_from_exe(True)
        st.call("gfx.attach", [])
        if with_dir:
            st.files_put("snapshots/readme.txt", b"x")
        for k in existing:
            st.files_put(f"{SNAP}{k}.bmp", b"old")
        for name, v in (("g_screenWidth", 800), ("g_screenHeight", 600), ("g_zoomScale", 4), ("g_cameraRot", 0),
                        ("gCameraTileX", 20), ("gCameraTileY", 20), ("g_bulletinAnimTick", 0)):
            _w32(st, _off(lib, name), v)
        ev = _off(lib, "g_bulletinEvents") + 0x28
        for k, d in ((0, 0x5000), (4, 0x5000), (9, 0x4000)):
            _w32(st, ev + 0x30 * k, d)
        ph = _off(lib, "f_w_4e3db8")
        _w32(st, ph, 4)
        _w32(st, ph + 4, (20 << 10) + 0x200)
        _w32(st, _off(lib, "g_scrollTextWin") + 0x54, (20 << 10) + 0x200)
        st.call("showBulletinBoard", [])
        assert _r32(st, ph + 4) == -1, "the capture did not run (the position is still pending)"
        return {k: f for k in range(22) if (f := st.files_read(f"{SNAP}{k}.bmp")) is not None}


def test_original_saves_only_the_last_post(glib):
    files = _board(glib, True)
    assert sorted(files) == [4]


def test_play_saves_the_same_day_records_too(glib):
    files = _board(glib, False)
    assert sorted(files) == [0, 4]
    assert files[0] == files[4] and len(files[4]) == 0x36 + 0xC8 * 0xA0 * 3


def test_play_keeps_an_existing_photo(glib):
    files = _board(glib, False, existing=(0,))
    assert files[0] == b"old" and sorted(files) == [0, 4]


@pytest.mark.parametrize("switch_on", [True, False])
def test_no_snapshots_directory_writes_nothing(glib, switch_on):
    assert _board(glib, switch_on, with_dir=False) == {}
