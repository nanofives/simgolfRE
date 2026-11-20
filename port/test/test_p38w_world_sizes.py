"""P38W (Mariano 2026-11-20; log/roadmap/P38/P38W_notes.md): the world map's courses carry their world sizes.
P38W2 (log/roadmap/P38/P38W2_notes.md): the acres an even progression 40 .. 600 over records 0..15 (byte +0x2a = acres / 10), the
size the smallest that holds them with half a lot for sale on every side (records 0..2 50, 3..11 101, 12..15 152), the price
$1,250 an acre to the nearest $1,000; a record without byte +0x2a (an older save) keeps P38W's tier price scaled by its size.

Play switch courseActionMenu.worldSizes (1 = the original). At 0: seedCourseTable writes each record's size into byte +0x29
(code 0 = 50, else N = 3 * code + 2; P38W2's split below) and draws nothing more from the RNG; the world map
prices a record at ((N - 2) / 48)^2 times its tier price (to the nearest $1,000; the sandbox's test stays the 50-size price) and
the purchase takes that price; switchToCourse (a career's next course) starts the world at the record's size. The map's drawing
(the size badge, the scaled acres) is judged in the app (screenshots, P38W_notes.md)."""
from __future__ import annotations

import ctypes as C
import struct

import pytest

from port.test import lib as L
from port.test import test_course_menu as TCM
from port.test import test_saveload as TS
from port.test import emu38b as E

pytestmark = [pytest.mark.port,
              pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")]

SWITCH = "courseActionMenu.worldSizes"
TIER = [500, 600, 700, 800, 1200, 1500, 2000, 2500, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000]
ACRES = [40, 80, 110, 150, 190, 230, 260, 300, 340, 380, 410, 450, 490, 530, 560, 600]   # P38W2
SIZES = [50] * 3 + [101] * 9 + [152] * 4


def price_of_acres(a: int) -> int:
    return (a * 125 + 50) // 100 * 10   # $1,250 an acre, to the nearest $1,000 (x $100)


def at(v50: int, n: int) -> int:
    """world.h world_amount_at"""
    if n == 50:
        return v50
    b = n - 2
    return (v50 * b * b + 2304 * 5) // (2304 * 10) * 10


def _table(lib, img: bytes) -> bytes:
    off, _ = TS.objs(lib)["g_courseTable"]
    return img[off:off + 17 * 0x2E]


def _seed(portlib, switch_on: bool):
    portlib.defect(SWITCH, not switch_on)
    try:
        with portlib.state() as st:
            st.set_data_base(E.DATA_VA)
            st.load_image(TCM.menu_image(portlib, 31))
            st.call("seedCourseTable", [])
            return st.image()
    finally:
        portlib.defect_reset()


def test_seed_writes_the_sizes_and_nothing_else(portlib):
    on, off = _seed(portlib, True), _seed(portlib, False)
    t_on, t_off = _table(portlib, on), _table(portlib, off)
    codes = [t_on[0x2E * c + 0x29] for c in range(16)]
    assert codes == [0 if n == 50 else (n - 2) // 3 for n in SIZES]       # P38W2: 50 x 3, 101 x 9, 152 x 4
    assert [t_on[0x2E * c + 0x2A] * 10 for c in range(16)] == ACRES        # the even acres
    assert [t_on[0x2E * c + 0x1D] for c in range(16)] == [min(a, 0xFA) for a in ACRES]   # the original's byte (50 x 50 ones exact)
    assert all(t_off[0x2E * c + 0x29] == 0 and t_off[0x2E * c + 0x2A] == 0 for c in range(17))   # the switch at 1: the original
    strip = lambda t: bytes(b for i, b in enumerate(t) if i % 0x2E not in (0x1D, 0x29, 0x2A))
    assert strip(t_on) == strip(t_off)                                     # the same records (the same RNG draws)
    off_t, _ = TS.objs(portlib)["g_courseTable"]
    assert on[:off_t] == off[:off_t] and on[off_t + 17 * 0x2E:] == off[off_t + 17 * 0x2E:]


def _buy(portlib, record: int, code: int, cash: int, switch_on: bool, flags: int = 0, refused: bool = False, acres10: int = 0):
    """the world map with record `record` (open, size code `code`) bought by a press on its spot; returns (eax, cash after)"""
    img = bytearray(TCM.menu_image(portlib, 7, cash=cash, flags=flags))
    off, _ = TS.objs(portlib)["g_courseTable"]
    img[off + 0x2E * record + 0x29] = code
    img[off + 0x2E * record + 0x2A] = acres10
    img[off + 0x2E * record + 0x22] = 0xFF
    steps = [TCM.press(*TCM.spot_of_record(portlib, bytes(img), record))]
    if refused:   # the insufficient-funds window waits for a key (test_course_menu's buy_without_cash)
        steps += [TCM.MStep([TCM.CE.Raw(8, 0x41)], TCM.BACK, 0), TCM.MStep([TCM.CE.Raw(7, 0x41, 0x41)], TCM.BACK, 0)]
    steps += [TCM.press(*TCM.BACK)]
    raws = [r for s in steps for r in s.raws]
    curs = TCM.cursors_of(steps)
    words = [len(raws), len(curs)]
    for c in raws:
        words += [c.handler, c.a0 & 0xFFFFFFFF, c.a1 & 0xFFFFFFFF, c.async_ctrl, c.async_shift, c.t]
    for x, y in curs:
        words += [x & 0xFFFFFFFF, y & 0xFFFFFFFF]
    script = (C.c_uint32 * len(words))(*words)
    portlib.defect(SWITCH, not switch_on)
    try:
        with portlib.state() as st:
            if st.lib.has("audio_exe.stubRequests"):   # the purchase's sound (records whose sound byte is 0..3)
                st.call("audio_exe.stubRequests", [1])
            st.set_data_base(E.DATA_VA)
            st.load_image(bytes(img))
            st.map(0x30000000, script, readonly=True)
            st.call("saveload.setScript", [0x30000000])
            eax = st.call("courseActionMenu", [0]).eax
            st.call("saveload.clearScript", [])
            o, _ = TS.objs(portlib)["g_cash_div100"]
            return eax, struct.unpack_from("<i", st.image(), o)[0]
    finally:
        portlib.defect_reset()


@pytest.mark.parametrize("code,n", [(0, 50), (33, 101), (50, 152)])
def test_purchase_takes_the_price_at_the_record_size(portlib, code, n):
    cash = 10 ** 6
    eax, after = _buy(portlib, 9, code, cash, switch_on=True)
    assert eax == 9 and after == cash - at(TIER[9], n)                     # 4000 -> 4000 / 17000 / 39060 (x $100)
    eax, after = _buy(portlib, 9, code, cash, switch_on=False)
    assert eax == 9 and after == cash - TIER[9]                            # the switch at 1: the original's price, size ignored


def test_unaffordable_at_size_is_refused_and_sandbox_keeps_the_50_price(portlib):
    eax, after = _buy(portlib, 9, 50, 9000, switch_on=True, refused=True)  # 39,060 > 9,000: no purchase (Back)
    assert eax == 0xFFFFFFFF and after == 9000
    eax, after = _buy(portlib, 9, 50, 9000, switch_on=True, flags=0x1000000)
    assert eax == 9 and after == 9000 - TIER[9]                            # the sandbox tests and takes the 50-size price


def test_size_codes_outside_world_sizes_read_50(portlib):
    eax, after = _buy(portlib, 9, 16, 10 ** 6, switch_on=True)            # 3 * 16 + 2 = 50: the original size
    assert after == 10 ** 6 - TIER[9]
    eax, after = _buy(portlib, 9, 200, 10 ** 6, switch_on=True)           # 602: not a world size -> 50
    assert after == 10 ** 6 - TIER[9]


@pytest.mark.parametrize("code,n", [(50, 152), (33, 101), (0, 50)])
def test_switch_to_course_starts_the_world_at_the_record_size(portlib, code, n):
    """switchToCourse (a career's next course) after the map: the world resized to the bought record's size before the course is
    made, the price taken at it and no cash scaling."""
    img = bytearray(TCM.menu_image(portlib, 7, cash=10 ** 6))
    off, _ = TS.objs(portlib)["g_courseTable"]
    img[off + 0x2E * 9 + 0x29] = code
    img[off + 0x2E * 9 + 0x22] = 0xFF
    steps = [TCM.press(*TCM.spot_of_record(portlib, bytes(img), 9))]
    raws = [r for s in steps for r in s.raws]
    curs = TCM.cursors_of(steps)
    words = [len(raws), len(curs)]
    for c in raws:
        words += [c.handler, c.a0 & 0xFFFFFFFF, c.a1 & 0xFFFFFFFF, c.async_ctrl, c.async_shift, c.t]
    for x, y in curs:
        words += [x & 0xFFFFFFFF, y & 0xFFFFFFFF]
    script = (C.c_uint32 * len(words))(*words)
    why = portlib.exe_load()   # generateRandomCourse / buildGolferBioText read the exe's tables and the game files
    if why:
        pytest.skip(why)
    portlib.defect(SWITCH, False)
    try:
        with portlib.state() as st:
            st.files_root()
            if st.lib.has("audio_exe.stubRequests"):
                st.call("audio_exe.stubRequests", [1])
            st.set_data_base(E.DATA_VA)
            st.load_image(bytes(img))
            st.map(0x30000000, script, readonly=True)
            st.call("saveload.setScript", [0x30000000])
            try:
                st.call("switchToCourse", [0])
            except L.SgError:
                # this headless state has no Terrain object: applyCourseType (after the purchase, the resize and the generation)
                # or, at 50, generateRandomCourse over the menu image stops the call; the app drives judge the whole start
                pass
            st.call("saveload.clearScript", [])
            assert st.world_n() == n
            o, _ = TS.objs(portlib)["g_cash_div100"]
            assert struct.unpack_from("<i", st.image(), o)[0] == 10 ** 6 - at(TIER[9], n)
            if n != 50:
                st.world_resize(50)
    finally:
        portlib.defect_reset()


@pytest.mark.parametrize("record", [0, 1, 2, 3, 9, 11, 12, 15])
def test_p38w2_price_is_proportional_to_the_acres(portlib, record):
    a, n = ACRES[record], SIZES[record]
    cash = 10 ** 5
    eax, after = _buy(portlib, record, 0 if n == 50 else (n - 2) // 3, cash, switch_on=True, acres10=a // 10)
    assert eax == record and after == cash - price_of_acres(a)            # $50,000 for 40 acres .. $750,000 for 600
    eax, after = _buy(portlib, record, 0 if n == 50 else (n - 2) // 3, cash, switch_on=False, acres10=a // 10)
    assert after == cash - TIER[record]                                    # the switch at 1: the original's tier price


def test_p38w2_start_cash_buys_records_0_and_1(portlib):
    """the career's $100,000: 40 acres ($50,000) and 80 acres ($100,000) can be bought, 110 acres ($138,000) cannot"""
    assert [price_of_acres(a) <= 1000 for a in ACRES] == [True, True] + [False] * 14
    eax, after = _buy(portlib, 1, 0, 1000, switch_on=True, acres10=8)
    assert eax == 1 and after == 0
    eax, after = _buy(portlib, 2, 0, 1000, switch_on=True, acres10=11, refused=True)
    assert eax == 0xFFFFFFFF and after == 1000
