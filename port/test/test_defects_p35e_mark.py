"""P35E item 10: the play switch loadGame.cheatMarkKept.

g_flags bit 29 (the asterisk P6_StatusBar draws after the cash) is set only by the cash key '+'. With the switch at 1 (the original)
loadGame keeps the file's bit 29 (it rewrites bits 2, 3 and 30 only) and switchToCourse (the career's move to another course,
courseActionMenu's purchase) writes no flag bit, so the mark stays on a course and follows the career; at 0 both clear it. Every
other bit is the original's."""
from __future__ import annotations

import ctypes as C
import os
import pathlib
import struct
import sys
import tempfile

import pytest

from port.test import emu38b as E
from port.test import lib as L
from port.test import test_course_menu as CM
from port.test import test_saveload as TS

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "re" / "tools"))
import sve  # noqa: E402

SWITCH = "loadGame.cheatMarkKept"
BIT29 = 1 << 29
FLAGS_VA = 0x0059E7B8  # the save field that holds g_flags (re/tools/sve.py FLAGS_VA)


@pytest.fixture(autouse=True)
def _reset(portlib):
    portlib.defect_reset()
    yield
    portlib.defect_reset()


def _marked(name="mood5.sve", mark=True) -> bytes:
    blob = bytearray((TS.SAVES / name).read_bytes())
    s = sve.parse(bytes(blob))
    f = s.field(FLAGS_VA)
    v = struct.unpack_from("<I", blob, f.offset)[0]
    struct.pack_into("<I", blob, f.offset, (v | BIT29) if mark else (v & ~BIT29))
    return bytes(blob)


def _flags(lib, image) -> int:
    return struct.unpack_from("<I", image, TS.objs(lib)["g_flags"][0])[0]


def _load(lib, mark, entry_flags=0):
    files = TS.save_files({"Saved Games/p35e_mark.sve": _marked(mark=mark)})
    img = TS.case_image(lib, seed=35, flags=entry_flags)
    eax, out, code, _ = TS.port_run(lib, "loadGame", [TS.BLOCK_VA, 1, 0, 0], img, files, text_at=TS.name_block("p35e_mark.sve"))
    assert code == 0, code
    return _flags(lib, out), out


@TS.need_exe
@TS.need_saves
def test_original_load_keeps_the_mark(portlib):
    assert portlib.defect(SWITCH, True) == 1
    f, _ = _load(portlib, True)
    assert f & BIT29


@TS.need_exe
@TS.need_saves
@pytest.mark.parametrize("entry", [0, BIT29, 0x0F0FFFFF | BIT29])
def test_play_load_clears_only_the_mark(portlib, entry):
    """the file's mark is dropped (whatever the entry flags hold); every other bit equals the original's load"""
    portlib.defect(SWITCH, True)
    a, ia = _load(portlib, True, entry)
    portlib.defect(SWITCH, False)
    b, ib = _load(portlib, True, entry)
    assert a & BIT29 and not b & BIT29
    assert a & ~BIT29 == b & ~BIT29
    o = TS.objs(portlib)["g_flags"][0]
    assert ia[:o] == ib[:o] and ia[o + 4:] == ib[o + 4:]


@TS.need_exe
@TS.need_saves
def test_play_load_of_a_clear_save_is_the_original(portlib):
    portlib.defect(SWITCH, True)
    a, ia = _load(portlib, False, BIT29)
    portlib.defect(SWITCH, False)
    b, ib = _load(portlib, False, BIT29)
    assert ia == ib and not a & BIT29


# ---- switchToCourse: courseActionMenu's purchase of record 3 (test_course_menu's buy_record_3), then the new course
def _switch(lib, mark):
    scen = next(s for s in CM.SCEN if s[0] == "buy_record_3")
    cid, builder, kw, files_extra = scen
    img = bytearray(CM.menu_image(lib, **dict(kw, flags=BIT29 if mark else 0)))
    steps = builder(lib, bytes(img))
    raws = [r for s in steps for r in s.raws]
    curs = CM.cursors_of(steps)
    words = [len(raws), len(curs)]
    for c in raws:
        words += [c.handler, c.a0 & 0xFFFFFFFF, c.a1 & 0xFFFFFFFF, c.async_ctrl, c.async_shift, c.t]
    for x, y in curs:
        words += [x & 0xFFFFFFFF, y & 0xFFFFFFFF]
    script = (C.c_uint32 * len(words))(*words)
    with tempfile.TemporaryDirectory(prefix="sg35e_switch_") as d:
        prev = os.environ.get("SG_FILES_WRITE_THROUGH")
        os.environ["SG_FILES_WRITE_THROUGH"] = "1"
        try:
            with lib.state() as st:
                st.set_data_base(E.DATA_VA)
                st.load_image(bytes(img))
                for k, v in CM._saves_themes.items():  # generateRandomCourse reads the theme files
                    q = pathlib.Path(d) / k
                    q.parent.mkdir(parents=True, exist_ok=True)
                    q.write_bytes(v)
                st.files_root(pathlib.Path(d))
                st.map(0x30000000, script, readonly=True)
                logbuf = (C.c_uint32 * (1 + 8 * TS.PEER_LOG_CAP))()
                st.map(TS.PEER_LOG_VA, logbuf)
                for nm in [q[1] for q in E.SAVE_PEERS if q[1] != "Window::refreshFull"]:  # as test_course_menu.run_port
                    st.call("commands.recordPeer", [E.fnv1a(nm), TS.PEER_LOG_VA, TS.PEER_LOG_CAP])
                st.call("saveload.setScript", [0x30000000])
                try:
                    st.call("switchToCourse", [0])
                    code = 0
                except L.SgError as e:
                    code = e.code
                st.call("saveload.clearScript", [])
                st.call("commands.recordClear", [])
                out = st.image()
        finally:
            if prev is None:
                os.environ.pop("SG_FILES_WRITE_THROUGH", None)
            else:
                os.environ["SG_FILES_WRITE_THROUGH"] = prev
    return code, out


@TS.need_exe
@TS.need_saves
def test_switch_to_course_mark(portlib):
    """at 1 the new course keeps the mark (the original); at 0 it starts without it; nothing else differs. In this harness the run
    stops inside the chosen-course callees (g_date held at 0; the menu image is not a whole game), after courseActionMenu's
    purchase picked record 3 and switchToCourse took its course type; the switch acts first on the chosen-course path, so both
    images are compared where the run stops (the same code at both values)"""
    portlib.defect(SWITCH, True)
    ca, a = _switch(portlib, True)
    portlib.defect(SWITCH, False)
    cb, b = _switch(portlib, True)
    assert ca == cb, (ca, cb)
    fa, fb = _flags(portlib, a), _flags(portlib, b)
    assert fa & BIT29 and not fb & BIT29 and fa & ~BIT29 == fb & ~BIT29
    o = TS.objs(portlib)["g_flags"][0]
    assert a[:o] == b[:o] and a[o + 4:] == b[o + 4:]
    # the chosen-course path ran: courseActionMenu picked record 3 and switchToCourse took that record's course type
    t_off = TS.objs(portlib)["g_courseType"][0]
    rec = TS.objs(portlib)["g_courseTable"][0] + 0x2E * 3
    assert a[t_off] == a[rec + 0x20]
    menu = CM.menu_image(portlib, **dict(next(s for s in CM.SCEN if s[0] == "buy_record_3")[2], flags=BIT29))
    assert a != menu


@TS.need_exe
@TS.need_saves
def test_switch_to_course_cancelled_is_the_original(portlib):
    """courseActionMenu's back button (-1): no course change, the mark stays at both values"""
    scen_back = next(s for s in CM.SCEN if s[0] == "back")
    def run():
        eax, out, code, calls, after = CM.run_port(portlib, scen_back)
        return code, out
    portlib.defect(SWITCH, True)
    a = run()
    portlib.defect(SWITCH, False)
    b = run()
    assert a[0] == b[0] and a[1] == b[1]
