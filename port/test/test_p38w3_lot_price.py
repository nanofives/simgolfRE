"""P38W3 (Mariano 2026-11-20; log/roadmap/P38/P38W3_notes.md): TRACTS FOR SALE on the world map's price scale.

Play switch showBuyLandScreen.lotPricePerTile (1 = the original's per-tile sum: 5, plus 1 << g_landPurchaseCount for every tile for
sale whose Random::range(3) draw is 0, * 20 / 100 * 10). At 0 a lot costs the acres the screen shows for it (its tiles for sale / 10)
x $1,250, to the nearest $1,000 (worldsizes.h world_price_of_acres), at every world size and after any number of purchases; the
draws are still made (the same RNG state after the screen), the purchase converts the same tiles, and mainLoop's frame still adds
one to g_landPurchaseCount per purchase (tick_frame.inc ML+0xed76; this test sets the count before each purchase). The screen's price line shows that cost (the purchase test and the cash taken use the same frame word)."""
from __future__ import annotations

import struct

import pytest

from port.test import cmdemu as CE
from port.test import lib as L
from port.test import test_commands as TC
from port.test import test_p38fe_buyland as FE

pytestmark = [pytest.mark.port,
              pytest.mark.skipif(not L.exe_path().exists(), reason="the user's golf_clean.exe is not present")]
SWITCH = "showBuyLandScreen.lotPricePerTile"


def price_of_acres(a: int) -> int:
    return (a * 125 + 50) // 100 * 10   # $1,250 an acre to the nearest $1,000 (x $100)


def _buy_corners(portlib, n, per_acre: bool, lots=(8, 6, 2, 0)):
    """the corner lots bought one after the other on a flat course at n; per purchase (count, lot, tiles for sale, price, rng after,
    tiles left for sale)"""
    portlib.defect(SWITCH, not per_acre)
    out = []
    try:
        with portlib.state() as st:
            if st.lib.has("audio_exe.stubRequests"):
                st.call("audio_exe.stubRequests", [1])
            st.set_data_base(TC.DATA_VA)
            st.load_image(FE._course(portlib, n))
            if n != 50:
                st.world_resize(n)
            st.call("generateFlatCourse", [3])
            o = TC.objs(portlib)
            L_ = (n - 2) // 3
            for p, lot in enumerate(lots):
                st.mem_write(TC.DATA_VA + o["g_landPurchaseCount"][0], struct.pack("<i", p))
                types = st.mem_read(st.grid_va("g_tileType"), n * n)
                r0, c0 = (lot % 3) * L_ + 1, (lot // 3) * L_ + 1
                for_sale = sum(1 for i in range(L_) for j in range(L_) if types[(r0 + i) * n + c0 + j] == 0x14)
                before = FE._cash(st)
                x, y = FE._lot_point(lot)
                got = FE._run(st, CE.click(x, y), (x, y), p + (0 if per_acre else 8))
                img = st.image()
                types2 = st.mem_read(st.grid_va("g_tileType"), n * n)
                left = sum(1 for i in range(L_) for j in range(L_) if types2[(r0 + i) * n + c0 + j] == 0x14)
                rng = struct.unpack_from("<I", img, o["g_rng"][0])[0]
                count = struct.unpack_from("<i", img, o["g_landPurchaseCount"][0])[0]
                assert got == 1, (n, p, lot)
                out.append((p, lot, for_sale, before - FE._cash(st), rng, left, count, types2))
            if n != 50:
                st.world_resize(50)
    finally:
        portlib.defect_reset()
    return out


@pytest.mark.parametrize("n", [50, 101, 152])
def test_lot_price_is_the_acres_shown_times_1250(portlib, n, capsys):
    rows = _buy_corners(portlib, n, per_acre=True)
    for p, lot, fs, price, rng, left, count, _ in rows:
        assert price == price_of_acres(fs // 10), (n, p, lot, fs, price)
        assert left == 0   # (mainLoop's frame adds 1 to g_landPurchaseCount when the screen returns 1; the test sets it)
    # no doubling: a lot with the same tiles for sale costs the same after any number of purchases
    by_tiles = {}
    for p, lot, fs, price, *_ in rows:
        by_tiles.setdefault(fs, set()).add(price)
    assert all(len(v) == 1 for v in by_tiles.values()), by_tiles
    with capsys.disabled():
        print(f"\n[P38W3] N {n}: " + "; ".join(f"purchase {p + 1} lot {lot + 1}: {fs} tiles ({fs // 10} acres) ${price * 100:,}"
                                              for p, lot, fs, price, *_ in rows))


@pytest.mark.parametrize("n", [50, 152])
def test_same_draws_and_tiles_either_way(portlib, n):
    """the switch changes the cost only: the RNG state after each screen, the converted tiles and the purchase count are equal"""
    on, off = _buy_corners(portlib, n, per_acre=True), _buy_corners(portlib, n, per_acre=False)
    for a, b in zip(on, off):
        assert (a[0], a[1], a[2], a[4], a[5], a[6]) == (b[0], b[1], b[2], b[4], b[5], b[6])
        assert a[7] == b[7]


@pytest.mark.parametrize("n", [50, 152])
def test_switch_at_1_keeps_the_per_tile_sum(portlib, n):
    rows = _buy_corners(portlib, n, per_acre=False)
    for p, lot, fs, price, *_ in rows:
        assert (5 * 20 // 100) * 10 <= price <= ((5 + (fs << p)) * 20 // 100) * 10   # the original's arithmetic (test_p38fe_buyland)
    # it doubles: the last purchase costs more than per-acre pricing at 152 (and at 50 grows past the first)
    assert rows[-1][3] > rows[0][3]


@pytest.mark.parametrize("n,left", [(50, 5), (152, 9), (152, 1)])
def test_a_scrap_of_land_is_one_acre_never_free(portlib, n, left):
    """a lot with 1..9 tiles for sale shows 1 acre and costs $1,000 (the shown acres are at least 1 while any tile is for sale)"""
    portlib.defect(SWITCH, False)
    try:
        with portlib.state() as st:
            if st.lib.has("audio_exe.stubRequests"):
                st.call("audio_exe.stubRequests", [1])
            st.set_data_base(TC.DATA_VA)
            st.load_image(FE._course(portlib, n))
            if n != 50:
                st.world_resize(n)
            st.call("generateFlatCourse", [3])
            L_ = (n - 2) // 3
            lot = 8
            r0, c0 = (lot % 3) * L_ + 1, (lot // 3) * L_ + 1
            tva = st.grid_va("g_tileType")
            types = bytearray(st.mem_read(tva, n * n))
            kept = 0
            for i in range(L_):
                for j in range(L_):
                    k = (r0 + i) * n + c0 + j
                    if types[k] == 0x14:
                        if kept < left:
                            kept += 1
                        else:
                            types[k] = 4   # owned rough
            st.mem_write(tva, bytes(types))
            before = FE._cash(st)
            x, y = FE._lot_point(lot)
            assert FE._run(st, CE.click(x, y), (x, y), 20) == 1
            assert before - FE._cash(st) == 10          # 1 acre x $1,250 -> $1,000 (x $100)
            if n != 50:
                st.world_resize(50)
    finally:
        portlib.defect_reset()
