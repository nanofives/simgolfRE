"""P38FE2 (Mariano 2026-11-19; log/roadmap/P38/P38FE2_notes.md).

1. The start cash scales with the world: world.h world_cash = the 50-size amount * ((N - 2) / 48)^2 rounded to $1,000.
2. Play switch showBuyLandScreen.financeOtherNarrow (1 = the original): six 152 lots bought one after the other wrap the year's
   finance row's int16 `other` word; at 0 the finance report's value (finance_other_shown) is the true sum, at 1 the wrapped word.
3. Key C at N != 50 is judged in the app (screenshot)."""
from __future__ import annotations

import struct

import pytest

from port.test import lib as L
from port.test import test_commands as TC
from port.test import test_p38fe_buyland as FE

pytestmark = [pytest.mark.port,
              pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")]


def test_world_cash_scale(portlib):
    with portlib.state() as st:
        assert [st.call("p38feWorldCash", [c]).eax for c in (1000, 10000)] == [1000, 10000]      # 50: unchanged
        st.world_resize(152)
        assert [st.call("p38feWorldCash", [c]).eax for c in (1000, 10000)] == [9770, 97660]     # $977,000 / $9,766,000
        st.world_resize(101)
        assert [st.call("p38feWorldCash", [c]).eax for c in (1000, 10000)] == [4250, 42540]     # $425,000 / $4,254,000
        st.world_resize(50)


def _buy_six(portlib, narrow: bool):
    portlib.defect("showBuyLandScreen.financeOtherNarrow", narrow)
    with portlib.state() as st:
        if st.lib.has("audio_exe.stubRequests"):
            st.call("audio_exe.stubRequests", [1])
        st.set_data_base(TC.DATA_VA)
        st.load_image(FE._course(portlib, 152))
        st.world_resize(152)
        st.call("generateFlatCourse", [3])
        o = TC.objs(portlib)
        year = struct.unpack_from("<h", st.image(), o["g_year"][0])[0]
        row = year % 100
        at = TC.DATA_VA + o["g_financeYears"][0] + 20 * row + 0xE
        start = struct.unpack("<h", st.mem_read(at, 2))[0]
        st.mem_write(TC.DATA_VA + o["g_cash_div100"][0], struct.pack("<i", 10 ** 7))
        paid = 0
        for p, lot in enumerate((8, 6, 2, 0, 7, 1)):
            st.mem_write(TC.DATA_VA + o["g_landPurchaseCount"][0], struct.pack("<i", p))
            before = FE._cash(st)
            x, y = FE._lot_point(lot)
            assert FE._run(st, FE.CE.click(x, y), (x, y), 10 + p) == 1
            paid += before - FE._cash(st)
        word = struct.unpack("<h", st.mem_read(at, 2))[0]
        shown = st.call("p38feFinanceOther", [row]).eax
        shown = shown - (1 << 32) if shown & 0x80000000 else shown
    return start, paid, word, shown


def test_six_152_lots_wrap_the_word_and_the_wide_sum_holds(portlib, capsys):
    start, paid, word, shown = _buy_six(portlib, narrow=False)
    true = start - paid
    assert paid > 0x7FFF                                                        # past 16 bits
    assert word == struct.unpack("<h", struct.pack("<H", true & 0xFFFF))[0]       # the stored word: the original's int16 sum
    assert word != true and shown == true                                       # the report: the true sum
    start1, paid1, word1, shown1 = _buy_six(portlib, narrow=True)
    assert (paid1, word1) == (paid, word) and shown1 == word1                   # switch 1: the original's wrapped word
    with capsys.disabled():
        print(f"\n[P38FE2] six 152 lots: paid {paid} (x100 $), row other: word {word}, report {shown} (switch 1: {shown1})")
